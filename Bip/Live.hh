//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Live.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Simlpe liveness checker based on the Biere transformation.
//| 
//| (C) Copyright 2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Live_hh
#define ZZ__Bip__Live_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Liveness {
    enum Engine {
        eng_NULL,   // -- just transform; output file (or PAR mode) must be specified
        eng_Bmc,
        eng_Treb,
        eng_TrebAbs,
        eng_Pdr2,
        eng_Imc,
    };

    enum { L2S = UINT_MAX, INC = UINT_MAX-1, };
        // -- special values for 'k'

    uint   k;       // k-liveness is default unless 'k == L2S'
    Engine eng;
    uint   bmc_max_depth;

    String aig_output;
    String gig_output;
    String witness_output;

    Params_Liveness() :
        k(L2S),
        eng(eng_Treb),
        bmc_max_depth(UINT_MAX)
    {}
};


lbool liveness(NetlistRef N0, uint fair_prop_no, const Params_Liveness& P = Params_Liveness(), Cex* out_cex = NULL, uint* out_loop = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
