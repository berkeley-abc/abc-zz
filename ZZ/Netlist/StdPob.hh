//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdPob.hh
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

#ifndef ZZ__Netlist__StdPob_hh
#define ZZ__Netlist__StdPob_hh

#include "Netlist.hh"
#include "StdPec.hh"
#include "DynFanouts.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Standard POBs:


Declare_Pob(strash, Strash);
Declare_Pob(fanouts, Fanouts);
Declare_Pob(fanout_count, FanoutCount);
Declare_Pob(dyn_fanouts, DynFanouts);
Declare_Pob(up_order, UpOrder); // -- flops will appear on the input side
Declare_Pob(flop_init, FlopInit);
Declare_Pob(aiger_comment, RawData);
Declare_Pob(properties, VecWire);
Declare_Pob(constraints, VecWire);
Declare_Pob(fair_properties, VecVecWire);
Declare_Pob(fair_constraints, VecWire);
Declare_Pob(init_bad, VecWire); // -- a vector of size 2; elem0 is a PO for "init", elem1 for "bad"
Declare_Pob(init_nl, Netlist);
Declare_Pob(reset, Wire);
Declare_Pob(mem_info, MemInfo);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Convenience macros:


// NOTE! The last parameter is always just a *name*. A variable with that name will be introduced
// by the macro.


// Pob 'fanouts' must be present:
#define For_Outputs(w, c)                                                               \
    if (uint dummy__##c = 0); else                                                      \
    for (Get_PobText(netlist(w), fanouts, fanouts__); dummy__##c == 0; dummy__##c++)    \
    if (Connect c = Connect(Wire_NULL, 0)); else                                        \
    for (uint i__##c = 0; i__##c < fanouts__[w].size(); i__##c++)                       \
        if (c = fanouts__[w][i__##c], false); else


// Pob 'up_order' must be present:
#define For_UpOrder(N, w)                                                       \
    if (uint dummy__##w = 0); else                                              \
    for (Get_PobText(N, up_order, up_order__); dummy__##w == 0; dummy__##w++)   \
    if (Wire w = Wire_NULL); else                                               \
    for (uintg i__##w = 0; i__##w < up_order__.size(); i__##w++)                \
        if (w = up_order__[i__##w], false); else

#define For_DownOrder(N, w)                                                     \
    if (uint dummy__##w = 0); else                                              \
    for (Get_PobText(N, up_order, up_order__); dummy__##w == 0; dummy__##w++)   \
    if (Wire w = Wire_NULL); else                                               \
    for (uintg i__##w = up_order__.size(); i__##w > 0; i__##w--)                \
        if (w = up_order__[i__##w - 1], false); else


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
