//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Gig.hh
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Parser for .gig format used by the old Netlist.
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__IO__Gig_hh
#define ZZ__Gig__IO__Gig_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void readGig    (In& in         , /*out*/Gig& N);
void readGigFile(String filename, /*out*/Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
