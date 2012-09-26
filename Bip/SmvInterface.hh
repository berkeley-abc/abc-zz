//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SmvInterface.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Quick wrapper for calling Cadence SMV as an external tool
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__SmvInterface_hh
#define ZZ__Bip__SmvInterface_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void exportSmv(Out& out, NetlistRef N);
void exportSmv(String filename, NetlistRef N);
lbool callSmv(NetlistRef N, Cex& cex);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
