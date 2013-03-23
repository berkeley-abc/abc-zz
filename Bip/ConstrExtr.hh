#ifndef ZZ__Bip__ConstrExtr_hh
#define ZZ__Bip__ConstrExtr_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void constrExtr(NetlistRef N, const Vec<GLit>& bad, uint k, uint l, /*out*/Vec<Cube>& eq_classes);
    // -- use 'k' or 'l' == UINT_MAX to skip forward/backward extraction.

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
