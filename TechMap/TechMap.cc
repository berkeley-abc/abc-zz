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
sizes  : 4 uchars: #inputs, offset-ftb, offset-selection, unused
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
    uchar&  offSel_()      { return ((uchar*)&base[0])[6]; }
    uchar&  reserved_()    { return ((uchar*)&base[0])[7]; }

    uint&   input_(uint i) { return ((uint*)&base[1])[i]; }
    uint64& ftb_(uint i)   { return base[offFtb_() + i]; }
    uchar&  sel_(uint i)   { return ((uchar*)&base[offSel_()])[i]; }

    void init(Array<gate_id> inputs, uint64* ftb, uchar* sel);

public:
    Cut() : base(NULL) {}
    Cut(uint64* base_) : base(base_) {}

    static uint allocSz(uint n_inputs);
    Cut(uint64* base_, Array<gate_id> inputs, uint64* ftb, uchar* sel) : base(base_) { init(inputs, ftb, sel); }
        // -- size of 'ftb' should be 'ceil(2^n / 64)' i.e. '(1 << inputs.size() + 63) >> 6'.

    Null_Method(Cut) { return base == NULL; }

    uint sig()                        const { return me().sig_(); }
    uint size()                       const { return me().size_(); }
    gate_id operator[](int input_num) const { return me().input_(input_num); }

    uint64 ftb(uint word_num = 0)     const { return me().ftb_(word_num); }
    uchar  sel(uint byte_num)         const { return me().sel_(byte_num); }

    uint ftbSz() const { return ((1ull << size()) + 63) >> 6; }
};


// Returns the number of 'uint64's to allocate for a cut of size 'n_inputs'.
inline uint Cut::allocSz(uint n_inputs)
{
    uint ftb_words = ((1ull << n_inputs) + 63) >> 6;
    uint sel_words = (n_inputs + 7) >> 3;
    uint alloc_sz = ((n_inputs + 3) >> 1) + ftb_words + sel_words;
    return alloc_sz;
}


inline void Cut::init(Array<gate_id> inputs, uint64* ftb, uchar* sel)
{
    assert(inputs.size() <= 32);
    uint ftb_words = ((1ull << inputs.size()) + 63) >> 6;
    uint sel_words = (inputs.size() + 7) >> 3;
    uint alloc_sz = ((inputs.size() + 3) >> 1) + ftb_words + sel_words;

    size_()     = inputs.size();
    offFtb_()   = (inputs.size() + 3) >> 1;
    offSel_()   = offFtb_() + ftb_words;
    reserved_() = 0;
    assert(offSel_() + sel_words == alloc_sz);

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

    for (uint i = 0; i < (inputs.size() + 7) >> 3; i++)
        base[offSel_() + i] = 0;     // -- avoid uninitialized memory
    for (uint i = 0; i < inputs.size(); i++)
        sel_(i) = sel[i];
}


