//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.hh
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description : Parsers for fault-tree formats.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Fta__Parser_hh
#define ZZ__Fta__Parser_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void readFaultTree(String tree_file, /*outputs:*/Gig& N, Vec<double>& ev_probs, Vec<String>& ev_names);
void readFtp(String ftp_file, /*outputs:*/Gig& N, Vec<double>& ev_probs, Vec<String>& ev_names, uint proc_no = UINT_MAX);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
