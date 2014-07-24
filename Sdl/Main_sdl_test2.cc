#include "Prelude.hh"
#include "SdlWrapper.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Bitmap bm(10, 10, Color(0, 255, 0, 0));
    bm(5, 5) = Color(255, 255, 255);

    Bitmap bm2(10, 10);
    bm2(3, 5) = Color(255, 255, 0);

    Win win(1024, 768, "Test");
    win.bg = Color(0, 255, 0);
    win.layers.push(win.mkTex());
    win.layers.push(win.mkTex(bm2));
    win.layers.push(win.mkTex(bm));

    win[0].bm().set(100, 100, Color(255, 255, 0));
    win[0].bm().set(101, 100, Color(255, 255, 0));
    win[0].bm().set(100, 101, Color(255, 255, 0));
    win[0].bm().set(101, 101, Color(255, 255, 0));
    win[0].update(100, 100, 2, 2);

    win.layers[0].tex.update();

    win.present();

    Win win2(512, 512, "Test2");
    Bitmap bm3(12, 12);
    bm3(3, 5) = Color(255, 255, 0);
    win2.layers.push(win2.mkTex(bm3));
    win2.present();

    uint n_wins = 2;
    for(;;){
        SDL_Event ev;
        waitEvent(ev);

        if (ev.type == SDL_KEYDOWN && (uint)ev.key.keysym.sym == SDLK_ESCAPE){
            exit(0);

        }else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE){
            if (ev.window.windowID == win.id())
                WriteLn "Closed 'win'";
            else if (ev.window.windowID == win2.id())
                WriteLn "Closed 'win2'";
            n_wins--;
            if (n_wins == 0)
                exit(0);
        }
    }

    return 0;
}
