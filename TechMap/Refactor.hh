//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Refactor.hh
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Refactor big conjunctions and XORs
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__TechMap__Refactor_hh
#define ZZ__TechMap__Refactor_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Refactor {
    uint max_conj_size;
    bool timing_aware;      // <<== ignored for now
    bool quiet;

    Params_Refactor() :
        max_conj_size(100),
        timing_aware(true),
        quiet(false)
    {}
};


void refactor(Gig& N, WMapX<GLit>& remap, const Params_Refactor& P);

void introduceXorsAndMuxes(Gig& N, uint fanout_lim = 2);
    // -- may be moved to Gig/StdLib (only used in command line mode as part of parsing)


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
