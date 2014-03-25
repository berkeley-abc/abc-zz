//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Implement property-driven reachability
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Two possible solutions to the collaborating PDR problem:
//| 
//|   (1) Check each incoming cube. If it is valid, put it in its right place, otherwise in F[1]
//|       (conservative, but maybe we cannot afford more). It will be pushed forward when a new
//|       frame is added.
//|       
//|   (2) Send "finished frame" messages as well, and abort the "recusive block clause" loop upon 
//|       receiving this message and go to propagation phase (which should prove the property at
//|       the final frame -- check this!)
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Pdr.hh"
#include "ZZ/Generics/Heap.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"
#include "Bmc.hh"
#include "ParClient.hh"

//#define RECURSE_INTO_NONINDUCTIVE
#define RELATIVE_INDUCTION              // To be turned off for evaluation purposes only!
#define TERNARY_SIMULATION              // To be turned off for evaluation purposes only!
//#define FILTER_COI_IN_SAT
//#define FIND_INVAR_SUBSET


namespace ZZ {
using namespace std;

ZZ_PTimer_Add(PDR_ALL);
ZZ_PTimer_Add(Other);
ZZ_PTimer_Add(COI);
ZZ_PTimer_Add(INIT);
ZZ_PTimer_Add(Subsume);
ZZ_PTimer_Add(SIM);
ZZ_PTimer_Add(SAT);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Property Driven Reachability:


//=================================================================================================
// -- Exception for callback aborting:


struct Excp_Pdr_Abort : Excp {};


//=================================================================================================
// -- Helper type 'Pdr_Cla':


struct Pdr_Cla_Data {
    uint64  abstr;
    uint    sz;
    uint    refC;
    GLit    data[1];
};


// A 'Pdr_Cla' clause is sorted and static in size, contains a 64-bit abstraction and does
// reference counting for the underlying literal vector.
struct Pdr_Cla {
    Pdr_Cla_Data* ptr;

    static uint allocSize(uint n_elems) { return sizeof(Pdr_Cla_Data) - sizeof(GLit) + sizeof(GLit) * n_elems; }

  //________________________________________
  //  Constructors:

    Pdr_Cla() : ptr(NULL) {}

    Pdr_Cla(const Vec<GLit>& ps) {
        ptr = (Pdr_Cla_Data*)ymalloc<char>(allocSize(ps.size()));
        ptr->abstr = 0;
        ptr->sz = ps.size();
        ptr->refC = 1;
        for (uind i = 0; i < ps.size(); i++){
            ptr->abstr |= uint64(1) << (ps[i].data() & 63);
            ptr->data[i] = ps[i]; }
        Array<GLit> proxy(ptr->data, ptr->sz);
        sort(proxy);
    }

  //________________________________________
  //  Reference counting:

    Pdr_Cla(const Pdr_Cla& other) {
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
    }

    Pdr_Cla& operator=(const Pdr_Cla& other) {
        if (this == &other) return *this;
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz)); }
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
        return *this;
    }

   ~Pdr_Cla() {
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz)); }
    }

  //________________________________________
  //  Methods:

    uint   size      ()       const { return ptr->sz; }
    GLit&  operator[](uint i) const { return ptr->data[i]; }
    uint64 abstr     ()       const { return ptr->abstr; }
    bool   null      ()       const { return ptr == NULL; }

    void invert() {
        assert(!null());
        for (uint i = 0; i < ptr->sz; i++)
            ptr->data[i] = ~ptr->data[i];
        ptr->abstr = ((ptr->abstr & 0xAAAAAAAAAAAAAAAAull) >> 1) | ((ptr->abstr & 0x5555555555555555ull) << 1);
    }
};


template<> fts_macro void write_(Out& out, const Pdr_Cla& c) {
    if (c.null()) out += "<null>";
    else write_(out, slice(c.ptr->data[0], c.ptr->data[c.ptr->sz])); }


macro bool operator==(const Pdr_Cla& x, const Pdr_Cla& y)
{
    if (x.ptr == y.ptr) return true;
    if (x.null() || y.null()) return false;
    if (x.abstr() != y.abstr()) return false;
    return vecEqual(x, y);
}


bool subsumes(const Pdr_Cla& small_, const Pdr_Cla& big)
{
    if (small_.abstr() & ~big.abstr()) return false;
    uint j = 0;
    for (uint i = 0; i < small_.size();){
        if (j >= big.size())
            return false;
        if (small_[i] == big[j]){
            i++;
            j++;
        }else
            j++;
    }
    return true;
}


//=================================================================================================
// -- Helper type 'ProofObl':


struct ProofObl_Data {
    uint        frame;
    uint        prio;
    Pdr_Cla     state;      // -- really a cube, not a clause

    RefC<ProofObl_Data> next;
    uint                refC;
};


struct ProofObl : RefC<ProofObl_Data> {
    ProofObl() : RefC<ProofObl_Data>() {}
        // -- create null object

    ProofObl(uint frame, uint prio, Pdr_Cla state, ProofObl next = ProofObl()) :
        RefC<ProofObl_Data>(empty_)
    {
        (*this)->frame = frame;
        (*this)->prio  = prio;
        (*this)->state = state;
        (*this)->next  = next;
    }

    ProofObl(const RefC<ProofObl_Data> p) : RefC<ProofObl_Data>(p) {}
        // -- downcast from parent to child
};


