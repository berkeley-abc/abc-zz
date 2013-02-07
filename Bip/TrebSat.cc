//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TrebSat.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT solving abstraction for the PDR enginge 'Treb'.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "TrebSat.hh"
#include "ZZ_MetaSat.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ/Generics/Sort.hh"

//#define WEAKEN_BY_SAT
//#define SAT_SNAPSHOT

//#define LAZY_CUBES

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


ZZ_PTimer_Add(treb_sat);
ZZ_PTimer_Add(treb_unsat);
ZZ_PTimer_Add(treb_sim);
ZZ_PTimer_Add(treb_newsim);
ZZ_PTimer_Add(treb_multi_choose);
ZZ_PTimer_Add(treb_weaken_sat);
ZZ_PTimer_Add(treb_randvar);
ZZ_PTimer_Add(treb_lazy_validate);
ZZ_PTimer_Add(treb_lazy_refine);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// TrebSat_Common:


struct TrebSat_Common : TrebSat {
    //  External references:
    NetlistRef                  N;
    const Vec<Vec<Cube> >&      F;
    const WMapS<float>&         activity;
    const Params_Treb&          P;

    //  Minor helpers:
    uint depth() const { return F.size() - 2; }

    //  Major helpers for derived classes:
    Cube weakenByJust(Cube s, Cube bad);
    Cube weakenBySim (Cube s, Cube bad, bool pre_weak_by_just);
    Cube weaken      (Cube s, Cube bad);

    // Constructor:
    TrebSat_Common(NetlistRef N_, const Vec<Vec<Cube> >& F_, const WMapS<float>& activity, const Params_Treb& P);

    //  Debug:
    FmtCube  fmt(Cube  c) const { return FmtCube (N, c); }
    FmtTCube fmt(TCube s) const { return FmtTCube(N, s); }

private:
    //  Internal temporaries:
    XSimulate   xsim;
    Cex         cex;

    uint64      seed;
};


TrebSat_Common::TrebSat_Common(NetlistRef N_, const Vec<Vec<Cube> >& F_, const WMapS<float>& activity_, const Params_Treb& P_) :
    N(N_),
    F(F_),
    activity(activity_),
    P(P_)
{
    xsim.init(N);
    cex.flops .push();
    cex.inputs.push();

    seed = P.seed;
}


//=================================================================================================
// -- Implementation:


struct WeakenByJust_SimData {
    uint val  : 1;
    uint just : 1;
    uint lev  : 30;
    WeakenByJust_SimData(uint val_ = 0, uint lev_ = 0) : val(val_), just(false), lev(lev_) {}
};


Cube TrebSat_Common::weakenByJust(Cube c, Cube bad)
{
    ZZ_PTimer_Scope(treb_newsim);
    typedef WeakenByJust_SimData Sim;

    // Get topological order:
    Get_Pob(N, init_bad);
    Vec<gate_id> order;
    if (bad){
        Vec<Wire> sinks(bad.size());
        for (uint i = 0; i < bad.size(); i++)
            sinks[i] = N[bad[i]];
        upOrder(sinks, order);
    }else{
        Vec<Wire> sinks(1, init_bad[1]);
        upOrder(sinks, order);
    }

    // Expand 'c' to a map:
    WMap<Sim> sim;
    for (uint i = 0; i < c.size(); i++)
        sim(N[c[i]]) = Sim(!sign(N[c[i]]), 0);

    // Simulate:
    sim(N.True()) = Sim(true, 0);
    for (uint i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        switch (type(w)){
        case gate_PI:   break;
        case gate_Flop: break;
        case gate_And:{
            bool val = (sim[w[0]].val ^ sign(w[0])) & (sim[w[1]].val ^ sign(w[1]));
            uint lev = max_(sim[w[0]].lev, sim[w[1]].lev);
            sim(w) = Sim(val, lev + 1);
            break; }
        case gate_PO:{
            bool val = (sim[w[0]].val ^ sign(w[0]));
            uint lev = sim[w[0]].lev;
            sim(w) = Sim(val, lev + 1) ;
            break; }
        default: assert(false); }
    }

    // Validate 'bad':
    if (bad){
        for (uint i = 0; i < bad.size(); i++){
            Wire w = N[bad[i]];
            uint val = sim[w[0]].val ^ sign(w) ^ sign(w[0]);
            assert(val == 1);
        }
    }else{
        Wire w = init_bad[1];
        uint val = sim[w[0]].val ^ sign(w) ^ sign(w[0]);
        assert(val == 1);
    }

    // Initialize justification queue:
    #define Tag_Last sim(N[Q.last()]).just = true
    Vec<GLit> Q;
    if (bad){
        for (uint i = 0; i < bad.size(); i++)
            if (!sim[N[bad[i]][0]].just) Q.push(+N[bad[i]][0]), Tag_Last;
    }else
        Q.push(+init_bad[1][0]), Tag_Last;

    // Justify:
    Vec<GLit> result;
    while (Q.size() > 0){
        Wire w = N[Q.popC()];

        if (type(w) == gate_Flop){
            result.push(w.lit() ^ !sim[w].val);

        }else if (type(w) == gate_And){
            if (sim[w].val){
                // Both inputs have to be justified:
                if (!sim[w[0]].just) Q.push(+w[0]), Tag_Last;
                if (!sim[w[1]].just) Q.push(+w[1]), Tag_Last;
            }else{
                // Only one input has to be justified, pick the one with lowest level:
                bool may_just0 = !(sim[w[0]].val ^ sign(w[0]));
                bool may_just1 = !(sim[w[1]].val ^ sign(w[1]));
                bool just0 = sim[w[0]].just && may_just0;
                bool just1 = sim[w[1]].just && may_just1;
                if (!just0 && !just1){
                    if (may_just0 && (sim[w[0]].lev <= sim[w[1]].lev || !may_just1))
                        Q.push(+w[0]), Tag_Last;
                    else
                        assert(may_just1),
                        Q.push(+w[1]), Tag_Last;
                }
            }

        }else
            assert(type(w) != gate_PO);
    }
    #undef Tag_Last

    return Cube(result);
}


