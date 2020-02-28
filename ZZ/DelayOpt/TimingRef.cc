//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TimingRef.cc
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Static timing, reference implementation.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Not optimized for speed, only for correctness and readability.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "TimingRef.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Timing through table lookup:


float fullLookup(const SC_Surface& S, float slew, float load)
{
    // Find closest sample points in surface:
    uint s, l;
    for (s = 1; s < S.index0.size()-1; s++)
        if (S.index0[s] > slew)         // -- S.index0 = slew, S.index = load
            break;
    s--;

    for (l = 1; l < S.index1.size()-1; l++)
        if (S.index1[l] > load)
            break;
    l--;

    // Interpolate (or extrapolate) function value from sample points:
    float sfrac = (slew - S.index0[s]) / (S.index0[s+1] - S.index0[s]);
    float lfrac = (load - S.index1[l]) / (S.index1[l+1] - S.index1[l]);

    float p0 = S.data[s  ][l] + lfrac * (S.data[s  ][l+1] - S.data[s  ][l]);
    float p1 = S.data[s+1][l] + lfrac * (S.data[s+1][l+1] - S.data[s+1][l]);

    return p0 + sfrac * (p1 - p0);      // <<== multiply result with K factor here 
}


// First two timing arguments are on fanin side, last three on fanout side (and last two of those are 
// updated): arr[v], slew[v], load[w], arr(w), slew(w) (with v == w[k] for some pin# k).
//
void timeGate(const SC_Timing& t, TValues arr_in, TValues slew_in, TValues load, uint approx, TValues& arr, TValues& slew)
{
    // Time gate with separate max for "arrival" and "output slew":
    if (t.tsense == sc_ts_Pos || t.tsense == sc_ts_Non){
        newMax(arr .rise,  arr_in.rise + lookup(t.cell_rise , slew_in.rise, load.rise, approx));
        newMax(arr .fall,  arr_in.fall + lookup(t.cell_fall , slew_in.fall, load.fall, approx));
        newMax(slew.rise,  lookup(t.rise_trans, slew_in.rise, load.rise, approx));
        newMax(slew.fall,  lookup(t.fall_trans, slew_in.fall, load.fall, approx));
    }

    if (t.tsense == sc_ts_Neg || t.tsense == sc_ts_Non){
        newMax(arr .rise,  arr_in.fall + lookup(t.cell_rise , slew_in.fall, load.rise, approx));
        newMax(arr .fall,  arr_in.rise + lookup(t.cell_fall , slew_in.rise, load.fall, approx));
        newMax(slew.rise,  lookup(t.rise_trans, slew_in.fall, load.rise, approx));
        newMax(slew.fall,  lookup(t.fall_trans, slew_in.rise, load.fall, approx));
    }

    // NOTE! We can improve the timing model in the cases where positive unate and negative
    // unate "when" statements cover all cases. Rather than having a single "non-unate" 
    // table, we could have two separate tables for the two cases.
}


void revTimeGate(const SC_Timing& t, TValues dep_out, TValues slew_in, TValues load, uint approx, TValues& dep)
{
    if (t.tsense == sc_ts_Pos || t.tsense == sc_ts_Non){
        newMax(dep.rise, dep_out.rise + lookup(t.cell_rise, slew_in.rise, load.rise, approx));
        newMax(dep.fall, dep_out.fall + lookup(t.cell_fall, slew_in.fall, load.fall, approx));
    }

    if (t.tsense == sc_ts_Neg || t.tsense == sc_ts_Non){
        newMax(dep.fall, dep_out.rise + lookup(t.cell_rise , slew_in.fall, load.rise, approx));
        newMax(dep.rise, dep_out.fall + lookup(t.cell_fall , slew_in.rise, load.fall, approx));
    }
}


// 'w' is parent, 'v' is child. 'arr' and 'slew' are updated.
macro void timeGate(const SC_Timing& t, Wire w, Wire v, const TMap& load, uint approx, TMap& arr, TMap& slew) {
    return timeGate(t, arr[v], slew[v], load[w], approx, arr(w), slew(w)); }
    //return timeGate(t, arr[v], slew[v], load[w], approx && type(v) != gate_PI, arr(w), slew(w)); }



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// General support functions:


float getTotalArea(NetlistRef N, const SC_Lib& L)
{
    double area = 0;
    For_Gatetype(N, gate_Uif, w){
        uint cell_idx = attr_Uif(w).sym;
        const SC_Cell& cell = L.cells[cell_idx];

        area += cell.area;
    }

    return area;
}


// 'wire_cap[a]' and 'wire_cap[b]' are both defined, but all values in between are missing
// and will be interpolated.
static
void fillGap(Vec<float>& wire_cap, uint a, uint b)
{
    for (uint i = a+1; i < b; i++)
        wire_cap[i] = (wire_cap[a] * (b-i) + wire_cap[b] * (i-a)) / (b-a);
}


void getWireLoadModel(const SC_Lib& L, Str model, /*out*/Vec<float>& wire_cap)
{
    uind m = L.wire_load.idx(model);
    if (m == UIND_MAX)
        Throw(Excp_Msg) "No such wire load table: %_", model;

    const SC_WireLoad& wl = L.wire_load[m];
    for (uint i = 0; i < wl.fanout_len.size(); i++)
        wire_cap(wl.fanout_len[i].fst, -1) = wl.fanout_len[i].snd * wl.cap;

    // Fill in gaps (if any):
    if (wire_cap.size() == 0)
        Throw(Excp_Msg) "Empty wire load table: %_", model;

    if (wire_cap[0] == -1)
        wire_cap[0] = 0;

    for (uint i = 1; i < wire_cap.size(); i++){
        if (wire_cap[i] == -1){
            uint j;
            for (j = i+1;; j++){
                assert(j < wire_cap.size());
                if (wire_cap[j] != -1) break; }

            fillGap(wire_cap, i-1, j);
            i = j;
        }
    }
}


bool getWireLoadModel(NetlistRef N, const SC_Lib& L, /*out*/Vec<float>& wire_cap, Str* model_chosen)
{
    // Get name of wire load model to use:
    Str wire_load_name;

    if (L.default_wire_load_sel){
        uind n = L.wire_load_sel.idx(L.default_wire_load_sel);
        if (n == UIND_MAX)
            Throw(Excp_Msg) "No such wire load selection: %_", L.default_wire_load_sel;

        float area = getTotalArea(N, L);

        const Vec<Trip<float,float,Str> >& sel = L.wire_load_sel[n].sel;
        for (uint i = 0; i < sel.size(); i++){
            if (area >= sel[i].fst && area < sel[i].snd){
                wire_load_name = sel[i].trd;
                goto Found;
            }
        }
        wire_load_name = sel.last().trd;
      Found:;

    }else if (L.default_wire_load){
        wire_load_name = L.default_wire_load;

    }else{
        if (model_chosen != NULL)
            *model_chosen = slize("(no wire load model)");
        return false;
    }

    if (model_chosen != NULL)
        *model_chosen = wire_load_name;

    // Get the actual table and reformat it for 'wire_cap' output:
    getWireLoadModel(L, wire_load_name, wire_cap);

    return true;
}


// Computes capacitance ("load") on the output side for each gate. Output parameter 'load' should
// be default constructed and unmodifed.
void computeLoads(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, /*out*/TMap& load)
{
    For_Gatetype(N, gate_Uif, w){
        uint cell_idx = attr_Uif(w).sym;
        const SC_Cell& cell = L.cells[cell_idx];
        For_Inputs(w, v){
            const SC_Pin& pin = cell.pins[Iter_Var(v)];
            load(v).rise += pin.rise_cap;
            load(v).fall += pin.fall_cap;
        }
    }

    Auto_Pob(N, fanout_count);
    if (wire_cap.size() > 0){
        For_Gates(N, w){
            uint n = fanout_count[w];
            newMin(n, wire_cap.size() - 1);
            load(w).rise += wire_cap[n];
            load(w).fall += wire_cap[n];
        }
    }
}


