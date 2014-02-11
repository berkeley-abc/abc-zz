//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdLib.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Netlist standard library. 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__StdLib_hh
#define ZZ__Netlist__StdLib_hh

#include "Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Convenience functions: 


// NOTE! Use 's_And' etc. for strashed netlists.

macro Wire mk_And  (Wire x, Wire y) {
    assert(nl(x) == nl(y));
    return netlist(x).add(And_(), x, y); }

macro Wire mk_Or   (Wire x, Wire y)           { return ~mk_And(~x, ~y); }
macro Wire mk_Mux  (Wire c, Wire d1, Wire d0) { return ~mk_And(~mk_And(c, d1), ~mk_And(~c, d0)); }
macro Wire mk_Xor  (Wire x, Wire y)           { return mk_Mux(x, ~y,  y); }
macro Wire mk_Equiv(Wire x, Wire y)           { return mk_Mux(x,  y, ~y); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sources, sinks, flops:


macro bool isGlobalSource(Wire w) { return type(w) == gate_PI ||                       type(w) == gate_Flop || type(w) == gate_MFlop || type(w) == gate_Uif; }
macro bool isGlobalSink  (Wire w) { return type(w) == gate_PO || type(w) == gate_SO || type(w) == gate_Flop || type(w) == gate_MFlop || type(w) == gate_Uif; }
macro bool isFlopType    (Wire w) { return type(w) == gate_Flop || type(w) == gate_MFlop || type(w) == gate_Uif; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cleanup:


void removeUnreach(NetlistRef N, Vec<GLit>* removed_gates = NULL, bool keep_sources = true);
    // -- PIs are kept unless 'keep_sources' is FALSE.
macro void removeAllUnreach(NetlistRef N, Vec<GLit>* removed_gates = NULL) {
    removeUnreach(N, removed_gates, false); }

void removeUnreach(NetlistRef N, Vec<GLit>& Q, bool keep_sources = true);
void removeUnreach(Wire w, bool keep_sources = true);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Topological orders:


void upOrder(NetlistRef N, /*out*/Vec<gate_id>& order, bool flops_last = false, bool strict_sinks = true);
void upOrder(const Vec<Wire>& sinks, /*out*/Vec<gate_id>& order, bool flops_last = false);
void upOrder(const Vec<Wire>& sinks, VPred<Wire>& stop_at, /*out*/Vec<gate_id>& order, bool flops_last = false);
    // -- will stop recursion if 'stop_at()' returns TRUE (gate not included in order).
    //
    // NOTE! The resulting order never contains the constant gates.
    //
    // NOTE! Flops are treated as PIs by default. If 'flops_last' is TRUE, they are
    // put last in the order (and won't exist on the inputs side).

void topoOrder(NetlistRef N, /*out*/Vec<GLit>& order);
    // -- currently only works for combinational circuits; treats 'Uif' as logic gates, not 
    // sequentials (as suggested by 'isGlobalSink()' and 'isGlobalSource()'. NOTE! 'order'
    // is cleared first by this function.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Miscellaneous:


String info(NetlistRef N);
String verifInfo(NetlistRef N);
void nameByCurrentId(NetlistRef N, bool only_touch_unnamed = false);

void renumberPIs  (NetlistRef N, Vec<int>* orig_num = NULL);
void renumberPOs  (NetlistRef N, Vec<int>* orig_num = NULL);
void renumberFlops(NetlistRef N, Vec<int>* orig_num = NULL);

macro void renumber(NetlistRef N){
    renumberPIs(N);
    renumberPOs(N);
    renumberFlops(N); }

bool checkNumberingPIs  (NetlistRef N, bool check_dense = false);
bool checkNumberingPOs  (NetlistRef N, bool check_dense = false);
bool checkNumberingFlops(NetlistRef N, bool check_dense = false);
bool checkNumbering     (NetlistRef N, bool check_dense = false);
    // -- check that all external elements (PIs, POs, Flops) are numbered distinctly

macro int nextNum_PI  (NetlistRef N) { int n = 0; For_Gatetype(N, gate_PI  , w) newMax(n, attr_PI  (w).number + 1); return n; }
macro int nextNum_PO  (NetlistRef N) { int n = 0; For_Gatetype(N, gate_PO  , w) newMax(n, attr_PO  (w).number + 1); return n; }
macro int nextNum_Flop(NetlistRef N) { int n = 0; For_Gatetype(N, gate_Flop, w) newMax(n, attr_Flop(w).number + 1); return n; }
    // -- return the highest "number" of a type plus one.

uint sizeOfCone(Wire w);
    // -- Returns the size of the fanin logic cone of 'w', stopping at global sources.
    // NOTE! If 'w' is a flop (global source and sink), '1' is returned.

bool detectCombCycle(NetlistRef N, Vec<Wire>* cycle = NULL);
    // -- find and return a combinational cycle (returns FALSE if none found) (for debugging)

void computeDominators(NetlistRef N, WMap<gate_id>& dom);
    // -- compute (immediate) domintators for all nodes, treating flops as PIs. Dominating
    // flops are given IDs 'id(w_flop) + N.size()' in the 'dom[]' array.

void splitFlops(NetlistRef N, bool cut_up = false);
    // -- split each 'Flop' into a pair of 'Flop' (= state-input) and 'SO' (state-output,
    // given the same 'number' attribute as the flop)

bool removeBuffers(NetlistRef N);
    // -- Eliminates all gate of type 'gate_Buf', forwarding names to the remaining gates.
    // Returns FALSE if an infinite loop was detected among the buffers.

void migrateNames(Wire from, Wire into, Str prefix = Str(), bool skip_auto_generated = false);
    // -- Copy all names for wire 'from' to wire 'into' (possibly from a different netlist). If
    // 'prefix' is given, all names are prefixed by this string.

void transitiveFanin(Wire w_sink, WZet& seen);
    // -- Mark the (combinational) transitive fanin by adding gates to 'seen'. 


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Collect Conjunctions:


bool isMux(Wire w, Wire& sel, Wire& d1, Wire& d0);
macro bool isMux(Wire w) { Wire sel, d1, d0; return isMux(w, sel, d1, d0); }

void countFanouts(Wire w, WMap<uint>& n_fanouts);

bool collectConjunction(Wire w, const WZet& keep           , WZetS& seen, Vec<Wire>& out_conj);
bool collectConjunction(Wire w, const WMap<uint>& n_fanouts, WZetS& seen, Vec<Wire>& out_conj);
bool collectConjunction(Wire w,                              WZetS& seen, Vec<Wire>& out_conj);
    // -- 'seen' is a temporary provided by the caller. It will be cleared before its used.
    // Returns FALSE if 'w' is constant FALSE ('out_conj' is then undefined).


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Normalize:


macro void translateInputs(Wire w, const WWMap& xlat) {
    For_Inputs(w, v)
        if (xlat[v] != v.lit())
            w.set(Iter_Var(v), xlat[v]); }


void introduceXorsAndMuxes(NetlistRef N);
void introduceOrs(NetlistRef N);
void introduceBigAnds(NetlistRef N);

void normalizeXors(NetlistRef N);

bool hasGeneralizedGates(NetlistRef N);
    // -- Does 'N' contain any generalize AIG gates?

void expandGeneralizedGates(NetlistRef N);
    // -- Convert a generalized AIG into a plain (binary ANDs only) AIG. NOTE! Names may be lost.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// "Type" control:


void assertAig(NetlistRef N, const String& where_text);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