// NOTE! Cube 'c' contains both FFs and PIs, but the returned cube only has FFs. 'bad' can either
// be 'Cube_NULL' or a cube of FFs (which will be automatically translated to flop inputs).
Cube TrebSat_Common::weakenBySim(Cube c0, Cube bad, bool pre_weak_by_just)
{
    ZZ_PTimer_Scope(treb_sim);

    if (pre_weak_by_just){
        Cube c = weakenByJust(c0, bad);

        // Setup counterexample:
        cex.flops [0].clear();
        cex.inputs[0].clear();
        for (uint i = 0; i < c.size(); i++){
            Wire  w   = N[c[i]]; assert(type(w) == gate_Flop);
            lbool val = sign(w) ? l_False : l_True;
            cex.flops [0](w) = val;
        }
        for (uint i = 0; i < c0.size(); i++){
            Wire  w = N[c0[i]];
            if (type(w) == gate_PI){
                lbool val = sign(w) ? l_False : l_True;
                cex.inputs[0](w) = val;
            }
        }

    }else{
        Cube& c = c0;

        // Setup counterexample:
        cex.flops [0].clear();
        cex.inputs[0].clear();
        for (uint i = 0; i < c.size(); i++){
            Wire  w   = N[c[i]];
            lbool val = sign(w) ? l_False : l_True;
            if (type(w) == gate_Flop)        cex.flops [0](w) = val;
            else assert(type(w) == gate_PI), cex.inputs[0](w) = val;
        }
    }

    xsim.simulate(cex);

    // Do simulation:
    Get_Pob(N, init_bad);
    XSimAssign target = bad ? XSimAssign() : XSimAssign(0, init_bad[1], l_Undef);

    Vec<GLit> ffs;
    For_Gatetype(N, gate_Flop, w)
        ffs.push(w);
    if (P.seed != 0)
        shuffle(seed, ffs);
    if (P.use_activity)
        sobSort(ordStabilize(ordReverse(sob(ffs, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N)))))));

    for (uint j = 0; j < ffs.size(); j++){
        Wire w = N[ffs[j]];
        if (xsim[0][w] == l_Undef) continue;
        xsim.propagate(XSimAssign(0, w, l_Undef), /*abstr*/NULL, target);

        bool failed = false;
        if (!target){
            for (uind i = 0; i < bad.size(); i++){
                if (xsim[0][N[bad[i]][0]] == l_Undef){
                    failed = true;
                    break; }
            }
        }else
            failed = (xsim[0][init_bad[1]] == l_Undef);

        if (failed) xsim.propagateUndo();
        else        xsim.propagateCommit();
    }

#if 0
    /*EXPERIMENTAL*/
    uint orig_size = 0;
    For_Gatetype(xsim.N, gate_Flop, w){
        lbool val = xsim[0][w];
        if (val != l_Undef)
            orig_size++;
    }

    uint n_flips = 0;
    For_Gatetype(N, gate_PI, w){
        lbool curr = xsim[0][w];
        if (curr == l_Undef) continue;

        xsim.propagate(XSimAssign(0, w, l_Undef), /*abstr*/NULL, target);

        bool failed = false;
        if (!target){
            for (uind i = 0; i < bad.size(); i++){
                if (xsim[0][N[bad[i]][0]] == l_Undef){
                    failed = true;
                    break; }
            }
        }else
            failed = (xsim[0][init_bad[1]] == l_Undef);

        if (failed)
            xsim.propagateUndo();
        else{
            xsim.propagateCommit();
            xsim.propagate(XSimAssign(0, w, ~curr), /*abstr*/NULL, target);
            xsim.propagateCommit();
            n_flips++;
        }
    }

    if (n_flips > 0){
        for (uint j = ffs.size(); j > 0;){ j--;
            Wire w = N[ffs[j]];
            if (xsim[0][w] == l_Undef) continue;
            xsim.propagate(XSimAssign(0, w, l_Undef), /*abstr*/NULL, target);

            bool failed = false;
            if (!target){
                for (uind i = 0; i < bad.size(); i++){
                    if (xsim[0][N[bad[i]][0]] == l_Undef){
                        failed = true;
                        break; }
                }
            }else
                failed = (xsim[0][init_bad[1]] == l_Undef);

            if (failed) xsim.propagateUndo();
            else        xsim.propagateCommit();
        }
    }
    /*END*/
#endif

    // Collect result:
    Vec<GLit> z;
    For_Gatetype(xsim.N, gate_Flop, w){
        lbool val = xsim[0][w];
        if (val != l_Undef)
            z.push(w.lit() ^ (val == l_False));
    }

    //**/if (orig_size != z.size()) WriteLn "%_ -> %_", orig_size, z.size();

    return Cube(z);
}


