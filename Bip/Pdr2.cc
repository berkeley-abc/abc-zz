//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pdr2.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : PDR with learned cubes over arbitrary internal variables.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Pdr2.hh"
#include "ZZ_CnfMap.hh"
#include "ZZ_Npn4.hh"
#include "ZZ/Generics/Heap.hh"
#include "ZZ/Generics/RefC.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


ZZ_PTimer_Add(pdr2_justify);
ZZ_PTimer_Add(pdr2_minCut);
ZZ_PTimer_Add(pdr2_weakenBySim);
ZZ_PTimer_Add(pdr2_weakenBySim_init);
ZZ_PTimer_Add(pdr2_clausify);
ZZ_PTimer_Add(pdr2_extractModel);
ZZ_PTimer_Add(pdr2_addCube);
ZZ_PTimer_Add(pdr2_recycleSolver);
ZZ_PTimer_Add(pdr2_isBlocked);
ZZ_PTimer_Add(pdr2_isInitial);
ZZ_PTimer_Add(pdr2_ssSim);
ZZ_PTimer_Add(pdr2_ssStoreVec);
ZZ_PTimer_Add(pdr2_semantSubsumes);
ZZ_PTimer_Add(pdr2_solveRel);
ZZ_PTimer_Add(pdr2_generlize);
ZZ_PTimer_Add(pdr2_blockCube);
ZZ_PTimer_Add(pdr2_propagate);

/*tmp*/ZZ_PTimer_Add(aug_quick);
/*tmp*/ZZ_PTimer_Add(aug_bfs);
/*tmp*/ZZ_PTimer_Add(aug_update);


// Avoid linker conflicts:
#define TCube         Pdr2_TCube
#define ProofObl      Pdr2_ProofObl
#define ProofObl_Data Pdr2_ProofObl_Data


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


static
uint lubyLog(uint x)
{
    uint size, seq;
    for (size = 1, seq = 0; size <= x; seq++, size = 2*size + 1);

    while (x != size - 1){
        size >>= 1;
        seq--;
        if (x >= size) x-= size;
    }

    return seq;
}


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

    typedef uint TCube::*bool_type;
    operator bool_type() const { return (frame == frame_NULL) ? 0 : &TCube::frame; }

    TCube(Cube c = Cube_NULL, uint f = frame_NULL) : cube(c), frame(f) {}

    // Forward operations on the 'cube' member:
    uint   size      ()       const { return cube.size(); }
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
// 'Pdr2' class:


static const uint sr_NoInduct     = 1;      // -- solve [P & F & T & s'], otherwise [P & F & ~s & T & s']
static const uint sr_ExtractModel = 2;      // -- if SAT, extract and weaken model by ternary simulation

class Pdr2 {
  //________________________________________
  //  Problem statement:

    NetlistRef            N;
    Params_Pdr2           P;

    CCex&                 cex;
    NetlistRef            N_invar;

  //________________________________________
  //  State:

    Vec<MultiSat>               S;  // -- main solver for relative induction queries
    Vec<Vec<LLMap<GLit,Lit> > > n2s;
    Vec<uint>                   wasted_lits;
    Vec<lbool>                  model;
    uint                        model_frame;

    MiniSat2              Z;        // -- used for initial state queries
    Vec<LLMap<GLit,Lit> > n2z;      // -- only a vector because 'lutClausify()' requires it

    Vec<Vec<Cube> >       F;
    WMap<float>           activity;

    WMap<uint>            level;    // -- used in justification heuristic

    uint64                seed;

    // simulation vectors (for subsumption testing)
    // occurance lists (ditto?)

  //________________________________________
  //  Internal methods:

    void   showProgress(String prefix);

    void   addFrame();
    Lit    clausify(uint d, GLit p, uint side = 0);
    Lit    clausifyInit(GLit p);
    void   recycleSolver(uint d);

    void   storeModel(uint d);
    lbool  lvalue(GLit p, uint side = 0);
    bool   value (GLit p, uint side = 0);

    void   justify(const Cube& c, /*outputs:*/WSeen seen[2], Vec<GLit>& sources, WZet& sinks);
    Cube   minCut(WSeen& seen, const Vec<GLit>& sources, const WZet& sinks);
    Cube   weakenBySim(WSeen seen[2], Cube src, Cube snk);
    Cube   extractModel(Cube c);

    template<class GVec>
    bool   isInitial (const GVec& c, GLit except = glit_NULL);
    void   addCube   (TCube s);
    bool   isBlocked (TCube s);
    TCube  solveRel  (TCube s, uint params = 0);
    TCube  generalize(TCube s);
    bool   blockCube (TCube s);
    bool   propagate ();

    void   extractCex(ProofObl pobl);
    uint   invariantSize();
    void   storeInvariant(NetlistRef N_invar);

public:
  //________________________________________
  //  Main:

    Pdr2(NetlistRef N_, const Params_Pdr2& P_, CCex& cex_, NetlistRef N_invar_) :
        N(N_), P(P_), cex(cex_), N_invar(N_invar_), n2z(1), activity(0), seed(DEFAULT_SEED) {}

