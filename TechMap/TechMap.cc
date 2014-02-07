//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TechMap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"

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


*/
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

    Cut& me() const { return *const_cast<Cut*>(this); }

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
// Cut sets:


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


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


class DynCutSet {
    Vec<uint64> mem;
    Vec<uint>   off;

    void clearCut() { inputs.clear(); ftb.clear(); sel.clear(); }

    void storeCut() {
        off.push((uint)mem.size());
        mem.growTo(mem.size() + Cut::allocSz(inputs.size()));
        Cut(&mem[off[LAST]], inputs.slice(), ftb.base(), sel.base());
    }

public:
    Vec<gate_id> inputs;
    Vec<uint64>  ftb;
    Vec<uchar>   sel;

    void begin() { mem.clear(); off.clear(); clearCut(); }
    void next()  { storeCut(); clearCut(); }        // -- 'next()' must be called before the final 'done()'

    template<class ALLOC>   // -- NOTE! Should allocate 'uint64's
    CutSet done(ALLOC& allocator) {
        uint off_adj = (off.size() + 2) >> 1;
        uint64* data = allocator.alloc(mem.size() + off_adj);
        uint*   tab = (uint*)data;
        tab[0] = off.size();

        for (uint i = 0; i < off.size(); i++)
            tab[i+1] = off[i] + off_adj;
        if ((off.size() & 1) == 0) tab[off.size() + 1] = 0;    // -- avoid uninitialized memory 

        for (uint i = 0; i < mem.size(); i++)
            data[i + off_adj] = mem[i];
        return CutSet(data);
    }
};


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


class TechMap {
    // Input:
    Gig&    N;

    // State:
    StackAlloc<uint64> mem;
    WMap<CutSet>      cutmap;
    WMap<Cut>         winner;
    WMap<float>       area_est;
    WMap<float>       fanout_est;
    WMap<float>       arrival;
    WMap<float>       depart;
    WMap<uchar>       active;

    uint              iter;
    float             target_arrival;

    void run();

    void generateCuts(Wire w);


public:
    TechMap(Gig& N_) : N(N_) { run(); }
};


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

    // Map:     <<== use Iter_Params here.... (how to score cuts, how to update target delay, whether to recycle cuts...)
    for (iter = 0; iter < 1; iter++){
        // Generate cuts:
        double T0 = cpuTime();
        For_All_Gates(N, w)
            generateCuts(w);
        double T1 = cpuTime();
        /**/Dump(T1 - T0);

        // Finalize mapping:

        //<<== set target delay (at least if iter 0)
        //<<== if not recycling cuts, set 'winner'

        // Computer estimations:
    }


    // Cleanup:
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut generation:


void TechMap::generateCuts(Wire w)
{ }
#if 0
void LutMap::generateCuts(Wire w)
{
    assert(!w.sign);

    uint64 ftb;
    uchar  sel;

    switch (w.type()){
    case gate_Const:
        ftb = (w == GLit_True) ? 0xFFFFFFFFFFFFFFFFull : 0;
         Cut(mem, Array<gate_id>(empty_), &ftb, NULL);

    Cut(Alloc& mem, Array<gate_id> inputs, uint64* ftb, uchar* sel);
        ...
    case gate_Reset:    // -- not used, but is part of each netlist
    case gate_Box:      // -- treat sequential boxes like a global source
    case gate_PI:
    case gate_FF:
        // Base case -- Global sources:
        cutmap(w) = Array<Cut>(empty_);     // -- only the trivial cut
        area_est(w) = 0;
        arrival(w) = 0;
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

    case gate_And:
    case gate_Lut4:
        // Inductive case:
        if (!cutmap[w]){
            Vec<Cut>& cuts = tmp_cuts;
            cuts.clear();   // -- keep last winner
            if (!winner[w].null())
                cuts.push(winner[w]);     // <<==FT cannot push last winner if mux_depth gets too big
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

    default:
        ShoutLn "INTERNAL ERROR! Unhandled gate type: %_", w.type();
        assert(false);
    }
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Testing:


void test()
{
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
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
