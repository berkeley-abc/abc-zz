//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelayOpt.cc
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Buffering and resizing using standard cell library.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "DelayOpt.hh"
#include "TimingRef.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Heap.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "OrgCells.hh"
#include <csignal>

//#define CONSISTENCY_CHECKING

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Signal handler -- temporary:


static volatile bool ctrl_c_pressed = false;

extern "C" void SIGINT_handler(int signum);
void SIGINT_handler(int signum)
{
    if (ctrl_c_pressed) _exit(0);
    ctrl_c_pressed = true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Simple buffering:


void preBuffer(NetlistRef N, const SC_Lib& L, uint branchf, uint buf_sym, bool quiet)
{
    if (branchf == 0) return;

    Auto_Pob(N, dyn_fanouts);

    // Get reverse level (est. of departure time):
    Vec<GLit> order;
    topoOrder(N, order);

    WMap<uint> rlevel;
    for (uint i = order.size(); i > 0;){ i--;
        Wire w = order[i] + N;
        Fanouts fs = dyn_fanouts[w];

        for (uint j = 0; j < fs.size(); j++)
            newMax(rlevel(w), rlevel[fs[j]] + 1);
    }

    // Insert buffers:
    Vec<Connect> cs;
    uint n_bufs = 0;

    For_Gates(N, w){
        if (type(w) == gate_Clamp) continue;
        if (type(w) == gate_Uif && L.cells[attr_Uif(w).sym].n_outputs > 1) continue;

        Fanouts fs = dyn_fanouts[w];
        if (fs.size() > branchf){
            for (uint i = 0; i < fs.size(); i++)
                cs.push(fs[i]);

            sobSort(sob(cs, proj_lt(brack<uint,Connect>(rlevel))));

            for (uint i = 0; i < cs.size()-branchf; i += branchf){
                Wire wb = N.add(Uif_(buf_sym), w);
                /**/WriteLn "Added: %n (%_)", wb, wb;
                n_bufs++;
                for (uint j = i; j < cs.size() && j < i + branchf; j++)
                    cs[j].set(wb);
                cs.push(Connect(wb, 0));
            }

            cs.clear();
        }
    }

    if (!quiet)
        WriteLn "\a/Pre-buffering:\a/ inserted \a*%,d\a* buffers", n_bufs;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Inititial delay computation:


void computeLoads(NetlistRef N, const Vec<D_Cell>& dcells, const Vec<float>& wire_cap, /*out*/TMap& load)
{
    For_Gatetype(N, gate_Uif, w){
        uint cell_idx = attr_Uif(w).sym;
        const D_Cell& cell = dcells[cell_idx];
        For_Inputs(w, v){
            uint i = Iter_Var(v);
            load(v) += cell[i].cap;
        }
    }

    For_Gatetype(N, gate_Clamp, w)
        load(w[0]) = load[w];

    if (wire_cap.size() > 0){
        WMap<uint> fanout_count(0);
        For_Gates(N, w){
            if (type(w) != gate_Uif || !dcells[attr_Uif(w).sym].pseudo()){  // <<== huh?
                For_Inputs(w, v)
                    fanout_count(v)++;
            }
        }

        For_Gates(N, w){
            uint n = fanout_count[w];
            newMin(n, wire_cap.size() - 1);
            load(w).rise += wire_cap[n];
            load(w).fall += wire_cap[n];
        }
    }
}


macro void timeGate(const SC_Timing& t, Wire w, Wire v, const TMap& load, uint approx, TMap& arr, TMap& slew) {
    timeGate(t, arr[v], slew[v], load[w], approx, arr(w), slew(w)); }


// Output parameters 'arr' and 'slew' should be default constructed and unmodifed.
void staticTiming(NetlistRef N, const Vec<D_Cell>& dcells, const TMap& load, const Vec<GLit>& order, uint approx, /*outputs:*/TMap& arr, TMap& slew)
{
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;

        if (type(w) == gate_Uif){
            uint cell_idx = attr_Uif(w).sym;
            const D_Cell& dcell = dcells[cell_idx];

            For_Inputs(w, v){
                const SC_Timing* t = dcell[Iter_Var(v)].timing;
                if (t)
                    timeGate(*t, w, v, load, approx, arr, slew);
            }

        }else if (type(w) == gate_Clamp){
            arr (w) = arr [w[0]];
            slew(w) = slew[w[0]];
        }
    }
}


float getTotalArea(NetlistRef N, const Vec<D_Cell>& dcells)
{
    double area = 0;
    For_Gatetype(N, gate_Uif, w){
        uint cell_idx = attr_Uif(w).sym;
        area += dcells[cell_idx].area;
    }

    return area;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
Features needed:
  - Incremental timing
  - Simple dynamic fanout handling (only shrinking set, but buffers introduce new nodes)
  - Levelization (space 2^k steps apart to allow for buffer insertion)

Algorithms needed:
  - Gate sizing
  - Buffer insertion
  - Buffer removal and area recovering
  - Legalization?
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Delay optimization:


struct LvQueue_lt {
    NetlistRef        N;
    const WMap<uint>& level;

    LvQueue_lt(NetlistRef N_, const WMap<uint>& level_) : N(N_), level(level_) {}
    bool operator()(GLit x, GLit y) const { return level[x + N] < level[y + N]; }
};


class DelayOpt {
    typedef KeyHeap<GLit, false, LvQueue_lt> LvQueue;

    // Problem input:
    NetlistRef          N;
    const SC_Lib&       L;
    const Vec<float>&   wire_cap;
    uint                approx;

    // Alt. cell representation:
    Vec<Vec<uint> > groups;
    uint            buf_group;
    WMap<gate_id>   next_output;
    Vec<D_Cell>     dcells;

    // Timing data:
    TMap            load;
    TMap            arr;
    TMap            slew;
    float           area;

    IdHeap<float,1> crit;
    Vec<float>      po_arr;
    Vec<GLit>       po_list;

    // Incremental update:
    WMap<uint>      level;
    LvQueue         Q;
    WSeen           in_Q;

    // Other:
    uint64          seed;
    Wire            w_buf;

  //________________________________________
  //  Internal helpers:

    float maxArrival();

    uint  critArr (Wire w0);  // -- returns the pin# of timing critical input (or UINT_MAX if no inputs)
    uint  critSlew(Wire w0);  // -- returns the pin# of slew critical input (or UINT_MAX if no inputs)

    void  getCritPath(Wire w0, Vec<GLit>& out_crit_path);    // -- 'w0' must be a PO

    void  enqueue(Wire w);
    Wire  dequeue();

    void  findBufferGroup();
    bool  isBuf(Wire w);

  //________________________________________
  //  Major internal methods:

    float incUpdate(bool eval = false);
    bool  tryBuffering(Wire w);
    bool  tryBuffering2(Wire w);
    bool  tryResize(Wire w, bool shrink_only);
    bool  tryResize2(Wire w1, Wire w2);
    bool  tryBypass(Wire wb);
    void  optimize();

  //________________________________________
  //  Debug:

    void  verifyLoad();
    void  verifyTiming();

public:
  //________________________________________
  //  Public interface:

    DelayOpt(NetlistRef N_, const SC_Lib& L_, const Vec<float>& wire_cap_) :
        N(N_), L(L_), wire_cap(wire_cap_), buf_group(0), area(0), Q(LvQueue_lt(N, level)), seed(DEFAULT_SEED), w_buf(Wire_NULL) {}

    void run(uint pre_buffer, bool forget_sizes);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


inline float DelayOpt::maxArrival()
{
  #if !defined(CONSISTENCY_CHECKING)
    Wire w = po_list[crit.peek()] + N;
    return max_(arr[w[0]].rise, arr[w[0]].fall);

  #else
    // <<== also check that 'load' has the correct capacity values... (+- epsilon?)

    Wire w = po_list[crit.peek()] + N;
    float ret = max_(arr[w[0]].rise, arr[w[0]].fall);

    float max_arr = 0;
    Wire w_fail;
    For_Gatetype(N, gate_Uif, w){
        if (newMax(max_arr, arr[w].rise)) w_fail = w;
        if (newMax(max_arr, arr[w].fall)) w_fail = w;
    }
    assert(ret == max_arr);

    return ret;
  #endif
}


inline uint DelayOpt::critArr(Wire w0)
{
    uint ret = UINT_MAX;
    float worst_arr = -1;
    For_Inputs(w0, v)
        if (newMax(worst_arr, arr[v].rise) || newMax(worst_arr, arr[v].fall))
            ret = Iter_Var(v);
    return ret;
}


inline uint DelayOpt::critSlew(Wire w0)
{
    uint ret = UINT_MAX;
    float worst_slew = -1;
    For_Inputs(w0, v)
        if (newMax(worst_slew, slew[v].rise) || newMax(worst_slew, slew[v].fall))
            ret = Iter_Var(v);
    return ret;
}


// Critical path will be given in order from outputs to inputs (with last element being fed by a PI).
// Remark 1: 'w' must be a PO and will not itself be included in 'path'.
// Remark 2: 'path' is cleared by this method.
void DelayOpt::getCritPath(Wire w, Vec<GLit>& path)
{
    assert(type(w) == gate_PO);
    path.clear();
    w = w[0];
    while (type(w) != gate_PI){
        path.push(w);
        w = w[critArr(w)];
    }
}


inline void DelayOpt::enqueue(Wire w)
{
    if (!in_Q.add(w))
        Q.add(w);
}


inline Wire DelayOpt::dequeue()
{
    Wire w = Q.pop() + N;
    in_Q.exclude(w);
    return w;
}


void DelayOpt::findBufferGroup()
{
    for (uint i = 0; i < groups.size(); i++){
        uint idx = dcells[groups[i][0]].idx_L;
        if (idx == UINT_MAX) continue;

        const SC_Cell& cell = L.cells[idx];
        if (cell.n_inputs == 1 && cell.n_outputs == 1 && (cell.pins[1].func[0] & 3) == 2){
            buf_group = i;
            return; }
    }
    ShoutLn "INTERNAL ERROR! No buffers found in library.";
    exit(1);
}


inline bool DelayOpt::isBuf(Wire w)
{
    if (type(w) != gate_Uif) return false;
    uint sym = attr_Uif(w).sym;
    return dcells[sym].group == buf_group;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Incremental timing update:


float DelayOpt::incUpdate(bool eval)
{
    Get_Pob(N, dyn_fanouts);

    float t0 ___unused = maxArrival();
    float arr_delta = 0;
    uint iter = 0;
    while (Q.size() > 0){
        Wire w = dequeue();
        //**/WriteLn "\a/incUpdate(%n)\a/   --  arr=%_  slew=%_", w, arr[w], slew[w];

        if (type(w) == gate_PO){
            uint num = attr_PO(w).number;
            //**/WriteLn "PO# %_ = %n:  %_ -> %_", num, w, po_arr[num], arr[w[0]];
            po_arr[num] = max_(arr[w[0]].rise, arr[w[0]].fall);
            crit.update(num);

        }else if (type(w) == gate_Clamp){
            arr (w) = arr [w[0]];
            slew(w) = slew(w[0]);
            Fanouts fs = dyn_fanouts[w];
            for (uint i = 0; i < fs.size(); i++)
                enqueue(fs[i]);

        }else{
            assert(type(w) == gate_Uif);

            TValues orig_arr  = arr [w];
            TValues orig_slew = slew[w];

            const D_Cell& dcell = dcells[attr_Uif(w).sym];
            arr(w) = slew(w) = TValues(0, 0);
            For_Inputs(w, v){
                timeGate(*dcell[Iter_Var(v)].timing, w, v, load, approx, arr, slew); }

            if (arr[w] != orig_arr || slew[w] != orig_slew){
                arr_delta += (orig_arr.rise - arr[w].rise) + (orig_arr.fall - arr[w].fall);     // <<== should do this for critical (or almost critical) nodes only
                //**/WriteLn "  -- new arr=%_   new slew=%_", arr[w], slew[w];
                Fanouts fs = dyn_fanouts[w];
                for (uint i = 0; i < fs.size(); i++)
                    enqueue(fs[i]);
            }
        }

        iter++;
#if 0
        if (eval && iter == 1000){
            if (!(maxArrival() < t0 || (maxArrival() == t0 && arr_delta < 0))){
                return arr_delta; }
        }
#endif
    }

    return arr_delta;
}


void DelayOpt::verifyLoad()
{
    float epsilon = 1e-6;
    TMap load_ref;
    computeLoads(N, dcells, wire_cap, load_ref);
    For_Gates(N, w){
        if (type(w) == gate_PI) continue;
        if (fabs(load[w].rise - load_ref[w].rise) > epsilon || fabs(load[w].fall - load_ref[w].fall) > epsilon){
            WriteLn "Mismatch in load for %n:  load=%_  load_ref=%_", w, load[w], load_ref[w];
            assert(false);
        }
    }
}


void DelayOpt::verifyTiming()
{
    verifyLoad();

    Vec<GLit> order;
    topoOrder(N, order);

    TMap arr_ref, slew_ref;
    staticTiming(N, dcells, load, order, approx, arr_ref, slew_ref);

    For_Gates(N, w){
        if (arr_ref [w] != arr [w]) WriteLn "Difference in arrival time.";
        if (slew_ref[w] != slew[w]) WriteLn "Difference in slew.";

        if (arr_ref[w] != arr[w] || slew_ref[w] != slew[w]){
            WriteLn "%n", w;
            WriteLn "arr_ref=%_   slew_ref=%_", arr_ref[w], slew_ref[w];
            WriteLn "arr=%_   slew=%_", arr[w], slew[w];
            assert(false);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Optimization loop:


// <<== Duger inte! Måste slå ihop åtminstone två..

bool DelayOpt::tryBuffering(Wire w)
{
    // Pick a fanout to insert buffer in front of:
    Get_Pob(N, dyn_fanouts);
    Fanouts fs = dyn_fanouts[w];
    if (fs.size() < 1)
        return false;

    Connect w0 = fs[irand(seed, fs.size())];
    if (type(w0) == gate_PO || type(w0) == gate_Clamp)
        return false;   // -- no need to buffer POs

    if (level[w0] - level[w] < 2)
        return false;   // -- no room for a new level between 'w0' and its child 'w'

    float t0 = maxArrival();

    // Insert a random buffer:
    uint alt = groups[buf_group][irand(seed, groups[buf_group].size())];
    //**/WriteLn "Trying buffer %_ between %n/%_ <- %n", L.cells[alt].name, w0, w0.pin, w;
    //**/WriteLn "  %_:  arr=%_  slew=%_  load=%_", w0, arr[w0], slew[w0], load[w0];

    float orig_area = area;
    area += dcells[alt].area;

    if (w_buf == Wire_NULL)
        w_buf = N.add(Uif_(alt), w);
    else{
        attr_Uif(w_buf).sym = alt;
        w_buf.set(0, w);
    }
    w0.set(w_buf);

    level(w_buf) = (level[w] + level[w0]) / 2;

    TValues orig_w_load = load[w];
    TValues cap0 = dcells[attr_Uif(w0).sym].inputs[w0.pin].cap;
    load(w_buf) = cap0;
    load(w) -= cap0;
    load(w) += dcells[alt].inputs[0].cap;
    //**/WriteLn "  new w load: %_", load[w];
    //**/WriteLn "  levels: %x %x %x", level[w], level[w_buf], level[w0];

    enqueue(w);
    enqueue(w_buf);
    enqueue(w0);

    // Improvement?
    float delta = incUpdate(true);
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    if (maxArrival() < t0 || (maxArrival() == t0 && delta < 0)){
        w_buf = Wire_NULL;
        return true; }
    //**/WriteLn "  fail: %_ -> %_", t0, maxArrival();

    // Undo:
    w0.set(w);
    load(w) = orig_w_load;
    area = orig_area;
    enqueue(w);
    enqueue(w0);

    // Recycle buffer:
    arr(w_buf) = slew(w_buf) = TValues();
    load(w_buf) = TValues();
    w_buf.disconnect(0);

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
    maxArrival();
  #endif

    return false;
}


bool DelayOpt::tryBuffering2(Wire w)
{
    // Pick a fanout to insert buffer in front of:
    Get_Pob(N, dyn_fanouts);
    Fanouts fs = dyn_fanouts[w];
    if (fs.size() < 2)
        return false;

    uint i0 = irand(seed, fs.size());
    uint i1 = irand(seed, fs.size() - 1);
    if (i1 == i0) i1 = fs.size() - 1;

    Connect w0 = fs[i0];
    Connect w1 = fs[i1];
    if (type(w0) == gate_PO || type(w0) == gate_Clamp || type(w1) == gate_PO || type(w1) == gate_Clamp)
        return false;   // -- no need to buffer POs

    if (level[w0] - level[w] < 2 || level[w1] - level[w] < 2)
        return false;   // -- no room for a new level between 'w0' and its child 'w'

    float t0 = maxArrival();

    // Insert a random buffer:
    uint alt = groups[buf_group][irand(seed, groups[buf_group].size())];

    float orig_area = area;
    area += dcells[alt].area;

    if (w_buf == Wire_NULL)
        w_buf = N.add(Uif_(alt), w);
    else{
        attr_Uif(w_buf).sym = alt;
        w_buf.set(0, w);
    }
    w0.set(w_buf);
    w1.set(w_buf);

    level(w_buf) = (level[w] + min_(level[w0], level[w1])) / 2;

    TValues orig_w_load = load[w];
    TValues cap0 = dcells[attr_Uif(w0).sym].inputs[w0.pin].cap;
    TValues cap1 = dcells[attr_Uif(w1).sym].inputs[w1.pin].cap;
    load(w_buf) = cap0 + cap1;
    load(w) -= cap0 + cap1;
    load(w) += dcells[alt].inputs[0].cap;

    enqueue(w);
    enqueue(w_buf);
    enqueue(w0);
    enqueue(w1);

    // Improvement?
    float delta = incUpdate(true);
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    if (maxArrival() < t0 || (maxArrival() == t0 && delta < 0)){
        w_buf = Wire_NULL;
        return true; }
    //**/WriteLn "  fail: %_ -> %_", t0, maxArrival();

    // Undo:
    w0.set(w);
    w1.set(w);
    load(w) = orig_w_load;
    area = orig_area;
    enqueue(w);
    enqueue(w0);
    enqueue(w1);

    // Recycle buffer:
    arr(w_buf) = slew(w_buf) = TValues();
    load(w_buf) = TValues();
    w_buf.disconnect(0);

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
    maxArrival();
  #endif

    return false;
}


bool DelayOpt::tryResize(Wire w, bool shrink_only)
{
    if (type(w) != gate_Uif)
        return false;

    uint sym = attr_Uif(w).sym;
    const D_Cell& dcell = dcells[sym];
    const Vec<uint>& alts = groups[dcell.group];
    uint  curr_alt = dcell.group_pos; assert(sym == alts[curr_alt]);

    //**/WriteLn "Picked %n of type %_. Current alt %_/%_.", w, L.cells[dcell.idx_L].name, curr_alt, alts.size();
    //**/if (next_output[w] != 0) WriteLn "This is a multi-output node.";
    if (next_output[w] != 0)
        return false;       // <<== deal with those later

    if (alts.size() == 1)
        return false;       // -- only choice

    float t0 = maxArrival();

    uint pick;
    if (shrink_only){
        if (curr_alt == 0)
            return false;
        pick = irand(seed, curr_alt);
    }else
        pick = irand(seed, alts.size()-1);
    if (pick == curr_alt) pick = alts.size()-1;

    // Update gate type:
    if (next_output[w] == 0)
        attr_Uif(w).sym = alts[pick];
    else{
        /**/assert(false);  // <<== not finished
        Wire w_start = w;
        do{
            attr_Uif(w).sym = groups[attr_Uif(w).sym][pick];
            w = next_output[w] + N;
        }while (w != w_start);
    }

    // Update load on inputs:
    const D_Cell& new_dcell = dcells[attr_Uif(w).sym];

    TValues orig_load[16]; assert(w.size() <= 16);
    for (uint i = 0; i < w.size(); i++){
        Wire v = w[i];
        if (v == Wire_NULL) continue;
        if (type(v) == gate_PI) continue;   // -- don't touch PIs

        orig_load[i] = load[v];
        load(v) -= dcell[i].cap;
        load(v) += new_dcell[i].cap;
        enqueue(v);
    }
    enqueue(w);

    float orig_area = area;
    area -= dcell.area;
    area += new_dcell.area;

    // Do incremental update:
    float delta = incUpdate(true);
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    if (maxArrival() < t0 || (maxArrival() == t0 && delta < 0))
        return true;

    // Undo:
    //**/WriteLn "===UNDO===";
    attr_Uif(w).sym = alts[curr_alt];       // <<== + for multi-output gates

    for (uint i = w.size(); i > 0;){ i--;
        Wire v = w[i];
        if (v == Wire_NULL) continue;
        if (type(v) == gate_PI) continue;   // -- don't touch PIs
        load(v) = orig_load[i];
        enqueue(v);
    }
    enqueue(w);

    area = orig_area;

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    /**/if (!(maxArrival() == t0)) Dump(t0, maxArrival());
    assert(maxArrival() == t0);

    return false;
}


bool DelayOpt::tryResize2(Wire w1, Wire w2)
{
    if (type(w1) != gate_Uif || type(w2) != gate_Uif)
        return false;

    uint sym1 = attr_Uif(w1).sym;
    uint sym2 = attr_Uif(w2).sym;
    const D_Cell& dcell1 = dcells[sym1];
    const D_Cell& dcell2 = dcells[sym2];
    const Vec<uint>& alts1 = groups[dcell1.group];
    const Vec<uint>& alts2 = groups[dcell2.group];
    uint  curr_alt1 = dcell1.group_pos; assert(sym1 == alts1[curr_alt1]);
    uint  curr_alt2 = dcell2.group_pos; assert(sym2 == alts2[curr_alt2]);

    //**/WriteLn "Picked %n of type %_. Current alt %_/%_.", w, L.cells[dcell.idx_L].name, curr_alt, alts.size();
    //**/if (next_output[w] != 0) WriteLn "This is a multi-output node.";
    if (next_output[w1] != 0 || next_output[w2] != 0)
        return false;       // <<== deal with those later

    if (alts1.size() == 1 || alts2.size() == 1)
        return false;       // -- only choice

    float t0 = maxArrival();

    uint pick1 = irand(seed, alts1.size()-1); if (pick1 == curr_alt1) pick1 = alts1.size()-1;
    uint pick2 = irand(seed, alts2.size()-1); if (pick2 == curr_alt2) pick2 = alts2.size()-1;

    // Update gate type:
    assert(next_output[w1] == 0);
    assert(next_output[w2] == 0);
    attr_Uif(w1).sym = alts1[pick1];
    attr_Uif(w2).sym = alts2[pick2];

    // Update load on inputs:
    const D_Cell& new_dcell1 = dcells[attr_Uif(w1).sym];
    const D_Cell& new_dcell2 = dcells[attr_Uif(w2).sym];

    TValues orig_load[2][16]; assert(w1.size() <= 16); assert(w2.size() <= 16);
    for (uint n = 0; n < 2; n++){
        Wire w = (n == 0) ? w1 : w2;
        const D_Cell& dcell = (n == 0) ? dcell1 : dcell2;
        const D_Cell& new_dcell = (n == 0) ? new_dcell1 : new_dcell2;

        for (uint i = 0; i < w.size(); i++){
            Wire v = w[i];
            if (v == Wire_NULL) continue;
            if (type(v) == gate_PI) continue;   // -- don't touch PIs

            orig_load[n][i] = load[v];
            load(v) -= dcell[i].cap;
            load(v) += new_dcell[i].cap;
            enqueue(v);
        }
        enqueue(w);
    }

    float orig_area = area;
    area -= dcell1.area;
    area -= dcell2.area;
    area += new_dcell1.area;
    area += new_dcell2.area;

    // Do incremental update:
    float delta = incUpdate(true);
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    if (maxArrival() < t0 || (maxArrival() == t0 && delta < 0))
        return true;

    // Undo:
    attr_Uif(w1).sym = alts1[curr_alt1];
    attr_Uif(w2).sym = alts2[curr_alt2];

    for (uint n = 2; n > 0;){ n--;
        Wire w = (n == 0) ? w1 : w2;

        for (uint i = w.size(); i > 0;){ i--;
            Wire v = w[i];
            if (v == Wire_NULL) continue;
            if (type(v) == gate_PI) continue;   // -- don't touch PIs
            load(v) = orig_load[n][i];
            enqueue(v);
        }
        enqueue(w);
    }
    area = orig_area;

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    /**/if (!(maxArrival() == t0)) Dump(t0, maxArrival());
    assert(maxArrival() == t0);

    return false;
}


// Try bypassing a buffer for one of its fanouts.
bool DelayOpt::tryBypass(Wire wb)
{
//**/return false;
    Get_Pob(N, dyn_fanouts);

    if (type(wb[0]) != gate_Uif)
        return false;

    Fanouts fs = dyn_fanouts[wb];
    if (fs.size() == 0)
        return false;

    float t0 = maxArrival();
    float orig_area = area;

    Connect w0 = fs[irand(seed, fs.size())];

    if (type(w0) != gate_Uif)
        return false;

    // Bypass buffer in chain 'w0 <- wb <- wb[0]':
    w0.set(wb[0]);

    const D_Cell& cell_wb = dcells[attr_Uif(wb).sym]; assert(cell_wb.group == buf_group);
    const D_Cell& cell_w0 = dcells[attr_Uif(w0).sym];

    TValues orig_load_wb0 = load[wb[0]];
    TValues orig_load_wb  = load[wb];

    if (fs.size() == 1){
        area -= cell_wb.area;
        load(wb[0]) -= cell_wb[0].cap;
    }
    load(wb[0]) += cell_w0[w0.pin].cap;
    load(wb)    -= cell_w0[w0.pin].cap;     // <<== not accurate with wire delay model...
    enqueue(wb[0]);
    enqueue(wb);
    enqueue(w0);

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    if (fs.size() > 1)  // -- otherwise not accurate (gate removal below)
        verifyTiming();
  #endif
    if (maxArrival() < t0 || (maxArrival() == t0 && area <= orig_area)){    // -- allow null moves
        if (fs.size() == 1)
            remove(wb);
        return true;
    }

    // Undo:
    load(wb[0]) = orig_load_wb0;
    load(wb)    = orig_load_wb;
    area = orig_area;

    w0.set(wb);

    enqueue(wb[0]);
    enqueue(wb);
    enqueue(w0);

    incUpdate();
  #if defined(CONSISTENCY_CHECKING)
    verifyTiming();
  #endif
    /**/if (!(maxArrival() == t0)) Dump(t0, maxArrival());
    assert(maxArrival() == t0);

    return false;
}


void DelayOpt::optimize()
{
    //**/nameByCurrentId(N);
    //**/N.write("N_dopt.gig");
    //**/WriteLn "Wrote: N_dopt.gig";

    // Optimization loop:
    signal(SIGINT, SIGINT_handler);

    WriteLn "Starting arrival time: %_   (area: %_)", maxArrival(), area;
    NewLine;

    Get_Pob(N, dyn_fanouts);

    uint n_resize  = 0;
    uint n_resize2 = 0;
    uint n_buffer  = 0;
    uint n_buffer2 = 0;
    uint n_bypass  = 0;
    for(;;){
        // Improving delay:
        Vec<GLit> path;
        bool improved = false;
        for (uint n = 0; n < 20; n++){
            for(;;){
                if (ctrl_c_pressed){ WriteLn "\n**** CTRL-C pressed, aborting ****"; goto Done; }

                Wire w_po = po_list[crit.peek()] + N;
                getCritPath(w_po, path);

                for (uint i = 0; i < path.size(); i++){
                    Wire w = path[i] + N;
                    if (type(w) == gate_Clamp) continue;

                    if (tryResize(w, false)) { n_resize++; goto Found; }
                    Fanouts fs = dyn_fanouts[w];
                    for (uint j = 0; j < fs.size(); j++){
                        if (tryResize(fs[j], false)) { n_resize++; goto Found; }
                        if (tryResize2(w, fs[j]))    { n_resize2++; goto Found; }
                    }

                    For_Inputs(w, v){
                        if (tryResize(v, false)) { n_resize++; goto Found; }
                        if (tryResize2(w, v))    { n_resize2++; goto Found; }
                    }

                    if (isBuf(w) && tryBypass(w)){ n_bypass++; goto Found; }
                    if (tryBuffering(w))         { n_buffer++; goto Found; }
                    if (tryBuffering2(w))        { n_buffer2++; goto Found; }
                }
                break;

              Found:;
                improved = true;
                Write "\r\a/DELAY:\a/  arrival: \a*%.2f ps\a*   area: \a*%,d\a*   (rsz=%_/%_  byp=%_  buf=%_/%_)   [%t]\f",
                    L.ps(maxArrival()), (uint64)area, n_resize, n_resize2, n_bypass, n_buffer, n_buffer2, cpuTime();
            }
        }
        if (improved)
            NewLine;

        // Improving area:
        improved = false;
        For_Gatetype(N, gate_Uif, w){
            if (ctrl_c_pressed){ WriteLn "\n**** CTRL-C pressed, aborting ****"; goto Done; }
            if (w == w_buf) continue;   // <<== need a better mechanism to avoid these...
            if (type(w) == gate_Clamp) continue;

            bool update = false;
            if (isBuf(w) && tryBypass(w)){ n_bypass++; update = true; improved = true; }
            else if (tryResize(w, true)) { n_resize++; update = true; improved = true; }

            if (update)
                Write "\r\a/AREA: \a/  arrival: \a*%.2f ps\a*   area: \a*%,d\a*   (rsz=%_/%_  byp=%_  buf=%_/%_)   [%t]\f",
                    L.ps(maxArrival()), (uint64)area, n_resize, n_resize2, n_bypass, n_buffer, n_buffer2, cpuTime();
        }
        if (improved)
            NewLine;

        if (ctrl_c_pressed){ WriteLn "\n**** CTRL-C pressed, aborting ****"; goto Done; }
    }

  Done:;
    if (w_buf)
        remove(w_buf);  // <<== handle better
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void DelayOpt::run(uint pre_buffer, bool forget_sizes)
{
    approx = 0;

    // Clear internal names:  (needed if feeding result of this algorithm back to itself)
    {
        Vec<char> tmp;
        For_Gates(N, w){
            for (uint i = 0; i < N.names().size(w); i++){
                cchar* name = N.names().get(w, tmp, i);
                if (strstr(name, "ZZ^")){
                    N.names().clear(w);
                    break;
                }
            }
        }
    }

    // Change representation of multi-output gates:
    groupCellTypes(L, groups);
    splitMultiOutputCells(N, L, groups, next_output, dcells);
    findBufferGroup();

    // Forget sizes?
    if (forget_sizes){
        For_Gatetype(N, gate_Uif, w){
            uint g = dcells[attr_Uif(w).sym].group;
          attr_Uif(w).sym = groups[g][0];                             // -- smallest
          //attr_Uif(w).sym = groups[g].last();                         // -- largest
          //attr_Uif(w).sym = groups[g][irand(seed, groups[g].size())]; // -- random
          //attr_Uif(w).sym = groups[g][groups[g].size() / 2];          // -- average
        }
    }

    // Get rid of massive fanouts:
    Auto_Pob(N, dyn_fanouts);
    if (pre_buffer > 0)
        preBuffer(N, L, pre_buffer, groups[buf_group][0]);

    // Topological order:
    Vec<GLit> order;
    topoOrder(N, order);

    // Levelize circuit:
    level.nil = 0;
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        For_Inputs(w, v)
            newMax(level(w), level[v] + 1);
    }

    // Stretch levelization (to allow for buffering):
    uint max_level = 0;
    For_Gates(N, w)
        newMax(max_level, level[w]);
    uint shift = 0;
    while (max_level < (1u << 31)){
        shift++;
        max_level *= 2; }
    For_Gates(N, w)
        level(w) <<= shift;

    // Compute initial timing:
    computeLoads(N, dcells, wire_cap, load);

    staticTiming(N, dcells, load, order, approx, arr, slew);
    float max_arr = 0;
    For_Gates(N, w){
        newMax(max_arr, arr[w].rise);
        newMax(max_arr, arr[w].fall); }

    area = getTotalArea(N, dcells);

    // Populate 'crit':
    assert(checkNumberingPOs(N));
    crit.prio = &po_arr;
    For_Gatetype(N, gate_PO, w){
        uint num = attr_PO(w).number;
        po_arr(num, 0.0f) = max_(arr[w[0]].rise, arr[w[0]].fall);
        po_list(num) = w;
        crit.add(num);
    }

    optimize();

    // Name buffers:   <<== make this an option
    uint bufC = 0;
    String tmp;
    For_Gatetype(N, gate_Uif, w){
        if (dcells[attr_Uif(w).sym].group == buf_group && N.names().size(w) == 0){
            FWrite(tmp) "ZZ^buf<%_>", bufC;
            N.names().add(w, tmp.slice());
            tmp.clear();

            FWrite(tmp) "inst`ZZ^buf<%_>", bufC++;
            N.names().add(w, tmp.slice());
            tmp.clear();
        }
    }

    mergeMultiOutputCells(N, L, groups, next_output, dcells);

    // Name multi-output instances:   <<== worth preserving their names? or change representation back to original...
    uint instC = 0;
    For_Gatetype(N, gate_Uif, w){
        if (N.names().size(w) == 0){
            FWrite(tmp) "inst`ZZ^multi<%_>", instC++;
            N.names().add(w, tmp.slice());
            tmp.clear();
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper function:


// NOTE! Will change the representation of multi-fanout gates (for now).
void optimizeDelay(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, uint pre_buffer, bool forget_sizes)
{
    DelayOpt opt(N, L, wire_cap);
    opt.run(pre_buffer, forget_sizes);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
