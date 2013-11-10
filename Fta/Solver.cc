//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
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


void shrink(Vec<Lit>& zcube, MiniSat2& Z, const IntMap<uint, double>& zprob)
{
    lbool result = Z.solve(zcube);
    assert(result == l_False);

    //Z.getConflict(zcube);

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

    for (uint i = 0; i < zcube.size(); i++)
        if (zcube[i] == Z.True() || zprob[zcube[i].id] > 0.99)  // <<== make this threshold a parameter
            zcube[i] = Lit_NULL;

    filterOut(zcube, isNull<Lit>);
}


// Takes a fault-tree 'N' (which will be massaged into a more optimized form) and computes
// all satisfying assignments.
void enumerateModels(Gig& N, const Vec<double>& ev_probs0, const Vec<String>& ev_names)
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

    // Generate Cnf:
    Params_CnfMap P_cnf;
    P_cnf.quiet = true;
    P_cnf.map_to_luts = false;
    cnfMap(N, P_cnf);

    MiniSat2 S, Z;
    WMapX<Lit> n2s, n2z;
    S.addClause( clausify(N.enumGate(gate_PO, 0), S, n2s));
    Z.addClause(~clausify(N.enumGate(gate_PO, 0), Z, n2z));

    // Add dummy variable for irrelevant polarities:
    for (uint i = 0; i < 2*n_vars; i++){
        Wire w = N(gate_PI, i);
        if (!n2s[w]) n2s(w) = S.addLit();
        if (!n2z[w]) n2z(w) = Z.addLit();
    }

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
    for(;;){
        printf("\r#clauses: %u   (prob est.: %g)", (uint)S.nClauses(), total_prob); fflush(stdout);

        lbool result = S.solve();

        if (result == l_True){
            // Extract cube:
            Vec<GLit> zcube;
            For_Gatetype(N, gate_PI, w){
                lbool v = S.value(n2z[w]);
                if (v == l_True)
                    zcube.push(n2z[w]);
            }

            // Shrink it to a prime:
            shrink(zcube, Z, zprob);

            Vec<GLit> prime;
            for (uint i = 0; i < zcube.size(); i++)
                prime.push(z2n[zcube[i].id]);

            // Compute probability:
            double prob = 1;
            for (uint i = 0; i < prime.size(); i++){
                Wire w = prime[i] + N;
                prob *= ev_probs[w.num()];
            }
            //**/Dump(zcube.size(), prob);
            total_prob += prob;     // -- this is not numericaly sound, nor is it a proper lower bound since primes may overlap

            // Add clause:
            Vec<Lit> tmp;
            for (uint i = 0; i < prime.size(); i++)
                tmp.push(~n2s[prime[i]]);
            S.addClause(tmp);

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
