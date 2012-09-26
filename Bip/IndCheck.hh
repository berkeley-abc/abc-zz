//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : IndCheck.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Incremental induction checker.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Only 1-induction supported.
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__IndCheck_hh
#define ZZ__Bip__IndCheck_hh

#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Incremental induction test:


class IndCheck {
    bool        fwd;    // Direction of induction test.

    NetlistRef  N;      // Read-only.
    Netlist     F;      // Two-frame unrolling of 'N'
    SatStd      S;      // SAT encoding of 'F'

    Vec<Wire>   ff0;    // Flip flop map for frame 0 (used by 'add()' to map numbered flops)
    Vec<Wire>   ff1;    // Flip flop map for frame 1
    WMap<Wire>  x2f;    // Cleared for every call to 'add()'.
    WMap<Lit>   f2s;    // Persistent.
    Wire        disj_f; // Disjunction of states inserted by 'add()'.
    WZet        keep_f; // Gates in 'F' to keep (for sure) in clausification

    Clausify<SatStd> CF;
    EffortCB*        cb;

public:
    IndCheck(NetlistRef N, bool forward = true, EffortCB* cb = NULL);
        // -- 'N' must be strashed and have a 'fanout_count' pob.

    void  clear(lbool forward = l_Undef);
        // -- Restore class to initial state after construction, possibly changing direction.

    void  add(Wire x);  // -- Add states to the induction hypothesis.
    Wire  get();        // -- Get disjunction of all states ('w's) added.
    lbool run();        // -- Run SAT check to see if the disjunction of all states added is 1-inductive (=> returns 'l_True')

    const SatStd& solver() { return S; } // -- for statistics output
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
