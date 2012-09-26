//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CutSim.cc
//| Author(s)   : Niklas Een
//| Module      : CutSim
//| Description : Generalization of ternary simulation.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "CutSim.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
Tre "input" typer:
  - PI
  - abstrakta floppar (pseudo-PI)
  - floppar i frame 0
  
- Alla inputs kan vara 0/1/X. 
- Simulera med cuts en k-upprullning.
- Stöd inkrementell uppdatering
- Ibland är endast värdet i vissa punkter av intresse (så gör en callback eller liknande när 
  någon av dem ändrar sitt värde i en inkrementell simulering).
- Markera fanin-kon av punkter av intresse och simulera endast inom den?  

- Någon motsvarighet till "reason? (för justifieringsendamål)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constants and Helpers:


static const ushort proj[4] = { 0xAAAA, 0xCCCC, 0xF0F0, 0xFF00 };


static uchar bits_set[256];

ZZ_Initializer(bits_set, 0)
{
    for (uint i = 0; i < 256; i++){
        uint n = 0;
        for (uint j = 0; j < 8; j++){
            if (i & (1 << j))
                n++;
        }
        bits_set[i] = n;
    }
}

macro uint countBits(ushort ftb) {
    return bits_set[ftb & 0xFF] + bits_set[ftb >> 8]; }


perm4_t qrotate[4];

