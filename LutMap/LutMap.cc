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

#define DELAY_FRACTION 1.0      // -- 'arg' value of 'Delay' gates is divided by this value


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut representation:


#define Cut LutMap_Cut      // -- avoid linking problems


class Cut {                // -- this class represents 6-input cuts.
    void extendAbstr(gate_id g) { abstr |= 1u << (g & 31); }

    gate_id inputs[6];
    uint    sz;
public:
    uint    abstr;

    Cut(Tag_empty) : sz(0), abstr(0) {}
    Cut(gate_id g) : sz(1), abstr(0) { inputs[0] = g; extendAbstr(g); }
    Cut()          : sz(7)           {}

    uint    size()                const { return sz; }
    gate_id operator[](int index) const { return inputs[index]; }
    bool    null()                const { return uint(sz) > 6; }
    void    mkNull()                    { sz = 7; }

    void    push(gate_id g) { if (!null()){ inputs[sz++] = g; extendAbstr(g); } }
};

#define Cut_NULL Cut()


template<> fts_macro void write_(Out& out, const Cut& v)
{
    if (v.null())
        FWrite(out) "<null>";
    else{
        out += '{';
        if (v.size() > 0){
            out += v[0];
            for (uint i = 1; i < v.size(); i++)
                out += ',', ' ', v[i];
        }
        out += '}';
    }
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


// Check if cut 'c' is a subset of cut 'd'. Cuts must be sorted. FTB is ignored.
macro bool subsumes(const Cut& c, const Cut& d)
{
    assert_debug(!c.null());
    assert_debug(!d.null());

    if (d.size() < c.size())
        return false;

    if (c.abstr & ~d.abstr)
        return false;

    if (c.size() == d.size()){
        for (uint i = 0; i < c.size(); i++)
            if (c[i] != d[i])
                return false;
    }else{
        uint j = 0;
        for (uint i = 0; i < c.size(); i++){
            while (c[i] != d[j]){
                j++;
                if (j == d.size())
                    return false;
            }
        }
    }

    return true;
}


macro bool moreThanSixBits(uint a)
{
  #if defined(__GNUC__)
    return __builtin_popcount(a) > 6;
  #else
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    return a;
  #endif
}


// PRE-CONDITION: Inputs of 'cut1' and 'cut2' are sorted.
// Output: A cut representing AND of 'cut1' and 'cut2' with signs 'inv1' and 'inv2' respectively;
// or 'Cut_NULL' if more than 6 inputs would be required.
static
Cut combineCuts_Bin(const Cut& cut1, const Cut& cut2)
{
    if (moreThanSixBits(cut1.abstr | cut2.abstr))
        return Cut_NULL;

    Cut   result(empty_);
    uint  i = 0;
    uint  j = 0;
    if (cut1.size() == 0) goto FlushCut2;
    if (cut2.size() == 0) goto FlushCut1;
    for(;;){
        if (result.size() == 6) return Cut_NULL;
        if (cut1[i] < cut2[j]){
            result.push(cut1[i]), i++;
            if (i >= cut1.size()) goto FlushCut2;
        }else if (cut1[i] > cut2[j]){
            result.push(cut2[j]), j++;
            if (j >= cut2.size()) goto FlushCut1;
        }else{
            result.push(cut1[i]), i++, j++;
            if (i >= cut1.size()) goto FlushCut2;
            if (j >= cut2.size()) goto FlushCut1;
        }
    }

  FlushCut1:
    if (result.size() + cut1.size() - i > 6) return Cut_NULL;
    while (i < cut1.size())
        result.push(cut1[i]), i++;
    goto Done;

  FlushCut2:
    if (result.size() + cut2.size() - j > 6) return Cut_NULL;
    while (j < cut2.size())
        result.push(cut2[j]), j++;
    goto Done;

  Done:
    return result;
}

// PRE-CONDITION: Inputs of 'cut0..3' are sorted.
// Output: A cut representing the merge of 'cut0..3' after applying function 'ftb'; 
// or 'Cut_NULL' if more than 6 inputs would be required.
static
Cut combineCuts_Tern(const Cut& cut0, const Cut& cut1, const Cut& cut2)
{
    if (moreThanSixBits(cut0.abstr | cut1.abstr | cut2.abstr))
        return Cut_NULL;

    // Merge inputs from cuts:
    Cut result(empty_);
    uint i0 = 0, i1 = 0, i2 = 0;
    for(;;){
        gate_id x0 = (i0 == cut0.size()) ? gid_MAX : cut0[i0];
        gate_id x1 = (i1 == cut1.size()) ? gid_MAX : cut1[i1];
        gate_id x2 = (i2 == cut2.size()) ? gid_MAX : cut2[i2];
        gate_id smallest = min_(min_(x0, x1), x2);

        if (smallest == gid_MAX) break;
        if (result.size() == 6) return Cut_NULL;

        if (x0 == smallest) i0++;
        if (x1 == smallest) i1++;
        if (x2 == smallest) i2++;

        result.push(smallest);
    }

    return result;
}


// PRE-CONDITION: Inputs of 'cut0..3' are sorted.
// Output: A cut representing the merge of 'cut0..3' after applying function 'ftb'; 
// or 'Cut_NULL' if more than 6 inputs would be required.
static
Cut combineCuts_Quad(const Cut& cut0, const Cut& cut1, const Cut& cut2, const Cut& cut3)
{
    if (moreThanSixBits(cut0.abstr | cut1.abstr | cut2.abstr | cut3.abstr))
        return Cut_NULL;

    // Merge inputs from cuts:
    Cut result(empty_);
    uint i0 = 0, i1 = 0, i2 = 0, i3 = 0;
    for(;;){
        gate_id x0 = (i0 == cut0.size()) ? gid_MAX : cut0[i0];
        gate_id x1 = (i1 == cut1.size()) ? gid_MAX : cut1[i1];
        gate_id x2 = (i2 == cut2.size()) ? gid_MAX : cut2[i2];
        gate_id x3 = (i3 == cut3.size()) ? gid_MAX : cut3[i3];
        gate_id smallest = min_(min_(x0, x1), min_(x2, x3));

        if (smallest == gid_MAX) break;
        if (result.size() == 6) return Cut_NULL;

        if (x0 == smallest) i0++;
        if (x1 == smallest) i1++;
        if (x2 == smallest) i2++;
        if (x3 == smallest) i3++;

        result.push(smallest);
    }

    return result;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// LutMap class:


struct LutMap_Cost {
    uint    idx;
    uint    cut_size;
    float   delay;
    float   area;
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
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<float>       arrival;
    WMap<float>       depart;               // -- 'FLT_MAX' marks a deactivated node

    uint              round;
    uint64            cuts_enumerated;      // -- for statistics
    float             target_arrival;

    uint64            mapped_area;
    float             mapped_delay;

    // Internal methods:
    void  evaluateCuts(Wire w, Array<Cut> cuts);
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


macro Pair<Cut, Array<Cut> > getCuts(Wire w, const WMap<Array<Cut> >& cutmap)
{
    if (w == gate_Const)
        return tuple(Cut(empty_), Array<Cut>(empty_));
    else
        return tuple(Cut(w.id), cutmap[w]);
}


// Add 'cut' to 'out' performing subsumption tests in both diretcions. If cut is constant or
// trivial, FALSE is returned (abort the cut enumeration), otherwise TRUE.
static
bool applySubsumptionAndAddCut(const Cut& cut, Vec<Cut>& out)
{
    if (cut.size() <= 1){
        // Constant cut, buffer or inverter:
        out.clear();
        out.push(cut);
        return false;
    }

    // Test for subsumption (note that in presence of subsumption, the resulting cut set is no longer unique)
    for (uint k = 0; k < out.size(); k++){
        if (subsumes(out[k], cut)){
            // Cut is subsumed by existing cut; don't add anything:
            return true; }

        if (subsumes(cut, out[k])){
            // Cut subsumes at least one existing cut; need to remove them all:
            out[k] = cut;
            for (k++; k < out.size();){
                assert_debug(!subsumes(out[k], cut));
                if (subsumes(cut, out[k])){
                    out[k] = out.last();
                    out.pop();
                }else
                    k++;
            }
            return true;
        }
    }
    out.push(cut);  // (non-subsuming and non-subsumed cut)
    return true;
}


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
// Cut evalutation:


/*
Om inte omgenererar cuts, spara hälften delay-opt, hälften area-opt?
Om växlar snabba (återanvänder cuts) och långsamma faser (nya cuts), hantera fanout-est blendingen annorlunda.
Om genererar nya cuts, spara bästa (eller bästa k stycken) cut från förra fasen.
*/

struct Delay_lt {
    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const {
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
      #if 0
        return x.area < y.area;
      #else
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        return x.cut_size < y.cut_size;
      #endif
    }
};


struct Area_lt {
    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const {
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
      #if 0
        return (x.delay < y.delay);
      #else
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
        return x.cut_size < y.cut_size;
      #endif
    }
};


void LutMap::evaluateCuts(Wire w, Array<Cut> cuts)
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

        for (uint j = 0; j < cuts[i].size(); j++){
            Wire w = cuts[i][j] + N;
            newMax(costs[i].delay, arrival[w]);
            costs[i].area += area_est[w];
        }
        costs[i].area += 1;     // -- LUT cost = 1
    }

    // Compute order:
    sobSort(sob(costs, Delay_lt()));
    if (round > 0){
        float req_time;
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

        Array<Cost> suf = costs.slice(min_(j, P.cuts_per_node / 2));
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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


/*
delay optimal everywhere
globally delay optimal, use slack for area recovery

departure estimation for non-mapped nodes? worth being conservative by having 'pessimistic_departure' using unmapped cuts?
make sure to keep previous best choice as an option in each cut list!
*/

void LutMap::generateCuts_LogicGate(Wire w, Vec<Cut>& out)
{
    //**/if (w == gate_And){ generateCuts_And(w, out); return; }
    assert(w == gate_And || w == gate_Lut4);

    Array<Cut> cs[4];
    Cut        triv[4];
    int        lim[4];
    uint       sz;
    for (sz = 0; sz < w.size(); sz++){
        if (+w[sz] == Wire_NULL) break;
        l_tuple(triv[sz], cs[sz]) = getCuts(w[sz], cutmap);
        lim[sz] = keep.has(w[sz]) ? 0 : (int)cs[sz].size();
    }

    // Compute cross-product:
    switch (sz){
    case 0:
    case 1: {
        Cut cut(w[0].id);
        if (!applySubsumptionAndAddCut(cut, out)) return;
        break;
    }

    case 2:{
        for (int i0 = -1; i0 < lim[0]; i0++){ const Cut& c0 = (i0 == -1) ? triv[0] : cs[0][i0];
        for (int i1 = -1; i1 < lim[1]; i1++){ const Cut& c1 = (i1 == -1) ? triv[1] : cs[1][i1];
            Cut cut = combineCuts_Bin(c0, c1);
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out)) return;
        }}
        break;
    }

    case 3:{
        for (int i0 = -1; i0 < lim[0]; i0++){ const Cut& c0 = (i0 == -1) ? triv[0] : cs[0][i0];
        for (int i1 = -1; i1 < lim[1]; i1++){ const Cut& c1 = (i1 == -1) ? triv[1] : cs[1][i1];
        for (int i2 = -1; i2 < lim[2]; i2++){ const Cut& c2 = (i2 == -1) ? triv[2] : cs[2][i2];
            Cut cut = combineCuts_Tern(c0, c1, c2);
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out)) return;
        }}}
        break;
    }

    case 4:{
        for (int i0 = -1; i0 < lim[0]; i0++){ const Cut& c0 = (i0 == -1) ? triv[0] : cs[0][i0];
        for (int i1 = -1; i1 < lim[1]; i1++){ const Cut& c1 = (i1 == -1) ? triv[1] : cs[1][i1];
        for (int i2 = -1; i2 < lim[2]; i2++){ const Cut& c2 = (i2 == -1) ? triv[2] : cs[2][i2];
        for (int i3 = -1; i3 < lim[3]; i3++){ const Cut& c3 = (i3 == -1) ? triv[3] : cs[3][i3];
            Cut cut = combineCuts_Quad(c0, c1, c2, c3);
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out)) return;
        }}}}
        break;
    }

    default: assert(false); }
}


