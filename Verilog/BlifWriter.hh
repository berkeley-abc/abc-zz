//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BlifWriter.hh
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Write a parsed and flattened Verilog file.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Verilog__BlifWriter_hh
#define ZZ__Verilog__BlifWriter_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeFlatBlif(Out& out, String module_name, NetlistRef N, const SC_Lib& L);
bool writeFlatBlifFile(String filename, String module_name, NetlistRef N, const SC_Lib& L);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
