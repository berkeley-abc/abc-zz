//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DelaySweep.cc
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Initial buffering and sizing using non-incremental sweeping algorithm.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "DelaySweep.hh"
#include "TimingRef.hh"
#include "OrgCells.hh"
#include "DelayOpt.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "ZZ/Generics/Heap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
slew_in is unknown => parameter in the answer (if linear approx?)
load depends on fanout gate sizes.
  - if single fanout, my output slew doesn't depend on my input slew (much), so
    find best size for output gate giving minimum departure time 
  - if multiple fanouts, do preliminary buffering
*/


#if 0
void revStaticTiming(NetlistRef N, const SC_Lib& L, const TMap& load, const TMap& slew, const Vec<GLit>& order, uint approx, /*out*/TMap& dep)
{
    for (uint i = order.size(); i > 0;){ i--;
        Wire w = order[i] + N;

        if (type(w) == gate_Uif && !isMultiOutput(w, L)){
            const SC_Cell& cell = L.cells[attr_Uif(w).sym];
            const SC_Pin& pin = cell.pins[w.size()];

            For_Inputs(w, v){
                const SC_Timings& ts = pin.rtiming[Iter_Var(v)]; assert(ts.size() == 1);
                const SC_Timing& t = ts[0];
                revTimeGate(t, dep[w], slew[v], load[w], approx, dep(v));
            }

        }else if (type(w) == gate_Pin){
            const SC_Cell& cell = L.cells[attr_Uif(w[0]).sym];
            const SC_Pin& pin = cell.pins[w[0].size() + attr_Pin(w).number];

            For_Inputs(w[0], v){
                const SC_Timings& ts = pin.rtiming[Iter_Var(v)];
                if (ts.size() == 0) continue;
                const SC_Timing& t = ts[0];
                revTimeGate(t, dep[w], slew[v], load[w], approx, dep(v));
            }
        }
    }
}
#endif


