//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Scl.hh
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Read and write Standard Cell Library information.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This is a compact binary version of the Liberty format, containing only the fields relevant to
//| static timing analysis.
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__Scl_hh
#define ZZ__Liberty__Scl_hh

#include "Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeScl(Out& out, const SC_Lib& L);
bool writeSclFile(String filename, const SC_Lib& L);
    // -- returns FALSE if file 'filename' could not be created. 

void readScl(In& in, SC_Lib& L);
void readSclFile(String filename, SC_Lib& L);
    // -- may throw 'Excp_ParseError'.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
