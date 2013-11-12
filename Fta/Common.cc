//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
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
#include "ZZ_Gig.hh"
#include "ZZ/Generics/Map.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for CNF mapping:


void convertToAig(Gig& N)
{
    WMapX<GLit> xlat;
    xlat.initBuiltins();

    For_UpOrder(N, w){
        // Translate inputs:
        For_Inputs(w, v)
            w.set(Iter_Var(v), xlat[v]);

        // Turn gate into ANDs:
        switch (w.type()){
        case gate_PI:
        case gate_PO:
            xlat(w) = w;
            break;

        case gate_Not:
            xlat(w) = ~w[0];
            break;

        case gate_Conj:{
            Wire acc = N.True();
            For_Inputs(w, v)
                acc = mkAnd(acc, v);
            xlat(w) = acc;
            break; }

        case gate_Disj:{
            Wire acc = N.True();
            For_Inputs(w, v)
                acc = mkAnd(acc, ~v);
            xlat(w) = ~acc;
            break; }

        case gate_Odd:{
            Wire acc = N.True();
            For_Inputs(w, v)
                acc = mkXor(acc, ~v);
            xlat(w) = ~acc;
            break; }

        case gate_CardG:{
            // Temporary:
            assert(w.arg() == 2 || w.arg() == 3);
            assert(w.size() == 3 || w.size() == 4);
            if (w.size() == 3){
                if (w.arg() == 2){
                    Wire u0 = mkAnd(w[0], w[1]);
                    Wire u1 = mkAnd(w[0], w[2]);
                    Wire u2 = mkAnd(w[1], w[2]);
                    xlat(w) = mkOr(mkOr(u0, u1), u2);
                }else{
                    Wire u0 = mkAnd(mkAnd(w[0], w[1]), w[2]);
                    xlat(w) = u0;
                }

            }else{
                if (w.arg() == 2){
                    Wire u0 = mkAnd(w[0], w[1]);
                    Wire u1 = mkAnd(w[0], w[2]);
                    Wire u2 = mkAnd(w[0], w[3]);
                    Wire u3 = mkAnd(w[1], w[2]);
                    Wire u4 = mkAnd(w[1], w[3]);
                    Wire u5 = mkAnd(w[2], w[3]);
                    xlat(w) = mkOr(mkOr(mkOr(u0, u1), mkOr(u2, u3)), mkOr(u4, u5));
                }else{
                    Wire u0 = mkAnd(w[0], mkAnd(w[2], w[3]));
                    Wire u1 = mkAnd(w[1], mkAnd(w[2], w[3]));
                    Wire u2 = mkAnd(w[2], mkAnd(w[0], w[1]));
                    Wire u3 = mkAnd(w[3], mkAnd(w[0], w[1]));
                    xlat(w) = mkOr(mkOr(u0, u1), mkOr(u2, u3));
                }
            }
            break; }

        default:
            ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", w;
            assert(false);
        }
    }

    removeUnreach(N);
    N.strash();
    N.unstrash();
}


static
GLit pushNeg(Wire w, WMap<GLit>& pmap, WMap<GLit>& nmap)
{
    if (!w.sign){
        if (!pmap[w]){
            // Add AND gate:
            assert(w == gate_And);
            Wire wp = gig(w).add(gate_And);
            wp.set(0, pushNeg(w[0], pmap, nmap));
            wp.set(1, pushNeg(w[1], pmap, nmap));
            pmap(w) = wp;
        }
        return pmap[w];

    }else{
        if (!nmap[w]){
            // Add "OR" gate:
            assert(w == gate_And);
            Wire wn = gig(w).add(gate_And);
            wn.set(0, ~pushNeg(~w[0], pmap, nmap));
            wn.set(1, ~pushNeg(~w[1], pmap, nmap));
            nmap(w) = ~wn;
        }
        return nmap[w];
    }
}