Cube TrebSat_Common::weaken(Cube c, Cube bad)
{
    return (P.weaken == Params_Treb::NONE) ? c :
           (P.weaken == Params_Treb::SIM ) ? weakenBySim (c, bad, P.pre_weak) :
           /*otherwise*/                     weakenByJust(c, bad) ;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'TrebSat_MonoSat':


struct TrebSat_MonoSat : TrebSat_Common {
  //________________________________________
  //  State:

    WZet              keep;
    MultiSat          S;
    Clausify<MetaSat> C;
    WMap<Lit>         n2s;
    Vec<Lit>          act_;

#if defined(LAZY_CUBES)
    Set<Cube_Data*>   has_cube;     // -- has cube been added to 'S' yet? [EXPERIMENTAL]
    WMap<lbool>       model;
    uint              n_cubes_added;
#endif

    Cube              last_cube;
    Lit               tmp_act;
    uint              wasted_lits;

    uint64            seed;

  //________________________________________
  //  Local methods:

    TrebSat_MonoSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P);
    Lit  act(uint k);
    void recycle();
    Cube weakenBySat(Cube s, Cube bad);
    lbool S_solve(const Vec<Lit>& assumps, uint frame);

  //________________________________________
  //  Virtual interface:

    void  blockCubeInSolver(TCube s);
    void  recycleSolver();
    TCube solveRelative(TCube s, uint params = 0, Vec<Cube>* avoid = NULL);
    Cube  solveBad(uint k, bool restart);
    bool  isBlocked(TCube s);
    bool  isInitial(Cube c);
    void  extractCex(const Vec<Cube>& cs, Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff, const WZetL* abstr);

    void  solveMutual(const Vec<Cube>& bads, uint frame, /*out*/Vec<Cube>& blocked, /*out*/Vec<Cube>& preds){}
};


//=================================================================================================
// -- Lazy cubes:


lbool TrebSat_MonoSat::S_solve(const Vec<Lit>& assumps, uint frame)
{
#if !defined(LAZY_CUBES)
    return S.solve(assumps);

#else
    for(;;){
        ZZ_PTimer_Mark(last_solve);
        lbool result = S.solve(assumps);
        if (result == l_False){
            return l_False; }

        {
            ZZ_PTimer_Scope(treb_lazy_validate);

            For_Gatetype(N, gate_Flop, w){
                Lit p = n2s[w];
                model(w) = p ? (p.id < S.nVars() ? S.value(p) : l_Undef) : l_Undef;
            }

            /**/bool retry = false;
            for (uind d = F.size(); d > frame;){ d--;
                for (uint i = 0; i < F[d].size(); i++){
                    const Cube& c = F[d][i];
                    for (uint j = 0; j < c.size(); j++){
                        if ((model[c[j] + N] ^ sign(c[j])) == l_False)
                            goto Satisfied;
                    }
                    // Found unsatisfied clause/cube; add it:
                    {
                        Vec<Lit> ps;
                        if (d != F.size() - 1){
                            ps.push(~act(d));
                            assert(has(assumps, act(d)));
                        }

                        for (uint i = 0; i < c.size(); i++)
                            ps.push(C.clausify(~c[i] + N));
                        S.addClause(ps);
                        n_cubes_added++;

                        ZZ_PTimer_AddTo(treb_lazy_refine, last_solve);
                        //goto Retry;
                        /**/retry = true;
                    }
                  Satisfied:;
                }
            }
            /**/if (retry) goto Retry;

            return result;
        }

      Retry:;
    }
#endif
}


//=================================================================================================
// -- Local methods:


TrebSat_MonoSat::TrebSat_MonoSat(NetlistRef N_, const Vec<Vec<Cube> >& F_, const WMapS<float>& activity_, const Params_Treb& P_) :
    TrebSat_Common(N_, F_, activity_, P_),
    S(P.sat_solver),
    C(S, N, n2s, keep),
    last_cube(Cube_NULL),
    tmp_act(lit_Undef),
    wasted_lits(0),
    seed(P_.seed)
{
    // Initialize 'keep' to the set of gates with multiple fanouts + flop inputs:
    Auto_Pob(N, fanout_count);
    For_Gates(N, w)
        if (fanout_count[w] > 1)
            keep.add(w);
    For_Gatetype(N, gate_Flop, w)
        keep.add(w[0]);

  #if !defined(SAT_SNAPSHOT)
    C.quant_claus = true;
  #else
    C.quant_claus = false;
  #endif

#if defined(LAZY_CUBES)
    n_cubes_added = 0;
#endif

    recycle();  // -- put in clauses from 'F' if not empty.
}


Lit TrebSat_MonoSat::act(uint k)
{
    while (k >= act_.size()){
        act_.push(S.addLit());
        if (act_.size() == 1){
            // Attach initial state to activation literal 'act_[0]':
            Get_Pob(N, flop_init);
            For_Gatetype(N, gate_Flop, w){
                if (flop_init[w] != l_Undef){
                    Lit p = C.clausify(w);
                    S.addClause(~act_[0], p ^ (flop_init[w] == l_False));
                }
            }
        }
    }
    return act_[k];
}

void TrebSat_MonoSat::recycle()
{
    //**/WriteLn "[Recycled SAT]";
    C.clear();
    act_.clear();
#if !defined(LAZY_CUBES)
    for (uint d = 0; d < F.size(); d++)
        for (uint i = 0; i < F[d].size(); i++)
            blockCubeInSolver(TCube(F[d][i], (d == depth()+1) ? frame_INF : d));

#else
    uint total_cubes = 0;
    for (uint d = 0; d < F.size(); d++)
        total_cubes += F[d].size();
    //**/WriteLn "[Recycled SAT]  %_/%_ cubes were added", n_cubes_added, total_cubes;
    n_cubes_added = 0;
#endif

    wasted_lits = 0;
    last_cube = Cube_NULL;
    tmp_act = lit_Undef;
}


