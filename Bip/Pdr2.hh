//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr2.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR with learned cubes over arbitrary internal variables.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Pdr2_hh
#define ZZ__Bip__Pdr2_hh

#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Pdr2 {
    enum Recycling { NONE, SOLVER, VARS };
    enum PobCone   { JUST, PSEUDO_JUST, COI };
    enum JustStrat { FIRST, LOWEST_LEVEL, ACTIVITY };

    Recycling   recycling;
    PobCone     pob_cone;
    bool        pob_internals;
    bool        pob_weaken;
    bool        pob_rotate;
    JustStrat   just_strat;
    bool        use_activity;
    bool        randomize;
    bool        restarts;
    uint        gen_orbits;
    bool        tweak_cut;
    SolverType  sat_solver;

    Params_Pdr2() :
        recycling    (SOLVER),
        pob_cone     (JUST),
        pob_internals(false),
        pob_weaken   (true),
        pob_rotate   (false),
        just_strat   (FIRST),
        use_activity (true),
        randomize    (false),
        restarts     (false),
        gen_orbits   (2),
        tweak_cut    (false),
        sat_solver   (sat_Abc)
    {}
};


void addCli_Pdr2(CLI& cli);
void setParams(const CLI& cli, Params_Pdr2& P);


bool pdr2(NetlistRef N, const Vec<Wire>& props, const Params_Pdr2& P, Cex* cex, NetlistRef N_invar);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
