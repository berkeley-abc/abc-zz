//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelayOpt2.cc
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Second attempt at delay optimization.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "DelayOpt2.hh"
#include "TimingRef.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/Heap.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "OrgCells.hh"
#include <csignal>


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


ZZ_PTimer_Add(cont_eval_phase);
ZZ_PTimer_Add(cont_resize_phase);
ZZ_PTimer_Add(cont_between_phase);

ZZ_PTimer_Add(cont_max_of);
ZZ_PTimer_Add(cont_softmax_of);

ZZ_PTimer_Add(cont_eval);
ZZ_PTimer_Add(cont_inc_update);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Signal handler -- temporary:


static volatile bool ctrl_c_pressed = false;

extern "C" void SIGINT_handler2(int signum);
void SIGINT_handler2(int signum)
{
    if (ctrl_c_pressed) _exit(0);
    ctrl_c_pressed = true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


macro double sq(double x) { return x * x; }


static
float maxOf(const TMap& ts)
{
    /*T*/ZZ_PTimer_Scope(cont_max_of);
    float m = -FLT_MAX;
    for (uint i = 0; i < ts.base().size(); i++){
        newMax(m, ts.base()[i].rise);
        newMax(m, ts.base()[i].fall);
    }
    return m;
}


macro float maxOf(const TValues& v) {
    return max_(v.rise, v.fall); }


macro float maxAbs(const TValues& v) {
    return max_(fabsf(v.rise), fabsf(v.fall)); }


static
double softMaxOf(const TMap& arr, const TMap& dep)
{
    /*T*/ZZ_PTimer_Scope(cont_softmax_of);
    // Find length of critical path:
    float max_len = -FLT_MAX;
    uint  best_i = 0;
    assert(arr.base().size() == dep.base().size());
    for (uint i = 0; i < arr.base().size(); i++){
        TValues a = arr.base()[i];
        TValues d = dep.base()[i];
        if (newMax(max_len, a.rise + d.rise)) best_i = i;
        if (newMax(max_len, a.fall + d.fall)) best_i = i;
    }

    // Compute soft maximum:
    double sum = 0;
    for (uint i = 0; i < arr.base().size(); i++){
        if (i == best_i) continue;  // -- makes no real difference... (but to be absolutely correct)

        TValues a = arr.base()[i];
        TValues d = dep.base()[i];
        double  v = max_(a.rise + d.rise, a.fall + d.fall) - max_len;
        sum += exp(v);   // <<== prova att sortera; se om skillnad
    }

    return log1p(sum) + max_len;
}


macro bool almostEq(const TValues& x, const TValues& y)
{
    if (maxAbs(x - y) < 1e-6) return true;
    return false;
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Supporting types:


struct ContCell {
    const SC_Cell& cell0;
    const SC_Cell& cell1;
    const float frac;
    ContCell(const SC_Cell& cell0_, const SC_Cell& cell1_, float frac_) : cell0(cell0_), cell1(cell1_), frac(frac_) {}

    TValues inCap(uint pin) const {
        return TValues(cell0.pins[pin].rise_cap * (1-frac) + cell1.pins[pin].rise_cap * frac,
                       cell0.pins[pin].fall_cap * (1-frac) + cell1.pins[pin].fall_cap * frac);
    }
};


struct LevQueue_lt {
    NetlistRef        N;
    const WMap<uint>& level;

    LevQueue_lt(NetlistRef N_, const WMap<uint>& level_) : N(N_), level(level_) {}
    bool operator()(GLit x, GLit y) const { return level[x + N] < level[y + N]; }
};


class FlowMap {
    WMap<uint> moff;    // -- 'flow[moff[w] + pin]' gives flow for input pin 
    Vec<TValues> flow;

public:
    FlowMap(NetlistRef N){
        uint offC = 0;
        For_Gates(N, w){
            moff(w) = offC;
            offC += w.size();
        }
        flow.growTo(offC, TValues(0, 0));
    }

    TValues* operator[](Wire w) { return &flow[moff[w]]; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'DelayOpt' class:


class DelayOpt {
  //________________________________________
  //  Types:

    typedef KeyHeap<GLit, false, LevQueue_lt> LevQueue;
    typedef KeyHeap<GLit, true , LevQueue_lt> RevQueue;

  //________________________________________
  //  State:

    // Problem input:
    NetlistRef              N;
    const SC_Lib&           L;
    const Vec<float>&       wire_cap;
    const Params_DelayOpt&  P;

    // Derived:
    Vec<Vec<uint> >       groups;
    Vec<Pair<uint,uint> > group_inv;    // -- Location of cell in 'groups': group_inv[cell_idx] == (group_no, alt_no)
    uint                  buf_sym;
    uint                  buf_grp;
    Vec<GLit>             order;        // -- topological order; may be empty during phases where gates are added or removed

    // Timing data:
    TMap            load;
    TMap            arr;
    TMap            slew;
    TMap            dep;
    float           area;

    // Continuous model:
    WMap<float>     alt;        // -- cell alternative: '0 .. n-1', where 'n' is the number of functionally equivalent cells

    // Incremental update:
    WMap<uint>      level;
    LevQueue        Q;
    WSeen           in_Q;
    RevQueue        R;
    WSeen           in_R;

    // Update selection:
    Vec<float>      len;
    IdHeap<float,1> crit;

    // Other:
    uint64          seed;

    // Temporaries:
    WZet            win;
    Vec<TValues>    orig_arr;
    Vec<TValues>    orig_dep;
    Vec<TValues>    orig_slew;

  //________________________________________
  //  Internal Methods:

    // Helpers:
    uint  grpNo   (Wire w) const;
    uint  altNo   (Wire w) const;
    void  setAltNo(Wire w, uint alt);
    uint  maxAltNo(Wire w) const;

    float computeCritLen(uint approx = UINT_MAX);
    void  setupOrder();

    bool  capOk(Wire w, Wire w0, uint out_pin);

    void  clearInternalNames();
    void  addInternalNames();

    void  enqueue(Wire w);
    Wire  dequeue();
    void  enqueueR(Wire w);
    Wire  dequeueR();

    // Continuous sizing:
    void     contStaticTiming();
    ContCell contCell(Wire w, float alt) const;
    void     contComputeGateLoad(Wire w);
    void     contUpdateArrival(Wire w, bool update_multi = true);
    void     contUpdateDeparture(Wire w, bool update_multi = true, bool use_win = false);
    void     contUpdateThisDeparture(Wire w);
    void     contIncPropagate();
    void     contResizeGate(Wire w, float new_alt);
    void     contNudge(float eval, Wire w, float step);
    double   contEval(Wire w0, const WSeen& crit, float delta);
    void     continuousResizing();
    void     alternativeResizing(float req_time);

    float    flowEvalGate(Wire w, FlowMap& flow);
    void     flowResizeGate(Wire w, FlowMap& flow);

    // Major methods:
    void  legalize();
    void  preBuffer();

public:
  //________________________________________
  //  Public interface:

    DelayOpt(NetlistRef N_, const SC_Lib& L_, const Vec<float>& wire_cap_, const Params_DelayOpt& P_) :
        N(N_), L(L_), wire_cap(wire_cap_), P(P_),
        buf_sym(UINT_MAX), buf_grp(UINT_MAX), area(-1), Q(LevQueue_lt(N, level)), R(LevQueue_lt(N, level)), seed(DEFAULT_SEED)
        {}

    void run();
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


inline uint DelayOpt::grpNo(Wire w) const {
    assert_debug(type(w) == gate_Uif);
    return group_inv[attr_Uif(w).sym].fst; }

inline uint DelayOpt::altNo(Wire w) const {
    return (type(w) != gate_Uif) ? 0 : group_inv[attr_Uif(w).sym].snd; }

inline void DelayOpt::setAltNo(Wire w, uint alt) {
    if (type(w) != gate_Uif)
        assert(alt == 0);
    else
        attr_Uif(w).sym = groups[group_inv[attr_Uif(w).sym].fst][alt]; }

inline uint DelayOpt::maxAltNo(Wire w) const {
    return (type(w) != gate_Uif) ? 0 : groups[group_inv[attr_Uif(w).sym].fst].size() - 1; }


float DelayOpt::computeCritLen(uint approx)
{
    assert(order.size() > 0);
    TMap load;
    computeLoads(N, L, wire_cap, load);

    TMap arr, slew;
    staticTiming(N, L, load, order, (approx == UINT_MAX) ? P.approx : approx, arr, slew);

    return maxOf(arr);
}


void DelayOpt::setupOrder()
{
    // Topological order:
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
}


// 'w' is the node with the fanouts (Pin or Uif), 'w0' is the standard cell (always Uif).
bool DelayOpt::capOk(Wire w, Wire w0, uint out_pin) {
    const SC_Pin& pin = cell(w0, L).outPin(out_pin);
    return load[w].rise <= pin.max_out_cap && load[w].fall <= pin.max_out_cap; }


// Clear internal names; needed if feeding result of this algorithm back to itself.
void DelayOpt::clearInternalNames()
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


void DelayOpt::addInternalNames()
{
    // Name buffers:
    uint bufC = 0;
    String tmp;
    For_Gatetype(N, gate_Uif, w){
        if (grpNo(w) == buf_grp && N.names().size(w) == 0){
            FWrite(tmp) "ZZ^buf<%_>", bufC;
            N.names().add(w, tmp.slice());
            tmp.clear();

            FWrite(tmp) "inst`ZZ^buf<%_>", bufC++;
            N.names().add(w, tmp.slice());
            tmp.clear();
        }
    }
}


inline void DelayOpt::enqueue(Wire w)
{
    //**/WriteLn "## enqueued: %n", w;
    if (!in_Q.add(w))
        Q.add(w);
}


inline Wire DelayOpt::dequeue()
{
    Wire w = Q.pop() + N;
    in_Q.exclude(w);
    return w;
}


inline void DelayOpt::enqueueR(Wire w)
{
    if (!in_R.add(w))
        R.add(w);
}


inline Wire DelayOpt::dequeueR()
{
    Wire w = R.pop() + N;
    in_R.exclude(w);
    return w;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Legalization:


void DelayOpt::legalize()
{
    assert(order.size() > 0);

    uint pre_violations  = 0;
    uint post_violations = 0;
    uint n_upsized = 0;
    for (uint i = order.size(); i > 0;){ i--;
        Wire w = order[i] + N;

        uint pin = UINT_MAX;
        Wire w0 = Wire_NULL;
        if (type(w) == gate_Uif){
            if (!isMultiOutput(w, L)){
                pin = 0;
                w0 = w;
            }
        }else if (type(w) == gate_Pin){
            pin = attr_Pin(w).number;
            w0 = w[0];
        }

        if (w0){
            if (!capOk(w, w0, pin)){
                pre_violations++;
                bool upsized = false;
                while (!capOk(w, w0, pin)){
                    if (altNo(w0) == maxAltNo(w0)){
                        post_violations++;
                        if (P.verbosity >= 2)
                            WriteLn "WARNING! %n (%_)  set to largest size %_,  load=%_  max_cap=%_", w0, w0, L.cells[attr_Uif(w0).sym].name, load[w], cell(w0, L).outPin(pin).max_out_cap;
                        break;
                    }else{
                        For_Inputs(w0, v){  // -- subtract load of current gate
                            load(v).rise -= cell(w0,L).pins[Iter_Var(v)].rise_cap;
                            load(v).fall -= cell(w0,L).pins[Iter_Var(v)].fall_cap; }

                        setAltNo(w0, altNo(w0) + 1);
                        upsized = true;

                        For_Inputs(w0, v){  // -- add load of new gate
                            load(v).rise += cell(w0,L).pins[Iter_Var(v)].rise_cap;
                            load(v).fall += cell(w0,L).pins[Iter_Var(v)].fall_cap; }
                    }
                }

                n_upsized += (uint)upsized;
            }
        }
    }

    if (P.verbosity >= 1)
        WriteLn "Legalization violations:  \a*%_\a* -> \a*%_\a*   (upsized gates: %_)",
            pre_violations, post_violations, n_upsized;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pre-buffer:


void DelayOpt::preBuffer()
{
    assert(order.size() > 0);

    if (P.prebuf_lo == 0) return;
    uint lo_lim = P.prebuf_lo;
    uint hi_lim = P.prebuf_hi;

    assert(hi_lim >= lo_lim);
    assert(lo_lim > 1);

    Auto_Pob(N, dyn_fanouts);

    // Get reverse level (est. of departure time):
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

    Heap<uint,Connect> Q;
    Vec<uint>    ds;
    Vec<Connect> ws;
    For_Gates(N, w){
        if (isMultiOutput(w, L)) continue;

        Fanouts fs = dyn_fanouts[w];
        if (fs.size() > hi_lim){
            assert(Q.size() == 0);
            for (uint i = 0; i < fs.size(); i++)
                Q.add(rlevel[fs[i]], fs[i]);

            while (Q.size() > hi_lim){
                // Pull out 'hi_lim' nodes:
                for (uint i = 0; i <= hi_lim; i++){     // -- pull one extra out
                    ds.push(Q.peekKey());
                    ws.push(Q.peekValue());
                    Q.pop();
                }

                // Select split point:
                uint split = lo_lim;
                uint delta = ds[split] - ds[split-1];
                for (uint i = lo_lim+1; i <= hi_lim; i++)
                    if (newMax(delta, ds[i] - ds[i-1]))
                        split = i;

                // Put back superfluous nodes:
                for (uint i = split; i <= hi_lim; i++)
                    Q.add(ds[i], ws[i]);

                // Buffer the rest:
                Wire w_buf = N.add(Uif_(buf_sym), w);
                n_bufs++;
                uint rlev = 0;
                for (uint i = 0; i < split; i++){
                    newMax(rlev, rlevel[ws[i]]);
                    ws[i].set(w_buf);
                }
                rlevel(w_buf) = rlev + 1;
                Q.add(rlevel[w_buf], Connect(w_buf, 0));

                ds.clear();
                ws.clear();
            }
            Q.clear();
        }
    }

    // Recompute order:
    if (n_bufs > 0)
        setupOrder();

    if (P.verbosity >= 1)
        WriteLn "\a/Pre-buffering:\a/ inserted \a*%,d\a* buffers", n_bufs;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Continuous sizing:


static
float contGateArea(const ContCell& cc)
{
    return cc.cell0.area * (1 - cc.frac) + cc.cell1.area * cc.frac;
}


static
void contTimeGate(const ContCell& cc, uint out_pin, uint in_pin,
                  TValues arr_in, TValues slew_in, TValues load, uint approx, TValues& arr, TValues& slew)
{
    const SC_Pin&     pin0 = cc.cell0.outPin(out_pin);
    const SC_Pin&     pin1 = cc.cell1.outPin(out_pin);
    const SC_Timings& ts0  = pin0.rtiming[in_pin];
    const SC_Timings& ts1  = pin1.rtiming[in_pin];

    if (ts0.size() == 0 || ts1.size() == 0) return;
        // -- no timing dependency between input and output pins ('ts0' and 'ts1' should always be
        // the same size, but in principle, the Liberty file may not follow this)

    const SC_Timing& t0 = ts0[0];
    const SC_Timing& t1 = ts1[0];

    // Time gate with separate max for "arrival" and "output slew":
    if (t0.tsense == sc_ts_Pos || t0.tsense == sc_ts_Non){
        newMax(arr .rise, arr_in.rise + (1-cc.frac)*lookup(t0.cell_rise , slew_in.rise, load.rise, approx) + cc.frac*lookup(t1.cell_rise , slew_in.rise, load.rise, approx));
        newMax(arr .fall, arr_in.fall + (1-cc.frac)*lookup(t0.cell_fall , slew_in.fall, load.fall, approx) + cc.frac*lookup(t1.cell_fall , slew_in.fall, load.fall, approx));
        newMax(slew.rise, (1-cc.frac)*lookup(t0.rise_trans, slew_in.rise, load.rise, approx) + cc.frac*lookup(t1.rise_trans, slew_in.rise, load.rise, approx));
        newMax(slew.fall, (1-cc.frac)*lookup(t0.fall_trans, slew_in.fall, load.fall, approx) + cc.frac*lookup(t1.fall_trans, slew_in.fall, load.fall, approx));
    }

    if (t0.tsense == sc_ts_Neg || t0.tsense == sc_ts_Non){
        newMax(arr .rise, arr_in.fall + (1-cc.frac)*lookup(t0.cell_rise , slew_in.fall, load.rise, approx) + cc.frac*lookup(t1.cell_rise , slew_in.fall, load.rise, approx));
        newMax(arr .fall, arr_in.rise + (1-cc.frac)*lookup(t0.cell_fall , slew_in.rise, load.fall, approx) + cc.frac*lookup(t1.cell_fall , slew_in.rise, load.fall, approx));
        newMax(slew.rise, (1-cc.frac)*lookup(t0.rise_trans, slew_in.fall, load.rise, approx) + cc.frac*lookup(t1.rise_trans, slew_in.fall, load.rise, approx));
        newMax(slew.fall, (1-cc.frac)*lookup(t0.fall_trans, slew_in.rise, load.fall, approx) + cc.frac*lookup(t1.fall_trans, slew_in.rise, load.fall, approx));
    }
}


static
void contRevTimeGate(const ContCell& cc, uint out_pin, uint in_pin,
                     TValues dep_out, TValues slew_in, TValues load, uint approx, TValues& dep)
{
    const SC_Pin&     pin0 = cc.cell0.outPin(out_pin);
    const SC_Pin&     pin1 = cc.cell1.outPin(out_pin);
    const SC_Timings& ts0  = pin0.rtiming[in_pin];
    const SC_Timings& ts1  = pin1.rtiming[in_pin];

    if (ts0.size() == 0 || ts1.size() == 0) return;
        // -- no timing dependency between input and output pins ('ts0' and 'ts1' should always be
        // the same size, but in principle, the Liberty file may not follow this)

    const SC_Timing& t0 = ts0[0];
    const SC_Timing& t1 = ts1[0];

    if (t0.tsense == sc_ts_Pos || t0.tsense == sc_ts_Non){
        newMax(dep.rise, dep_out.rise + (1-cc.frac)*lookup(t0.cell_rise, slew_in.rise, load.rise, approx) + cc.frac*lookup(t1.cell_rise, slew_in.rise, load.rise, approx));
        newMax(dep.fall, dep_out.fall + (1-cc.frac)*lookup(t0.cell_fall, slew_in.fall, load.fall, approx) + cc.frac*lookup(t1.cell_fall, slew_in.fall, load.fall, approx));
    }

    if (t0.tsense == sc_ts_Neg || t0.tsense == sc_ts_Non){
        newMax(dep.fall, dep_out.rise + (1-cc.frac)*lookup(t0.cell_rise , slew_in.fall, load.rise, approx) + cc.frac*lookup(t1.cell_rise , slew_in.fall, load.rise, approx));
        newMax(dep.rise, dep_out.fall + (1-cc.frac)*lookup(t0.cell_fall , slew_in.rise, load.fall, approx) + cc.frac*lookup(t1.cell_fall , slew_in.rise, load.fall, approx));
    }
}


ContCell DelayOpt::contCell(Wire w, float alt) const
{
    const Vec<uint>& gs = groups[grpNo(w)];
    uint a = (uint)alt;
    if (a == gs.size()-1)
        return ContCell(L.cells[gs[a]], L.cells[gs[a]], 0);
    else{
        //**/WriteLn "// gate: %n  alts: %_, %_   frac: %_", w, L.cells[gs[a]].name, L.cells[gs[a+1]].name, alt - a;
        return ContCell(L.cells[gs[a]], L.cells[gs[a+1]], alt - a);
    }
}


void DelayOpt::contComputeGateLoad(Wire w)
{
    assert(!isMultiOutput(w, L));

    Get_Pob(N, dyn_fanouts);
    Fanouts fs = dyn_fanouts[w];

    load(w) = 0;
    for (uint i = 0; i < fs.size(); i++){
        Connect  c = fs[i];
        if (type(c) != gate_Uif) continue;

        ContCell cc = contCell(c, alt[c]);
        load(w) += cc.inCap(c.pin);
    }

    if (wire_cap.size() > 0){
        uint n = min_(fs.size(), wire_cap.size() - 1);
        load(w).rise += wire_cap[n];
        load(w).fall += wire_cap[n];
    }
}


void DelayOpt::contUpdateArrival(Wire w, bool update_multi)
{
    if (!((type(w) == gate_Uif && !isMultiOutput(w, L)) || type(w) == gate_Pin)) return;

    Wire w_uif   = (type(w) == gate_Pin) ? w[0] : w; assert(type(w_uif) == gate_Uif);
    uint out_pin = (type(w) == gate_Pin) ? attr_Pin(w).number : 0;
    ContCell cc = contCell(w_uif, alt[w_uif]);

    arr(w) = slew(w) = TValues(0, 0);
    For_Inputs(w_uif, v)
        contTimeGate(cc, out_pin, Iter_Var(v), arr[v], slew[v], load[w], P.approx, arr(w), slew(w));

    if (update_multi && w_uif != w){
        newMax(arr(w_uif).rise, arr[w].rise);
        newMax(arr(w_uif).fall, arr[w].fall);
    }
}


// NOTE! Computes 'w's impact on the departure time of its CHILDREN, not 'w' itself. The departure
// time of the children CAN ONLY INCREASE, and must be zeroed before calling this method.
void DelayOpt::contUpdateDeparture(Wire w, bool update_multi, bool use_win)
{
    if (!((type(w) == gate_Uif && !isMultiOutput(w, L)) || type(w) == gate_Pin)) return;

    Wire w_uif   = (type(w) == gate_Pin) ? w[0] : w; assert(type(w_uif) == gate_Uif);
    uint out_pin = (type(w) == gate_Pin) ? attr_Pin(w).number : 0;
    ContCell cc = contCell(w_uif, alt[w_uif]);

    if (w_uif != w){
        if (arr[w].rise == arr[w_uif].rise)   // -- only update departure if this is the critical output
            newMax(dep(w_uif).rise, dep[w].rise);
        if (arr[w].fall == arr[w_uif].fall)
            newMax(dep(w_uif).fall, dep[w].fall);
    }

    For_Inputs(w_uif, v)
        if (!use_win || win.has(v))
            contRevTimeGate(cc, out_pin, Iter_Var(v), dep[w], slew[v], load[w], P.approx, dep(v));
}


void DelayOpt::contUpdateThisDeparture(Wire w)
{
    assert(!isMultiOutput(w, L));

    Get_Pob(N, dyn_fanouts);
    Fanouts fs = dyn_fanouts[w];

    dep(w) = TValues(0, 0);
    for (uint i = 0; i < fs.size(); i++){
        Connect  c = fs[i];
        if (type(c) != gate_Uif) continue;
        ContCell cc = contCell(c, alt[c]);

        if (!isMultiOutput(c, L))
            contRevTimeGate(cc, 0/*out_pin*/, c.pin, dep[c], slew[w], load[c], P.approx, dep(w));
        else{
            Fanouts gs = dyn_fanouts[c];
            for (uint j = 0; j < gs.size(); j++){
                // Complicated code to make sure partial update to departure time w.r.t. rising/falling edge:
                assert(type(gs[j]) == gate_Pin);
                if (arr[c].rise == arr[gs[j]].rise){    // -- only update departure if this is the critical output
                    if (arr[c].fall == arr[gs[j]].fall){
                        contRevTimeGate(cc, attr_Pin(gs[j]).number, c.pin, dep[gs[j]], slew[w], load[gs[j]], P.approx, dep(w));
                    }else{
                        float tmp = dep[w].fall;
                        contRevTimeGate(cc, attr_Pin(gs[j]).number, c.pin, dep[gs[j]], slew[w], load[gs[j]], P.approx, dep(w));
                        dep(w).fall = tmp;
                    }
                }else{
                    float tmp = dep[w].rise;
                    contRevTimeGate(cc, attr_Pin(gs[j]).number, c.pin, dep[gs[j]], slew[w], load[gs[j]], P.approx, dep(w));
                    dep(w).rise = tmp;
                }
            }
        }
    }
}


void DelayOpt::contStaticTiming()
{
    load.clear();
    arr .clear();
    slew.clear();
    dep .clear();

    // Compute loads:
    For_Gatetype(N, gate_Uif, w){
        ContCell cc = contCell(w, alt[w]);
        For_Inputs(w, v)
            load(v) += cc.inCap(Iter_Var(v));
    }

    Auto_Pob(N, fanout_count);  // <<== dyn_fanouts?
    if (wire_cap.size() > 0){
        For_Gates(N, w){
            uint n = fanout_count[w];
            newMin(n, wire_cap.size() - 1);
            load(w).rise += wire_cap[n];
            load(w).fall += wire_cap[n];
        }
    }

    // Compute arrival times:
    for (uint i = 0; i < order.size(); i++)
        contUpdateArrival(order[i] + N);

    // Compute departure times:
    for (uint i = order.size(); i > 0;) i--,
        contUpdateDeparture(order[i] + N);
}


void DelayOpt::contIncPropagate()
{
    Get_Pob(N, dyn_fanouts);

    // Forward propagation:
    while (Q.size() > 0){
        Wire w = dequeue();
        if (isMultiOutput(w, L)){
            arr(w) = slew(w) = TValues(0, 0);
            Fanouts fs = dyn_fanouts[w];
            for (uint i = 0; i < fs.size(); i++)
                enqueue(fs[i]);

        }else{
            TValues curr_arr  = arr[w];
            TValues curr_slew = slew[w];
            contUpdateArrival(w);
            //**/WriteLn "## arr [%n] : %_ -> %_", w, curr_arr , arr [w];
            //**/WriteLn " | slew[%n] : %_ -> %_", w, curr_slew, slew[w];

            if (!almostEq(arr[w], curr_arr) || !almostEq(slew[w], curr_slew)){
                Fanouts fs = dyn_fanouts[w];
                for (uint i = 0; i < fs.size(); i++)
                    enqueue(fs[i]);
            }

            if (!almostEq(slew[w], curr_slew)){
                enqueueR(w); }
        }
    }

    // Backward propagation:
    while (R.size() > 0){
        Wire w = dequeueR();
        if (isMultiOutput(w, L)){
            dep(w) = TValues(0, 0);
            Fanouts fs = dyn_fanouts[w];
            for (uint i = 0; i < fs.size(); i++){
                if (arr[w].rise == arr[fs[i]].rise)    // -- only update departure if this is the critical output
                    newMax(dep(w).rise, dep[fs[i]].rise);
                if (arr[w].fall == arr[fs[i]].fall)
                    newMax(dep(w).fall, dep[fs[i]].fall);
            }
            For_Inputs(w, v)
                enqueueR(v);

        }else{
            TValues curr_dep  = dep[w];
            contUpdateThisDeparture(w);

            if (!almostEq(dep[w], curr_dep)){
                For_Inputs(w, v)
                    enqueueR(v);
            }
        }
    }

#if 0   /*DEBUG*/
    TMap load_copy, slew_copy, arr_copy, dep_copy;
    load.copyTo(load_copy);
    slew.copyTo(slew_copy);
    arr .copyTo(arr_copy);
    dep .copyTo(dep_copy);
    contStaticTiming();
    bool mismatch = false;
    For_Gates(N, w){
        if (maxAbs(load[w] - load_copy[w]) > 1e-3){ WriteLn "Mismatch: %n  orig_load=%_  cont_load=%_", w, load_copy[w], load[w]; mismatch = true; }
        if (maxAbs(slew[w] - slew_copy[w]) > 1e-3){ WriteLn "Mismatch: %n  orig_slew=%_  cont_slew=%_", w, slew_copy[w], slew[w]; mismatch = true; }
        if (maxAbs(arr [w] - arr_copy [w]) > 1e-3){ WriteLn "Mismatch: %n  orig_arr=%_   cont_arr=%_" , w, arr_copy [w], arr [w]; mismatch = true; }
        if (maxAbs(dep [w] - dep_copy [w]) > 1e-3){ WriteLn "Mismatch: %n  orig_dep=%_   cont_dep=%_" , w, dep_copy [w], dep [w]; mismatch = true; }

    }
    if (mismatch){
        N.write("N.gig"); WriteLn "Wrote: N.gig";
        exit(1);
    }
    WriteLn "Continuous static timing verified";
#endif  /*END DEBUG*/
}


void DelayOpt::contResizeGate(Wire w0, float new_alt)
{
    /*T*/ZZ_PTimer_Scope(cont_inc_update);

    //**/WriteLn "##----------------------------------------";
    assert(type(w0) == gate_Uif);

    newMax(new_alt, 0.0f);
    newMin(new_alt, (float)maxAltNo(w0));
    if (new_alt == alt[w0]) return;

    //**/WriteLn "## alt[%n] : %_ -> %_", w0, alt[w0], new_alt;
    alt(w0) = new_alt;
    enqueue(w0);
    For_Inputs(w0, v){
        //**/Write "## load[%n] : %_", v, load[v];
        contComputeGateLoad(v);     // <<== do a differential update with occasional refresh (using a counter)? BEWARE: in principle, a gate may feed several outputs of the same gate (pre-condition?)
        //**/WriteLn " -> %_", load[v];
        enqueue(v);
        enqueueR(v);
    }

    contIncPropagate();     // <<== option to delay this until all gates have been resized (faster but less precise (?))
}


inline void DelayOpt::contNudge(float eval, Wire w, float step)
{
    contResizeGate(w, alt[w] + ((eval > 0) ? -step : +step));
}


double DelayOpt::contEval(Wire w0, const WSeen& crit, float delta)
{
    /*T*/ZZ_PTimer_Scope(cont_eval);
    assert(type(w0) == gate_Uif);

    Get_Pob(N, dyn_fanouts);

    // Setup evaluation window:
    win.clear();
    win.add(w0); assert(crit.has(w0));
    For_Inputs(w0, v)
        if (crit.has(v))
            win.add(v);

    uint lev_count = 0;
    uint lev_end = win.size();
    assert(win.size() == win.list().size());
    for (uint i = 0; i < win.size(); i++){
        if (i == lev_end){
            lev_count++;
            if (lev_count == P.C.eval_levels) break; }

        Wire v = win.list()[i];
        Fanouts fs = dyn_fanouts[v];
        for (uint j = 0; j < fs.size(); j++){
            Wire u = fs[j];
            if (crit.has(u)){
                win.add(u);

                if (isMultiOutput(u, L)){           // -- one "level" of logic is two levels in the netlist for multi-output nodes
                    Fanouts gs = dyn_fanouts[u];
                    for (uint k = 0; k < gs.size(); k++)
                        if (crit.has(gs[k]))
                            win.add(gs[k]);
                }
            }
        }
    }

    for (uint i = 0; i < win.list().size(); i++){   // -- exclude multi-output Uifs (we have their Pins)
        Wire w = win.list()[i];
        if (isMultiOutput(w, L))
            win.exclude(w);
    }
    win.compact();

    sobSort(sob(win.list(), proj_lt(brack<uint,Wire>(level))));     // -- sort 'win.list()' on level
    //**/WriteLn "window: %n", win.list();

    // Resize 'w0' and update loads:
    float orig_alt = alt[w0];
    alt(w0) += delta;

    TValues orig_load[16]; assert(w0.size() <= 16);
    for (uint i = 0; i < w0.size(); i++){
        Wire v = w0[i];
        if (v == Wire_NULL)     continue;
        if (type(v) == gate_PI) continue;   // -- don't touch PIs

        orig_load[i] = load[v];

        load(v) -= contCell(w0, orig_alt).inCap(i);
        load(v) += contCell(w0, alt[w0] ).inCap(i);
    }

    // Recompute timing for window:
    assert(win.size() == win.list().size());
    for (uint i = 0; i < win.size(); i++){
        Wire w = win.list()[i];

        orig_arr (i) = arr [w];
        orig_slew(i) = slew[w];
        orig_dep (i) = dep [w];

        contUpdateArrival(w, false);
    }

    // Recompute reverse timing for window: (intentionally partial -- both 'w' and its child 'v' must be in the window)
    for (uint i = 0; i < win.size(); i++){
        Wire w = win.list()[i];
        Wire w_uif = (type(w) == gate_Pin) ? w[0] : w; assert(type(w_uif) == gate_Uif);
        For_Inputs(w_uif, v)
            if (win.has(v))
                dep(v) = TValues(0, 0);
    }
    for (uint i = win.size(); i > 0;){ i--;
        Wire w = win.list()[i];
        contUpdateDeparture(w, false, true);
    }

    // Evaluate change in lengths:
    double sum0 = 0;
    double sum1 = 0;
    switch (P.C.eval_fun){
    case Params_ContSize::LINEAR:{
        for (uint i = 0; i < win.size(); i++){
            Wire w = win.list()[i];
            sum0 += maxOf(orig_arr[i] + orig_dep[i]);
            sum1 += maxOf(arr[w] + dep[w]);
        }
        break;}

    case Params_ContSize::SQUARE:{
        for (uint i = 0; i < win.size(); i++){
            Wire w = win.list()[i];
            float len0 = maxOf(orig_arr[i] + orig_dep[i]);
            float len1 = maxOf(arr[w] + dep[w]);
            sum0 += len0 * len0;
            sum1 += len1 * len1;
        }
        sum0 = sqrt(sum0);
        sum1 = sqrt(sum1);
        break;}

    case Params_ContSize::MAX:{
        for (uint i = 0; i < win.size(); i++){
            Wire w = win.list()[i];
            double len0 = maxOf(orig_arr[i] + orig_dep[i]);
            double len1 = maxOf(arr[w] + dep[w]);
            newMax(sum0, len0);
            newMax(sum1, len1);
        }
        break;}

    case Params_ContSize::SOFTMAX:{
        uint  i0 = 0;
        uint  i1 = 0;
        float max0 = 0;
        float max1 = 0;
        for (uint i = 0; i < win.size(); i++){
            Wire w = win.list()[i];
            float len0 = maxOf(orig_arr[i] + orig_dep[i]);
            float len1 = maxOf(arr[w] + dep[w]);
            if (newMax(max0, len0)) i0 = i;
            if (newMax(max1, len1)) i1 = i;
        }

        for (uint i = 0; i < win.size(); i++){
            Wire w = win.list()[i];
            float len0 = maxOf(orig_arr[i] + orig_dep[i]);
            float len1 = maxOf(arr[w] + dep[w]);
            if (i != i0) sum0 += exp(len0 - max0);
            if (i != i1) sum1 += exp(len1 - max1);
        }
        sum0 = log1p(sum0) + max0;
        sum1 = log1p(sum1) + max1;
        break;}

    default: assert(false); }

    // Undo changes:
    alt(w0) = orig_alt;

    for (uint i = 0; i < w0.size(); i++){
        Wire v = w0[i];
        if (v == Wire_NULL)     continue;
        if (type(v) == gate_PI) continue;   // -- don't touch PIs

        load(v) = orig_load[i];
    }

    for (uint i = 0; i < win.size(); i++){
        Wire w = win.list()[i];

        arr (w) = orig_arr [i];
        slew(w) = orig_slew[i];
        dep (w) = orig_dep [i];
    }

    return (sum1 - sum0) / delta;
}


struct AbsFst {
    typedef Pair<float,GLit> Key;
    float operator()(const Key& p) const { return fabsf(p.fst); }
};

/*

*/

void DelayOpt::continuousResizing()
{
    if (P.verbosity >= 1){
        WriteLn "\a/_______________________________________________________________________________\a/";
        WriteLn "\a/||\a/  \a*Continuous Gate-Sizing\a*";
        NewLine;
    }

    Auto_Pob(N, dyn_fanouts);

    For_Gatetype(N, gate_Uif, w)
        alt(w) = altNo(w);

    bool  use_softmax = (P.C.cost_fun == Params_ContSize::DELAY_SOFTMAX);
    float crit_len_cont = maxOf(arr);
    float cost = use_softmax ? softMaxOf(arr, dep) : crit_len_cont;
    float step = P.C.step_init;
    uint  discrC = 0;
    for (uint iter = 0;; iter++){
        if (P.verbosity >= 1){
            if (use_softmax)
                WriteLn "%>4%_:   soft \a*%.2f\a*   cont \a*%.2f\a*   step %.3f   [%t]", iter, L.ps(cost), L.ps(crit_len_cont), step, cpuTime();
            else
                WriteLn "%>4%_:   cont \a*%.2f\a*   step %.3f   [%t]", iter, L.ps(crit_len_cont), step, cpuTime();
        }

        if (step < P.C.step_quit) break;
        if (ctrl_c_pressed){ WriteLn "\n**** CTRL-C pressed, aborting ****"; return; }

        float epsilon = crit_len_cont * P.C.crit_epsilon;
        for (uint n = 0; n < P.C.refine_orbits; n++){
            crit_len_cont = maxOf(arr);

            // Evaluate critical nodes:
            /*T*/ZZ_PTimer_Begin(cont_eval_phase);
            WSeen crit;
            Vec<Pair<double,GLit> > ps;
            For_Gates(N, w){
                if (type(w) != gate_Uif && type(w) != gate_Pin) continue;

                float length = max_(arr[w].rise + dep[w].rise, arr[w].fall + dep[w].fall);
                if (length + epsilon >= crit_len_cont){
                    crit.add(w);
                    if (maxAltNo(w) > 0)
                        ps.push(make_tuple(0, w));
                }
                // <<== beräkna generösare crit region här för softmax?
            }

            for (uint i = 0; i < ps.size(); i++){
                Wire w = ps[i].snd + N;
                float delta = (alt[w] + P.C.eval_dx < maxAltNo(w)) ? P.C.eval_dx : -P.C.eval_dx;
                ps[i].fst = contEval(w, crit, delta);
            }

            sobSort(ordReverse(sob(ps, proj_lt(AbsFst()))));
            /*T*/ZZ_PTimer_End(cont_eval_phase);

            // Resize critical nodes:
            /*T*/ZZ_PTimer_Begin(cont_resize_phase);
            for (uint i = 0; i < ps.size(); i++){
#if 1   /*DEBUG*/
                {
                Wire  w    = ps[i].snd + N;
                float delta = (alt[w] + P.C.eval_dx < maxAltNo(w)) ? P.C.eval_dx : -P.C.eval_dx;
                if (fabsf(contEval(w, crit, delta)) < fabsf(ps[0].fst) * P.C.move_lim)
                    continue;
                }
#endif  /*END DEBUG*/

                if (fabsf(ps[i].fst) < fabsf(ps[0].fst) * P.C.move_lim){
                    ps.shrinkTo(i);
                    break; }

                Wire  w    = ps[i].snd + N;
                float eval = ps[i].fst;

                if (P.C.move_type == Params_ContSize::NUDGE){
                    contNudge(eval, w, step);

                }else{ assert(P.C.move_type == Params_ContSize::NEWTON);
                    double dx = P.C.eval_ddx;
                    if (alt[w] < dx || alt[w] > maxAltNo(w) - dx)
                        contNudge(eval, w, step);
                    else{
                        double r1 = contEval(w, crit, -dx);
                        double r2 = contEval(w, crit, +dx);
                        double avg_r = (r1 + r2) / 2;
                        double move = avg_r / (r1-r2);
                        if (r2 > r1){
                            newMin(move, +step);
                            newMax(move, -step);
                            contResizeGate(w, alt[w] + (float)move);
                        }else
                            contNudge(eval, w, step);
                    }
                }
            }
            /*T*/ZZ_PTimer_End(cont_resize_phase);
        }

        // Recompute timing info for whole circuit:
        /*T*/ZZ_PTimer_Begin(cont_between_phase);
        crit_len_cont = maxOf(arr);
        float new_cost = use_softmax ? softMaxOf(arr, dep) : crit_len_cont;

        if (new_cost >= cost){
            step *= P.C.step_reduce;
            discrC++;
            if (discrC == P.C.discr_freq){
                // Discretize:
                discrC = 0;

                For_Gatetype(N, gate_Uif, w){
                    alt(w) = floor(alt[w] + P.C.discr_upbias);
                    setAltNo(w, (uint)alt[w]); }

                contStaticTiming();
                crit_len_cont = maxOf(arr);
                new_cost = use_softmax ? softMaxOf(arr, dep) : crit_len_cont;

                if (P.verbosity >= 1)
                    WriteLn "[DISCRETIZE]   delay \a/%.2f ps\a/   area \a/%,d\a/", L.ps(crit_len_cont), (uint64)(getTotalArea(N, L) + 0.5);
            }
        }
        cost = new_cost;
        /*T*/ZZ_PTimer_End(cont_between_phase);
    }

    // Final discretization:
    For_Gatetype(N, gate_Uif, w)
        setAltNo(w, (uint)floor(alt[w] + 0.5));

    // Static timing:
    load.clear(); arr.clear(); dep.clear(); slew.clear();
    computeLoads(N, L, wire_cap, load);
    staticTiming(N, L, load, order, P.approx, arr, slew);
    revStaticTiming(N, L, load, slew, order, P.approx, dep);
    area = getTotalArea(N, L);

    if (P.verbosity >= 1){
        NewLine;
        WriteLn "FINAL   delay \a/%.2f ps\a/   area \a/%,d\a/ ", L.ps(maxOf(arr)), (uint64)(area + 0.5);
        WriteLn "\a/_______________________________________________________________________________\a/";
    }
}


//=================================================================================================
// -- Experimental:


void DelayOpt::alternativeResizing(float req_time)
{
    if (P.verbosity >= 1){
        WriteLn "\a/_______________________________________________________________________________\a/";
        WriteLn "\a/||\a/  \a*Flow-Based Gate-Sizing\a*";
        NewLine;
    }

    Auto_Pob(N, dyn_fanouts);

    For_Gatetype(N, gate_Uif, w)
        alt(w) = altNo(w);

    float crit_len_cont = maxOf(arr);
    WriteLn "cont-delay: %_", L.ps(crit_len_cont);

    if (req_time == 0)
        req_time = crit_len_cont * 0.25;     // <<== for now
    else
        req_time /= L.ps(1);
    WriteLn "Required time set to: %_", L.ps(req_time);

    float total_flow = 100000;
    float lambda = 1.0;

    // Setup flow map:
    FlowMap     flow(N);
    for (uint iter = 0; iter < 50; iter++){
        if (ctrl_c_pressed){ WriteLn "\n**** CTRL-C pressed, aborting ****"; return; }

        lambda = 0.1 / (iter + 3);
        WMap<float> in_flow;

        //req_time = crit_len_cont * 0.95;     // <<== for now

        // Update flow map:
        For_Gates(N, w){
            if (type(w) == gate_PO){
                flow[w][0].rise += (arr[w[0]].rise - req_time) * lambda;
                flow[w][0].fall += (arr[w[0]].fall - req_time) * lambda;
                //**/WriteLn "%_ -- arr: %_   req: %_", w, arr[w[0]], req_time;
            }else if (type(w) == gate_Uif){
                ContCell cc = contCell(w, alt[w]);
                assert(cc.cell0.n_outputs == 1);    // <<== for now, we only handle single output gates

                for (uint pin = 0; pin < w.size(); pin++){
                    TValues arr_out, slew_out;
                    contTimeGate(cc, /*out_pin*/0, pin, arr[w[pin]], slew[w[pin]], load[w/*pin here*/], P.approx, arr_out, slew_out);

                    TValues len = dep[w] + arr_out;
                    flow[w][pin].rise += (len.rise - req_time) * lambda;
                    flow[w][pin].fall += (len.fall - req_time) * lambda;
                    //**/Dump(w, pin, arr_out, arr[w[pin]], flow[w][pin], arr[w]);
                }
            }
        }

        // Seed flow at fanin of POs:
        double pos_flow = 0;
        For_Gatetype(N, gate_PO, w){
            pos_flow += max_(0.0f, flow[w][0].rise);
            pos_flow += max_(0.0f, flow[w][0].fall);
        }

        double norm = total_flow / pos_flow;
        For_Gatetype(N, gate_PO, w){
            if (flow[w][0].rise > 0) in_flow(w[0]) += flow[w][0].rise * norm;
            if (flow[w][0].fall > 0) in_flow(w[0]) += flow[w][0].fall * norm;
        }

        // Distribute flow:
        for (uint i = order.size(); i > 0;){ i--;
            Wire w = order[i] + N;
            if (type(w) == gate_PO) continue;
            if (in_flow[w] == 0.0) continue;

            pos_flow = 0;
            for (uint pin = 0; pin < w.size(); pin++){
                pos_flow += max_(0.0f, flow[w][pin].rise);
                pos_flow += max_(0.0f, flow[w][pin].fall);
            }
            //**/WriteLn "inflow %_: %_  (pos %_)", w, in_flow[w], pos_flow;

            norm = in_flow[w] / pos_flow;
            for (uint pin = 0; pin < w.size(); pin++){
                if (flow[w][pin].rise > 0){
                    flow[w][pin].rise *= norm;
                    in_flow(w[pin]) += flow[w][pin].rise;
                }else
                    flow[w][pin].rise = 0;

                if (flow[w][pin].fall > 0){
                    flow[w][pin].fall *= norm;
                    in_flow(w[pin]) += flow[w][pin].fall;
                }else
                    flow[w][pin].fall = 0;
            }
        }

#if 0   /*DEBUG*/
{
        double sum = 0;
        For_Gatetype(N, gate_PI, w)
            sum += in_flow[w];
        Dump(sum);
}
#endif  /*END DEBUG*/


    #if 0   /*DEBUG*/
        For_Gates(N, w){
            for (uint i = 0; i < w.size(); i++){
                WriteLn "flow[%_][%_] = %_", w, i, flow[w][i];
            }
        }

        nameByCurrentId(N);
        N.write("N.gig"); WriteLn "Wrote: N.gig";

        For_Gatetype(N, gate_Uif, w)
            WriteLn "%_ : %_", w, L.cells[attr_Uif(w).sym].name;
    #endif  /*END DEBUG*/

        // Resize gates locally: (disregarding fanin side)
        for (uint i = order.size(); i > 0;){ i--;
            Wire w = order[i] + N;
            if (type(w) == gate_Uif)
                flowResizeGate(w, flow);
        }

        contStaticTiming();
        crit_len_cont = maxOf(arr);
        WriteLn "cont-delay: %_", L.ps(crit_len_cont);
    }


    // Final discretization:
    For_Gatetype(N, gate_Uif, w)
        setAltNo(w, (uint)floor(alt[w] + 0.5));

    // Static timing:
    load.clear(); arr.clear(); dep.clear(); slew.clear();
    computeLoads(N, L, wire_cap, load);
    staticTiming(N, L, load, order, P.approx, arr, slew);
    revStaticTiming(N, L, load, slew, order, P.approx, dep);
    area = getTotalArea(N, L);

    if (P.verbosity >= 1){
        NewLine;
        WriteLn "FINAL   delay \a/%.2f ps\a/   area \a/%,d\a/ ", L.ps(maxOf(arr)), (uint64)(area + 0.5);
        WriteLn "\a/_______________________________________________________________________________\a/";
    }
}


float DelayOpt::flowEvalGate(Wire w, FlowMap& flow)
{
    ContCell cc = contCell(w, alt[w]);
    double cost = 0;
    for (uint pin = 0; pin < w.size(); pin++){
        if (flow[w][pin].rise > 0 || flow[w][pin].fall > 0){
            TValues arr_in(0, 0);
            TValues arr_out, slew_out;
            contTimeGate(cc, /*out_pin*/0, pin, arr_in, slew[w[pin]], load[w/*pin here*/], P.approx, arr_out, slew_out);

            if (flow[w][pin].rise > 0) cost += /*sq*/(arr_out.rise * flow[w][pin].rise);
            if (flow[w][pin].fall > 0) cost += /*sq*/(arr_out.fall * flow[w][pin].fall);
        }
    }

#if 1
    Wire w0 = w;
    For_Inputs(w0, w){
        if (type(w) != gate_Uif) continue;

        contComputeGateLoad(w);
        ContCell cc = contCell(w, alt[w]);
        for (uint pin = 0; pin < w.size(); pin++){
            if (flow[w][pin].rise > 0 || flow[w][pin].fall > 0){
                TValues arr_in(0, 0);
                TValues arr_out, slew_out;
                contTimeGate(cc, /*out_pin*/0, pin, arr_in, slew[w[pin]], load[w/*pin here*/], P.approx, arr_out, slew_out);

                if (flow[w][pin].rise > 0) cost += /*sq*/(arr_out.rise * flow[w][pin].rise);
                if (flow[w][pin].fall > 0) cost += /*sq*/(arr_out.fall * flow[w][pin].fall);
            }
        }
    }
#endif
    cost += /*sq*/(contGateArea(cc));

    return cost;
}


void DelayOpt::flowResizeGate(Wire w, FlowMap& flow)
{
    contComputeGateLoad(w);

    float best = flowEvalGate(w, flow);
    float best_alt = alt[w];
    float delta = 0.01;

    while (alt[w] + delta <= maxAltNo(w)){      // <<== simlpe minded optimization; improve later!
        alt(w) = alt[w] + delta;
        float eval = flowEvalGate(w, flow);
        //**/WriteLn "+ alt=%_ best=%_ eval=%_", alt[w], best, eval;
        if (newMin(best, eval))
            best_alt = alt[w];
        else
            break;
    }

    while (alt[w] - delta >= 0){
        alt(w) = alt[w] - delta;
        float eval = flowEvalGate(w, flow);
        //**/WriteLn "- alt=%_ best=%_ eval=%_", alt[w], best, eval;
        if (newMin(best, eval))
            best_alt = alt[w];
        else
            break;
    }

    alt(w) = best_alt;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


void DelayOpt::run()
{
    Auto_Pob(N, dyn_fanouts);

    clearInternalNames();

    // Topological order and levelization:
    setupOrder();

    // Organize cells:
    groupCellTypes(L, groups);
    if (P.filter_groups)
        filterGroups(N, L, groups);
    computeGroupInvert(groups, group_inv);

    // Find smallest buffer:
    for (uint i = 0; i < groups.size(); i++){
        const SC_Cell& cell = L.cells[groups[i][0]];
        if (cell.n_inputs == 1 && cell.n_outputs == 1 && (cell.pins[1].func[0] & 3) == 2){
            buf_sym = groups[i][0];
            buf_grp = i;
            break; }
    }

    // Down-scale all cells:
    if (P.forget_sizes){
        For_Gatetype(N, gate_Uif, w)
            setAltNo(w, 0);
    }

    // Pre-buffer:
    preBuffer();

    // Compute initial loads:
    computeLoads(N, L, wire_cap, load);

    // Legalize:
    WriteLn "Initial delay  : %.2f ps   (area %,d)", L.ps(computeCritLen()), (uint64)getTotalArea(N, L);
    legalize();
    WriteLn "Legalized delay: %.2f ps   (area %,d)", L.ps(computeCritLen()), (uint64)getTotalArea(N, L);

    // Initial timing:
    staticTiming(N, L, load, order, P.approx, arr, slew);
    revStaticTiming(N, L, load, slew, order, P.approx, dep);
    area = getTotalArea(N, L);

    /**/signal(SIGINT, SIGINT_handler2);
    if (getenv("ALT"))
        alternativeResizing(P.req_time);
    else
        continuousResizing();

    // If using approximations, output table based evaluation for comparison:
    if (P.verbosity >= 1 && P.approx != 0){
        WriteLn "Table-based delay: \a/%.2f ps\a/", L.ps(computeCritLen(0)); }

    addInternalNames();
}


void optimizeDelay2(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, const Params_DelayOpt& P)
{
    DelayOpt dopt(N, L, wire_cap, P);
    dopt.run();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Options:


#define S(arg) (FMT "%_", arg)
#define B(arg) ((arg) ? "yes" : "no")


static
String select(String enum_alts, uint i)
{
    Vec<Str> alts;
    splitArray(enum_alts.slice(), "{} ,", alts);
    assert(i < alts.size());
    return String(alts[i]);
}


void addCli_DelayOpt(CLI& cli)
{
    // GLOBAL:
    Params_DelayOpt P;  // -- get default values.
    String approx_types   = "{none, lin, satch, quad}";
    String approx_default = select(approx_types, P.approx);

    cli.add("req"   , "ufloat"    , S(P.req_time)     , "Required time (used with 'alternative' engine only).");
    cli.add("apx"   , approx_types, approx_default    , "Approximation to use for timing; 'none' uses Liberty tables.");
    cli.add("forget", "bool"      , B(P.forget_sizes) , "Forget current gate sizes.");
    cli.add("filter", "bool"      , B(P.filter_groups), "Filter cell groups for smoother sizing.");
    cli.add("prebuf", "uint"      , S(P.prebuf_lo)    , "Pre-buffer fanouts larger than this before optimizing (0 or 1 = off).");
    cli.add("pgrace", "ufloat"    , S(P.prebuf_grace) , "Pre-buffer grace (in percent). Upper limit on prebuffering is 'prebuf * (1 + pgrace/100)'.");
    cli.add("v"     , "uint"      , S(P.verbosity)    , "Verbosity level; 0 = silent.");

    // CONTINUOUS SIZING:
    Params_ContSize C;  // -- get default values.
    String cost_fun_types   = "{max, softmax}";
    String cost_fun_default = select(cost_fun_types, (uint)C.cost_fun);
    String eval_fun_types   = "{linear, square, max, softmax}";
    String eval_fun_default = select(eval_fun_types, (uint)C.eval_fun);
    String move_types       = "{nudge, newton}";
    String move_default     = select(move_types, (uint)C.move_type);

    cli.add("cs-cost-fun"     , cost_fun_types, cost_fun_default   , "Global cost function used to determine progress (and when to quit).");
    cli.add("cs-crit-epsilon" , "float[0+:1]" , S(C.crit_epsilon)  , "Width of critical region as a fraction of the length of the critical path.");
    cli.add("cs-refine-orbits", "int[1:]"     , S(C.refine_orbits) , "Number of incremental refinement orbits between each global evalutaion.");
    cli.add("cs-eval-levels"  , "int[1:]"     , S(C.eval_levels)   , "Evaluate each candidate resizing by propagating its effect for this many levels.");
    cli.add("cs-eval-fun"     , eval_fun_types, eval_fun_default   , "Evaluation function to use for resizing operation.");
    cli.add("cs-eval-dx"      , "float[0+:]"  , S(C.eval_dx)       , "Delta used for approximating first order derivatives.");
    cli.add("cs-eval-ddx"     , "float[0+:]"  , S(C.eval_ddx)      , "Delta used for approximating second order derivatives (must be greater than 'dx').");
    cli.add("cs-step-init"    , "float[0+:]"  , S(C.step_init)     , "Initial gate resizing step-size (1 = one full gate).");
    cli.add("cs-step-quit"    , "float[0+:]"  , S(C.step_quit)     , "Stop algorithm when step-size goes below this level.");
    cli.add("cs-step-reduce"  , "float[0:1-]" , S(C.step_reduce)   , "Multiply step-size by this value when global cost function stops improving.");
    cli.add("cs-move-lim"     , "float[0+:1]" , S(C.move_lim)      , "Don't perform moves with estimated improvment less than this value (as a fraction of the best move; 1 = all moves).");
    cli.add("cs-move-type"    , move_types    , move_default       , "Type of resizing move: nudge=\"uniform step\", newton=\"newton-raphson\".");
    cli.add("cs-discr-freq"   , "uint"        , S(C.discr_freq)    , "When step-size is reduced, gate sizes are discretized every 'disrcr-freq' time.");
    cli.add("cs-discr-upbias" , "float[0:1]"  , S(C.discr_upbias)  , "Large number => prefer upsizing; small number => prefer downsizing (in discretization phase). 0.5 = no bias.");
}


void setParams(const CLI& cli, Params_DelayOpt& P)
{
    // GLOBAL:
    P.req_time      = cli.get("req").float_val;
    P.approx        = cli.get("apx").int_val;
    P.forget_sizes  = cli.get("forget").bool_val;
    P.filter_groups = cli.get("filter").bool_val;
    P.prebuf_lo     = cli.get("prebuf").int_val;
    P.prebuf_grace  = cli.get("pgrace").float_val;
    P.verbosity     = cli.get("v").int_val;

    P.prebuf_hi = uint(P.prebuf_lo * (1.0 + P.prebuf_grace/100.0));


    // CONTINUOUS SIZING:
    typedef Params_ContSize::CostFun CF;
    typedef Params_ContSize::EvalFun EF;
    typedef Params_ContSize::MoveTyp MT;

    P.C.cost_fun   =(CF)cli.get("cs-cost-fun").enum_val;
    P.C.crit_epsilon  = cli.get("cs-crit-epsilon").float_val;
    P.C.refine_orbits = cli.get("cs-refine-orbits").int_val;
    P.C.eval_levels   = cli.get("cs-eval-levels").int_val;
    P.C.eval_fun   =(EF)cli.get("cs-eval-fun").enum_val;
    P.C.eval_dx       = cli.get("cs-eval-dx").float_val;
    P.C.eval_ddx      = cli.get("cs-eval-ddx").float_val;
    P.C.step_init     = cli.get("cs-step-init").float_val;
    P.C.step_quit     = cli.get("cs-step-quit").float_val;
    P.C.step_reduce   = cli.get("cs-step-reduce").float_val;
    P.C.move_lim      = cli.get("cs-move-lim").float_val;
    P.C.move_type  =(MT)cli.get("cs-move-type").enum_val;
    P.C.discr_freq    = cli.get("cs-discr-freq").int_val;
    P.C.discr_upbias  = cli.get("cs-discr-upbias").float_val;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}

/*
_______________________________________________________________________________
                                                           Profile Timer Report
Estimated CPU speed: 1.99 GHz
Total run-time     : 683.01 s
Memory usage       : 2.02 GB

cont_eval_phase:        80.55 s  (11.79 %)
cont_resize_phase:     410.65 s  (60.12 %)
cont_discr_timing:       2.59 s  (0.38 %)
cont_static_timing:     13.86 s  (2.03 %)
cont_max_of:             8.79 s  (1.29 %)
cont_softmax_of:        40.82 s  (5.98 %)
cont_eval:               3.98 s  (0.58 %)
cont_inc_update:       409.60 s  (59.97 %)
_______________________________________________________________________________

approx=quad  =>  5998 ps   (cpu-time: 663 s)
approx=none  =>  5986 ps   (cpu-time: 747 s)


TODO:

  - snap-shot best discretized solution? best continuous?
  - improve discritizer (+ upsize only in critical region?)
  - improve print-outs (cost column, cpu time, periodical discr => extra output)
*/
