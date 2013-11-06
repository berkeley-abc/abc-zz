//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver2.cc
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
#include "Solver2.hh"
#include "ZZ_MetaSat.hh"
#include "ZZ_Gip.Common.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ/Generics/Heap.hh"
#include "Common.hh"

namespace ZZ {
using namespace std;


ZZ_PTimer_Add(fta_SAT);
ZZ_PTimer_Add(fta_Heap);
ZZ_PTimer_Add(fta_splitRegion);
ZZ_PTimer_Add(fta_splitRegion_shrink);
ZZ_PTimer_Add(fta_findPrime);
ZZ_PTimer_Add(fta_newRegion);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Supporting types:


struct Region {
    Cube   prime;
    Cube   bbox;
    double prob;    // -- product of probabilities of 'bbox'
    Region() : prob(-1) {}
    Region(Cube prime_, Cube bbox_, double prob_) : prime(prime_), bbox(bbox_), prob(prob_) {}
};

#if 1
macro bool operator<(const Region& r1, const Region& r2) {
    return r1.prob > r2.prob; }     // -- intentional '>'

#else
macro bool operator<(const Region& r1, const Region& r2) {
    if (r1.prob > r2.prob) return true;
    if (r1.prob < r2.prob) return false;
    uint d0 = r1.prime.size() - r1.bbox.size();
    uint d1 = r2.prime.size() - r2.bbox.size();
    return d0 > d1;
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


struct Fmt_Cube {
    const Vec<String>& var2name;
    const Cube& cube;
    Fmt_Cube(const Vec<String>& var2name_, const Cube& cube_) : var2name(var2name_), cube(cube_) {}
};


template<> fts_macro void write_(Out& out, const Fmt_Cube& v)
{
    out += '{';
    for (uint i = 0; i < v.cube.size(); i++){
        assert(!v.cube[i].sign);
        if (i != 0) out += ", ";
        out += v.var2name[v.cube[i].id];
    }
    out += '}';
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'FtaBound':


class FtaBound {
  //________________________________________
  //  Problem specification;

    const Gig&         N0;
    const Vec<double>& ev_probs;
    const Vec<String>& ev_names;

  //________________________________________
  //  State variables:

    Gig             N;

    KeyHeap<Region> open;
    Vec<Region>     closed;

    MiniSat2        S;
    WMapX<Lit>      n2s;

    MiniSat2        Z;
    WMapX<Lit>      n2z;

    uint            n_vars;     // -- number of events (= positive variables)
    Vec<Lit>        vars;       // -- positive events followed by negative events (so twice the size of 'ev_probs' and 'ev_names')
    Vec<double>     var_prob;   // -- Maps a SAT variable of an event to the probability corresponding to that event (undefined for other SAT variables)
    Vec<Lit>        flip;       // -- Maps a SAT variable of an event to the SAT variable of the negation of that event

  //________________________________________
  //  Internal methods:

    double upperProb();
    double lowerProb();

    void newRegion(Cube prime, Cube bbox);
    Cube findPrime(Cube bbox);
    void splitRegion(const Region& r);
    void approxTopEvent();

  //________________________________________
  //  Debug:

    Vec<String> var2name;
    Fmt_Cube fmt(const Cube& c) const { return Fmt_Cube(var2name, c); }

public:
  //________________________________________
  //  Public interface:

    FtaBound(const Gig& N0_, const Vec<double>& ev_probs_, const Vec<String>& ev_names_) :
        N0(N0_), ev_probs(ev_probs_), ev_names(ev_names_) {}

    void run();
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compute probabilities:


double FtaBound::lowerProb()
{
    double total = 0;
    for (uint i = 0; i < closed.size(); i++)
        total += closed[i].prob;
    return total;
}


double FtaBound::upperProb()
{
    const Vec<Region>& rs = open.base();
    double total = 0;
    for (uint i = 0; i < rs.size(); i++)    // <<== should do this using heap to make it numerically stable
        total += rs[i].prob;
    return total + lowerProb();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


// If 'prime' is non-NULL, add region to 'open' (unless 'prime == bbox').
// <<== later: also shrink bbox by SAT (if variable can be removed without losing solutions)
void FtaBound::newRegion(Cube prime, Cube bbox)
{
    ZZ_PTimer_Scope(fta_newRegion);

    if (!prime)
        return;

    double prob = 1;
    for (uint i = 0; i < bbox.size(); i++){
        assert(!bbox[i].sign);
        prob *= var_prob[bbox[i].id]; }

    if (subsumes(prime, bbox)){
        assert(prime == bbox);
        closed.push(Region(prime, prime, prob));
    }else{
      #if 0   // TEMPORARY
        Vec<Lit> assumps(copy_, bbox);
        for (uint i = 0; i < prime.size(); i++){
            if (has(bbox, prime[i])) continue;

            assumps.push(flip[prime[i].id]);
            if (S.solve(assumps) == l_True)
                assumps.pop();
            else{
                assumps.pop();
                assumps.push(prime[i]);
            }
        }
        if (assumps.size() > bbox.size()){
            WriteLn "Tighten BBox: %_ -> %_", fmt(bbox), fmt(assumps);
            bbox =  Cube(assumps);
        }
      #endif

        // <<== shrink bbox (probe by adding flipped prime literals missing from bbox; if that space is UNSAT the unflipped literal can be added to bbox)
        open.add(Region(prime, bbox, prob));
        //**/WriteLn "Added region with prob=%_; bbox=%_", prob, bbox;
    }
}


Cube FtaBound::findPrime(Cube bbox)
{
    ZZ_PTimer_Scope(fta_findPrime);

    Vec<Lit> assumps(copy_, bbox);
    ZZ_PTimer_Begin(fta_SAT);
    lbool result = S.solve(assumps);
    ZZ_PTimer_End(fta_SAT);
    if (result == l_False)
        return Cube_NULL;

    Vec<Lit> model;
    for (uint i = 0; i < vars.size(); i++)
        if (S.value(vars[i]) == l_True)
            model.push(vars[i]);

    // <<== try to remove literals here
    return Cube(model);
}


void FtaBound::splitRegion(const Region& r)
{
    ZZ_PTimer_Scope(fta_splitRegion);

    ZZ_PTimer_Begin(fta_splitRegion_shrink);
    // Shrink bounding-box with least probable literal in prime:
    double best_prob = DBL_MAX;
    Lit best_var;
    for (uint i = 0; i < r.prime.size(); i++){
        Lit p = r.prime[i]; assert(!p.sign);
        if (!has(r.bbox, p) && newMin(best_prob, var_prob[p.id]))
            best_var = p;
    }
    ZZ_PTimer_End(fta_splitRegion_shrink);

    //**/WriteLn "-- splitting on %_: bbox=%_  prime=%_", var2name[best_var.id], fmt(r.bbox), fmt(r.prime);
    newRegion(r.prime, r.bbox + Cube(best_var));

    Cube bbox = r.bbox + Cube(flip[best_var.id]);
    newRegion(findPrime(bbox), bbox);
}


void FtaBound::approxTopEvent()
{
    Vec<GLit> empty;    // <<== fix this later (add empty cube to 'Pack.hh')
    newRegion(findPrime(Cube(empty)), Cube(empty));

    uint iter = 0;
    while (open.size() > 0){
        ZZ_PTimer_Begin(fta_Heap);
        Region r = open.pop();
        ZZ_PTimer_End(fta_Heap);
        splitRegion(r);

        if (iter % 128 == 0 || open.size() == 0){
            char up[128];
            char lo[128];
            sprintf(up, "%g", upperProb());
            sprintf(lo, "%g", lowerProb());
            WriteLn "open: %_   closed: %_   upper: %_   lower: %_   [%t]", open.size(), closed.size(), up, lo, cpuTime();
        }

        iter++;
    }

#if 1   /*QUICK HACK FOR BARUCH*/
    WriteLn "Cover:";
    for (uint i = 0; i < closed.size(); i++)
        WriteLn "   %_", fmt(closed[i].bbox);
#endif  /*END DEBUG*/

}

// paper bound: 3.47065e-08

// paper: 0.0000000347065
// upper: 0.000000181161463500765   
// lower: 0.0000000284454046663883

// open: 10,722,642   closed: 54,349   upper: 3.6037e-08   lower: 3.43725e-08   [1:09 h]


void FtaBound::run()
{
    // Prepare fault-tree:
    N0.copyTo(N);
    WriteLn "FTA input : %_", info(N);

    convertToAig(N);
    WriteLn "FTA AIG   : %_", info(N);

    pushNegations(N);
    WriteLn "FTA unate : %_", info(N);

    Params_CnfMap P_cnf;
    P_cnf.quiet = true;
    P_cnf.map_to_luts = false;
    cnfMap(N, P_cnf);
    WriteLn "FTA mapped: %_", info(N);

    // Generate CNF:
    assert(N.enumSize(gate_PO) == 1);
    Lit top = clausify(N.enumGate(gate_PO, 0), S, n2s);

    n_vars = N0.enumSize(gate_PI);
    vars.growTo(2 * n_vars);
    var_prob.growTo(S.nVars());
    For_Gatetype(N, gate_PI, w){
        uint n = w.num();
        Lit  p = n2s[w]; assert(!p.sign);       // -- we don't want basic events to have a negated encoding (why should they?)
        vars[n] = p;
        var_prob[p.id] = (n < n_vars) ? ev_probs[n] : 1 - ev_probs[n - n_vars];
    }

#if 1   // -- quick hack
    for (uint n = 0; n < 2*n_vars; n++){
        if (!vars[n]){
            vars[n] = S.addLit();
            var_prob(vars[n].id) = (n < n_vars) ? ev_probs[n] : 1 - ev_probs[n - n_vars];
        }
    }
#endif

    flip.growTo(S.nVars());
    for (uint i = 0; i < n_vars; i++){
        flip[vars[i].id] = vars[i + n_vars];
        flip[vars[i + n_vars].id] = vars[i];
        //**/WriteLn "Flip: %_ <-> %_", vars[i], vars[i + n_vars];
    }

    S.addClause(top);   //  -- all SAT queries will require the top node to be TRUE
    for (uint i = 0; i < n_vars; i++)
        S.addClause(~vars[i], ~vars[i + n_vars]);

    // For debug:
    for (uint i = 0; i < n_vars; i++){
        var2name(vars[i].id) = ev_names[i];
        var2name(vars[i + n_vars].id) = String("~") + ev_names[i];
    }

    // Call approximator:
    approxTopEvent();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper function:


void ftaBound(const Gig& N, const Vec<double>& ev_probs, const Vec<String>& ev_names)
{
    FtaBound fb(N, ev_probs, ev_names);
    fb.run();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
