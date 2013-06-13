//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LutMap.cc
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "LutMap.hh"
#include "ZZ_Gig.hh"
#include "ZZ_BFunc.hh"
#include "ZZ/Generics/Sort.hh"
#include "Cut.hh"

#define DELAY_FRACTION 1.0      // -- 'arg' value of 'Delay' gates is divided by this value


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// LutMap class:


#define Cut LutMap_Cut
#define Cut_NULL Cut()


#define FUNC_TERMS 3


struct LutMapT_Expr {
    float   C;
    uint    a0;     // }- use 'UINT_MAX' for no metric 
    uint    a1;     // }
    LutMapT_Expr(float C_ = 0, uint a0_ = UINT_MAX, uint a1_ = UINT_MAX) : C(C_), a0(a0_), a1(a1_) {}
};


struct LutMapT_Ctrl {
    uint    cut_width;      // Usually 6, but perhaps smaller will work well for earlier phases? (esp. if we instantiate and simplify)
    bool    reuse_cuts;     // Use last cut set (must be FALSE for first phase)
    uint    cuts_per_node;  // If 'reuse_cuts' is set, must be same as in last phase
    uint    sticky;         // Keep this many cuts from the previous phase (best ones)
    float   alpha;          // Blending parameter for fanout estimation

    bool    shuffle;        // If true, randomize before sort
    float   xslack_activ;   // subtracted from required time for active nodes
    float   xslack_inact;   // subtracted from required time for inactive nodes
    bool    req_conserv;    // Conservative mode means required times for an inactive node must be at least as good as last iteration
    bool    dep_conserv;    // Departure times for inactive nodes are conservatively estimated (default: keep last active value)
    float   tsharp;         // Sharpness of threshold function; 'C' in 'atan(x*C)/pi + 0.5' and 'x' is slack, selecting between primary and secondary cost function
    uint    seco_cuts;      // If UINT_MAX, 'tsharp' is used to combine cost functions, else this many cuts are first taken from the *secondary* cost function, the rest from the primary. The order is PSSSPPPPP
    float   delay_factor;   // Target delay is optimal delay times this number (so should be >= 1)

    LutMapT_Expr prim[FUNC_TERMS];  // -- used when positive slack (unless 'seco_cuts == UINT_MAX')
    LutMapT_Expr seco[FUNC_TERMS];  // -- used when negative slack
};


struct LutMapT_Cost {
    // Node specific:
    float   slack;          // estimated slack
    float   depart_norm;    // normalized by dividing by total delay of current mapping

    // Cut specific:
    float   idx;            // 0, 1, 2...
    float   delay;          // arrival time, normalized to 0..1 for (0=min for all cuts of this node, 1=max)
    float   area;           // estimated area, normalized to 0..1 in the same way
    float   cut_size;       // number of inputs to the cut
    float   avg_fanout;     // total fanouts of fanins / cut size
};


#define N_METRICS (sizeof(LutMapT_Cost) / sizeof(float))


class LutMapT {
    typedef Params_LutMap Params;
    typedef LutMapT_Cost Cost;
    typedef LutMapT_Ctrl Ctrl;

    // Input:
    const Params&     P;
    Gig&              N;
    WSeen&            keep;

    // Script:
    Vec<Ctrl>         script;

    // State:
    SlimAlloc<Cut>    mem;
    WMap<Array<Cut> > cutmap;
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<float>       arrival;
    WMap<float>       depart;
    WMap<float>       prev_depart;
    WMap<uchar>       active;

    uint              round;
    Ctrl              C;
    uint64            cuts_enumerated;      // -- for statistics
    float             best_delay;
    float             target_delay;

    uint64            mapped_area;
    float             mapped_delay;

    uint64            seed;

    // Internal methods:
    void  prioritizeCuts(Wire w, Array<Cut> cuts);
    void  generateCuts_LogicGate(Wire w, Vec<Cut>& out);
    void  generateCuts(Wire w);
    void  updateFanoutEst(bool instantiate);
    void  run();

