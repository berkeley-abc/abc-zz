//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PostProcess.hh
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Post-process mapping to fit particular target constraints.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__LutMap__PostProcess_hh
#define ZZ__LutMap__PostProcess_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void removeRemapSigns(Gig& N, WMapX<GLit>& remap);
void removeInverters(Gig& N, WMapX<GLit>* remap = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