void extractBufferNetwork(Wire w0, const SC_Lib& L, const TMap& dep)
{
    NetlistRef N = netlist(w0);
    Get_Pob(N, fanouts);

    WZet Q;
    WZet region;
    Vec<Connect> fouts;

    region.add(w0);
    Q.add(w0);

    for (uint q = 0; q < Q.size(); q++){
        Wire w = Q.list()[q] + N;
        if (isMultiOutput(w, L)) continue;

        Fanouts fs = fanouts[w];
        for (uint i = 0; i < fs.size(); i++){
            Wire v = fs[i];
            bool is_new = !region.add(v);
            if (type(v) == gate_Uif && isBufOrInv(v, L))
                Q.add(v);
            else if (is_new)
                fouts.push(fs[i]);
        }
    }

    if (Q.size()-1 > 2){
        WriteLn "%n: buf-tree size (#buffers): %_", w0, Q.size() - 1;
        for (uint i = 0; i < fouts.size(); i++){
            Connect c = fouts[i];
            if (type(c) == gate_PO)
                Write "PO:0";
            else
                Write "%_:%_", L.cells[attr_Uif(c).sym].name, c.pin;

            Write "@%_/%_, ", dep[c[c.pin]].rise, dep[c[c.pin]].fall;
        }
        NewLine;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sizing and buffering through sweep:


//=================================================================================================
// -- Supporting types:


// Estimated departure time (and related data) for a possible gate-size choice.
struct DelSweep_Est {
    TValues dep;            // -- reverse arrival time (departure time) at the *output* of node
    TValues load;           // -- load seen on output of this gate for this choice
    uint    crit_outpin;    // -- which of my output pins is critical?
    uint    crit_alt;       // -- which size alternative (index into 'group[..][alt]') for my critical output should I pick?
    GLit    crit_fo;        // -- which gate is my critical output?

    DelSweep_Est(TValues dep_         = TValues(FLT_MAX, FLT_MAX),
                TValues  load_        = TValues(FLT_MAX, FLT_MAX),
                uint     crit_outpin_ = UINT_MAX,
                Connect  crit_fo_     = Connect(),
                uint     crit_alt_    = UINT_MAX)
        : dep(dep_), load(load_), crit_outpin(crit_outpin_), crit_alt(crit_alt_), crit_fo(crit_fo_) {}
};


template<> fts_macro void write_(Out& out, const DelSweep_Est& v)
{
    FWrite(out) "{dep=%_; load=%_; crit_outpin=%_; crit_fo=%n; crit_alt=%_}", v.dep, v.load, v.crit_outpin, v.crit_fo, v.crit_alt;
}


//=================================================================================================
// -- 'DelSweep' class:


struct LvQueue_lt {
    NetlistRef        N;
    const WMap<uint>& level;
    bool              flip;

    LvQueue_lt(NetlistRef N_, const WMap<uint>& level_) : N(N_), level(level_), flip(false) {}
    bool operator()(GLit x, GLit y) const { return bool(level[x + N] < level[y + N]) ^ flip; }
};


class DelSweep {
    typedef DelSweep_Est Est;
    typedef KeyHeap<GLit, false, LvQueue_lt> LvQueue;

    // Problem Input:
    NetlistRef        N;
    const SC_Lib&     L;
    const Vec<float>& wire_cap;
    uint              approx;

    // State:
    Vec<GLit>             order;        // -- Topological order
    Vec<Vec<uint> >       groups;       // -- Cells grouped on function they implement (internally sorted on size).
    Vec<Pair<uint,uint> > group_inv;    // -- Location of cell in 'groups': group_inv[cell_idx] == (group_no, alt_no)

    StackAlloc<TValues>   mem;          // -- Memory pool for 'est'
    WMap<Array<Est> >     est;          // -- Size choices and their estimated departure times.

    WMap<uchar>           fixed;        // -- 0=not fixed, 1=upper bound, 2=fixed 

    IdHeap<float,1>       ready;        // -- Contains nodes with at least one input fixed (after initial sweep).
    Vec<float>            len;          // -- Length of longest path gate is on (arr + dep): this is the priority vector for 'ready'.
    TMap                  arr;          // -- Arrival times for nodes in 'ready'.
    TMap                  slew;         // -- Approximation of output slew.
    float                 crit_len;     // -- Approximation of the length of the critical path

    WMap<uint>            level;        // }
    LvQueue_lt            lt;           // }
    LvQueue               Q;            // }- Incremental update.
    WSeen                 in_Q;         // }
    Wire                  w_fixed;      // }

    // Temporaries:
    Vec<Est>  tmp_cs;
    Vec<uint> tmp_alts;

    // Helpers:
    uint  grpNo   (Wire w) const     { return group_inv[attr_Uif(w).sym].fst; }
    uint  altNo   (Wire w) const     { return (type(w) != gate_Uif) ? 0 : group_inv[attr_Uif(w).sym].snd; }
    void  setAltNo(Wire w, uint alt) { if (type(w) != gate_Uif) assert(alt == 0); else attr_Uif(w).sym = groups[group_inv[attr_Uif(w).sym].fst][alt]; }
    uint  maxAltNo(Wire w) const     { return (type(w) != gate_Uif) ? 0 : groups[group_inv[attr_Uif(w).sym].fst].size() - 1; }
    Est&  currEst (Wire w)           { return est[w][altNo(w)]; }

    void  enqueue(Wire w) { if (!in_Q.add(w)) Q.add(w); }
    Wire  dequeue()       { for(;;){ if (Q.size() == 0) return Wire_NULL; Wire w = Q.pop() + N; in_Q.exclude(w); if (fixed[w]) return w; } }
        // -- enqueues any node but only dequeues nodes that are currently fixed (or Wire_NULL when no more such nodes)
    void  updateArrivals();
    void  updateDepartures();

    bool  tryAlt(Wire w, /*in/out*/DelSweep_Est& est);
    Est   findBestDep(Wire w, TValues req = TValues(0, 0), bool fix = false);
    bool  estimateGate(Wire w);
    void  fixGate(Wire w);
    void  fixCritPath(Wire w);

public:
    // Public methods:
    DelSweep(NetlistRef N_, const SC_Lib& L_, const Vec<float>& wire_cap_, uint approx_) :
        N(N_), L(L_), wire_cap(wire_cap_), approx(approx_), ready(len), crit_len(0), lt(N, level), Q(lt) {}

    void run();
};


// Stub.
void initialBufAndSize(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, uint approx)
{
    DelSweep B(N, L, wire_cap, approx);
    B.run();
}


//=================================================================================================
// -- Helpers:


static
float midPoint(const SC_Surface& S, float load, uint approx)
{
    uint s = S.index0.size() / 2;
    float slew = (S.index0.size() % 2 == 0) ? (S.index0[s-1] + S.index0[s]) / 2 : S.index0[s];

    return lookup(S, slew, load, approx);
}


// Will update 'est' and return TRUE if alternative is better than current 'est'.
bool DelSweep::tryAlt(Wire w, /*in/out*/DelSweep_Est& est)
{
    assert_debug(!isMultiOutput(w, L));

    Get_Pob(N, fanouts);
    Fanouts fs = fanouts[w];

    Wire w0     = (type(w) == gate_Pin) ? w[0] : w;
    uint outpin = (type(w) == gate_Pin) ? attr_Pin(w).number : 0;

    const SC_Pin* pin;
    if (type(w) == gate_PI)
        pin = NULL;
    else{
        const SC_Cell& cell = L.cells[attr_Uif(w0).sym];
        pin = &cell.pins[cell.n_inputs + outpin];
        assert(pin->dir == sc_dir_Output);
    }

    // Compute load (and accumulate selected alternatives for fanout gate-sizes):
    TValues load;
    for (uint i = 0; i < fs.size(); i++){
        Connect c = fs[i];
        if (type(c) == gate_PO) continue;
        assert(type(c) == gate_Uif);

        load.rise += L.cells[attr_Uif(c).sym].pins[c.pin].rise_cap;
        load.fall += L.cells[attr_Uif(c).sym].pins[c.pin].fall_cap;
    }
    // <<== add wire delay model here (from smallest possible size?)
    // <<== if load is too big for 'cell_idx' (spec. case POs), abort

    // Estimate slew between 'w' and its fanouts:
    TValues slew;       // -- initialized to (0, 0), which is the correct slew estimate for PIs 
    if (pin){
        for (uint i = 0; i < pin->rtiming.size(); i++){
            if (pin->rtiming[i].size() == 0) continue;
            assert(pin->rtiming[i].size() == 1);

            const SC_Timing& timing = pin->rtiming[i][0];
            if (type(w0[i]) == gate_PI){
                newMax(slew.rise, lookup(timing.rise_trans, 0.0f, load.rise, approx));
                newMax(slew.fall, lookup(timing.fall_trans, 0.0f, load.fall, approx));
            }else{
                newMax(slew.rise, midPoint(timing.rise_trans, load.rise, approx));
                newMax(slew.fall, midPoint(timing.fall_trans, load.fall, approx));
            }
        }
    }
    //**/slew.rise *= 1.6;
    //**/slew.fall *= 1.6;

    // Compute departure time:
    Connect crit_fo;
    uint    crit_alt = UINT_MAX;

    TValues dep;
    for (uint i = 0; i < fs.size(); i++){
        Connect c = fs[i];
        if (type(c) == gate_PO) continue;
        if (fixed[c] == 2) continue;         // -- ignore fanouts that are already fixed
        assert(type(c) == gate_Uif);

        const SC_Cell&    cell  = L.cells[attr_Uif(c).sym];
        const SC_Pin&     pin   = cell.pins[cell.n_inputs + currEst(c).crit_outpin];
        const SC_Timings& ts    = pin.rtiming[c.pin];
        if (ts.size() == 0) continue;
        assert(ts.size() == 1);

        TValues new_dep;
        revTimeGate(ts[0], currEst(c).dep, slew, currEst(c).load, approx, new_dep);
        if (max_(new_dep.rise, new_dep.fall) > max_(dep.rise, dep.fall)){
            dep = new_dep;
            crit_fo = c;
            crit_alt = altNo(c);
        }
    }

    // Update 'est' if better solution.
    if (max_(dep.rise, dep.fall) < max_(est.dep.rise, est.dep.fall)){
        est.dep         = dep;
        est.load        = load;
        est.crit_outpin = outpin;
        est.crit_alt    = crit_alt;
        est.crit_fo     = crit_fo;
        return true;
    }else
        return false;
}


// Try various gate-sizes for the fanouts of 'w' and return the estimated best departure time,
// or if 'req' is given, stop as soon as the departure time is smaller than 'req'.
DelSweep_Est DelSweep::findBestDep(Wire w, TValues req, bool fix)
{
    Get_Pob(N, fanouts);
    Fanouts fs = fanouts[w];

    // Set fanouts to smallest gate size:
    Vec<uint>& orig_alts = tmp_alts; orig_alts.clear();      // -- store a copy of current sizes
    for (uint i = 0; i < fs.size(); i++){
        Wire v = fs[i];
        orig_alts.push(altNo(v));
        if (fixed[v] != 2)
            setAltNo(v, 0);
    }

    // Upsize gates greedily:
    Est est;
    bool ok = tryAlt(w, est); assert(ok);
    //**/WriteLn "========================================beginning: %_=%n", w, w;
    //**/Dump(est);
    for(;;){
        Wire v = est.crit_fo + N;
        if (!v) break;

        uint alt = altNo(v);
        if( alt == maxAltNo(v)
//        ||  (fixed[v] == 1 && alt == orig_alts[find(fs, v)])        // <<== can be sped up
        ||  (est.dep.rise <= req.rise && est.dep.fall <= req.fall)
        ){
            setAltNo(v, est.crit_alt);
            break;
        }

        setAltNo(v, alt + 1);
        //**/Write "alt=%_  crit_fo=%n  ==>>  ", alt+1, v;
        tryAlt(w, est);     // <<== introduce early abort here?
        //**/Dump(est);
    }

    // Fix size of critical fanout of 'w'?
    if (fix){
        for (uint i = 0; i < fs.size(); i++){
            if (newMax(fixed(fs[i]), 1) || orig_alts[i] != altNo(fs[i])){
                enqueue(fs[i]);
                For_Inputs(fs[i], v)
                    enqueue(v);
            }
        }
        if (est.crit_fo){
            w_fixed = est.crit_fo + N;
            //**/WriteLn "\a/fixed:\a/ %n   (req=%_, cell=%_)", w_fixed, req, (type(w_fixed) == gate_Uif) ? L.cells[attr_Uif(w_fixed).sym].name : slize("<non-Uif>");
            fixed(w_fixed) = 2; }

    }else{
        for (uint i = 0; i < fs.size(); i++)
            setAltNo(fs[i], orig_alts[i]);
    }

    return est;
}


void DelSweep::updateArrivals()
{
    Get_Pob(N, fanouts);

    for(;;){
        Wire w = dequeue();
        if (!w) break;

        if (type(w) == gate_Uif){
            if (isMultiOutput(w, L)){
                Fanouts fs = fanouts[w];
                for (uint i = 0; i < fs.size(); i++){
                    fixed(fs[i]) = 2;
                    enqueue(fs[i]); }

            }else{
                Wire w0     = (type(w) == gate_Pin) ? w[0] : w;
                uint outpin = (type(w) == gate_Pin) ? attr_Pin(w).number : 0;

                // Compute load:
                TValues load;
                Fanouts fs = fanouts[w];
                for (uint i = 0; i < fs.size(); i++){
                    Connect c = fs[i];
                    if (type(c) != gate_Uif) continue;

                    if (fixed[c] == 0) setAltNo(c, 0);      // -- hack

                    const SC_Cell& cell = L.cells[attr_Uif(c).sym];
                    const SC_Pin&  pin  = cell.pins[c.pin];
                    load.rise += pin.rise_cap;
                    load.fall += pin.fall_cap;
                }

                // Time gate:
                const SC_Cell& cell = L.cells[attr_Uif(w0).sym];
                const SC_Pin&  pin  = cell.pins[cell.n_inputs + outpin];
                //**/WriteLn "cell=%_  pin=%_", cell.name, pin.name;

                TValues orig_arr  = arr [w0];
                TValues orig_slew = slew[w0];

                arr(w) = slew(w) = TValues();
                For_Inputs(w0, v){
                    if (fixed[v] != 2) continue;    // -- only compute arrival time from fixed gates
                    const SC_Timings& ts = pin.rtiming[Iter_Var(v)];
                    if (ts.size() == 0) continue;
                    assert(ts.size() == 1);
                    const SC_Timing& t = ts[0];

                    timeGate(t, arr[v], slew[v], load, approx, arr(w), slew(w));
                }

                if (w0 != w){
                    if (max_(arr [w].rise, arr [w].fall) > max_(arr [w0].rise, arr [w0].fall)) arr (w0) = arr [w];
                    if (max_(slew[w].rise, slew[w].fall) > max_(slew[w0].rise, slew[w0].fall)) slew(w0) = slew[w];
                }

                // Update critical path estimation:
                if (fixed[w0] == 2){
                    uint alt = altNo(w0);
                    len(id(w0), FLT_MAX) = max_(arr[w0].rise + est[w0][alt].dep.rise, arr[w0].fall + est[w0][alt].dep.fall);
                    newMax(crit_len, len[id(w0)]);
                    //**/Write "  -- w=%n  ", w0; Dump(crit_len, arr[w0], len[id(w0)]);

                    // Update ready queue:
                    if (ready.has(id(w0)))
                        ready.update(id(w0));
                    else
                        ready.add(id(w0));
                }

                // Propagate:
                if (arr[w0] != orig_arr || slew[w0] != orig_slew){
                    for (uint i = 0; i < fs.size(); i++)
                        enqueue(fs[i]);
                }
            }
        }
    }
}


void DelSweep::updateDepartures()
{
    assert(dequeue() == Wire_NULL);
    lt.flip = true;
    enqueue(w_fixed);

    for(;;){
        Wire w = dequeue();
        if (!w) break;

        if (estimateGate(w)){
            For_Inputs(w, v)
                enqueue(v);

            len(id(w), FLT_MAX) = max_(est[w][0].dep.rise, est[w][0].dep.fall);
            if (ready.has(id(w)))
                ready.update(id(w));
            else
                ready.add(id(w));
        }
    }
    lt.flip = false;
}


//=================================================================================================
// -- Main:


inline void DelSweep::fixGate(Wire w)
{
    TValues req(crit_len - arr[w].rise, crit_len - arr[w].fall);
    /**/req.rise *= 0.9;
    /**/req.fall *= 0.9;
    findBestDep(w, req, true);
    updateArrivals();
    updateDepartures();
}


// när klar, beräkna arrival för alla fixerade noder (icke-inkrementellt för stunden) och sortera fixerade noder (med icke-fixerade fanouts) på kritiskhet
void DelSweep::fixCritPath(Wire w)
{
    while (w){
        fixGate(w);
        /**/WriteLn "  %n (%_): %_", w, w, arr[w];
        w = est[w][altNo(w)].crit_fo + N;
    }

    // <<== update 'dep' estimates incrementally
}


// Returns TRUE if estimate for 'w' was changed.
bool DelSweep::estimateGate(Wire w)
{
    Get_Pob(N, fanouts);
    Vec<Est>& cs = tmp_cs;
    cs.clear();

    if (type(w) == gate_Uif){
        if (isMultiOutput(w, L)){
            // Multi-output gate -- merge results from output pins (lossy, but good approximation):
            Fanouts fs = fanouts[w]; assert(fs.size() > 0); // (should have at least one 'Pin' above it...)
            uint sz = est[fs[0]].size();

            for (uint k = 0; k < sz; k++){
                float max_dep = 0;
                uint best_i = 0;
                for (uint i = 0; i < fs.size(); i++){
                    Est e = est[fs[i]][k];
                    if (newMax(max_dep, e.dep.rise)) best_i = i;
                    if (newMax(max_dep, e.dep.fall)) best_i = i;
                }
                cs.push(est[fs[best_i]][k]);
            }

        }else{
            // Single-output gate:
            Vec<uint>& group = groups[group_inv[attr_Uif(w).sym].fst];
            for (uint j = 0; j < group.size(); j++){
                if (fixed[w] == 2 && j != altNo(w))
                    cs.push(Est(0,0));
                else{
                    attr_Uif(w) = group[j];
                    cs.push(findBestDep(w));
                }
            }
        }

    }else if (type(w) == gate_Pin){
        // One output of multi-output gate:
        Vec<uint>& group = groups[group_inv[attr_Uif(w[0]).sym].fst];
        for (uint j = 0; j < group.size(); j++){
            if (fixed[w] == 2 && j != altNo(w))
                cs.push(Est(0,0));
            else{
                attr_Uif(w[0]) = group[j];
                cs.push(findBestDep(w));
            }
        }

    }else if (type(w) == gate_PI){
        // Primary input -- just store once choice:
        cs.push(findBestDep(w));

    }else
        assert(type(w) == gate_PO);

    if (est[w]){
        assert(cs.size() == est[w].size());
        bool ret = false;
        for (uint i = 0; i < cs.size(); i++){
            if (est[w][i].dep != cs[i].dep)
                ret = true;
            est(w)[i] = cs[i];
        }
        return ret;

    }else{
        est(w) = Array_copy(cs, mem);
        return true;
    }
}


void DelSweep::run()
{
    // Pre buffer:
    uint buf_cell = UINT_MAX;
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
        if (cell.seq || cell.unsupp) continue;
        if (cell.n_inputs == 1 && cell.n_outputs == 1 && (cell.pins[1].func[0] & 3) == 2){
            buf_cell = i;
            break; }
    }

    if (buf_cell == UINT_MAX){
        ShoutLn "INTERNAL ERROR! No buffers found in library.";
        exit(1); }

    preBuffer(N, L, 10, buf_cell);

    // Levelize circuit:
    groupCellTypes(L, groups, &group_inv);

    Auto_Pob(N, fanouts);
    topoOrder(N, order);

    level.nil = 0;
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;
        For_Inputs(w, v)
            newMax(level(w), level[v] + 1);
    }

    // Initial sweep:
    for (uint i = order.size(); i > 0;) i--,
        estimateGate(order[i] + N);

    // Initialize ready queue with PIs:
    crit_len = 0;
    For_Gatetype(N, gate_PI, w){
        assert(est[w].size() == 1);
        len(id(w), FLT_MAX) = max_(est[w][0].dep.rise, est[w][0].dep.fall);
        newMax(crit_len, len[id(w)]);
        arr(w) = 0;
        fixed(w) = 2;
        ready.push(id(w));
    }
    ready.heapify();

    /**/WriteLn "crit_len=\a/%.2f ps\a/", L.ps(crit_len);

    // Fix sizes in order of decreasing criticality:
    while (ready.size() > 0){
        Wire w = ready.pop() + N;
        //fixCritPath(w);
        fixGate(w);

        /**/Write "\rCritical length: %.2f ps\f", L.ps(crit_len);
    }
    /**/NewLine;
    /**/WriteLn "Critical length: %.2f ps", L.ps(crit_len);

    /**/For_Gatetype(N, gate_Uif, w)
    /**/    if (fixed[w] != 2)
    /**/        WriteLn "UIF not fixed: %n (%_)", w, w;

    /**/NewLine;
    /**/WriteLn "CPU time: %t", cpuTime();
    /**/WriteLn "Mem used: %DB", memUsed();

    //**/nameByCurrentId(N);
    /**/N.write("N.gig"); WriteLn "Wrote: N.gig";

    /**/reportTiming(N, L, approx, false);

    //**/exit(0);

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
