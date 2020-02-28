#include "Prelude.hh"
#include "SdlWrapper.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    const uint ts = 32;

    // Load tiles:
    Bitmap bm_tiles;
    if (!readPng(basePath("dctiles.png"), bm_tiles)){
        ShoutLn "Could not load tiles.";
        exit(1); }

    // Load font:
    TTF_Font* font = TTF_OpenFont(basePath("VeraSerif.ttf").c_str(), 20);
    if (!font){
        ShoutLn "Could not load font.";
        exit(1); }

    // Create window:
    Win win(992, 736, "SDL Test 3");
    Tex t_main = win.mkTex();

    // Generate maze:
    uint64 seed = 42;
    Vec<Vec<uchar> > maze(win.height() / ts);
    for (uint j = 0; j < maze.size(); j++)
        maze(j).growTo(win.width() / ts, '.');

    for (uint x = 0; x < maze[0].size(); x++){
        maze[0][x] = '#';
        maze[LAST][x] = '#'; }
    for (uint y = 0; y < maze.size(); y++){
        maze[y][0] = '#';
        maze[y][LAST] = '#'; }

    for (uint y = 2; y < maze.size() - 2; y += 2)
    for (uint x = 2; x < maze[0].size() - 2; x += 2){
        maze[y][x] = '#';
        switch (irand(seed, 4)){
        case 0: maze[y-1][x] = '#'; break;
        case 1: maze[y+1][x] = '#'; break;
        case 2: maze[y][x-1] = '#'; break;
        case 3: maze[y][x+1] = '#'; break;
        }
    }

    // Draw maze:
    for (uint y = 0; y < win.height() / ts; y++)
    for (uint x = 0; x < win.width () / ts; x++){
        if (maze[y][x] == '.')
            copy(bm_tiles, 16*ts, 13*ts, ts, ts, t_main.bm(), x*ts, y*ts);
        else if (maze[y][x] == '#')
            copy(bm_tiles, 8*ts, 16*ts, ts, ts, t_main.bm(), x*ts, y*ts);
        else
            assert(false);
    }
    t_main.update();

    // Setup player texture:
    Bitmap bm_player(copy_, bm_tiles, 22*ts, 1*ts, ts, ts);
    Tex t_player = win.mkTex(bm_player);
    uint px = 1, py = 1;

    // Setup steps background:
    Bitmap bm_steps(120, 40, Color(150, 0, 0, 120));
    Tex t_steps = win.mkTex(bm_steps);

    // Create scene:
    win.layers.push(t_main);
    win.layers.push(t_player);
    win.layers.push(t_steps);
    win.layers.push();      // -- step counter text

    win.layers[2].dst.x = (win.width() - win[2].bm().width) / 2;
    win.layers[2].dst.y = 50;


    uint n_steps = 0;
    for(;;){
        // Update and draw scene:
        win.layers[1].dst.x = px * ts;
        win.layers[1].dst.y = py * ts;

        String text;
        FWrite(text) "Steps: %_", n_steps;
        win.layers[3] = win.mkTex(font, Color(255,255,255), text);
        win.layers[3].dst.x = win.layers[2].dst.x + (win[2].bm().width  - win[3].bm().width ) / 2;
        win.layers[3].dst.y = win.layers[2].dst.y + (win[2].bm().height - win[3].bm().height) / 2;

        win.present();

        // Wait for event:
        SDL_Event ev;
        waitEvent(ev);
        if (ev.type == SDL_KEYDOWN){
            uint sym = (uint)ev.key.keysym.sym;

            if (sym == SDLK_ESCAPE)
                exit(0);
            else if (sym == SDLK_UP && maze[py-1][px] == '.')
                n_steps++, py--;
            else if (sym == SDLK_DOWN && maze[py+1][px] == '.')
                n_steps++, py++;
            else if (sym == SDLK_LEFT && maze[py][px-1] == '.')
                n_steps++, px--;
            else if (sym == SDLK_RIGHT && maze[py][px+1] == '.')
                n_steps++, px++;
        }
    }

    TTF_CloseFont(font);
    return 0;
}
