//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ExportImport.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Read and write various AIG/netlist formats.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__ExportImport_hh
#define ZZ__Netlist__ExportImport_hh

#include "Netlist.hh"
#include "StdPec.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


Declare_Exception(Excp_AigerParseError);
Declare_Exception(Excp_BlifParseError);

void writeTaig(Out& out, NetlistRef N);
void writeTaig(String filename, NetlistRef N);
    // -- write AIG (And, Flop, PI, PO, Const) as a Text-AIG. No names, only external numbers.

void readAiger(In& in, NetlistRef N, bool store_comment = true);
void readAigerFile(String filename, NetlistRef N, bool store_comment = true);
    // -- may throw 'Excp_AigerParseError'.

void removeFlopInit(NetlistRef N);
    // -- helper function to prepare file for AIGER writing (removes 'flop_init')

bool writeAiger(Out& out , NetlistRef N, Array<uchar> comment = Array<uchar>());
bool writeAigerFile(String filename, NetlistRef N, Array<uchar> comment = Array<uchar>());
    // -- returns FALSE if file could not be created. 

void makeAllOutputsProperties(NetlistRef N);

void readBlif(String filename, NetlistRef N, bool expect_aig, bool store_names);
    // -- read a flat BLIF file, which must be either an AIG or a 4-input LUT netlist.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
