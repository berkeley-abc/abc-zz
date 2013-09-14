//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bmc.hh
//| Author(s)   : Niklas Een
//| Module      : Gip
//| Description : Multi-property BMC procedure
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gip__Bmc_hh
#define ZZ__Gip__Bmc_hh

#include "ZZ_Gip.Common.hh"
#include "ZZ_MetaSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Bmc {
    SolverType sat_solver;

    Params_Bmc() :
        sat_solver(sat_Msc)
    {}
};


void bmc(Gig& N0, Params_Bmc& P, EngRep& R, const Vec<uint>& props);
void bmc(Gig& N0, Params_Bmc& P, EngRep& R);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
