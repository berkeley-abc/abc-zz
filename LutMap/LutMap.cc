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


struct LutMap_Cost {
    uint    idx;
    float   delay;
    float   area;
    uint    cut_size;
    float   avg_fanout;
};


class LutMap {
    typedef LutMap_Cost Cost;

    // Input:
    const Params_LutMap& P;
    Gig&                 N;
    WSeen&               keep;

    // State:
    SlimAlloc<Cut>    mem;
    WMap<Array<Cut> > cutmap;
    WMap<Cut>         winner;
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<float>       arrival;
    WMap<float>       depart;               // -- 'FLT_MAX' marks a deactivated node
    WMap<uchar>       active;

    uint              round;
    uint64            cuts_enumerated;      // -- for statistics
    float             target_arrival;
    float             best_arrival;

    uint64            mapped_area;
    float             mapped_delay;

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

    LutMap(Gig& N, Params_LutMap P, WSeen& keep);
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

Perhaps only allow for two or three sorting functions and then reuse them
if more phases (or put copying of these functions into mutation code).


====================
Sorting atoms:
====================

  Per cut:
    float   delay       (or normalized delay [0..1])
    float   area        (normalized area [0..1])
    uint    cut_size
    float   avg_fanout

  For all cuts of a node:
    float   req_time
    uint    level, rev_level (and normalized versions)
    enum    mode             -- (1) normal, (2) randomize before sort, (3) use 'idx' as final tie-breaking

Lexicographical, weighted sum, sharp threshold, soft threshold (atan)

    (a < b * C) ?[D]


            req_time = (depart[w] == FLT_MAX) ? FLT_MAX : target_arrival - (depart[w] + 1);
            req_time = (depart[w] == FLT_MAX) ? costs[0].delay + 1 : target_arrival - (depart[w] + 1);  // -- give one unit of artificial slack



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


struct Delay_lt {
    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const {
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        if (x.avg_fanout > y.avg_fanout) return true;
        if (x.avg_fanout < y.avg_fanout) return false;
        return false;
//        return x.cut_size < y.cut_size;
    }
};


struct Area_lt {
    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const {
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
        if (x.avg_fanout > y.avg_fanout) return true;
        if (x.avg_fanout < y.avg_fanout) return false;
        return false;
//        return x.cut_size < y.cut_size;
    }
};


macro float sq(float x) { return x*x; }

struct Weighted_lt {
    float min_delay;
    float max_delay;
    float min_area;
    float max_area;
    float req_time;
    uint  round;
    Weighted_lt(float min_delay_, float max_delay_, float min_area_, float max_area_, float req_time_, uint round_) : min_delay(min_delay_), max_delay(max_delay_), min_area(min_area_), max_area(max_area_), req_time(req_time_), round(round_) {}

    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const
    {
        float xa = (x.area  - min_area ) / (max_area  - min_area + 1);
        float xd = (x.delay - min_delay) / (max_delay - min_delay + 1);
        float ya = (y.area  - min_area ) / (max_area  - min_area + 1);
        float yd = (y.delay - min_delay) / (max_delay - min_delay + 1);

        if (req_time == FLT_MAX)
//            return xa/1024 + xd < ya/1024 + yd;
            return xd < yd;

        else{
            xd *= atan((x.delay - req_time) * 4) / M_PI + 0.5;
            yd *= atan((y.delay - req_time) * 4) / M_PI + 0.5;

            return xa + xd*4 < ya + yd*4;
        }
        //return sq(xa + xd) < sq(ya + yd);
        //return sqrt(xa) + sqrt(xd) < sqrt(ya) + sqrt(yd);
    }
};


static void sortCuts(Vec<LutMap_Cost>& costs, uint round, float req_time) ___unused;
static void sortCuts(Vec<LutMap_Cost>& costs, uint round, float req_time)
{
    float min_delay = +FLT_MAX;
    float max_delay = -FLT_MAX;
    float min_area  = +FLT_MAX;
    float max_area  = -FLT_MAX;
    for (uint i = 0; i < costs.size(); i++){
        newMin(min_delay, costs[i].delay);
        newMax(max_delay, costs[i].delay);
        newMin(min_area , costs[i].area );
        newMax(max_area , costs[i].area );
    }

    Weighted_lt lt(min_delay, max_delay, min_area, max_area, req_time, round);
    sobSort(sob(costs, lt));
}


void LutMap::prioritizeCuts(Wire w, Array<Cut> cuts)
{
    assert(cuts.size() > 0);
    assert(fanout_est[w] > 0);

    // Setup cost vector:
    Vec<Cost>& costs = tmp_costs;
    costs.setSize(cuts.size());

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
        costs[i].area += 1;     // -- LUT cost = 1
        costs[i].avg_fanout /= cuts[i].size();
    }

