//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Abstraction.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Localization using counterexample- and proof-based abstraction.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| This is an old module. Some coding conventions has changed since its conception.
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Abstraction_hh
#define ZZ__Bip__Abstraction_hh

#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_LocalAbstr {
    // Termination:
    uint64  max_conflicts;  // Stop at this number of conflicts
    uint64  max_inspects;   // Stop at this number of BCP operations 
    uint    max_depth;      // Stop at this depth
    uint    stable_lim;     // LEGACY: continue beyond 'max_depth' until this many steps have been without refinement
    uint    bob_stable;     // LEGACY: go up to at least this depth, then stop when the latest stretch of stable steps are greater than depth/2
    double  cpu_timeout;    // Stop after this amount of time (non-deterministic!)
    bool    renumber;       // Renumber PIs and FFs in AIGER file (otherwise dummy PIs/FFs are tied to zero).
    String  dump_prefix;    // If non-empty, AIGER files of abstract models are written while running

    // Experimental:
    bool    randomize;

    // Report / Debug:
    bool quiet;
    int  sat_verbosity;

    Params_LocalAbstr() :
        max_conflicts(UINT64_MAX),
        max_inspects (UINT64_MAX),
        max_depth    (UINT_MAX),
        stable_lim   (0),
        bob_stable   (UINT_MAX),
        cpu_timeout  (DBL_MAX),
        renumber     (false),
        dump_prefix  (""),
        randomize    (false),
        quiet        (false),
        sat_verbosity(0)
    {}
};


void localAbstr(NetlistRef N0,
                Vec<Wire>& props,
                const Params_LocalAbstr& P,
                /*in out*/IntSet<uint>& abstr,
                /*out*/Cex* cex,
                /*out*/int& bug_free_depth);


void writeAbstrAiger(NetlistRef N, const IntSet<uint>& abstr, String aig_filename, bool renumber_, bool quiet);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
