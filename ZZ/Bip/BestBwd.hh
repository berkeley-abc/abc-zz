//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BestBwd.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Best-first Backward Reachability.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__BestBwd_hh
#define ZZ__Bip__BestBwd_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Bbr {
    enum Weaken { TSIM, JUST, IMPL };

    Weaken  weaken;     // -- Weakening method (ternary simulation, justification or implication graph)
    uint    branch;     // -- Branching factor (how many pre-image cubes to generate at a time)
    bool    quiet;

    Params_Bbr() :
        weaken(IMPL),
        branch(5),
        quiet(false)
    {}
};


lbool bbr(NetlistRef N0, const Vec<Wire>& props, const Params_Bbr& P, Cex* cex, NetlistRef N_invar, int* bf_depth);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
