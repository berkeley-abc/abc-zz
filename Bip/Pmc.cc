//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pmc.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR inspired BMC implementation.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Pmc.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ_CnfMap.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Heap.hh"

namespace ZZ {
using namespace std;


// Avoid linker conflicts:
#define TCube         Pmc_TCube
#define ProofObl      Pmc_ProofObl
#define ProofObl_Data Pmc_ProofObl_Data


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


ZZ_PTimer_Add(pmc_recycle);
ZZ_PTimer_Add(pmc_justify);
ZZ_PTimer_Add(pmc_weakenBySim);
ZZ_PTimer_Add(pmc_propagate);
ZZ_PTimer_Add(pmc_findInvar);
ZZ_PTimer_Add(pmc_solveImg);
ZZ_PTimer_Add(pmc_blockCube);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper type -- 'TCube':


// A "timed cube" is a variant type that is either:
//
//   - A pair '(cube, time_frame)'
//   - Just a cube (referred to as "untimed").
//
// Untimed cubes evaluate to FALSE if used as a conditional (e.g. in an if-statement). For timed
// cubes, a special 'frame_INF' may be used to denote the "infinite" time frame. For untimed cubes,
// the field 'frame' is 'frame_NULL'.

static const uint frame_INF  = UINT_MAX;
static const uint frame_NULL = UINT_MAX - 1;

struct TCube {
    Cube cube;
    uint frame;

    Null_Method(TCube) { return frame == frame_NULL; }

    TCube(Cube c = Cube_NULL, uint f = frame_NULL) : cube(c), frame(f) {}

