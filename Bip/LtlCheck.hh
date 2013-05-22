//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LtlCheck.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : LTL checking based on circuit monitor synthesis.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__LtlCheck_hh
#define ZZ__Bip__LtlCheck_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_LtlCheck {
    enum Engine {
        eng_NULL,   // -- just transform
        eng_KLive,
        eng_L2sBmc,
        eng_L2sPdr
    };

    bool    inv;
    bool    free_vars;
    Engine  eng;
    String  witness_output;

    bool    debug_names;
    String  spec_gig;
    String  monitor_gig;
    String  final_gig;
    bool    fuzz_output;

    Params_LtlCheck() :
        inv(false),
        free_vars(false),
        eng(eng_L2sPdr),
        witness_output(""),
        debug_names(false),
        spec_gig(""),
        monitor_gig(""),
        final_gig(""),
        fuzz_output(false)
    {}
};


lbool ltlCheck(NetlistRef N, Wire spec, const Params_LtlCheck& P);
lbool ltlCheck(NetlistRef N, String spec_text, const Params_LtlCheck& P);
lbool ltlCheck(NetlistRef N, String spec_file, uint prop_no, const Params_LtlCheck& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