ZZ_Initializer(qrotate, 0){
    qrotate[0] = pseq4_to_perm4[pseq4Make(3, 0, 1, 2)];     // <<== byt mot konstanter PERM_XXXX
    qrotate[1] = pseq4_to_perm4[pseq4Make(0, 3, 1, 2)];
    qrotate[2] = pseq4_to_perm4[pseq4Make(0, 1, 3, 2)];
    qrotate[3] = pseq4_to_perm4[pseq4Make(0, 1, 2, 3)];
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'CutSim_Cut':


class CutSim_Cut {
    gate_id in[4];
    uint    abstr : 24;     // <<== ta bort! används ju inte!
    uint    xs    : 5;
    uint    sz    : 3;

    static uint abstrMask(gate_id g) { return 1u << (g % 24); }

public:
  //________________________________________
  //  Public state:

    ushort  ftb_lo;
    ushort  ftb_hi;

  //________________________________________
  //  Constructors:

    CutSim_Cut(lbool val) {
        sz = 0;
        xs = 0;
        abstr = 0;
        if      (val == l_Undef) ftb_lo = 0, ftb_hi = 0xFFFF;
        else if (val == l_False) ftb_lo = 0, ftb_hi = 0;
        else if (val == l_True ) ftb_lo = 0xFFFF, ftb_hi = 0xFFFF;
        else                     assert(false);
    }

    CutSim_Cut(gate_id g, lbool val_g0, lbool val_g1) {
        in[0] = g;
        sz = 1;
        xs = 0;
        if (val_g0 == l_Undef) xs += 8;
        if (val_g1 == l_Undef) xs += 8;
        abstr = abstrMask(g);
        ftb_lo = ftb_hi = 0;

        if      (val_g0 == l_Undef) ftb_hi |= 0x5555;
        else if (val_g0 == l_True ) ftb_lo |= 0x5555, ftb_hi |= 0x5555;
        else    assert(val_g0 == l_False);

        if      (val_g1 == l_Undef) ftb_hi |= 0xAAAA;
        else if (val_g1 == l_True ) ftb_lo |= 0xAAAA, ftb_hi |= 0xAAAA;
        else    assert(val_g1 == l_False);
    }

  //________________________________________
  //  Null cut:

    CutSim_Cut() { sz = 7; }
    void mkNull() { sz = 7; }
    Null_Method(CutSim_Cut) { return sz > 4; }
        // -- growing a cut beyond size 4 will make it "null" (but must catch it immediately, or size may wrap)

  //________________________________________
  //  Read access:

    gate_id operator[](uint i) const { assert(i < sz); return in[i]; }
    uint size() const { return sz; }

    bool operator==(const CutSim_Cut& other) const;

    uint nUndefs() const { return xs; }
    bool betterThan(const CutSim_Cut& other) const;
    bool subsetOf(const CutSim_Cut& other) const;

  //________________________________________
  //  Modify:

    void push(gate_id g) { assert(sz < 4); in[sz++] = g; }
    void neg () { ftb_lo ^= 0xFFFF; ftb_hi ^= 0xFFFF; swp(ftb_lo, ftb_hi); }    // -- does not require 'finalize'
    void elim(uint i);           // -- remove input 'i' and shift remaining inputs down. FTB must NOT depend on the removed input
    void trim();                 // -- remove all inputs that are semantically unused
    void translate(gate_id off); // -- add 'off' to all inputs

    void finalize();    // -- must be called after pushing new inputs onto the cut or changing its FTBs
};


macro CutSim_Cut operator~(CutSim_Cut c) { c.neg(); return c; }
macro CutSim_Cut operator^(CutSim_Cut c, bool s) { return s ? ~c : c; }


template<> fts_macro void write_(Out& out, const CutSim_Cut& v)
{
    if (v.size() == 0)
        FWrite(out) "[]:";
    else{
        FWrite(out) "[%d", v[0];
        for (uint i = 1; i < v.size(); i++)
        FWrite(out) " %d", v[i];
        FWrite(out) "]:";
    }

    for (uint i = 16; i > 0;){ i--;
        bool hi = (v.ftb_hi >> i) & 1;
        bool lo = (v.ftb_lo >> i) & 1;
        if         ( hi &&  lo)  out += '1';
        else if    (!hi && !lo)  out += '0';
        else assert( hi && !lo), out += '*';

        if (i % 4 == 0 && i !=0)
            out += ',';
    }
}


inline bool CutSim_Cut::operator==(const CutSim_Cut& other) const
{
    if (ftb_lo != other.ftb_lo) return false;
    if (ftb_hi != other.ftb_hi) return false;
    if (sz != other.sz) return false;
    for (uint i = 0; i < sz; i++)
        if (in[i] != other.in[i]) return false;
    return true;
}


inline void CutSim_Cut::finalize()
{
    assert(!null());
    abstr = 0;
    for (uint i = 0; i < sz; i++)
        abstr |= abstrMask(in[i]);
    xs = countBits(ftb_hi & (0xFFFF^ftb_lo));
}


inline bool CutSim_Cut::betterThan(const CutSim_Cut& other) const
{
    return nUndefs() < other.nUndefs()
        || (nUndefs() == other.nUndefs() && size() < other.size());
}


inline bool CutSim_Cut::subsetOf(const CutSim_Cut& other) const
{
    assert(!null());
    if ((abstr & other.abstr) != abstr)
        return false;
    for (uint i = 0; i < sz; i++){
        for (uint j = 0; j < other.sz; j++){
            if (other.in[j] == in[i])
                goto Found;
        }
        return false;
      Found:;
    }
    return true;
}


void CutSim_Cut::elim(uint i)
{
    assert(i < sz);
    //**/WriteLn "  (about to eliminate pin %_ from %_)", i, *this;
    ftb_hi = apply_perm4[qrotate[i]][ftb_hi];
    ftb_lo = apply_perm4[qrotate[i]][ftb_lo];
    // <<==assert
    sz--;
    for (uint j = i; j < sz; j++)
        in[j] = in[j+1];
    finalize();
    //**/WriteLn "  (result:                         %_)", *this;
}


void CutSim_Cut::trim()
{
    if (sz == 0) goto Done;
    while ((ftb_lo & 0x5555) == ((ftb_lo & 0xAAAA) >> 1) && (ftb_hi & 0x5555) == ((ftb_hi & 0xAAAA) >> 1)){
        ftb_lo = apply_perm4[PERM4_3012][ftb_lo];
        ftb_hi = apply_perm4[PERM4_3012][ftb_hi];
        in[0] = in[1];
        in[1] = in[2];
        in[2] = in[3];
        sz--;
        if (sz == 0) goto Done;
    }

    if (sz == 1) goto Done;
    while ((ftb_lo & 0x3333) == ((ftb_lo & 0xCCCC) >> 2) && (ftb_hi & 0x3333) == ((ftb_hi & 0xCCCC) >> 2)){
        ftb_lo = apply_perm4[PERM4_0312][ftb_lo];
        ftb_hi = apply_perm4[PERM4_0312][ftb_hi];
        in[1] = in[2];
        in[2] = in[3];
        sz--;
        if (sz == 1) goto Done;
    }

    if (sz == 2) goto Done;
    while ((ftb_lo & 0x0F0F) == ((ftb_lo & 0xF0F0) >> 4) && (ftb_hi & 0x0F0F) == ((ftb_hi & 0xF0F0) >> 4)){
        ftb_lo = apply_perm4[PERM4_0132][ftb_lo];
        ftb_hi = apply_perm4[PERM4_0132][ftb_hi];
        in[2] = in[3];
        sz--;
        if (sz == 2) goto Done;
    }

    if (sz == 3) goto Done;
    if ((ftb_lo & 0x00FF) == ((ftb_lo & 0xFF00) >> 8) && (ftb_hi & 0x00FF) == ((ftb_hi & 0xFF00) >> 8))
        sz--;

  Done:
    finalize();
}


void CutSim_Cut::translate(gate_id off)
{
    for (uint i = 0; i < sz; i++)
        in[i] += off;
    finalize();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Local typedefs:


typedef CutSim_Cut Cut;
typedef Array<Cut> Cuts;

static const CutSim_Cut Cut_Undef(l_Undef);
static const CutSim_Cut Cut_True (l_True);
static const CutSim_Cut Cut_False(l_False);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut manipulation:


static
Cut unitCut(Wire w)
{
    if (type(w) == gate_Const){
        assert(id(w) == gid_True);      // -- must be proper AIG
        return Cut(sign(w) ? l_False : l_True);

    }else{
        NetlistRef N = netlist(w);
        Get_Pob(N, fanout_count);
        if (fanout_count[w] <= 1)
            return Cut(l_Undef);        // -- single-fanout nodes are immediately abstracted
        else
            return sign(w) ? Cut(id(w), l_True, l_False) : Cut(id(w), l_False, l_True);
    }
}


static
void mergeInputLists(const Cut& c, const Cut& d, Pair<uchar,uchar> where[8], uint& sz)
{
    sz = 0;

    uint i = 0, j = 0;
    if (c.size() == 0) goto FlushC;
    if (d.size() == 0) goto FlushD;
    for(;;){
        if (c[i] < d[j]){
            where[sz] = tuple(i, 255);
            sz++; i++;
            if (i == c.size()) goto FlushD;
        }else{
            where[sz] = tuple(255, j);
            if (c[i] == d[j]){
                where[sz].fst = i;
                i++;
                if (i == c.size()){
                    sz++; j++;
                    goto FlushD; }
            }
            sz++; j++;
            if (j == d.size()) goto FlushC;
        }
    }

  FlushC:
    while (i < c.size()){
        where[sz] = tuple(i, 255);
        sz++, i++; }
    return;

  FlushD:
    while (j < d.size()){
        where[sz] = tuple(255, j);
        sz++; j++; }
}


macro Pair<ushort, ushort> quantPin(ushort ftb_lo, ushort ftb_hi, uint pin)
{
    uint s  = 1u << pin;
    ushort hi = (ftb_hi & proj[pin]) | ((ftb_hi & (0xFFFF^proj[pin])) << s);
    hi |= hi >> s;
    ushort lo = (ftb_lo & proj[pin]) & ((ftb_lo & (0xFFFF^proj[pin])) << s);
    lo |= lo >> s;
    return tuple(lo, hi);
}


static
uint elimCost(const Cut& c, uint pin)
{
    // Compute FTBs for 'c' with 'pin' quantified away:
    ushort lo, hi;
    l_tuple(lo, hi) = quantPin(c.ftb_lo, c.ftb_hi, pin);

    // Count how many constants turned to Xs:
    ushort cmask = (c.ftb_lo & c.ftb_hi) | (0xFFFF^(c.ftb_lo | c.ftb_hi));
    ushort xmask = hi & (0xFFFF^lo);
    uint   cost = countBits(cmask & xmask);

    //**/Cut k = c;
    //**/k.ftb_lo = lo;
    //**/k.ftb_hi = hi;
    //**/Dump(c);
    //**/Dump(k, cost, pin);

    return cost;
}


static
void quantify(Cut& c, uint pin)
{
    //**/WriteLn "QUANT before: %_ (pin=%_)", c, pin;
    l_tuple(c.ftb_lo, c.ftb_hi) = quantPin(c.ftb_lo, c.ftb_hi, pin);
    c.finalize();
    //**/WriteLn "      after : %_", c;
}


static
void addCut(Vec<Cut>& cuts, const Cut& c, uint max_cuts)
{
    //**/WriteLn "ADD CUT: %_   (max %_)", c, max_cuts;
    //**/for (uint i = 0; i < cuts.size(); i++)
    //**/    WriteLn "  %_", cuts[i];

    if (c.nUndefs() == 16) return;                                      // -- no information in this cut
    if (cuts.size() == max_cuts && cuts.last().betterThan(c)) return;   // -- we have only room for better cuts

    for (uint i = 0; i < cuts.size(); i++){
        if (cuts[i].subsetOf(c)){
            if (c.size() > cuts[i].size() || c == cuts[i])
                return;         // -- 'c' is (heuristically) subsumed by existing cut

        }else if (c.subsetOf(cuts[i]))
            cuts[i].mkNull();   // -- tag cut for removal (subsumed by 'c')
    }

    // Remove cuts:
    uint j = 0;
    for (uint i = 0; i < cuts.size(); i++)
        if (!cuts[i].null())
            cuts(j++) = cuts[i];
    cuts.shrinkTo(j);

    // Add 'c' in proper place:
    uint pos = cuts.size();
    for (uint i = 0; i < cuts.size(); i++){
        if (c.betterThan(cuts[i])){
            pos = i;
            break;
        }
    }

    cuts.push();
    for (uint i = cuts.size()-1; i > pos; i--)
        cuts[i] = cuts[i-1];
    cuts[pos] = c;

    cuts.shrinkTo(max_cuts);

    //**/WriteLn "RESULT:";
    //**/for (uint i = 0; i < cuts.size(); i++)
    //**/    WriteLn "  %_", cuts[i];
    //**/NewLine;
}


static
void combineCutPair(Cut c, Cut d, Vec<Cut>& out_cuts, uint max_cutsize, uint cuts_per_node, uint heuristic_cutoff)
{
    //**/Write "combineCutPair: ";
    //**/Dump(c, d);
    if (c.size() + d.size() > heuristic_cutoff) return;

    // Make sure 'c' and 'd' together has only 4 distinct inputs:
    if (c.size() + d.size() > max_cutsize){
        Pair<uchar,uchar> where[8];
        uint sz;
        mergeInputLists(c, d, where, sz);
        //**/for (uint i = 0; i < sz; i++)
        //**/    WriteLn "where[%_] = (%d, %d)", i, where[i].fst, where[i].snd;
        if (sz > max_cutsize){
            // Compute costs:
            uint cost[8];
            for (uint i = 0; i < sz; i++){
                cost[i] = 0;
                if (where[i].fst != 255) cost[i] += elimCost(c, where[i].fst);
                if (where[i].snd != 255) cost[i] += elimCost(d, where[i].snd);
                //**/Dump(cost[i]);
            }

            // Quantify away variables with smallest cost:
            //**/Dump(sz);
            for (uint n = 0; n < sz - max_cutsize; n++){
                uint best_i = n;
                uint best = cost[n];
                for (uint i = n+1; i < sz; i++){
                    if (newMin(best, cost[i]))
                        best_i = i;
                }
                swp(where[n], where[best_i]);
                swp(cost [n], cost [best_i]);

                if (where[n].fst != 255){
                    //**/WriteLn "(Quant C -- %_)", n;
                    quantify(c, where[n].fst); }
                if (where[n].snd != 255){
                    //**/WriteLn "(Quant D -- %_)", n;
                    quantify(d, where[n].snd); }
            }
        }
        c.trim();
        d.trim();
    }

    // Merge inputs and normalize FTBs:
    Cut   result(l_Undef);
    uchar perm_c[4] = {0, 1, 2, 3};
    uchar perm_d[4] = {0, 1, 2, 3};
    uint  i = 0;
    uint  j = 0;
    if (c.size() == 0) goto FlushD;
    if (d.size() == 0) goto FlushC;
    //**/WriteLn "MERGE: %_  AND  %_", c, d;
    for(;;){
        //**/Dump(i, j);
        if (c[i] < d[j]){
            swp(perm_c[i], perm_c[result.size()]);
            result.push(c[i]), i++;
            if (i >= c.size()) goto FlushD;
        }else if (c[i] > d[j]){
            swp(perm_d[j], perm_d[result.size()]);
            result.push(d[j]), j++;
            if (j >= d.size()) goto FlushC;
        }else{
            swp(perm_c[i], perm_c[result.size()]);
            swp(perm_d[j], perm_d[result.size()]);
            result.push(c[i]), i++, j++;
            if (i >= c.size()) goto FlushD;
            if (j >= d.size()) goto FlushC;
        }
    }

  FlushC:
    while (i < c.size()){
        swp(perm_c[i], perm_c[result.size()]);
        result.push(c[i]), i++; }
    goto Done;

  FlushD:
    while (j < d.size()){
        swp(perm_d[j], perm_d[result.size()]);
        result.push(d[j]), j++; }
    goto Done;

  Done:;

    // Compute new FTB:
    c.ftb_lo = apply_perm4[pseq4_to_perm4[pseq4Make(perm_c[0], perm_c[1], perm_c[2], perm_c[3])]][c.ftb_lo];
    c.ftb_hi = apply_perm4[pseq4_to_perm4[pseq4Make(perm_c[0], perm_c[1], perm_c[2], perm_c[3])]][c.ftb_hi];
    d.ftb_lo = apply_perm4[pseq4_to_perm4[pseq4Make(perm_d[0], perm_d[1], perm_d[2], perm_d[3])]][d.ftb_lo];
    d.ftb_hi = apply_perm4[pseq4_to_perm4[pseq4Make(perm_d[0], perm_d[1], perm_d[2], perm_d[3])]][d.ftb_hi];
    //**/Dump(c, d);

    result.ftb_lo = c.ftb_lo & d.ftb_lo;
    result.ftb_hi = c.ftb_hi & d.ftb_hi;
    //**/WriteLn "Before trimming: %_", result;
    result.trim();  // -- will finalize

    // Check if subsumes/subsumed:
    addCut(out_cuts, result, cuts_per_node);
    //**/Dump(result);


}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'CutSim':


class CutSim {
    NetlistRef      N;
    StackAlloc<Cut> cutmem;
    WMap<Cuts>      cutmap;
    WMapL<Cuts>     ff_map;     // -- 'cutmap' for state-outputs (flops are considered inputs)

  //________________________________________
  //  Helpers:

    template<template<class> class V>
    Cuts allocCuts(const V<Cut>& cuts, bool neg = false);

    Cuts mergeCuts(Wire u, Wire v);     // -- AND two cuts together, possibly quantifying away inputs
    Cuts dupCuts(Wire w);

public:
  //________________________________________
  //  Parameters:

    uint max_cutsize;       // -- Between 0..4
    uint cuts_per_node;     // -- Greater than or equal to 1
    uint heuristic_cutoff;  // -- Between 0..8

  //________________________________________
  //  Public:

    CutSim();
    CutSim(NetlistRef N) { init(N); }

    void init(NetlistRef N_){
        N = N_;
        assert(checkNumberingFlops(N));
        Assure_Pob0(N, fanout_count);
        Assure_Pob0(N, up_order);
        reset();

        max_cutsize = 4;
        cuts_per_node = 8;
        heuristic_cutoff = 6;
    }

    void  reset();          // -- initialize state to 'flop_init'.
    void  simulateCycle();
    void  weakenState();    // -- replace exact cut-information for FFs with 0/1/X

    lbool get  (Wire w);                        // -- read a single wire
    lbool getFF(Wire w);                        // -- read a single flop
    void  getState(Vec<lbool>& tstate);         // -- read ternary state of flops (approximation of real state)
    void  setState(const Vec<lbool>& tstate);   // -- set ternary state of flops
};


void CutSim::reset()
{
    Get_Pob(N, flop_init);
    Vec<Cut> sing(1);
    For_Gatetype(N, gate_Flop, w){
        if      (flop_init[w] == l_True ){ sing[0] = Cut(l_True) ; ff_map(w) = allocCuts(sing); }
        else if (flop_init[w] == l_False){ sing[0] = Cut(l_False); ff_map(w) = allocCuts(sing); }
        else if (flop_init[w] == l_Undef){ ff_map(w) = Cuts(empty_); }
        else    assert(false);
    }
}


// Return the state of an internal wire.
lbool CutSim::get(Wire w)
{
    assert(cutmap[w]);
    if (cutmap[w].size() > 0 && cutmap[w][0].size() == 0){
        assert(cutmap[w].size() == 1);
        assert(cutmap[w][0].ftb_lo == cutmap[w][0].ftb_hi);
        ushort ftb = cutmap[w][0].ftb_lo; assert(ftb == 0 || ftb == 0xFFFF);
        return (ftb == 0) ? l_False : l_True;
    }else
        return l_Undef;
}


// Return the state of a flop-input.
lbool CutSim::getFF(Wire w)
{
    assert(ff_map[w]);
    if (ff_map[w].size() > 0 && ff_map[w][0].size() == 0){
        assert(ff_map[w].size() == 1);
        assert(ff_map[w][0].ftb_lo == ff_map[w][0].ftb_hi);
        ushort ftb = ff_map[w][0].ftb_lo; assert(ftb == 0 || ftb == 0xFFFF);
        return (ftb == 0) ? l_False : l_True;
    }else
        return l_Undef;
}


// Get state output vector.
void CutSim::getState(Vec<lbool>& tstate)
{
    tstate.clear();
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        tstate(num, l_Error) = getFF(w);
    }
}


// Set flop cuts from state vector.
void CutSim::setState(const Vec<lbool>& tstate)
{
    Vec<Cut> sing(1);
    For_Gatetype(N, gate_Flop, w){
        int   num = attr_Flop(w).number;
        lbool val = tstate[num];

        if      (val == l_True ){ sing[0] = Cut(l_True) ; ff_map(w) = allocCuts(sing); }
        else if (val == l_False){ sing[0] = Cut(l_False); ff_map(w) = allocCuts(sing); }
        else if (val == l_Undef){ ff_map(w) = Cuts(empty_); }
        else    assert(false);
    }
}


void CutSim::weakenState()
{
    Vec<lbool> tstate;
    getState(tstate);
    setState(tstate);
}


template<template<class> class V>
Cuts CutSim::allocCuts(const V<Cut>& cuts, bool inv)
{
    uint n = cuts.size();
    Cuts cs(cutmem.alloc(n), n);
    for (uint i = 0; i < n; i++)
        cs[i] = inv ? ~cuts[i] : cuts[i];
    return cs;
}


Cuts CutSim::mergeCuts(Wire u, Wire v)
{
    Cuts us = cutmap[u];
    Cuts vs = cutmap[v];

    Vec<Cut> new_cuts;
    for (int i = -1; i < (int)us.size(); i++){
        if (i == -1 && us.size() == 1 && us[0].size() == 0) continue;       // -- skip unit cut for constant nodes
        Cut c = (i == -1) ? unitCut(u) : us[i] ^ sign(u);
        if (c == Cut_Undef) continue;

        for (int j = -1; j < (int)vs.size(); j++){
            if (j == -1 && vs.size() == 1 && vs[0].size() == 0) continue;   // -- skip unit cut for constant nodes
            Cut d = (j == -1) ? unitCut(v) : vs[j] ^ sign(v);
            if (d == Cut_Undef) continue;

            //**/WriteLn "mergeCuts: %_ * %_ = %_ * %_", u, v, c, d;
            combineCutPair(c, d, new_cuts, max_cutsize, cuts_per_node, heuristic_cutoff);
        }
    }

    return allocCuts(new_cuts);
}


Cuts CutSim::dupCuts(Wire w)
{
    assert(type(w) != gate_Const || id(w) == gid_True);
    return allocCuts(cutmap[w], sign(w));
}


void CutSim::simulateCycle()
{
    // Initialize flops:
    Vec<Cut>  cuts;
    Vec<uint> sizes;

    // Make temporary copy of flop cuts:
    For_Gatetype(N, gate_Flop, w){
        sizes.push(ff_map[w].size());
        for (uint i = 0; i < ff_map[w].size(); i++)
            cuts.push(ff_map[w][i]);
    }

    // Clear memory and reinsert flop cuts at state-input side:
    cutmem.clear();
    cutmap.clear();
    Vec<Cut> cs;
    uint i = 0;
    uint off = 0;
    For_Gatetype(N, gate_Flop, w){
        cs.clear();
        for (uint j = 0; j < sizes[i]; j++){
            cs.push(cuts[off++]);
            cs.last().translate(N.size());
        }
        cutmap(w) = allocCuts(cs);
        i++;
    }

    // Add constant True to map:
    Vec<Cut> sing(1, Cut(l_True));
    cutmap(N.True()) = allocCuts(sing);

    // Simulate one cycle:
    For_UpOrder(N, w){
        switch (type(w)){
        case gate_PI:
            cutmap(w) = Cuts(empty_);
            break;

        case gate_Flop:
            assert(cutmap[w]);
            break;

        case gate_And:
            //**/WriteLn "#### %_", w;
            cutmap(w) = mergeCuts(w[0], w[1]);
            //**/if (cutmap[w].size() > 0 && cutmap[w][0].size() == 0)
            //**/    Dump(w, cutmap[w]);
            break;

        case gate_PO:
            cutmap(w) = dupCuts(w[0]);
            break;

        default:
            ShoutLn "Unexpected gate type: %_", GateType_name[type(w)];
            assert(false);
        }
    }

    For_Gatetype(N, gate_Flop, w)
        ff_map(w) = dupCuts(w[0]);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Finding bounds for temporal decomposition:


// Checks if 'states[start]' and 'states[stop]' can be unified to prove a number of constants that
// exceeds 'best_consts' (measuered over state variables). The reduced state for which this was
// proven (and the number of constants in that state) is returned through reference. 'n_consts' is
// redundant and derived from 'reduced_state' for convenience. NOTE! 'reduced_state' is used
// internally as a temporary, so it is modified even if the function returns FALSE.
static
bool match(CutSim& sim, const Vec<Vec<lbool> >& states, uint start, uint stop, uint best_consts,
  /*outs:*/uint& n_consts, Vec<lbool>& reduced_state)
{
    const Vec<lbool>& s0 = states[start];
    const Vec<lbool>& s1 = states[stop];
    Vec<lbool>& r = reduced_state;

    // Setup union 'r' of 's0' and 's1':
    r.clear();
    assert(s0.size() == s1.size());
    for (uint i = 0; i < s0.size(); i++){
        if (s0[i] == s1[i]) r.push(s0[i]);
        else                r.push(l_Undef);
    }

    n_consts = 0;
    for (uint i = 0; i < r.size(); i++)
        if (r[i] != l_Undef)
            n_consts++;

    // Simulate and reduce 'r' until fixed point reached:
    Vec<lbool> s;
    uint iter = 0;
    while (n_consts > best_consts){
        //**/WriteLn "  -- interval: [%_,%_[   iteration: %_    #consts: %_", start, stop, iter, n_consts;
        sim.setState(r);
        for (uint d = start; d < stop; d++)
            sim.simulateCycle();
        sim.getState(s);

        bool refined = false;
        for (uint i = 0; i < r.size(); i++){
            if (r[i] != l_Undef && s[i] != r[i]){
                r[i] = l_Undef;
                n_consts--;
                refined = true;
            }
        }

        if (!refined){
            //   /**/WriteLn "  -- interval: [%_,%_[   iteration: %_    #consts: %_", start, stop, iter, n_consts;
            //**/WriteLn " \a/(SUCCESS, #consts: %_)\a/", n_consts;
            return true; }
        iter++;
    }
    //**/WriteLn " (failed, #consts: %_)", n_consts;
    return false;
}


static
void determineLengths(
    NetlistRef N,
    uint max_cutsize,
    uint cuts_per_node,
    uint heuristic_cutoff,
    uint min_cycle_len,
    uint max_cycle_len,
    uint min_total_len,
    uint max_total_len,
    /*outputs:*/
    uint& start,
    uint& stop,
    uint& n_consts,
    Vec<lbool>& reduced_state       // -- 3-valued state at the start of the cycle ("reduced" because it may contain constants)
    )
{
    // Setup simulator:
    CutSim sim(N), tmpsim(N);
    sim.max_cutsize      = tmpsim.max_cutsize      = max_cutsize;
    sim.cuts_per_node    = tmpsim.cuts_per_node    = cuts_per_node;
    sim.heuristic_cutoff = tmpsim.heuristic_cutoff = heuristic_cutoff;

    Vec<Vec<lbool> > states;
    uint       best_start  = 0;
    uint       best_stop   = 1;
    uint       best_consts = 0;
    Vec<lbool> best_state;

    sim.getState(states(0));
    for (uint d = 1; d <= max_total_len; d++){
        Write "\r  -- simulating cycle %_\f", d-1;

        sim.simulateCycle();
        sim.getState(states(d));

        for (uint i = 0; i < d-1; i++){
            if (d < min_total_len) continue;
            if (d-i < min_cycle_len || d-i > max_cycle_len) continue;

            Vec<lbool> r;
            uint       n;
            if (match(tmpsim, states, i, d, best_consts, n, r)){
                best_start  = i;
                best_stop   = d;
                best_consts = n;
                r.copyTo(best_state);
            }
        }
    }
    Write "\r\f";

    // Return result:
    start = best_start;
    stop  = best_stop;
    n_consts = best_consts;
    best_state.copyTo(reduced_state);
}


// Returns the prefix and cycle lengths selected.
Pair<uint,uint> tempDecomp(NetlistRef N, NetlistRef M, uint max_cycle_len, uint max_total_len)
{
    assert(checkNumbering(N, true));

    uint start, stop, n_consts;
    Vec<lbool> reduced_state;

    WriteLn "Determining a good cycle length.";
    determineLengths(N, 1, 1, 1, 1, max_cycle_len, 1, max_total_len, /*results*/start, stop, n_consts, reduced_state);
    uint cyc = stop - start;
    WriteLn "  ==> length=%_   (#consts %_)", cyc, n_consts;

    WriteLn "Determining a good prefix length.";
    determineLengths(N, 2, 2, 4, cyc, cyc, 1, max_total_len, /*results*/start, stop, n_consts, reduced_state);
    uint pfx = start;
    WriteLn "  ==> prefix=%_   (#consts %_)", pfx, n_consts;

    WriteLn "Resimulating with higher accuracy.";
    determineLengths(N, 3, 8, 6, cyc, cyc, pfx + cyc, pfx + cyc, /*results*/start, stop, n_consts, reduced_state);
    assert(start == pfx);
    assert(stop - start == cyc);
    WriteLn "  ==> #consts: %_", n_consts;

    applyTempDecomp(N, M, pfx, cyc, reduced_state);

    return tuple(pfx, cyc);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Perform decomposition:


/*
Notes on the transformation:

  - Pre-condition: PIs, POs and FFs are uniquely numbered.

  - POs not corresponding to properties are removed! Remaining POs are preserved but now
    represents the property failing at one of many original clock cycles.

  - Let:
        '#pi' be the highest numbered PI plus one
        '#ff' be the highest numbered FF plus one
        'pfx' the length of the prefix
        'cyc' the length of the cycle

  - The new circuit will have one extra FF, 'reset', unless already present.

  - The new circuit will have '(pfx + cyc) * #pi' inputs: '#pi * frame + i' gives the PI
    number of input 'i' at the given frame, where the first 'pfx' frames contains the
    prefix inputs, the rest of the frames the cycle inputs.

  - Initialized flops will be replaced by constants in the prefix, so their value cannot
    be extracted from the counterexample of the new circuit, but have to be obtained from
    the 'flop_init' of the original circuit. However, uninitialized flops will still feed
    the prefix, so their value can be extracted.
*/

#if 0
/*DEBUG*/static Wire sAnd(Wire u, Wire v) { return netlist(u).add(And_(), u, v); }
/*DEBUG*/static Wire sOr (Wire u, Wire v) { return ~netlist(u).add(And_(), ~u, ~v); }
#define s_And sAnd
#define s_Or sOr
#endif


void applyTempDecomp(NetlistRef N, NetlistRef M, uint pfx, uint cyc, const Vec<lbool>& cyc_start_state)
{
    uint n_pis = nextNum_PI(N);
    uint n_ffs = nextNum_Flop(N);

    Get_Pob (N, flop_init);

    // Setup target 'M':
    assert(M.empty());
    Add_Pob0(M, strash);
    Add_Pob2(M, flop_init, flop_init_M);
    Add_Pob2(M, properties, properties_M);

    // Setup initial state in prefix part:
    WMap<GLit> n2m;
    n2m(N.True()) = M.True();
    Vec<Wire> ff;

    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        ff(num) = M.add(Flop_(num));

        if      (flop_init[w] == l_True ) n2m(w) =  M.True();
        else if (flop_init[w] == l_False) n2m(w) = ~M.True();
        else{
            assert(flop_init[w] == l_Undef);
            n2m(w) = ff[num];
        }
    }

    // Add reset flop:
    Add_Pob (M, reset);
    reset = M.add(Flop_(n_ffs), ~M.True());
    flop_init_M(reset) = l_True;

    // Build prefix:
    Get_Pob(N, properties);
    Vec<Wire> props(properties.size(), M.True());

    Auto_Pob(N, up_order);
    Vec<GLit> ff_tmp;
    for (uint d = 0; d < pfx; d++){
        // Build logic:
        For_UpOrder(N, w){
            if (type(w) == gate_PI){
                int num = attr_PI(w).number;
                n2m(w) = M.add(PI_(d * n_pis + num));

            }else if (type(w) == gate_And){
                Wire u = M[n2m[w[0]] ^ sign(w[0])];
                Wire v = M[n2m[w[1]] ^ sign(w[1])];
                n2m(w) = s_And(u, v);

            }else{
                assert(type(w) == gate_PO || type(w) == gate_Flop);
            }
        }

        // Map flops:
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            ff_tmp(num) = n2m[w[0]] ^ sign(w[0]);
        }
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            n2m(w) = ff_tmp[num];
        }

        // Accumulate properties:
        for (uint i = 0; i < properties.size(); i++){
            Wire w = properties[i]; assert(type(w) == gate_PO);
            props[i] = s_And(props[i], M[n2m[w[0]]] ^ sign(w[0]));
        }
    }

    // Add guard to prefix properties:
    for (uint i = 0; i < props.size(); i++)
        props[i] = s_And(props[i], reset);

    // Add prefix reset logic to flops:
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        ff_tmp(num) = s_Or(s_And(reset, M[n2m[w]]), s_And(~reset, ff[num]));
    }
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        n2m(w) = ff_tmp[num];
    }

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    // <<== INITIALIZE WITH CYC_STATE HERE!!