    // Compute order:
#if 1
    sobSort(sob(costs, Delay_lt()));
    if (round > 0){
        float req_time;
      #if 0
        assert((depart[w] != FLT_MAX) == active[w]);
        if (P.map_for_area)
            req_time = (depart[w] == FLT_MAX) ? FLT_MAX : target_arrival - (depart[w] + 1);
        else
            req_time = (depart[w] == FLT_MAX) ? costs[0].delay + 1 : target_arrival - (depart[w] + 1);  // -- give one unit of artificial slack
      #else
        assert(depart[w] != FLT_MAX);
        if (P.map_for_area)
            req_time = target_arrival - (depart[w] + 1);
        else
            req_time = target_arrival - (depart[w] + 1) - (uint)active[w];
      #endif

        uint j = 0;
        for (uint i = 0; i < costs.size(); i++)
            if (costs[i].delay <= req_time)
                swp(costs[i], costs[j++]);

        Array<Cost> pre = costs.slice(0, j);
        sobSort(sob(pre, Area_lt()));

        uint n_delay_cuts = uint(P.cuts_per_node * 0.3) + 1;
        Array<Cost> suf = costs.slice(min_(j, costs.size() - n_delay_cuts));
        sobSort(sob(suf, Delay_lt()));
    }
#else
    //float req_time = (round == 0 || depart[w] == FLT_MAX) ? FLT_MAX : target_arrival - (depart[w] + 1);
    float best_delay = FLT_MAX;
    for (uint i = 0; i < costs.size(); i++)
        newMin(best_delay, costs[i].delay);
    float req_time = (round == 0)           ? FLT_MAX :
                     (depart[w] == FLT_MAX) ? best_delay :
                     /*otherwise*/            target_arrival - (depart[w] + 1);

    sortCuts(costs, round, req_time);
#endif

    // Implement order:
    Vec<uint>& where = tmp_where;
    Vec<uint>& list  = tmp_list;
    where.setSize(cuts.size());
    list .setSize(cuts.size());
    for (uint i = 0; i < cuts.size(); i++)
        where[i] = list[i] = i;

    for (uint i = 0; i < cuts.size(); i++){
        uint w = where[costs[i].idx];
        where[list[i]] = w;
        swp(list[i], list[w]);
        swp(cuts[i], cuts[w]);
    }