/*
void LutMap::generateCuts_And(Wire w, Vec<Cut>& out)
{
    assert(w == gate_And);
    assert(out.size() == 0);

    Wire u = w[0], v = w[1];
    Array<Cut> cs, ds;
    Cut triv_u, triv_v;
    l_tuple(triv_u, cs) = getCuts(u, cutmap);
    l_tuple(triv_v, ds) = getCuts(v, cutmap);

    // Compute cross-product:
    int cs_lim = keep.has(u) ? 0 : (int)cs.size();
    int ds_lim = keep.has(v) ? 0 : (int)ds.size();

    for (int i = -1; i < cs_lim; i++){
        const Cut& c = (i == -1) ? triv_u : cs[i];
        for (int j = -1; j < ds_lim; j++){
            const Cut& d = (j == -1) ? triv_v : ds[j];

            Cut cut = combineCuts_Bin(c, d);
            if (!cut.null() && !applySubsumptionAndAddCut(cut, out))
                return;
        }
    }
}

*/


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
            cuts.clear();
            generateCuts_LogicGate(w, cuts);
            cuts_enumerated += cuts.size();
            evaluateCuts(w, cuts.slice());
            cuts.shrinkTo(P.cuts_per_node);
            cutmap(w) = Array_copy(cuts, mem);
        }else
            evaluateCuts(w, cutmap[w]);
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

      #if 0
        N.setMode(gig_Lut6);
        N.assertMode();
      #endif
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void LutMap::run()
{
    round = 0;

    area_est  .reserve(N.size());
    fanout_est.reserve(N.size());

    // Initialize fanout estimation (and zero area estimation):
    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            area_est  (w) = 0;
            fanout_est(w) = nFanouts(w);
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

        if (!P.quiet){
            if (round == 0)
                WriteLn "cuts_enumerated=%,d", cuts_enumerated;
            WriteLn "round=%d   mapped_area=%,d   mapped_delay=%_   [enum: %t, blend: %t]", round, mapped_area, mapped_delay, T1-T0, T2-T1;
        }

      #if 1
        if (round == 0){
            for (uint i = 0; i < cutmap.base().size(); i++)
                dispose(cutmap.base()[i], mem);
            cutmap.clear();
        }
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