Cube TrebSat_MonoSat::weakenBySat(Cube s, Cube bad)
{
    Cube s_new = weaken(s, bad);    // -- 's_new' is minimized but without PIs that we need for SAT.
#if !defined(WEAKEN_BY_SAT)
    return s_new;
#endif

    ZZ_PTimer_Scope(treb_weaken_sat);
    Vec<Lit> assumps;

    // Add assumption for output:
    Lit tmp_act = lit_Undef;
    if (!bad){
        Get_Pob(N, init_bad);
        assumps.push(C.clausify(~init_bad[1]));

    }else{
        tmp_act = S.addLit();       // <<== use recycling mechanism here?
        assumps.push(tmp_act);

        Vec<Lit> tmp;
        tmp.push(~tmp_act);
        for (uint i = 0; i < bad.size(); i++)
            tmp.push(C.clausify(~N[bad[i]][0] ^ sign(bad[i])));
        S.addClause(tmp);
    }

    // Add assumption for LHS flops:
    for (uint i = 0; i < s_new.size(); i++){        // <<== sort flops?
        Wire w = N[s_new[i]]; assert(type(w) == gate_Flop);
        assumps.push(C.clausify(w));
    }

    // Add assumption for inputs:
    for (uint i = 0; i < s.size(); i++){
        Wire w = N[s[i]];
        if (type(w) == gate_PI)
            assumps.push(C.clausify(w));
    }

    // Solve:
    lbool result = S.solve(assumps);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    S.addClause(~tmp_act);

    // Remove literals from 's_new':
    Vec<GLit> s_final;
    Vec<Lit>  conflict;
    S.getConflict(conflict);
    for (uint i = 0; i < s_new.size(); i++){
        Wire w = N[s_new[i]];
        if (has(conflict, C.clausify(w)))
            s_final.push(w);
    }

    Cube z(s_final);
    /**/if (z.size() == s_new.size()) WriteLn "     sat-weaken: %_ -> (same)", z.size();
    /**/else                          WriteLn "  !! sat weaken: \a/%_ -> %_\a/", s_new.size(), z.size();
    //**/WriteLn "________________________________________";
    //**/WriteLn "bad  : %_", bad;
    //**/WriteLn "s_new: %_", s_new;
    //**/WriteLn "z    : %_", z;
    return z;
}


//=================================================================================================
// -- Virtual interface:


void TrebSat_MonoSat::blockCubeInSolver(TCube s)
{
    // Add negation of cube, possibly guarded by an activation literal, to the one SAT solver:
    Vec<Lit> ps;
    if (s.frame != frame_INF){
        assert(s.frame <= depth());
        ps.push(~act(s.frame)); }

    for (uint i = 0; i < s.cube.size(); i++)
        ps.push(C.clausify(~N[s.cube[i]]));
    S.addClause(ps);

    if (P.redund_cubes){
        if (s.frame != frame_INF && s.frame > 1){
            ps.clear();
            ps.push(~act(s.frame - 1));

            for (uint i = 0; i < s.cube.size(); i++){
                Wire w = ~N[s.cube[i]];
                w = w[0] ^ sign(w);
                ps.push(C.clausify(w));
            }
            S.addClause(ps);
        }
    }
}


void TrebSat_MonoSat::recycleSolver()
{
    recycle();
}


void solveMutual(const Vec<Cube>& bads, uint frame, /*out*/Vec<Cube>& blocked, /*out*/Vec<Cube>& preds)
{
/*
    antag bads i frame-1
    bevisa bads i frame (m. aktivation literals sa vi vet vilken som failar)
    aktivera blockerade kuber i frame-1

    om UNSAT, generalizera alla kuber i bads, spara i 'blocked', ta bort ur 'bads'
    om SAT, ternary sim pa predecessor, lagg till i preds, upprepa internt utan den bad
*/
}


#if !defined(SAT_SNAPSHOT)
TCube TrebSat_MonoSat::solveRelative(TCube s, uint params, Vec<Cube>* avoid)
{
    //*D*/Write "*\f";
    if (wasted_lits * 2 > S.nVars()){
        //**/WriteLn "[recycled SAT-solver]";
        recycle(); }

    Cube c = s.cube;
    uint k = s.frame;
    Vec<Lit> assumps;

    Lit avoid_act = lit_Undef;
    if (avoid){
        avoid_act = S.addLit();
        Vec<Cube>& cs = *avoid;
        Vec<Lit>   tmp;
        wasted_lits++;
        for (uint i = 0; i < cs.size(); i++){
            tmp.push(~avoid_act);
            for (uint j = 0; j < cs[i].size(); j++)
                tmp.push(~C.clausify(cs[i][j] + N));
            S.addClause(tmp);
            tmp.clear();
        }
        assumps.push(avoid_act);
        // <<== check if avoid is a superset of previous avoid...
    }

  #if 0
    // Assume property:     (<<== have option not to do this -- will produce proper invariants in 'F[inf]')
    Get_Pob(N, init_bad);
    assumps.push(C.clausify(~init_bad[1]));
  #endif

    // Add inductive assumption:
    if (!(params & sr_NoInduct)){   // (please do not refrain from avoiding the lack of double negations...)
        // Check if old clause is subsumed by 'g' (=> can recycle its activation literal):
        if (!last_cube)
            tmp_act = S.addLit();
        else{
            if (!subsumes(c, last_cube)){
                S.addClause(~tmp_act);
                tmp_act = S.addLit();
                wasted_lits++;
            }
        }
        last_cube = c;

        Vec<Lit> tmp;
        for (uint i = 0; i < c.size(); i++)
            tmp.push(C.clausify(~N[c[i]]));
        tmp.push(~tmp_act);
        S.addClause(tmp);
        assumps.push(tmp_act);
    }

    // Activate the right blocked cubes:
    if (k != frame_INF){
        for (int d = depth(); d >= (int)k-1; d--)
            assumps.push(act(d));
    }

    // Assume 's' at state outputs:
    Vec<GLit> cc(copy_, c);
    if (seed != 0)
        shuffle(seed, cc);
    if (P.use_activity)
        sobSort(ordReverse(sob(cc, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N))))));
    for (uint i = 0; i < cc.size(); i++){
        assert(type(N[cc[i]]) == gate_Flop);
        assumps.push(C.clausify(N[cc[i]][0]) ^ cc[i].sign); }

    // Solve:
    ZZ_PTimer_Mark(solve);
    lbool result = S_solve(assumps, k-1);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    TCube     ret;
    Vec<Lit>  conflict;
    S.getConflict(conflict);
    if (result == l_False){
        // UNSAT -- inspect use of assumption to return a shorter and later timed cube than 's' (if possible):

        // What frame did we need to assume for the blocked clauses?
        ret.frame = frame_INF;
        for (uint d = k-1; d <= depth(); d++){
            for (uint i = 0; i < conflict.size(); i++){
                if (conflict[i] == act(d)){
                    ret.frame = d + 1;
                    goto Found;
                }
            }
        }
      Found:;
        if (ret.frame == depth() + 1)
            ret.frame = depth();    // -- we don't allow pushing the cube beyond the last frame

        //**/ret.frame = k;  // <<== TEMPORARY!

        // Count "non-initialness" (for quick intersection check against Init):     <<== bwd
        Get_Pob(N, flop_init);
        uint counter = 0;
        for (uint i = 0; i < c.size(); i++){
            Wire w = N[c[i]];
            if ((flop_init[w] ^ sign(w)) == l_False)
                counter++;
        }

        // Figure out subset of 's' proved to be UNSAT:
        Vec<GLit> z;
        for (uint i = cc.size(); i > 0;){ i--;
            Wire w = N[cc[i]];
            bool init_lit = (flop_init[w] ^ sign(w)) == l_False;
            if (init_lit && counter == 1){
                z.push(w);   // -- cannot remove this literal or 's' would intersect the initial states
            }else{
                if (!has(conflict, C.clausify(w[0]) ^ sign(w))){
                    if (init_lit) counter--;    // -- literal could be removed since it was not used in the proof
                }else
                    z.push(w);
            }
        }
        ret.cube = Cube(z);

    }else{
        // SAT -- extract model from SAT and weaken it by ternary simulation:

        if (params & sr_ExtractModel){
            if (params & sr_NoSim)
                ret.cube = extractModel(S, C, false);
            else
                ret.cube = weakenBySat(extractModel(S, C, P.weaken != Params_Treb::NONE), c);
        }
    }

    /**/if (avoid_act != lit_Undef) S.addClause(~avoid_act);
    return ret;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#else