// Assumes 'N' is an AIG; will push negations to the leaves, introducing ORs (coded as ANDs +
// inverters). Leaves will also be negation free; instead a new variable will be introduced
// for negative PIs (doubling the number of PIs).
void pushNegations(Gig& N)
{
    WMap<GLit> pmap, nmap;
    pmap(GLit_True) = GLit_True;
    nmap(GLit_True) = ~GLit_True;

    // Add PIs for negated original PIs:
    uint n_pis = N.enumSize(gate_PI);
    for (uint i = 0 ; i < n_pis; i++){
        Wire w = N(gate_PI, i);
        Wire w_neg = N.add(gate_PI);
        assert(w_neg.num() - w.num() == n_pis);

        pmap(w) = w;
        nmap(w) = w_neg;
    }


    For_Gatetype(N, gate_PO, w)
        w.set(0, pushNeg(w[0], pmap, nmap));

    N.compact();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constraints:


class BddCutOff {
    // Problem definition:
    Gig&        N;
    double      cutoff_lo;
    double      cutoff_hi;
    Vec<GLit>   vars;       // -- basic events as PIs in 'N'; should be sorted from smallest prob. to largest.
    Vec<double> costs;
    double      quanta;

    // Memoization:    
    Map<Pair<uint,double>, GLit> memo;
        // -- NOTE! The 'memo' is only doing work if some probabilities are repeated. This can be 
        // improved by preprocessing the probabilities.

    Wire build(uint idx, double sum, double material_left);

public:
    BddCutOff(Gig& N_, double cutoff_lo_, double cutoff_hi_, const Vec<GLit>& vars_, const Vec<double>& costs_, double quanta_);

    Wire top;   // -- after constructor is done, points to the top-node of the constraint (not protected by a PO)
};


BddCutOff::BddCutOff(Gig& N_, double cutoff_lo_, double cutoff_hi_, const Vec<GLit>& vars_, const Vec<double>& costs_, double quanta_) :
    N(N_), cutoff_lo(cutoff_lo_), cutoff_hi(cutoff_hi_), vars(copy_, vars_), costs(copy_, costs_), quanta(quanta_)
{
    assert(vars.size() == costs.size());

    if (quanta != 0){
        for (uint i = 0; i < costs.size(); i++)
            costs[i] = floor(costs[i] * quanta) / quanta;

        assert(cutoff_lo == 0);
        cutoff_hi = ceil(cutoff_hi * quanta) / quanta;
    }

    sobSort(ordByFirst(sob(costs), sob(vars)));

    //**/Dump(costs);
    //**/Dump(cutoff_lo, cutoff_hi);
    double material_left = 0;
    for (uint i = 0; i < costs.size(); i++)
        material_left += costs[i];

    top = build(vars.size(), 0, material_left);
}


Wire BddCutOff::build(uint idx, double sum, double material_left)
{
    double lo_lim = (cutoff_lo == DBL_MAX) ? DBL_MAX : cutoff_lo - sum;
    double hi_lim = (cutoff_hi == DBL_MAX) ? DBL_MAX : cutoff_hi - sum;

#if 0
    if (lo_lim <= 0 && hi_lim >= material_left)
        return N.True();
    else if (lo_lim > material_left || hi_lim < 0)
        return ~N.True();
#else
    const double eps = 1e-8;
    if (lo_lim <= eps && hi_lim >= material_left - eps)
        return N.True();
    else if (lo_lim > material_left - eps || hi_lim < eps)
        return ~N.True();
#endif
    // <<== need to handle numeric imprecision here!

    Pair<uint, double> key = tuple(idx, lo_lim);
    GLit* ret;

    //**/Dump(idx, sum, material_left, lo_lim, hi_lim, cutoff_lo, cutoff_hi);
    if (!memo.get(key, ret)){
        assert(idx > 0);
        idx--;
        Wire w1 = build(idx, sum + costs[idx], material_left - costs[idx]);
        Wire w0 = build(idx, sum             , material_left - costs[idx]);
        *ret = xig_Mux(vars[idx] + N, w1, w0);
    }
    //**/else putchar('.'), fflush(stdout);

    return *ret + N;
}


Wire addCutoff(Gig& N, double cutoff_lo, double cutoff_hi, const Vec<GLit>& vars, const Vec<double>& costs, double quanta)
{
    BddCutOff dummy(N, cutoff_lo, cutoff_hi, vars, costs, quanta);
    return dummy.top;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
