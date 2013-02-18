//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Types and functions of generic nature, used throughout Bip.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Common_hh
#define ZZ__Bip__Common_hh

#include "Effort.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_MetaSat.hh"
#include "Clausify.hh"
#include "Cube.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compile-time parameters:


static const uint   VIRT_TIME_QUANTA = 100000;    // -- smaller values => issue callbacks more frequently
static const double SEC_TO_VIRT_TIME = 10000000;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for verification:


void initBmcNetlist(NetlistRef N0, const Vec<Wire>& props, NetlistRef N, bool keep_flop_init, Wire* fairness_monitor = NULL, bool toggle_bad = false);
void initBmcNetlist(NetlistRef N0, const Vec<Wire>& props, NetlistRef N, bool keep_flop_init, WMap<Wire>& xlat, Wire* fairness_monitor = NULL, bool toggle_bad = false);
void instantiateAbstr(NetlistRef N, const IntSet<uint>& abstr, /*outputs:*/ NetlistRef M, WMap<Wire>& n2m, IntMap<uint,uint>& pi2ff);
void addReset(NetlistRef N, int flop_num = num_NULL, int pi_num = num_ERROR);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counter-example:


template<class Map>
struct Cex_ {
  //________________________________________
  //  Public fields:

    Vec<Map> flops;      // -- for a concrete cex, 'flops[1..]' are redundant
    Vec<Map> inputs;     // -- both vectors can be address up to and INCLUDING 'depth()'

  //________________________________________
  //  Methods:

    void clear(bool dealloc = false) { flops.clear(dealloc); inputs.clear(dealloc); }

    uint size () const { return inputs.size(); }
    int  depth() const { return (int)inputs.size() - 1; }
    bool null () const { return depth() == -1; }

    void moveTo(Cex_& dst) { flops.moveTo(dst.flops); inputs.moveTo(dst.inputs); }

    void copyTo(Cex_& dst) const {
        dst.flops .setSize(flops .size());
        dst.inputs.setSize(inputs.size());
        for (uind i = 0; i < flops .size(); i++) flops [i].copyTo(dst.flops [i]);
        for (uind i = 0; i < inputs.size(); i++) inputs[i].copyTo(dst.inputs[i]);
    }
};

typedef Cex_<WMapL<lbool> >      Cex;
typedef Cex_<IntMap<int,lbool> > CCex;      // -- canonical counterexample -- expressed in PI/Flop numbers.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Model extraction:


Cube extractModel(SatStd&  S, const Clausify<SatStd>&  C, bool keep_inputs);
Cube extractModel(SatPfl&  S, const Clausify<SatPfl>&  C, bool keep_inputs);
Cube extractModel(MetaSat& S, const Clausify<MetaSat>& C, bool keep_inputs);
    // -- extract model projected onto state-variables and optionally also inputs.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ternary simulation:


struct XSimAssign {
    uint    value : 2;      // -- apply 'lbool_new()' to 'value' to get back the original value.
    uint    depth : 30;
    gate_id gate;

    XSimAssign() : value(l_Undef.value), depth(0x3FFFFFFFu), gate(gid_NULL) {}
    XSimAssign(uind d, GLit w, lbool v) : value((v ^ w.sign).value), depth(d), gate(w.id) {}

    bool null() const { return depth == 0x3FFFFFFFu; }
    typedef gate_id XSimAssign::*bool_type;
    operator bool_type() const { return null() ? 0 : &XSimAssign::gate; }
};



// Usage: Create a 'XSimulate' object. The netlist must not be modified during the life-span of
// this object. After simulating a counter-example, a series of propagate/commit or propagate/undo
// can be issued (a sequence of propagates should always be terminated by a commit or undo).
//
class XSimulate {
    Vec<XSimAssign>          undo;

    Vec<WZet>                tmp_seen;
    Vec<Pair<uint,gate_id> > tmp_Q;

    void init();

public:
    NetlistRef        N;        // Reference to external netlist.
    Vec<WMap<lbool> > sim;      // Current simulation values.

    XSimulate() : N(Netlist_NULL) {}
    XSimulate(NetlistRef N_) { init(N_); }

    void init(NetlistRef N_){
        N = N_;
        Assure_Pob0(N, fanouts);
        Assure_Pob0(N, up_order); }

    void simulate(const Cex& cex, const WZetL* abstr = NULL);
        // -- resimulate counterexample from scratch
    void propagate(XSimAssign assign, const WZetL* abstr = NULL , XSimAssign abort = XSimAssign());
        // -- incrementally update simulation (currently limited to updates to or from 'l_Undef' (such as 1->X or X->0))
    void propagateCommit();
    void propagateUndo();

