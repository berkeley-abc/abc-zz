//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.cc
//| Author(s)   : Niklas Een
//| Module      : MaxSat
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// First attempt:


void naiveMaxSat(MaxSatProb& P)
{
    // Insert clauses into SAT solver:
    MiniSat2 S;
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
        act_pi.push(N.add(gate_PI));
    oddEvenSort(N, act_pi);
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

    // Optimization loop:
    for (uint slack = 0; slack <= act.size(); slack++){
        lbool result;
        if (slack != act.size()){
          #if 1
            uint n = act.size() - 1 - slack;
            //uint n = slack;
            Lit p = clausify(N(gate_PO, n), S, n2s);
            result = S.solve(p);
          #else
            Vec<Lit> assumps;
            for (uint n = 0; n < act.size() - slack; n++)
                assumps.push(clausify(N(gate_PO, n), S, n2s));
            result = S.solve(assumps);
          #endif
        }else
            result = S.solve();

        if (result == l_True){
            WriteLn "MODEL FOUND.  Relaxed clauses: %_.  Satisfied clauses: %_", slack, act.size() - slack;
#if 0   /*DEBUG*/
            For_Gatetype(N, gate_PI, w)
                WriteLn "pi[%_] = %_", w.num(), S.value(n2s[w]);
            NewLine;
            For_Gatetype(N, gate_PO, w)
                WriteLn "po[%_] = %_", w.num(), S.value(n2s[w]);
#endif  /*END DEBUG*/
            WriteLn "CPU-time: %t", cpuTime();

            return;
        }else
            WriteLn "%_ relaxed clasuses => UNSAT...   [%t]", slack, cpuTime();
    }

    WriteLn "Hard clauses are UNSAT.";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
