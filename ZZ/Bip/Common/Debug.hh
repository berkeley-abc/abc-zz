//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Debug.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Functions to facilitate debugging.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Debug_hh
#define ZZ__Bip__Debug_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void dumpFormula(Wire w);
uind dagSize(Wire w);
bool bmcCheck(NetlistRef N, Wire init, Wire bad, uint depth);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
