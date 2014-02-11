//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cube.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : A cube is a set of literals (used to have a specialized implementation).
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Cube_hh
#define ZZ__Bip__Cube_hh

#include "ZZ/Generics/Lit.hh"
#include "ZZ/Generics/Pack.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


typedef Pack<Lit> Cube;
static const Cube Cube_NULL;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
