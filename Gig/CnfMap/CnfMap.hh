//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CnfMap.hh
//| Author(s)   : Niklas Een
//| Module      : Gig.CnfMap
//| Description : Techmap for CNF generation
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__CnfMap__CnfMap_hh
#define ZZ__Gig__CnfMap__CnfMap_hh

#include "ZZ_Gig.hh"
#include "Cut.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


struct Params_CnfMap {
    bool    map_to_luts;        // If FALSE, maps to 'gate_Npn4's instead
    uint    cuts_per_node;      // How many cuts should we store at most per node?
    uint    n_rounds;           // #iterations in techmapper. First iteration will always be depth optimal, later phases will use area recovery.
    bool    intro_muxes;        // Introduces MUXes first (faster, and often better quality)
    bool    quiet;

    Params_CnfMap() :
        map_to_luts(true),
        cuts_per_node(8),
        n_rounds(4),
        intro_muxes(true),
        quiet(false)
    {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// CNF generatation:


void cnfMap(Gig& N, Params_CnfMap P);
    // -- Maps the 'And', 'Xor' and 'Mux' gates of 'N' into 'Lut4's.
    // PRE-CONDITION: 'N' must be in canonical mode.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