// Experimental version using SAT-solver snapshot mechanism
TCube TrebSat_MonoSat::solveRelative(TCube s, uint params, Vec<Cube>* avoid)
{
    assert(avoid == NULL);


    // Snapshot solver:
    act(depth());
    S.copyTo(S_copy);
    //**/WriteLn "//////////////////////////////////////// SNAPSHOT (#vars=%_)", S.nVars();

    Cube c = s.cube;
    uint k = s.frame;
    Vec<Lit> assumps;

    // Add inductive assumption:
    if (!(params & sr_NoInduct)){   // (please do not refrain from avoiding the lack of double negations...)
        Vec<Lit> tmp;
        for (uint i = 0; i < c.size(); i++)
            tmp.push(C.clausify(~N[c[i]]));
        S.addClause(tmp);
    }

    // Activate the right blocked cubes:
    if (k != frame_INF){
        for (int d = depth(); d >= (int)k-1; d--)
            assumps.push(act(d));
    }

    // Assume 's' at state outputs:
    Vec<GLit> cc(copy_, c);
    if (seed != 0)
        shuffle(seed, cc);
    if (P.use_activity)
        sobSort(ordReverse(sob(cc, proj_lt(compose(brack<float,Wire>(activity), brack<Wire,GLit>(N))))));
    for (uint i = 0; i < cc.size(); i++){
        assert(type(N[cc[i]]) == gate_Flop);
        assumps.push(C.clausify(N[cc[i]][0]) ^ cc[i].sign); }

    // Solve:
    ZZ_PTimer_Mark(solve);
    lbool result = S_solve(assumps, k-1);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    TCube     ret;
    Vec<Lit>  conflict;
    S.getConflict(conflict);
    if (result == l_False){
        // UNSAT -- inspect use of assumption to return a shorter and later timed cube than 's' (if possible):

        // What frame did we need to assume for the blocked clauses?
        ret.frame = frame_INF;
        for (uint d = k-1; d <= depth(); d++){
            for (uint i = 0; i < conflict.size(); i++){
                if (conflict[i] == act(d)){
                    ret.frame = d + 1;
                    goto Found;
                }
            }
        }
      Found:;
        if (ret.frame == depth() + 1)
            ret.frame = depth();    // -- we don't allow pushing the cube beyond the last frame

        // Count "non-initialness" (for quick intersection check against Init):     <<== bwd
        Get_Pob(N, flop_init);
        uint counter = 0;
        for (uint i = 0; i < c.size(); i++){
            Wire w = N[c[i]];
            if ((flop_init[w] ^ sign(w)) == l_False)
                counter++;
        }

        // Figure out subset of 's' proved to be UNSAT:
        Vec<GLit> z;
        for (uint i = cc.size(); i > 0;){ i--;
            Wire w = N[cc[i]];
            bool init_lit = (flop_init[w] ^ sign(w)) == l_False;
            if (init_lit && counter == 1){
                z.push(w);   // -- cannot remove this literal or 's' would intersect the initial states
            }else{
                if (!has(conflict, C.clausify(w[0]) ^ sign(w))){
                    if (init_lit) counter--;    // -- literal could be removed since it was not used in the proof
                }else
                    z.push(w);
            }
        }
        ret.cube = Cube(z);

    }else{
        // SAT -- extract model from SAT and weaken it by ternary simulation:

        if (params & sr_ExtractModel){
            if (params & sr_NoSim)
                ret.cube = extractModel(S, C, false);
            else
                ret.cube = weakenBySat(extractModel(S, C, P.weaken != Params_Treb::NONE), c);
        }
    }

    // Restore solver:
    //**/WriteLn "//////////////////////////////////////// RESTORE";
    S_copy.copyTo(S);
    For_All_Gates(N, w){
        if (n2s[w] && n2s[w].id >= S.nVars()){
            n2s(w) = lit_Undef;
            //**/Dump(w, n2s[w]);
        }
    }

    return ret;
}
#endif