macro bool operator<(const ProofObl& x, const ProofObl& y)
{
    assert(x); assert(y);
    return x->frame < y->frame || (x->frame == y->frame && x->prio < y->prio);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'Pdr':


class Pdr {
  //________________________________________
  //  Types:

    typedef Pdr_Cla Cla;
    typedef Pdr_Cla Cub;    // -- cube; just for implicit documentation

  //________________________________________
  //  Problem statement:

    NetlistRef       N0;            // Original design
    Vec<Wire>        props;         // List of POs in 'N0' -- a set of properties to prove simultaneously.
    bool             quiet;         // Suppress output.
    bool             dump_invar;    // Output invariant on stdout at completion
    bool             minimal_cex;   // Enforce minimal counterexamples (worsens performance).

  //________________________________________
  //  Call back:

    EffortCB*        cb;            // Callback.
    Info_Pdr         info;          // Info returned through callback.

  //________________________________________
  //  State:

    Netlist          N;             // Simplified version of N0.
    Vec<Wire>        ff0;           // Flops of 'N', indexed by 'number'.
    Vec<Wire>        ff1;           // Flop input of 'N', indexed by 'number'.
    Vec<Wire>        pi0;           // PIs of 'N', indexed by 'number'.

    SatStd           S;
    Vec<Lit>         act;           // Activation literals for init ('act[0]') and image clauses ('act[1..k]')
    Lit              act_bad;       // Activation literal for Bad.

    WMap<Lit>        n2s;           // Translation from gates in 'N' to literals in 'S'
    WZet             keep;          // Signals to always keep in clausification (no variable elimination applied)
    Clausify<SatStd> C;             // Clausifier from 'N' to 'S'.

    WMap<Lit>        f2s;           // }
    WZet             keep_F;        // }- Same as above, but for frame 1 (used only for 'bad').
    Clausify<SatStd> CF;            // }

    XSimulate        xsim;          // Ternary simulation object

    Vec<Vec<Cla> >   clauses;       // 'clauses[d]' is the set of clauses proven to hold upto (and including) frame 'd'.
    Vec<Cla>         invars;        // Invariant clauses found during search.

    Vec<uint>        activity;      // Variable activity; currently = #times variable occured in a inductive clause.
    uint             wasted_lits;   // Number of activation literals that have been forever disabled.

    uint64           seed;          // Random seed used to shuffle clauses.
    uint64           seed_act;      // Random seed for initial variable activity (0 means no randomization).

  //________________________________________
  //  Temporaries:

    Vec<Lit>  push_assumps;
    Vec<Lit>  push_tmp;
    Vec<GLit> last_tmp_cla;
    Lit       last_tmp_act;

  //________________________________________
  //  Statistics:

    Vec<double>         frame_sat_time;
    PdrStats            output_stats;

  //________________________________________
  //  Helper methods:

    Lit   actLit(uint d);
    uint  depth() const { return clauses.size() - 1; }

    void  initSolver (bool assert_property);
    void  scrapSolver(bool assert_property);

    void  readNextState(Vec<GLit>& out_state);
    void  readCex(uint n_frames, Cex& out_cex);
    void  weakenBySim(Cex& cex, Vec<GLit>& out_state, Vec<GLit>* bad = NULL);
    bool  getBadPredecessor(Vec<GLit>& out_state);
    bool  isInitial(Cub s);
    bool  isInitial(const Vec<GLit>& s, uint remove_i = UINT_MAX);

    bool  solveRelative     (uint k, Vec<GLit>& g, bool& proper_invariant);
    bool  shrinkToInductive (uint k, Vec<GLit>& g, bool& proper_invariant);
    void  generalize        (uint k, Vec<GLit>& g, bool& proper_invariant);
    bool  generalizeOrRefute(uint k, Vec<GLit>& g, bool& proper_invariant);

    void  extractCex(ProofObl start, Cex& cex);

    void  outputProgress(bool newline = false);

    bool  isBlocked(Cub s, uint k);
    void  addBlockingClause(const Vec<GLit>& s, uint k, bool proper_invariant, bool external = false);
    bool  blockState(const Vec<GLit>& state, Cex* out_cex);
    bool  pushClauseForward(uint i, uint j);
    bool  pushClauses();
    void  storeInvariant(NetlistRef M);

public:
  //________________________________________
  //  Public interface:

    Pdr(NetlistRef N0_, const Vec<Wire>& props_, EffortCB* cb_, const Params_Pdr& P);
   ~Pdr();
    lbool run(Cex* cex, NetlistRef invariant, int* bf_depth);
};


//=================================================================================================
// -- Constructor:


Pdr::Pdr(NetlistRef N0_, const Vec<Wire>& props_, EffortCB* cb_, const Params_Pdr& P) :
    N0(N0_),
    props(copy_, props_),
    C (S, N, n2s, keep),
    CF(S, N, f2s, keep_F)
{
    ZZ_PTimer_Begin(INIT);

    output_stats = P.output_stats;
    minimal_cex = P.minimal_cex;
    quiet = P.quiet;
    dump_invar = P.dump_invariant;
    cb = cb_;

    C .quant_claus = true;
    CF.quant_claus = true;
    last_tmp_act = lit_Undef;
    wasted_lits = 0;
    seed = DEFAULT_SEED;
    seed_act = P.seed;

    cb = cb_;
    if (cb) cb->info = &info;

    initBmcNetlist(N0, props, N, true);
    Get_Pob(N, init_bad);
    xsim.init(N);

    WMap<uint> n_fanouts;
    countFanouts(init_bad[1], n_fanouts);
    For_Gates(N, w)
        if (n_fanouts[w] > 1)
            keep_F.add(w);

    Assure_Pob(N, fanout_count);
    For_Gates(N, w)
        if (fanout_count[w] > 1)
            keep.add(w);

    For_Gatetype(N, gate_Flop, w){
        ff0(attr_Flop(w).number) = w;
        ff1(attr_Flop(w).number) = w[0]; }
    For_Gatetype(N, gate_PI, w)
        pi0(attr_PI(w).number) = w;

    initSolver(true);

    // Reset variable activity:
    activity.growTo(ff0.size(), 0);
    ZZ_PTimer_End(INIT);
}


Pdr::~Pdr()
{
#if 0
    /*TEMPORARY*/
    WriteLn "-------------------------------------------------------------------------------";
    WriteLn "#solves: %_", S.statistics().solves;
    WriteLn "   #sat: %_", S.statistics().solves_sat;
    WriteLn "   #uns: %_", S.statistics().solves_unsat;
    NewLine;
    WriteLn "#inspects: %_", S.statistics().inspections;
    WriteLn "     #sat: %<10%_ (%.2f per call)", S.statistics().inspections_sat  , (double)S.statistics().inspections_sat   / S.statistics().solves_sat;
    WriteLn "     #uns: %<10%_ (%.2f per call)", S.statistics().inspections_unsat, (double)S.statistics().inspections_unsat / S.statistics().solves_unsat;
    NewLine;
    WriteLn "cpu-time: %t", S.statistics().time;
    WriteLn "     sat: %<10%t (%t per call)", S.statistics().time_sat  , S.statistics().time_sat   / S.statistics().solves_sat;
    WriteLn "     uns: %<10%t (%t per call)", S.statistics().time_unsat, S.statistics().time_unsat / S.statistics().solves_unsat;
    WriteLn "-------------------------------------------------------------------------------";
    /*END*/
#endif
}


void Pdr::initSolver(bool assert_property)
{
    Get_Pob(N, init_bad);

    // Set up SAT callback:
    if (cb){
        S.timeout         = VIRT_TIME_QUANTA;
        S.timeout_cb      = satEffortCB;
        S.timeout_cb_data = (void*)cb;
    }

    // Insert Property into frame 0:
    if (assert_property)
        S.addClause(~C.clausify(init_bad[1]));

    // Insert transition relation:
    act_bad = S.addLit();
    for (uind i = 0; i < ff1.size(); i++){
        if (ff1[i] == Wire_NULL) continue;

        Lit p = S.addLit();
        Lit q = C.clausify(ff1[i]);
        S.addClause(~act_bad,  p, ~q);
        S.addClause(~act_bad, ~p,  q);
        f2s(ff0[i]) = p;
    }

    // Insert guarded Bad into frame 1:
    S.addClause(CF.clausify(init_bad[1]));

    // Attach initial state to activation literal 'act[0]':
    Get_Pob(N, flop_init);
    act.push(S.addLit()); assert(act.size() == 1);
    For_Gatetype(N, gate_Flop, w){
        if (flop_init[w] != l_Undef){
            Lit p = C.clausify(w);
            S.addClause(~act[0], p ^ (flop_init[w] == l_False));
        }
    }

    // Randomize variable order:
    if (seed_act != 0)
        S.randomizeVarOrder(seed_act);
}


//=================================================================================================
// -- Recycle SAT Solver:


void Pdr::scrapSolver(bool assert_property)
{
    C.clear();
    CF.clear();
    act.clear();
    act_bad = lit_Undef;
    last_tmp_cla.clear();
    last_tmp_act = lit_Undef;
    wasted_lits = 0;

    initSolver(assert_property);

    // Insert clauses:
    Vec<Lit> ps;
    for (uind i = 0; i < clauses.size(); i++){
        Lit act_i = actLit(i);
        Vec<Cla>& cs = (i == 0) ? invars : clauses[i];

        for (uind j = 0; j < cs.size(); j++){
            Cla& c = cs[j];

            ps.clear();
            if (i > 0)
                ps.push(~act_i);
            for (uint k = 0; k < c.size(); k++){
                Wire w = ff0[c[k].id] ^ c[k].sign;
                ps.push(n2s[w] ^ sign(w));
            }
            S.addClause(ps);
        }
    }
}


//=================================================================================================
// -- Small helpers:


struct GLitActLT {
    const Vec<uint>& activity;
    GLitActLT(const Vec<uint>& activity_) : activity(activity_) {}
    bool operator()(GLit x, GLit y) const { return activity[x.id] < activity[y.id]; }
};


static
void trimCube(Vec<GLit>& g)
{
    uint j = 0;
    for (uind i = 0; i < g.size(); i++)
        if (g[i] != glit_MAX)
            g[j++] = g[i];
    g.shrinkTo(j);
}


Lit Pdr::actLit(uint d)
{
    while (d >= act.size()){
        Lit p = S.addLit();
        if (act.size() > 1)
            S.addClause(~act.last(), p);    // -- add 'act[k] -> act[k+1]' for all 'k'
        act.push(p);
    }
    return act[d];
}


// Read state from frame 1 in SAT solver:
void Pdr::readNextState(Vec<GLit>& out_state)
{
    ZZ_PTimer_Begin(Other);
    out_state.clear();
    for (uind i = 0; i < ff1.size(); i++){
        if (ff1[i] == Wire_NULL) continue;
        Lit p = n2s[ff1[i]] ^ sign(ff1[i]);
        if (+p == lit_Undef) continue;

        lbool val = S.value(p); assert(val != l_Undef);
        out_state.push(GLit((uint)i, (val == l_False)));
    }
    ZZ_PTimer_End(Other);
}


// 'n_frames' is either 1 or 2.
void Pdr::readCex(uint n_frames, Cex& out_cex)
{
    ZZ_PTimer_Begin(Other);
    out_cex.flops .setSize(n_frames);
    out_cex.inputs.setSize(n_frames);
    for (uint i = 0; i < n_frames; i++){
        out_cex.flops [i].clear();
        out_cex.inputs[i].clear();

        For_Gates(N, w){
            if (type(w) == gate_Flop || type(w) == gate_PI){
                Lit p = (i == 0) ? n2s[w] : f2s[w];
                if (p != lit_Undef){
                    lbool val = S.value(p);
                    if (val == l_Undef) continue;

                    if (type(w) == gate_Flop)
                        out_cex.flops[i](w) = val;
                    else
                        out_cex.inputs[i](w) = val;
                }
            }
        }
    }
    ZZ_PTimer_End(Other);
}


// Does cube 's' overlap with the set of initial states?
bool Pdr::isInitial(Cub s)
{
    ZZ_PTimer_Begin(Other);
    Get_Pob(N, flop_init);
    for (uind i = 0; i < s.size(); i++){
        Wire w = ff0[s[i].id] ^ s[i].sign;
        if ((flop_init[w] ^ sign(w)) == l_False){
            ZZ_PTimer_End(Other);
            return false; }
    }
    ZZ_PTimer_End(Other);
    return true;
}


bool Pdr::isInitial(const Vec<GLit>& s, uint remove_i)
{
    ZZ_PTimer_Begin(Other);
    Get_Pob(N, flop_init);
    for (uind i = 0; i < s.size(); i++){
        if (i == remove_i)    continue;
        if (s[i] == glit_MAX) continue;

        Wire w = ff0[s[i].id] ^ s[i].sign;
        if ((flop_init[w] ^ sign(w)) == l_False){
            ZZ_PTimer_End(Other);
            return false; }
    }
    ZZ_PTimer_End(Other);
    return true;
}


//=================================================================================================
// -- State weakening by ternary simulation:


//**/double experimental_time;
//**/ZZ_Initializer(experimental_time, 0) { experimental_time = 0; }
//**/ZZ_Finalizer(experimental_time, 0) { WriteLn "SAT time for Pob. shrinking: %t", experimental_time; }


// Reduce the state (flops of frame 0) stored in the counter-example to a smaller cube
// in such a way that all states represented by that cube can reach Bad in one step.
// This cube is stored in 'out_state'. If argument 'bad' is NULL, the actual bad states of the
// original design is used. If not, the cube 'bad' is considered the as target Bad states.
void Pdr::weakenBySim(Cex& cex, Vec<GLit>& out_state, Vec<GLit>* bad)
{
#if !defined(TERNARY_SIMULATION)
    out_state.clear();
    for (uint i = 0; i < ff0.size(); i++){
        lbool val = cex.flops[0][ff0[i]];
        if (val != l_Undef)
            out_state.push(GLit(i, (val == l_False)));
    }
    return;
#endif


    ZZ_PTimer_Begin(SIM);
    XSimulate& X = xsim;

    // Do simulation:
    Get_Pob(N, init_bad);
    X.simulate(cex, NULL);

    for (uint i = 0; i < ff0.size(); i++){
        if (ff0[i] == Wire_NULL) continue;

        bool failed = false;
        if (bad == NULL){
            X.propagate(XSimAssign(0, ff0[i], l_Undef), NULL, XSimAssign(1, init_bad[1], l_Undef));
            if (X[1][init_bad[1]] == l_Undef)
                failed = true;

        }else{
            X.propagate(XSimAssign(0, ff0[i], l_Undef));    // <<== extend XSimulate to handle multiple aborts
            for (uind k = 0; k < bad->size(); k++){
                if (X[0][ff1[(*bad)[k].id]] == l_Undef){
                    failed = true;
                    break;
                }
            }
        }

        if (failed) X.propagateUndo();
        else        X.propagateCommit();
    }

#if 0
    /*EXPERIMENTAL*/
    double T0 = cpuTime();

    Netlist M;
    Add_Pob0(M, strash);
    Vec<gate_id> order0;
    Vec<gate_id> order1;
    Vec<Wire>    sinks0;

    // Get sinks for frame 0 (state outputs):
    if (bad == NULL){
        Vec<Wire> sinks1(1, init_bad[1]);
        upOrder(sinks1, order1);
        for (uind i = 0; i < order1.size(); i++){
            Wire w = N[order1[i]];
            if (type(w) == gate_Flop)
                sinks0.push(ff1[attr_Flop(w).number]);
        }
    }else{
        for (uind k = 0; k < bad->size(); k++)
            sinks0.push(ff1[(*bad)[k].id]);
    }

    // Copy frame 0 into 'M' with PIs bound to constant values from the SAT model:
    upOrder(sinks0, order0);
    WMap<Wire> n2m;
    n2m(N.True()) = M.True();
    for (uind i = 0; i < order0.size(); i++){
        Wire w = N[order0[i]];
        switch (type(w)){
        case gate_PI:
            if (X[0][w] == l_Undef) n2m(w) = M.add(PI_());     // <<== why do unassigned PIs exist?
            else                    n2m(w) = M.True() ^ (X[0][w] == l_False);
            break;
        case gate_Flop:
            n2m(w) = M.add(Flop_(attr_Flop(w).number));
            break;
        case gate_And:
            n2m(w) = s_And(n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]));
            break;
        default: assert(false); }
    }

    // Copy frame 1 (if present) into 'M':
    Wire m_bad = Wire_NULL;
    if (bad == NULL){
        WMap<Wire> nn2m;
        nn2m(N.True()) = M.True();
        for (uind i = 0; i < order1.size(); i++){
            Wire w = N[order1[i]];
            switch (type(w)){
            case gate_PI:
                if (X[1][w] == l_Undef) /**/Ping, nn2m(w) = M.add(PI_());     // <<== why do unassigned PIs exist?
                else                    nn2m(w) = M.True() ^ (X[1][w] == l_False);
                break;
            case gate_PO:
                assert(m_bad == Wire_NULL);
                m_bad = nn2m[w[0]] ^ sign(w[0]);
                break;
            case gate_Flop:
                nn2m(w) = n2m[w[0]] ^ sign(w[0]);
                break;
            case gate_And:
                nn2m(w) = s_And(nn2m[w[0]] ^ sign(w[0]), nn2m[w[1]] ^ sign(w[1]));
                break;
            default: assert(false); }
        }
        assert(m_bad != Wire_NULL);
    }

    // Clausify:
    SatStd    Z;
    WMap<Lit> m2z;
    WZet      keep_M;
    Add_Pob(M, fanout_count);
    For_Gates(M, w)
        if (fanout_count[w] > 1)
            keep_M.add(w);
    Clausify<SatStd> CZ(Z, M, m2z, keep_M);

    if (bad == NULL){
        Z.addClause(CZ.clausify(~m_bad));   // -- assert 'bad' is FALSE
    }else{
        Vec<Lit> clause;
        for (uind k = 0; k < bad->size(); k++){
            Wire w = ff1[(*bad)[k].id] ^ (*bad)[k].sign;
            clause.push(CZ.clausify(~n2m[w] ^ sign(w)));
        }
        Z.addClause(clause);    // -- assert that at least one literal differ in cube
    }

    // Setup assumptions:
    Vec<Lit> assumps;
    for (uint i = 0; i < ff0.size(); i++){
        Wire w = ff0[i]; assert(!sign(w));
        if (w == Wire_NULL) continue;
        if (X[0][w] == l_Undef) continue;
        bool sgn = (X[0][w] == l_False);
        assumps.push(CZ.clausify(n2m[w] ^ sgn));
    }

    // Run solver:
    Vec<Lit> orig_assumps(copy_, assumps);
    lbool result = Z.solve(assumps); assert(result == l_False); // <<== use core

    for (uint i = 0; i < assumps.size();){
        Lit p = assumps[i];
        assumps[i] = assumps.last();
        assumps.pop();
        lbool result = Z.solve(assumps);
        if (result == l_True){
            if (i == assumps.size())
                assumps.push(p);
            else{
                assumps.push(assumps[i]);
                assumps[i] = p;
            }
            i++;
        }
        // <<== use core...
    }

    /**/if (assumps.size() != orig_assumps.size()) WriteLn "  \a/core reduction %s: %_ -> %_\a/", (bad ? "(pob)" : "(bad)"), orig_assumps.size(), assumps.size();

    // Store result:
    out_state.clear();
    for (uind i = 0; i < assumps.size(); i++){
        for (uint j = 0; j < ff0.size(); j++){
            Wire w = ff0[j];
            if (w == Wire_NULL) continue;
            if (X[0][w] == l_Undef) continue;
            bool sgn = (X[0][w] == l_False);
            if (CZ.clausify(n2m[w] ^ sgn) == assumps[i]){
                out_state.push(GLit(attr_Flop(w).number, sgn));
                goto Found;
            }
        }
        assert(false);
      Found:;
    }
    experimental_time += cpuTime() - T0;
    ZZ_PTimer_End(SIM);
    return;

    /*END EXPERIMENTAL*/
