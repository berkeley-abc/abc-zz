//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : VerilogWriter.hh
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Write back a parsed and flattened Verilog file.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__VerilogWriter_hh
#define ZZ__Liberty__VerilogWriter_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeFlatVerilog(Out& out, String module_name, NetlistRef N, const SC_Lib& L);
bool writeFlatVerilogFile(String filename, String module_name, NetlistRef N, const SC_Lib& L);
    // -- returns FALSE if file could not be created.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
