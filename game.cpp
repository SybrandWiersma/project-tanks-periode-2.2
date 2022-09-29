#include "precomp.h" // include (only) this in every .cpp file

constexpr auto num_tanks_blue = 2048;
constexpr auto num_tanks_red = 2048;

constexpr auto tank_max_health = 1000;
constexpr auto rocket_hit_value = 60;
constexpr auto particle_beam_hit_value = 50;

constexpr auto tank_max_speed = 1.0;

constexpr auto health_bar_width = 70;

constexpr auto max_frames = 2000;

//Global performance timer
constexpr auto REF_PERFORMANCE = 158260; //UPDATE THIS WITH YOUR REFERENCE PERFORMANCE (see console after 2k frames)
static timer perf_timer;
static float duration;

//Load sprite files and initialize sprites
static Surface* tank_red_img = new Surface("assets/Tank_Proj2.png");
static Surface* tank_blue_img = new Surface("assets/Tank_Blue_Proj2.png");
static Surface* rocket_red_img = new Surface("assets/Rocket_Proj2.png");
static Surface* rocket_blue_img = new Surface("assets/Rocket_Blue_Proj2.png");
static Surface* particle_beam_img = new Surface("assets/Particle_Beam.png");
static Surface* smoke_img = new Surface("assets/Smoke.png");
static Surface* explosion_img = new Surface("assets/Explosion.png");

static Sprite tank_red(tank_red_img, 12);
static Sprite tank_blue(tank_blue_img, 12);
static Sprite rocket_red(rocket_red_img, 12);
static Sprite rocket_blue(rocket_blue_img, 12);
static Sprite smoke(smoke_img, 4);
static Sprite explosion(explosion_img, 9);
static Sprite particle_beam_sprite(particle_beam_img, 3);

const static vec2 tank_size(7, 9);
const static vec2 rocket_size(6, 6);

const static float tank_radius = 3.f;
const static float rocket_radius = 5.f;

const int cellsize = 20;
const float collision_max_edge = (tank_radius * 2) / 20;
const float rockets_max_edge = (tank_radius + rocket_radius) / 20;

vector<Tank*> grid[65][35];

const unsigned int thread_count = thread::hardware_concurrency() * 2;

ThreadPool pool(thread_count);

