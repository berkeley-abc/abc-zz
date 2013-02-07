//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pmc.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR inspired BMC implementation.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Pmc_hh
#define ZZ__Bip__Pmc_hh

#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Pmc {
    bool        multi_sat;
    bool        use_f_inf;
    bool        term_check;
    bool        cube_gen;
    SolverType  sat_solver;

    Params_Pmc() :
        multi_sat(false),
        use_f_inf(true),
        term_check(true),
        cube_gen(true),
        sat_solver(sat_Abc)
    {}
};


void addCli_Pmc(CLI& cli);
void setParams(const CLI& cli, Params_Pmc& P);


bool pmc(NetlistRef N, const Vec<Wire>& props, const Params_Pmc& P, Cex* cex);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
