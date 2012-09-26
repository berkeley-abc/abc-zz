//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cost.hh
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Compute area and delay using a constant size, constant delay model.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Verilog__Cost_hh
#define ZZ__Verilog__Cost_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


float  computeDelays(NetlistRef N, const Vec<Str>& uif_names, String delay_file);
double computeArea  (NetlistRef N, const Vec<Str>& uif_names, String sizes_file);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
