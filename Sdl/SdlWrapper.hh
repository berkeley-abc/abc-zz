#ifndef ZZ__Sdl__SdlWrapper_hh
#define ZZ__Sdl__SdlWrapper_hh

#include <SDL.h>
#include <SDL_ttf.h>
#include "ZZ_Graphics.hh"
#include "ZZ/Generics/RefC.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper types:


struct Rect : SDL_Rect {
    Rect(int x_, int y_, int w_, int h_) { x = x_; y = y_; w = w_; h = h_; }
    Rect(int w_, int h_)                 { x = 0 ; y = 0 ; w = w_; h = h_; }
    Rect()                               { x = 0 ; y = 0 ; w = 0 ; h = 0 ; }
};


template<> fts_macro void write_(Out& out, const SDL_Rect& v)
{
    FWrite(out) "Rect{x=%_; y=%_; w=%_; h=%_}", v.x, v.y, v.w, v.h;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Textures:


struct Tex_data {
    uint          refC;

    SDL_Renderer* ren;
    SDL_Texture*  tex;
    Bitmap        bm;       // -- backing of texture content

    Tex_data(SDL_Renderer* ren_, Bitmap& bm_) {
        ren = ren_;
        bm_.moveTo(bm);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, bm.width, bm.height);
        update(0, 0, bm.width, bm.height);
    }

   ~Tex_data() {
        SDL_DestroyTexture(tex);
        /**/WriteLn "Destroying texture...";
    }

    void update(uint x, uint y, uint w, uint h) {
        SDL_Rect rect;
        rect.x = x;
        rect.y = y;
        rect.w = w;
        rect.h = h;
        SDL_UpdateTexture(tex, &rect, &bm(x, y), bm.line_sz * 4);
    }
};


class Tex : public RefC<Tex_data> {
    friend class Win;

    Tex(SDL_Renderer* ren, Bitmap& bm) : RefC<Tex_data>(new Tex_data(ren, bm)) {}   // -- steals 'bm'
    Tex(const RefC<Tex_data> p) : RefC<Tex_data>(p) {} // -- downcast from parent to child

public:
    Tex() {}
    Bitmap& bm() { return (*this)->bm; }            // -- returns the bitmap backing data for the texture

    void update(const Rect& rect)               { (*this)->update(rect.x, rect.y, rect.w, rect.h); }
    void update(uint x, uint y, uint w, uint h) { (*this)->update(x, y, w, h); }
        // -- write portion of bitmap backing to SDL texture (in graphics memory)
    void update()                               { (*this)->update(0, 0, bm().width, bm().height); }
        // -- write entire bitmap backing to SDL texture
};



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Layers:


// Placement and rotation of a texture:
struct Layer {
    Tex    tex;
    Rect   src;         // }- zero width/height means full size
    Rect   dst;         // }
    double angle;       // -- in degrees
    int    rot_xoff;    // }- point of rotation, offset from center
    int    rot_yoff;    // }
    bool   flip_horz;
    bool   flip_vert;

    Layer(Tex tex_ = Tex()) : tex(tex_), angle(0.0), rot_xoff(0), rot_yoff(0), flip_horz(false), flip_vert(false) {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Window:


// Represents a stack of layers in a given window.
class Win {
    SDL_Window*   win;
    SDL_Renderer* ren;
    uint          win_id;

    uint w;
    uint h;

public:
    Win();          // -- full-screen
    Win(uint w_, uint h_, String title = "", bool resizable = true, bool borderless = false);
   ~Win();

    void close();   // -- dispose window resources and close window
    Null_Method(Win) { return win == NULL; }

    // Update these directly, then call 'present()':
    Color       bg;         // -- if fully transparent, bottom layer will be drawn opaquely.
    Vec<Layer>  layers;     // -- drawn with alpha-blend, starting with 'layers[0]'.

    Tex operator[](uint i) { return layers[i].tex; } // -- returns texture of layer 'i'

    uint id() const { return win_id; }

    uint width () const { return w; }
    uint height() const { return h; }

    Tex mkTex(Bitmap& bm);                          // -- takes ownership of 'bm'; make texture of same size as 'bm'
    Tex mkTex(uint w, uint h, Color c = Color());   // -- make texture of given size
    Tex mkTex(Color c = Color());                   // -- make a texture of the same size as the window

    Tex mkTex(TTF_Font* font, Color c_text, String text); // -- creates a texture from a piece of text

    void present();         // -- flush updates made to textures and layers to the physical display
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global functions:


bool waitEvent(SDL_Event& ev, double timeout = DBL_MAX);
    // -- may call 'present()' on windows that are made fully or partially visible.
    // Window resizing must be handled by caller (update 'layers' and call 'present()').
    // Returns FALSE if timeout was reached (no timeout => always returns TRUE).xs

Win* getWin(uint win_id);
    // -- returns NULL if window has been closed.


String basePath(String filename);
    // -- prefix 'filename' with the path of the executable.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
