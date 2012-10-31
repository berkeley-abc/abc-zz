//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bmc.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Bounded model checking.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Bmc_hh
#define ZZ__Bip__Bmc_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


struct Params_Bmc {
    SolverType sat_solver;      // -- SAT-solver to use.
    bool    simple_tseitin;
    bool    quant_claus;
    uint    la_steps;           // -- look-ahead frames
    double  la_decay;           // -- relative focus between step k and k+1 (< 1 means less focus on k+1)
    bool    quiet;

    Params_Bmc() :
        sat_solver    (sat_Msc),
        simple_tseitin(false),
        quant_claus   (false),
        la_steps      (1),
        la_decay      (0.8),
        quiet         (false)
    {}
};


struct Info_Bmc {
    uint depth;

    Info_Bmc() : depth(0) {}
};


struct EffortCB_BmcDepth : EffortCB {
    uint depth_limit;
    EffortCB_BmcDepth(uint d) : depth_limit(d) {}
        // -- Check upto but not including 'd' transitions.

    bool operator()() { return ((Info_Bmc*)info)->depth < depth_limit; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


lbool bmc(NetlistRef        N0,
          const Vec<Wire>&  props,
          const Params_Bmc& P              = Params_Bmc(),
          Cex*              cex            = NULL,
          int*              bug_free_depth = NULL,
          EffortCB*         cb             = NULL,      // -- info will be of type 'Info_Bmc*'
          uint              max_depth      = UINT_MAX   // -- 0 means initial state will still be checked
         );

Wire staticBmc(NetlistRef N0, const Vec<Wire>& props, uint k0, uint k1, bool initialized, bool simplify, NetlistRef F_out);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
