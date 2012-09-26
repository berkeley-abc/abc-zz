//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Treb.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Second go at the PDR algorithm, with several generalizations.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 'Treb' is short for Trebuchet. A slightly longer name would have been:
//|
//| Incremental Inductive Invariant Generation through Property Directed Reachability Strengthening
//| using Stepwise Relative Induction.
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Treb_hh
#define ZZ__Bip__Treb_hh

#include "ZZ_CmdLine.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// The Trebuchet:


struct Params_Treb {
    enum Weaken { NONE, SIM, JUST };

    uint64  seed;               // Random seed. Currently only used for 'rec_nonind'.
    bool    bwd;                // Backward PDR (NOT IMPLEMENTED).
    bool    multi_sat;          // Use multiple SAT solvers?
    bool    use_activity;       // Use activity heuristic.
    Weaken  weaken;             // Method to weaken proof-obligations with.
    uint    rec_nonind;         // Recurse into non-inductive region (#tries).
    uint    targ_enl;           // Target enlargement. Currently don't produce correct CEXs.
    uint    semant_coi;         // Semantic cone-of-influence (bit0=before, bit1=after forward-propagation).
    bool    skip_prop;          // Don't run forward-propagate 
    double  restart_lim;        // Initial restart limit. 0=no restarts
    double  restart_mult;       // Restart limit multiplier (>= 1)
    bool    use_abstr;          // Self-abstraction
    bool    pdr_refinement;     // Use PDR based refinement for self-abstraction (instead of CEX based).
    bool    cmb_refinement;     // Combine CEX and PDR based refinement.
    bool    abc_refinement;     // Use ABC to refine 
    bool    sort_pob_size;      // Sort proof-obligations on size instead of priority.
    uint    pre_cubes;          // Size of pre-image.
    float   orbits;             // How many orbits should 'generlize()' try?
    bool    gen_with_cex;       // Store counterexamples in 'generalize()' to speedup multiple orbits.
    bool    hq;                 // High quality generalization (slower)
    uint    dump_invar;         // Dump invariant (0=no, 1=clauses, 2=PLA).
    bool    quiet;              // Suppress output.

    Params_Treb() :
        seed(0),
        bwd(false),
        multi_sat(false),
        use_activity(true),
        weaken(SIM),
        rec_nonind(0),
        targ_enl(0),
        semant_coi(0),
        skip_prop(false),
        restart_lim(0),
        restart_mult(1.2),
        use_abstr(false),
        pdr_refinement(false),
        cmb_refinement(false),
        abc_refinement(false),
        sort_pob_size(false),
        pre_cubes(1),
        orbits(2),
        gen_with_cex(false),
        hq(false),
        dump_invar(0),
        quiet(false)
    {}
};


struct Info_Treb {
    uint depth;

    Info_Treb() : depth(0) {}
};


lbool treb( NetlistRef          N0,
            const Vec<Wire>&    props,
            const Params_Treb&  P,
            Cex*                cex,
            NetlistRef          invariant,
            int*                bug_free_depth,
            EffortCB*           cb
            );

void addCli_Treb(CLI& cli);
void setParams(const CLI& cli, Params_Treb& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