#endif

    // Store result:
    out_state.clear();
    for (uint i = 0; i < ff0.size(); i++){
        if (ff0[i] == Wire_NULL) continue;

        lbool val = X[0][ff0[i]];
        if (val != l_Undef)
            out_state.push(GLit(i, (val == l_False)));
    }
    ZZ_PTimer_End(SIM);
}


//=================================================================================================
// -- Get predecessor of bad:


bool Pdr::getBadPredecessor(Vec<GLit>& out_state)
{
    Vec<Lit> assumps;
    assumps.push(actLit(depth()));
    assumps.push(act_bad);
    ZZ_PTimer_Begin(SAT);
    lbool result = S.solve(assumps);
    ZZ_PTimer_End(SAT);
    if (result == l_Undef) throw Excp_Pdr_Abort();

    if (result == l_True){
        Cex cex;
        readCex(2, cex);
        weakenBySim(cex, out_state);
        return true;
    }else
        return false;
}


//=================================================================================================
// -- Inductive generalizaion:


/*tmp*/Vec<Vec<uint64> > sim_vecs;

// <<== returnera vilka literaler av
static bool isReachable(const Vec<GLit>& s) ___unused;
static bool isReachable(const Vec<GLit>& s)
{
    for (uind i = 0; i < sim_vecs.size(); i++){
        Vec<uint64>& sim = sim_vecs[i];
        uint64 mask = 0;
        for (uind k = 0; k < s.size(); k++){
            if (s[k] == glit_MAX) continue;
            mask |= sim[s[k].id] ^ (s[k].sign ? 0 : (uint64)0xFFFFFFFFFFFFFFFFull);
        }
        if (mask != 0xFFFFFFFFFFFFFFFFull){
            return true; }
    }
    return false;
}


