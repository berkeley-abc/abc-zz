//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelaySweep.hh
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Initial buffering and sizing using non-incremental sweeping algorithm.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__DelayOpt__DelaySweep_hh
#define ZZ__DelayOpt__DelaySweep_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void initialBufAndSize(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, uint approx);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