template<> fts_macro void write_(Out& out, const Cut& v)
{
    FWrite(out) "Cut{";
    for (uint i = 0; i < v.size(); i++){
        if (i != 0) FWrite(out) ", ";
        FWrite(out) "w%_:%d", v[i], v.sel(i); }
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
    uint off(uint i) const { return ((uint*)data)[i+1]; }

    friend class DynCutSet;
    CutSet(uint64* data_) : data(data_) {}

public:
    CutSet() : data(NULL) {}
    Null_Method(CutSet) { return data == NULL; }

    uint size() { return ((uint*)data)[0]; }
    Cut operator[](uint i) const { return Cut(&data[off(i)]); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Dynamic cut-set:


// Usage: call 'begin()', define cut by modifying public variables, call 'next()', repeat this 
// step, finally call 'done()' to get a static 'CutSet'. The dynamic cut set can then be reused.
class DynCutSet {
    Vec<uint64> mem;
    Vec<uint>   off;

    void clearCut() { inputs.clear(); ftb.clear(); sel.clear(); }

    void storeCut() {
        off.push((uint)mem.size());
        mem.growTo(mem.size() + Cut::allocSz(inputs.size()));
        if (sel.size() == 0) sel.growTo(inputs.size(), 0);
        Cut(&mem[off[LAST]], inputs.slice(), ftb.base(), sel.base());
    }

public:
  //________________________________________
  //  Add a cut:

    Vec<gate_id> inputs;    // }- these fields must be populated before calling 'next()'
    Vec<uint64>  ftb;       // }
    Vec<uchar>   sel;       // -- this field can be left empty ('next()' will pad with zeros)

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
#if 1
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

    //**/WriteLn "====================";
    //**/for (uint i = 0; i < mem_needed; i++)
    //**/    WriteLn "%>2%d: %.16x", i, data[i];
#else
    // <<== NEEDS TO BE REDONE IN PRESENCE OF 'swap()' and' 'shrinkTo()'
    uint off_adj = (off.size() + 2) >> 1;
    uint64* data = allocator.alloc(mem.size() + off_adj);
    uint*   tab = (uint*)data;
    tab[0] = off.size();

    for (uint i = 0; i < off.size(); i++)
        tab[i+1] = off[i] + off_adj;
    if ((off.size() & 1) == 0) tab[off.size() + 1] = 0;    // -- avoid uninitialized memory

    for (uint i = 0; i < mem.size(); i++)
        data[i + off_adj] = mem[i];

    //**/WriteLn "--------------------";
    //**/for (uint i = 0; i < mem.size() + off_adj; i++)
    //**/    WriteLn "%>2%d: %.16x", i, data[i];
#endif

    return CutSet(data);
}


//=================================================================================================
// -- Helpers:


// Check if the support of 'c' is a subset of the support of 'd'. Does NOT assume cuts to be
// sorted. The FTB and cut selection ('sel') is not used.
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
// TechMap class:


struct Params_TechMap {
    uint  cut_size;             // -- maximum cut size
    float delay_fraction;       // -- delays in 'gate_Delay' are multiplied by this value

    Params_TechMap() :
        cut_size      (6),
        delay_fraction(1.0)
    {}
};


struct LutMap_Cost {
    uint    idx;
    float   delay;
    float   area;
};


class TechMap {
    typedef LutMap_Cost Cost;

    // Input:
    Gig&                  N;
    const Params_TechMap& P;

    // State:
    StackAlloc<uint64>  mem;
    WMap<CutSet>        cutmap;
    WMap<Cut>           winner;
    WMap<float>         area_est;
    WMap<float>         fanout_est;
    WMap<float>         arrival;
    WMap<float>         depart;
    WMap<uchar>         active;

    uint                iter;
    float               target_arrival;

    // Temporaries:
    DynCutSet           dcuts;
    Vec<Cost>           tmp_costs;
    Vec<uint>           tmp_where;
    Vec<uint>           tmp_list;

    // Statistics:
    uint64              cuts_enumerated;

    // Internal methods:
    void run();

    void generateCuts(Wire w);
    void generateCuts_LogicGate(Wire w, DynCutSet& out_dcuts);
    void prioritizeCuts(Wire w, DynCutSet& dcuts);
    void updateEstimates();

public:
    TechMap(Gig& N_, const Params_TechMap& P_ = Params_TechMap()) : N(N_), P(P_) { run(); }
};


//=================================================================================================
// -- Cut generation:


#include "TechMap_CutGen.icc"


struct Area_lt {
    bool operator()(const LutMap_Cost& x, const LutMap_Cost& y) const {
        if (x.area < y.area) return true;
        if (x.area > y.area) return false;
        if (x.delay < y.delay) return true;
        if (x.delay > y.delay) return false;
        return false;
    }
};


void TechMap::prioritizeCuts(Wire w, DynCutSet& dcuts)
{
    // Compute costs:
    Vec<Cost>& costs = tmp_costs;
    costs.setSize(dcuts.size());

    for (uint i = 0; i < dcuts.size(); i++){
        costs[i].idx = i;
        costs[i].delay = 0.0f;
        costs[i].area  = 0.0f;

        for (uint j = 0; j < dcuts[i].size(); j++){
            Wire w = dcuts[i][j] + N;
            newMax(costs[i].delay, arrival[w]);
            costs[i].area += area_est[w];
        }
        costs[i].area += dcuts[i].size();       // <<==  P.lut_cost[cuts[i].size()];
        costs[i].delay += 1.0f;
    }

    // Compute order:
    sobSort(sob(costs, Area_lt()));

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

    //
    area_est(w) = costs[0].area / fanout_est[w];
    arrival(w)  = costs[0].delay;
    dcuts.shrinkTo(8);
}


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
    case gate_FF:
        // Global sources have only the implicit trivial cut:
        area_est(w) = 0;
        arrival(w) = 0;
        break;

    case gate_Bar:
    case gate_Sel:
        // Treat barriers and pin selectors as PIs except for delay:
        area_est(w) = 0;
        arrival(w) = arrival[w[0]];
        break;

    case gate_Delay:{
        // Treat delay gates as PIs except for delay:
        area_est(w) = 0;
        float arr = 0;
        For_Inputs(w, v)
            newMax(arr, arrival[v]);
        arrival(w) = arr + w.arg() * P.delay_fraction;
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
    active.clear();

    For_Gates(N, w){
        if (isCO(w))
            active(w) = true;
    }

    For_DownOrder(N, w){
        if (!active[w]) continue;

        if (isLogic(w)){
            //<<== reprioritizeCuts(w, cutmap[w]);
            const Cut& cut = cutmap[w][0];
            for (uint i = 0; i < cut.size(); i++)
                active(cut[i] + N) = true;

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
                const Cut& cut = cutmap[w][0];
                for (uint i = 0; i < cut.size(); i++){
                    Wire v = cut[i] + N;
                    fanouts(v)++;
                    newMax(depart(v), depart[w] + 1.0f);    // <<== something different here for MUX7?
                }
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

    // Prepare for mapping:
    target_arrival = 0;

    cutmap    .reserve(N.size());
    winner    .reserve(N.size());
    area_est  .reserve(N.size());
    fanout_est.reserve(N.size());
    arrival   .reserve(N.size());
    depart    .reserve(N.size());
    active    .reserve(N.size());

    {
        Auto_Gob(N, FanoutCount);
        For_Gates(N, w){
            area_est  (w) = 0;
            fanout_est(w) = nFanouts(w);
            arrival   (w) = FLT_MAX;
            depart    (w) = FLT_MAX;
            active    (w) = true;
        }
    }

    cuts_enumerated = 0;

    // Map:     <<== use Iter_Params here.... (how to score cuts, how to update target delay, whether to recycle cuts...)
    for (iter = 0; iter < 1; iter++){
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
    }


    // Cleanup:

    /**/mem.report();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Testing:


void test()
{
#if 0
    DynCutSet cuts;
    cuts.begin();

    cuts.inputs += 100, 200, 300, 400, 500, 600, 700;
    cuts.ftb += 0xABBADEAD00112233ull, 0x0123456789abcdefull;
    cuts.sel += 7, 6, 5, 4, 3, 2, 1;
    cuts.next();

    cuts.inputs += 111, 222;
    cuts.ftb += 0xCC;
    cuts.sel += 9, 8;
    cuts.next();

    StackAlloc<uint64> mem;
    CutSet final = cuts.done(mem);

    for (uint i = 0; i < final.size(); i++)
        Dump(final[i]);
#endif

  #if 1
    Gig N;
    readAigerFile("/home/een/ZZ/LutMap/comb/nm.aig", N, false);

  #else
    Gig N;
    Wire x = N.add(gate_PI);
    Wire y = N.add(gate_PI);
    Wire z = N.add(gate_PI);
    Wire f = N.add(gate_And).init(x, y);
    Wire g = N.add(gate_And).init(~x, z);
    Wire h = N.add(gate_And).init(~f, ~g);
    Wire t ___unused = N.add(gate_PO).init(~h);
  #endif

    TechMap map(N);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
