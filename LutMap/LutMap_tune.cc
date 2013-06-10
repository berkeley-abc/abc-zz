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

struct LutMapT_Expr {
    float   C;
    uint    a0;
    uint    a1;
};


struct LutMapT_Params {
    bool    reuse_cuts;     // Use last cut set (must be FALSE for first phase)
    uint    cuts_per_node;  // If 'reuse_cuts' is set, must be same as in last phase
    uint    sticky_prim;    // Keep this many cuts from primary sorting criteria of the previous phase (best ones)
    uint    sticky_seco;    // Keep this many cuts from secondary sorting criteria ofthe previous phase (best ones)
    uint    cut_width;      // Usually 6, but perhaps smaller will work well for earlier phases? (esp. if we instantiate and simplify)
    float   alpha;          // Blending parameter for fanout estimation

    uint    mode;           // 0=normal, 1=randomize before sort, 2=use cut index for tie-breaking
    float   xslack_activ;   // subtracted from required time for active nodes
    float   xslack_inact;   // subtracted from required time for inactive nodes
    bool    req_conserv;    // Conservative mode means required times means inactive node must be at least as good as last iteration
    bool    dep_conserv;    // Departure times for inactive nodes are conservatively estimated (default: keep last active value)
    float   tsharp;         // Sharpness of threshold function; 'C' in 'tan(x*C)/pi + 0.5'
    float   delay_factor;   // Target delay is optimal delay times this number (so should be >= 1)

    LutMapT_Expr prim[3];
    LutMapT_Expr seco[3];
};


struct LutMapT_Cost {
    // Node specific:
    float   req_time;       // estimated required time
    float   depart_norm;    // normalized by dividing by total delay of current mapping

    // Cut specific:
    float   idx;
    float   delay_norm;     // arrival time, normalized to 0..1 for (0=min for all cuts of this node, 1=max)
    float   area_norm;      // estimated area, normalized to 0..1 in the same way
    float   cut_size;
    float   avg_fanout;
};


class LutMapT {
    typedef LutMapT_Cost   Cost;
    typedef LutMapT_Params Params;

    // Input:
    const Params_LutMap& P;
    Gig&                 N;
    WSeen&               keep;

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
    uint64            cuts_enumerated;      // -- for statistics
    float             best_delay;
    float             target_delay;

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


void LutMapT::prioritizeCuts(Wire w, Array<Cut> cuts)
{
#if 0
    assert(cuts.size() > 0);
    assert(fanout_est[w] > 0);

    // Setup cost vector:
    Vec<Cost>& costs = tmp_costs;
    costs.setSize(cuts.size());

            req_time = (depart[w] == FLT_MAX) ? FLT_MAX : target_arrival - (depart[w] + 1);

    for (uint i = 0; i < cuts.size(); i++){
        costs[i].idx = i;
        costs[i].delay_norm = 0.0f;
        costs[i].area_norm  = 0.0f;
        costs[i].cut_size = cuts[i].size();
        costs[i].avg_fanout = 0.0f;

        costs[i].depart_norm = ((depart[w] != FLT_MAX) ? depart[w] : prev_depart[w]) / max_delay;
        costs[i].req_time    = ...;

        for (uint j = 0; j < cuts[i].size(); j++){
            Wire w = cuts[i][j] + N;
            newMax(costs[i].delay_norm, arrival[w]);
            costs[i].area_norm += area_est[w];
            costs[i].avg_fanout += fanout_est[w];
        }
        costs[i].area += 1;     // -- LUT cost = 1
        costs[i].avg_fanout /= cuts[i].size();
    }

    float min_delay = +FLT_MAX;
    float max_delay = -FLT_MAX;
    float min_area  = +FLT_MAX;
    float max_area  = -FLT_MAX;
    for (uint i = 0; i < costs.size(); i++){
        newMin(min_delay, costs[i].delay_norm);
        newMax(max_delay, costs[i].delay_norm);
        newMin(min_area , costs[i].area_norm );
        newMax(max_area , costs[i].area_norm );
    }
    for (uint i = 0; i < costs.size(); i++){    // -- normalize delay and area
        costs[i] = (costs[i] - min_delay) / (max_delay - min_delay);
        costs[i] = (costs[i] - min_area ) / (max_area  - min_area );
    }

    // Compute order:
#if 1
    sobSort(sob(costs, Delay_lt()));
    if (round > 0){
        float req_time;
        assert((depart[w] != FLT_MAX) == active[w]);
        if (P.map_for_area)
            req_time = (depart[w] == FLT_MAX) ? FLT_MAX : target_arrival - (depart[w] + 1);
        else
            req_time = (depart[w] == FLT_MAX) ? costs[0].delay + 1 : target_arrival - (depart[w] + 1);  // -- give one unit of artificial slack

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
#endif
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
    round = 0;

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
            target_delay = mapped_delay * P.delay_factor;

        if (!P.quiet){
            if (round == 0)
                WriteLn "cuts_enumerated=%,d", cuts_enumerated;
            WriteLn "round=%d   mapped_area=%,d   mapped_delay=%_   [enum: %t, blend: %t]", round, mapped_area, mapped_delay, T1-T0, T2-T1;
        }

      #if 1
        if (round == 0)
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
