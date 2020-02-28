//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PostProcess.hh
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__TechMap__PostProcess_hh
#define ZZ__TechMap__PostProcess_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void removeRemapSigns(Gig& N, WMapX<GLit>& remap);
void removeInverters(Gig& N, WMapX<GLit>* remap, bool quiet = false);
void removeMuxViolations(Gig& N, const WMap<float>& arrival, float target_arrival, float delay_fraction);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