/*EXPERIMENTAL*/
    if (cyc_start_state.size() > 0){
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            if      (cyc_start_state[num] == l_True)  n2m(w) =  M.True();
            else if (cyc_start_state[num] == l_False) n2m(w) = ~M.True();
        }
    }
/*END*/

    // Build cycle:
    Vec<Wire> props2(properties.size(), M.True());
    for (uint d = 0; d < cyc; d++){
        // Build logic:
        For_UpOrder(N, w){
            if (type(w) == gate_PI){
                int num = attr_PI(w).number;
                n2m(w) = M.add(PI_((pfx + d) * n_pis + num));

            }else if (type(w) == gate_And){
                Wire u = M[n2m[w[0]] ^ sign(w[0])];
                Wire v = M[n2m[w[1]] ^ sign(w[1])];
                n2m(w) = s_And(u, v);

            }else{
                assert(type(w) == gate_PO || type(w) == gate_Flop);
            }
        }

        // Map flops:
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            ff_tmp(num) = n2m[w[0]] ^ sign(w[0]);
        }
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number;
            n2m(w) = ff_tmp[num];
        }

        // Accumulate properties:
        for (uint i = 0; i < properties.size(); i++){
            Wire w = properties[i]; assert(type(w) == gate_PO);
            props2[i] = s_And(props2[i], M[n2m[w[0]]] ^ sign(w[0]));
        }
    }

    // Close loops for flops:
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        ff[num].set(0, M[n2m[w[0]]] ^ sign(w[0]));
    }

    // Add guard to suffix properties:
    for (uint i = 0; i < props2.size(); i++)
        props2[i] = s_And(props2[i], ~reset);
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    // Add properties to 'M':
    for (uint i = 0; i < props.size(); i++)
        properties_M.push(M.add(PO_(i), s_Or(props[i], props2[i])));

    removeUnreach(M);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