// Output parameters 'arr' and 'slew' should be default constructed and unmodifed.
void staticTiming(NetlistRef N, const SC_Lib& L, const TMap& load, const Vec<GLit>& order, uint approx, /*outputs:*/TMap& arr, TMap& slew)
{
    for (uint i = 0; i < order.size(); i++){
        Wire w = order[i] + N;

        if (type(w) == gate_Uif){
            uint cell_idx = attr_Uif(w).sym;
            const SC_Cell& cell = L.cells[cell_idx];
            if (cell.n_outputs > 1) continue;

            const SC_Pin& pin = cell.pins[w.size()];    // = first output; may be many (handled by 'gate_Pin' below?)

            For_Inputs(w, v){
                const SC_Timings& ts = pin.rtiming[Iter_Var(v)];
                if (ts.size() == 0) continue;
                assert(ts.size() == 1);
                const SC_Timing& t = ts[0];

                timeGate(t, w, v, load, approx, arr, slew);
            }

        }else if (type(w) == gate_Pin){
            uint out_pin = attr_Pin(w).number;
            uint cell_idx = attr_Uif(w[0]).sym;
            const SC_Cell& cell = L.cells[cell_idx];
            const SC_Pin& pin = cell.pins[w[0].size() + out_pin];

            For_Inputs(w[0], v){
                const SC_Timings& ts = pin.rtiming[Iter_Var(v)];
                if (ts.size() == 0) continue;
                assert(ts.size() == 1);
                const SC_Timing& t = ts[0];

                timeGate(t, w, v, load, approx, arr, slew);
            }
            newMax(arr(w[0]).rise, arr[w].rise);
            newMax(arr(w[0]).fall, arr[w].fall);
        }
    }
}


// Must be run AFTER 'staticTiming' so that 'load' and 'slew' are computed.
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

            newMax(dep(w[0]).rise, dep[w].rise);
            newMax(dep(w[0]).fall, dep[w].fall);

            For_Inputs(w[0], v){
                const SC_Timings& ts = pin.rtiming[Iter_Var(v)];
                if (ts.size() == 0) continue;
                const SC_Timing& t = ts[0];
                revTimeGate(t, dep[w], slew[v], load[w], approx, dep(v));
            }
        }
    }
}


// This function should deal better with units. The '*1000' in print-outs are a temporary hack!
void dumpCriticalPath(NetlistRef N, const SC_Lib& L, const TMap& load, const TMap& arr, const TMap& slew)
{
    // Quick-and-dirty dump pf critical path:
    float max_arr = 0, crit_load = 0, crit_slew = 0;
    Wire w0 = Wire_NULL;
    bool rise = true;
    For_Gates(N, w){
        if (newMax(max_arr, arr[w].rise)) w0 = w, rise = true , crit_load = load[w].rise, crit_slew = slew[w].rise;
        if (newMax(max_arr, arr[w].fall)) w0 = w, rise = false, crit_load = load[w].fall, crit_slew = slew[w].fall;
    }
    NewLine;
    WriteLn "=== Maximal arrival time: \a/%.0f ps\a/", L.ps(max_arr);
    NewLine;

    WriteLn "=== Critical path:";
    while (type(w0) != gate_PI){
        String cell_name = "-";
        if (type(w0) == gate_Uif)
            cell_name = L.cells[attr_Uif(w0).sym].name;
        else if (type(w0) == gate_Pin && type(w0[0]) == gate_Uif)
            cell_name = L.cells[attr_Uif(w0[0]).sym].name;
        String edge_name = rise ? "rise" : "fall";

        if (type(w0) == gate_Pin){
            WriteLn "%n:%_ : %<10%_(%_)   --%>5%.0f ps  (load %.2f ff, slew %.1f ps)", w0[0], attr_Pin(w0).number, cell_name, edge_name, L.ps(max_arr), L.ff(crit_load), L.ps(crit_slew);
            w0 = w0[0];
        }else
            WriteLn "%n : %<10%_(%_)   --%>5%.0f ps  (load %.2f ff, slew %.1f ps)", w0, cell_name, edge_name, L.ps(max_arr), L.ff(crit_load), L.ps(crit_slew);

        if (w0.size() == 0) break;
        max_arr = 0;
        Wire w = w0[0];
        For_Inputs(w0, v){
            if (newMax(max_arr, arr[v].rise)) w = v, rise = true , crit_load = load[v].rise, crit_slew = slew[v].rise;
            if (newMax(max_arr, arr[v].fall)) w = v, rise = false, crit_load = load[v].fall, crit_slew = slew[v].fall;
        }
        w0 = w;
    }
    NewLine;
}


