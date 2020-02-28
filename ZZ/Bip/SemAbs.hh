//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SemAbs.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Semantic abstraction engine.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__SemAbs_hh
#define ZZ__Bip__SemAbs_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


lbool semAbs(NetlistRef N, const Vec<Wire>& props);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
