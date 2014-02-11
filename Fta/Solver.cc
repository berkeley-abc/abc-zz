//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description :
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Solver.hh"
#include "Common.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ_Gip.Common.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


struct Fmt_Prime {
    const Gig& N;
    const Vec<GLit>& prime;
    const Vec<String>& ev_names;
    Fmt_Prime(const Gig& N_, const Vec<GLit>& prime_, const Vec<String>& ev_names_) : N(N_), prime(prime_), ev_names(ev_names_) {}
};


template<> fts_macro void write_(Out& out, const Fmt_Prime& v)
{
    out += '{';
    for (uint i = 0; i < v.prime.size(); i++){
        if (i > 0) out += ", ";
        Wire w = v.prime[i] + v.N; assert(w == gate_PI);
        out += v.ev_names[w.num()];
    }
    out += '}';
}


// Naive simulator (recursive, no memoization (so better be tree-like netlist)).
static
lbool sim(Wire w, const Vec<GLit>& prime, const Gig& N_prime)
{
    if (w == gate_PI){
        uint n = w.num();
        for (uint i = 0; i < prime.size(); i++)
            if ((prime[i] + N_prime).num() == n)
                return (prime[i].sign == w.sign) ? l_True : l_False;
        return l_Undef;

    }else if (w == gate_PO){
        return sim(w[0], prime, N_prime) ^ sign(w);

    }else if (w == gate_And){
        return (sim(w[0], prime, N_prime) & sim(w[1], prime, N_prime)) ^ sign(w);

    }else
        assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void shrink(Vec<Lit>& zcube, MiniSat2& Z, const IntMap<uint, double>& zprob, const Params_FtaEnum& P)
{
#if 1
    lbool result = Z.solve(zcube);
    assert(result == l_False);

    Z.getConflict(zcube);

  #if 0
    // Temporary selection sort:
    for (uint i = 0; i < zcube.size()-1; i++){
        double best_prob = zprob[zcube[i].id];
        uint best_idx = i;
        for (uint j = i+1; j < zcube.size(); j++){
            if (newMin(best_prob, zprob[zcube[j].id]))
                best_idx = j;
        }
        swp(zcube[i], zcube[best_idx]);
    }

    for (uint i = 0; i < zcube.size(); i++){
        Lit p = zcube[i];
        zcube[i] = Z.True();
        if (Z.solve(zcube) == l_True)
            zcube[i] = p;
    }
  #endif
#endif

    for (uint i = 0; i < zcube.size(); i++)
        if (zcube[i] == Z.True() || zprob[zcube[i].id] > P.high_prob_thres)
            zcube[i] = Lit_NULL;

    filterOut(zcube, isNull<Lit>);
}


// Takes a fault-tree 'N' (which will be massaged into a more optimized form) and computes
// all satisfying assignments.
void enumerateModels(Gig& N, const Vec<double>& ev_probs0, const Vec<String>& ev_names, const Params_FtaEnum& P)
{
    assert(N.enumSize(gate_PO) == 1);

    // Change to AIG encoding with one literal for each polarity of a basic event:
    convertToAig(N);
    uint n_vars = N.enumSize(gate_PI);

    pushNegations(N);
    WriteLn "Unmapped unate netlist: %_", info(N);

    // Setup probabilities for new encoding:
    Vec<double> ev_probs;
    for (uint i = 0; i < ev_probs0.size(); i++) ev_probs.push(ev_probs0[i]);
    for (uint i = 0; i < ev_probs0.size(); i++) ev_probs.push(1 - ev_probs0[i]);

    // Add cutoff:
    {
        double cutoff = P.mcs_cutoff;
        N.strash();
        uint sz = N.count();
        Vec<GLit> vars;
        Vec<double> costs;
        for (uint i = 0; i < 2*n_vars; i++){
            if (ev_probs[i] > P.high_prob_thres) continue;
            vars.push(N(gate_PI, i));
            costs.push(-log(ev_probs[i]));
        }

        N.add(gate_PO).init(addCutoff(N, 0, -log(cutoff), vars, costs, P.cutoff_quant));
        N.unstrash();

        WriteLn "Cutoff logic: %_ gates", N.count() - sz;
    }

    // Generate Cnf:
    Params_CnfMap P_cnf;
    P_cnf.quiet = true;
    P_cnf.map_to_luts = false;
    cnfMap(N, P_cnf);

    Wire w_top = N.enumGate(gate_PO, 0);
    Wire w_cut = N.enumGate(gate_PO, 1);
    MiniSat2 S, Z;
    WMapX<Lit> n2s, n2z;
    S.addClause( clausify(w_top, S, n2s));
    S.addClause( clausify(w_cut, S, n2s));
    Z.addClause(~clausify(w_top, Z, n2z));

    // Add dummy variable for irrelevant polarities:
    for (uint i = 0; i < 2*n_vars; i++){
        Wire w = N(gate_PI, i);
        if (!n2s[w]) n2s(w) = S.addLit();
        if (!n2z[w]) n2z(w) = Z.addLit();
    }

    // A variable can't be both true and false:
    Vec<GLit> vars(2 * n_vars);
    For_Gatetype(N, gate_PI, w){
        uint n = w.num();
        Lit  p = n2s[w]; assert(!p.sign);       // -- we don't want basic events to have a negated encoding (why should they?)
        vars[n] = p;
    }

    for (uint i = 0; i < n_vars; i++)
        S.addClause(~vars[i], ~vars[i + n_vars]);

    // Back maps:
    IntMap<uint, GLit> z2n;   // -- literal ID of variable in 'Z' -> PI in 'N'
    IntMap<uint, double> zprob;
    for (uint i = 0; i < 2*n_vars; i++){
        Lit p = n2z[N(gate_PI, i)]; assert(!p.sign);
        z2n(p.id) = N(gate_PI, i);
        zprob(p.id) = ev_probs[i];
    }


    // Print some stats:
    WriteLn "Mapped unate netlist: %_   (#clauses: %_)", info(N), S.nClauses();

    // Enumerate primes:
    double total_prob = 0;
    uint n_cubes = 0;

    uint  iter = 0;
    double lim = 10;
    for(;;){

        lbool result = S.solve();

        if (iter > lim || result == l_False){
            iter = 0;
            lim *= 1.3;

            char pr[128];
            sprintf(pr, "%g", total_prob);
            WriteLn "#MCS: %_    Prob: %<12%_   [%t]", n_cubes, pr, cpuTime();
        }

        if (result == l_True){
            // Extract cube:
            Vec<GLit> zcube;
            //**/Vec<GLit> scube;
            For_Gatetype(N, gate_PI, w){
                lbool v = S.value(n2s[w]);
                if (v == l_True)
                    //**/scube.push(n2s[w]),
                    zcube.push(n2z[w]);
            }

            // Shrink it to a prime:
            shrink(zcube, Z, zprob, P);

            Vec<GLit> prime;
            for (uint i = 0; i < zcube.size(); i++)
                prime.push(z2n[zcube[i].id]);

            // Compute probability:
            double prob = 1;
            for (uint i = 0; i < prime.size(); i++){
                Wire w = prime[i] + N;
                prob *= ev_probs[w.num()];
            }
            //**/Dump(scube, prob);
            total_prob += prob;     // -- this is not numerically sound, nor is it a proper lower bound since primes may overlap

            // Add clause:
            Vec<Lit> tmp;
#if 0
            for (uint i = 0; i < prime.size(); i++)
                tmp.push(~n2s[prime[i]]);
#else
            for (uint i = 0; i < prime.size(); i++){
                Wire w = prime[i] + N;
                uint num = (w.num() < n_vars) ? w.num() + n_vars : w.num() - n_vars;
                tmp.push(n2s[N(gate_PI, num)]);
            }
#endif
            S.addClause(tmp);

            n_cubes++;
            iter++;

        }else
            break;
    }
    printf("\n"); fflush(stdout);

    WriteLn "CPU-time: %t", cpuTime();
}


/*
reparam
push negations + add {~x, ~y} for each x=e0, y=~e0. [now we can optimize for prob]
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
