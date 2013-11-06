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


struct FtModel {
    const MetaSat& S;
    const WMapX<Lit>& n2s;
    FtModel(const MetaSat& S_, const WMapX<Lit>& n2s_) : S(S_), n2s(n2s_) {}

    bool operator[](GLit w) const {
        Lit p = n2s[w]; assert(p);
        lbool v = S.value(p); assert(v != l_Undef);
        return v == l_True;
    }
};



void justify(const Gig& N, const FtModel& model, /*outputs:*/WSeen& seen, Vec<GLit>& prime)
{
    #define Push(w) (seen.add(+(w)) || (Q.push(+(w).lit()), true))

    Vec<GLit> Q;
    seen.add(GLit_True);
    Push(+N(gate_PO, 0)[0]);

    // Justification loop:
    while (Q.size() > 0){
        Wire w = Q.popC() + N; assert(!sign(w));

        if (w == gate_PI)
            prime.push(w);

        else{
            assert(w == gate_Npn4);
            uint cl = w.arg();
            uint a = 0;
            For_Inputs(w, v)
                if (model[v])
                    a |= 1u << Iter_Var(v);
            uint just = npn4_just[cl][a];       // -- look up all minimal justifications

            // Follow one justification:
            for (uint i = 0; i < 4; i++){
                if (just & (1u << i))
                    Push(w[i]);
            }
        }
    }

    // Add signs to prime literal:
    for (uint i = 0; i < prime.size(); i++)
        if (!model[prime[i]])
            prime[i] = ~prime[i];

    #undef Push
}


void weaken(const Gig& N, const FtModel& model, /*out*/Vec<GLit>& prime)
{
    WSeen seen;
    justify(N, model, seen, prime);
    //ternary sim...
}


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


// Takes a fault-tree 'N' (which will be massaged into a more optimized form) and computes
// all satisfying assignments.
void enumerateModels(Gig& N, const Vec<double>& ev_probs, const Vec<String>& ev_names)
{
    convertToAig(N);

    Gig N_ref;
    N.copyTo(N_ref);

#if 0   /*QUICK HACK*/
    For_UpOrder(N, w){
        if (w == gate_PI)
            WriteLn "%_ = PI [%_]", w.lit(), w.num();
        else if (w == gate_And)
            WriteLn "%_ = And(%_, %_)", w.lit(), w[0].lit(), w[1].lit();
        else { assert(w == gate_PO);
            WriteLn "%_ = PO(%_) [%_]", w.lit(), w[0].lit(), w.num(); }
    }
#endif  /*END HACK*/

    Params_CnfMap P_cnf;
    P_cnf.quiet = true;
    P_cnf.map_to_luts = false;
    cnfMap(N, P_cnf);

    WriteLn "Mapped: %_", info(N);

    assert(N.enumSize(gate_PO) == 1);
    MiniSat2 S;
    WMapX<Lit> n2s;
    Lit p_top = clausify(N.enumGate(gate_PO, 0), S, n2s);

    double total_prob = 0;
    for(;;){
        printf("\r#clauses: %u   (prob: %g)", (uint)S.nClauses(), total_prob); fflush(stdout);

        lbool result = S.solve(p_top);
        FtModel model(S, n2s);

        if (result == l_True){
            Vec<GLit> prime;
            weaken(N, model, prime);
            //WriteLn "prime: %_", Fmt_Prime(N, prime, ev_names);
            //WriteLn "sim says: %_", sim(N_ref(gate_PO, 0), prime, N);

            // Compute probability:
            double prob = 1;
            for (uint i = 0; i < prime.size(); i++){
                Wire w = prime[i] + N;
                double x = ev_probs[w.num()];
                prob *= w.sign ? (1 - x) : x;
            }
            total_prob += prob;     // <<== this is not numerically sound, nor is it a proper lower bound since primes may overlap

            // Add clause:
            Vec<Lit> tmp;
            for (uint i = 0; i < prime.size(); i++)
                tmp.push(~n2s[prime[i]]);
            S.addClause(tmp);

        }else
            break;
    }

    WriteLn "CPU-time: %t", cpuTime();
}


/*
reparam
push negations + add {~x, ~y} for each x=e0, y=~e0. [now we can optimize for prob]
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
