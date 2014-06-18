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
#include "ZZ/Generics/IdHeap.hh"
#include "Unmap.hh"
#include "PostProcess.hh"
#include "Refactor.hh"

#define ELA_GLOBAL_LIM 200      // -- if nodes costing mroe than this are dereferenced, MFFC is too big to consider

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


macro bool isLogic(Wire w) { return isTechmapLogic(w); }


macro uint countInputs(Wire w) {
    uint n = 0;
    for (uint i = 0; i < w.size(); i++)
        if (w[i])
            n++;
    return n;
}


template<class ALLOC>
fts_macro CutSet noCuts(ALLOC& allocator) {
    DynCutSet dcuts;
    return dcuts.done(allocator); }


static
void upOrderBfs(Gig& N, Vec<gate_id>& result)
{
    assert(result.size() == 0);

    bool was_frozen = N.is_frozen;
    N.is_frozen = true;
    Auto_Gob(N, Fanouts);

    WMap<uint> ready(N, 0);
    For_All_Gates(N, w){
        /**/if (!isCI(w) && w.size() == 0){ WriteLn "WARNING, bad combinational input: %_", w; result.push(w.id); }
        if (isCI(w))
            result.push(w.id);
        else{
            for (uint i = 0; i < w.size(); i++){
                if (!w[i])
                    ready(w)++;     // -- discount null inputs
            }
        }
    }

    for (uint q = 0; q < result.size(); q++){
        Wire w = result[q] + N;
        Fanouts fs = fanouts(w);
        for (uint i = 0; i < fs.size(); i++){
            Wire v = fs[i];
            if (isCI(v)) continue;  // -- don't continue through flops
            ready(v)++;
            if (ready[v] == v.size())
                result.push(v.id);
        }
    }

#if 1   /*DEBUG*/
    For_All_Gates(N, w)
        if (ready[w] != w.size() && !isCI(w))
            Dump(w, ready[w], w.size());
#endif  /*END DEBUG*/

    N.is_frozen = was_frozen;
}


