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
#include "ZZ_Npn4.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Heap.hh"
#include "Cut.hh"

#define DELAY_FRACTION 1.0      // -- 'arg' value of 'Delay' gates is divided by this value


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


macro bool isLogic(Wire w) {
    return w == gate_And || w == gate_Lut4;
}


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
    WMapX<GLit>*         remap;

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
    void  reprioritizeCuts(Wire w, Array<Cut> cuts);
    void  generateCuts_LogicGate(Wire w, Vec<Cut>& out);
    void  generateCuts(Wire w);
    void  updateFanoutEst(bool instantiate);
    void  run();

    // Temporaries:
    Vec<Cut>     tmp_cuts;
    Vec<Cost>    tmp_costs;
    Vec<uint>    tmp_where;
    Vec<uint>    tmp_list;
    WZet         in_memo;
    WMap<uint64> memo;

    bool probeRound() const { return P.map_for_delay && round == 0; }
    bool delayRound() const { return P.map_for_delay ? round == 1 : round == 0; }

public:

    LutMap(Gig& N, Params_LutMap P, WSeen& keep, WMapX<GLit>* remap);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


static
uint64 computeFtb_(Wire w, const Cut& cut, WZet& in_memo, WMap<uint64>& memo)
{
    uint64 imask = (w.sign ? 0xFFFFFFFFFFFFFFFFull : 0ull);

    if (w.id == gid_True)
        return ~imask;

    if (in_memo.has(w))
        return memo[w] ^ imask;

    uint64 ret;
    for (uint i = 0; i < cut.size(); i++)
        if (w.id == cut[i]){
            ret = ftb6_proj[0][i];
            goto Found; }
    /*else*/
    {
        if (w == gate_And)
            ret = computeFtb_(w[0], cut, in_memo, memo) & computeFtb_(w[1], cut, in_memo, memo);
        else if (w == gate_Lut4){
            uint64 ftb[4];
            for (uint i = 0; i < 4; i++)
                ftb[i] = w[i] ? computeFtb_(w[i], cut, in_memo, memo) : 0ull;

            ret = 0;
            for (uint64 m = 1; m != 0; m <<= 1){
                uint b = uint(bool(ftb[0] & m)) | (uint(bool(ftb[1] & m)) << 1) | (uint(bool(ftb[2] & m)) << 2) | (uint(bool(ftb[3] & m)) << 3);
                if (w.arg() & (1 << b))
                    ret |= m;
            }

        }else
            assert(false);
    }Found:;

    memo(w) = ret;
    in_memo.add(w);

    return ret ^ imask;
}