    bool run();
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Progress:


void Pdr2::showProgress(String prefix)
{
    Write "\a/[%<6%t]\a/ ", cpuTime();
    Write "\a*%_:\a*", prefix;
    uint sum = 0;
    for (uint k = 1; k < F.size(); k++){
        Write " %_", F[k].size();
        sum += F[k].size();
    }

    WriteLn " = %_", sum;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clausification:


void Pdr2::addFrame()
{
    F.push();
    S.push();
    S.last().selectSolver(P.sat_solver);
    n2s.push();
    n2s.last().growTo(2);
    wasted_lits.push(0);
}


Lit Pdr2::clausify(uint d, GLit p, uint side)
{
    ZZ_PTimer_Scope(pdr2_clausify);
    if (!+n2s[d][side][p]){
        Vec<Pair<uint,GLit> > roots;
        roots.push(tuple(side, p));
        lutClausify(N, roots, false, S[d], n2s[d]);
    }

    return n2s[d][side][p];
}


Lit Pdr2::clausifyInit(GLit p)
{
    ZZ_PTimer_Scope(pdr2_clausify);
    if (!+n2z[0][p]){
        Vec<Pair<uint,GLit> > roots;
        roots.push(tuple(0, p));
        lutClausify(N, roots, true, Z, n2z);
    }

    return n2z[0][p];
}


void Pdr2::recycleSolver(uint d)
{
    //**/WriteLn "[RECYCLE]";
    ZZ_PTimer_Scope(pdr2_recycleSolver);
    S[d].clear();
    assert(n2s[d].size() == 2);
    n2s[d][0].clear();
    n2s[d][1].clear();
    wasted_lits[d] = 0;

    Vec<Lit> tmp;
    for (uint k = d; k < F.size(); k++){
        for (uint i = 0; i < F[k].size(); i++){
            for (uint j = 0; j < F[k][i].size(); j++)
                tmp.push(~clausify(d, F[k][i][j]));
            S[d].addClause(tmp);
            tmp.clear();
        }
    }
}


void Pdr2::storeModel(uint d)
{
    S[d].getModel(model);
    model_frame = d;
}


inline lbool Pdr2::lvalue(GLit w, uint side)
{
    Lit lit_w = n2s[model_frame][side][w];
    if (!+lit_w)
        return l_Undef;
    else
        return model[lit_w.id] ^ lit_w.sign;
}


inline bool Pdr2::value(GLit w, uint side)
{
    lbool v = lvalue(w, side); assert(v != l_Undef);
    return v == l_True;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Model extraction and weakening:


// PROFILING:
uint64 n_pobs = 0;
uint64 n_justs = 0;
uint64 total_just = 0;
uint64 n_cuts = 0;
uint64 total_cut = 0;
uint64 n_tsims = 0;
uint64 total_tsim = 0;
uint64 n_cubes = 0;
uint64 total_cube = 0;
uint64 n_recycle_sat = 0;


ZZ_Finalizer(stats, 0){
    if (n_justs == 0) return;

    NewLine;
    WriteLn "number of original POBs produced: %_", n_pobs;
    WriteLn "average justification/COI size..: %.2f", double(total_just) / n_justs;
    WriteLn "average cut size................: %.2f", double(total_cut)  / n_cuts;
    WriteLn "average ternary simulation size.: %.2f", double(total_tsim) / n_tsims;
    NewLine;
    WriteLn "number of learned cubes.........: %_", n_cubes;
    WriteLn "average learned cube size.......: %.2f", double(total_cube) / n_cubes;
    NewLine;
    WriteLn "number of SAT solver recycles...: %_", n_recycle_sat;
}
// END PROFILING


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
void Pdr2::justify(const Cube& c, /*outputs:*/WSeen seen[2], Vec<GLit>& sources, WZet& sinks)
{
    ZZ_PTimer_Scope(pdr2_justify);

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
            if (P.pob_cone == Params_Pdr2::COI){
                // Get cone-of-influence rather than a minimal justification:
                For_Inputs(w, v)
                    Push(v, d);

            }else{
                // Follow one or all minimal justification:
                uint cl = attr_Npn4(w).cl;
                uint a = 0;
                For_Inputs(w, v)
                    if (value(v, d))
                        a |= 1u << Iter_Var(v);

                uint just = npn4_just[cl][a];       // -- look up all minimal justifications

                if (P.pob_cone == Params_Pdr2::PSEUDO_JUST){
                    // Follow all:
                    uint all_just = 0;
                    while (just != 0){
                        all_just |= just;
                        just >>= 4; }

                    for (uint i = 0; i < 4; i++){
                        if (all_just & (1u << i))
                            Push(w[i], d);
                    }

                }else{ assert(P.pob_cone == Params_Pdr2::JUST);
                    // Follow one:
                    if (P.just_strat == Params_Pdr2::ACTIVITY){
                        // Pick most active justification:
                        uchar  best_just = 0;
                        double best_act = -1;
                        while (just != 0){
                            double act = 0;
                            for (uint i = 0; i < 4; i++){
                                if (just & (1u << i))
                                    act += activity[w[i]];  // <<== read out value from current model if activity with sign
                            }
                            if (newMax(best_act, act))
                                best_just = just & 15u;

                            just >>= 4;
                        }

                        for (uint i = 0; i < 4; i++){
                            if (best_just & (1u << i))
                                Push(w[i], d);
                        }

                    }else if (P.just_strat == Params_Pdr2::LOWEST_LEVEL){
                        // Pick justification of lowest level:
                        uchar best_just = 0;
                        uint  best_level = UINT_MAX;
                        while (just != 0){
                            uint level_sum = 0;
                            for (uint i = 0; i < 4; i++){
                                if (just & (1u << i))
                                    level_sum += level[w[i]];
                            }
                            if (newMin(best_level, level_sum))
                                best_just = just & 15u;

                            just >>= 4;
                        }

                        for (uint i = 0; i < 4; i++){
                            if (best_just & (1u << i))
                                Push(w[i], d);
                        }

                    }else{ assert(P.just_strat == Params_Pdr2::FIRST);
                        // Pick first justification:
                        for (uint i = 0; i < 4; i++){
                            if (just & (1u << i))
                                Push(w[i], d);
                        }
                    }
                }
            }

        }else
            assert(false);
    }

    sortUnique(sources);
    /*stat*/n_justs++; total_just += sources.size();

    // Add sign to sources:
    for (uint i = 0; i < sources.size(); i++)
        if (!value(sources[i]))
            sources[i] = ~sources[i];

    #undef Push
}


//=================================================================================================
// -- Min-cut:


// 'sub' denotes the sub-graph of 'N' to compute min-cut for. It will be updated to end at the
// selected cut (with all nodes below being removed from the set).
Cube Pdr2::minCut(WSeen& sub, const Vec<GLit>& sources, const WZet& sinks)
{
    ZZ_PTimer_Scope(pdr2_minCut);

    Get_Pob(N, fanouts);

    WMap<uchar>    used;    // -- edges used: 0=unused, 1-4=input 0-3 used (subtract one)
    WTmpMapS<GLit> from;    // }- positive literal marks the OUTPUT of a gate, negative INPUT
    Vec<GLit>      Q;       // }

    Vec<GLit> cut;
    uint q_resume = 0;

    for (;;){
        // Find augmenting path (quick, incomplete):
/*tmp*/ZZ_PTimer_Begin(aug_quick);
        for (uint q = q_resume; q < sources.size(); q++){
            Wire w = +sources[q] + N; assert(sub.has(w));
            if (used[w] == 255) continue;

            from(~w) = glit_MAX;       // -- marks a source
            from(w) = ~w;

            for(;;){
                Fanouts fs = fanouts[w];
                for (uint i = 0; i < fs.size(); i++){
                    Wire v = +fs[i];
                    if (!sub.has(v))   continue;
                    if (from[~v])      continue;     // -- avoid cycles through flops
                    if (used[~v] != 0) continue;

                    from(~v) = w;
                    from(v) = ~v;

                    if (type(v) == gate_SO){
                        // Last push was SINK:
                        assert(sinks.has(v));
                        Q.push(~v);
                        q_resume = q;
/*tmp*/ZZ_PTimer_End(aug_quick);
                        goto Break; }

                    w = v;
                    goto Found;
                }
                break;
              Found:;
            }
        }
        from.clear();
/*tmp*/ZZ_PTimer_End(aug_quick);

        // Find augmenting path (BFS):
/*tmp*/ZZ_PTimer_Begin(aug_bfs);
        for (uint i = 0; i < sources.size(); i++){
            Wire w = +sources[i] + N;
            from(~w) = glit_MAX;       // -- marks a source
            Q.push(~w);
        }

        for (uint q = 0; q < Q.size(); q++){
            Wire w = Q[q] + N; assert(sub.has(w));

            if (!sign(w)){
                // Output side:
                Fanouts fs = fanouts[w];
                for (uint i = 0; i < fs.size(); i++){
                    Wire v = +fs[i];
                    if (!sub.has(v)) continue;

                    if (!from[~v]){
                        if (used[~v] == 0 || +v[used[~v]-1] != w){
                            // Fantout Push:
                            from(~v) = w;
                            Q.push(~v);

                            if (type(v) == gate_SO){
                                // Last push was SINK:
                                assert(sinks.has(v));
/*tmp*/ZZ_PTimer_End(aug_bfs);
                                goto Break; }
                        }
                    }

                    if (used[w] != 0){
                        if (!from[~w]){
                            // Internal Back-Push:
                            from(~w) = w;
                            Q.push(~w); }
                    }
                }

            }else{
                // Input side:
                if (used[w] == 0){                              // -- note: w is signed
                    // Continue to output side:
                    assert(type(w) != gate_SO);
                    if (!from[+w]){
                        // Internal Push:
                        from(+w) = w;
                        Q.push(+w); }

                }else if (used[w] != 255){                      // -- 255 marks a used source
                    // Have incoming arc, must follow it backwards:
                    Wire v = +w[used[w]-1];
                    if (!from[v]){
                        // Backward Push:
                        from(v) = w;
                        Q.push(v); }
                }
            }
        }
/*tmp*/ZZ_PTimer_End(aug_bfs);
      Break:;

/*tmp*/ZZ_PTimer_Begin(aug_update);
        Wire w = Q.last() + N;
        if (sinks.has(w)){
            // Augmenting path found -- update 'used':
            assert(type(w) == gate_SO); assert(sinks.has(w));
            for(;;){
                Wire v = from[w] + N;
                if (+v == glit_MAX){
                    used(w) = 255;                              // -- mark source as used
                    break; }

                assert(sign(w) != sign(v));
                if (!sign(w)){
                    if (+w != +v){
                        assert(used[v] != 0); assert(+v[used[v] - 1] == w);
                        used(v) = 0; }
                }else{
                    if (+w != +v){
                        uint k = pinNumPlusOne(w, v); assert(k != 0);
                        used(w) = k; }
                }

                w = v;
            }
            assert(has(sources, w) || has(sources, ~w));

        }else{
            // No more augmenting paths, extract min-cut from max-flow:
            sort(Q);
            if (sign(Q[0]))
                cut.push(+Q[0]);
            for (uint i = 0; i < Q.size()-1; i++)
                if (sign(Q[i+1]) && Q[i] != ~Q[i+1])
                    cut.push(+Q[i+1]);

            // Update support cone:
            for (uint i = 0; i < Q.size(); i++)
                if (!sign(Q[i]))
                    sub.exclude(Q[i] + N);
            break;
        }

        from.clear();
        Q.clear();
    }
/*tmp*/ZZ_PTimer_End(aug_update);

    assert(cut.size() <= sources.size());

    // Return cut (with signs):
    Vec<GLit> result;
    for (uint i = 0; i < cut.size(); i++){
        Wire w = cut[i] + N; assert(!sign(w));
        result.push(w ^ !value(w));
    }
    //**/Dump(result);

    if (P.tweak_cut){
        // Tweak cut using activity:
        for(;;){
            bool changed = false;
            for (uint i = 0; i < result.size(); i++){
                Wire w = result[i] + N;
                Fanouts fs = fanouts[w];

                uint count = 0;
                Wire v;
                for (uint j = 0; j < fs.size(); j++){
                    if (sub.has(fs[j]) && !has(cut, +fs[j])){
                        count++;
                        v = +fs[j];
                    }
                }

                if (count == 1 && type(v) == gate_Npn4 && activity(v) > activity(w)){
                    result[i] = v ^ !value(v);
                    changed = true;
                    sub.exclude(w);     // -- keep support cone up-to-date
                }
            }
            if (!changed) break;
        }
    }

    /*stat*/n_cuts++; total_cut += result.size();
    return Cube(result);
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
        //*Tr*/if (!had) WriteLn "-- enqueued: %_ @%_", +w, d;
    }
}


