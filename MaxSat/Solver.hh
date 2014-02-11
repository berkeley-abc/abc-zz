//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.hh
//| Author(s)   : Niklas Een
//| Module      : MaxSat
//| Description :
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__MaxSat__Solver_hh
#define ZZ__MaxSat__Solver_hh

#include "Parser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void sorterMaxSat(MaxSatProb& P, bool down);
void coreMaxSat(MaxSatProb& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
