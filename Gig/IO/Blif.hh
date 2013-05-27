//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Blif.hh
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Blif writer, intended for transferring LUT mapped designs to ABC
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__IO__Blif_hh
#define ZZ__Gig__IO__Blif_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeBlif    (Out& out       , Gig& N);
bool writeBlifFile(String filename, Gig& N);
    // -- returns FALSE if file could not be created. 


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
