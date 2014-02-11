//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
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
#include "ZZ_Gig.hh"
#include "ZZ/Generics/Map.hh"
#include "ZZ/Generics/Sort.hh"

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
            if (w.arg() > w.size())
                xlat(w) = ~GLit_True;
            else if (w.arg() == 0)
                xlat(w) = GLit_True;
            else{
                Vec<GLit> fs(copy_, w.fanins());
                oddEvenSort(N, fs);
                xlat(w) = fs[w.arg() - 1];
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
// Cut-off constraint:


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
