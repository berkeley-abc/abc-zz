#ifndef ZZ__Fun__RallyVroom__Debug_hh
#define ZZ__Fun__RallyVroom__Debug_hh

#include <SDL.h>

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeSdlEvent(Out& out, const SDL_Event& ev);

template<> fts_macro void write_(Out& out, const SDL_Event& ev) {
    writeSdlEvent(out, ev); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