    WMap<lbool>&       operator[](uind i)       { return sim[i]; }
    const WMap<lbool>& operator[](uind i) const { return sim[i]; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counter-example functions:


void translateCex(const Cex& from, NetlistRef N_to, Cex& to, const WMap<Wire>& n2m);
void translateCex(const Vec<Vec<lbool> >& pi, const Vec<Vec<lbool> >& ff, NetlistRef N_cex, /*out*/Cex& cex);
void translateCex(const CCex& ccex, NetlistRef N_cex, /*out*/Cex& cex);
void translateCex(const Cex& cex, CCex& ccex, NetlistRef N);
void makeCexInitial(NetlistRef N, Cex& cex);

bool verifyCex(NetlistRef N, const Vec<Wire>& props, Cex& cex, /*out*/Vec<uint>* fails_at = NULL, const Vec<Wire>* observe = NULL, Vec<Vec<lbool> >* obs_val = NULL);
void dumpCex(NetlistRef N, const Cex& cex, Out& out = std_out);

bool verifyInvariant(NetlistRef N, const Vec<Wire>& props, NetlistRef invariant, /*out*/uint* failed_prop = NULL);
static const uint VINV_not_inductive = UINT_MAX;        // }- May be returned as 'failed_prop'
static const uint VINV_not_initial   = UINT_MAX - 1;    // }

bool readInvariant(String filename, Vec<Wire>& props, NetlistRef H);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Copy formula:


Wire copyAndSimplify(Wire w, NetlistRef M);
Wire copyFormula(Wire w_src, NetlistRef N_dst);
    // 'w_src' should be from another netlist. Attribute 'number' on flops and PIs are used.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple SAT checks:


lbool checkDistinct(Wire w0, Wire w1, NetlistRef N_cex, Cex* cex, EffortCB* cb);
    // 'w0' and 'w1' may be from different netlists. 'copyFormula()' is used internally.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Unrollings:


typedef Vec<Pair<Vec<GLit>, Vec<GLit> > > UifList;

struct MemUnroll {
    MemInfo info;
    UifList uif;    // 'List of pairs '(addr, data)'.
};

void initMemu(NetlistRef N, Vec<MemUnroll>& memu);

struct Params_Unroll {
    WZet*           keep;       // If set, nodes with fanout > 1 will be added to this set.
    Vec<MemUnroll>* memu;       // Provide this vector if unrolling with memories.
    bool            uninit;     // Uninitialized trace -- flops are not set to their initial value in cycle 0, but left as flops (with their original number and no fanin).
    uint            number_pis; // If non-zero, inputs in the unrolling is numbered 'number_pis * frame# + orig_pi#'.

    Params_Unroll(WZet* keep_ = NULL, Vec<MemUnroll>* memu_ = NULL , bool uninit_ = false, uint number_pis_ = 0) :
        keep(keep_), memu(memu_), uninit(uninit_), number_pis(number_pis_) {}
};


struct CompactBmcMap {
    NetlistRef  F;
    WMap<uint>* remap;
    Vec<uint>*  retire;
    bool        owner;

    uint      age;
    Vec<GLit> map;

    CompactBmcMap() {}
   ~CompactBmcMap() { if (owner){ delete remap; delete retire; } }
    void init(NetlistRef F_, WMap<uint>* remap_, Vec<uint>* retire_, bool owner_);

    Wire operator[](Wire w) const {
        if (type(w) == gate_Const) return w.lit() + F;
        uint i = (*remap)[w];
        assert(i < map.size());
        return map[i] + F; }

    GLit& operator()(Wire w) {
        uint i = (*remap)[w]; assert(i < map.size()); return map[i]; }

    void advance();
};


Wire insertUnrolled(Wire w, uint k, NetlistRef F, Vec<WMap<Wire> >&   n2f, const Params_Unroll& P = Params_Unroll());
Wire insertUnrolled(Wire w, uint k, NetlistRef F, Vec<CompactBmcMap>& n2f, const Params_Unroll& P = Params_Unroll());


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// CNF-Mapped Unrollings:


void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, /*outputs:*/ MetaSat& S, Vec<LLMap<GLit,Lit> >& m2s);
void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, /*outputs:*/ SatPfl&  S, Vec<LLMap<GLit,Lit> >& m2s);
void lutClausify(NetlistRef M, Vec<Pair<uint,GLit> >& roots, bool initialized, /*outputs:*/ SatStd&  S, Vec<LLMap<GLit,Lit> >& m2s);
    // -- 'roots' is a list of pairs '(frame#, gate)'.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constraint handling:


void foldConstraints(NetlistRef N);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Write Header:


void writeHeader(String text, uint width);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
