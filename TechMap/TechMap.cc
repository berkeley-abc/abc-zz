//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TechMap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Second generation technology mapper.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Currently targeted at FPGA mapping.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "TechMap.hh"
#include "ZZ_Gig.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_BFunc.hh"
#include "ZZ_Npn4.hh"
#include "ZZ/Generics/Sort.hh"
#include "Unmap.hh"


namespace ZZ {
using namespace std;


#define Cut TechMap_Cut
#define Cut_NULL Cut()

#define CutSet TechMap_CutSet
#define CutSet_NULL CutSet()

#define CutImpl TechMap_CutImpl
#define CutImpl_NULL CutImpl()

#define Cost TechMap_Cost
#define Cost_NULL TechMap_Cost()

#include "TechMap_CutSets.icc"


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


macro bool isLogic(Wire w) {
    return w == gate_And
        || w == gate_Xor
        || w == gate_Mux
        || w == gate_Maj
        || w == gate_One
        || w == gate_Gamb
        || w == gate_Dot
        || w == gate_Lut4;
}


macro uint countInputs(Wire w) {
    uint n = 0;
    for (uint i = 0; i < w.size(); i++)
        if (w[i])
            n++;
    return n;
}


// <<== note! could provide slack here and use inverters rather than duplicating gates if timing allows it
static
void removeInverters(Gig& N, bool quiet)
{
    uint  luts_added  = 0;
    uint  wires_added = 0;
    uint  invs_added  = 0;

    // Categorize fanout-inverter situation, considering only non-LUT fanouts:
    WSeen pos, neg;
    For_Gates(N, w){
        if (w == gate_Lut6) continue;

        For_Inputs(w, v){
            if (v.sign){
                if (v == gate_Lut6){
                    // LUT has a signed fanout:
                    neg.add(v);
                }else if (v == gate_Const){
                    if (v == ~N.True())
                        w.set(Input_Pin(v), N.False());
                    else if (v == ~N.False())
                        w.set(Input_Pin(v), N.True());
                    else
                        assert(false);
                }else{
                    // Non-LUT to non-LUT connection with inverters -- need to insert a inverter:
                    Wire u = N.add(gate_Lut6).init(+v);
                    ftb(u) = 0x5555555555555555ull;
                    w.set(Input_Pin(v), u);
                    invs_added++;
                }
            }else if (v == gate_Lut6)
                // LUT has a non-signed fanout:
                pos.add(v);
        }
    }

    // Remove inverters between a non-LUT fed by a LUT:
    WSeen flipped;
    WSeen added;
    WMap<GLit> neg_copy;
    For_Gates(N, w){
        if (added.has(w)) continue;

        For_Inputs(w, v){
            if (!neg.has(v)) continue;

            if (!pos.has(v)){
                // Only negated fanouts; invert FTB:
                if (!flipped.has(v)){
                    ftb(v) = ~ftb(v);
                    flipped.add(v);
                }
                w.set(Input_Pin(v), ~v);

            }else{
                // Both negated and non-negated fanouts, duplicate gate:
                if (!neg_copy[v]){
                    luts_added++;
                    Wire u = N.add(gate_Lut6);
                    added.add(u);
                    for (uint i = 0; i < 6; i++){
                        u.set(i, v[i]);
                        if (v[i] != Wire_NULL) wires_added++;
                    }
                    ftb(u) = ~ftb(v);
                    neg_copy(v) = u;
                }

                if (v.sign)
                    w.set(Input_Pin(v), neg_copy[v]);
            }
        }
    }

    // Remove inverters on the input side of a LUT:
    For_Gates(N, w){
        if (w != gate_Lut6) continue;

        For_Inputs(w, v){
            if (v.sign){
                w.set(Input_Pin(v), +v);
                ftb(w) = ftb6_neg(ftb(w), Input_Pin(v));
            }
        }
    }

    if (luts_added > 0 && !quiet)
        WriteLn "%_/%_ LUTs/wires added due to inverter removal.", luts_added, wires_added;
    if (invs_added > 0 && !quiet)
        WriteLn "%_ irremovable inverters between non-logic gates.", invs_added;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut implementation:



// NOTE! Through out the code, 'impl' is indexed 'impl[sel][wire]' where
//   'sel == 0' LUT cut
//   'sel == 1' F7 cut


struct CutImpl {
    enum { TRIV_CUT = -1, NO_CUT = -2 };    // -- used with 'idx'
    int     idx;
    float   arrival;
    float   area_est;

    CutImpl() : idx(NO_CUT), arrival(0.0f), area_est(0.0f) {}
};


template<> fts_macro void write_(Out& out, const CutImpl& v)
{
    FWrite(out) "CutImpl{idx=%_; arrival=%_; area_est=%_}", v.idx, v.arrival, v.area_est;
}


// Returns '(arrival, area_est, late)'; arrival and area for an area optimal selection UNDER
// the assumption that 'req_time' is not exceeded. If no such cut exists,
// '(FLT_MAX, FLT_MAX)' is returned.  If 'sel' is non-null, cut selection is recorded there.
// NOTE! Setting 'req_time = -FLT_MAX' is equivalent of calling 'cutImpl_bestDelay()'.
template<class CUT>
inline Trip<float,float,bool> cutImpl_bestArea(CUT cut, const Vec<WMap<CutImpl> >& impl, float req_time)
{
    // Find best arrival:
    float arrival = 0;
    for (uint i = 0; i < cut.size(); i++)
        newMax(arrival, impl[0][GLit(cut[i])].arrival);     // -- impl[0] is delay optimal

    bool late = false;
    if (req_time == -FLT_MAX)            // -- special value => return delay optimal selection
        req_time = arrival;
    else if (arrival > req_time){
        late = true;
        req_time = arrival;
    }

    // Pick best area meeting that arrival:
    float total_area_est = 0;
    for (uint i = 0; i < cut.size(); i++){
        GLit p = GLit(cut[i]);
        float area_est = impl[0][p].area_est;
        uint best_j = 0;
        for (uint j = 1; j < impl.size(); j++){
//          if (impl[j][p].idx == CutImpl::NO_CUT) break;
            if (impl[j][p].idx == CutImpl::NO_CUT) continue;        // <<== see line 764 /*test*/
            if (impl[j][p].arrival <= req_time){
                if (newMin(area_est, impl[j][p].area_est))
                    best_j = j;
            }
        }

        newMax(arrival, impl[best_j][p].arrival);
        total_area_est += area_est;
    }

    return tuple(arrival, total_area_est, late);
}


// Returns '(arrival, area_est)'; arrival and area of a delay optimal selection for 'cut'.
// If 'sel' is non-null, cut selection is recorded there.
template<class CUT>
inline Pair<float,float> cutImpl_bestDelay(CUT cut, const Vec<WMap<CutImpl> >& impl)
{
    Trip<float,float,bool> t = cutImpl_bestArea(cut, impl, -FLT_MAX);
    assert(!t.trd);
    return tuple(t.fst, t.snd);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Costs for sorting cuts:


struct Cost {
    uint    idx;
    float   delay;
    float   area;
    uchar   inputs;
    uchar   late;     // -- (conceptually of type 'bool') implementation does not meet timing requirement
};


template<> fts_macro void write_(Out& out, const Cost& v)
{
    FWrite(out) "Cost{idx=%_; late=%_; delay=%_; area=%_; inputs=%_}", v.idx, v.late, v.delay, v.area, v.inputs;
}


struct Area_lt {
    bool operator()(const Cost& x, const Cost& y) const {
        if (!x.late && y.late) return true;
        if (x.late && !y.late) return false;
        if (!x.late){
            if (x.area < y.area) return true;
            if (x.area > y.area) return false;
            /**/if (x.inputs < y.inputs) return true;       // <<== evaluate this
            /**/if (x.inputs > y.inputs) return false;
            if (x.delay < y.delay) return true;
            if (x.delay > y.delay) return false;
        }else{
            if (x.delay < y.delay) return true;
            if (x.delay > y.delay) return false;
            /**/if (x.inputs < y.inputs) return true;       // <<== evaluate this
            /**/if (x.inputs > y.inputs) return false;
            if (x.area < y.area) return true;
            if (x.area > y.area) return false;
        }
        return false;
    }
};


struct Delay_lt {
    bool operator()(const Cost& x, const Cost& y) const {
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
        if (x.inputs < y.inputs) return true;
        if (x.inputs > y.inputs) return false;
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        return false;
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// TechMap class:


class TechMap {
    enum { INACTIVE, ACTIVE, FIRST_CUT };   // -- use with 'active'

    // Input:
    Gig&                  N;
    const Params_TechMap& P;

    // State:
    StackAlloc<uint64>  mem;
    StackAlloc<uint64>  mem_win;
    WMap<CutSet>        cutmap;
    WMap<Cut>           winner;
    Vec<WMap<CutImpl> > impl;

    WMap<float>         fanout_est;
    WMap<float>         depart;
    WMap<uchar>         active;     // -- for non-logic: ACTIVE/INACTIVE. For logic: 'INACTIVE' or 'FIRST_CUT + choice#'
    WSeen               dual_phase; // -- these gates feed COs both positively and negatively

    uint                iter;
    float               target_arrival;
    WMap<uint>          fanouts;    // -- number of fanouts in the current induced mapping

    // Temporaries:
    DynCutSet           dcuts;
    Vec<Cost>           tmp_costs;
    Vec<uint>           tmp_where;
    Vec<uint>           tmp_list;

    // Statistics:
    uint64              cuts_enumerated;

    // Helper methods:
    float lutCost(GLit w, Cut c);
    void  reserveImpls(uint n);

    // Major internal methods:
    void bypassTrivialCutsets(Wire w);
    void findDualPhaseGates();
    void generateCuts(Wire w);
    void generateCuts_LogicGate(Wire w, DynCutSet& out_dcuts);
    template<class CUTSET>
    void prioritizeCuts(Wire w, CUTSET& dcuts);
    void updateTargetArrival();
    void induceMapping(bool instantiate);
    void updateEstimates();
    void copyWinners();
    void printProgress(double T0);

public:
    TechMap(Gig& N_, const Params_TechMap& P_) : N(N_), P(P_), active(INACTIVE) {}
    void run();
};


//=================================================================================================
// -- Helpers:


inline float TechMap::lutCost(GLit w, Cut c)
{
    float v = P.lut_cost[c.size()];
    return !dual_phase.has(w) ? v : 2*v;
}


void TechMap::reserveImpls(uint n)
{
    impl.setSize(n);
    for (uint i = 0; i < n; i++)
        impl[i].reserve(N.size());
}


//=================================================================================================
// -- Cut prioritization:


template<class CUTSET>
void TechMap::prioritizeCuts(Wire w, CUTSET& dcuts)
{
    Vec<Cost>& costs = tmp_costs;
    costs.setSize(dcuts.size());

    // Compute optimal delay:
    float best_delay = FLT_MAX;
    uint  best_delay_idx = UINT_MAX;
    for (uint i = 0; i < dcuts.size(); i++){
        costs[i].idx  = i;
        costs[i].late = false;
        l_tuple(costs[i].delay, costs[i].area) = cutImpl_bestDelay(dcuts[i], impl);

        if (newMin(best_delay, costs[i].delay))
            best_delay_idx = i;

        costs[i].area += lutCost(w, dcuts[i]);
        costs[i].delay += 1.0;
        costs[i].inputs = dcuts[i].size();
    }

    // Iteration specific cut sorting and cut implementations:
    float req_time = FLT_MAX;
    if (iter == 0){
        assert(impl.size() == 1);
        sobSort(sob(costs, Delay_lt()));

    }else{
        req_time = active[w] ? target_arrival - depart[w] - 1.0f : best_delay;
#if 1
        if (!active[w] && iter >= 2){
            For_Inputs(w, v)
                if (active[v]) newMax(req_time, target_arrival - depart[v]);
            req_time += 1.0;
        }
#endif
        if (req_time < best_delay)
            req_time = best_delay;   // -- if we change heuristics so we cannot always meet timing, at least do the best we can

        for (uint i = 0; i < dcuts.size(); i++){
            costs[i].idx = i;
            l_tuple(costs[i].delay, costs[i].area, costs[i].late) = cutImpl_bestArea(dcuts[i], impl, req_time);
            assert(costs[i].delay >= best_delay);

            costs[i].area += lutCost(w, dcuts[i]);
            costs[i].delay += 1.0f;
            costs[i].inputs = dcuts[i].size();
        }

        swp(costs[0], costs[best_delay_idx]);
        Array<Cost> tail = costs.slice(1);
        sobSort(sob(tail, Area_lt()));
    }

    // Implement order:
    Vec<uint>& where = tmp_where;
    Vec<uint>& list  = tmp_list;
    where.setSize(dcuts.size());
    list .setSize(dcuts.size());
    for (uint i = 0; i < dcuts.size(); i++)
        where[i] = list[i] = i;

    for (uint i = 0; i < dcuts.size(); i++){
        uint w = where[costs[i].idx];
        where[list[i]] = w;
        swp(list[i], list[w]);
        dcuts.swap(i, w);
    }

    // Store delay optimal implementation:
    impl[0](w).idx      = 0;
    impl[0](w).area_est = costs[0].area / fanout_est[w];
    impl[0](w).arrival  = costs[0].delay;

    // Clear all other implementations:
    for (uint n = 1; n < impl.size(); n++)
        impl[n](w).idx = CutImpl::NO_CUT;

    if (iter > 0){
        // Store area optimal implementations:
        if (costs.size() > 1){
            if (costs[0].area > costs[1].area){
                impl[1](w).idx      = 1;
                impl[1](w).area_est = costs[1].area / fanout_est[w];
                impl[1](w).arrival  = costs[1].delay;
            }else
                impl[1](w).idx = CutImpl::NO_CUT;
        }
    }

#if 0
    if (P.use_fmux){
        // Compute and store F7 implementation:
        for (uint i = 0; i < dcuts.size(); i++){
            Cut c = dcuts[i];
            if (c.size() == 3){
                // Possibly a MUX:
                Npn4Norm n = npn4_norm[c.ftb()];
                if (n.eq_class == npn4_cl_MUX){  // -- pin order: (pin0 ? pin2 : pin1)
                    float arrival;
                    float area_est;
                    bool  late;
                    l_tuple(arrival, area_est, late) = cutImpl_bestAreaMux(dcuts[i], impl, req_time);
                }
            }
        }
    }
#endif

    dcuts.shrinkTo(P.cuts_per_node);
}


//=================================================================================================
// -- Cut generation:


#include "TechMap_CutGen.icc"


void TechMap::bypassTrivialCutsets(Wire w)
{
    // If a buffer or a constant was detected during cut enumeration, replace input
    // with input of buffer or the constant that was detected:
    For_Inputs(w, v){
        if (cutmap[v].size() == 1 && cutmap[v][0].size() <= 1){
            assert(cutmap[v][0].ftbSz() == 1);
            uint64 ftb = cutmap[v][0].ftb();
            if (ftb == 0x0000000000000000ull)
                w.set(Input_Pin(v), ~N.True() ^ sign(v));
            else if (ftb == 0xFFFFFFFFFFFFFFFFull)
                w.set(Input_Pin(v), N.True() ^ sign(v));
            else if (ftb == 0x5555555555555555ull)
                w.set(Input_Pin(v), ~N[cutmap[v][0][0]] ^ sign(v));
            else if (ftb == 0xAAAAAAAAAAAAAAAAull)
                w.set(Input_Pin(v), N[cutmap[v][0][0]] ^ sign(v));
            else
                assert(false);
        }
    }
}


void TechMap::findDualPhaseGates()
{
    dual_phase.clear();
    WSeen occur_pos;
    WSeen occur_neg;
    For_Gates(N, w){
        if (isLogic(w)) continue;
        For_Inputs(w, v){
            if (!isLogic(v)) continue;

            if (v.sign) occur_neg.add(v);
            else        occur_pos.add(v);
            if (occur_neg.has(v) && occur_pos.has(v))
                dual_phase.add(v);
        }
    }
}


void TechMap::generateCuts(Wire w)
{
    assert(!w.sign);

    if (!isSeqElem(w))
        bypassTrivialCutsets(w);

    dcuts.begin();
    bool skip = false;

    switch (w.type()){
    case gate_Const:
    case gate_Reset:    // -- not used, but is part of each netlist
    case gate_Box:      // -- treat sequential boxes like a global source
    case gate_PI:
    case gate_FF:{
        // Global sources have only the implicit trivial cut:
        for (uint i = 0; i < impl.size(); i++){
            CutImpl& m = impl[i](w);
            m.idx = CutImpl::TRIV_CUT;
            m.area_est = 0;
            m.arrival = 0;
        }
        break;}

    case gate_Bar:
    case gate_Sel:
        // Treat barriers and pin selectors as PIs, but with area and delay:
        for (uint i = 0; i < impl.size(); i++){
            CutImpl& m = impl[i](w);
            CutImpl& n = impl[i](w[0]);
            if (n.idx == CutImpl::NO_CUT)
                m.idx = CutImpl::NO_CUT;
            else{
                m.idx = CutImpl::TRIV_CUT;
                m.area_est = n.area_est;
                m.arrival = n.arrival;
            }
        }
        break;

    case gate_Delay:{
        // Treat delay gates as PIs except for delay:
        CutImpl& md = impl[0](w);
        md.idx = CutImpl::TRIV_CUT;
        l_tuple(md.arrival, md.area_est) = cutImpl_bestDelay(w, impl);

        if (impl.size() > 1){
            CutImpl& ma = impl[1](w);
            ma.idx = CutImpl::TRIV_CUT;
            bool late;
            l_tuple(ma.arrival, ma.area_est, late) = cutImpl_bestArea(w, impl, FLT_MAX);
            assert(!late);
        }

        // Add box delay:
        for (uint i = 0; i < impl.size(); i++)
            impl[i](w).arrival += w.arg() * P.delay_fraction;

        break;}

    case gate_And:
    case gate_Xor:
    case gate_Mux:
    case gate_Maj:
    case gate_One:
    case gate_Gamb:
    case gate_Dot:
    case gate_Lut4:
        // Logic:
        if (iter < P.recycle_iter){
            if (winner[w]){
                assert(dcuts.inputs.size() == 0);
                for (uint i = 0; i < winner[w].size(); i++)
                    dcuts.inputs.push(winner[w][i]);
                for (uint i = 0; i < winner[w].ftbSz(); i++)
                    dcuts.ftb.push(winner[w].ftb(i));
                dcuts.next();
            }

            generateCuts_LogicGate(w, dcuts);
            cuts_enumerated += dcuts.size();    // -- for statistics
            prioritizeCuts(w, dcuts);

            //if (!probeRound()){
            //    cuts.shrinkTo(P.cuts_per_node);
            //}else{
            //    for (uint i = 1; i < cuts.size(); i++){
            //        if (delay(cuts[i], arrival, N) > delay(cuts[0], arrival, N))
            //            cuts.shrinkTo(i);
            //    }
            //    cuts.shrinkTo(2 * P.cuts_per_node);
            //}

        }else{
            prioritizeCuts(w, cutmap(w));   // <<== need to be able to update a static cut-set!
            skip = true;
        }
        break;

    case gate_PO:
    case gate_Seq:
        // Don't store trivial cuts for sinks (saves a little memory):
        skip = true;
        break;

    default:
        ShoutLn "INTERNAL ERROR! Unhandled gate type: %_", w.type();
        assert(false);
    }

    if (!skip)
        cutmap(w) = dcuts.done(mem);
}


//=================================================================================================
// -- Update estimates:


void TechMap::updateTargetArrival()
{
    target_arrival = 0;
    For_Gates(N, w)
        if (isCO(w))
            newMax(target_arrival, impl[0][w[0]].arrival);

    if (P.delay_factor > 0)
        target_arrival *= P.delay_factor;
    else
        target_arrival -= P.delay_factor;
}


void TechMap::induceMapping(bool instantiate)
{
    active.clear();
    depart.clear();
    fanouts.clear();

    For_All_Gates_Rev(N, w){
        if (isCO(w))
            active(w) = ACTIVE;

        if (!active[w]){
            depart(w) = FLT_MAX;    // <<== for now, give a well defined value to inactive nodes

        }else{
            if (isLogic(w)){
                float req_time = target_arrival - depart[w];

                uint  best_i    = UINT_MAX;
                float best_area = FLT_MAX;
              #if 0
                //<<== failade här för np.aig, runda 2 (best_i förblev UINT_MAX)
                // Pick best implementation among current choices:
                for (uint i = 0; i < impl.size() && impl[i][w].idx != CutImpl::NO_CUT; i++){
                    if (impl[i][w].arrival <= req_time)
                        if (newMin(best_area, impl[i][w].area_est))
                            best_i = i;
                }

              #else
                // Pick best implementation among updated choices:
                for (uint i = 0; i < impl.size(); i++){
                    int j = impl[i][w].idx;
                    if (j == CutImpl::NO_CUT) continue;

                    Cut cut = cutmap[w][j];

                    float arrival;
                    float area_est;
                    bool  late;
                    l_tuple(arrival, area_est, late) = cutImpl_bestArea(cut, impl, req_time - 1.0f);    // <<== 1.0f
                    area_est += lutCost(w, cut);

                    if (!late)
                        if (newMin(best_area, area_est))
                            best_i = i;
                }
              #endif

                // For non-COs close to the outputs, required time may not be met:
                if (best_i == UINT_MAX){
                    best_i = 0;
                    for (uint i = 1; i < impl.size() && impl[i][w].idx != CutImpl::NO_CUT; i++){
                        if (impl[i][w].arrival < impl[best_i][w].arrival
                        ||  (impl[i][w].arrival == impl[best_i][w].arrival && impl[i][w].area_est < impl[best_i][w].area_est)
                        ){
                            best_i = i; }
                    }
                }

                assert(best_i != UINT_MAX);

                uint j = impl[best_i][w].idx; assert(j < cutmap[w].size());
                Cut cut = cutmap[w][j];

                assert(j < 254);
                active(w) = j + FIRST_CUT;

                for (uint k = 0; k < cut.size(); k++){
                    Wire v = cut[k] + N;
                    active(v) = ACTIVE;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);                // <<== 1.0f should not be used for F7 and F8 etc
                }

                if (instantiate){
                    // Change AND gate into a LUT6:
                    assert(cut.size() <= 6);
                    change(w, gate_Lut6);
                    for (uint i = 0; i < cut.size(); i++)
                        w.set(i, cut[i] + N);
                    ftb(w) = cut.ftb(0);
                }

            }else{
                if (!isCI(w)){
                    float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
                    For_Inputs(w, v){
                        active(v) = ACTIVE;
                        fanouts(v)++;
                        newMax(depart(v), depart[w] + delay);
                    }
                }
            }
        }
    }

    if (instantiate){
        For_Gates_Rev(N, w)
            if (isLogic(w))
                remove(w);
        N.compact();
    }

    For_Gates(N, w)
        assert(!active[w] || depart[w] != FLT_MAX);
}


void TechMap::updateEstimates()
{
    // Fanout est. (temporary)
    uint  r = iter + 1;
    float alpha = 1.0f - 1.0f / (float)(r*r + 1.0f);
    //float alpha = 1.0f - 1.0f / (float)(r*r*r*r + 2.0f);
    float beta  = 1.0f - alpha;

    For_Gates(N, w){
        if (isLogic(w)){
            fanout_est(w) = alpha * max_(fanouts[w], 1u)
                          + beta  * fanout_est[w];
        }
    }
}


void TechMap::copyWinners()
{
    mem_win.clear();
    For_Gates(N, w){
        if (active[w] < FIRST_CUT)
            winner(w) = Cut_NULL;
        else{
            Cut cut = cutmap[w][active[w] - FIRST_CUT];
            winner(w) = cut.dup(mem_win);
        }
    }
}


//=================================================================================================
// -- Main:


// 'T0' is the CPU time for the beginning of the current iteration.
void TechMap::printProgress(double T0)
{
    float mapped_delay = 0.0f;
    For_Gates(N, w)
        if (isCI(w) && active[w])
            newMax(mapped_delay, depart[w]);

    double total_wires = 0;
    double total_luts  = 0;
    For_Gates(N, w){
        if (w == gate_Lut6){
            total_wires += countInputs(w);
            total_luts  += 1;
        }else if (active[w] && isLogic(w)){
            total_wires += cutmap[w][active[w] - FIRST_CUT].size();
            total_luts  += 1;
        }
    }

    double T = cpuTime();
    WriteLn "Delay: %_    Wires: %,d    LUTs: %,d   [iter=%t  total=%t]", mapped_delay, (uint64)total_wires, (uint64)total_luts, T - T0, T;

    if (P.batch_output && iter == P.n_iters - 1){
        Write "%>11%,d    ", (uint64)total_luts;
        Write "%>11%,d    ", (uint64)total_wires;
        Write "%>6%d    "  , (uint64)mapped_delay;
        Write "%>10%t"     , cpuTime();
        NewLine;
    }
}


void TechMap::run()
{
    assert(!N.is_frozen);

    // Normalize netlist:
    if (Has_Gob(N, Strash))
        Remove_Gob(N, Strash);

    if (!isCanonical(N)){
        WriteLn "Compacting... %_", info(N);
        N.compact();       // <<== needs remap here
    }

    normalizeLut4s(N);

    // Prepare for mapping:
    target_arrival = 0;

    cutmap    .reserve(N.size());
    winner    .reserve(N.size());
    fanout_est.reserve(N.size());
    depart    .reserve(N.size());
    active    .reserve(N.size());
    fanouts   .reserve(N.size());

    reserveImpls(1 + P.use_fmux);

    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            fanout_est(w) = nFanouts(w);
            depart    (w) = FLT_MAX;
            active    (w) = ACTIVE;
        }
    }
    cuts_enumerated = 0;

    // Map:     <<== use Iter_Params here.... (how to score cuts, how to update target delay, whether to recycle cuts...)
    for (iter = 0; iter < P.n_iters; iter++){
        if (iter < P.recycle_iter)
            mem.clear();

        double T0 = cpuTime();
        if (iter == 1)
            reserveImpls(2 + P.use_fmux);

        // Generate cuts:
        findDualPhaseGates();       // -- may change during mapping due to 'bypassTrivialCutsets()' which rewires the circuit.
        For_All_Gates(N, w)
            generateCuts(w);

        // Computer estimations:
        if (iter == 0)
            updateTargetArrival();

        bool instantiate = (iter == P.n_iters - 1);
        induceMapping(instantiate);
        //<<== ELA, reprio?
        if (!instantiate){
            updateEstimates();
            copyWinners(); }

        printProgress(T0);
    }
    mem.clear();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void techMap(Gig& N, const Vec<Params_TechMap>& Ps)
{
    // Techmap:
    assert(Ps.size() >= 1);
    for (uint round = 0; round < Ps.size(); round++){
        if (round > 0){
            unmap(N);
            if (Ps[round].unmap_to_ands)
                expandXigGates(N);
            N.unstrash();
            N.compact();

            NewLine;
            WriteLn "Unmap: %_", info(N);
            NewLine;
        }
        TechMap map(N, Ps[round]);
        map.run();
    }

    if (!Ps.last().batch_output){
        NewLine;
        WriteLn "Legalization..."; }
    removeInverters(N, Ps.last().batch_output);
    //<<==legalize FMUXes
}


void techMap(Gig& N, const Params_TechMap& P, uint n_rounds)
{
    assert(n_rounds >= 1);
    Vec<Params_TechMap> Ps;
    for (uind i = 0; i < n_rounds; i++)
        Ps.push(P);
    techMap(N, Ps);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
TODO:

 - double polarity nodes (double costs of luts, don't allow for F7/F8)
 - FMUX fanout est. If >1, add cost of input LUT times this value minus one to area.
 - legalization phase: duplicate LUTs feeding 2 or more FMUXes. Remove negations (done already).
 - Area recovery (reprioritization? maybe not needed anymore)
 - make Unmap depth aware
 - signal tracking?? (ouch); esp. across unmap
 - check memory behavior

 - ban F7/F8 fed by non-LUT
 - ban F7/F8 feeding CO? (or subset of COs?)

*/

/*


- Each node stores multiple types of cuts:
  - Several cuts on a trade-off between area/delay
  - Cuts that corresponds to different points in target architecture (FMUX7, inverted output in standard cell etc.)

- FTB computation and semantic cuts/constant propagation.

- Native BigAnd/BigXor with dynamic internal points discovery?

- More compact cut-set representation


Cut
Organized CutSet

CutMap

struct CutSet
F7s, F8s

Example circuit:
  #Seq=834,781
  #Lut4=2,340,883
  #Delay=455
  #Box=374,355
  #Sel=21,965


*/
