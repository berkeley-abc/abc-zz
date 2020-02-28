//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.cc
//| Author(s)   : Niklas Een
//| Module      : MaxSat
//| Description :
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Parser.hh"
#include "ZZ_MetaSat.hh"
#include "ZZ_Gig.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ_Gip.Common.hh"
#include "Sorters.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sorting network:


macro void cmp2(Gig& N, Vec<GLit>& fs, uint pos)
{
    Wire a = fs[pos] + N;
    Wire b = fs[pos + 1] + N;
    fs[pos]     = mkOr (a, b);
    fs[pos + 1] = mkAnd(a, b);
}


static
void riffle(Vec<GLit>& fs)
{
    Vec<GLit> tmp(copy_, fs);
    for (uint i = 0; i < fs.size() / 2; i++){
        fs[i*2]     = tmp[i];
        fs[i*2 + 1] = tmp[i+fs.size() / 2];
    }
}


static
void unriffle(Vec<GLit>& fs)
{
    Vec<GLit> tmp(copy_, fs);
    for (uint i = 0; i < fs.size() / 2; i++){
        fs[i]                 = tmp[i*2];
        fs[i + fs.size() / 2] = tmp[i*2 + 1];
    }
}


static
void oddEvenMerge(Gig& N, Vec<GLit>& fs, uint begin, uint end)
{
    assert(end - begin > 1);
    if (end - begin == 2)
        cmp2(N, fs, begin);

    else{
        uint      mid = (end - begin) / 2;
        Vec<GLit> tmp;
        for (uint i = 0; i < end - begin; i++)
            tmp.push(fs[begin+i]);

        unriffle(tmp);
        oddEvenMerge(N, tmp, 0  , mid);
        oddEvenMerge(N, tmp, mid, tmp.size());
        riffle(tmp);

        for (uint i = 1; i < tmp.size()-1; i += 2)
            cmp2(N, tmp, i);
        for (uint i = 0; i < tmp.size(); i++)
            fs[i + begin] = tmp[i];
    }
}


// 'fs' should contain the inputs to the sorting network and will be overwritten by the outputs.
// NOTE: The number of comparisons is bounded by: n * log n * (log n + 1)
// NOTE: Network sorts 1s to lower part of 'fs', 0s to upper part.
void oddEvenSort(Gig& N, Vec<GLit>& fs)
{
    uint orig_sz = fs.size();
    uint sz;
    for (sz = 1; sz < fs.size(); sz *= 2);
    fs.growTo(sz, ~GLit_True);

    for (uint i = 1; i < fs.size(); i *= 2)
        for (uint j = 0; j + 2*i <= fs.size(); j += 2*i)
            oddEvenMerge(N, fs, j, j+2*i);

    fs.shrinkTo(orig_sz);
}


//=================================================================================================


struct CmpGig {
    Gig& N;
    Vec<GLit>& result; // -- initialized to input signals; will contain output signals

    CmpGig(Gig& N_, Vec<GLit>& result_) : N(N_), result(result_) {}

    void operator()(uint i, uint j) {
        l_tuple(result[i], result[j]) = make_tuple(mkOr (result[i] + N, result[j] + N),
                                              mkAnd(result[i] + N, result[j] + N));
    }
};


void oddEvenSort2(Gig& N, Vec<GLit>& fs)
{
    uint orig_sz = fs.size();
    uint sz;
    for (sz = 1; sz < fs.size(); sz *= 2);
    fs.growTo(sz, ~GLit_True);

    CmpGig cmp(N, fs);
    oeSort(fs.size(), cmp);

    fs.shrinkTo(orig_sz);
}


