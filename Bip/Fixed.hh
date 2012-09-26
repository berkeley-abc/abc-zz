//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Fixed.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT based reachability engine.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Fixed_hh
#define ZZ__Bip__Fixed_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Fixed {
    uint    depth;
    bool    quiet;

    Params_Fixed() :
        depth(0),
        quiet(false)
    {}
};


lbool fixed(NetlistRef N0, const Vec<Wire>& props,
            const Params_Fixed& P = Params_Fixed(),
            Cex* cex = NULL, NetlistRef N_invar = NULL, int* bf_depth = NULL, EffortCB* cb = NULL);

void testQuantify();


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
