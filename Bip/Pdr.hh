//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Implement property-driven reachability
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Pdr_hh
#define ZZ__Bip__Pdr_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


enum PdrStats {
    pdr_Clauses,      // -- default
    pdr_Time,
    pdr_Vars,
};


struct Params_Pdr {
    uint64   seed;
    bool     minimal_cex;
    bool     quiet;
    PdrStats output_stats;
    bool     dump_invariant;

    Params_Pdr() :
        seed(0),
        minimal_cex(false),
        quiet(false),
        output_stats(pdr_Clauses),
        dump_invariant(false)
    {}
};


struct Info_Pdr {
    uint depth;

    Info_Pdr() : depth(0) {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


lbool propDrivenReach(NetlistRef        N0,
                      const Vec<Wire>&  props,
                      const Params_Pdr& P = Params_Pdr(),
                      Cex*              cex = NULL,
                      NetlistRef        invariant = NetlistRef(),
                      int*              bug_free_depth = NULL,
                      EffortCB*         cb = NULL     // -- info will be of type 'Info_Pdr*'
                      );


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