#if !defined(SAT_SNAPSHOT)
// NOTE! 'k == frame_NULL' is allowed
Cube TrebSat_MonoSat::solveBad(uint k, bool restart)      // <<== 'k' redundant, always 'depth()' => for loop below can be simplified
{
    if (restart){
        recycle();
        S.randomizeVarOrder(seed);
    }

    Vec<Lit> assumps;

    // Assume bad:
    Get_Pob(N, init_bad);
    assumps.push(C.clausify(init_bad[1]));     // <<== bwd

    // Activate the right blocked cubes:
    for (uint d = k; d <= depth(); d++)     // -- here we rely on 'frame_NULL > depth()'
        assumps.push(act(d));

    // Solve:
    ZZ_PTimer_Mark(solve);
    lbool result = S_solve(assumps, k);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    // Extract model (if any):
    if (result == l_False)
        return Cube_NULL;
    else
        return weakenBySat(extractModel(S, C, P.weaken != Params_Treb::NONE), Cube_NULL);
}

#else
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

Cube TrebSat_MonoSat::solveBad(uint k, bool restart)
{
    // Snapshot solver:
    act(depth());
    S.copyTo(S_copy);

    // Assume bad:
    Get_Pob(N, init_bad);
    S.addClause(C.clausify(init_bad[1]));

    // Activate the right blocked cubes:
    for (uint d = k; d <= depth(); d++)     // -- here we rely on 'frame_NULL > depth()'
        S.addClause(act(d));

    // Solve:
    ZZ_PTimer_Mark(solve);
    Vec<Lit> empty;
    lbool result = S_solve(empty, k);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    // Extract model (if any):
    Cube ret;
    if (result == l_False)
        ret = Cube_NULL;
    else
        ret = weakenBySat(extractModel(S, C, P.weaken != Params_Treb::NONE), Cube_NULL);

    // Restore solver:
    S_copy.copyTo(S);
    For_All_Gates(N, w){
        if (n2s[w] && n2s[w].id >= S.nVars())
            n2s(w) = lit_Undef;
    }

    return ret;
}

#endif


#if !defined(SAT_SNAPSHOT)
bool TrebSat_MonoSat::isBlocked(TCube s)
{
    Cube c = s.cube;
    uint k = s.frame;
    Vec<Lit> assumps;

    // Activate the right blocked cubes:
    for (uint d = k; d <= depth(); d++)
        assumps.push(act(d));

    // Assume 's' at state inputs:
    for (uint i = 0; i < c.size(); i++){
        Wire w = N[c[i]];
        assumps.push(C.clausify(w)); }

    // Solve:
    ZZ_PTimer_Mark(solve);
    lbool result = S_solve(assumps, k);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    return result == l_False;
}

#else
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool TrebSat_MonoSat::isBlocked(TCube s)
{
    act(depth());
    S.copyTo(S_copy);

    Cube c = s.cube;
    uint k = s.frame;

    // Activate the right blocked cubes:
    for (uint d = k; d <= depth(); d++)
        S.addClause(act(d));

    // Assume 's' at state inputs:
    for (uint i = 0; i < c.size(); i++){
        Wire w = N[c[i]];
        S.addClause(C.clausify(w)); }

    // Solve:
    ZZ_PTimer_Mark(solve);
    Vec<Lit> empty;
    lbool result = S_solve(empty, k);
    if (result == l_Undef) throw Excp_TrebSat_Abort();

    if (result == l_True) ZZ_PTimer_AddTo(treb_sat  , solve);
    else                  ZZ_PTimer_AddTo(treb_unsat, solve);

    // Restore solver:
    S_copy.copyTo(S);
    For_All_Gates(N, w){
        if (n2s[w] && n2s[w].id >= S.nVars())
            n2s(w) = lit_Undef;
    }

    return result == l_False;
}


#endif

bool TrebSat_MonoSat::isInitial(Cube c)
{
    Get_Pob(N, flop_init);
    for (uint i = 0; i < c.size(); i++){
        Wire w = N[c[i]];
        if ((flop_init[w] ^ sign(w)) == l_False)
            return false;
    }

    return true;
}


//=================================================================================================
// -- Counterexample extraction:


