//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GigReader.hh
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Partial GIG reader for techmapping experimental purposes only.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__LutMap__GigReader_hh
#define ZZ__LutMap__GigReader_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void readGigForTechmap(String filename, Gig& N);
bool writeGigForTechmap(String filename, Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