// 'w0' is selector, 'w1' is the true ("then") branch, 'w2' the false ("else") branch.
static
bool muxInputs(Gig& N, const Cut& c, Wire& w0, Wire& w1, Wire& w2, bool assert_is_mux = false)
{
    Npn4Norm n = npn4_norm[(ushort)c.ftb()];
    if (n.eq_class == npn4_cl_MUX){  // -- pin order: (pin0 ? pin2 : pin1)
        pseq4_t seq = perm4_to_pseq4[n.perm];
        w0 = (c[pseq4Get(seq, 0)] + N) ^ bool(n.negs & (1 << pseq4Get(seq, 0)));
        w1 = (c[pseq4Get(seq, 2)] + N) ^ bool(n.negs & (1 << pseq4Get(seq, 2)));
        w2 = (c[pseq4Get(seq, 1)] + N) ^ bool(n.negs & (1 << pseq4Get(seq, 1)));
        return true;
    }else
        assert(!assert_is_mux);
    return false;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut implementation:


struct CutImpl {
    enum { NONE = -2, TRIV = -1 };
    int     idx;
    float   arrival;
    float   area_est;

    CutImpl() : idx(NONE), arrival(0.0f), area_est(0.0f) {}
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
inline Trip<float,float,bool> cutImpl_bestArea(CUT cut, const Vec<WMap<CutImpl> >& impl, float req_time, bool skip_last)
{
    // Find best arrival:
    float arrival = 0;
    for (uint i = 0; i < cut.size(); i++)
        newMax(arrival, impl[0][GLit(cut[i])].arrival);     // -- impl[0] is delay optimal

    bool late = false;
    if (req_time == -FLT_MAX){           // -- special value => return delay optimal selection
        req_time = arrival;
    }else if (arrival > req_time){
        late = true;
        req_time = arrival;
    }

    // Pick best area meeting that arrival:
    float total_area_est = 0;
    for (uint i = 0; i < cut.size(); i++){
        GLit p = GLit(cut[i]);
        float area_est = impl[0][p].area_est;
        uint best_j = 0;
        for (uint j = 1; j < impl.size() - skip_last; j++){
            if (impl[j][p].idx == CutImpl::NONE) continue;
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
inline Pair<float,float> cutImpl_bestDelay(CUT cut, const Vec<WMap<CutImpl> >& impl, bool skip_last)
{
    Trip<float,float,bool> t = cutImpl_bestArea(cut, impl, -FLT_MAX, skip_last);
    assert(!t.trd);
    return tuple(t.fst, t.snd);
}


// Required time is for data. Data inputs are treated as arriving one level earlier.
inline Pair<float,float> cutImpl_bestAreaMux(Gig& N, Cut cut, const Vec<WMap<CutImpl> >& impl, float req_time, Wire w0)
{
    assert(req_time != -FLT_MAX);

    // Pick best area meeting that arrival:
    float arrival = -FLT_MAX;
    float total_area_est = 0;
    for (uint i = 0; i < cut.size(); i++){
        GLit p = GLit(cut[i]);
        float area_est = impl[0][p].area_est;
        uint best_j = 0;
        float d = (p == +w0) ? 0.0f : -1.0f;
        for (uint j = 1; j < impl.size() - (p != +w0); j++){     // -- "-(p != +w0)" to skip F7 implementation
            if (impl[j][p].idx == CutImpl::NONE) continue;
            if (impl[j][p].arrival + d <= req_time){
                if (newMin(area_est, impl[j][p].area_est))
                    best_j = j;
            }
        }

        newMax(arrival, impl[best_j][p].arrival + d);
        total_area_est += area_est;
    }

    if (arrival == -FLT_MAX) arrival = FLT_MAX;
    return tuple(arrival, total_area_est);
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
    enum { INACTIVE, ACTIVE, FIRST_CUT };   // -- used with 'active'
    enum { DELAY=0, AREA=1, F7MUX=2 };      // -- used with 'impl'

    struct Tmp {
        DynCutSet dcuts;
        Vec<Cost> costs;
        Vec<uint> where;
        Vec<uint> list;
        Vec<gate_id> inputs;
    };

    friend void* genCutThread(void* data);

    // Input:
    Gig&                  N;
    const Params_TechMap& P;
    WMapX<GLit>*          remap;

    // State:
    StackAlloc<uint64>  mem;
    StackAlloc<uint64>  mem_win;
    WMap<CutSet>        cutmap;
    WMap<Cut>           winner;
    Vec<WMap<CutImpl> > impl;

    WMap<float>         fanout_est;
    WMap<float>         depart;
    WMap<uchar>         active;     // -- for non-logic: ACTIVE/INACTIVE. For logic: 'INACTIVE' or 'FIRST_CUT + impl#'
    WSeen               dual_phase; // -- these gates feed COs both positively and negatively
    WMap<GLit>          mux_fanout; // -- maps LUTs to the F7 they feed (if any)

    uint                iter;
    float               target_arrival;
    WMap<uint>          fanouts;    // -- number of fanouts in the current induced mapping

    WMapX<GLit>         bufmap;     // -- used if remap is non-NULL; internal remap required to handle buffer removal

    // Temporaries:
    Tmp                 tmp;

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

    // Exact local area:
    float       acc_cost;
    float       acc_lim;
    Vec<GLit>   undo;

    WMap<float>   arrival;            // -- only used during ELA; elsewhere 'impl' is used.
    IdHeap<float> Q;
    WZet          depQ;

    bool deref(GLit w);
    void undoDeref();
    void ref(GLit w);
    bool tryRef(uint depth);
    void exactLocalArea();

public:
    TechMap(Gig& N_, const Params_TechMap& P_, WMapX<GLit>* remap_) :
        N(N_), P(P_), remap(remap_), active(INACTIVE), Q(arrival.base()) {}
    void run();
};


//=================================================================================================
// -- Helpers:


inline float TechMap::lutCost(GLit w, Cut c)
{
    float v = P.lut_cost[c.size()];
    return !dual_phase.has(w) ? v : 2*v;
}


inline void TechMap::reserveImpls(uint n)
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
    Vec<Cost>& costs = tmp.costs;
    costs.setSize(dcuts.size());

    // Compute optimal delay:
    float best_delay = FLT_MAX;
    uint  best_delay_idx = UINT_MAX;
    for (uint i = 0; i < dcuts.size(); i++){
        costs[i].idx  = i;
        costs[i].late = false;
        l_tuple(costs[i].delay, costs[i].area) = cutImpl_bestDelay(dcuts[i], impl, P.use_fmux);

        if (newMin(best_delay, costs[i].delay))
            best_delay_idx = i;

        costs[i].area += lutCost(w, dcuts[i]);
        costs[i].delay += 1.0;
        costs[i].inputs = dcuts[i].size();
    }

    // Iteration specific cut sorting and cut implementations:
    float req_time;
    if (iter == 0){
        req_time = FLT_MAX;
        sobSort(sob(costs, Delay_lt()));

    }else{
        if (active[w])
            req_time = target_arrival - depart[w] - 1.0f;
        else{
            req_time = 0;
            For_Inputs(w, v)
                if (active[v]) newMax(req_time, target_arrival - depart[v]);
            req_time += 1.0;
        }

        //**/Dump(w, req_time, best_delay, target_arrival, depart[w]);
        if (req_time < best_delay)
            req_time = best_delay;   // -- if we change heuristics so we cannot always meet timing, at least do the best we can

        for (uint i = 0; i < dcuts.size(); i++){
            costs[i].idx = i;
            l_tuple(costs[i].delay, costs[i].area, costs[i].late) = cutImpl_bestArea(dcuts[i], impl, req_time, false);
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
    Vec<uint>& where = tmp.where;
    Vec<uint>& list  = tmp.list;
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
    impl[DELAY](w).idx      = 0;
    impl[DELAY](w).area_est = costs[0].area / fanout_est[w];
    impl[DELAY](w).arrival  = costs[0].delay;

    // Clear all other implementations:
    assert(DELAY == 0);
    for (uint n = DELAY+1; n < impl.size(); n++)
        impl[n](w).idx = CutImpl::NONE;

    if (iter > 0){
        // Store area optimal implementations:
        if (costs.size() > 1 && costs[0].area > costs[1].area){
            impl[AREA](w).idx      = 1;
            impl[AREA](w).area_est = costs[1].area / fanout_est[w];
            impl[AREA](w).arrival  = costs[1].delay;
        }
    }

    if (P.use_fmux){
        // Compute and store F7 implementation:
        float best_area = FLT_MAX;
        for (uint i = 0; i < dcuts.size(); i++){
            Cut c = dcuts[i];
            if (c.size() == 3){
                // Possibly a MUX:
                Wire w0, w1, w2;
                if (muxInputs(N, c, w0, w1, w2)){
                    // Check that both data inputs are from logic:
//                  if (isLogic(w1) && isLogic(w2)){
                    if (isLogic(w1) && isLogic(w2) && (!mux_fanout[w1] || mux_fanout[w1] == w) && (!mux_fanout[w2] || mux_fanout[w2] == w)){
                        float arrival;
                        float area_est;
                        l_tuple(arrival, area_est) = cutImpl_bestAreaMux(N, dcuts[i], impl, req_time, w0);
                        area_est += P.mux_cost;

                        if (arrival <= req_time && newMin(best_area, area_est)){
                            impl[F7MUX](w).idx = i;
                            impl[F7MUX](w).area_est = area_est / fanout_est[w];
                            //impl[F7MUX](w).arrival = arrival + 1.0f;
                            impl[F7MUX](w).arrival = max_(arrival + 1.0f, impl[DELAY][w].arrival);  // -- since not all F7s can be realized, don't give it better delay than the best LUT implementation
                        }
                    }
                }
            }
        }

        if (impl[F7MUX][w].idx != CutImpl::NONE && impl[F7MUX][w].idx >= (int)P.cuts_per_node){
            dcuts.swap(impl[F7MUX][w].idx, P.cuts_per_node-1);
            impl[F7MUX](w).idx = P.cuts_per_node-1;
        }
    }

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
        if (!isLogic(v)) continue;
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

    switch (w.type()){
    case gate_Const:
    case gate_Reset:    // -- not used, but is part of each netlist
    case gate_Box:      // -- treat sequential boxes like a global source
    case gate_PI:
    case gate_FF:{
        // Global sources have only the implicit trivial cut:
        for (uint i = 0; i < impl.size(); i++){
            CutImpl& m = impl[i](w);
            m.idx = CutImpl::TRIV;
            m.area_est = 0;
            m.arrival = 0;
        }
        cutmap(w) = noCuts(mem);
        break;}

    case gate_Bar:
    case gate_Sel:
        // Treat barriers and pin selectors as PIs, but with area and delay:
        for (uint i = 0; i < impl.size(); i++){
            CutImpl& m = impl[i](w);
            CutImpl& n = impl[i](w[0]);
            if (n.idx == CutImpl::NONE)
                m.idx = CutImpl::NONE;
            else{
                m.idx = CutImpl::TRIV;
                m.area_est = n.area_est;
                m.arrival = n.arrival;
            }
        }
        cutmap(w) = noCuts(mem);
        break;

    case gate_Delay:{
        // Treat delay gates as PIs except for delay:
        CutImpl& md = impl[0](w);
        md.idx = CutImpl::TRIV;
        l_tuple(md.arrival, md.area_est) = cutImpl_bestDelay(w, impl, P.use_fmux);

        if (impl.size() > 1){
            CutImpl& ma = impl[1](w);
            ma.idx = CutImpl::TRIV;
            bool late;
            l_tuple(ma.arrival, ma.area_est, late) = cutImpl_bestArea(w, impl, FLT_MAX, false);
            assert(!late);
        }

        // Add box delay:
        for (uint i = 0; i < impl.size(); i++)
            impl[i](w).arrival += w.arg() * P.delay_fraction;

        cutmap(w) = noCuts(mem);
        break;}

    case gate_And:
    case gate_Xor:
    case gate_Mux:
    case gate_Maj:
    case gate_One:
    case gate_Gamb:
    case gate_Dot:
    case gate_Lut4:{
        if (iter < P.recycle_iter){
            DynCutSet& dcuts = tmp.dcuts;
            dcuts.begin();
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

            cutmap(w) = dcuts.done(mem);

            if (remap){
                uint64 ftb = cutmap[w][0].ftb();
                if (ftb == 0x0000000000000000ull)
                    bufmap(w) = N.False();
                else if (ftb == 0xFFFFFFFFFFFFFFFFull)
                    bufmap(w) = N.True();
                else if (ftb == 0x5555555555555555ull)
                    bufmap(w) = bufmap[~N[cutmap[w][0][0]]];
                else if (ftb == 0xAAAAAAAAAAAAAAAAull)
                    bufmap(w) = bufmap[N[cutmap[w][0][0]]];
            }

        }else
            prioritizeCuts(w, cutmap(w));
        break;}

    case gate_PO:
    case gate_Seq:
        break;          // -- don't store trivial cuts for sinks (saves a little memory)

    default:
        ShoutLn "INTERNAL ERROR! Unhandled gate type: %_", w.type();
        assert(false);
    }
}


//=================================================================================================
// -- Exact local area:


// Dereference all cuts in the MFFC of 'w' and:
//  - Add the cost of all dereferenced cuts to 'acc_cost'.
//  - Update 'fanouts' and 'mux_fanout' for removed cuts except for the top node.
//  - Abort if 'acc_cost' exceeds 'acc_lim'.
// Returns TRUE if dereference was successful, FALSE if exceeded 'acc_lim'.
// NOTE! 'active' is not touched by this procedure, meaning incative nodes may still have
// implementations (must be fixed afterwards).
//
bool TechMap::deref(GLit w)
{
    assert(!w.sign);

    uint j   = active[w] - FIRST_CUT;
    uint sel = impl[j][w].idx;
    Cut  cut = cutmap[w][sel];

    acc_cost += (j == F7MUX) ? P.mux_cost : lutCost(w, cut);

    //*E*/WriteLn "deref(%_) -- acc_cost=%_  acc_lim=%_", w, acc_cost, acc_lim;
    //*E*/Write "  cut-inputs:";
    //*E*/for (uint i = 0; i < cut.size(); i++) Write " %_", cut[i] + N;
    //*E*/NewLine;
    if (acc_cost > acc_lim)
        return false;

    // Recurse:
    for (uint i = 0; i < cut.size(); i++){
        Wire v = cut[i] + N;
        assert(fanouts[v] > 0);
        assert(!v.sign);
        if (!isLogic(v)) continue;

        if (j == F7MUX && mux_fanout[v]){
            assert(mux_fanout[v] == +w);
            undo.push(mux_fanout[v]);
            undo.push(~v);
            mux_fanout(v) = GLit_NULL;
        }else
            undo.push(v);

        //*E*/WriteLn "deref:ing %_ (%_ fanouts)", v + N, fanouts[v];
        fanouts(v)--;
        if (fanouts[v] == 0){
            if (!deref(v))
                return false;
        }
    }
    return true;
}


void TechMap::undoDeref()
{
    while (undo.size() > 0){
        GLit v = undo.popC();
        //*E*/WriteLn "undo:ing %_", v + N;
        if (v.sign)
            mux_fanout(v) = undo.popC();
        fanouts(v)++;
    }
}


// Update fanout count for new selection.
void TechMap::ref(GLit w)
{
    uint j   = active[w] - FIRST_CUT; assert(j != F7MUX); // <<== later
    uint sel = impl[j][w].idx;
    Cut  cut = cutmap[w][sel];

    //*E*/Write "ref(%_) cut:", w;
    //*E*/for (uint i = 0; i < cut.size(); i++) Write " %_", cut[i] + N;
    //*E*/NewLine;

    // Recurse:
    for (uint i = 0; i < cut.size(); i++){
        Wire v = cut[i] + N;
        if (!isLogic(v)) continue;

        if (j == F7MUX && mux_fanout[v])
            assert(false);

        //*E*/WriteLn "ref:ing %_", v + N;
        fanouts(v)++;
        if (fanouts[v] == 1){
            depart(v) = 0.0f;
            ref(v);
        }
        newMax(arrival(w), arrival[v] + 1.0f);      // -- new implementation may  be slower
        assert(depart[w] != FLT_MAX);
    }
}


// Find best implementation for 'w'. 'acc_lim' will be set to best solution so far (initialized to
// the size of the dereferenced logic).
bool TechMap::tryRef(uint depth)
{
    if (Q.size() == 0){
        acc_lim = acc_cost;
        return true; }

    Wire w = Q.pop() + N;
    const CutSet& cuts = cutmap[w];
    //*E*/WriteLn "tryRef(depth=%_) -- Q.size()=%_   w=%_", depth, Q.size(), w;

    uint added[6];
    uint added_sz;
    float old_dep[6];

    // For now, try every combination always:
    bool success = false;
    for (uint k = 0; k < cuts.size(); k++){
#if 0
        /**/int sel;
        /**/if (depth > 2){
        /**/    if (k == 0) sel = impl[DELAY][w].idx;
        /**/    else break;
        /**/}else sel = k;
#else
        int sel;
        if (depth < 2)
            sel = k;
        else if (depth < 4){
            if (k < 4) sel = k;
            else break;
        }else{
            if (k == 0) sel = impl[DELAY][w].idx;
            else break;
        }
        if (sel == CutImpl::NONE)
            continue;
#endif

        Cut c = cuts[sel]; assert(c.size() <= 6);
        float cost = lutCost(w, c);
        if (acc_cost + cost >= acc_lim)
            continue;

        // Check timing:
        bool meets_timing = true;
        for (uint i = 0; i < c.size(); i++){
            Wire v = c[i] + N;
            if (arrival[v] + depart[w] + 1.0f > target_arrival){
                meets_timing = false;
                break; }
        }
        if (!meets_timing)
            continue;

        acc_cost += cost;

        added_sz = 0;
        for (uint i = 0; i < c.size(); i++){
            Wire v = c[i] + N;

            old_dep[i] = depart[v];
            newMax(depart(v), depart[w] + 1.0f);

            //*E*/WriteLn "    <<working on cut #%_,  fanouts[%_]=%_  inQ=%_>>  for %_", k, v, fanouts[v], Q.has(v.id), w;
            if (fanouts[v] == 0 && !Q.has(v.id)){
                Q.add(v.id);     // <<== could add up conservative costs for inputs and abort earlier
                added[added_sz++] = v.id;
            }
        }

        if (tryRef(depth + 1)){
            // New best implementation; store it:
            impl[AREA](w).idx = sel;
            active(w) = AREA + FIRST_CUT;
            success = true;
        }

        for (uint i = c.size(); i > 0;){ i--;
            Wire v = c[i] + N;
            depart(v) = old_dep[i];
        }

        for (uint i = 0; i < added_sz; i++)
            if (Q.has(added[i]))
                Q.remove(added[i]);

        acc_cost -= cost;
    }

    if (!success)
        Q.weakAdd(w.id);

    return success;
}


void TechMap::exactLocalArea()
{
    // Recompute arrival times on the now induced mapping (including non-mapped nodes):
    arrival.clear();
    For_Gates(N, w){
        if (isCI(w)) continue;

        if (isLogic(w)){
            uint sel = (active[w] >= FIRST_CUT) ? active[w] - FIRST_CUT : DELAY;
            Cut cut = cutmap[w][impl[sel][w].idx];
            if (sel != F7MUX){
                for (uint i = 0; i < cut.size(); i++)
                    newMax(arrival(w), arrival[cut[i] + N] + 1.0f);
            }else{
                Wire w0, w1, w2;
                muxInputs(N, cut, w0, w1, w2, true);
                newMax(arrival(w), arrival[w0] + 1.0f);
                newMax(arrival(w), arrival[w1]);
                newMax(arrival(w), arrival[w2]);
            }

        }else{
            assert(w != gate_Lut6);
            assert(w != gate_F7Mux);

            float del = (w == gate_Delay) ? w.arg() * P.delay_fraction : 0.0f;
            For_Inputs(w, v)
                newMax(arrival(w), arrival[v] + del);
        }
    }

    if (!P.exact_local_area)
        return;     // -- we still want to populate 'arrival', so exit here

    float ela_max_arrival = 0;
    For_Gates(N, w)
        if (isCO(w))
            newMax(ela_max_arrival, arrival[w]);
    //Dump(ela_max_arrival);

    //WMap<uint> fanouts_copy;
    //WMap<GLit> mux_fanout_copy;
    //fanouts.copyTo(fanouts_copy);
    //mux_fanout.copyTo(mux_fanout_copy);

    For_Gates_Rev(N, w){
        if (fanouts[w] > 0 && isLogic(w)){
            acc_cost = 0;
            acc_lim  = ELA_GLOBAL_LIM;
            //*E*/WriteLn "\a/--TRYING: \a*%_\a0", w;
            //*E*/Write "\a/ACTIVE 1:"; For_Gates(N, w) Write " %_=%d(%d)", w.lit(), active[w], fanouts[w]; NewLine; Write "\a/";
            if (deref(w)){
                //*E*/Write "\a/ACTIVE 2:"; For_Gates(N, w) Write " %_=%d(%d)", w.lit(), active[w], fanouts[w]; NewLine; Write "\a/";
                acc_lim = acc_cost;
                acc_cost = 0;
                //*E*/WriteLn "Trying to beat cost: %_", acc_lim;
                Q.add(w.id);
                if (!tryRef(0)){
                    //*E*/WriteLn "[didn't find better solution]";
                    undoDeref();
                }else{
                    //*E*/WriteLn "[Found better solution: %_]", acc_lim;
                    undo.clear();
                    ref(w);

                    // Update departure recursively:
                    assert(depQ.size() == 0);
                    depQ.add(w);
                    for (uint q = 0; q < depQ.size(); q++){
                        Wire w = depQ[q] + N;
                        if (isLogic(w)){
                            assert(active[w] >= FIRST_CUT);
                            Cut cut = cutmap[w][impl[active[w] - FIRST_CUT][w].idx];

                            for (uint i = 0; i < cut.size(); i++){
                                Wire v = cut[i] + N;
                                if (newMax(depart(v), depart[w] + 1.0f)){
                                    depQ.add(v); }
                            }

                        }else{
                            if (!isCI(w)){
                                float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
                                For_Inputs(w, v){
                                    assert(active[v]);
                                    if (newMax(depart(v), depart[w] + delay))
                                        depQ.add(v);
                                }
                            }
                        }
                    }
                    depQ.clear();
                }
                while (Q.size() > 0) Q.pop();   // -- clear queue
            }else
                undoDeref();
            //*E*/Write "\a/ACTIVE 3:"; For_Gates(N, w) Write " %_=%d(%d)", w.lit(), active[w], fanouts[w]; NewLine; Write "\a/";
        }
    }

    For_Gates(N, w)
        if (isLogic(w) && fanouts[w] == 0)
            active(w) = INACTIVE;

    //*E*/For_Gates(N, w) WriteLn "depart[%_] = %_    active[%_] = %d", w, depart[w], w, active[w];
#if 0
    For_Gates(N, w){
        if (fanouts[w] != fanouts_copy[w]){ Dump(w, fanouts[w], fanouts_copy[w]); assert(false); }
        if (mux_fanout[w] != mux_fanout_copy[w]){ Dump(w, mux_fanout[w], mux_fanout_copy[w]); assert(false); }
    }
#endif
}


//=================================================================================================
// -- Induce mapping and update estimates:


// Note that the target arrival is set without relying on F7MUXes (making it conservative).
void TechMap::updateTargetArrival()
{
    target_arrival = 0;
    For_Gates(N, w)
        if (isCO(w))
            newMax(target_arrival, impl[DELAY][w[0]].arrival);

    if (P.delay_factor > 0)
        target_arrival *= P.delay_factor;
    else
        target_arrival -= P.delay_factor;
}


void TechMap::induceMapping(bool instantiate)
{
    #if 0   /*DEBUG*/
    {
        float target_arrival = 0;
        For_Gates(N, w)
            if (isCO(w))
                newMax(target_arrival, impl[DELAY][w[0]].arrival);
        WriteLn "-- recomputed target arrival: %_", target_arrival;
    }
    #endif  /*END DEBUG*/

    active.clear();
    depart.clear();
    fanouts.clear();
    mux_fanout.clear();

    WSeen skip_f7;  // -- gates in this set will not be considered for F7MUX (they feed at least one 'Seq' gate)

    For_All_Gates_Rev(N, w){
        if (isCO(w)){
            active(w) = ACTIVE;
            if (P.slack_util != FLT_MAX)
                depart(w) = max_(0.0f, (target_arrival - impl[DELAY][w[0]].arrival) - P.slack_util);
        }

        if (!active[w]){
            depart(w) = FLT_MAX;    // <<== for now, give a well defined value to inactive nodes

        }else{
            if (isLogic(w)){
                float req_time = target_arrival - depart[w];

                uint  best_i    = UINT_MAX;
                float best_area = FLT_MAX;

                // Pick best implementation among current choices:
                for (uint i = 0; i < impl.size(); i++){
                    if (impl[i][w].idx == CutImpl::NONE) continue;
                    if (impl[i][w].arrival <= req_time){
                        if (i == F7MUX){
                            if (skip_f7.has(w))
                                continue;
                            int j = impl[F7MUX][w].idx;
                            Cut cut = cutmap[w][j];
                            Wire w0, w1, w2;
                            muxInputs(N, cut, w0, w1, w2, true);
                            if (mux_fanout[w] != GLit_NULL || mux_fanout[w1] != GLit_NULL || mux_fanout[w2] != GLit_NULL)
                                continue;   // -- either feeding a F7 or fed by a LUT that feed a F7
                        }
                        if (newMin(best_area, impl[i][w].area_est))
                            best_i = i;
                    }
                }

                // For non-COs close to the outputs, required time may not be met:
                if (best_i == UINT_MAX)
                    best_i = DELAY;

                uint j = impl[best_i][w].idx; assert(j < cutmap[w].size());
                Cut cut = cutmap[w][j];

                assert(j < 254);
                active(w) = best_i + FIRST_CUT;

                for (uint k = 0; k < cut.size(); k++){
                    Wire v = cut[k] + N;
                    //**/if (v == gate_Lut4 && v.arg() == 0xAAAA) Dump(w, v);
                    active(v) = ACTIVE;
                    fanouts(v)++;
                }

                if (best_i != F7MUX){
                    for (uint k = 0; k < cut.size(); k++)
                        newMax(depart(cut[k] + N), depart[w] + 1.0f);

                }else{
                    Wire w0, w1, w2;
                    muxInputs(N, cut, w0, w1, w2, true);
                    newMax(depart(w0), depart[w] + 1.0f);
                    newMax(depart(w1), depart[w]);
                    newMax(depart(w2), depart[w]);

                    mux_fanout(w1) = w;   // -- mark fanin LUTs as "consumed" w.r.t. F7MUXes.
                    mux_fanout(w2) = w;
                }

            }else{
                if (!isCI(w)){
                    float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
                    For_Inputs(w, v){
                        active(v) = ACTIVE;
                        fanouts(v)++;
                        newMax(depart(v), depart[w] + delay);
                    }

                    if (w == gate_Seq && !P.fmux_feeds_seq){
                        For_Inputs(w, v)
                            skip_f7.add(v);
                    }
                }
            }
        }
    }

    /**/For_Gates(N, w) assert(!active[w] || !isLogic(w) || (fanouts[w] > 0 && active[w] >= FIRST_CUT));
    //**/if (iter != 0)
    //**/if (iter == P.n_iters - 1)
    exactLocalArea();

#if 0   /*DEBUG*/
    active.clear();
    depart.clear();
    fanouts.clear();
    mux_fanout.clear();

    For_All_Gates_Rev(N, w){
        if (isCO(w)){
            active(w) = ACTIVE;
            if (P.slack_util != FLT_MAX)
                depart(w) = max_(0.0f, (target_arrival - impl[DELAY][w[0]].arrival) - P.slack_util);
        }

        if (!active[w]){
            depart(w) = FLT_MAX;    // <<== for now, give a well defined value to inactive nodes

        }else{
            if (isLogic(w)){
                float req_time = target_arrival - depart[w];

                uint  best_i    = UINT_MAX;
                float best_area = FLT_MAX;

                // Pick best implementation among current choices:
                for (uint i = 0; i < impl.size(); i++){
                    if (impl[i][w].idx == CutImpl::NONE) continue;
                    if (impl[i][w].arrival <= req_time){
                        if (i == F7MUX){
                            int j = impl[F7MUX][w].idx;
                            Cut cut = cutmap[w][j];
                            Wire w0, w1, w2;
                            muxInputs(N, cut, w0, w1, w2, true);
                            if (mux_fanout[w] != GLit_NULL || mux_fanout[w1] != GLit_NULL || mux_fanout[w2] != GLit_NULL)
                                continue;   // -- either feeding a F7 or fed by a LUT that feed a F7
                        }
                        if (newMin(best_area, impl[i][w].area_est))
                            best_i = i;
                    }
                }

                // For non-COs close to the outputs, required time may not be met:
                if (best_i == UINT_MAX)
                    best_i = DELAY;

                uint j = impl[best_i][w].idx; assert(j < cutmap[w].size());
                Cut cut = cutmap[w][j];

                assert(j < 254);
                active(w) = best_i + FIRST_CUT;

                for (uint k = 0; k < cut.size(); k++){
                    Wire v = cut[k] + N;
                    //**/if (v == gate_Lut4 && v.arg() == 0xAAAA) Dump(w, v);
                    active(v) = ACTIVE;
                    fanouts(v)++;
                }

                if (best_i != F7MUX){
                    for (uint k = 0; k < cut.size(); k++)
                        newMax(depart(cut[k] + N), depart[w] + 1.0f);

                }else{
                    Wire w0, w1, w2;
                    muxInputs(N, cut, w0, w1, w2, true);
                    newMax(depart(w0), depart[w] + 1.0f);
                    newMax(depart(w1), depart[w]);
                    newMax(depart(w2), depart[w]);

                    mux_fanout(w1) = w;   // -- mark fanin LUTs as "consumed" w.r.t. F7MUXes.
                    mux_fanout(w2) = w;
                }

            }else{
                if (!isCI(w)){
                    float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
                    For_Inputs(w, v){
                        active(v) = ACTIVE;
                        /**/if (v == gate_Lut4 && v.arg() == 0xAAAA) Dump(2, w, v);
                        fanouts(v)++;
                        newMax(depart(v), depart[w] + delay);
                    }
                }
            }
        }
    }
#endif  /*END DEBUG*/


    if (instantiate){
        // Change AND gate into a LUT6 or MUX:
        For_Gates_Rev(N, w){
            if (!active[w] || !isLogic(w)) continue;

            uint best_i = active[w] - FIRST_CUT;
            uint j = impl[best_i][w].idx; assert(j < cutmap[w].size());
            Cut cut = cutmap[w][j]; assert(cut.size() <= 6);

            if (best_i != F7MUX){
                change(w, gate_Lut6);
                for (uint i = 0; i < cut.size(); i++)
                    w.set(i, cut[i] + N);
                ftb(w) = cut.ftb(0);
            }else{
                Wire w0, w1, w2;
                muxInputs(N, cut, w0, w1, w2, true);
                change(w, gate_F7Mux);
                w.init(w0, w1, w2);
            }
        }


        For_Gates_Rev(N, w)
            if (isLogic(w))
                remove(w);

        if (remap){
            // Apply bufmap first:
            for (uint i = 0; i < remap->base().size(); i++)
                remap->base()[i] = bufmap[remap->base()[i]];
        }

        GigRemap m;
        N.compact(m);
        if (remap){
            m.applyTo(remap->base());

          #if 0     // -- no longer necessary
            removeMuxViolations(N, arrival, target_arrival, P.delay_fraction);
            WriteLn "  -- Legalizing MUXes by duplication, adding:  #Mux=%_   #Lut6=%_", N.typeCount(gate_F7Mux) - n_muxes, N.typeCount(gate_Lut6) - n_luts;
            N.compact();
          #endif
        }
    }

    For_Gates(N, w){
        /**/if (!(!active[w] || depart[w] != FLT_MAX)) Dump(w);
        assert(!active[w] || depart[w] != FLT_MAX); }
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
            uint j = active[w] - FIRST_CUT;
            if (j == F7MUX) j = DELAY;
            Cut cut = cutmap[w][impl[j][w].idx];
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
            uint best_i = active[w] - FIRST_CUT;
            uint j = impl[best_i][w].idx;
            if (best_i != F7MUX){
                total_wires += cutmap[w][j].size();
                total_luts  += 1;
            }else
                total_wires += 1;   // -- selector needs routing
        }
    }

    // TEMPORARY <<==
    uint   len_count = 0;
    double len_total = 0.0;
    if (iter == P.n_iters - 1){
        WMap<float> arrival;
        For_Gates(N, w){
            if (w == gate_F7Mux){
                arrival(w) = max_(arrival[w[0]] + 1.0f, max_(arrival[w[1]], arrival[w[2]]));
            }else{
                float del = (w == gate_Lut6)  ? 1.0f :
                            (w == gate_Delay) ? w.arg() * P.delay_fraction :
                            /*otherwise*/       0.0f;
                float arr = 0;
                For_Inputs(w, v)
                    newMax(arr, arrival[v]);
                arrival(w) = arr + del;
            }
        }

        WMap<float> depart_;
        For_Gates_Rev(N, w){
            if (w == gate_F7Mux){
                newMax(depart_(w[0]), depart_[w] + 1.0f);
                newMax(depart_(w[1]), depart_[w]);
                newMax(depart_(w[2]), depart_[w]);
            }else{
                float del = (w == gate_Lut6)  ? 1.0f :
                            (w == gate_Delay) ? w.arg() * P.delay_fraction :
                            /*otherwise*/       0.0f;
                For_Inputs(w, v)
                    newMax(depart_(v), depart_[w] + del);
            }
        }

        For_Gates(N, w){
            if (active[w] >= FIRST_CUT && (w == gate_Lut6 || isLogic(w))){
                len_total += arrival[w] + depart_[w];
                len_count++;
            }
        }
    }

    double T = realTime();
    WriteLn "Delay: %_    Wires: %,d    LUTs: %,d   [iter=%t  total=%t]", mapped_delay, (uint64)total_wires, (uint64)total_luts, T - T0, T;

    if (len_count > 0)
        WriteLn "Average path length: %.2f", len_total / len_count;

    if (P.batch_output && iter == P.n_iters - 1){
        Write "%>11%,d    ", (uint64)total_luts;
        Write "%>11%,d    ", (uint64)total_wires;
        Write "%>6%d    "  , (uint64)mapped_delay;
        Write "%>10%t"     , realTime();
        NewLine;
    }
}


void TechMap::run()
{
    assert(!N.is_frozen);

    // Normalize netlist:
    if (Has_Gob(N, Strash))
        Remove_Gob(N, Strash);

    assert(isCanonical(N));
    normalizeLut4s(N);

    // Prepare for mapping:
    cutmap    .reserve(N.size());
    fanout_est.reserve(N.size());
    depart    .reserve(N.size());
    active    .reserve(N.size());
    fanouts   .reserve(N.size());
    mux_fanout.reserve(N.size());
    winner    .reserve(N.size());

    target_arrival = 0;

    reserveImpls(2 + P.use_fmux);

    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            fanout_est(w) = nFanouts(w);
            depart    (w) = FLT_MAX;
            active    (w) = ACTIVE;
        }
    }
    cuts_enumerated = 0;

    if (remap){
        For_All_Gates(N, w)
            bufmap(w) = w;
    }

    // Map:
    Vec<gate_id> order;
#if 1
    upOrderBfs(N, order);
#else
    Vec<GLit> order_;
    upOrder(N, order_);
    for (uint i = 0; i < order_.size(); i++) order.push(order_[i].id);
    order_.clear(true);
#endif

    for (iter = 0; iter < P.n_iters; iter++){
        if (iter < P.recycle_iter)
            mem.clear();

        double T0 = realTime();

        // Generate cuts:
        findDualPhaseGates();       // -- may change during mapping due to 'bypassTrivialCutsets()' which rewires the circuit.
        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            generateCuts(w); }

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


static void compact(Gig& N, WMapX<GLit>& remap) ___unused;
static void compact(Gig& N, WMapX<GLit>& remap)
{
    GigRemap m;
    N.compact(m);
    m.applyTo(remap.base());
}


void techMap(Gig& N, const Vec<Params_TechMap>& Ps, WMapX<GLit>* remap)
{
    // Add protectors for bad netlists:
    Vec<uint> protectors;
    {
        Assure_Gob(N, FanoutCount);
        For_Gates(N, w){
            if (nFanouts(w) == 0 && !isCO(w)){
                WriteLn "WARNING! Fanout-free non-combinational output: %_", w;
                protectors.push(N.add(gate_PO).init(w).num());
            }
        }
        if (protectors.size() > 0)
            WriteLn "Protectors added: %_", protectors.size();
    }

    Vec<Trip<GLit,GLit,uint> > co_srcs;
    if (remap){
        remap->initBuiltins();

        WSeen seen;
        For_Gates(N, w){
            if (!isLogic(w)){
                For_Inputs(w, v){
                    if (isLogic(v) && !seen.has(v)){
                        co_srcs.push(tuple(w, v, Iter_Var(v)));
                        seen.add(v);
                    }
                }
            }
        }
    }


    // Make sure netlist is topologically sorted:
    N.unstrash();
    if (!isCanonical(N)){
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

    // Techmap:
    assert(Ps.size() >= 1);
    for (uint round = 0; round < Ps.size(); round++){
        //if (round > 0)
        {
            WMapX<GLit> xlat;
            unmap(N, &xlat, Ps[round].unmap);
            N.unstrash();
            if (Ps[round].unmap_to_ands)
                expandXigGates(N);
            compact(N, xlat);

            if (remap){
                Vec<GLit>& v = remap->base();
                for (uint i = gid_FirstUser; i < v.size(); i++)
                    v[i] = xlat[v[i]];
            }

            NewLine;
            WriteLn "Unmap.: %_", info(N);

            if (Ps[round].refactor /*hack*/&& round <= 1){  // <<== only apply in round 0?
                Params_Refactor P;
                P.quiet = true;
                refactor(N, xlat, P);
                if (remap){
                    Vec<GLit>& v = remap->base();
                    for (uint i = gid_FirstUser; i < v.size(); i++)
                        v[i] = xlat[v[i]];
                }
                WriteLn "Refact: %_", info(N);
            }
            NewLine;
        }
        TechMap map(N, Ps[round], remap);

        map.run();
    }

    if (!Ps.last().batch_output){
        NewLine;
        WriteLn "Legalization..."; }
    removeInverters(N, remap, Ps.last().batch_output);

    // Patch up remap from sources of combinational outputs:
    if (remap){
        for (uint i = 0; i < co_srcs.size(); i++){
            Wire w = (*remap)[co_srcs[i].fst] + N; assert(+w);
            Wire v = (*remap)[co_srcs[i].snd] + N;
            uint pin = co_srcs[i].trd;
            if (!+v || v.sign){
                assert(!w[pin].sign);
                (*remap)(+co_srcs[i].snd) = w[pin] ^ co_srcs[i].snd.sign;
            }
        }
    }

    // Remove protectors:
    if (protectors.size() > 0){
        for (uint i = 0; i < protectors.size(); i++)
            remove(N(gate_PO, protectors[i]));
    }
}


void techMap(Gig& N, const Params_TechMap& P, uint n_rounds, WMapX<GLit>* remap)
{
    assert(n_rounds >= 1);
    Vec<Params_TechMap> Ps;
    for (uind i = 0; i < n_rounds; i++){
        Ps.push(P);
        /**/if (i != n_rounds-1) Ps.last().exact_local_area = false;
    }
    //**/WriteLn "DON'T FORGET TO REMOVE DELAY FACTOR HACK!";
    //**/Ps[0].delay_factor = 1.2;
    //**/Ps[1].delay_factor = 1.1;
    //**/Ps[2].delay_factor = 1.0;
    techMap(N, Ps, remap);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
 - unmap in round 0 (to get rid of Lut4s) -- followed by coarsening? or is Unmap good enough
   for coarsening?

 - fix ELA for F7s (hard work)
*/