uint64 hash(const Vec<lbool>& tstate)
{
    if (tstate.size() == 0) return UINT64_MAX;

    uint64 h = tstate[0].value;
    for (uint i = 1; i < tstate.size(); i++)
        h = h * 482339018847839ull + tstate[i].value;
    return h;
}


void test(NetlistRef N, uint max_cutsize, uint cuts_per_node, uint heuristic_cutoff, bool weaken_state)
{
#if 1
    CutSim sim(N);
    sim.max_cutsize = max_cutsize;
    sim.cuts_per_node = cuts_per_node;
    sim.heuristic_cutoff = heuristic_cutoff;
    Vec<Vec<lbool> > states;
    Vec<uint64>      hashes;
    bool             verified = false;
    for(uint d = 0;; d++){
        sim.getState(states(d));
        hashes.push(hash(states[d]));

        /**/Write "%>3%_: ", d;
        /**/for (uint j = 0; j < states[d].size(); j++)
        /**/    std_out += states[d][j];
        /**/NewLine;

        if (d != 0){
            for (uint i = 0; i < d; i++){
                if (hashes[i] == hashes[d] && vecEqual(states[i], states[d])){
                    WriteLn "Loop between %_ and %_%_", i, d, !verified ? "" : " (again?)";
                    if (verified)
                        goto Done;
                    else{
                        NewLine;
                        WriteLn "Rerunning, starting from proper Xs...";
                        states.shrinkTo(i+1);
                        hashes.shrinkTo(i+1);
                        sim.setState(states[i]);
                        d = i;
                        verified = true;
                    }
                }
            }
        }

        sim.simulateCycle();
        if (weaken_state)
            sim.weakenState();
    }
    Done:;
#endif

#if 0
    Cut c(l_Undef);
    c.push(0);
    c.push(1);
    c.push(2);
    c.push(3);
    c.ftb_lo = 0x8000;
    c.ftb_hi = 0x8000;

    Cut d(l_Undef);
    d.push(2);
    d.push(3);
    d.push(4);
    d.push(5);
    d.ftb_lo = 0x8000;
    d.ftb_hi = 0x8000;

  #if 1
    uint64 seed = 1234;
    c.ftb_lo = irand(seed, 65536);
    c.ftb_hi = irand(seed, 65536) | c.ftb_lo;
    d.ftb_lo = irand(seed, 65536);
    d.ftb_hi = irand(seed, 65536) | d.ftb_lo;
  #endif

    Dump(c);
    Dump(d);

    Vec<Cut> cuts;
    combineCutPair(c, d, false, false, cuts);
#endif
}


/*
Problem: ~/ZZ/Bip/s_00005_0007_abs.aig
Abstraktion har gjort flopnumrering sparse = problem
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
