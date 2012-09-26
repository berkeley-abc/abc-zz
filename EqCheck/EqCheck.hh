//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : EqCheck.hh
//| Author(s)   : Niklas Een
//| Module      : EqCheck
//| Description : Combinational equivalence checking of two design through ABC.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__EqCheck__EqCheck_hh
#define ZZ__EqCheck__EqCheck_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void eqCheck(NetlistRef N1, NetlistRef N2, const SC_Lib& L, String aiger_file = "");


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