// Return the result of SAT problem: F[k] & T & ~g & g'
// If 'k == UINT_MAX', then the 'F[k]' part is dropped.
//
// If SAT:
//    - Returns TRUE and leaves 'proper_invariant' untouched.
//
// If UNSAT:
//    - Returns FALSE and sets 'proper_invariant' to TRUE if result did not depend on 'F[k]',
//      or FALSE otherwise.
//    - Shrinks 'g' by removing literals unnecessary for the proof of UNSAT (the shrinking
//      is achieved by setting literals to 'glit_MAX' rather than resizing the vector).
//
bool Pdr::solveRelative(uint k, Vec<GLit>& g, bool& proper_invariant)
{
    double T0 = (output_stats == pdr_Time) ? cpuTime() : 0;

    // Check if old clause is subsumed by 'g' (=> can recycle its activation literal):
    bool subsumes = false;
    if (last_tmp_cla.size() > 0){
        subsumes = true;
        for (uind i = 0; i < g.size(); i++){
            if (g[i] == glit_MAX) continue;
            if (!has(last_tmp_cla, g[i])){
                subsumes = false;
                break; }
        }
    }
    g.copyTo(last_tmp_cla);
    trimCube(last_tmp_cla);

#if defined(RELATIVE_INDUCTION)
    // Allocate new temporary activation literal:
    if (!subsumes){
        if (last_tmp_act != lit_Undef)
            S.addClause(~last_tmp_act);
        last_tmp_act = S.addLit();
        wasted_lits++;
    }

    // Add clause '~g' to SAT solver (guarded by temporary activation literal):
    Vec<Lit> tmp;
    for (uint i = 0; i < g.size(); i++)
        if (g[i] != glit_MAX){
            Wire w = ~ff0[g[i].id] ^ g[i].sign; assert(n2s[w] != lit_Undef);
            tmp.push(n2s[w] ^ sign(w)); }

    tmp.push(~last_tmp_act);
    S.addClause(tmp);
#endif

    // Setup assumptions:
    Vec<Lit> assumps;
#if defined(RELATIVE_INDUCTION)
    assumps.push(last_tmp_act);
#endif
    if (k != UINT_MAX)
        assumps.push(act[k-1]);

    for (uint i = 0; i < g.size(); i++)
        if (g[i] != glit_MAX){
            Wire w = ff1[g[i].id] ^ g[i].sign; assert(n2s[w] != lit_Undef);
            assumps.push(n2s[w] ^ sign(w)); }

#if defined(FILTER_COI_IN_SAT)
    // EXPERIMENTAL -- filter out variables outside cone of influence
    ZZ_PTimer_Begin(COI);
    IntZet<Var>  coi_vars;
    Vec<gate_id> coi_gates;

    // Add transitive fanin of cube 'g':
    Vec<Wire>    sinks;
    for (uint i = 0; i < g.size(); i++)
        if (g[i] != glit_MAX)
            sinks.push(+ff1[g[i].id]);
    upOrder(sinks, coi_gates);
    for (uind i = 0; i < coi_gates.size(); i++){
        Lit p = n2s[N[coi_gates[i]]];
        if (p != lit_Undef)
            coi_vars.add(var(p));
    }

    For_Gatetype(N, gate_Flop, w){
        assert(n2s[w] != lit_Undef);
        coi_vars.add(var(n2s[w])); }
    for (uind i = 0; i < assumps.size(); i++)
        coi_vars.add(var(assumps[i]));

    //**/WriteLn "Activating %_/%_ vars", coi_vars.size(), S.varCount();
    S.setFilter(coi_vars);
    ZZ_PTimer_End(COI);
#endif

    // Call SAT:
    ZZ_PTimer_Begin(SAT);
    lbool result = S.solve(assumps);
    ZZ_PTimer_End(SAT);
#if defined(FILTER_COI_IN_SAT)
    S.clearFilter();
#endif
    if (result == l_Undef) throw Excp_Pdr_Abort();

    if (result == l_False){     // -- UNSAT
        // Check if 'F[k]' was used (else proper invariant):
        proper_invariant = true;
        if (k != UINT_MAX){
            for (uind i = 0; i < S.conflict.size(); i++)
                if (S.conflict[i] == act[k-1]){
                    proper_invariant = false;
                    break; }
        }

        // Shrink 'g' by removing literals corresponding to unused assumptions:
        {
            // Count "non-initialness":
            Get_Pob(N, flop_init);
            uint counter = 0;
            for (uind i = 0; i < g.size(); i++){
                if (g[i] == glit_MAX) continue;
                Wire w = ff0[g[i].id] ^ g[i].sign;
                if ((flop_init[w] ^ sign(w)) == l_False)
                    counter++;
            }

            // Remove literals from 'g':
//            for (uind i = 0; i < g.size(); i++){
            for (uind i = g.size(); i > 0;){ i--;
                if (g[i] == glit_MAX) continue;
                Wire w0 = ff0[g[i].id] ^ g[i].sign;
                bool init_lit = (flop_init[w0] ^ sign(w0)) == l_False;
                if (init_lit && counter == 1) continue;

                Wire w = ff1[g[i].id] ^ g[i].sign;  assert(n2s[w] != lit_Undef);
                if (!has(S.conflict, n2s[w] ^ sign(w))){
                    g[i] = glit_MAX;
                    if (init_lit) counter--;
                }
            }
        }

    }else
        assert(result == l_True);

    if (output_stats == pdr_Time)
        frame_sat_time(k, 0) += cpuTime() - T0;

    return result == l_True;
}


bool Pdr::shrinkToInductive(uint k, Vec<GLit>& g, bool& proper_invariant)
{
    Vec<GLit> g_copy(copy_, g);
    for(;;){
        if (solveRelative(k, g, proper_invariant)){
            // Make 'g' satisfied by current model of frame 0:
            for (uint i = 0; i < g.size(); i++){
                if (g[i] == glit_MAX) continue;

                Wire  w = ff0[g[i].id] ^ g[i].sign;
                Lit   p = n2s[w] ^ sign(w);
                if (S.value(p) == l_False)
                    g[i] = glit_MAX;
            }

            if (isInitial(g)){
                g_copy.copyTo(g);
                return false;
            }

        }else{
            trimCube(g);
            return true;
        }
    }
}


/*tmp*/KeyHeap<ProofObl>* heap = NULL;

