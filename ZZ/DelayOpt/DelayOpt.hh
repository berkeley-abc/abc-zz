//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelayOpt.hh
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Buffering and resizing using standard cell library.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__DelayOpt__DelayOpt_hh
#define ZZ__DelayOpt__DelayOpt_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void preBuffer(NetlistRef N, const SC_Lib& L, uint branchf, uint buf_sym, bool quiet = false);

void optimizeDelay(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, uint pre_buffer, bool forget_sizes);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