    // Temporaries:
    Vec<Cut>  tmp_cuts;
    Vec<Cost> tmp_costs;
    Vec<uint> tmp_where;
    Vec<uint> tmp_list;

public:

    LutMapT(Gig& N, Params_LutMap P, WSeen& keep);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


static
uint64 computeFtb(Wire w, const Cut& cut)
{
    if (w.id == gid_True)
        return w.sign ? 0ull : 0xFFFFFFFFFFFFFFFFull;

    for (uint i = 0; i < cut.size(); i++)
        if (w.id == cut[i])
            return ftb6_proj[w.sign][i];

    return (computeFtb(w[0], cut) & computeFtb(w[1], cut)) ^ (w.sign ? 0xFFFFFFFFFFFFFFFFull : 0ull);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
/*

====================
Globally:
====================

  - Initialize fanouts to:
     1. Unit
     2. Actual fanouts
     3. Fanouts after XOR/Mux extraction
     4. Fanouts of fanouts.

====================
Per round:
====================

  [bool]    recompute or reuse cuts?
  [uint]    cuts to keep (only if recomputing, else same as previous round)
  [uint]    max cut-width (probably always 6, but perhaps for first phase...)
  [float]   alpha coefficient for fanout est. blending
  [uint]    force keeping k best cuts from last round
  [sort]    cut sorting criteria
  [uint]    keep m cuts from secondary sorting criterita (put them last)
  [sort]    secondary cut sorting criteria
  [bool]    departure time estimation for inactive nodes (else req_time = last best)

Perhaps only allow for two or three sorting functions and then reuse them
if more phases (or put copying of these functions into mutation code).


====================
Sorting atoms:
====================

  Per cut:
    float   delay/arriv (or normalized delay [0..1])
    float   area        (normalized area [0..1])
    uint    cut_size
    float   avg_fanout

  For all cuts of a node:
    uint    artif_slack_inact  (not visible for cost function)
    uint    artif_slack_activ  (not visible for cost function)
    float   req_time    (normalized?)
    float   depart      (or 1 - norm_depart?)
    enum    mode             -- (1) normal, (2) randomize before sort, (3) use 'idx' as final tie-breaking

Lexicographical, weighted sum, sharp threshold, soft threshold (atan)

  one weighted function for within req_time, one for above, a threshold steepness param.

  expr := const * atom
        | const * atom * atom

  sum := expr + ... + expr


====================
Scoring:
====================

  - delay
  - area
  - runtime

Either limits on two, improve the third, or perhaps weighted percentual improvement over a reference
point.


====================
Later extensions:
====================

Re-expanding mapped LUTs in various ways (then 4-input LUT mapping will be more important,
esp. if combined with lut-strashing and const. propagation/simple rules).


*/
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut evalutation:


static
long double eval(const float metrics[N_METRICS], LutMapT_Expr func[FUNC_TERMS])
{
    long double sum = 0;
    for (uint i = 0; i < FUNC_TERMS; i++){
        long double prod = (func[i].a0 == UINT_MAX) ? 1 : metrics[func[i].a0];
        if (func[i].a1 != UINT_MAX)
            prod *= metrics[func[i].a1];
        sum += prod * func[i].C;
    }
    return sum;
}


struct Cut_Simple_lt {
    LutMapT_Expr* func;
    Cut_Simple_lt(LutMapT_Expr* func_) : func(func_) {}

    bool operator()(const LutMapT_Cost& x, const LutMapT_Cost& y) const {
        const float* xx = reinterpret_cast<const float*>(&x);
        const float* yy = reinterpret_cast<const float*>(&y);
        return eval(xx, func) < eval(yy, func);
    }
};


struct Cut_Composite_lt {
    LutMapT_Expr* prim;
    LutMapT_Expr* seco;
    float         tsharp;
    Cut_Composite_lt(LutMapT_Expr* prim_, LutMapT_Expr* seco_, float tsharp_) : prim(prim_), seco(seco_), tsharp(tsharp_) {}

    bool operator()(const LutMapT_Cost& x, const LutMapT_Cost& y) const {
#if 0   /*DEBUG*/
        //**/Dump(x.slack, x.delay, x.area, y.slack, y.delay, y.area);
        if (x.slack >= 0){
            if (y.slack >= 0)
                return x.area < y.area;
            else
                return true;
        }else{
            if (y.slack >= 0)
                return false;
            else
                return x.delay < y.delay;
        }
#endif  /*END DEBUG*/

#if 1
        LutMapT_Cost x_ = x;
        LutMapT_Cost y_ = y;
        if (x_.slack > 0) x_.delay /= 1e12; else x_.area /= 1e12;
        if (y_.slack > 0) y_.delay /= 1e12; else y_.area /= 1e12;
        const float* xx = reinterpret_cast<const float*>(&x_);
        const float* yy = reinterpret_cast<const float*>(&y_);
        return eval(xx, prim) < eval(yy, prim);
#endif

#if 0
        const float* xx = reinterpret_cast<const float*>(&x);
        const float* yy = reinterpret_cast<const float*>(&y);
        float alpha = atan(min_(x.slack, y.slack) * tsharp) / M_PI + 0.5;
        long double xeval = eval(xx, prim) * alpha  +  eval(xx, seco) * (1 - alpha);
        long double yeval = eval(yy, prim) * alpha  +  eval(yy, seco) * (1 - alpha);
        return xeval < yeval;
#endif
    }
};


void LutMapT::prioritizeCuts(Wire w, Array<Cut> cuts)
{
    assert(cuts.size() > 0);
    assert(fanout_est[w] > 0);

    // Setup cost vector:
    Vec<Cost>& costs = tmp_costs;
    costs.setSize(cuts.size());

    float min_delay = +FLT_MAX;
    float max_delay = -FLT_MAX;
    float min_area  = +FLT_MAX;
    float max_area  = -FLT_MAX;

    for (uint i = 0; i < cuts.size(); i++){
        costs[i].idx = i;
        costs[i].delay = 0.0f;
        costs[i].area  = 0.0f;
        costs[i].cut_size = cuts[i].size();
        costs[i].avg_fanout = 0.0f;

        for (uint j = 0; j < cuts[i].size(); j++){
            Wire w = cuts[i][j] + N;
            newMax(costs[i].delay, arrival[w]);
            costs[i].area += area_est[w];
            costs[i].avg_fanout += fanout_est[w];
        }
        costs[i].area += 1;    // -- unit LUT area
        costs[i].delay += 1;   // -- unit LUT delay 
        costs[i].avg_fanout /= cuts[i].size();

        newMin(min_delay, costs[i].delay);
        newMax(max_delay, costs[i].delay);
        newMin(min_area , costs[i].area );
        newMax(max_area , costs[i].area );
    }

    for (uint i = 0; i < cuts.size(); i++){
        if (round == 0 || (C.req_conserv && !active[w]))
            costs[i].slack = min_delay - costs[i].delay + C.xslack_inact;
        else{
            float req_time = target_delay - depart[w] + (active[w] ? C.xslack_activ : C.xslack_inact);
            costs[i].slack = req_time - costs[i].delay;
            //**/Dump(w, target_delay, costs[i].delay, depart[w], C.xslack_activ, costs[i].slack, (uint)active[w]);
        }
    }

    // Normalize delay and area (are "actual delay" and "actual area" up to this point):
    for (uint i = 0; i < costs.size(); i++){    // -- normalize delay and area
        costs[i].delay = (costs[i].delay - min_delay) / (max_delay - min_delay + 1.0/256);
        costs[i].area  = (costs[i].area  - min_area ) / (max_area  - min_area  + 1.0/256);
        costs[i].depart_norm = ((depart[w] != FLT_MAX) ? depart[w] : prev_depart[w]) / max_delay;   // <<== skall vi sätta den till FLT_MAX eller inte...
    }

    // Order cuts:
    if (C.shuffle)
        shuffle(seed, cuts);

    if (C.seco_cuts == UINT_MAX){
#if 0   /*DEBUG*/
        Write "cuts:";
        for (uint i = 0; i < costs.size(); i++)
            Write " (sl=%_, ar=%_, de=%_)", costs[i].slack, costs[i].area, costs[i].delay;
        NewLine;
#endif  /*END DEBUG*/
        sobSort(sob(costs, Cut_Composite_lt(C.prim, C.seco, C.tsharp)));
        //**/WriteLn "===";

    }else{
        Cut_Simple_lt prim_lt(C.prim);
#if 0   /*DEBUG*/
        Write "cuts:";
        for (uint i = 0; i < costs.size(); i++)
            Write " (sl=%_, ar=%_, de=%_)", costs[i].slack, costs[i].area, costs[i].delay;
        NewLine;
#endif  /*END DEBUG*/

      #if 0
        sobSort(sob(costs, prim_lt));
      #else
        Cut_Simple_lt seco_lt(C.seco);

        uint best = 0;
        for (uint i = 0; i < costs.size(); i++)
            if (prim_lt(costs[i], costs[best]))
                best = i;
        swp(costs[0], costs[best]);

        Array<Cost> pre = costs.slice(1);
        sobSort(sob(pre, seco_lt));

        if (1 + C.seco_cuts < costs.size()){
            Array<Cost> suf = costs.slice(1 + C.seco_cuts);
            sobSort(sob(suf, prim_lt));
        }
      #endif
    }

    // Implement order:
    Vec<uint>& where = tmp_where;
    Vec<uint>& list  = tmp_list;
    where.setSize(cuts.size());
    list .setSize(cuts.size());
    for (uint i = 0; i < cuts.size(); i++)
        where[i] = list[i] = i;

    for (uint i = 0; i < cuts.size(); i++){
        uint w = where[(uint)costs[i].idx];
        where[list[i]] = w;
        swp(list[i], list[w]);
        swp(cuts[i], cuts[w]);
    }

    // Store area flow and arrival time:
    float area  = 0;
    float delay = 0;
    for (uint j = 0; j < cuts[0].size(); j++){
        Wire w = cuts[0][j] + N;
        newMax(delay, arrival[w]);
        area += area_est[w];
    }

    area_est(w) = area + 1;     // -- unit LUT area
    arrival(w)  = delay + 1;    // -- unit LUT delay
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


#define LutMap LutMapT
#include "LutMap_CutGen.icc"
#undef LutMap


void LutMapT::generateCuts(Wire w)
{
    switch (w.type()){
    case gate_Const:    // -- constants should really have been propagated before mapping, but let's allow for them
    case gate_Reset:    // -- not used, but is part of each netlist
    case gate_Box:      // -- treat sequential boxes like a global source
    case gate_PI:
    case gate_FF:
        // Base case -- Global sources:
        cutmap(w) = Array<Cut>(empty_);     // -- only the trivial cut
        area_est(w) = 0;
        arrival(w) = 0;
        break;

    case gate_And:
    case gate_Lut4:
        // Inductive case:
        if (!cutmap[w]){
            Vec<Cut>& cuts = tmp_cuts;
            cuts.clear();
            generateCuts_LogicGate(w, cuts);
            cuts_enumerated += cuts.size();
            prioritizeCuts(w, cuts.slice());
                // <<== måste lägga in "legacy cuts"
            cuts.shrinkTo(P.cuts_per_node);
            cutmap(w) = Array_copy(cuts, mem);
        }else
            prioritizeCuts(w, cutmap[w]);
        break;

    case gate_PO:
    case gate_Seq:
        // Nothing to do.
        break;

    case gate_Sel:
        // Treat pin selectors as PIs except for delay:
        cutmap(w) = Array<Cut>(empty_);
        area_est(w) = 0;
        arrival(w) = arrival[w[0]];
        break;

    case gate_Delay:{
        // Treat delay gates as PIs except for delay:
        cutmap(w) = Array<Cut>(empty_);
        area_est(w) = 0;
        float arr = 0;
        For_Inputs(w, v)
            newMax(arr, arrival[v]);
        arrival(w) = arr + w.arg() / DELAY_FRACTION;
        break;}

    default:
        ShoutLn "INTERNAL ERROR! Unhandled gate type: %_", w.type();
        assert(false);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Fanout estimation:


// Updates:
//
//   - depart
//   - fanout_est
//   - mapped_area
//   - mapped_delay
//
void LutMapT::updateFanoutEst(bool instantiate)
{
    // Compute the fanout count for graph induced by mapping:
    WMap<uint> fanouts(N, 0);
    fanouts.reserve(N.size());

    For_Gates(N, w)
        if (depart[w] != FLT_MAX)
            prev_depart(w) = depart[w];

    depart.clear();
    mapped_area = 0;
    For_All_Gates_Rev(N, w){
        if (w == gate_And || w == gate_Lut4){
            if (fanouts[w] > 0){
                const Cut& cut = cutmap[w][0];
                mapped_area += 1;       // -- LUT cost = 1

                for (uint i = 0; i < cut.size(); i++){
                    Wire v = cut[i] + N;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);
                }
                active(w) = true;
            }else{
                // <<== depart. est. heuristic goes here (leave last, reset, conservative etc.)
                active(w) = false;
            }

        }else if (!isCI(w)){
            float delay = (w != gate_Delay) ? 0.0f : w.arg() / DELAY_FRACTION;
            For_Inputs(w, v){
                fanouts(v)++;
                newMax(depart(v), depart[w] + delay);
            }
        }
    }

    mapped_delay = 0.0f;
    For_Gates(N, w)
        if (isCI(w))
            newMax(mapped_delay, depart[w]);

    if (!instantiate){
        // Blend new values with old:
        uint  r = round + 1.0f;
        float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 1.0f);
        float beta  = 1.0f - alpha;

        For_Gates(N, w){
            if (w == gate_And){
                fanout_est(w) = alpha * max_(fanouts[w], 1u)
                              + beta  * fanout_est[w];
                              //+ beta  * fanout_count[w];
            }
        }

    }else{
        // Compute FTBs:
        Vec<uint64> ftbs(reserve_, mapped_area);
        For_Gates(N, w){
            if (w == gate_And && depart[w] != FLT_MAX){
                const Cut& cut = cutmap[w][0];
                ftbs += computeFtb(w, cut);
            }
        }

        // Build LUT representation:
        N.thaw();
        N.setMode(gig_FreeForm);
        uint j = 0;
        For_Gates(N, w){
            if (w == gate_And && depart[w] != FLT_MAX){
                // Change AND gate into a LUT6:
                const Cut& cut = cutmap[w][0];
                change(w, gate_Lut6);
                ftb(w) = ftbs[j++];
                for (uint i = 0; i < cut.size(); i++)
                    w.set(i, cut[i] + N);
            }
        }

        For_Gates_Rev(N, w)
            if (w == gate_And)
                remove(w);
        N.compact();
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void LutMapT::run()
{
    round        = 0;
    best_delay   = FLT_MAX;
    target_delay = FLT_MAX;
    mapped_area  = UINT64_MAX;
    seed         = DEFAULT_SEED;

    area_est   .reserve(N.size());
    fanout_est .reserve(N.size());
    active     .reserve(N.size());
    depart     .reserve(N.size());
    prev_depart.reserve(N.size());

    // Initialize maps:
    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            fanout_est(w) = nFanouts(w);
            active    (w) = true;
            if (w == gate_And || w == gate_Lut4){
                For_Inputs(w, v)
                    newMax(arrival(w), arrival[v] + 1.0f); }
        }

        For_All_Gates_Rev(N, w)
            if (w == gate_And || w == gate_Lut4){
                For_Inputs(w, v)
                    newMax(depart(v), depart[w] + 1.0f); }

        mapped_delay = 0.0f;
        For_Gates(N, w)
            if (isCI(w))
                newMax(mapped_delay, depart[w]);

        mapped_area = N.typeCount(gate_And) + N.typeCount(gate_Lut4);
    }

    // Techmap:
    for (round = 0; round < P.n_rounds; round++){
        C = script[round];

        double T0 = cpuTime();
        cuts_enumerated = 0;
        For_All_Gates(N, w)
            generateCuts(w);
        double T1 = cpuTime();

        bool instantiate = (round == P.n_rounds - 1);
        updateFanoutEst(instantiate);
        double T2 = cpuTime();

        newMin(best_delay, mapped_delay);
        if (round == 0)
            target_delay = mapped_delay * C.delay_factor;

        if (!P.quiet){
            if (round == 0)
                WriteLn "cuts_enumerated=%,d", cuts_enumerated;
            WriteLn "round=%d   mapped_area=%,d   mapped_delay=%_   [enum: %t, blend: %t]", round, mapped_area, mapped_delay, T1-T0, T2-T1;
        }

      #if 1
//        if (round == 0)
        {
            for (uint i = 0; i < cutmap.base().size(); i++)
                dispose(cutmap.base()[i], mem);
            cutmap.clear();
        }
      #endif
    }
}


LutMapT::LutMapT(Gig& N_, Params_LutMap P_, WSeen& keep_) :
    P(P_), N(N_), keep(keep_)
{
    if (!N.isCanonical()){
        WriteLn "Compacting... %_", info(N);
        N.compact();
        WriteLn "Done... %_", info(N);
    }

    // Temporary initialization of the script:
    {
        script.push();
        script[LAST].cut_width = 6;
        script[LAST].reuse_cuts = false;
        script[LAST].cuts_per_node = 10;
        script[LAST].sticky = 0;
        script[LAST].alpha = 0.5;
        script[LAST].shuffle = false;
        script[LAST].xslack_activ = 0;
        script[LAST].xslack_inact = 0;
        script[LAST].req_conserv = true;
        script[LAST].dep_conserv = false;
        script[LAST].tsharp = 256.0;
        script[LAST].seco_cuts = 1;
        script[LAST].delay_factor = 100;
        script[LAST].prim[0] = LutMapT_Expr(1e16, 3, UINT_MAX);      // 3=delay, 4=area
        script[LAST].prim[1] = LutMapT_Expr(   1, 4, UINT_MAX);
        script[LAST].seco[0] = LutMapT_Expr(   1, 3, UINT_MAX);
        script[LAST].seco[1] = LutMapT_Expr(   1, 4, UINT_MAX);

        script.push(script[LAST]);
        script[LAST].req_conserv = false;
        swp(script[LAST].prim[0], script[LAST].seco[0]); swp(script[LAST].prim[1], script[LAST].seco[1]); swp(script[LAST].prim[2], script[LAST].seco[2]);
        script[LAST].seco_cuts = UINT_MAX;
        script[LAST].alpha = 0.941176;

        script.push(script[LAST]);
        script[LAST].alpha = 0.987805;

        script.push(script[LAST]);
        script[LAST].alpha = 0.996109;
    }

    run();

    // Free memory:
    for (uint i = 0; i < cutmap.base().size(); i++)
        dispose(cutmap.base()[i], mem);
    mem.clear(false);

    area_est  .clear(true);
    fanout_est.clear(true);
}


// Wrapper function:
void lutMapTune(Gig& N, Params_LutMap P, WSeen* keep)
{
    WSeen keep_dummy;
    if (!keep)
        keep = &keep_dummy;

    LutMapT inst(N, P, *keep);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}

/*
Prova att spara 'depart' från iteration till iteration så att omappade noder har en bättre gissning.
Markera omappade noder på annat sätt.
*/

/*
Om inte omgenererar cuts, spara hälften delay-opt, hälften area-opt?
Om växlar snabba (återanvänder cuts) och långsamma faser (nya cuts), hantera fanout-est blendingen annorlunda.
Om genererar nya cuts, spara bästa (eller bästa k stycken) cut från förra fasen.

Departure est. för omappade noder: välj ett cut i termer av mappade noder (om existerar) och tag max: ELLER
loopa över alla cuts i alla instansierade noder och ta max.

Parametrar:
  - arrival-time range
  - area-est range
  - est. slack (incl. "unknown" as a boolean)

+ previous best choice? Or not needed?

Major rounds:
  - blending ratio
  - reuse cuts (boolean)

*/

/*
delay optimal everywhere
globally delay optimal, use slack for area recovery
*/
