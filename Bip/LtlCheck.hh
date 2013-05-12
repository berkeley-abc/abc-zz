//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LtlCheck.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : LTL checking based on circuit monitor synthesis.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__LtlCheck_hh
#define ZZ__Bip__LtlCheck_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void ltlCheck(NetlistRef N, Wire spec);
void ltlCheck(NetlistRef N, String spec_file, uint prop_no);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
