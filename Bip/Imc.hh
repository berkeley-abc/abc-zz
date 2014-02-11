//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Imc.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Interpolation based model checking; top-level algorithm.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Imc_hh
#define ZZ__Bip__Imc_hh

#include "ZZ_Bip.Common.hh"
#include "IndCheck.hh"
#include "ImcTrace.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


struct Params_ImcStd {
    bool    fwd;
    uint    first_k;
    bool    simplify_itp;
    bool    prune_itp;
    bool    simple_tseitin;     // -- only for reference benchmarking
    bool    quant_claus;
    bool    spin;
    bool    quiet;

    Params_ImcStd() :
        fwd           (true),
        first_k       (0),
        simplify_itp  (true),
        prune_itp     (false),
        simple_tseitin(false),
        quant_claus   (false),
        spin          (false),
        quiet         (false)
    {}
};


struct Info_ImcStd {
    uint            k;
    uint            d;
    const ImcTrace* imc;    // }- will be NULL before the first image computation (when
    const IndCheck* ind;    // }  checking that bad and init don't intersect)

    Info_ImcStd() : k(0), d(0), imc(NULL), ind(NULL) {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


lbool imcStd(NetlistRef           N0,
             const Vec<Wire>&     props,
             const Params_ImcStd& P = Params_ImcStd(),
             Cex*                 cex = NULL,
             NetlistRef           invariant = NetlistRef(),
             int*                 bug_free_depth = NULL,
             EffortCB*            cb = NULL     // -- info will be of type 'Info_ImcStd*'
             );

void imcPP(NetlistRef N0, const Vec<Wire>& props);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