// NOTE! Will remove sources from 'seen[0]'.
Cube Pdr2::weakenBySim(WSeen seen[2], Cube src, Cube snk)
{
    ZZ_PTimer_Scope(pdr2_weakenBySim);

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
    ZZ_PTimer_Begin(pdr2_weakenBySim_init);
    WMap<lbool> sim[2];
    PropQ Q(N, level);

    For_Gates(N, w){
        if (seen[0].has(w)) sim[0](w) = lvalue(w, 0);
        if (seen[1].has(w)) sim[1](w) = lvalue(w, 1);
    }

    for (uint i = 0; i < src.size(); i++)
        seen[0].exclude(src[i] + N);                // -- this will prevent update of sources

    //*Tr*/N.write("N.gig"); WriteLn "Wrote: N.gig";
    //*Tr*/Dump(src);
    //*Tr*/Dump(snk);
    //*Tr*/Write "seen[0]:"; For_Gates(N, w) if (seen[0].has(w)) Write " %_", w; NewLine;
    //*Tr*/Write "seen[1]:"; For_Gates(N, w) if (seen[1].has(w)) Write " %_", w; NewLine;

    ZZ_PTimer_End(pdr2_weakenBySim_init);

    // Try to put as many X as possible into 'src':
    WZet snk_set;
    for (uint i = 0; i < snk.size(); i++) snk_set.add(snk[i] + N);

    Vec<Trip<uint,GLit,lbool> > undo;   // -- triplets '(frame, gate, value)'
    Vec<GLit> cube(copy_, src);

    if (P.randomize)
        shuffle(seed, cube);
    else if (P.use_activity)
//        sobSort((ordReverse(sob(cube, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N))))));
        sobSort(sob(cube, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N)))));

    //*Tr*/WriteLn "----";
    for (uint i = 0; i < cube.size(); i++){
        Wire w0 = cube[i] + N;

        if (type(w0) == gate_PI && !reach.has(w0)){
            cube[i] = glit_NULL;        // -- we assume initial state does not depend on PIs
            continue; }

        if (isInitial(cube, w0))
            continue;

        //*Tr*/WriteLn "\a*== seed:\a* %_ @0 = %_ -> ?", w0, sim[0][w0];
        undo.push(tuple(0, w0, sim[0][w0]));
        sim[0](w0) = l_Undef;
        enqueueFanouts(fanouts[w0], 0, Q, seen);

        while (Q.size() > 0){                           // -- propagate
            uint d; Wire w; l_tuple(d, w) = Q.pop();
            if (sim[d][w] == l_Undef) continue;

            undo.push(tuple(d, w, sim[d][w]));
            evalGate(w, d, sim);
            //*Tr*/WriteLn "-- eval: %_ @%_ = %_ (was %_)", w, d, sim[d][w], undo.last().trd;
            if (sim[d][w] == l_Undef && snk_set.has(w)){
                // Undo:
                for (uint j = undo.size(); j > 0;) j--,
                    sim[undo[j].fst](undo[j].snd + N) = undo[j].trd;
                //*Tr*/WriteLn "-->> kept %_", w0;
                goto KeepLit;
            }
            if (sim[d][w] != undo.last().trd)
                enqueueFanouts(fanouts[w], d, Q, seen);
        }
        //*Tr*/WriteLn "-->> removed %_", w0;
        cube[i] = glit_NULL;

      KeepLit:
        Q.clear();
        undo.clear();
    }

    filterOut(cube, isNull<GLit>);

    /*stat*/n_tsims++; total_tsim += src.size();
    return Cube(cube);
}


