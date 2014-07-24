#include <SDL.h>
#include "Prelude.hh"
#include "ZZ_Graphics.hh"
#include "Debug.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Endianess adjustment may possibly be needed here.
uint surf_r_mask = 0xFF;
uint surf_g_mask = 0xFF00;
uint surf_b_mask = 0xFF0000;
uint surf_a_mask = 0xFF000000;

#define SURF_MASKS surf_r_mask, surf_g_mask, surf_b_mask, surf_a_mask


void waitKey()
{
    for(;;){
        SDL_PumpEvents();
        SDL_Event ev;
        if (SDL_WaitEvent(&ev) == 0){
            ShoutLn "SDL_WaitEvent failed: ", SDL_GetError();
            exit(1); }

        if (ev.type == SDL_KEYDOWN && (uint)ev.key.keysym.sym == SDLK_SPACE)
            break;

        if (ev.type == SDL_KEYDOWN && (uint)ev.key.keysym.sym == SDLK_ESCAPE)
            exit(0);

        else if (ev.type == SDL_MOUSEMOTION)
            ;/*nothing*/
        else
            WriteLn "NOTE! Unhandled event: \a/%_\a/", ev;

    }
}


int main(int argc, char** argv)
{
    ZZ_Init;

    Dump(sizeof(SDL_Event));

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
        ShoutLn "SDL_Init failed: %_", SDL_GetError();
        exit(1); }

    SDL_Window* win = SDL_CreateWindow("My Window", 0, 0, 1024, 768, SDL_WINDOW_BORDERLESS);
    if (!win){
        ShoutLn "SDL_CreateWindow failed: ", SDL_GetError();
        exit(1); }

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren){
        ShoutLn "SDL_CreateRenderer failed: ", SDL_GetError();
        exit(1); }

    SDL_RenderPresent(ren);
    Bitmap bm(1000, 1000);

    for (uint y = 0; y < bm.width; y++)
    for (uint x = 0; x < bm.height; x++)
        bm.set(x, y, Color(x*3, y*2, x+4*y, x+y));

#if 0
    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(bm.data, bm.width, bm.height, 32, bm.width*4, SURF_MASKS);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
#else
//    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, bm.width, bm.height);
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, bm.width, bm.height);
    SDL_UpdateTexture(tex, /*rectangle*/NULL, bm.data, bm.line_sz * 4);

  #if 1
    {
        double T0 = realTime();
        for (uint i = 0; i < 100; i++){
          #if 1
            SDL_UpdateTexture(tex, /*rectangle*/NULL, bm.data, bm.line_sz * 4);
          #else
            Color* tex_data;
            int    tex_pitch;
            SDL_LockTexture(tex, /*rectangle*/NULL, (void**)&tex_data, &tex_pitch);
            tex_pitch /= 4;
            for (uint y = 0; y < bm.width; y++)
            for (uint x = 0; x < bm.height; x++)
                tex_data[y * tex_pitch + x] = bm(x, y);
            SDL_UnlockTexture(tex);
          #endif
        }
        double T1 = realTime();
        WriteLn "UpdateTexture Time: %t", (T1-T0);
    }
  #endif

#endif
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    SDL_Rect rect;
    rect.x = 10;
    rect.y = 10;
    rect.w = bm.width;
    rect.h = bm.height;

    uint64 seed = 42;
    for (uint i = 0; i < 1000; i++){
        rect.x = irand(seed, 100);
        rect.y = irand(seed, 100);
        SDL_RenderCopyEx(ren, tex, /*src-rect*/NULL, /*dst-rect*/&rect, /*rotation*/0, /*center*/NULL, SDL_FLIP_NONE);
    }

    double T0 = realTime();
    SDL_RenderPresent(ren);
    double T1 = realTime();
    WriteLn "RenderPresent Time: %t", (T1-T0);
    waitKey();
#if 0

    rect.x += 40;
    SDL_RenderCopyEx(ren, tex, /*src-rect*/NULL, /*dst-rect*/&rect, /*rotation*/0, /*center*/NULL, SDL_FLIP_NONE);
    SDL_RenderPresent(ren);
    waitKey();
#endif

    return 0;
}


// SdlWinEvent{EXPOSED : 0 0}


// int SDL_WaitEventTimeout(SDL_Event* event, int timeout)


/*

- Testa hastighet på statisk vs strömmande texture
- Testa hastighet på UpdateTexture vs. Lock/Unlock

*/


/*
Modell:

 - Render-kommandon lägger up saker på render-kön
 - RenderPresent() ritar ut sakerna i kön och tömmer den.

Streaming + Update verkar vara okej...
*/
