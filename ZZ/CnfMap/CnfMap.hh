//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CnfMap.hh
//| Author(s)   : Niklas Een
//| Module      : CnfMap
//| Description : Techmap for CNF generation
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__CnfMap__CnfMap_hh
#define ZZ__CnfMap__CnfMap_hh

#include "ZZ_Netlist.hh"
#include "Cut.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


struct Params_CnfMap {
    uint    cuts_per_node;      // How many cuts should we store at most per node?
    uint    n_rounds;           // #iterations in techmapper. First iteration will always be depth optimal, later phases will use area recovery.
    bool    quiet;

    Params_CnfMap() :
        cuts_per_node(10),
        n_rounds(4),
        quiet(false)
    {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// CNF generatation:


void cnfMap(NetlistRef N, Params_CnfMap P, /*outs:*/NetlistRef M, WWMap& n2m);
    // -- Supported gate types are: And, PI, PO, Flop. 
    // Output netlist 'M' will contain: Npn4, PI, PO, Flop.  


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
