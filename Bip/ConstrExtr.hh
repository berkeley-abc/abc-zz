//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ConstrExtr.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Constraint extraction for verification.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

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
