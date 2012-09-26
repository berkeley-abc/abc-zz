//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CnfMap.hh
//| Author(s)   : Niklas Een
//| Module      : CnfMap
//| Description : Techmap for CNF generation
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
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


class CnfMap {
    // Input:
    const Params_CnfMap& P;
    NetlistRef           N;

    // State:
    SlimAlloc<Cut>    mem;
    WMap<Array<Cut> > cutmap;
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<uint>        level;

    uint              round;
    uint64            mapped_area;
    uint64            mapped_luts;
    uint64            cuts_enumerated;

    // Output:
    WWMap&      n2m;
    NetlistRef  M;

    // Internal methods:
    float evaluateCuts(Array<Cut> cuts);
    void  generateCuts_And(Wire w, Vec<Cut>& out);
    void  generateCuts(Wire w);
    void  updateFanoutEst(bool instantiate);
    void  run();

    // Temporaries:
    Vec<Cut>   tmp_cuts;
    Vec<float> tmp_cut_area;
    Vec<Pair<uint,float> > tmp_cut_level;

public:
    CnfMap(NetlistRef N, Params_CnfMap P, /*outs:*/NetlistRef M, WWMap& n2m);
};


// Wrapper function:
macro void cnfMap(NetlistRef N, Params_CnfMap P, /*outs:*/NetlistRef M, WWMap& n2m) {
    CnfMap dummy(N, P, M, n2m); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