// -----------------------------------------------------------
// Initialize the simulation state
// This function does not count for the performance multiplier
// (Feel free to optimize anyway though ;) )
// -----------------------------------------------------------
void Game::init()
{
    frame_count_font = new Font("assets/digital_small.png", "ABCDEFGHIJKLMNOPQRSTUVWXYZ:?!=-0123456789.");

    tanks_alive.reserve(num_tanks_blue + num_tanks_red);

    uint max_rows = 24;

    float start_blue_x = tank_size.x + 40.0f;
    float start_blue_y = tank_size.y + 30.0f;

    float start_red_x = 1088.0f;
    float start_red_y = tank_size.y + 30.0f;

    float spacing = 7.5f;

    //Spawn blue tanks
    for (int i = 0; i < num_tanks_blue; i++)
    {
        vec2 position{ start_blue_x + ((i % max_rows) * spacing), start_blue_y + ((i / max_rows) * spacing) };
        tanks_alive.push_back(Tank(i, position.x, position.y, BLUE, &tank_blue, &smoke, 1100.f, position.y + 16, tank_radius, tank_max_health, tank_max_speed));
    }
    //Spawn red tanks
    for (int i = 0; i < num_tanks_red; i++)
    {
        vec2 position{ start_red_x + ((i % max_rows) * spacing), start_red_y + ((i / max_rows) * spacing) };
        tanks_alive.push_back(Tank((i + num_tanks_blue), position.x, position.y, RED, &tank_red, &smoke, 100.f, position.y + 16, tank_radius, tank_max_health, tank_max_speed));
    }

    particle_beams.push_back(Particle_beam(vec2(590, 327), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
    particle_beams.push_back(Particle_beam(vec2(64, 64), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
    particle_beams.push_back(Particle_beam(vec2(1200, 600), vec2(100, 50), &particle_beam_sprite, particle_beam_hit_value));
}

// -----------------------------------------------------------
// Close down application
// -----------------------------------------------------------
void Game::shutdown()
{
    
}

// -----------------------------------------------------------
// Checks first the closest grid cells for enemy tank, growing in radius when enemy tank not found 
// -----------------------------------------------------------
Tank& Game::find_closest_enemy(Tank& current_tank)
{
    float closest_distance = numeric_limits<float>::infinity();
    int closest_index = 0;
    int radius = 1;

    bool n = TRUE, e = TRUE, s = TRUE, w = TRUE;

    Tank* closest_tank{};

    vec2 position = current_tank.position;
    int x = position.x / cellsize;
    int y = position.y / cellsize;
    int north = y, east = x, south = y, west = x;

    while (closest_tank == NULL) {
        // check if the side you want to check is not out of the field 
        if (north < 34) {
            north++;
        }
        else
            n = FALSE;
        if (east < 64) {
            east++;
        }
        else
            e = FALSE;
        if (south > 0) {
            south--;
        }
        else
            s = FALSE;
        if (west > 0) {
            west--;
        }
        else
            w = FALSE;

        // check if enemy tank is in cell you want to check
        if(n)
            for(int i = west; i <= east; i++)
                check_closest(grid[i][north], current_tank, closest_tank, closest_distance);
        if(e)
            for (int i = south; i <= north; i++)
                check_closest(grid[east][i], current_tank, closest_tank, closest_distance);
        if(s)
            for (int i = west; i <= east; i++)
                check_closest(grid[i][south], current_tank, closest_tank, closest_distance);
        if(w)
            for (int i = south; i <= north; i++)
                check_closest(grid[west][i], current_tank, closest_tank, closest_distance);
        
        radius++;
    }
    return *closest_tank;

}

void Game::check_closest(vector<Tank*>&tanks, Tank& current_tank, Tank*& closest_tank, float& closest_distance) {
    for (Tank* tank : tanks) {
        if (tank->allignment == current_tank.allignment)
            continue;
        float sqr_dist = fabsf((tank->get_position() - current_tank.get_position()).sqr_length());
        if (sqr_dist < closest_distance)
        {
            closest_distance = sqr_dist;
            closest_tank = tank;
        }
    }
}

//Checks if a point lies on the left of an arbitrary angled line
bool Tmpl8::Game::left_of_line(vec2 line_start, vec2 line_end, vec2 point)
{
    return ((line_end.x - line_start.x) * (point.y - line_start.y) - (line_end.y - line_start.y) * (point.x - line_start.x)) < 0;
}


// -----------------------------------------------------------
// Update the game state:
// Move all objects
// Update sprite frames
// Collision detection
// Targeting etc..
// -----------------------------------------------------------
void Game::update(float deltaTime)
{
    //Calculate the route to the destination for each tank using BFS
    //Initializing routes here so it gets counted for performance..

    if (frame_count == 0)
    {
        for (Tank& t : tanks_alive)
        {
            t.set_route(background_terrain.get_route_Astar(t, t.target));
        }
        update_grid();

    }


    check_collisions();
    update_tank();
    update_grid();
    update_smokes();
    update_rockets();
    update_forcefield();
    update_particle_beam();
    update_explosions();


}

// Function for updating grid
void Game::update_grid() {
    //Clear grid
    for (int i = 0; i < 65; i++)
    {
        for (int j = 0; j < 35; j++)
        {
            grid[i][j].clear();
        }
    }

    //Add tanks to grid for optimalization
    for (Tank& tank : tanks_alive)
    {
        vec2 position = tank.get_position();
        float x = position.x / cellsize;
        float y = position.y / cellsize;
        int gridposx = x;
        int gridposy = y;

        grid[gridposx][gridposy].push_back(&tank);
    }
}

//Check tank collision and nudge tanks away from each other
void Game::check_collisions() {  
    for (Tank& tank : tanks_alive)
    {
        vec2 position = tank.get_position();
        vec2 grid_pos = position / cellsize;
        int x = grid_pos.x, y = grid_pos.y;
        float from_edge_x = grid_pos.x - x, from_edge_y = grid_pos.y - y;

        //Calculate if tank is near the edge a grid cell so the tank can also collide with the tank in the cell adjesent to it.
        int x_max = x, y_max = y;
        if (from_edge_x < collision_max_edge)
            x--;
        if (from_edge_x > (1 - collision_max_edge))
            x_max++;
        if (from_edge_y < collision_max_edge)
            y--;
        if (from_edge_y > (1 - collision_max_edge))
            y_max++;

        for (int i = x; i <= x_max; i++) {
            for (int j = y; j <= y_max; j++) {
                for (Tank* other_tank : grid[i][j])
                {
                    if (tank.id == other_tank->get_id()) continue;

                    vec2 dir = tank.get_position() - other_tank->get_position();
                    float dir_squared_len = dir.sqr_length();

                    float col_squared_len = (tank.get_collision_radius() + other_tank->get_collision_radius());
                    col_squared_len *= col_squared_len;

                    if (dir_squared_len < col_squared_len)
                    {
                        tank.push(dir.normalized(), 1.f);
                    }
                }
            }
        }

    }
}

// Function for updating tanks
void Game::update_tank() {
    for (Tank& tank : tanks_alive)
    {
        //Move tanks according to speed and nudges (see above) also reload
        tank.tick(background_terrain);

        //Shoot at closest target if reloaded
        if (tank.rocket_reloaded())
        {
            Tank& target = find_closest_enemy(tank);

            rockets.push_back(Rocket(tank.position, (target.get_position() - tank.position).normalized() * 3, rocket_radius, tank.allignment, ((tank.allignment == RED) ? &rocket_red : &rocket_blue)));

            tank.reload_rocket();
        }

    }
}

// Function for updating smoke plumes
void Game::update_smokes() {
    
    for (Smoke& smoke : smokes)
    {
        smoke.tick();
    }
}

// Function for updating rockets
void Game::update_rockets() {
    
    for (Rocket& rocket : rockets)
    {
        rocket.tick();

        vec2 position = rocket.position;
        vec2 grid_pos = position / cellsize;
        int x = grid_pos.x, y = grid_pos.y;
        float from_edge_x = grid_pos.x - x, from_edge_y = grid_pos.y - y;

        //Calculate if tank is near the edge a grid cell so the tank can also collide with the tank in the cell adjesent to it.
        int x_max = x, y_max = y;
        if (from_edge_x < rockets_max_edge)
            x--;
        if (from_edge_x > (1 - rockets_max_edge))
            x_max++;
        if (from_edge_y < rockets_max_edge)
            y--;
        if (from_edge_y > (1 - rockets_max_edge))
            y_max++;

        for (int i = x; i <= x_max; i++) {
            for (int j = y; j <= y_max; j++) {
                //Check if rocket collides with enemy tank, spawn explosion, and if tank is destroyed spawn a smoke plume
                for (Tank* tank : grid[i][j])
                {
                    if ((tank->allignment != rocket.allignment) && rocket.intersects(tank->position, tank->collision_radius))
                    {
                        explosions.push_back(Explosion(&explosion, tank->position));

                        if (tank->hit(rocket_hit_value))
                        {
                            tanks_dead.push_back(*tank);
                            smokes.push_back(Smoke(smoke, tank->position - vec2(7, 24)));
                        }

                        rocket.active = false;
                        break;
                    }
                }
            }
        }
    }

    tanks_alive.erase(remove_if(tanks_alive.begin(), tanks_alive.end(), [](const Tank& tank) { return !tank.active; }), tanks_alive.end());

    //Disable rockets if they collide with the "forcefield"
    //Hint: A point to convex hull intersection test might be better here? :) (Disable if outside)
    for (Rocket& rocket : rockets)
    {
        if (rocket.active)
        {
            for (size_t i = 0; i < forcefield_hull.size(); i++)
            {
                if (circle_segment_intersect(forcefield_hull.at(i), forcefield_hull.at((i + 1) % forcefield_hull.size()), rocket.position, rocket.collision_radius))
                {
                    explosions.push_back(Explosion(&explosion, rocket.position));
                    rocket.active = false;
                }
            }

        }
    }



    //Remove exploded rockets with remove erase idiom
    rockets.erase(std::remove_if(rockets.begin(), rockets.end(), [](const Rocket& rocket) { return !rocket.active; }), rockets.end());
}

void Game::update_forcefield() {
    //Calculate "forcefield" around active tanks
    forcefield_hull.clear();

    //Find first active tank
    vec2 point_on_hull = tanks_alive.at(0).position;

    //Find left most tank position
    for (Tank& tank : tanks_alive)
    {

        if (tank.position.x <= point_on_hull.x)
        {
            point_on_hull = tank.position;
        }

    }

    //Calculate convex hull for 'rocket barrier'
    for (Tank& tank : tanks_alive)
    {

        forcefield_hull.push_back(point_on_hull);
        vec2 endpoint = tanks_alive.at(0).position;

        for (Tank& tank : tanks_alive)
        {

            if ((endpoint == point_on_hull) || left_of_line(point_on_hull, endpoint, tank.position))
            {
                endpoint = tank.position;
            }

        }
        point_on_hull = endpoint;

        if (endpoint == forcefield_hull.at(0))
        {
            break;
        }
        
    }
}

void Game::update_particle_beam() {
    //Update particle beams
    for (Particle_beam& particle_beam : particle_beams)
    {
        particle_beam.tick(tanks_alive);
        particle_beam.tick(tanks_dead);


        //Damage all tanks within the damage window of the beam (the window is an axis-aligned bounding box)
        for (int i = 0; i < tanks_alive.size(); i++)
        {
            Tank& tank = tanks_alive[i];
            if (particle_beam.rectangle.intersects_circle(tank.get_position(), tank.get_collision_radius()))
            {
                if (tank.hit(particle_beam.damage))
                {
                    tanks_alive.erase(tanks_alive.begin() + i);
                    tanks_dead.push_back(tank);
                    smokes.push_back(Smoke(smoke, tank.position - vec2(0, 48)));
                }
            }
        }
    }
}

void Game::update_explosions() {
    //Update explosion sprites and remove when done with remove erase idiom
    for (Explosion& explosion : explosions)
    {
        explosion.tick();
    }

    explosions.erase(std::remove_if(explosions.begin(), explosions.end(), [](const Explosion& explosion) { return explosion.done(); }), explosions.end());
}






// -----------------------------------------------------------
// Draw all sprites to the screen
// (It is not recommended to multi-thread this function)
// -----------------------------------------------------------
void Game::draw()
{
    // clear the graphics window
    screen->clear(0);

    //Draw background
    background_terrain.draw(screen);

    //Draw sprites
    for (Tank tank : tanks_alive)
    {
        tank.draw(screen);

    }
    for (Tank tank : tanks_dead)
    {
        tank.draw(screen);

    }

    for (Rocket& rocket : rockets)
    {
        rocket.draw(screen);
    }

    for (Smoke& smoke : smokes)
    {
        smoke.draw(screen);
    }

    for (Particle_beam& particle_beam : particle_beams)
    {
        particle_beam.draw(screen);
    }

    for (Explosion& explosion : explosions)
    {
        explosion.draw(screen);
    }

    //Draw forcefield (mostly for debugging, its kinda ugly..)
    
    for (size_t i = 0; i < forcefield_hull.size(); i++)
    {
        vec2 line_start = forcefield_hull.at(i);
        vec2 line_end = forcefield_hull.at((i + 1) % forcefield_hull.size());
        line_start.x += HEALTHBAR_OFFSET;
        line_end.x += HEALTHBAR_OFFSET;
        screen->line(line_start, line_end, 0x0000ff);
    }
    

    


    //Draw sorted health bars
    for (int t = 0; t < 2; t++)
    {
        const int NUM_TANKS = ((t < 1) ? num_tanks_blue : num_tanks_red);

        const int begin = ((t < 1) ? 0 : num_tanks_blue);
        vector<int> tanks_hp;  
        for (Tank &tank : tanks_alive)
            if((t == 0 && tank.allignment == BLUE) || (t == 1 && tank.allignment == RED))
            tanks_hp.push_back(tank.get_health());
        
        CountSort(tanks_hp);
        draw_health_bars(tanks_hp, t);
    }
}

void Game::CountSort(vector<int>& a)
{
    int count[101];
    
    for (int i = 0; i < size(count); i++)
        count[i] = 0;

    for (int tank_hp : a) {
        tank_hp /= 10;
        count[tank_hp]++;
    }
    a.clear();

    for (int i = 0; i <= 100; i++) {
        for (int j = 0; j < count[i]; j++)
            a.emplace_back(i * 10);
    }
}



// -----------------------------------------------------------
// Draw the health bars based on the given tanks health values
// -----------------------------------------------------------
void Tmpl8::Game::draw_health_bars(vector<int> tanks_hp, const int team)
{
    int health_bar_start_x = (team < 1) ? 0 : (SCRWIDTH - HEALTHBAR_OFFSET) - 1;
    int health_bar_end_x = (team < 1) ? health_bar_width : health_bar_start_x + health_bar_width - 1;

    for (int i = 0; i < SCRHEIGHT - 1; i++)
    {
        //Health bars are 1 pixel each
        int health_bar_start_y = i * 1;
        int health_bar_end_y = health_bar_start_y + 1;

        screen->bar(health_bar_start_x, health_bar_start_y, health_bar_end_x, health_bar_end_y, REDMASK);
    }

    //Draw the <SCRHEIGHT> least healthy tank health bars
    int draw_count = std::min(SCRHEIGHT, (int)tanks_hp.size());
    for (int i = 0; i < draw_count - 1; i++)
    {
        //Health bars are 1 pixel each
        int health_bar_start_y = i * 1;
        int health_bar_end_y = health_bar_start_y + 1;

        float health_fraction = (1 - ((double)tanks_hp[i] / (double)tank_max_health));

        if (team == 0) { screen->bar(health_bar_start_x + (int)((double)health_bar_width * health_fraction), health_bar_start_y, health_bar_end_x, health_bar_end_y, GREENMASK); }
        else { screen->bar(health_bar_start_x, health_bar_start_y, health_bar_end_x - (int)((double)health_bar_width * health_fraction), health_bar_end_y, GREENMASK); }
    }
}

// -----------------------------------------------------------
// When we reach max_frames print the duration and speedup multiplier
// Updating REF_PERFORMANCE at the top of this file with the value
// on your machine gives you an idea of the speedup your optimizations give
// -----------------------------------------------------------
void Tmpl8::Game::measure_performance()
{
    char buffer[128];
    if (frame_count >= max_frames)
    {
        if (!lock_update)
        {
            duration = perf_timer.elapsed();
            cout << "Duration was: " << duration << " (Replace REF_PERFORMANCE with this value)" << endl;
            lock_update = true;
        }

        frame_count--;
    }

    if (lock_update)
    {
        screen->bar(420 + HEALTHBAR_OFFSET, 170, 870 + HEALTHBAR_OFFSET, 430, 0x030000);
        int ms = (int)duration % 1000, sec = ((int)duration / 1000) % 60, min = ((int)duration / 60000);
        sprintf(buffer, "%02i:%02i:%03i", min, sec, ms);
        frame_count_font->centre(screen, buffer, 200);
        sprintf(buffer, "SPEEDUP: %4.1f", REF_PERFORMANCE / duration);
        frame_count_font->centre(screen, buffer, 340);
    }
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::tick(float deltaTime)
{
    if (!lock_update)
    {
        update(deltaTime);
    }
    draw();

    measure_performance();

    //Print frame count
    frame_count++;
    string frame_count_string = "FRAME: " + std::to_string(frame_count);
    frame_count_font->print(screen, frame_count_string.c_str(), 350, 580);
}