void Pdr::generalize(uint k, Vec<GLit>& g, bool& proper_invariant)
{
#if 0    // Fixed-point
    uint last_elimed = 0;        // -- keep going until we have a full orbit of failures to remove a literal
    for (uint i = 0; (i+1) % g.size() != last_elimed; i = (i+1) % g.size()){
        if (g[i] == glit_MAX) continue;
        if (isInitial(g, i)) continue;

        GLit gone = g[i];
        g[i] = glit_MAX;
        if (solveRelative(k, g, proper_invariant))
            g[i] = gone;
        else
            last_elimed = i;
    }
#endif

#if 0   // One round
    for (uind i = 0; i < g.size(); i++){
        if (g[i] == glit_MAX) continue;
        if (isInitial(g, i)) continue;

        GLit gone = g[i];
        g[i] = glit_MAX;
        if (solveRelative(k, g, proper_invariant))
            g[i] = gone;
    }
#endif

#if 0   // EXPERIMENTAL! Order 'g' so that literals with a high probability of covering other bad cubes (if removed) is tried first
    if (heap != NULL){
        Vec<float> scores;
        Vec<uint>  bumps;
        const Vec<ProofObl>& pobls = heap->base();
        for (uind i = 0; i < pobls.size(); i++){
            if (pobls[i]->frame == k){
                // If a cube disagrees with 'g' on 'n' literals, give each of these literals a score of '2^-n'.
                const Cub& z= pobls[i]->state;
                for (uind i = 0; i < g.size(); i++){
                    if (has(z, ~g[i])){
                        scores.setSize(g.size(), 0.0);
                        bumps.push(i);
                    }
                }
                float amount = pow(2.0, -(double)bumps.size());
                for (uind i = 0; i < bumps.size(); i++)
                    scores[bumps[i]] -= amount;
            }
        }
        if (scores.size() > 0){
            Vec<uint> place(scores.size());
            for (uint i = 0; i < place.size(); i++) place[i] = i;
            sobSort(ordByFirst(ordLexico(sob(scores), sob(place)), sob(g)));
            //**/Dump(scores);
        }
    }
#endif

#if 1   // Two rounds [DEFAULT]
    uint last_elimed = 0;        // -- keep going until we have a full orbit of failures to remove a literal
    for (uint i = 0, j = 0; (i+1) % g.size() != last_elimed && j < 2*g.size(); i = (i+1) % g.size(), j++){
        if (g[i] == glit_MAX) continue;
        if (isInitial(g, i)) continue;

        GLit gone = g[i];
        g[i] = glit_MAX;
        if (solveRelative(k, g, proper_invariant))
            g[i] = gone;
        else{
            //*trc*/WriteLn "  -- %_", g;
            last_elimed = i; }
    }
#endif

#if 0   // Partial round
    for (uind i = 0; i < g.size() && i < 3; i++){
        if (g[i] == glit_MAX) continue;
        if (isInitial(g, i)) continue;

        GLit gone = g[i];
        g[i] = glit_MAX;
        if (solveRelative(k, g, proper_invariant))
            g[i] = gone;
    }
#endif

    trimCube(g);


#if defined(RECURSE_INTO_NONINDUCTIVE)
    // Step into non-inductive realm and see if smaller inductive clause exists:
    Vec<uint> h;
    for (uint i = 0; i < g.size(); i++)
        if (g[i] != glit_MAX) h.push(i);
    shuffle(seed, h);
    h.shrinkTo(3);

    for (uint j = 0; j < h.size(); j++){
        uint i = h[j];
        if (isInitial(g, i)) continue;

        GLit gone = g[i];
        g[i] = glit_MAX;
        if (shrinkToInductive(k, g, proper_invariant)){
            generalize(k, g, proper_invariant);
            return;
        }
        g[i] = gone;
    }
#endif
}



// If '(F[k-1] & ~g & T & g')' is UNSAT, generalize 'g' by removing literals and return TRUE.
// If SAT, store predecessor in 'g' and return FALSE.
bool Pdr::generalizeOrRefute(uint k, Vec<GLit>& g, bool& proper_invariant)
{
    //*trc*/WriteLn "Trying %_: %_", k, g;
    if (solveRelative(k, g, proper_invariant)){
        // Found "bad" predecessor:
        Cex cex;
        readCex(1, cex);
        weakenBySim(cex, g, &g);
        //*trc*/WriteLn "Failed. New p.obl: %_", g;
        return false;

    }else{
        //*trc*/WriteLn "  -- %_", g;
        generalize(k, g, proper_invariant);
        return true;
    }
}


//=================================================================================================
// -- Counter-example extraction:


void Pdr::extractCex(ProofObl start, Cex& cex)
{
    scrapSolver(false);

    // Produce counter-example:
    Vec<Vec<lbool> > cex_pi;
    Vec<Vec<lbool> > cex_ff;

    Cub s0 = start->state;
    bool first_iter = true;
    for(;;){
        // Setup assumptions:
        Vec<Lit> assumps;
        if (first_iter){
            assumps.push(act[0]);
            first_iter = false; }

        for (uint phase = 0; phase < 2; phase++){
            if (phase == 1 && !start->next){
                assumps.push(act_bad);
                break; }

            Vec<Wire>& ff = (phase == 0) ? ff0 : ff1;
            Cub s = (phase == 0) ? s0 : start->next->state;

            uint i = 0;
            for (uint num = 0; num < ff.size(); num++){
                if (ff[num] == Wire_NULL) continue;

                if (i < s.size() && s[i].id == num){
                    if (Wire w = ff[num] ^ s[i].sign){
                        if (n2s[w] != lit_Undef)
                            assumps.push(n2s[w] ^ sign(w));
                    }
                    i++;
                }
            }
        }

        // Run SAT-solver to find inputs for one transition:
        lbool result = S.solve(assumps); assert(result == l_True);

        // Store model in counter-example:
        cex_pi.push();
        for (uind i = 0; i < pi0.size(); i++){
            if (Wire w = pi0[i]){
                Lit p = n2s[w] ^ sign(w);
                if (+p == lit_Undef || S.value(p) == l_Undef)
                    cex_pi.last()(i, l_Undef) = l_False;
                else
                    cex_pi.last()(i, l_Undef) = S.value(p);
            }
        }

        cex_ff.push();
        for (uind i = 0; i < ff0.size(); i++){
            if (Wire w = ff0[i]){
                Lit p = n2s[w] ^ sign(w);
                if (+p == lit_Undef || S.value(p) == l_Undef)
                    cex_ff.last()(i, l_Undef) = l_Undef;
                else
                    cex_ff.last()(i, l_Undef) = S.value(p);
            }
        }

        // Move to next frame:
        Vec<GLit> next_s0;
        readNextState(next_s0);
        s0 = Cub(next_s0);

        start = start->next;
        if (!start) break;
    }

    // Last frame of counter-example is a bit tedious:
    Netlist M;
    Wire init = M.True();
    for (uind i = 0; i < s0.size(); i++){
        Wire w = ff0[s0[i].id];
        Wire v = M.add(Flop_(attr_Flop(w).number)) ^ s0[i].sign;
        init = M.add(And_(), init, v);
    }

    Get_Pob(N, init_bad);
    Wire bad = init_bad[1][0];

    Cex cex2;
    lbool result = checkDistinct(init, bad, N, &cex2, /*cb*/NULL); assert(result == l_False);

    cex_pi.push();
    for (uind i = 0; i < pi0.size(); i++)
        if (Wire w = pi0[i])
            cex_pi.last()(i, l_Undef) = cex2.inputs[0][w];

    cex_ff.push();
    for (uind i = 0; i < ff0.size(); i++)
        if (Wire w = ff0[i])
            cex_ff.last()(i, l_Undef) = cex2.flops[0][w];

    // Translate counter-example:
    translateCex(cex_pi, cex_ff, N0, cex);
}


//=================================================================================================
// -- Progress output:


void Pdr::outputProgress(bool newline)
{
    if (quiet) return;

    if (output_stats == pdr_Clauses){
        Write "\r\a*%_\a*:", clauses.size() - 1;
        for (uind k = 1; k < clauses.size(); k++)
            Write " %_", clauses[k].size();
        Write " (%_)\f", invars.size();

    }else if (output_stats == pdr_Time){
        double total_time = 0;
        for (uind i = 0; i < frame_sat_time.size(); i++)
            total_time += frame_sat_time[i];
        Write "\r%%time:";
        if (total_time > 0){
            for (uind k = 1; k < clauses.size(); k++)
                Write " %.0f", frame_sat_time(k, 0) / total_time * 100;
        }
        Write " \a/(%t)\a/\f", total_time;

    }else if (output_stats == pdr_Vars){
        Write "\r#vars:";
        IntZet<uint> vars;
        for (uint d = 1; d < clauses.size(); d++){
            vars.clear();
            for (uind i = 0; i < clauses[d].size(); i++){
                const Cla& c = clauses[d][i];
                for (uint j = 0; j < c.size(); j++)
                    vars.add(c[j].id);
            }
            Write " %_", vars.size();
        }
        Write " \a/(%_)\a/\f", N.typeCount(gate_Flop);
    }

    if (newline) NewLine;
}


