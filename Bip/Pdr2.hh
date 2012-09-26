//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr2.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR with learned cubes over arbitrary internal variables.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Pdr2_hh
#define ZZ__Bip__Pdr2_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Pdr2 {
};


lbool pdr2(NetlistRef N, const Vec<Wire>& props, const Params_Pdr2& P, Cex* cex, NetlistRef invariant, int* bug_free_depth);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
