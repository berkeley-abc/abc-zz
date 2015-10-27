//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdPob.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Declares and registers some standard netlist objects used through-out ZZ.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| NOTE! Pecs with only one meaningful instantiation (such as 'Pec_Strash') are instantiated
//| directly in 'StdPec.cc'. This module only contains instantiations of generic pecs such as
//| 'Pec_VecWire' or 'Pec_RawData'.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "StdPob.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Standard POBs:


Register_Pob(strash, Strash);
Register_Pob(fanouts, Fanouts);
Register_Pob(fanout_count, FanoutCount);
Register_Pob(up_order, UpOrder);
Register_Pob(flop_init, FlopInit);
Register_Pob(aiger_comment, RawData);
Register_Pob(properties, VecWire);
Register_Pob(constraints, VecWire);
Register_Pob(fair_properties, VecVecWire);
Register_Pob(fair_constraints, VecWire);
Register_Pob(init_bad, VecWire);
Register_Pob(init_nl, Netlist);
Register_Pob(reset, Wire);
Register_Pob(mem_info, MemInfo);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
