//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Dot.hh
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__IO__Dot_hh
#define ZZ__Gig__IO__Dot_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void growRegion(Gig& N, WZet& region, String grow_spec, uint lim);
void writeDot(String filename, Gig& N, Vec<String>* uif_names = NULL);
void writeDot(String filename, Gig& N, const WZet& region, Vec<String>* uif_names = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
