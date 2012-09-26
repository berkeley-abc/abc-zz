//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Various support functions for combining Verilog and Liberty files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__Common_hh
#define ZZ__Liberty__Common_hh

#include "ZZ_Liberty.hh"
#include "Parser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void computeUifMap(const Vec<VerilogModule>& modules, const SC_Lib& L, /*out*/IntMap<uint,uint>& mod2cell);
void remapUifs(NetlistRef N, const IntMap<uint,uint>& mod2cell);

void verifyPinsSorted(const Vec<VerilogModule>& modules, const IntMap<uint,uint>& mod2cell, SC_Lib& L);

void genPrelude(const SC_Lib& L, String& out, bool print_warnings = false);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
