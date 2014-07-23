//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SdlWrapper.cc
//| Author(s)   : Niklas Een
//| Module      : Sdl
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "SdlWrapper.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Initialize SDL:


ZZ_Initializer(SDL, 0) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
        ShoutLn "SDL_Init failed: %_", SDL_GetError();
        exit(1);
    }
}


ZZ_Finalizer(SDL, 0) {
    SDL_Quit();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


macro SDL_Color mkColor(uchar r, uchar g, uchar b, uchar a = 255)
{
    SDL_Color tmp;
    tmp.r = r;
    tmp.g = g;
    tmp.b = b;
    tmp.a = a;
    return tmp;
}


macro SDL_Color mkColor(Color c) {
    return mkColor(c.r, c.g, c.b, c.a); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Window:


Vec<Win*>* wins;

ZZ_Initializer(SDL_wins, 5) {
    wins = new Vec<Win*>();
};

ZZ_Finalizer(SDL_wins, 5) {
    delete wins;
}


// Full-screen window (should be the only window)
Win::Win()
{
    // Select largest screen resolution:
    for (int disp = 0; disp < SDL_GetNumVideoDisplays(); disp++){
        SDL_DisplayMode mode;
        SDL_GetDesktopDisplayMode(0, &mode);
        w = mode.w;
        h = mode.h;
        uint best_size = w * h;
        for (int i = 0; i < SDL_GetNumDisplayModes(disp); i++){
            if (SDL_GetDisplayMode(disp, i, &mode) != 0){
                ShoutLn "SDL_GetDisplayMode failed: %s", SDL_GetError();
                exit(1); }

            if (newMax(best_size, (uint)mode.w * (uint)mode.h)){
                w = mode.w;
                h = mode.h; }
        }
    }

    // Setup screen:
  #if defined(__APPLE__)
    win = SDL_CreateWindow("", 0, 0, w, h, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI);
  #else
    win = SDL_CreateWindow("", 0, 0, w, h, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS);
  #endif

    if (!win){
        ShoutLn "SDL_CreateWindow failed: ", SDL_GetError();
        exit(1); }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren){
        ShoutLn "SDL_CreateRenderer failed: ", SDL_GetError();
        exit(1); }

    wins->push(this);
}


// Normal window. May be resized by user.
Win::Win(uint w_, uint h_, String title, bool borderless)
{
    w = w_;
    h = h_;

    win = SDL_CreateWindow(title.c_str(), 0, 0, w, h, borderless);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren){
        ShoutLn "SDL_CreateRenderer failed: ", SDL_GetError();
        exit(1); }

    wins->push(this);
}


Win::~Win()
{
    revPullOut(*wins, this);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
}


Tex Win::mkTex(Bitmap& bm)
{
    return Tex(ren, bm);
}


Tex Win::mkTex(uint w, uint h, Color c)
{
    Bitmap bm(w, h, c);
    return Tex(ren, bm);
}


Tex Win::mkTex(Color c)
{
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(win, &win_w, &win_h);
    return mkTex(win_w, win_h, c);
}


void Win::present()
{
    if (bg.a != 0){
        assert(bg.a == 255);    // -- must be one or the other
        SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(ren, NULL);
    }

    bool first = true;
    for (uint i = 0; i < layers.size(); i++){
        if (!layers[i].tex) continue;

        SDL_BlendMode blend_mode = (first && bg.a == 0) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND;
        first = false;
        SDL_SetTextureBlendMode(layers[i].tex->tex, blend_mode);

        SDL_Rect src = layers[i].src;
        if (src.w == 0) src.w = layers[i].tex->bm.width;
        if (src.h == 0) src.h = layers[i].tex->bm.height;

        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(win, &win_w, &win_h);
        SDL_Rect dst = layers[i].dst;
        if (dst.w == 0) dst.w = win_w;
        if (dst.h == 0) src.h = win_h;

        int angle = int(layers[i].angle * 180 / M_PI + 0.5);

        SDL_Point center;
        center.x = dst.w / 2 + layers[i].rot_xoff;
        center.y = dst.h / 2 + layers[i].rot_yoff;

        uint64 flip = (layers[i].flip_horz ? SDL_FLIP_HORIZONTAL : 0)
                    | (layers[i].flip_vert ? SDL_FLIP_VERTICAL   : 0);

        SDL_RenderCopyEx(ren, layers[i].tex->tex, &src, &dst, angle, &center, (SDL_RendererFlip)flip);
    }

    SDL_RenderPresent(ren);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global functions:


// Returns TRUE if a new event was returned through 'ev', FALSE on timeout (given in seconds).
bool sdlWaitEvent(SDL_Event& ev, double timeout /*= DBL_MAX*/)
{
    bool ret = (timeout == DBL_MAX) ? SDL_WaitEvent(&ev) :
               /*otherwise*/          SDL_WaitEventTimeout(&ev, (int)(timeout * 1000 + 0.5));

    return ret;
        // <<== deal with window visibiliy here!
        // <<== deal with window closing here?
}


// Returns NULL if window has been closed.
Win* getWin(uint win_id)
{
    for (uind i = 0; i < wins->size(); i++)
        if ((*wins)[i]->id() == win_id)
            return (*wins)[i];
    return NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
