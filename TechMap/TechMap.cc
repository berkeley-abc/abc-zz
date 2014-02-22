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
#include "ZZ_Gig.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_BFunc.hh"
#include "ZZ_Npn4.hh"
#include "ZZ/Generics/Sort.hh"

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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
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
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


macro bool isLogic(Wire w) {
    return w == gate_And || w == gate_Lut4; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Cut' class:


/*
Cut layout:

sig    : 1 uint
sizes  : 4 uchars: #inputs, offset-ftb, unused, unused
inputs : 'size' uints
pad    : 0 or 1 uint (to get to 64-bit align)
ftb    : '2^size / 64' uint64s, rounded up.
*/

class Cut {
public:
private:
    uint64* base;

    Cut&    me() const     { return *const_cast<Cut*>(this); }

    uint&   sig_()         { return ((uint*)&base[0])[0]; }

    uchar&  size_()        { return ((uchar*)&base[0])[4]; }    // -- 'size == n_inputs'
    uchar&  offFtb_()      { return ((uchar*)&base[0])[5]; }
    uchar&  reserved0_()   { return ((uchar*)&base[0])[6]; }
    uchar&  reserved1_()   { return ((uchar*)&base[0])[7]; }

    uint&   input_(uint i) { return ((uint*)&base[1])[i]; }
    uint64& ftb_(uint i)   { return base[offFtb_() + i]; }

    void init(Array<gate_id> inputs, uint64* ftb);

public:
    Cut() : base(NULL) {}
    Cut(uint64* base_) : base(base_) {}

    static uint allocSz(uint n_inputs);
    Cut(uint64* base_, Array<gate_id> inputs, uint64* ftb) : base(base_) { init(inputs, ftb); }
        // -- size of 'ftb' should be 'ceil(2^n / 64)' i.e. '(1 << inputs.size() + 63) >> 6'.

    Null_Method(Cut) { return base == NULL; }

    uint sig()                        const { return me().sig_(); }
    uint size()                       const { return me().size_(); }
    gate_id operator[](int input_num) const { return me().input_(input_num); }

    uint64 ftb(uint word_num = 0)     const { return me().ftb_(word_num); }

    uint ftbSz() const { return ((1ull << size()) + 63) >> 6; }
};


// Returns the number of 'uint64's to allocate for a cut of size 'n_inputs'.
inline uint Cut::allocSz(uint n_inputs)
{
    uint ftb_words = ((1ull << n_inputs) + 63) >> 6;
    uint alloc_sz = ((n_inputs + 3) >> 1) + ftb_words;
    return alloc_sz;
}


inline void Cut::init(Array<gate_id> inputs, uint64* ftb)
{
    assert(inputs.size() <= 32);
    uint ftb_words = ((1ull << inputs.size()) + 63) >> 6;
    uint alloc_sz = ((inputs.size() + 3) >> 1) + ftb_words;

    size_()      = inputs.size();
    offFtb_()    = (inputs.size() + 3) >> 1;
    reserved0_() = 0;
    reserved1_() = 0;
    assert(offFtb_() + ftb_words == alloc_sz);

    uint sig_mask = 0;
    for (uint i = 0; i < inputs.size(); i++)
        sig_mask |= 1ull << (inputs[i] & 31);
    sig_() = sig_mask;

    for (uint i = 0; i < inputs.size(); i++)
        input_(i) = inputs[i];
    if ((inputs.size() & 1) == 1)
        input_(inputs.size()) = 0;   // -- avoid uninitialized memory

    for (uint i = 0; i < ftb_words; i++)
        ftb_(i) = ftb[i];
}


template<> fts_macro void write_(Out& out, const Cut& v)
{
    FWrite(out) "Cut{";
    for (uint i = 0; i < v.size(); i++){
        if (i != 0) FWrite(out) ", ";
        FWrite(out) "w%_", v[i]; }
    FWrite(out) "}[";

    for (uint i= 0; i < v.ftbSz(); i++){    // -- perhaps reverse order
        if (i != 0) FWrite(out) ", ";
        FWrite(out) "0x%.16X", v.ftb(i); }
    FWrite(out) "]";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Static cut-set:


/*
Cutset layout:

n_cuts  : 1 uint
offsets : 'n_cuts' uints
pad     : 0 or 1 uint (to get to 64-bit align)
cuts    : several uint64s
*/

class CutSet {
    uint64* data;

    uint  off (uint i) const { return ((uint*)data)[i+1]; }
    uint& off_(uint i)       { return ((uint*)data)[i+1]; }

    friend class DynCutSet;
    CutSet(uint64* data_) : data(data_) {}

public:
    CutSet() : data(NULL) {}
    Null_Method(CutSet) { return data == NULL; }

    uint size() const { return ((uint*)data)[0]; }
    Cut operator[](uint i) const { return Cut(&data[off(i)]); }

    void swap(uint i, uint j) { swp(off_(i), off_(j)); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Dynamic cut-set:


// Usage: call 'begin()', define cut by modifying public variables, call 'next()', repeat this
// step, finally call 'done()' to get a static 'CutSet'. The dynamic cut set can then be reused.
class DynCutSet {
    Vec<uint64> mem;
    Vec<uint>   off;

    void clearCut() { inputs.clear(); ftb.clear(); }

    void storeCut() {
        off.push((uint)mem.size());
        mem.growTo(mem.size() + Cut::allocSz(inputs.size()));
        Cut(&mem[off[LAST]], inputs.slice(), ftb.base());
    }

public:
  //________________________________________
  //  Add a cut:

    Vec<gate_id> inputs;    // }- these fields must be populated before calling 'next()'
    Vec<uint64>  ftb;       // }

    void begin()  { mem.clear(); off.clear(); clearCut(); }
    void next ()  { storeCut(); clearCut(); }       // -- 'next()' must be called before the final 'done()'

  //________________________________________
  //  Manipulate cut set:

    uint size() const      { return off.size(); }         // -- returns the number of cuts stored by 'next()' calls
    Cut operator[](uint i) { return Cut(&mem[off[i]]); }  // -- cut is invalidated if cutset is changed (by calling 'next()')

    void swap    (uint i, uint j) { swp(off[i], off[j]); }
    void pop     ()               { assert(off.size() > 0); off.pop(); }
    void shrinkTo(uint new_size)  { if (new_size <= off.size()) off.shrinkTo(new_size); }

  //________________________________________
  //  Finalize:

    template<class ALLOC>   // -- NOTE! 'ALLOC' should allocate 'uint64's
    CutSet done(ALLOC& allocator);
};


template<class ALLOC>
CutSet DynCutSet::done(ALLOC& allocator)
{
    uint mem_needed = 0;
    for (uint i = 0; i < off.size(); i++)
        mem_needed += Cut::allocSz(Cut(&mem[off[i]]).size());

    uint off_adj = (off.size() + 2) >> 1;
    mem_needed += off_adj;
    uint64* data = allocator.alloc(mem_needed);
    uint*   tab = (uint*)data;
    tab[0] = off.size();

    for (uint i = 0; i < off.size(); i++){
        tab[i+1] = off_adj;

        uint sz = Cut::allocSz(Cut(&mem[off[i]]).size());
        memcpy(&data[off_adj], &mem[off[i]], sz * sizeof(uint64));
        off_adj += sz;
    }
    assert(off_adj == mem_needed);
    if ((off.size() & 1) == 0) tab[off.size() + 1] = 0;    // -- avoid uninitialized memory

    return CutSet(data);
}


//=================================================================================================
// -- Helpers:


// Check if the support of 'c' is a subset of the support of 'd'. Does NOT assume cuts to be
// sorted. The FTB is not used.
macro bool subsumes(const Cut& c, const Cut& d)
{
    assert_debug(c);
    assert_debug(d);

    if (d.size() < c.size())
        return false;

    if (c.sig() & ~d.sig())
        return false;

    for (uint i = 0; i < c.size(); i++){
        if (!has(d, c[i]))
            return false;
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut implementation:


// NOTE! Through out the code, 'impl' is indexed 'impl[sel][wire]' where 
//   'sel == 0' is delay optimal
//   'sel == 1' is area optimal
//   'sel == 2..' are trade-off choices ("balanced" cuts)


struct CutImpl {
    enum { TRIV_CUT = -1, NO_CUT = -2 };    // -- used with 'idx'
    int     idx;
    float   arrival;
    float   area_est;

    CutImpl() : idx(NO_CUT), arrival(0.0f), area_est(0.0f) {}
};


// Returns '(arrival, area_est)'; arrival and area for an area optimal selection UNDER
// the assumption that 'arrival_lim' is not exceeded. If no such cut exists, 
// '(FLT_MAX, FLT_MAX') is returned.  If 'sel' is non-null, cut selection is recorded there.
template<class CUT>
inline Trip<float,float,bool> cutImpl_bestArea(CUT cut, const Vec<WMap<CutImpl> >& impl, float arrival_lim, uchar* sel = NULL)
{
    // Find best arrival:
    float arrival = 0;
    for (uint i = 0; i < cut.size(); i++)
        newMax(arrival, impl[0][GLit(cut[i])].arrival);     // -- impl[0] is delay optimal

    bool late = false;
    if (arrival_lim == -FLT_MAX)            // -- special value => return delay optimal selection
        arrival_lim = arrival;
    else if (arrival > arrival_lim){
        late = true;
        arrival_lim = arrival;
    }

    // Pick best area meeting that arrival:
    float total_area_est = 0;
    for (uint i = 0; i < cut.size(); i++){
        GLit p = GLit(cut[i]);
        float area_est = impl[0][p].area_est;
        uint best_j = 0;
        for (uint j = 1; j < impl.size(); j++){
            if (impl[j][p].idx == CutImpl::NO_CUT) break;
            if (impl[j][p].arrival <= arrival_lim){
                if (newMin(area_est, impl[j][p].area_est))
                    best_j = j;
            }
        }

        newMax(arrival, impl[best_j][p].arrival);
        total_area_est += area_est;

        if (sel)
            sel[i] = best_j;
    }

    return tuple(arrival, total_area_est, late);
}


// Returns '(arrival, area_est)'; arrival and area of a delay optimal selection for 'cut'. 
// If 'sel' is non-null, cut selection is recorded there.
template<class CUT>
inline Pair<float,float> cutImpl_bestDelay(CUT cut, const Vec<WMap<CutImpl> >& impl, uchar* sel = NULL)
{
    Trip<float,float,bool> t = cutImpl_bestArea(cut, impl, -FLT_MAX, sel);
    assert(!t.trd);
    return tuple(t.fst, t.snd);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Costs for sorting cuts:


struct Cost {
    uint    idx;
    bool    late;       // -- implementation does not meet timing requirement
    float   delay;
    float   area;
};


template<> fts_macro void write_(Out& out, const Cost& v)
{
    FWrite(out) "Cost{idx=%_; late=%_; delay=%_; area=%_}", v.idx, v.late, v.delay, v.area;
}


struct Area_lt {
    bool operator()(const Cost& x, const Cost& y) const {
        if (!x.late && y.late) return true;
        if (x.late && !y.late) return false;
        if (!x.late){
            if (x.area < y.area) return true;
            if (x.area > y.area) return false;
            if (x.delay < y.delay) return true;
            if (x.delay > y.delay) return false;
        }else{
            if (x.delay < y.delay) return true;
            if (x.delay > y.delay) return false;
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
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        return false;
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// TechMap class:


struct Params_TechMap {
    uint        cut_size;             // -- maximum cut size
    float       delay_fraction;       // -- delay value in 'gate_Delay' are multiplied by this value
    uint        balanced_cuts;        // -- #cuts to compute between "best area" and "best delay"
    float       delta_delay;          // -- minimum delay improvement to consider when computing balanced cuts.
    Vec<float>  lut_cost;

    Params_TechMap() :
        cut_size      (6),
        delay_fraction(1.0),
        balanced_cuts (2),
        delta_delay   (0.1)
    {
        for (uint i = 0; i <= cut_size; i++)        // -- default LUT cost is "number of inputs" 
            lut_cost.push(i);
    }
};


class TechMap {
    // Input:
    Gig&                  N;
    const Params_TechMap& P;

    // State:
    StackAlloc<uint64>  mem;
    WMap<CutSet>        cutmap;
//    WMap<Cut>           winner;
    Vec<WMap<CutImpl> > impl;

    WMap<float>         fanout_est;
    WMap<float>         depart;
    WMap<uchar>         active; // <<== change to 'current choice' or 'sel'_

    uint                iter;
    float               target_arrival;

    // Temporaries:
    DynCutSet           dcuts;
    Vec<Cost>           tmp_costs;
    Vec<uint>           tmp_where;
    Vec<uint>           tmp_list;

    // Statistics:
    uint64              cuts_enumerated;

    // Helper methods:

    // Major internal methods:
    void generateCuts(Wire w);
    void generateCuts_LogicGate(Wire w, DynCutSet& out_dcuts);
    void prioritizeCuts(Wire w, DynCutSet& dcuts);
    void updateEstimates();

public:
    TechMap(Gig& N_, const Params_TechMap& P_) : N(N_), P(P_) {}
    void run();
};


//=================================================================================================
// == Cut prioritization:


#if 0

    [internal] arrival     <<== make internal?
    [internal] area_est    <<== make internal?

fanout_est

iter_no
target_arrival
depart
active

lut_cost (mux_cost)

temporaries...


CutImpl {
    current choice (
    k scored cuts
    n-k more cuts for enumeration purposes


categories: lut6, mux7, mux8
spread (delay heavy, area heavy,

for each cut:
  selectors     (0 = trivial cut? or -1?
  area_est
  arrival
#endif



/*
0 1 2 3 4 5 6 7

A A A A A D D M
*/


void TechMap::prioritizeCuts(Wire w, DynCutSet& dcuts)
{
//    uchar* sel = (uchar*)alloca(P.cut_size);

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

        costs[i].area  += P.lut_cost[dcuts[i].size()];
        costs[i].delay += 1.0f;
    }

    // Iteration specific cut sorting and cut implementations:
    if (iter == 0){
        assert(impl.size() == 1);
        sobSort(sob(costs, Delay_lt()));

    }else{
        float req_time = active[w] ? target_arrival - depart[w] - 1.0f : best_delay;
        /**/if (!active[w]){
        /**/    For_Inputs(w, v)
        /**/        if (active[v]) newMax(req_time, target_arrival - depart[v]);
        /**/}
        if (req_time < best_delay)
            req_time = best_delay;   // -- if we change heuristics so we cannot always meet timing, at least do the best we can

        for (uint i = 0; i < dcuts.size(); i++){
            costs[i].idx = i;
            l_tuple(costs[i].delay, costs[i].area, costs[i].late) = cutImpl_bestArea(dcuts[i], impl, req_time);
            assert(costs[i].delay >= best_delay);

            costs[i].area += P.lut_cost[dcuts[i].size()];
            costs[i].delay += 1.0f;
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

    // Store implementations:
    impl[0](w).idx      = 0;
    impl[0](w).area_est = costs[0].area / fanout_est[w];
    impl[0](w).arrival  = costs[0].delay;

    if (impl.size() > 1){
        uint idx = (costs[0].area < costs[1].area) ? 0 : 1;
        impl[1](w).idx      = idx;
        impl[1](w).area_est = costs[idx].area / fanout_est[w];
        impl[1](w).arrival  = costs[idx].delay;

        //**/float req_time = active[w] ? target_arrival - depart[w] - 1.0f : best_delay;
        //**/if (idx == 1 && impl[1][w].arrival <= req_time){
        //**/    dcuts.swap(0, 1);
        //**/    swp(impl[0](w), impl[1](w)); }
    }

    dcuts.shrinkTo(10);
}


//=================================================================================================
// -- Cut generation:


#include "TechMap_CutGen.icc"


void TechMap::generateCuts(Wire w)
{
    assert(!w.sign);

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

        if (impl.size() >= 1){
            CutImpl& ma = impl[1](w);
            ma.idx = CutImpl::TRIV_CUT;
            bool late;
            l_tuple(ma.arrival, ma.area_est, late) = cutImpl_bestArea(w, impl, FLT_MAX);
            assert(!late);

            float prev_arrival = ma.arrival;
            for (uint i = 2; i < impl.size(); i++){
                CutImpl& ma = impl[i](w);
                if (prev_arrival - P.delta_delay < md.arrival){
                    ma.idx = CutImpl::NO_CUT;
                    break; }

                l_tuple(ma.arrival, ma.area_est, late) = cutImpl_bestArea(w, impl, prev_arrival - P.delta_delay);
                if (late){
                    ma.idx = CutImpl::NO_CUT;
                    break;

                }else{
                    ma.idx = CutImpl::TRIV_CUT;
                    prev_arrival = ma.arrival;
                }
            }
        }

        // Add box delay:
        for (uint i = 0; i < impl.size(); i++)
            impl[i](w).arrival += w.arg() * P.delay_fraction;

        break;}

    case gate_And:
    case gate_Lut4:
        // Logic:
  //      if (!cutmap[w]){    // -- else reuse cuts from previous iteration

          #if 0     // <<== LATER
            if (!winner[w].null())
                cuts.push(winner[w]);     // <<==FT cannot push last winner if mux_depth gets too big
          #endif

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

//        }else
//            prioritizeCuts(w, cutmap[w]);   // <<== need to be able to update a static cut-set!
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

    if (!skip){
        cutmap(w) = dcuts.done(mem);
        //**/WriteLn "cutmap[%_]:  arrival=%_  area_est=%_", w, arrival[w], area_est[w];
        //**/for (uint i = 0; i < cutmap[w].size(); i++) WriteLn "  %_", cutmap[w][i];
    }
}


//=================================================================================================
// -- Update estimates:


void TechMap::updateEstimates()
{
#if 0
    active.clear();

    For_Gates(N, w){
        if (isCO(w))
            active(w) = true;
    }

    For_All_Gates_Rev(N, w){
        if (!active[w]) continue;

        if (isLogic(w)){
            //<<== reprioritizeCuts(w, cutmap[w]);
            const Cut& cut = cutmap[w][0];
            for (uint i = 0; i < cut.size(); i++)
                active(cut[i] + N) = true;
          #endif

        }else if (!isCI(w)){
            For_Inputs(w, v)
                active(v) = true;
        }
    }

    WMap<uint> fanouts(N, 0);
    fanouts.reserve(N.size());
    depart.clear();
    For_All_Gates_Rev(N, w){
        if (isLogic(w)){
            if (active[w]){
              #if 0
                CutImpl m;
                bool    late;
                l_tuple(m.arrival, m.area_est, late) = cutImpl_bestArea(w, impl, target_arrival);
              #else
                const Cut& cut = cutmap[w][0];
                for (uint i = 0; i < cut.size(); i++){
                    Wire v = cut[i] + N;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);    // <<== something different here for MUX7?
                }
              #endif
            }else
                depart(w) = FLT_MAX;    // <<== for now, give a well defined value to inactive nodes

        }else if (!isCI(w)){
            float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
            For_Inputs(w, v){
                fanouts(v)++;
                newMax(depart(v), depart[w] + delay);
            }
        }
    }

#else
    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    active.clear();
    depart.clear();
    WMap<uint> fanouts(N, 0);
    fanouts.reserve(N.size());

    For_All_Gates_Rev(N, w){
        if (isCO(w))
            active(w) = true;
        if (!active[w]) continue;

        if (isLogic(w)){

        }else{
            if (!isCI(w)){
                float delay = (w != gate_Delay) ? 0.0f : w.arg() * P.delay_fraction;
                For_Inputs(w, v){
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + delay);
                }
            }
        }

#endif

    For_Gates(N, w)
        assert(!active[w] || depart[w] != FLT_MAX);

    float mapped_delay = 0.0f;
    For_Gates(N, w)
        if (isCI(w))
            newMax(mapped_delay, depart[w]);
    /**/WriteLn "Delay: %_", mapped_delay;

    double total_area = 0;
    double total_luts = 0;
    For_Gates(N, w){
        if (active[w] && isLogic(w)){
            total_area += cutmap[w][0].size();
            total_luts += 1;
        }
    }

    /**/Dump(total_area, total_luts);

    if (iter == 0)
        target_arrival = mapped_delay;
}


//=================================================================================================
// -- Main:


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
    //**/writeDot("N.dot", N); WriteLn "Wrote: N.dot";


    // Prepare for mapping:
    target_arrival = 0;

    cutmap    .reserve(N.size());
    //winner    .reserve(N.size());     // <<==
    fanout_est.reserve(N.size());
    depart    .reserve(N.size());
    active    .reserve(N.size());

    impl.push();
    impl[0].reserve(N.size());

    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            fanout_est(w) = nFanouts(w);
            depart    (w) = FLT_MAX;
            active    (w) = true;
        }
    }

    cuts_enumerated = 0;

    // Map:     <<== use Iter_Params here.... (how to score cuts, how to update target delay, whether to recycle cuts...)
    for (iter = 0; iter < 2; iter++){
        if (iter == 1){
            impl.push();
            impl[1].reserve(N.size());
        }

        // Generate cuts:
        double T0 ___unused = cpuTime();
        For_All_Gates(N, w)
            generateCuts(w);
        double T1 ___unused = cpuTime();

        // Finalize mapping:
        //<<== set target delay (at least if iter 0)
        //<<== if not recycling cuts, set 'winner'

        // Computer estimations:
        updateEstimates();

        mem.clear();
    }


    // Cleanup:
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Testing:


}
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "GigReader.hh"
namespace ZZ {


void test(int argc, char** argv)
{
#if 0
    uint64 ftb = 0x8888888888888888ull;
    WriteLn "ftb: %.16x", ftb;
    ftb = ftb6_swap(ftb, 1, 0);
    WriteLn "ftb: %.16x", ftb;
    ftb = ftb6_swap(ftb, 2, 1);
    WriteLn "ftb: %.16x", ftb;

#else

  #if 1
    cli.add("input", "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.parseCmdLine(argc, argv);
    String input = cli.get("input").string_val;

    Gig N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, false);
        else if (hasExtension(input, "gnl"))
            N.load(input);
        else if (hasExtension(input, "gig"))
            readGigForTechmap(input, N);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

  #else
    Gig N;
    Wire x0 = N.add(gate_PI);
    Wire x1 = N.add(gate_PI);
    Wire x2 = N.add(gate_PI);
    Wire x3 = N.add(gate_PI);
    Wire x4 = N.add(gate_PI);
    Wire x5 = N.add(gate_PI);
    Wire x6 = N.add(gate_PI);
    Wire a1 = N.add(gate_And).init(x0, x1);
    Wire a2 = N.add(gate_And).init(a1, x2);
    Wire a3 = N.add(gate_And).init(a2, x3);
    Wire a4 = N.add(gate_And).init(a3, x4);
    Wire a5 = N.add(gate_And).init(a4, x5);
    Wire a6 = N.add(gate_And).init(a5, x6);
    Wire t ___unused = N.add(gate_PO).init(a6);
  #endif

    Params_TechMap P;
    TechMap map(N, P);
    map.run();
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
