//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ExportDot.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Export a netlist to a GraphViz DOT file.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__ExportDot_hh
#define ZZ__Bip__ExportDot_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void growRegion(NetlistRef N, WZet& region, String grow_spec, uint lim);

void writeDot(String filename, NetlistRef N, Vec<String>* uif_names = NULL);
void writeDot(String filename, NetlistRef N, const WZet& region, Vec<String>* uif_names = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