void pairWiseSort(Gig& N, Vec<GLit>& fs)
{
    uint orig_sz = fs.size();
    uint sz;
    for (sz = 1; sz < fs.size(); sz *= 2);
    fs.growTo(sz, ~GLit_True);

    CmpGig cmp(N, fs);
    pwSort(fs.size(), cmp);

    fs.shrinkTo(orig_sz);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// First attempt:


//up/down card req
//single/multi assumption
//invert act

#define SINGLE_ASSUMP
#define INVERT_ACT
//#define REVERSE_ORDER

void sorterMaxSat(MaxSatProb& P, bool down)
{
    // Insert clauses into SAT solver:
    MiniSat2s S;
    //GlrSat S;
    Vec<Lit> act;
    Vec<Lit> tmp;

    while (S.nVars() < P.n_vars + 2)
        S.addLit();

    for (uint i = 0; i < P.size(); i++){
        tmp.clear();
        for (uint j = 0; j < P[i].size(); j++)
            tmp.push(Lit(P[i][j].id + 1, P[i][j].sign));    // -- here we assume first literal has index 2 (true for MiniSat's)

        if (P.weight[i] == UINT64_MAX){
            S.addClause(tmp);

        }else if (P.weight[i] == 1){
            act.push(S.addLit());
            tmp.push(~act[LAST]);
            S.freeze(act[LAST].id);
            S.addClause(tmp);

        }else{
            WriteLn "Weighted MaxSat not supported yet.";
            exit(1);
        }
    }

    // Create sorting network for activation variables:
    Gig N;
    WMapX<Lit> n2s;

    Vec<GLit> act_pi;
    for (uint i = 0; i < act.size(); i++)
      #if !defined(INVERT_ACT)
        act_pi.push(N.add(gate_PI));
      #else
        act_pi.push(~N.add(gate_PI));
      #endif

    //**/uint64 seed = 42;
    //**/shuffle(seed, act_pi);
      #if defined(REVERSE_ORDER)
        reverse(act_pi);
      #endif
    //oddEvenSort(N, act_pi);
    //oddEvenSort2(N, act_pi);
    pairWiseSort(N, act_pi);
    for (uint i = 0; i < act_pi.size(); i++)
        N.add(gate_PO).init(act_pi[i]);

#if 0
    Params_CnfMap P_cnf;
    P_cnf.quiet = true;
    P_cnf.map_to_luts = false;
    cnfMap(N, P_cnf);
    // + migrate n2s (or just init. n2s here instead)
#endif

    For_Gatetype(N, gate_PI, w)
        n2s(w) = act[w.num()];

    WriteLn "N: %_", info(N);

    S.preprocess(true);
    uint orig_clauses = S.nClauses();

    if (!down){
        // Optimization loop:
        for (uint slack = 0; slack <= act.size(); slack++){
            lbool result;
        #if defined(SINGLE_ASSUMP)
          #if !defined(INVERT_ACT)
            uint n = act.size() - 1 - slack;
            Lit p = clausify(N(gate_PO, n), S, n2s);
          #else
            uint n = slack;
            Lit p = ~clausify(N(gate_PO, n), S, n2s);
          #endif
            result = S.solve(p);

        #else
            Vec<Lit> assumps;
          #if !defined(INVERT_ACT)
            for (uint n = 0; n < act.size() - slack; n++)
                assumps.push(clausify(N(gate_PO, n), S, n2s));
          #else
            for (uint n = slack; n < act.size(); n++)
                assumps.push(~clausify(N(gate_PO, n), S, n2s));
          #endif
            result = S.solve(assumps);
         #endif

            if (result == l_True){
                WriteLn "MODEL FOUND.  Relaxed clauses: %_.  Satisfied clauses: %_", slack, act.size() - slack;
                WriteLn "CPU-time: %t", cpuTime();

                return;
            }else
                WriteLn "%_ relaxed clasuses => UNSAT...   [%t]  (%_ clauses)", slack, cpuTime(), S.nClauses() - orig_clauses;
        }

        WriteLn "Hard clauses are UNSAT.";

    }else{
        // Optimization loop:
        for (int slack = act.size(); slack >= 0; slack--){
            lbool result;
            Vec<Lit> assumps;
        #if defined(SINGLE_ASSUMP)      // -- doesn't really make sense for sort-down
          #if !defined(INVERT_ACT)
            uint n = act.size() - 1 - slack;
            Lit p = (n < N.enumSize(gate_PO)) ? clausify(N(gate_PO, n), S, n2s) : S.True();
          #else
            uint n = slack;
            Lit p = (n < N.enumSize(gate_PO)) ? ~clausify(N(gate_PO, n), S, n2s) : S.True();
          #endif
            result = S.solve(p);

        #else
          #if !defined(INVERT_ACT)
            for (uint n = 0; n < act.size() - slack; n++)
                assumps.push(clausify(N(gate_PO, n), S, n2s));
          #else
            for (uint n = slack; n < act.size(); n++)
                assumps.push(~clausify(N(gate_PO, n), S, n2s));
          #endif
            result = S.solve(assumps);
        #endif

            if (result == l_False){
                WriteLn "UNSAT.  Relaxed clauses: %_.  Satisfied clauses: %_", slack+1, act.size() - (slack-1);
                WriteLn "CPU-time: %t", cpuTime();

                return;
            }else
                WriteLn "%_ relaxed clasuses => SAT...   [%t]  (%_ clauses)", slack, cpuTime(), S.nClauses() - orig_clauses;
        }
        WriteLn "All clauses are SAT. (relaxed claues: 0)";
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Second attempt:


void coreMaxSat(MaxSatProb& P)
{
    // Insert clauses into SAT solver:
    MiniSat2s S;
    //GlrSat S;
    Vec<Lit>    act;
    Vec<double> score;     // -- activity of activation literal (indexed by literal ID)
    Vec<Lit>    tmp;

    while (S.nVars() < P.n_vars + 2)
        S.addLit();

    for (uint i = 0; i < P.size(); i++){
        tmp.clear();
        for (uint j = 0; j < P[i].size(); j++)
            tmp.push(Lit(P[i][j].id + 1, P[i][j].sign));    // -- here we assume first literal has index 2 (true for MiniSat's)

        if (P.weight[i] == UINT64_MAX){
            S.addClause(tmp);

        }else if (P.weight[i] == 1){
            act.push(S.addLit());
            tmp.push(~act[LAST]);
            S.freeze(act[LAST].id);
            S.addClause(tmp);

        }else{
            WriteLn "Weighted MaxSat not supported yet.";
            exit(1);
        }
    }

    Vec<Lit> orig_act(copy_, act);

    double act_inc = 1.0;
    uint card_clauses = 0;
    uint slack = 0;
    for(;;){
        WriteLn "Solving for %_ relaxed subsets.  [%t]  (%_ card.cl.)", slack, cpuTime(), card_clauses;

        //**/static uint64 seed = 1;
        //**/shuffle(seed, act);
      #if 1
        lbool result = S.solve(act);
      #else
        lbool result;
        Vec<Lit> sub;
        for(;;){
            result = S.solve(sub);
            if (result == l_True){
                uint sub_sz = sub.size();
                for (uint i = 0; i < act.size(); i++){
                    if (S.value(act[i]) == l_False)
                        sub.push(act[i]);
                }
                if (sub_sz == sub.size())
                    break;
            }else
                break;
        }
      #endif

        if (result == l_True){
            uint n_unsat = 0;
            for (uint i = 0; i < orig_act.size(); i++)
                if (S.value(orig_act[i]) == l_False)
                    n_unsat++;

            WriteLn "Optimal solution found. Unsat clauses: %_", n_unsat;
            WriteLn "CPU-time: %t", cpuTime();
            break;
        }

        slack++;

        // Relax one element from the core:
        Vec<Lit> confl;
        S.getConflict(confl);

        Vec<Lit> ps;        // -- will participate in cardinality constraint
        /**/WriteLn "  -- core size: %_", confl.size();
        for (uint i = 0; i < confl.size(); i++){
            Lit p = S.addLit();
            Lit a = S.addLit();     // -- new activation literal
            Lit b = confl[i];       // -- old activation literal
            uind pos = find(act, b);
            S.addClause(p, ~a, b);
            act[pos] = a;
            score(a.id, 0.0) = score(b.id, 0.0) + act_inc;      // -- update activity score:

            S.freeze(a.id);
            S.thaw(b.id);

            ps.push(p);
        }
        act_inc *= 1.05;
        assert(act_inc < 1e100);

        // Cardinality constraint:
        uint n_clauses = S.nClauses();
        if (ps.size() <= 5){
            for (uint i = 0; i < ps.size(); i++)
                for (uint j = i+1; j < ps.size(); j++)
                    S.addClause(~ps[i], ~ps[j]);

        }else{
            Gig N;
            WMapX<Lit> n2s;

            Vec<GLit> act_pi;
            for (uint i = 0; i < ps.size(); i++)
                act_pi.push(N.add(gate_PI));

            For_Gatetype(N, gate_PI, w)
                n2s(w) = ps[w.num()];

            pairWiseSort(N, act_pi);
            for (uint i = 0; i < act_pi.size(); i++)
                N.add(gate_PO).init(act_pi[i]);

            Lit p = clausify(N(gate_PO, 1), S, n2s);
            S.addClause(~p);
        }
        card_clauses += S.nClauses() - n_clauses;

        // Sort 'act' on score:
        Vec<Pair<double, Lit> > as;
        for (uint i = 0; i < act.size(); i++)
            as.push(make_tuple(-score(act[i].id, 0.0), act[i]));
        sort(as);
        for (uint i = 0; i < as.size(); i++)
            act[i] = as[i].snd;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


// Prova assumption med assumption-aktivitet