void reportTiming(NetlistRef N, const SC_Lib& L, uint approx, uint plot_slack, String wire_load_model)
{
    // Compute loads:
    Vec<float> wire_cap;
    Str model_chosen;
    try{
        if (wire_load_model == "")
            getWireLoadModel(N, L, wire_cap, &model_chosen);
        else
            getWireLoadModel(L, wire_load_model.slice(), wire_cap);
    }catch (Excp_Msg err){
        ShoutLn "ERROR! %_", err;
        exit(1);
    }
    WriteLn "Using wire load model   : \a*%_\a*", model_chosen;
    WriteLn "Sum of gate-sizes (area): \a*%,d\a*", (uint64)getTotalArea(N, L);

    TMap load;
    computeLoads(N, L, wire_cap, load);

    // Compute static timing:
    Vec<GLit> order;
    topoOrder(N, order);

    TMap arr;
    TMap slew;
    staticTiming(N, L, load, order, approx, arr, slew);

    // Show result:
    dumpCriticalPath(N, L, load, arr, slew);

    // Plot slack:
    if (plot_slack != 0){
        // Compute lengths:
        TMap dep;
        revStaticTiming(N, L, load, slew, order, approx, dep);

        Vec<float> ys;
        if ((plot_slack & 3) == 1){
            For_Gates(N, w)
                if (type(w) == gate_Pin || (type(w) == gate_Uif && !isMultiOutput(w, L)))
                    ys.push(L.ps(max_(arr[w].rise + dep[w].rise, arr[w].fall + dep[w].fall)));
        }else{
            For_Gatetype(N, gate_PO, w)
                ys.push(L.ps(max_(arr[w[0]].rise + dep[w[0]].rise, arr[w[0]].fall + dep[w[0]].fall)));
        }
        sort(ys);

        // Produce data file:
        String gnu_data;
        {
            int    fd = tmpFile("__plot_data", gnu_data);
            File   wr(fd, WRITE);
            Out    out(wr);

            for (uint i = 0; i < ys.size(); i++)
                FWriteLn(out) "%_", ys[i];
        }

        // Produce GNU plot commands:
        String gnu_cmds;
        {
            int    fd = tmpFile("__plot_cmds", gnu_cmds);
            File   wr(fd, WRITE);
            Out    out(wr);

            if (plot_slack & 4)
                FWriteLn(out) "set term dumb";
            FWriteLn(out) "set xlabel \"gate# (sorted on length)\"";
            FWriteLn(out) "set ylabel \"length (ps)\"";
            if (!(plot_slack & 4)){
                FWriteLn(out) "plot \"%_\" with lines title \"\", %_ title \"\"", gnu_data, ys.last();
                FWriteLn(out) "pause -1";
            }else{
                FWriteLn(out) "plot \"%_\" with lines title \"\"", gnu_data;
            }
        }

        // Call GNU-plot:
        String cmd;
        FWrite(cmd) "gnuplot %_", gnu_cmds;
        int ignore ___unused = system(cmd.c_str());
        ::remove(gnu_data.c_str());
        ::remove(gnu_cmds.c_str());
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
grab units from liberty?
    time_unit : "1ns" ;
    capacitive_load_unit (1,pf) ;


output load (Farad)  = capacitance on the fanout side
input slew  (seconds) = quality of the slew curve on the input pin (separated into rising/faling "slew dir")

-> from these we compute "cell delay" and "output slew"

There is a scaling in the computation called "K-factor" (set by user?). The same K is used for both
computations.

FrL (Load Fraction) = (L - L1)/(L2 - L1)
FrS (Slew Fraction) = (S - S1)/(S2 - S1)
D0 = (S1,L1) + FrL*((S1,L2)-(S1,L1))
D1 = (S2,L1) + FrL*((S2,L2)-(S2,L1))
Slew = K*(FrS*(D1 - D0) + D0)

FrL (Load Fraction) = (L - L1)/(L2 - L1)
FrS (Slew Fraction) = (S - S1)/(S2 - S1)
D0 = (S1,L1) + FrL*((S1,L2)-(S1,L1))
D1 = (S2,L1) + FrL*((S2,L2)-(S2,L1))
Cell Delay = K*(FrS*(D1 - D0) + D0)
*/
