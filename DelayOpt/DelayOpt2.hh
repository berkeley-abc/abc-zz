//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelayOpt2.hh
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Second attempt at delay optimization.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__DelayOpt__DelayOpt2_hh
#define ZZ__DelayOpt__DelayOpt2_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"
#include "ZZ_CmdLine.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_ContSize {
    enum CostFun { DELAY_MAX, DELAY_SOFTMAX };
    enum EvalFun { LINEAR, SQUARE, MAX, SOFTMAX };
    enum MoveTyp { NUDGE, NEWTON };

    CostFun cost_fun;       // DELAY_SOFTMAX
    float   crit_epsilon;   // 0.0001   (fraction of length of critical path)
    uint    refine_orbits;  // 8 
    uint    eval_levels;    // 3
    EvalFun eval_fun;       // SOFTMAX
    float   eval_dx;        // 0.001    (for approximating first order derivative)
    float   eval_ddx;       // 0.01     (for approximating second derivative; needs to be bigger than 'eval_dx')
    float   step_init;      // 1
    float   step_quit;      // 0.25
    float   step_reduce;    // 0.95
    float   move_lim;       // 0.1      (moves with less improvement than this number times the best move are not performed)
    MoveTyp move_type;      // NEWTON
    uint    discr_freq;     // 6
    float   discr_upbias;   // 0.66     (0.5 = no bias)


    Params_ContSize() :
        cost_fun     (DELAY_SOFTMAX),
        crit_epsilon (0.0001),
        refine_orbits(8),
        eval_levels  (3),
        eval_fun     (SOFTMAX),
        eval_dx      (0.001),
        eval_ddx     (0.01),
        step_init    (1),
        step_quit    (0.15),
        step_reduce  (0.75),
        move_lim     (0.1),
        move_type    (NEWTON),
        discr_freq   (3),
        discr_upbias (0.66)
    {}
};


struct Params_DelayOpt {
    float req_time;
    uint  approx;
    bool  forget_sizes;
    bool  filter_groups;
    uint  prebuf_lo;         // -- 0 means no pre-buffering
    uint  prebuf_grace;
    Params_ContSize C;

    uint verbosity;

    // Derived:
    uint prebuf_hi;

    Params_DelayOpt() :
        req_time(0),
        approx(0),
        forget_sizes(true),
        filter_groups(true),
        prebuf_lo(0),
        prebuf_grace(25),
        verbosity(1),
        prebuf_hi(0)
    {}
};


void addCli_DelayOpt(CLI& cli);
void setParams(const CLI& cli, Params_DelayOpt& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void optimizeDelay2(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, const Params_DelayOpt& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
