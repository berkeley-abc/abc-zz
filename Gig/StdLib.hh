//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdLib.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Collection of small, commonly useful functions operating on a Gig.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__StdLib_hh
#define ZZ__Gig__StdLib_hh

#include "GigExtra.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Misc:


String info(const Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple predicates:


macro bool isCO(Wire w) { return combOutput(w.type()); }
macro bool isCI(Wire w) { return combInput (w.type()); }

macro bool isSO(Wire w) { return seqOutput(w.type()); }
macro bool isSI(Wire w) { return seqInput (w.type()); }

macro bool isSeqElem(Wire w) { return seqElem(w.type()); }

bool isMux(Wire w, Wire& sel, Wire& d1, Wire& d0);
    // -- Returns TRUE if 'w' is the top AND-gate of a balanced, 3 AND-gate tree making up a MUX.
    // Note that XOR is a special case.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Topological orders:


void upOrder(const Gig& N, /*out*/Vec<GLit>& order);
void upOrder(const Gig& N, const Vec<GLit>& sinks, /*out*/Vec<GLit>& order);
    // -- Provides a topological order, starting from the CIs (or sinks) and ending with the COs.
    // If gates are already topologically ordered, that order will be preserved. NOTE! Constant
    // gates are NOT included in 'order', even if reachable.


void removeUnreach(const Gig& N, /*outs*/Vec<GLit>* removed = NULL, Vec<GLit>* order = NULL);
    // -- Remove all nodes not reachable from a combinational output. If 'removed' is given,
    // deleted nodes are returned through that vector. If 'order' is given, a topological
    // order for the remaining node is returned through that vector. 


// DEBUG:
void upOrderTest(const Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Unstrashed AIG construction:


macro Wire mkAnd  (Wire x, Wire y)           { return gig(x).add(gate_And).init(x, y); }
macro Wire mkOr   (Wire x, Wire y)           { return ~mkAnd(~x, ~y); }
macro Wire mkMux  (Wire s, Wire d1, Wire d0) { return ~mkAnd(~mkAnd(s, d1), ~mkAnd(~s, d0)); }
macro Wire mkXor  (Wire x, Wire y)           { return mkMux(x, ~y,  y); }
macro Wire mkEquiv(Wire x, Wire y)           { return mkMux(x,  y, ~y); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// LUTs:


void introduceMuxes(Gig& N);
    // -- introduce 'Mux' and 'Xor' gates for 3-input 'And's matching those functions. 
    // Netlist will be in 'gig_Xig' or 'gig_FreeForm' mode afterwards. 

void normalizeLut4s(Gig& N, bool ban_constant_luts = true);
    // -- Make sure unused pins are always the uppermost ones. Optionally also ban zero-input LUTs.

void putIntoNpn4(Gig& N);
    // -- Put the netlist into Npn4 form. Will convert the following gate types into 'gate_Npn4':
    // And, Xor, Mux, Maj, Buf, Not, Or, Equiv, Lut4


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