void TrebSat_MonoSat::extractCex(const Vec<Cube>& cs, Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff, const WZetL* abstr)
{
    /*HACK*/
    SatStd            S;
    WMap<Lit>         n2s;
    Clausify<SatStd>  C(S, N, n2s, keep);

    Auto_Pob(N, fanout_count);
    For_Gates(N, w)
        if (fanout_count[w] > 1)
            keep.add(w);
    For_Gatetype(N, gate_Flop, w)
        keep.add(w[0]);

    C.quant_claus = true;
    /*END HACK*/

    assert(cs.size() > 0);

    pi.clear();
    ff.clear();

    // Extract the initial state:
    ff.push();
    for (uint i = 0; i < cs[0].size(); i++){
        Wire w = N[cs[0][i]];
        ff[0](attr_Flop(w).number) = sign(w) ? l_False : l_True; }

#if 1
    // Complete the initial state:
    Get_Pob(N, flop_init);
    For_Gatetype(N, gate_Flop, w){
        //**/if (abstr && !abstr->has(w)) continue;
        if (flop_init[w] != l_Undef){
            int num = attr_Flop(w).number;
            assert(ff[0](num) == l_Undef || ff[0](num) == flop_init[w]);
            ff[0](num) = flop_init[w];
            //**/WriteLn "  -- initial ff[%_] = %_", num, flop_init[w];
        }
    }
#endif

    // Extract CEX trace:
    Get_Pob(N, init_bad);
    for (uint k = 1; k <= cs.size(); k++){
        //**/Dump(k, cs.size());
        //**/Write "Abstr:"; For_Gatetype(N, gate_Flop, w){ if (abstr && abstr->has(w)) Write " %_", attr_Flop(w).number; } NewLine;
        //**/Write "Cube:";
        //**/if (k == cs.size())
        //**/    WriteLn " <bad states>";
        //**/else{
        //**/    for (uint i = 0; i < cs[k].size(); i++){
        //**/        int num = attr_Flop(cs[k][i] + N).number;
        //**/        Write " %C%_", sign(cs[k][i])?'~':0, num; }
        //**/    NewLine;
        //**/}

        // LHS assumptions:
        Vec<Lit> assumps;
        For_Gatetype(N, gate_Flop, w){
            /*A*/if (abstr && !abstr->has(w)) continue;
            int   num = attr_Flop(w).number;
            lbool val = ff[k-1](num);
            if (val != l_Undef){
                //**/WriteLn "  -- enforcing ff[%_] = %_", num, val;
                assumps.push(C.clausify(w) ^ (val == l_False)); }
        }

        // RHS: assumptions:
        if (k == cs.size()){
            assumps.push(C.clausify(init_bad[1]));
            //**/WriteLn "  -- enforcing bad' = 1";
        }else{
            for (uint i = 0; i < cs[k].size(); i++){
                Wire w = N[cs[k][i]];
                /*A*/assert(!abstr || abstr->has(w));
                assumps.push(C.clausify(w[0] ^ sign(w)));
                //**/WriteLn "  -- enforcing ff'[%_] = %_", attr_Flop(w).number, (uint)!sign(w);
            }
        }

        // RHS: clausify all flops:
        For_Gatetype(N, gate_Flop, w){
            /*A*/if (abstr && !abstr->has(w)) continue;
            C.clausify(w[0]); }

        // Solve:
        ZZ_PTimer_Mark(solve);
        lbool result = S.solve(assumps);
        if (result == l_Undef) throw Excp_TrebSat_Abort();
        assert(result == l_True);
        ZZ_PTimer_AddTo(treb_sat, solve);

        // Complete SIs:
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            if (ff[k-1](num) == l_Undef){
                Lit p = C.get(w);
                if (p == lit_Undef || S.value(p) == l_Undef){
                    assert(ff[k-1][num] == l_Undef);
                    ff[k-1](num) = l_False;     // <<== kan floppen redan ha motsatt värde??
                }else
                    ff[k-1](num) = S.value(p);
            }else{
                // Sanity checking:
                Lit p = C.get(w); assert(p != lit_Undef);
                assert(ff[k-1][num] == S.value(p));
            }
            //**/WriteLn "  -- completed ff[%_] = %_", num, ff[k-1][num];
        }

        // Push PIs and SOs:
        ff.push();
        pi.push();

        For_Gatetype(N, gate_Flop, w){
            /*A*/if (abstr && !abstr->has(w)) continue;
            Lit p = C.get(w[0]); assert(p != lit_Undef);
            if (S.value(p) == l_Undef){     // -- this can happen if variable does not occur in any clause (MiniSat 1.16 won't give it a value)
                assert(type(w[0]) == gate_PI);
//                ff[k](attr_Flop(w).number) = l_False ^ sign(w[0]);   // -- all flops in must have a value (or else we can get a broken trace in this extraction)
                ff[k](attr_Flop(w).number) = l_False ^ sign(p);
            }else
                ff[k](attr_Flop(w).number) = S.value(p);
            //**/WriteLn "  -- learned ff[%_] = %_", attr_Flop(w).number, ff[k][attr_Flop(w).number];
        }

        For_Gatetype(N, gate_PI, w){
            Lit p = C.get(w);
            if (p == lit_Undef || S.value(p) == l_Undef) continue;
            if (attr_PI(w).number == num_NULL) continue;
            pi[k-1](attr_PI(w).number) = S.value(p);
        }

        // Tie unused PIs to zero:
        For_Gatetype(N, gate_PI, w){
            int num = attr_PI(w).number;
            /*A*/if (num == num_NULL) continue;
            if (pi[k-1](num) == l_Undef)
                pi[k-1](num) = l_False;
        }
    }
    ff.pop();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'TrebSat_MultiSat':


struct TrebSat_MultiSat : TrebSat {
  //________________________________________
  //  State:

    Vec<TrebSat_MonoSat*> Z;
    Vec<uint64>           timestamp;
    uint                  ts_counter;

  //________________________________________
  //  Constructor / Destructor:

    TrebSat_MultiSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P);
   ~TrebSat_MultiSat();

  //________________________________________
  //  Virtual interface:

    void  blockCubeInSolver(TCube s);
    void  recycleSolver();
    TCube solveRelative(TCube s, uint params = 0, Vec<Cube>* avoid = NULL);
    Cube  solveBad(uint k, bool restart);
    bool  isBlocked(TCube s);
    bool  isInitial(Cube c);
    void  extractCex(const Vec<Cube>& cs, Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff, const WZetL* abstr);
};


//=================================================================================================
// -- Constructor / Destructor:


TrebSat_MultiSat::TrebSat_MultiSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P)
{
//    for (uint i = 0; i < 5; i++)
        Z.push(new TrebSat_MonoSat(N, F, activity, P));

    ts_counter = 0;
    timestamp.growTo(Z.size(), 0);
}