//=================================================================================================
// -- Recursively block one state:


// See if 's' is already blocked.
bool Pdr::isBlocked(Cub s, uint k)              // -- about 5% of runtime
{
    ZZ_PTimer_Begin(Subsume);
    s.invert();
    for (uint d = k; d < clauses.size(); d++){
        for (uind i = 0; i < clauses[d].size(); i++){
            if (subsumes(clauses[d][i], s)){
                s.invert();
                return true;
            }
        }
    }
    s.invert();
    ZZ_PTimer_End(Subsume);

    Vec<Lit> assumps;
    assumps.push(actLit(k));
    for (uint i = 0; i < s.size(); i++){
        Wire w = ff0[s[i].id] ^ s[i].sign;  assert(n2s[w] != lit_Undef);
        assumps.push(n2s[w] ^ sign(w)); }
    ZZ_PTimer_Begin(SAT);
    lbool result = S.solve(assumps);
    ZZ_PTimer_End(SAT);
    if (result == l_Undef) throw Excp_Pdr_Abort();

    return result == l_False;
}


// Adds the blocking clause '~s' both in the SAT-solver and in 'clauses[]'. NOTE! Argument 's' is a
// cube! Subsumed clauses are deleted. Clauses are shifted around (which may be problematic if you
// are iterating over them). If 'proper_invariant' is TRUE, 'k' is ignored.
void Pdr::addBlockingClause(const Vec<GLit>& s, uint k, bool proper_invariant, bool external)
{
#if 0
    if (par && !external){
        //**/WriteLn "~~~~>> sending @%_ %_", k, s;
        sendMsg_UnreachCube(s, k); }
#endif

    Cla gc(s);
    //**/Dump(gc, k, proper_invariant);
    gc.invert();

    // Remove subsumed clauses:
    ZZ_PTimer_Begin(Subsume);
    uint d_lim = proper_invariant ? clauses.size()-1 : k;
    for (uint d = 1; d <= d_lim; d++){
        for (uind i = 0; i < clauses[d].size();){
            if (!clauses[d][i].null() && subsumes(gc, clauses[d][i])){
                clauses[d][i] = clauses[d].last();
                clauses[d].pop();
            }else
                i++;
        }
    }
    ZZ_PTimer_End(Subsume);

    // Store clause:
    if (proper_invariant)
        invars.push(gc);
    else
        clauses[k].push(gc);

    Vec<Lit> ps;
    if (!proper_invariant)
        ps.push(~actLit(k));
    for (uind i = 0; i < gc.size(); i++){
        Wire w = ff0[gc[i].id] ^ gc[i].sign;
        ps.push(n2s[w] ^ sign(w));
    }
    S.addClause(ps);
    //**/Vec<GLit> ss(copy_, s); sort(ss);
    //**/WriteLn "\b0\rCube at %_: %_", k, ss;

    // Update activity:
    for (uind i = 0; i < gc.size(); i++)
        if (activity[gc[i].id] != UINT_MAX)
            activity[gc[i].id]++;
}


// Add clauses to block 'state' and return TRUE, or if counter-example found (with final transition
// from 'state' to 'bad_state'), populate 'cex' (if non-null) and return FALSE
bool Pdr::blockState(const Vec<GLit>& state, Cex* cex)
{
    KeyHeap<ProofObl> Q;
    uint              prioC = UINT_MAX;
    Vec<GLit>         g;
    /*tmp*/heap = &Q;

    Q.add(ProofObl(depth(), prioC--, Cub(state)));

    outputProgress();
    uint output_cc = UINT_MAX;
    while (Q.size() > 0){
        // Incorporate other unreachable cubes sent through the PAR interface:
        if (par){
            Msg msg;
            while (msg = pollMsg()){
                if (msg.type == 104/*UCube*/){
                    uint      frame;
                    Vec<GLit> state;
                    unpack_UCube(msg.pkg, frame, state);
#if 0
                    if (frame <= depth() || frame == UINT_MAX){
                        //**/WriteLn "~~~~ added @%_ %_", frame, state;
                        if (frame == UINT_MAX)
                            addBlockingClause(state, 0, true, /*external*/true);
                        else
                            addBlockingClause(state, frame, false, /*external*/true);
                    }else{
                        addBlockingClause(state, depth(), false, /*external*/true);
                        //**/WriteLn "~~~~ delayed @%_ %_", frame, state;
                    }
#else
                    bool proper_invariant = false;
                    uint k = 0;
                    while (k + 1 < clauses.size()){
                        if (solveRelative(k + 1, state, proper_invariant)) break;
                        k++;
                        if (proper_invariant) break;
                    }
                    trimCube(state);

                    // Add clause:
                    addBlockingClause(state, k, proper_invariant, /*external*/true);

                    if (proper_invariant) k = UINT_MAX;
                    //**/WriteLn "~~~~ cube from frame %_ added at frame %_", frame, k;
#endif
                }
            }
        }

        // Pop proof-obligation:
        ProofObl curr_po = Q.pop();
        uint     k       = curr_po->frame;
        Cub      s       = curr_po->state;
        ProofObl next_po = curr_po->next;

        if (k == 0){
            // Found counter-example:
            if (cex) extractCex(curr_po, *cex);
            outputProgress(true);
            /*tmp*/heap = NULL;
            return false;
        }

        assert(k > 0);
        assert(!isInitial(s));

        if (!isBlocked(s, k)){
            // Convert cube 's' to vector 'g' for easier manipulation (+ shuffle and sort):
            g.setSize(s.size());
            for (uind i = 0; i < s.size(); i++) g[i] = s[i];

#if 0 //DDD
            shuffle(seed, g);
            sobSort(sob(g, GLitActLT(activity)));
#endif

            // Do inductive generalization:
            bool proper_invariant = false;
            if (!generalizeOrRefute(k, g, proper_invariant)){
                // Cube 'g' is NOT blocked by 'img(F[k-1])'. Enqueue the predecessor as  a new
                // proof-obligation, and re-enqueue this P.O. as well:
                Q.add(ProofObl(k-1, prioC--, Cub(g), curr_po));
                Q.add(ProofObl(k,   prioC--, s     , next_po));

            }else{
                // See if generalized clause holds for larger 'k's:
                if (!proper_invariant){
                    while (k + 1 < clauses.size()){
                        if (solveRelative(k + 1, g, proper_invariant)) break;
                        k++;
                        if (proper_invariant) break;
                    }
                    trimCube(g);
                }

                // Add clause:
                addBlockingClause(g, k, proper_invariant);

                // Enqueue this state as a P.O. for the next time-frame as well:
                if (k != depth() && !proper_invariant && !minimal_cex)
                    Q.add(ProofObl(k+1, prioC--, s, next_po));
            }
        }

        // Time to reset the SAT solver?
#if 1 //DDD
        if (wasted_lits * 2 > S.nVars())
            scrapSolver(true);
#endif

        if (!quiet){
            if (output_cc >= clauses.size()){
                outputProgress();
                output_cc = 0;
            }else
                output_cc++;
        }
    }

    /*tmp*/heap = NULL;
    return true;
}


//=================================================================================================
// -- Push Clauses Forward (any maybe terminate):


