//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Aiger.hh
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Aiger reader and writer (upto version 1.9)
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__IO__Aiger_hh
#define ZZ__Gig__IO__Aiger_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


Declare_Exception(Excp_AigerParseError);

void readAiger    (In& in         , Gig& N, bool verif_prob);
void readAigerFile(String filename, Gig& N, bool verif_prob);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
