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
#include "Maps.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Misc:


String info(const Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple predicates:


macro bool ofType(Wire     w, uint64 type_mask) { return (1ull << w.type()) & type_mask; }
macro bool ofType(GateType t, uint64 type_mask) { return (1ull << t) & type_mask; }

macro bool isCO(Wire w) { return combOutput(w.type()); }
macro bool isCI(Wire w) { return combInput (w.type()); }

macro bool isSO(Wire w) { return seqOutput(w.type()); }
macro bool isSI(Wire w) { return seqInput (w.type()); }

macro bool isSeqElem(Wire w)   { return seqElem(w.type()); }
macro bool isLogicGate(Wire w) { return logicGate(w.type()); }

bool isMux(Wire w, Wire& sel, Wire& d1, Wire& d0);
    // -- Returns TRUE if 'w' is the top AND-gate of a balanced, 3 AND-gate tree making up a MUX.
    // Note that XOR is a special case.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Topological order:


void upOrder(const Gig& N, /*out*/Vec<GLit>& order);
void upOrder(const Gig& N, const Vec<GLit>& sinks, /*out*/Vec<GLit>& order);
    // -- Provides a topological order, starting from the CIs (or sinks) and ending with the COs.
    // If gates are already topologically ordered, that order will be preserved. NOTE! Constant
    // gates are NOT included in 'order', even if reachable.


bool removeUnreach(const Gig& N, /*outs*/Vec<GLit>* removed = NULL, Vec<GLit>* order = NULL);
    // -- Remove all nodes not reachable from a combinational output EXCEPT combinational inputs.
    // If 'removed' is given, deleted nodes are returned through that vector. If 'order' is given,
    // a topological order for the remaining nodes is returned through that vector.
    // Returns TRUE if at least one node was removed.

bool checkUnreach(const Gig& N, /*outs*/Vec<GLit>* unreach = NULL, Vec<GLit>* order = NULL);
    // Same as 'removeUnreach()' but doesn't actually delete the nodes, just adds them to output
    // vectore 'unreach' (which is the same as 'removed' would have been).


// DEBUG:
void upOrderTest(const Gig& N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist state:


bool isCanonical(const Gig& N);
    // -- children has lower IDs than parents (= gates topologically ordered)

macro bool isReach(const Gig& N){
    // -- all gates are reachabled from combinational outputs (CIs excluded from this check)
    return !checkUnreach(N); }

macro bool isCompact(const Gig& N) {
    // -- netlist has no deleted gates (no "holes" in the vector of gates)
    return N.nRemoved() == 0; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Unstrashed AIG construction:


macro Wire mkAnd  (Wire x, Wire y)           { return gig(x).add(gate_And).init(x, y); }
macro Wire mkOr   (Wire x, Wire y)           { return ~mkAnd(~x, ~y); }
macro Wire mkMux  (Wire s, Wire d1, Wire d0) { return ~mkAnd(~mkAnd(s, d1), ~mkAnd(~s, d0)); }
macro Wire mkXor  (Wire x, Wire y)           { return mkMux(x, ~y,  y); }
macro Wire mkEquiv(Wire x, Wire y)           { return mkMux(x,  y, ~y); }

Wire copyGate(Wire w, Gig& N, WMapX<GLit>& xlat);
    // -- copy a gate 'w' from netlist 'M' to netlist 'N', using 'xlat' to translate its inputs
    // (with exception of 'gate_Seq', which are ignored since they break cycles and have to be
    // tied up last). NOTE! Don't forget to call 'xlat.initBuiltins()' before using this function.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// LUTs etc:


void introduceMuxes(Gig& N);
    // -- introduce 'Mux' and 'Xor' gates for 3-input 'And's matching those functions. 
    // Netlist will be in 'gig_Xig' or 'gig_FreeForm' mode afterwards. 

void normalizeLut4s(Gig& N, bool ban_constant_luts = true);
    // -- Make sure unused pins are always the uppermost ones. Optionally also ban zero-input LUTs.

void putIntoNpn4(Gig& N, WSeen* out_inverted = NULL);
    // -- Put the netlist into Npn4 form. Will convert the following gate types into 'gate_Npn4':
    //   And, Xor, Mux, Maj, Buf, Not, Or, Equiv, Lut4.

void putIntoLut4(Gig& N);
    // -- Same as 'putIntoNpn4()' but for 'gate_Lut4' instead of 'gate_Npn4'.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