// Try to push 'clauses[i][j]' forward to 'clauses[i+1]'. If successful, the last clause of
// 'clauses[i]' will be moved to the 'j'th position.
bool Pdr::pushClauseForward(uint i, uint j)
{
    assert(i + 1 < clauses.size());
    Cla& c = clauses[i][j];

    push_assumps.clear();
    push_assumps.push(actLit(i));
    for (uind k = 0; k < c.size(); k++){
        Wire w = ff1[c[k].id] ^ c[k].sign; assert(n2s[w] != lit_Undef);
        push_assumps.push(~n2s[w] ^ sign(w));
    }

    ZZ_PTimer_Begin(SAT);
    lbool result = S.solve(push_assumps);
    ZZ_PTimer_End(SAT);
    if (result == l_Undef) throw Excp_Pdr_Abort();

    if (result == l_False){
        // Clause 'c' holds in the next frame as well:
        push_tmp.clear();
        for (uint k = 0; k < c.size(); k++){
            Wire w = ff0[c[k].id] ^ c[k].sign; assert(n2s[w] != lit_Undef);
            push_tmp.push(n2s[w] ^ sign(w));
        }
        push_tmp.push(~actLit(i+1));
        S.addClause(push_tmp);

        // Subsumption check:
        ZZ_PTimer_Begin(Subsume);
        for (uind n = 0; n < clauses[i+1].size();){
            if (subsumes(c, clauses[i+1][n])){
                clauses[i+1][n] = clauses[i+1].last();
                clauses[i+1].pop();
            }else
                n++;
        }
        ZZ_PTimer_End(Subsume);

        clauses[i+1].push(c);
        clauses[i][j] = clauses[i].last();
        clauses[i].pop();

        return true;

    }else
        return false;
}