    // Store area flow and arrival time:
    area_est(w) = costs[0].area / fanout_est[w];
    arrival(w) = costs[0].delay + 1.0f;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


#include "LutMap_CutGen.icc"


void LutMap::generateCuts(Wire w)
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
            cuts.clear();   // -- keep last winner
            if (!winner[w].null())
                cuts.push(winner[w]);
            generateCuts_LogicGate(w, cuts);
            cuts_enumerated += cuts.size();
            prioritizeCuts(w, cuts.slice());
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
void LutMap::updateFanoutEst(bool instantiate)
{
    // Compute the fanout count for graph induced by mapping:
    WMap<uint> fanouts(N, 0);
    fanouts.reserve(N.size());

    mapped_area = 0;
    depart.clear();

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
                depart(w) = FLT_MAX;    // -- marks deactivated node
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

#if 1   // -- temporary solution for computing estimated departure for inactive nodes
    WMap<float> tmpdep;
    depart.copyTo(tmpdep);
    For_Gates_Rev(N, w){
        if (isCI(w)) continue;
        if (tmpdep[w] == FLT_MAX) continue;

        //if (w != gate_And && w != gate_Lut4) continue;
        For_Inputs(w, v){
            if (v != gate_And && v != gate_Lut4) continue;
            if (depart[v] != FLT_MAX) continue;

            if (tmpdep[v] == FLT_MAX) tmpdep(v) = 0;
            newMax(tmpdep(v), tmpdep[w]);
        }
    }

    For_Gates(N, w)
        if (depart[w] == FLT_MAX && tmpdep[w] != FLT_MAX)
            depart(w) = tmpdep[w];
#endif

    mapped_delay = 0.0f;
    For_Gates(N, w)
        if (isCI(w))
            newMax(mapped_delay, depart[w]);

    if (!instantiate){
        // Blend new values with old:
        uint  r = round + 1.0f;
//        float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 1.0f);
        float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 2.0f);
        float beta  = 1.0f - alpha;

        For_Gates(N, w){
            if (w == gate_And || w == gate_Lut4){
//                fanout_est(w) = alpha * max_(fanouts[w], 1u)
                fanout_est(w) = alpha * max_(double(fanouts[w]), 0.95)   // -- slightly less than 1 leads to better delay
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


void LutMap::run()
{
    round = 0;
    best_arrival = 0;

    area_est  .reserve(N.size());
    fanout_est.reserve(N.size());
    active    .reserve(N.size());
    depart    .reserve(N.size());

    // Initialize fanout estimation (and zero area estimation):
    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            area_est  (w) = 0;
            fanout_est(w) = nFanouts(w);
            active    (w) = true;
            depart    (w) = FLT_MAX;
        }
    }

    // Techmap:
    for (round = 0; round < P.n_rounds; round++){
        double T0 = cpuTime();
        cuts_enumerated = 0;
        For_All_Gates(N, w)
            generateCuts(w);
        double T1 = cpuTime();

        bool instantiate = (round == P.n_rounds - 1);
        updateFanoutEst(instantiate);
        double T2 = cpuTime();

        if (round == 0)
            target_arrival = mapped_delay * P.delay_factor;
        newMin(best_arrival, mapped_delay);

        if (!P.quiet){
            if (round == 0)
                WriteLn "cuts_enumerated=%,d", cuts_enumerated;
            WriteLn "round=%d   mapped_area=%,d   mapped_delay=%_   [enum: %t, blend: %t]", round, mapped_area, mapped_delay, T1-T0, T2-T1;
        }

      #if 1
        if (round == 0){
            winner.clear();
            For_Gates(N, w)
                if ((w == gate_And || w == gate_Lut4) && cutmap[w].size() > 0)
                    winner(w) = cutmap[w][0];

            for (uint i = 0; i < cutmap.base().size(); i++)
                dispose(cutmap.base()[i], mem);
            cutmap.clear();
        }
      #endif

      #if 0
        if (round == P.n_rounds - 2)
            target_arrival = best_arrival;
      #endif
    }
}


LutMap::LutMap(Gig& N_, Params_LutMap P_, WSeen& keep_) :
    P(P_), N(N_), keep(keep_)
{
    if (!N.isCanonical()){
        WriteLn "Compacting... %_", info(N);
        N.compact();
        WriteLn "Done... %_", info(N);
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
void lutMap(Gig& N, Params_LutMap P, WSeen* keep)
{
    WSeen keep_dummy;
    if (!keep)
        keep = &keep_dummy;

    LutMap inst(N, P, *keep);
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