//=================================================================================================
// -- Proof-obligation orchestration:


// Extract model in terms of a left frame variables, enough to justify 'c' in the right frame.
Cube Pdr2::extractModel(Cube c)
{
    ZZ_PTimer_Scope(pdr2_extractModel);
    /*stat*/n_pobs++;

    // Justify:
    WSeen     seen[2];
    Vec<GLit> sources;
    WZet      sinks;

    if (P.pob_rotate){
        P.pob_internals ^= 1;
        if (P.pob_internals == 0)
            P.pob_cone = (P.pob_cone == Params_Pdr2::JUST) ? Params_Pdr2::PSEUDO_JUST : Params_Pdr2::JUST;
    }

    justify(c, seen, sources, sinks);
    Cube cut = P.pob_internals ? minCut(seen[0], sources, sinks) : Cube(sources);
    return P.pob_weaken ? weakenBySim(seen, cut, c) : cut;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Major methods:


void Pdr2::addCube(TCube s)
{
    ZZ_PTimer_Scope(pdr2_addCube);

    // Remove (some) subsumed cubes:
    //**/WriteLn "addCube(\a/%_\a/)", s;
    for (uint d = 0; d <= s.frame; d++){
        for (uint i = 0; i < F[d].size();){
            if (subsumes(s.cube, F[d][i])){
                /**/assert(s.cube != F[d][i] || s.frame > d);
                F[d][i] = F[d].last();
                F[d].pop();
            }else
                i++;
        }
    }

    // Add cube to trace:
    F[s.frame].push(s.cube);
    for (uint i = 0; i < s.cube.size(); i++)
        activity(s.cube[i] + N) += 1.0;

    // Add cube to SAT solvers:
    assert(s.size() != 0);
    Vec<Lit> tmp;
    for (uint d = 0; d <= s.frame; d++){        // <<== could avoid adding to S[0], but must take care of unit clauses in 'run()'
        for (uint i = 0; i < s.size(); i++)
            tmp.push(~clausify(d, s[i]));
        S[d].addClause(tmp);
        tmp.clear();
    }

    // <<== syntactic subsumption (when cubes just move forward)
    // <<== bump activities?
    // <<== subsumption checking via occurrence lists
}


bool Pdr2::propagate()
{
    ZZ_PTimer_Scope(pdr2_propagate);
    //*T*/WriteLn "\a*\a/==== PROPAGATE\a0";
    addFrame();

    for (uint k = P.prop_init ? 0 : 1; k < F.size()-1; k++){
        Vec<Cube> cubes(copy_, F[k]);
        for (uint i = 0; i < cubes.size(); i++){
            if (has(F[k], cubes[i])){       // <<== need to speed this up!
                TCube s = solveRel(TCube(cubes[i], k+1), sr_NoInduct);
                // <<== if s is unchanged, just move it to F[k+1]
                //*T*/WriteLn "--> propagate: %_", s;
                if (s) addCube(s);
            }
        }

        if (k > 0 && F[k].size() == 0)
            return true;
    }

    //*T*/WriteLn "\a*\a/==== [DONE]\a0";
    return false;
}


// Check if timed cube 's' is already blocked by the trail.
bool Pdr2::isBlocked(TCube s)
{
    ZZ_PTimer_Scope(pdr2_isBlocked);

    Cube c = s.cube;
    uint k = s.frame;
    Vec<Lit> assumps;

    // Assume cube:
    for (uint i = 0; i < c.size(); i++)
        assumps.push(clausify(k, c[i]));

    lbool result = S[k].solve(assumps); assert(result != l_Undef);
    //**/WriteLn "isBlocked(%_) = %_", s, (result == l_False);
    return result == l_False;
}


// Check if 'c' is a cube consistent with the initial states. The cube 'c' may contain 'glit_NULL's
// (which are ignored)
template<class GVec>
inline bool Pdr2::isInitial(const GVec& c, GLit except)
{
    ZZ_PTimer_Scope(pdr2_isInitial);

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
TCube Pdr2::solveRel(TCube s, uint params)
{
    ZZ_PTimer_Scope(pdr2_solveRel);

    //**/WriteLn "solveRel(%_)", s;
    Cube c = s.cube;
    uint k = s.frame - 1;
    Vec<Lit> assumps;

#if 1
    if (wasted_lits[k] > 300){
#else
    if (wasted_lits[k] * 2 > S[k].nVars() + 300){
#endif
        /*stat*/n_recycle_sat++;
        recycleSolver(k); }

    // Inductive assumption:
    Lit tmp_act;
    if (!(params & sr_NoInduct)){
        tmp_act = S[k].addLit();
        if (P.recycling == Params_Pdr2::SOLVER)
            wasted_lits[k]++;
        Vec<Lit> tmp;
        for (uint i = 0; i < c.size(); i++)
            tmp.push(clausify(k, ~c[i]));
        tmp.push(~tmp_act);
        S[k].addClause(tmp);
        assumps.push(tmp_act);
    }

    // Assume 's' at state outputs:
    for (uint i = 0; i < c.size(); i++)
        assumps.push(clausify(k, c[i], 1));

    // Solve:
    lbool result = S[k].solve(assumps); assert(result != l_Undef);

    TCube ret;
    if (result == l_False){ // -- UNSAT
        // Figure out subset of 's' enough for UNSAT:
        Vec<Lit> confl;
        S[k].getConflict(confl);

        // Get UNSAT core:
        Vec<GLit> z(copy_, c);
        for (uint i = 0; i < z.size(); i++){
            Wire w = z[i] + N;
            if (!has(confl, clausify(k, w, 1))){
                z[i] = z.last();
                z.pop();
                if (isInitial(z)){
                    z.push(z[i]);
                    z[i] = w;
                }else
                    i--;
            }
        }
        ret = TCube(Cube(z), k+1);

    }else{ // -- SAT
        storeModel(k);
        ret = (params & sr_ExtractModel) ? TCube(extractModel(c)) : TCube();
    }

    // Recycling of temporary clause:
    if (tmp_act){
#if 1
        if (P.recycling != Params_Pdr2::NONE)
#else
        if (P.recycling == Params_Pdr2::VARS)
#endif
            S[k].recycleLit(~tmp_act);
        else
            S[k].addClause(~tmp_act);
    }

    return ret;
}


TCube Pdr2::generalize(TCube s)
{
    ZZ_PTimer_Scope(pdr2_generlize);

    Vec<GLit> cube(copy_, s.cube);
    if (P.randomize)
        shuffle(seed, cube);
    else if (P.use_activity)
        sobSort(sob(cube, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N)))));

    if (P.gen_orbits == 1){
        // Shrink 's' (single orbit):
        for (uint i = 0; i < cube.size(); i++){
            if (has(s.cube, cube[i])){
                TCube new_s(s.cube - cube[i], s.frame);
                if (!isInitial(new_s.cube))
                    condAssign(s, solveRel(new_s));
            }
        }

    }else{ assert(P.gen_orbits == 2);
        // Shrink 's' (two orbits):
        uint last_elimed = 0;
        uint i = 0;
        for (uint n = 0; n < 2*cube.size(); n++){
            if (has(s.cube, cube[i])){
                TCube new_s(s.cube - cube[i], s.frame);
                if (!isInitial(new_s.cube)){
                    if (condAssign(s, solveRel(new_s)))
                        last_elimed = i;
                }
            }

            i = (i+1) % cube.size();
            if (i == last_elimed)
                break;
        }
    }

    return s;
}