bool Pdr::pushClauses()
{
    if (output_stats == pdr_Time)
        frame_sat_time.clear();
    // <<== can we avoid redoing some work if context has not changed for a certain clause?

#if 0
    for (uind i = 1; i < clauses.size() - 1; i++){
#else // EXPERIMENTAL
    uind start = (clauses.size() == 2) ? 0 : 1;
    for (uind i = start; i < clauses.size() - 1; i++){
#endif
        for (uind j = 0; j < clauses[i].size();){
            if (!pushClauseForward(i, j))
                j++;
        }
        outputProgress();

        if (i > 0 && clauses[i].size() == 0){
            if (!quiet) NewLine;

            // Dump invariant:
            if (!quiet){
                uint invar_sz = 0;
                for (uind j = i+1; j < clauses.size(); j++)
                    invar_sz += clauses[j].size();
                invar_sz += invars.size();
                if (invar_sz <= 15 || dump_invar){
                    WriteLn "Invariant F[%_]:", i;
                    for (uind j = i+1; j < clauses.size(); j++){
                        for (uind k = 0; k < clauses[j].size(); k++)
                            WriteLn "  %_", clauses[j][k];
                    for (uind k = 0; k < invars.size(); k++)
                        WriteLn "  %_", invars[k];
                    }
                }else
                    WriteLn "Invariant F[%_]: %_ clauses", i, invar_sz;
            }
            return true;
        }
    }

#if defined(FIND_INVAR_SUBSET)
    /*EXPERIMENTAL -- find invariant subset of clauses in F*/
    Vec<Cla> cands;
    for (uind i = 0; i < clauses.size(); i++)
        for (uind j = 0; j < clauses[i].size(); j++)
            cands.push(clauses[i][j]);
    /**/NewLine;
    /**/Write "\a*Cands:\a*\a/";
    Vec<Lit> tmp;
    for(;;){
        /**/Write " %_\f", cands.size();
        uint n_cands = cands.size();

        SatStd    Z;
        WMap<Lit> n2z;
        Clausify<SatStd> CZ(Z, N, n2z, keep);

        // Insert Property into frame 0:
        Get_Pob(N, init_bad);
        Z.addClause(~CZ.clausify(init_bad[1]));

        // Insert transition relation:
        for (uind i = 0; i < ff1.size(); i++){
            if (ff1[i] == Wire_NULL) continue;
            CZ.clausify(ff1[i]); }

        // Assume 'cands' in frame 0:
        for (uind i = 0; i < cands.size(); i++){
            const Cla& c = cands[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff0[c[j].id] ^ c[j].sign;
                tmp.push(n2z[w] ^ sign(w)); }
            Z.addClause(tmp);
        }

        // Assume 'invars' in frame 0:
        for (uind i = 0; i < invars.size(); i++){
            const Cla& c = invars[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff0[c[j].id] ^ c[j].sign;
                tmp.push(n2z[w] ^ sign(w)); }
            Z.addClause(tmp);
        }

        // See which clauses of 'cands' are implied in frame 1:
        for (uind i = 0; i < cands.size();){
            const Cla& c = cands[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff1[c[j].id] ^ c[j].sign;
                tmp.push(~n2z[w] ^ sign(w)); }
            lbool result = Z.solve(tmp);

            if (result == l_True){
                //clauses.last().push(cands[i]);   -- put it back where it came from?
                cands[i] = cands.last();
                cands.pop();
            }else{
                assert(result == l_False);
                i++;
            }
        }

        if (cands.size() == n_cands){
            // Is the property implied?
            WMap<Lit> f2z;
            Clausify<SatStd> CFZ(Z, N, f2z, keep_F);
            for (uind i = 0; i < ff1.size(); i++)
                if (ff1[i] != Wire_NULL)
                    f2z(ff0[i]) = CZ.clausify(ff1[i]);
            Z.addClause(CFZ.clausify(init_bad[1]));

            lbool result = Z.solve();
            if (result == l_False){
                Write "\a0 \a*PROVED!\a0"; }
            break;
        }
    }
    /**/Write "\a0";
    /**/NewLine;

    // Add invariant clauses:
    Vec<GLit> g;
    for (uind i = 0; i < cands.size(); i++){
        const Cla& c = cands[i];
        g.setSize(c.size());
        for (uind i = 0; i < c.size(); i++) g[i] = ~c[i];
        addBlockingClause(g, 0, true);
    }
    /*END*/
#endif


#if 0
    /*EXPERIMENTAL -- find invariant subset of last F*/
    Vec<Cla> cands;
    clauses.last().moveTo(cands);

    /**/Write "\n\a/Cands:\a/\f";
    Vec<Lit> tmp;
    for(;;){
        /**/Write " %_\f", cands.size();
        uint n_cands = cands.size();

        SatStd    Z;
        WMap<Lit> n2z;
        Clausify<SatStd> CZ(Z, N, n2z, keep);

        // Insert Property into frame 0:
        Get_Pob(N, init_bad);
        Z.addClause(~CZ.clausify(init_bad[1]));

        // Insert transition relation:
        for (uind i = 0; i < ff1.size(); i++){
            if (ff1[i] == Wire_NULL) continue;
            CZ.clausify(ff1[i]); }

        // Assume 'cands' in frame 0:
        for (uind i = 0; i < cands.size(); i++){
            const Cla& c = cands[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff0[c[j].id] ^ c[j].sign;
                tmp.push(n2z[w] ^ sign(w)); }
            Z.addClause(tmp);
        }

        // Assume 'invars' in frame 0:
        for (uind i = 0; i < invars.size(); i++){
            const Cla& c = invars[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff0[c[j].id] ^ c[j].sign;
                tmp.push(n2z[w] ^ sign(w)); }
            Z.addClause(tmp);
        }

        // See which clauses of 'cands' are implied in frame 1:
        for (uind i = 0; i < cands.size();){
            const Cla& c = cands[i];
            tmp.clear();
            for (uind j = 0; j < c.size(); j++){
                Wire w = ff1[c[j].id] ^ c[j].sign;
                tmp.push(~n2z[w] ^ sign(w)); }
            lbool result = Z.solve(tmp);

            if (result == l_True){
                clauses.last().push(cands[i]);
                cands[i] = cands.last();
                cands.pop();
            }else{
                assert(result == l_False);
                i++;
            }
        }

        if (cands.size() == n_cands) break;
    }
    /**/NewLine;

    // Add invariant clauses:
    for (uind i = 0; i < cands.size(); i++){
        const Cla& c = cands[i];
        tmp.clear();
        for (uind j = 0; j < c.size(); j++){
            Wire w = ff0[c[j].id] ^ c[j].sign;
            tmp.push(n2s[w] ^ sign(w)); }
        S.addClause(tmp);
        invars.push(c);
    }

    /*END*/
#endif

    return false;
}


void Pdr::storeInvariant(NetlistRef M)
{
    M.clear();
    Add_Pob0(M, strash);

    clauses.push();         // -- temporary put invariant clauses last in 'clauses[]' to ensure they are part of the returned invariant
    invars.moveTo(clauses.last());

    Vec<Wire> flops;
    Wire w_conj = M.True();
    bool adding = false;
    for (uind i = 1; i < clauses.size(); i++){
        if (!adding){
            if (clauses[i].size() == 0)
                adding = true;

        }else{
            for (uind j = 0; j < clauses[i].size(); j++){
                Cla& c = clauses[i][j];

                Wire w_disj = ~M.True();
                for (uind k = 0; k < c.size(); k++){
                    uint num = c[k].id;
                    if (flops(num, Wire_NULL) == Wire_NULL)
                        flops[num] = M.add(Flop_(num));
                    w_disj = s_Or(w_disj, flops[num] ^ c[k].sign);
                }
                w_conj = s_And(w_conj, w_disj);
            }
        }
    }
    assert(adding);

    Get_Pob(N, init_bad);
    Wire w_prop = copyFormula(~init_bad[1][0], M);
    w_conj = s_And(w_conj, w_prop);

    M.add(PO_(), w_conj);
    removeUnreach(M);

    clauses.last().moveTo(invars);  // -- restore 'clauses[]'
    clauses.pop();
}


//=================================================================================================
// -- Generate simulation data:


static void genSimData(NetlistRef N, uint n_words, uint depth, Vec<Vec<uint64> >& sim_vecs) ___unused;
static void genSimData(NetlistRef N, uint n_words, uint depth, Vec<Vec<uint64> >& sim_vecs)
{
    Get_Pob(N, flop_init);

    WMap<uint64> sim;
    sim(N.True()) = 0xFFFFFFFFFFFFFFFFull;

    Vec<gate_id> order;
    upOrder(N, order);
    Vec<gate_id> flops;
    uind j = 0;
    for (uind i = 0; i < order.size(); i++){
        if (type(N[order[i]]) == gate_Flop)
            flops.push(order[i]);
        else
            order[j++] = order[i];
    }
    order.shrinkTo(j);

    WMapL<uint64> nexts;
    uint64 seed = 0;
    for (uint n = 0; n < n_words; n++){
        for (uint d = 0; d < depth; d++){
            // Initialize/update flops:
            if (d == 0){
                for (uind i = 0; i < flops.size(); i++){
                    Wire w = N[flops[i]];
                    if (flop_init[w] == l_Undef)
                        sim(w) = irandl(seed);
                    else if (flop_init[w] == l_True)
                        sim(w) = 0xFFFFFFFFFFFFFFFFull;
                    else assert(flop_init[w] == l_False),
                        sim(w) = 0x0000000000000000ull;
                }
            }else{
                for (uind i = 0; i < flops.size(); i++){
                    Wire w = N[flops[i]];
                    sim(w) = nexts(w);
                }
            }

            // Simulate logic:
            for (uind i = 0; i < order.size(); i++){
                Wire w = N[order[i]];
                switch (type(w)){
                case gate_Flop:
                    break;      // -- nothing
                case gate_PI:
                    sim(w) = irandl(seed);
                    break;
                case gate_And:
                    sim(w) = (sim[w[0]] ^ (sign(w[0]) ? 0xFFFFFFFFFFFFFFFFull : 0))
                           & (sim[w[1]] ^ (sign(w[1]) ? 0xFFFFFFFFFFFFFFFFull : 0));
                    break;
                case gate_PO:
                    sim(w) = sim[w[0]] ^ (sign(w[0]) ? 0xFFFFFFFFFFFFFFFFull : 0);
                    break;
                default: assert(false); }
            }

            // Store value of state outputs:
            sim_vecs.push();
            For_Gatetype(N, gate_Flop, w){
                nexts(w) = sim[w[0]] ^ (sign(w[0]) ? 0xFFFFFFFFFFFFFFFFull : 0);
                sim_vecs.last()(attr_Flop(w).number) = nexts[w];
            }
        }
    }
}


//=================================================================================================
// -- Run method:


lbool Pdr::run(Cex* cex, NetlistRef invariant, int* bf_depth)
{
    if (bf_depth) *bf_depth = -1;

    if (!quiet) writeHeader("Property-driven Reachability", 79);

    // Check that init and bad don't overlap:
    {
        if (!quiet) Write "Checking for trivial counter-example.\f";

        // Convert 'flop_init' to single-output constraint:
        Get_Pob(N, flop_init);
        Netlist M;
#if 0
        Wire init = M.True();
        For_Gatetype(N, gate_Flop, w){
            if (flop_init[w] != l_Undef){
                Wire v = M.add(Flop_(attr_Flop(w).number));
                init = M.add(And_(), init, v ^ (flop_init[w] == l_False));
            }
        }
#else
        Vec<Wire> init_vec;
        For_Gatetype(N, gate_Flop, w){
            if (flop_init[w] != l_Undef){
                Wire v = M.add(Flop_(attr_Flop(w).number));
                init_vec.push(v ^ (flop_init[w] == l_False));
            }
        }

        Wire init = M.True();
        if (init_vec.size() > 0){
            for (uint i = 0; i < init_vec.size()-1; i += 2)
                init_vec.push(M.add(And_(), init_vec[i], init_vec[i+1]));
            init = init_vec.last();
        }
#endif

        Get_Pob(N, init_bad);
        Wire bad = init_bad[1][0];

        ZZ_PTimer_Begin(SAT);   // <<== not just sat
        lbool result = checkDistinct(init, bad, N0, cex, /*cb*/NULL);
        ZZ_PTimer_End(SAT);
        if (!quiet) WriteLn (result == l_True) ? " (None)" : "";
        if (result == l_False){
            if (!quiet) WriteLn "Initial states and bad states overlap!";
            if (!quiet) WriteLn "Counterexample found.";
            return l_False;
        }else if (result == l_Undef)
            return l_Undef;
    }

    //*tmp*/const uint sim_depth = 50;
    //*tmp*/genSimData(N, 20, sim_depth, sim_vecs);

    // Main algorithm:
    Vec<GLit> s0;
    if (bf_depth) *bf_depth = clauses.size();
    clauses.push();     // -- no clauses in frame 0 (initial state)
    info.depth = clauses.size();
#if 0   // -- EXPERIMENTAL
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        if (flop_init[w] != l_Undef){
            GLit p(attr_Flop(w).number, flop_init[w] == l_True);
            Vec<GLit> s(1, p);
            addBlockingClause(s, 0, false);
        }
    }
#endif
    for(;;){
        if (getBadPredecessor(s0)){
            if (!blockState(s0, cex)){
                if (!quiet) NewLine;
                if (!quiet) WriteLn "Counterexample found.";
                return l_False;
            }

        }else{
            if (!quiet) NewLine;
            if (bf_depth) *bf_depth = clauses.size();
            if (par) sendMsg_Progress(0, 1, (FMT "bug-free-depth: %_\n", clauses.size()));
            clauses.push();         // -- open up a new state

            info.depth = clauses.size();
            if (pushClauses()){     // -- move clauses to their highest provable depth
                if (invariant)
                    storeInvariant(invariant);
                if (!quiet) WriteLn "Inductive invariant found.";
                if (bf_depth) *bf_depth = INT_MAX;
                return l_True;
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper function:


lbool propDrivenReach(NetlistRef        N0,
                      const Vec<Wire>&  props,
                      const Params_Pdr& P,
                      Cex*              cex,
                      NetlistRef        invariant,
                      int*              bf_depth,
                      EffortCB*         cb
                      )
{
    ZZ_PTimer_Begin(PDR_ALL);
    try{
        Pdr pdr(N0, props, cb, P);
        lbool result = pdr.run(cex, invariant, bf_depth);
        ZZ_PTimer_End(PDR_ALL);

        if (par){
            Vec<uint> props_; assert(props.size() == 1);      // -- for now, can only handle singel properties in PAR mode
            props_.push(0);

            if (result == l_Undef){
                sendMsg_Result_unknown(props_, 1/*safety prop*/);
            }else if (result == l_False){
                assert(cex);
                assert(bf_depth);
                assert(*bf_depth + 1 >= 0);
                Vec<uint> depths;
                depths.push(cex->depth());
                sendMsg_Result_fails(props_, 1/*safety prop*/, depths, *cex, N0, true);
            }else if (result == l_True){
                sendMsg_Result_holds(props_, 1/*safety prop*/);
            }else
                assert(false);
        }

        return result;

    }catch(Excp_Pdr_Abort){
        if (!P.quiet) NewLine;
        ZZ_PTimer_End(PDR_ALL);
        if (par)
            sendMsg_Abort("callback");
        return l_Undef;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
TODO:

  - Fler temporarer som medlemsvariabler!
  - Prova att generalisera vidare redan larda klausuler vid pushFwd; om tidskravande, lagra tillstand
    som failade for varje literal och se om subsumerat
  - Randomisera variabelordningen i SAT losaren
  - Generalisera ofullstandiga symetrier i F[k] och se om dessa hypoteser kan bevisas
*/