    // Forward operations on the 'cube' member:
    uint   size()             const { return cube.size(); }
    GLit   operator[](uint i) const { return cube[i]; }
};


static const TCube TCube_NULL;


macro TCube next(TCube s) {
    assert(s.frame < frame_NULL);
    return TCube(s.cube, s.frame + 1); }


template<> fts_macro void write_(Out& out, const TCube& s) {
    FWrite(out) "(%_, ", s.cube;
    if      (s.frame == frame_INF ) FWrite(out) "inf)";
    else if (s.frame == frame_NULL) FWrite(out) "-)";
    else                            FWrite(out) "%_)", s.frame;
}

macro bool operator==(const TCube& x, const TCube& y) {
    return x.frame == y.frame && x.cube == y.cube;
}

macro bool operator<(const TCube& x, const TCube& y) {  // -- order tcubes first on 'cube', then on 'frame'
    return x.cube < y.cube || (x.cube == y.cube && x.frame < y.frame);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof Obligations:


// A proof obligation (PO) is a timed cube 'tcube = (cube, frame)' which has to be blocked,
// together with a priority 'prio'. POs are handled from the smallest frame number to the largest,
// and for ties, from the smallest priority to the the largest. Each PO stores a reference counted
// pointer to the 'next' PO that gave rise to it. If property fails, following this chain produces
// the counterexample.
//
struct ProofObl_Data {
    TCube   tcube;
    uint    prio;

    RefC<ProofObl_Data> next;
    uint                refC;
};


struct ProofObl : RefC<ProofObl_Data> {
    ProofObl() : RefC<ProofObl_Data>() {}
        // -- create null object

    ProofObl(TCube tcube, uint prio, ProofObl next = ProofObl()) :
        RefC<ProofObl_Data>(empty_)
    {
        (*this)->tcube = tcube;
        (*this)->prio  = prio;
        (*this)->next  = next;
    }

    ProofObl(const RefC<ProofObl_Data> p) : RefC<ProofObl_Data>(p) {}
        // -- downcast from parent to child
};


macro bool operator<(const ProofObl& x, const ProofObl& y) {
    assert(x); assert(y);
    return x->tcube.frame < y->tcube.frame || (x->tcube.frame == y->tcube.frame && x->prio < y->prio); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Pmc' class:


class Pmc {
  //________________________________________
  //  Problem statement:

    NetlistRef            N;
    Params_Pmc            P;

    CCex&                 cex;

  //________________________________________
  //  State:

    Wire                        w_prop;
    Vec<MultiSat>               S;  // -- main solver for image queries
    Vec<Vec<LLMap<GLit,Lit> > > n2s;
    Vec<uint>                   wasted_lits;
    Vec<lbool>                  model;
    uint                        model_frame;

    MiniSat2              Z;        // -- used for initial state queries
    Vec<LLMap<GLit,Lit> > n2z;      // -- only a vector because 'lutClausify()' requires it

    MiniSat2              R;
    Vec<LLMap<GLit,Lit> > n2r;

    uint                  depth;    // -- contains largest frame used in 'F' plus one.
    Vec<TCube>            F;        // -- list of pairs: '(cube, frame)'
    Vec<Cube>             F_inf;    // -- holds in reachable state space
    WMap<uint>            level;    // -- used in justification heuristic
    Vec<Lit>              act;      // -- used in single SAT mode only
    uint                  shared_clauses;

  //________________________________________
  //  Internal methods:

    void   showProgress(String prefix);

    void   addFrame();
    Lit    clausify(uint d, GLit p, uint side = 0);
    Lit    clausifyInit(GLit p);
    Lit    clausifyInvar(GLit p, uint side = 0);
    void   recycleSolver(uint d);

    void   storeModel(uint d);
    lbool  lvalue(GLit p, uint side = 0);
    bool   value (GLit p, uint side = 0);

    void   justify(const Cube& c, /*outputs:*/WSeen seen[2], Vec<GLit>& sources, WZet& sinks);
    Cube   weakenBySim(WSeen seen[2], Cube src, Cube snk);
    Cube   extractModel(Cube c);

    template<class GVec>
    bool   isInitial (const GVec& c, GLit except = glit_NULL);
    void   addCube   (TCube s);
    bool   isBlocked (TCube s);
    TCube  solveImg  (TCube s, bool extract_model);
    TCube  generalize(TCube s);
    bool   blockCube (TCube s);
    bool   findInvar ();
    bool   propagate ();

    void   extractCex(ProofObl pobl);

public:
  //________________________________________
  //  Main:

    Pmc(NetlistRef N_, const Params_Pmc& P_, CCex& cex_) :
        N(N_), P(P_), cex(cex_), n2z(1), n2r(2), depth(0), shared_clauses(0) {}

    bool run();
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Progress:


void Pmc::showProgress(String prefix)
{
    Write "\a/[%<6%t]\a/ ", cpuTime();
    Write "\a*%_:\a*", prefix;
#if 0
    uint sum = 0;
    for (uint k = 1; k < F.size(); k++){
        Write " %_", F[k].size();
        sum += F[k].size();
    }
    WriteLn " = %_", sum;
#else
    WriteLn " %_ cubes (%_ in solver)", F.size(), F.size() - shared_clauses;
#endif

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clausification:


void Pmc::addFrame()
{
    depth++;
    if (P.multi_sat || S.size() == 0){
        S.push();
        S.last().selectSolver(P.sat_solver);
        n2s.push();
        n2s.last().growTo(2);
        wasted_lits.push(0);
    }
    if (!P.multi_sat)
        act.push(S[0].addLit());
}


Lit Pmc::clausify(uint d, GLit p, uint side)
{
    if (!P.multi_sat) d = 0;

    if (!+n2s[d][side][p]){
        Vec<Pair<uint,GLit> > roots;
        roots.push(tuple(side, p));
        lutClausify(N, roots, false, S[d], n2s[d]);
    }

    return n2s[d][side][p];
}


Lit Pmc::clausifyInit(GLit p)
{
    if (!+n2z[0][p]){
        Vec<Pair<uint,GLit> > roots;
        roots.push(tuple(0, p));
        lutClausify(N, roots, true, Z, n2z);
    }

    return n2z[0][p];
}


Lit Pmc::clausifyInvar(GLit p, uint side)
{
    if (!+n2r[side][p]){
        Vec<Pair<uint,GLit> > roots;
        roots.push(tuple(side, p));
        lutClausify(N, roots, false, R, n2r);
    }

    return n2r[side][p];
}


void Pmc::recycleSolver(uint d)
{
    ZZ_PTimer_Scope(pmc_recycle);
    //**/WriteLn "[RECYCLE]";

    // Clear maps:
    S[d].clear();
    assert(n2s[d].size() == 2);
    n2s[d][0].clear();
    n2s[d][1].clear();
    wasted_lits[d] = 0;

    // Quadratic subsumption, fix this later: (group cubes into levels and have localized occurance lists?)
    for (uint i = 0; i < F.size(); i++){
        if (!F[i]) continue;
        for (uint j = 0; j < F.size(); j++){
            if (i != j && F[j] && F[i].frame == F[j].frame && subsumes(F[i].cube, F[j].cube))
                F[j] = TCube_NULL;
        }
    }
    filterOut(F, isNull<TCube>);

    // Add clauses:
    Vec<Lit> tmp;
    if (P.multi_sat){
        for (uint i = 0; i < F_inf.size(); i++){
            for (uint j = 0; j < F_inf[i].size(); j++)
                tmp.push(~clausify(d, F_inf[i][j]));
            S[d].addClause(tmp);
            tmp.clear();
        }

        for (uint i = 0; i < F.size(); i++){
            if (F[i].frame != d) continue;

            for (uint j = 0; j < F[i].size(); j++)
                tmp.push(~clausify(d, F[i][j]));
            S[d].addClause(tmp);
            tmp.clear();
        }

    }else{
        // Insert invariant clauses:
        for (uint i = 0; i < F_inf.size(); i++){
            for (uint j = 0; j < F_inf[i].size(); j++)
                tmp.push(~clausify(0, F_inf[i][j]));
            S[0].addClause(tmp);
            tmp.clear();
        }

        // Setup base activation literals:
        act.clear();
        for (uint i = 0; i < depth; i++)
            act.push(S[0].addLit());

        sortUnique(F);

        if (F.size() > 0){
            Map<Pair<uint,uint>, Lit> rng_act;  // -- range activation literals (an optimization)

            shared_clauses = 0;
            uint start = 0;
            for (uint i = 1; i <= F.size(); i++){
                if (i == F.size() || F[i].cube != F[start].cube || F[i].frame != F[i-1].frame + 1){
                    // Get activation literal for range:
                    Pair<uint,uint> rng(F[start].frame, F[i-1].frame);
                    Lit* a;
                    if (!rng_act.get(rng, a)){
                        if (rng.fst == rng.snd)
                            *a = act[rng.fst];
                        else{
                            *a = S[0].addLit();
                            for (uint k = rng.fst; k <= rng.snd; k++)
                                S[0].addClause(~act[k], *a);
                        }
                    }

                    shared_clauses += rng.snd - rng.fst;

                    // Insert clause:
                    for (uint j = 0; j < F[start].size(); j++)
                        tmp.push(~clausify(0, F[start][j]));
                    tmp.push(~*a);
                    S[0].addClause(tmp);
                    tmp.clear();

                    start = i;
                }
            }

            //**/WriteLn "(auxiliary activation literals: %_)", rng_act.size();
        }
    }
}


void Pmc::storeModel(uint d)
{
    if (!P.multi_sat) d = 0;

    S[d].getModel(model);
    model_frame = d;
}


inline lbool Pmc::lvalue(GLit w, uint side)
{
    Lit lit_w = n2s[model_frame][side][w];
    if (!+lit_w)
        return l_Undef;
    else
        return model[lit_w.id] ^ lit_w.sign;
}


inline bool Pmc::value(GLit w, uint side)
{
    lbool v = lvalue(w, side); assert(v != l_Undef);
    return v == l_True;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Model extraction and weakening:


//=================================================================================================
// -- Justification:


macro uint pinNumPlusOne(Wire w, Wire w_in)
{
    For_Inputs(w, v)
        if (+v == +w_in)
            return Iter_Var(v) + 1;
    return 0;
}


// Justify 'c', marking nodes in the justification cone as "seen". The sources (PIs and SIs
// of left frame) are stored in 'sources' WITH sign.
// PRE-CONDITION: SAT-model must have been stored.
void Pmc::justify(const Cube& c, /*outputs:*/WSeen seen[2], Vec<GLit>& sources, WZet& sinks)
{
    ZZ_PTimer_Scope(pmc_justify);

    #define Push(w, d) (seen[d].add(+(w)) || (Q.push(tuple(+(w).lit(), d)), true))

    // Justify:
    Vec<Pair<GLit,uint> > Q;
    seen[0].add(N.True());
    seen[1].add(N.True());

    for (uint i = 0; i < c.size(); i++)
        Push(c[i] + N, 1);

    while (Q.size() > 0){
        Wire w = Q.last().fst + N; assert(!sign(w));
        uint d = Q.last().snd;
        Q.pop();

        if (type(w) == gate_Flop){
            if (d == 1)
                Push(w[0], d-1);
            else
                sources.push(+w);

        }else if (type(w) == gate_PI){
            if (d == 0)
                sources.push(+w);

        }else if (type(w) == gate_SO){
            assert(d == 0);
            Push(w[0], d);
            sinks.add(w);

        }else if (type(w) == gate_Npn4){
            // Compute justifications:
            uint cl = attr_Npn4(w).cl;
            uint a = 0;
            For_Inputs(w, v)
                if (value(v, d))
                    a |= 1u << Iter_Var(v);

            uint just = npn4_just[cl][a];       // -- look up all minimal justifications

            // Pick first justification:
            for (uint i = 0; i < 4; i++){
                if (just & (1u << i))
                    Push(w[i], d);
            }

        }else
            assert(false);
    }

    sortUnique(sources);

    // Add sign to sources:
    for (uint i = 0; i < sources.size(); i++)
        if (!value(sources[i]))
            sources[i] = ~sources[i];

    #undef Push
}


//=================================================================================================
// -- Ternary simulation based weakening:


static
lbool ternaryEval(ftb4_t ftb, const lbool in[4])
{
    ftb4_t hi = ftb;
    ftb4_t lo = ftb;
    for (uint i = 0; i < 4; i++){
        if (in[i] == l_True){
            lo = (lo & lut4_buf[i]) >> (1u << i);
            hi = (hi & lut4_buf[i]) >> (1u << i);
            //*Tr*/WriteLn "  - in[%_]=1:  lo=%.4X  hi=%.4X", i, lo, hi;
        }else if (in[i] == l_Undef){
            hi |= hi >> (1u << i);
            lo &= lo >> (1u << i);
            //*Tr*/WriteLn "  - in[%_]=?:  lo=%.4X  hi=%.4X", i, lo, hi;
        }
        //*Tr*/else WriteLn "  - in[%_]=0:  lo=%.4X  hi=%.4X   (unchanged)", i, lo, hi;
    }
    hi &= 1;
    lo &= 1;

    return (hi == lo) ? lbool_lift(hi) : l_Undef;
}


static
void evalGate(Wire w, uint d, WMap<lbool> sim[2])
{
    switch (type(w)){
    case gate_SO:
        sim[d](w) = sim[d][w[0]] ^ sign(w[0]);
        break;

    case gate_Flop:
        assert(d == 1);
        sim[d](w) = sim[d-1][w[0]] ^ sign(w[0]);
        break;

    case gate_Npn4:{
        lbool in[4] = { l_Undef, l_Undef, l_Undef, l_Undef };
        For_Inputs(w, v){
            in[Iter_Var(v)] = sim[d][v] ^ sign(v); }

        ftb4_t ftb = npn4_repr[attr_Npn4(w).cl];
        sim[d](w) = ternaryEval(ftb, in);
        //*Tr*/WriteLn "  - ternary eval: ftb=%.4X  in=%_", ftb, slice(in[0], in[4]);
        break;}

    default:
        ShoutLn "INTERNAL ERROR! Trying to evaluate: %_", w;
        assert(false);
    }
}


class PropQ {
    NetlistRef N;

    Vec<GLit> Q;    // -- unsigned literals = left frame, signed = right frame
    WZetS     in_Q;

public:
    PropQ(NetlistRef N_, const WMap<uint>& level_) :
        N(N_) {}

    void clear() { Q.clear(); in_Q.clear(); }

    bool add(uint frame, GLit p) {  // -- returns TRUE if element already existed.
        assert(frame < 2);
        Wire w = N[+p ^ (frame == 1)];
        if (in_Q.has(w))
            return true;
        else{
            Q.push(w);
            in_Q.add(w);
            return false;
        }
    }

    Pair<uint, Wire> pop() {
        GLit p = Q.popC();
        in_Q.exclude(p + N);
        return tuple(uint(p.sign), N[+p]);
    }

    uint size() const { return Q.size(); }
};


// Helper function for 'weakenBySim()'. Enqueues the fanouts 'fs' of wire 'w' in frame 'frame', or
// the next if fanout is a flop. Only fanouts in 'seen' will be enqueued.
static
void enqueueFanouts(const Fanouts& fs, uint frame, PropQ& Q, const WSeen seen[2])
{
    for (uint i = 0; i < fs.size(); i++){
        Wire w = fs[i];
        uint d = (type(w) == gate_Flop) ? frame + 1 : frame;
        if (d == 2 || !seen[d].has(w)) continue;
        bool had ___unused = Q.add(d, w);
    }
}


// NOTE! Will remove sources from 'seen[0]'.
Cube Pmc::weakenBySim(WSeen seen[2], Cube src, Cube snk)
{
    ZZ_PTimer_Scope(pmc_weakenBySim);

    Get_Pob(N, fanouts);
    Get_Pob(N, up_order);

    // Analyze removable PIs:
    WZet reach;
    for (uint i = 0; i < src.size(); i++){
        Wire w = src[i] + N;
        if (type(w) != gate_PI)
            reach.add(w);
    }
    for (uint i = 0; i < reach.size(); i++){
        Wire w = reach[i] + N;
        if (type(w) != gate_Flop){
            For_Inputs(w, v)
                reach.add(v);
        }
    }

    // Initial simulation:
    WMap<lbool> sim[2];
    PropQ Q(N, level);

    For_Gates(N, w){
        if (seen[0].has(w)) sim[0](w) = lvalue(w, 0);
        if (seen[1].has(w)) sim[1](w) = lvalue(w, 1);
    }

    for (uint i = 0; i < src.size(); i++)
        seen[0].exclude(src[i] + N);                // -- this will prevent update of sources

    // Try to put as many X as possible into 'src':
    WZet snk_set;
    for (uint i = 0; i < snk.size(); i++) snk_set.add(snk[i] + N);

    Vec<Trip<uint,GLit,lbool> > undo;   // -- triplets '(frame, gate, value)'
    Vec<GLit> cube(copy_, src);

    for (uint i = 0; i < cube.size(); i++){
        Wire w0 = cube[i] + N;

        if (type(w0) == gate_PI && !reach.has(w0)){
            cube[i] = glit_NULL;        // -- we assume initial state does not depend on PIs
            continue; }

        undo.push(tuple(0, w0, sim[0][w0]));
        sim[0](w0) = l_Undef;
        enqueueFanouts(fanouts[w0], 0, Q, seen);

        while (Q.size() > 0){                           // -- propagate
            uint d; Wire w; l_tuple(d, w) = Q.pop();
            if (sim[d][w] == l_Undef) continue;

            undo.push(tuple(d, w, sim[d][w]));
            evalGate(w, d, sim);
            if (sim[d][w] == l_Undef && snk_set.has(w)){
                // Undo:
                for (uint j = undo.size(); j > 0;) j--,
                    sim[undo[j].fst](undo[j].snd + N) = undo[j].trd;
                goto KeepLit;
            }
            if (sim[d][w] != undo.last().trd)
                enqueueFanouts(fanouts[w], d, Q, seen);
        }
        cube[i] = glit_NULL;

      KeepLit:
        Q.clear();
        undo.clear();
    }

    filterOut(cube, isNull<GLit>);

    return Cube(cube);
}


//=================================================================================================
// -- Proof-obligation orchestration:


// Extract model in terms of a left frame variables, enough to justify 'c' in the right frame.
Cube Pmc::extractModel(Cube c)
{
    // Justify:
    WSeen     seen[2];
    Vec<GLit> sources;
    WZet      sinks;

    justify(c, seen, sources, sinks);
    Cube cut = Cube(sources);
    return weakenBySim(seen, cut, c);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Major methods:


void Pmc::addCube(TCube s)
{
    assert(s.frame < depth);
    F.push(s);

    // Add cube to SAT solver:
    uint d = s.frame;
    assert(s.size() != 0);
    Vec<Lit> tmp;
    for (uint i = 0; i < s.size(); i++)
        tmp.push(~clausify(d, s[i]));
    if (P.multi_sat)
        S[d].addClause(tmp);
    else{
        tmp.push(~act[d]);
        S[0].addClause(tmp);
    }
}


bool Pmc::findInvar()
{
    ZZ_PTimer_Scope(pmc_findInvar);

    // Select candidate cubes:
    sortUnique(F);
    Vec<Cube> cands;
    Vec<GLit> cinv;
    for (uint i = 0; i < F.size(); i++){
        if (i+1 < F.size() && F[i+1].cube == F[i].cube) continue;

        if (!isInitial(F[i].cube))
            cands.push(F[i].cube);
    }

    // Find maximal inductive subset: (NOTE! 'F_inf' included implicitly)
    Vec<Lit> tmp;
    for(;;){
        // Reset clausification:
        R.clear();
        n2r[0].clear();
        n2r[1].clear();

        // Insert 'F_inf':
        for (uint i = 0; i < F_inf.size(); i++){
            tmp.clear();
            for (uint j = 0; j < F_inf[i].size(); j++)
                tmp.push(clausifyInvar(~F_inf[i][j]));
            R.addClause(tmp);
        }

        // Insert 'cands' on LHS:
        for (uint i = 0; i < cands.size(); i++){
            const Cube& c = cands[i];
            tmp.clear();
            for (uint j = 0; j < c.size(); j++)
                tmp.push(clausifyInvar(~c[j]));
            R.addClause(tmp);
        }

        // See which cubes of 'cands' hold on RHS:
        uint n_cands = cands.size();
        for (uint i = 0; i < cands.size();){
            const Cube& c = cands[i];
            tmp.clear();
            for (uint j = 0; j < c.size(); j++)
                tmp.push(clausifyInvar(c[j], 1));
            lbool result = R.solve(tmp);

            if (result == l_True){
                cands[i] = cands.last();
                cands.pop();
            }else
                i++;
        }

        if (cands.size() == n_cands)
            break;
    }

    // <<== in multi-sat mode (in particular) we need to filter out candidates already in F_inf

    if (cands.size() > 0){
        /**/WriteLn "  -->> %_ new invariant cubes", cands.size();

        for (uint i = 0; i < cands.size(); i++){
            for (uint j = 0; j < F.size(); j++){
                if (F[j].cube == cands[i])
                    F[j] = TCube_NULL;
            }
        }
        filterOut(F, isNull<TCube>);

        append(F_inf, cands);
        sortUnique(F_inf);

        if (P.term_check){
            // Check if property is implied by 'F_inf':
            R.clear();
            n2r[0].clear();
            n2r[1].clear();

            for (uint i = 0; i < F_inf.size(); i++){
                tmp.clear();
                for (uint j = 0; j < F_inf[i].size(); j++)
                    tmp.push(clausifyInvar(~F_inf[i][j]));
                R.addClause(tmp);
            }

            R.addClause(~clausifyInvar(w_prop, 1));

            lbool result = R.solve();
            if (result == l_False)
                return true;        // -- property PROVED
        }
    }

    return false;
}


#if 0
void Pmc::constProp()
{
}
#endif


bool Pmc::propagate()
{
    ZZ_PTimer_Scope(pmc_propagate);

    addFrame();

    sortUnique(F);
    Vec<TCube> G(copy_, F);
    for (uint i = 0; i < G.size(); i++){
        if (i+1 < G.size() && G[i+1].cube == G[i].cube) continue;

        TCube s = solveImg(next(G[i]), false);
        if (s) addCube(s);
    }

#if 1
    XSimulate sim(N);
    Cex cex;

    cex.inputs.push();
    For_Gatetype(N, gate_PI, w)
        cex.inputs[0](w) = l_Undef;

    cex.flops.push();
    For_Gatetype(N, gate_Flop, w)
        cex.flops[0](w) = l_Undef;

    for (uint i = 0; i < F.size(); i++){
        if (F[i].frame == depth - 2 && F[i].cube.size() == 1)
            cex.flops[0](F[i].cube[0] + N) = (F[i].cube[0].sign ? l_True : l_False);
    }

    sim.simulate(cex);

    uint n_consts = 0;
    For_Gatetype(N, gate_Flop, w){
        if (sim[0][w[0]] != l_Undef){
            F.push(TCube(Cube(w ^ sign(w[0]) ^ (sim[0][w[0]] == l_True)), depth - 1));
            n_consts++; }
    }
    /**/if (n_consts > 1) WriteLn "  -->> %_ constant state variables", n_consts - 1;     // -- subtract the constant state variable we have introduced ourselves
#endif

    return P.use_f_inf ? findInvar() : false;
}


// Check if timed cube 's' is already blocked by the trail.
bool Pmc::isBlocked(TCube s)
{
    Cube c = s.cube;
    uint k = s.frame;
    Vec<Lit> assumps;

    // Assume cube:
    for (uint i = 0; i < c.size(); i++)
        assumps.push(clausify(k, c[i]));

    if (!P.multi_sat){
        assumps.push(act[k]);
        k = 0; }

    lbool result = S[k].solve(assumps); assert(result != l_Undef);
    return result == l_False;
}


// Check if 'c' is a cube consistent with (overlaps with) the initial states. 
// The cube 'c' may contain 'glit_NULL's (which are ignored).
template<class GVec>
inline bool Pmc::isInitial(const GVec& c, GLit except)
{
    bool sat_needed = false;
    for (uint i = 0; i < c.size(); i++){
        if (!c[i] || c[i] == except) continue;
        if (type(c[i] + N) != gate_Flop && type(c[i] + N) != gate_PI){
            sat_needed = true;
            break; }
    }

    if (sat_needed){
        // Check by SAT call:
        Vec<Lit> assumps;
        for (uint i = 0; i < c.size(); i++){
            if (!c[i] || c[i] == except) continue;
            assumps.push(clausifyInit(c[i]));
        }

        lbool result = Z.solve(assumps); assert(result != l_Undef);
        return result == l_True;

    }else{
        // Check syntactically:
        Get_Pob(N, flop_init);
        for (uint i = 0; i < c.size(); i++){
            if (!c[i] || c[i] == except) continue;
            Wire w = c[i] + N;
            if (type(w) == gate_PI) continue;
            if ((flop_init[w] ^ sign(w)) == l_False)
                return false;
        }
        return true;
    }
}


// Solve the SAT query "F & ~s & T & s'" where "s'" is of frame 's.frame' (i.e. proving 's' to be
// unreachable or find a predecessor witnessing the contrary).
TCube Pmc::solveImg(TCube s, bool extract_model)
{
    ZZ_PTimer_Scope(pmc_solveImg);

    Cube c = s.cube;
    uint k = s.frame - 1;
    Vec<Lit> assumps;

    uint k0 = P.multi_sat ? k : 0;
    wasted_lits[k0]++;      // -- bad name

    // Assume 's' at state outputs:
    for (uint i = 0; i < c.size(); i++)
        assumps.push(clausify(k, c[i], 1));

    if (!P.multi_sat)
        assumps.push(act[k]);

    // Solve:
    lbool result = S[k0].solve(assumps); assert(result != l_Undef);

    TCube ret;
    if (result == l_False){ // -- UNSAT
        // Figure out subset of 's' enough for UNSAT:
        Vec<Lit> confl;
        S[k0].getConflict(confl);

        // Get UNSAT core:
        Vec<GLit> z(copy_, c);
        for (uint i = 0; i < z.size(); i++){
            Wire w = z[i] + N;
            if (!has(confl, clausify(k, w, 1))){
                z[i] = z.last();
                z.pop();
                i--;
            }
        }
        ret = TCube(Cube(z), k+1);

    }else{ // -- SAT
        storeModel(k);
        ret = extract_model ? TCube(extractModel(c)) : TCube();
    }

    return ret;
}


TCube Pmc::generalize(TCube s)
{
    Vec<GLit> cube(copy_, s.cube);

    // Shrink 's' (single orbit):
    for (uint i = 0; i < cube.size(); i++){
        if (has(s.cube, cube[i])){
            TCube new_s(s.cube - cube[i], s.frame);
            condAssign(s, solveImg(new_s, false));
        }
    }
    //**/if (cube.size() != s.cube.size()) WriteLn "%_ -> %_", cube.size(), s.cube.size();

    return s;
}


bool Pmc::blockCube(TCube s0)
{
    ZZ_PTimer_Scope(pmc_blockCube);

    KeyHeap<ProofObl> Q;
    uint prioC = UINT_MAX;
    Q.add(ProofObl(s0, prioC--));

    uint iter = 0;
    while (Q.size() > 0){
        // Pop proof-obligation:
        ProofObl po = Q.pop();
        TCube    s  = po->tcube;

        if (!isBlocked(s)){
            if (s.frame == 0){
                // Found counterexample:
                extractCex(po);
                return false; }

            assert(!isInitial(s.cube));
            TCube z;
            z = solveImg(s, true);
            if (z){
                // Cube 's' was blocked by image of predecessor:
                addCube(P.cube_gen ? generalize(z) : z);

            }else{
                // Cube 's' was NOT blocked by image of predecessor:
                z.frame = s.frame - 1;
                Q.add(ProofObl(z, prioC--, po));        // -- enqueue the predecessor as a new proof-obligation, and re-enqueue the current P.O. as well
                Q.add(ProofObl(s, prioC--, po->next));  // -- both 's' and 'z' are models here
            }
        }

        iter++;
        if ((iter & 511) == 0)
            showProgress("work.");
    }

    return true;
}


bool Pmc::run()
{
    // Add a flop on top of the property:
    Get_Pob(N, properties);
    Get_Pob(N, flop_init);
    assert(properties.size() == 1);
    Wire w_prop_in = N.add(SO_(), properties[0][0] ^ sign(properties[0]));
    w_prop = N.add(Flop_(), w_prop_in);
    flop_init(w_prop) = l_True;

    removeAllUnreach(N);

    // Add pobs:
    Add_Pob(N, fanouts);
    Add_Pob(N, up_order);

    // Levelize circuit:
    level.nil = 0;
    For_UpOrder(N, w){
        if (type(w) == gate_Flop) continue;
        For_Inputs(w, v)
            newMax(level(w), level[v] + 1);
    }

    // Add (negation of) initial state to 'F[0]':
    assert(depth == 0);
    addFrame();

    For_Gatetype(N, gate_Flop, w)
        if (flop_init[w] != l_Undef)
            addCube(TCube(Cube(w ^ (flop_init[w] == l_True)), 0));

    //**/N.write("N.gig"); WriteLn "Wrote: N.gig";

    // Keep proving property deeper and deeper until CEX is found:
    for(;;){
        if (depth > 1)
            WriteLn "-------------------- Depth %_ --------------------", depth-2;

        if (!blockCube(TCube(Cube(~w_prop), depth-1))){
            showProgress("final");
            WriteLn "Counterexample of depth %_ found.", cex.depth();
            return false;

        }else{
            showProgress("block");
            if (propagate()){
                showProgress("final");
                WriteLn "Inductive invariant found.";
                return true;
            }
            if (!P.multi_sat && wasted_lits[0] > 10000)
                recycleSolver(0);
            showProgress("prop.");
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Result collection:


void Pmc::extractCex(ProofObl pobl)
{
    // Get initial state:
    Get_Pob(N, flop_init);
    Vec<Lit> st;
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num != num_NULL && flop_init[w] != l_Undef)
            st.push(w ^ (flop_init[w] == l_False));
    }

    // Get a fresh SAT solver:
    uint d = depth;
    addFrame();

    // Clausify all flops:
    For_Gatetype(N, gate_Flop, w)
        clausify(d, w, 1);

    // Accumulate trace:
    Vec<Lit> assumps;
    bool first = true;
    for(;;){
        Cube cL = pobl->tcube.cube;
        pobl = pobl->next;
        if (!pobl) break;

        Cube cR = pobl->tcube.cube;

        assumps.clear();
        for (uint j = 0; j < st.size(); j++) assumps.push(clausify(d, st[j], 0));
        for (uint j = 0; j < cL.size(); j++) assumps.push(clausify(d, cL[j], 0));
        for (uint j = 0; j < cR.size(); j++) assumps.push(clausify(d, cR[j], 1));

        if (!P.multi_sat) assumps.push(act[d]);
        lbool result = S[P.multi_sat ? d : 0].solve(assumps);
        assert(result == l_True);
        storeModel(d);

        cex.inputs.push();
        For_Gatetype(N, gate_PI, w){
            cex.inputs.last()(attr_PI(w).number) = lbool_lift(lvalue(w) == l_True); }

        // If initial state, store flop values:
        if (first){
            cex.flops.push();
            For_Gatetype(N, gate_Flop, w){
                int num = attr_Flop(w).number;
                if (num != num_NULL)
                    cex.flops[0](num) = lbool_lift(lvalue(w) == l_True);
            }
            first = false;
        }

        // Extract flop state in right frame:
        st.clear();
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            if (num != num_NULL)
                st.push(w ^ !value(w, 1));
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Returns TRUE if properties hold, FALSE if CEX is found.
bool pmc( NetlistRef          N,
          const Vec<Wire>&    props,
          const Params_Pmc&   P,
          Cex*                cex
          )
{
    WWMap n2l;
    Netlist L;

    // Preprocess netlist:
    {
        Netlist M;
        WMap<Wire> n2m;
        initBmcNetlist(N, props, M, /*keep_flop_init*/true, n2m);

        WWMap m2l;
        Params_CnfMap PC;
        PC.quiet = true;
        cnfMap(M, PC, L, m2l);

        For_Gates(N, w)
            n2l(w) = m2l[n2m[w]];

        // Copy initial state:
        {
            Get_Pob(N, flop_init);
            Add_Pob2(L, flop_init, new_flop_init);
            For_Gatetype(N, gate_Flop, w)
                if (n2l[w])     // -- some flops are outside the COI
                    new_flop_init(n2l[w] + L) = flop_init[w];
        }

        // Copy properties: (should be singleton)
        {
            Get_Pob(M, properties);
            Add_Pob2(L, properties, new_properties);
            for (uint i = 0; i < properties.size(); i++)
                new_properties.push(m2l[properties[i]] + L);
        }

        // Split flops:
        splitFlops(L);
    }

    // Run PDR algorithm:
    CCex ccex;
    Pmc  pmc(L, P, ccex);
    bool ret = pmc.run();
    if (!ret && cex)
        translateCex(ccex, N, *cex);
    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parameters:


#define S(arg) (FMT "%_", arg)
#define B(arg) ((arg) ? "yes" : "no")


static
String select(String enum_alts, uint i)
{
    Vec<Str> alts;
    splitArray(enum_alts.slice(), "{} ,", alts);
    assert(i < alts.size());
    return String(alts[i]);
}


void addCli_Pmc(CLI& cli)
{
    // GLOBAL:
    Params_Pmc P;  // -- get default values.
    String sat_types   = "{zz, msc, abc}";
    String sat_default = select(sat_types, (P.sat_solver == sat_Zz) ? 0 : (P.sat_solver == sat_Msc) ? 1 : 2);

    cli.add("multi"    , "bool"          , B(P.multi_sat)    , "Use one SAT-solver per time-frame.");
    cli.add("finf"     , "bool"          , B(P.use_f_inf)    , "Compute inductive subset (\"F[inf]\") in propagation phase.");
    cli.add("term"     , "bool"          , B(P.term_check)   , "Check if F[inf] suffices as a 1-inductive proof.");
    cli.add("gen"      , "bool"          , B(P.cube_gen)     , "Use PDR-style cube generalization.");
    cli.add("sat"      , sat_types       , sat_default       , "SAT-solver to use.");
}


void setParams(const CLI& cli, Params_Pmc& P)
{
    P.multi_sat  = cli.get("multi").bool_val;
    P.use_f_inf  = cli.get("finf").bool_val;
    P.use_f_inf  = cli.get("term").bool_val;
    P.cube_gen   = cli.get("gen").bool_val;
    P.sat_solver = (cli.get("sat").enum_val == 0) ? sat_Zz :
                   (cli.get("sat").enum_val == 1) ? sat_Msc :
                   (cli.get("sat").enum_val == 2) ? sat_Abc : (assert(false), sat_NULL);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