bool Pdr2::blockCube(TCube s0)
{
    ZZ_PTimer_Scope(pdr2_blockCube);

    KeyHeap<ProofObl> Q;
    uint prioC = UINT_MAX;
    Q.add(ProofObl(s0, prioC--));

    uint n_restarts = 0;
    uint restartC = pow(2, (double)lubyLog(n_restarts)) * 100; assert(restartC > 0);

    uint iter = 0;
    while (Q.size() > 0){
        // Pop proof-obligation:
        ProofObl po = Q.pop();
        TCube    s  = po->tcube;

        if (!isBlocked(s)){
            if (s.frame == 0){
                // Found counterexample:
                if (!P.check_klive)
                    extractCex(po);
                return false;
            }

            assert(!isInitial(s.cube));
            TCube z;
            z = solveRel(s, sr_ExtractModel);
            if (z){
                // Cube 's' was blocked by image of predecessor:
                z = generalize(z);
                while (z.frame+1 < F.size() && condAssign(z, solveRel(next(z))));   // -- push 'z' forward

                addCube(z);     // -- add blocking cube
                /*stat*/n_cubes++; total_cube += z.cube.size();

                s.frame = z.frame + 1;
                if (s.frame < F.size()){
                    Q.add(ProofObl(s, prioC--, po->next)); }

                if (P.restarts){
                    // Restarts:
                    restartC--;
                    if (restartC == 0){
                        showProgress((FMT "%<5%_", Q.size()));
                        n_restarts++;
                        restartC = pow(2, (double)lubyLog(n_restarts)) * 100; assert(restartC > 0);

                        while (Q.size() > 1)
                            Q.pop();
                    }
                }

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


bool Pdr2::run()
{
    // Add a flop on top of the property:
    Get_Pob(N, properties);
    Get_Pob(N, flop_init);
    assert(properties.size() == 1);
    Wire w_prop_in = N.add(SO_(), properties[0][0] ^ sign(properties[0]));
    Wire w_prop = N.add(Flop_(), w_prop_in);
    flop_init(w_prop) = l_True;

    removeAllUnreach(N);

    // Add pobs:
    Add_Pob(N, fanouts);
    Add_Pob(N, up_order);

#if 1   /*DEBUG*/
    For_Gates(N, w)
        activity(w) = fanouts[w].size();
    //**/For_Gatetype(N, gate_Flop, w)
    //**/    WriteLn "ff[%_]: %_ fanouts", attr_Flop(w).number, fanouts[w].size();
#endif  /*END DEBUG*/

    // Levelize circuit:
    level.nil = 0;
    For_UpOrder(N, w){
        if (type(w) == gate_Flop) continue;
        For_Inputs(w, v)
            newMax(level(w), level[v] + 1);
    }

    // Add (negation of) initial state to 'F[0]':
    assert(F.size() == 0);
    addFrame();

    For_Gatetype(N, gate_Flop, w)
        if (flop_init[w] != l_Undef)
            addCube(TCube(Cube(w ^ (flop_init[w] == l_True)), 0));

    //*T*/N.write("L.gig"); WriteLn "Wrote: L.gig";
    //*T*/Dump(w_prop);

    // Keep proving property deeper and deeper until invariant or CEX is found:
    uint klive_depth = 0;
    for(;;){
        if (F.size() > 1)
            WriteLn "-------------------- Depth %_ --------------------", F.size()-2;

        if (!blockCube(TCube(Cube(~w_prop), F.size()-1))){
            if (P.check_klive){
                Get_Pob(N, flop_init);
                Wire b_in = N.add(SO_());
                Wire b = N.add(Flop_(), b_in);
                flop_init(b) = l_False;
//                b_in     .set(0, N.add(Npn4_(npn4_cl_OR2), Wire_NULL, Wire_NULL,  b, ~w_prop_in[0]));
//                w_prop_in.set(0, N.add(Npn4_(npn4_cl_OR2), Wire_NULL, Wire_NULL, ~b,  w_prop_in[0]));
                b_in     .set(0, N.add(Npn4_(npn4_cl_OR2),  b, ~w_prop_in[0], Wire_NULL, Wire_NULL));
                w_prop_in.set(0, N.add(Npn4_(npn4_cl_OR2), ~b,  w_prop_in[0], Wire_NULL, Wire_NULL));

                addCube(TCube(Cube(b), 0));

                Get_Pob(N, fanouts);
                fanouts.recompute();

                Get_Pob(N, up_order);
                up_order.recompute();

#if 1
                for (uint d = 0; d < F.size(); d++)
                    recycleSolver(d);
#else
                for (uint d = 0; d < F.size(); d++){
                    n2s[d][0](w_prop_in) = lit_Undef;
                    n2s[d][1](w_prop) = lit_Undef;
                    clausify(d, w_prop, 1);
                }
#endif

                WriteLn "           \a*\a/==>> increasing\a/ k \a/to\a/ %_ \a/<<==\a/\a*", ++klive_depth;

                    //        Wire b = N.add(Flop_());
                    //        flop_init(b) = l_False;
                    //        b.set(0, s_Or(b, ts[i-1]));
                    //
                    //        ts(i) = s_And(ts[i-1], b);
            }else{
                showProgress("final");
                WriteLn "Counterexample of depth %_ found.", cex.depth();
                return false;
            }

        }else{
            showProgress("block");
            if (propagate()){
                showProgress("final");
                WriteLn "Inductive invariant found (%_ clauses).", invariantSize();
                if (N_invar) storeInvariant(N_invar);
                return true;
            }
            showProgress("prop.");
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Result collection:


void Pdr2::extractCex(ProofObl pobl)
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
    uint d = F.size();
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

        lbool result = S[d].solve(assumps);
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

    //**/Dump(cex.flops[0].base());
    //**/for (uint i = 0; i < cex.inputs.size(); i++)
    //**/Dump(i, cex.inputs[i].base());
    //**/N.write("N.gig"); WriteLn "Wrote: N.gig";
}


uint Pdr2::invariantSize()
{
    uint start = UINT_MAX;
    for (uint d = 1; d < F.size(); d++){
        if (F[d].size() == 0){
            start = d;
            break;
        }
    }
    assert(start != UINT_MAX);

    uint size = 0;
    for (uint d = start; d < F.size(); d++)
        size += F[d].size();

    return size;
}


#if 1   /*DEBUG*/
template<class T>
uind firstEmpty(const Vec<T>& v, uint start = 0)
{
    for (uind i = start; i < v.size(); i++)
        if (v[i].size() == 0)
            return i;
    return UIND_MAX;
}
#endif  /*END DEBUG*/


// <<== TEMPORARY! Only works for flop-based cubes.
void Pdr2::storeInvariant(NetlistRef N_invar)
{
    /**/return;     // <<== later
#if 0   /*DEBUG*/
    WriteLn "\a*\a_Invariant\a_\a*";

    for (uint d = firstEmpty(F, 1); d < F.size(); d++){
        for (uint j = 0; j < F[d].size(); j++){
            Cube c = F[d][j];
            Write "{";
            for (uint i = 0; i < c.size(); i++){
                Wire w = N[c[i]];
                Write "%C%Cs%_", (i > 0)?' ':0, sign(w)?0:'~', attr_Flop(w).number;
            }
            WriteLn "}";
        }
    }
    NewLine;
#endif  /*END DEBUG*/

    N_invar.clear();
    Add_Pob0(N_invar, strash);

    Vec<Wire> flops;
    Wire      w_conj = N_invar.True();
    bool      adding = false;

    for (uint d = 1; d < F.size(); d++){
        if (!adding){
            if (F[d].size() == 0)
                adding = true;
        }else{
            for (uint i = 0; i < F[d].size(); i++){
                Cube c = F[d][i];

                Wire w_disj = ~N_invar.True();
                for (uint j = 0; j < c.size(); j++){
                    int num = attr_Flop(N[c[j]]).number;
                    if (num == num_NULL){
                        if (sign(c[j])) goto Skip;
                        else            continue;
                    }
                    if (flops(num, Wire_NULL) == Wire_NULL)
                        flops[num] = N_invar.add(Flop_(num));
                    w_disj = s_Or(w_disj, ~flops[num] ^ c[j].sign);
                }
                w_conj = s_And(w_conj, w_disj);
              Skip:;
            }
        }
    }
    assert(adding);

#if 0
    Get_Pob(N, init_bad);
    Wire w_prop = copyFormula(~init_bad[1][0], N_invar);
    w_conj = s_And(w_conj, w_prop);
#endif

    N_invar.add(PO_(), w_conj);
    removeUnreach(N_invar);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// <<== report 'bug_free_depth' back to main() for ABC consumption...

// Returns TRUE if properties hold, FALSE if CEX is found.
bool pdr2( NetlistRef          N,
           const Vec<Wire>&    props,
           const Params_Pdr2&  P,
           Cex*                cex,
           NetlistRef          N_invar
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
#if 0
            Get_Pob(N, properties);
            Add_Pob2(L, properties, new_properties);
            for (uint i = 0; i < properties.size(); i++)
                new_properties.push(n2l[properties[i]] + L);
#else
            Get_Pob(M, properties);
            Add_Pob2(L, properties, new_properties);
            for (uint i = 0; i < properties.size(); i++)
                new_properties.push(m2l[properties[i]] + L);
#endif
        }

        // Split flops:
        splitFlops(L);
    }

    // Run PDR algorithm:
    CCex ccex;
    Pdr2 pdr2(L, P, ccex, N_invar);
    bool ret = pdr2.run();
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


void addCli_Pdr2(CLI& cli)
{
    // GLOBAL:
    Params_Pdr2 P;  // -- get default values.
    String recycling_types   = "{none, solver, vars}";
    String recycling_default = select(recycling_types, P.recycling);
    String pob_cone_types   = "{just, pjust, coi}";
    String pob_cone_default = select(pob_cone_types, P.pob_cone);
    String just_strat_types   = "{first, level, act}";
    String just_strat_default = select(just_strat_types, P.just_strat);
    String sat_types   = "{zz, msc, abc}";
    String sat_default = select(sat_types, (P.sat_solver == sat_Zz) ? 0 : (P.sat_solver == sat_Msc) ? 1 : 2);

    cli.add("recycle"  , recycling_types , recycling_default , "SAT solver recycling strategy.");
    cli.add("cone"     , pob_cone_types  , pob_cone_default  , "First POB phase: justification cone to consider.");
    cli.add("intern"   , "bool"          , B(P.pob_internals), "Second POB phase: do min-cut to select internal points (otherwise state-vars).");
    cli.add("weaken"   , "bool"          , B(P.pob_weaken)   , "Third POB phase: weaken cut by ternary simulation.");
    cli.add("rotate"   , "bool"          , B(P.pob_rotate)   , "Shift between POB generation strategies.");
    cli.add("just"     , just_strat_types, just_strat_default, "Justification strategy (if '-cone=just' is selected).");
    cli.add("act"      , "bool"          , B(P.use_activity) , "Use activity based heuristic in POB phase and generalization.");
    cli.add("rand"     , "bool"          , B(P.randomize)    , "Instead of activity, just shuffle the literals before POB/generalization.");
    cli.add("restarts" , "bool"          , B(P.restarts)     , "Clear queue of POBs periodically.");
    cli.add("orbits"   , "int[1:2]"      , S(P.gen_orbits)   , "Orbits of literal removal in generalization.");
    cli.add("tweak"    , "bool"          , B(P.tweak_cut)    , "Do a (weak) post-processing of min-cut to select more active variables.");
    cli.add("sat"      , sat_types       , sat_default       , "SAT-solver to use.");
    cli.add("prop-init", "bool"          , B(P.prop_init)    , "Propagate initial state unit cubes from F[0].");
}


void setParams(const CLI& cli, Params_Pdr2& P)
{
    typedef Params_Pdr2::Recycling RC;
    typedef Params_Pdr2::PobCone   PC;
    typedef Params_Pdr2::JustStrat JS;

    P.recycling   =(RC)cli.get("recycle").enum_val;
    P.pob_cone    =(PC)cli.get("cone").enum_val;
    P.pob_internals  = cli.get("intern").bool_val;
    P.pob_weaken     = cli.get("weaken").bool_val;
    P.pob_rotate     = cli.get("rotate").bool_val;
    P.just_strat  =(JS)cli.get("just").enum_val;
    P.use_activity   = cli.get("act").bool_val;
    P.randomize      = cli.get("rand").bool_val;
    P.restarts       = cli.get("restarts").bool_val;
    P.gen_orbits     = cli.get("orbits").int_val;
    P.tweak_cut      = cli.get("tweak").bool_val;
    P.prop_init      = cli.get("prop-init").bool_val;

    P.sat_solver = (cli.get("sat").enum_val == 0) ? sat_Zz :
                   (cli.get("sat").enum_val == 1) ? sat_Msc :
                   (cli.get("sat").enum_val == 2) ? sat_Abc : (assert(false), sat_NULL);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
Very different profile statistics:

    510.aig
    GCTWRAPPER3_prop8.aig
*/