static
uint64 computeFtb(Wire w, const Cut& cut, WZet& in_memo, WMap<uint64>& memo)
{
    uint64 ret = computeFtb_(w, cut, in_memo, memo);
    in_memo.clear();
    return ret;
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
        //**/if (x.cut_size < y.cut_size) return true;
        //**/if (x.cut_size > y.cut_size) return false;
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
    sobSort(sob(costs, Delay_lt()));
    float req_time = 0;
    if (!probeRound() && !delayRound()){
        assert(depart[w] != FLT_MAX);
        req_time = target_arrival - (depart[w] + 1);

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


// Applied during instantiation where some 'area_est[w]' is set to zero.
void LutMap::reprioritizeCuts(Wire w, Array<Cut> cuts)
{
    float best_delay = FLT_MAX;
    float best_area  = FLT_MAX;
    uint  best_i = 0;

    for (uint i = 0; i < cuts.size(); i++){
        float this_delay = 0.0f;
        float this_area  = 0.0f;
        for (uint j = 0; j < cuts[i].size(); j++){
            Wire v = cuts[i][j] + N;
            this_area += area_est[v];
            newMax(this_delay, arrival[v]);
        }

        if (i == 0){
            best_delay = this_delay;
            best_area  = this_area;
        }else if (this_delay > best_delay)
            break;

        if (newMin(best_area, this_area))
            best_i = i;
    }

    if (best_i != 0)
        swp(cuts[0], cuts[best_i]);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


#include "LutMap_CutGen.icc"


static
float delay(const Cut& cut, const WMap<float>& arrival, Gig& N)
{
    float arr = 0.0f;
    for (uint i = 0; i < cut.size(); i++){
        Wire w = cut[i] + N;
        newMax(arr, arrival[w]);
    }
    return arr + 1.0f;
}


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

            if (!probeRound()){
                cuts.shrinkTo(P.cuts_per_node);
            }else{
                for (uint i = 1; i < cuts.size(); i++){
                    if (delay(cuts[i], arrival, N) > delay(cuts[0], arrival, N))
                        cuts.shrinkTo(i);
                }
                cuts.shrinkTo(2 * P.cuts_per_node);
            }
            cutmap(w) = Array_copy(cuts, mem);
        }else
            prioritizeCuts(w, cutmap[w]);
        break;

    case gate_PO:
    case gate_Seq:
        // Nothing to do.
        break;

    case gate_Bar:
    case gate_Sel:
        // Treat barriers and pin selectors as PIs except for delay:
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


struct InstOrder {
    const WMap<float>& arrival;
    const WMap<float>& depart;
    InstOrder(const WMap<float>& arrival_, const WMap<float>& depart_) : arrival(arrival_), depart(depart_) {}

    bool operator()(GLit p, GLit q) const {
        float len_p = arrival[p] + depart[p];
        float len_q = arrival[q] + depart[q];
        if (len_p > len_q) return true;
        if (len_p < len_q) return false;
        if (arrival[p] < arrival[q]) return true;
        if (arrival[p] > arrival[q]) return false;
        return false;
    }
};


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

#if 1   /*EXPERIMENTAL*/
    active.clear();

    InstOrder lt(arrival, depart);
    KeyHeap<GLit, false, InstOrder> ready(lt);
    For_Gates(N, w){
        if (isCO(w)){
            ready.add(w);
            /**/assert(w);
            active(w) = true;
        }
    }

    while (ready.size() > 0){
        Wire w = ready.pop() + N;
        assert(active[w]);

        if (isLogic(w)){
            reprioritizeCuts(w, cutmap[w]);
            const Cut& cut = cutmap[w][0];
            mapped_area += 1;       // -- LUT cost = 1

            for (uint i = 0; i < cut.size(); i++){
                Wire v = cut[i] + N;
                area_est(v) = 0;
                if (!active[v]){
                    ready.add(v);
                    /**/if (!(v)) Dump(w, w[0], w[1], w[2], w[3]);
                    /**/assert(v);
                    active(v) = true;
                }
            }

        }else if (!isCI(w)){
            For_Inputs(w, v){
                if (!active[v]){
                    ready.add(v);
                    /**/assert(v);
                    active(v) = true;
                }
            }
        }
    }

    depart.clear();
    For_All_Gates_Rev(N, w){
        if (isLogic(w)){
            if (active[w]){
                const Cut& cut = cutmap[w][0];
                for (uint i = 0; i < cut.size(); i++){
                    Wire v = cut[i] + N;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);
                }
            }else
                depart(w) = FLT_MAX;    // -- marks deactivated node

        }else if (!isCI(w)){
            float delay = (w != gate_Delay) ? 0.0f : w.arg() / DELAY_FRACTION;
            For_Inputs(w, v){
                fanouts(v)++;
                newMax(depart(v), depart[w] + delay);
            }
        }
    }

#else  /*END EXPERIMENTAL*/
    depart.clear();
    For_All_Gates_Rev(N, w){
        if (isLogic(w)){
            if (fanouts[w] > 0){
                /**/prioritizeCuts(w, cutmap[w]);
                const Cut& cut = cutmap[w][0];
                mapped_area += 1;       // -- LUT cost = 1

                for (uint i = 0; i < cut.size(); i++){
                    Wire v = cut[i] + N;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);
                    /**/area_est(v) = 0;
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
#endif

    //**/For_All_Gates_Rev(N, w) Dump(w, (int)active[w], fanouts[w]);

#if 1   // -- temporary solution for computing estimated departure for inactive nodes
    WMap<float> tmpdep;
    depart.copyTo(tmpdep);
    For_Gates_Rev(N, w){
        if (isCI(w)) continue;
        if (tmpdep[w] == FLT_MAX) continue;

        //if (isLogic(w)) continue;
        For_Inputs(w, v){
            if (!isLogic(v)) continue;
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
        if (!P.map_for_delay || round > 0){
            // Blend new values with old:
            uint  r = round + 1.0f;
            if (P.map_for_delay && round != 0) r -= 1.0;
            float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 2.0f);
            float beta  = 1.0f - alpha;

            For_Gates(N, w){
                if (isLogic(w)){
                    fanout_est(w) = alpha * max_(fanouts[w], 1u)
//                    fanout_est(w) = alpha * max_(double(fanouts[w]), 0.95)   // -- slightly less than 1 leads to better delay
                                  + beta  * fanout_est[w];
                }
            }
        }

    }else{
        // Compute FTBs:
        Vec<uint64> ftbs(reserve_, mapped_area);
        For_Gates(N, w){
            if (isLogic(w) && active[w]){
                const Cut& cut = cutmap[w][0];
                ftbs += computeFtb(w, cut, in_memo, memo);
            }
        }

        // Build LUT representation:
        N.is_frozen = false;
        N.setMode(gig_FreeForm);
        uint j = 0;
        For_Gates(N, w){
            if (isLogic(w) && active[w]){
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
        GigRemap m;
        N.compact(m);
        if (remap)
            m.applyTo(remap->base());
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
    uint last_round = P.n_rounds - 1 + (uint)P.map_for_delay;
    for (round = 0; round <= last_round; round++){
        double T0 = cpuTime();
        cuts_enumerated = 0;
        For_All_Gates(N, w)
            generateCuts(w);
        double T1 = cpuTime();

        bool instantiate = (round == last_round);
        updateFanoutEst(instantiate);
        double T2 = cpuTime();

        if (round == 0)
            target_arrival = mapped_delay * P.delay_factor;
        newMin(best_arrival, mapped_delay);

        if (!P.quiet)
            WriteLn "round=%d   mapped_area=%,d   mapped_delay=%_   cuts=%,d   [enum: %t, blend: %t]", round, mapped_area, mapped_delay, cuts_enumerated, T1-T0, T2-T1;

        if (round == 0 || !P.recycle_cuts || (round == 1 && P.map_for_delay)){
            if (round != last_round){
                winner.clear();
                For_Gates(N, w)
                    if (isLogic(w) && cutmap[w].size() > 0)
                        winner(w) = cutmap[w][0];
            }

            for (uint i = 0; i < cutmap.base().size(); i++)
                dispose(cutmap.base()[i], mem);
            cutmap.clear();
        }
    }

    WriteLn "Result: %_", info(N);
}


// <<== put preprocessing here, not in Main_lutmap.cc

LutMap::LutMap(Gig& N_, Params_LutMap P_, WSeen& keep_, WMapX<GLit>* remap_) :
    P(P_), N(N_), keep(keep_), remap(remap_)
{
    if (Has_Gob(N, Strash))
        Remove_Gob(N, Strash);

    if (!N.is_canonical){
        WriteLn "Compacting... %_", info(N);

        gate_id orig_sz = N.size();
        GigRemap m;
        N.compact(m);
        if (remap){
            for (gate_id i = 0; i < orig_sz; i++){
                Lit p = GLit(i);
                (*remap)(p) = m(p);
            }
        }
        WriteLn "Done... %_", info(N);

    }else if (remap){
        For_All_Gates(N, w)
            (*remap)(w) = w;
    }

    normalizeLut4s(N);

    // Run mapper:
    run();

    // Free memory:
    for (uint i = 0; i < cutmap.base().size(); i++)
        dispose(cutmap.base()[i], mem);
    mem.clear(false);

    area_est  .clear(true);
    fanout_est.clear(true);
}


// If provided, each gate in 'keep' will be the output of a lookup table, and all the fanout logic
// of that gate will treat it as if it were a free input (i.e. no assumption is made on its logic
// function). This allows the LUT to be "forced" (modified to a constant, or indeed any logic)
// after the mapping phase without old logic on the fanin side leaking through the "keep gate" by
// means of circuit optimization done during mapping.
//
// The 'remap' map will map old gates to new gates (with sign, so 'x' can go to '~y'). Naturally,
// many signals may be gone; these are mapped to 'glit_NULL'. NOTE! Even inputs may be missing from
// 'remap' if they are not in the transitive fanin of any output.
// 
void lutMap(Gig& N, Params_LutMap P, WSeen* keep, WMapX<GLit>* remap)
{
    WSeen keep_dummy;
    if (!keep)
        keep = &keep_dummy;

    LutMap inst(N, P, *keep, remap);
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

hur få diversity med? varför svårt att hitta delay optimal med få cuts när Alan lyckas?
*/