TrebSat_MultiSat::~TrebSat_MultiSat()
{
    for (uint i = 0; i < Z.size(); i++)
        delete Z[i];
}


//=================================================================================================
// -- Virtual interface:


// TEMPORARY! Should put an efficient version of this in 'Netlist/StdLib.cc'
#if 0
// with marking of global sources
static
void markTransitiveFanin(Wire w_sink, WZet& seen)
{
    if (seen.has(w_sink)) return;

    seen.add(w_sink);
    if (isGlobalSource(w_sink)) return;

    NetlistRef N = netlist(w_sink);
    Vec<GLit> Q(1, w_sink);
    while (Q.size() > 0){
        Wire w = N[Q.popC()]; assert(!isGlobalSource(w));
        For_Inputs(w, v){
            if (!seen.has(v)){
                seen.add(v);
                if (!isGlobalSource(v))
                    Q.push(v);
            }
        }
    }
}

#else
static void markTransitiveFanin(Wire w_sink, WZet& seen) ___unused;
static void markTransitiveFanin(Wire w_sink, WZet& seen)
{
    if (seen.has(w_sink)) return;

    if (isGlobalSource(w_sink)) return;
    seen.add(w_sink);

    NetlistRef N = netlist(w_sink);
    Vec<GLit> Q(1, w_sink);
    while (Q.size() > 0){
        Wire w = N[Q.popC()]; assert(!isGlobalSource(w));
        For_Inputs(w, v){
            if (!seen.has(v)){
                if (!isGlobalSource(v)){
                    seen.add(v);
                    Q.push(v);
                }
            }
        }
    }
}
#endif


void TrebSat_MultiSat::blockCubeInSolver(TCube s)
{
    for (uint i = 0; i < Z.size(); i++)
        Z[i]->blockCubeInSolver(s);
}


void TrebSat_MultiSat::recycleSolver()
{
    for (uint i = 0; i < Z.size(); i++)
        Z[i]->recycleSolver();
}


TCube TrebSat_MultiSat::solveRelative(TCube s, uint params, Vec<Cube>* avoid)
{
#if 0
    ZZ_PTimer_Begin(treb_multi_choose);
    NetlistRef N = Z[0]->N;

    // Mark transitive fanin of 's':
    WZet seen;
    for (uint i = 0; i < s.cube.size(); i++)
        markTransitiveFanin(N[s.cube[i]][0], seen);

    // Look how much extra logic is in each solver:
    uint orig_sz = seen.size();
    uint best    = UINT_MAX;
    uint best_i  = UINT_MAX;
    for (uint i = 0; i < Z.size(); i++){
        For_Gatetype(N, gate_Flop, w){                      // <<== abort early here if seen has grown beyund orig_sz + best (and best != UINT_MAX)
            if (Z[i]->C.get(w[0]) != lit_Undef)
                markTransitiveFanin(w[0], seen);
        }

        uint waste = seen.size() - orig_sz;
        if (newMin(best, waste))
            best_i = i;

        for (uint j = orig_sz; j < seen.list().size(); j++)        // <<== make this method of IntZet
            seen.exclude(seen.list()[j]);
        seen.list().shrinkTo(orig_sz);
        //**/WriteLn "  -- solver %_: %_", i, waste;
    }

    if (orig_sz > 0){
        double ratio = double(best) / (orig_sz + best);
        if (ratio > 0.2){
            uint64 oldest = timestamp[0];
            best_i = 0;
            for (uint i = 1; i < Z.size(); i++)
                if (newMin(oldest, timestamp[i]))
                    best_i = i;

            Z[best_i]->recycleSolver();
            best = 0;
            ratio = 0;
            //**/WriteLn "Recycled solver %_", best_i;
        }
        //**/WriteLn "choosing solver %_ with waste %_ (%.2f %%)", best_i, best, ratio * 100;
    }
    ZZ_PTimer_End(treb_multi_choose);

    timestamp[best_i] = ++ts_counter;
    return Z[best_i]->solveRelative(s, params, avoid);

#else
    uint k = (s.frame == frame_INF) ? 0 : s.frame + 1;
    while (Z.size()-1 < k)
        Z.push(new TrebSat_MonoSat(Z[0]->N, Z[0]->F, Z[0]->activity, Z[0]->P));
    TCube ret = Z[k]->solveRelative(s, params, avoid);
    //**/if (ret) ret.frame = s.frame;
    return ret;
#endif
}


Cube TrebSat_MultiSat::solveBad(uint k, bool restart)
{
    // pick emptiest (or '0', which maybe should be the only solver with the Bad cone?)
    uint n = 0;
    return Z[n]->solveBad(k, restart);
}


bool TrebSat_MultiSat::isBlocked(TCube s)
{
    // pick emptiest (don't need cones at all -- maybe just have s special solver instance for that?)
    uint n = 0;
    return Z[n]->isBlocked(s);
}


bool TrebSat_MultiSat::isInitial(Cube c)
{
    return Z[0]->isInitial(c);
}


void TrebSat_MultiSat::extractCex(const Vec<Cube>& cs, Vec<Vec<lbool> >& pi, Vec<Vec<lbool> >& ff, const WZetL* abstr)
{
    return Z[0]->extractCex(cs, pi, ff, abstr);
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Factroy functions:


TrebSat* TrebSat_monoSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P)
{
    return new TrebSat_MonoSat(N, F, activity, P);
}


TrebSat* TrebSat_multiSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P)
{
    return new TrebSat_MultiSat(N, F, activity, P);
}


TrebSat* TrebSat_gigSat(NetlistRef N, const Vec<Vec<Cube> >& F, const WMapS<float>& activity, const Params_Treb& P)
{
    return NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
