//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TimingRef.hh
//| Author(s)   : Niklas Een
//| Module      : DelayOpt
//| Description : Static timing, reference implementation.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Not optimized for speed, only for correctness and readability.
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__TimingRef_hh
#define ZZ__Liberty__TimingRef_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Liberty.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Supporting types:


struct TValues {
    float rise;
    float fall;
    TValues(float rise_ = 0.0f, float fall_ = 0.0f) : rise(rise_), fall(fall_) {}

    bool operator==(const TValues& other) const { return rise == other.rise && fall == other.fall; }

    TValues& operator+=(const TValues& other) { rise += other.rise; fall += other.fall; return *this; }
    TValues& operator-=(const TValues& other) { rise -= other.rise; fall -= other.fall; return *this; }
    TValues operator+(const TValues& other) const { TValues ret = *this; ret += other; return ret; }
    TValues operator-(const TValues& other) const { TValues ret = *this; ret -= other; return ret; }
};


template<> fts_macro void write_(Out& out, const TValues& v)
{
    FWrite(out) "{rise=%_; fall=%_}", v.rise, v.fall;
}


typedef WMap<TValues> TMap;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


/*
'approx' takes the following values:

    0: no approximation
    1: linear approximation
    2: satchel approximation
    3: quadratic approximation
*/


void timeGate(const SC_Timing& t, TValues arr_in, TValues slew_in, TValues load, uint approx, /*in-outs*/TValues& arr, TValues& slew);
    // -- Time a single gate. 'approx == 0' for no approximation, 1-3 for LINEAR, SATCHEL, QUADRATIC.

void revTimeGate(const SC_Timing& t, TValues dep_out, TValues slew_in, TValues load, uint approx, /*in-out*/TValues& dep);
    // -- Reverse time a gate (calculate departure time).

float getTotalArea(NetlistRef N, const SC_Lib& L);
    // -- Sum up area of all standard cells in 'N'.

void getWireLoadModel(const SC_Lib& L, Str model, /*out*/Vec<float>& wire_cap);
bool getWireLoadModel(NetlistRef N, const SC_Lib& L, /*out*/Vec<float>& wire_cap, Str* model_chosen = NULL);
    // -- Returns a vector that maps "#fanouts -> wire capacitance". If no model is present in
    // the liberty file, FALSE is returned and 'model_chosen' set to "(no wire load model)".
    //
    // NOTE! May throw 'Excp_Msg'.

void computeLoads(NetlistRef N, const SC_Lib& L, const Vec<float>& wire_cap, /*out*/TMap& load);
    // -- Sum up the output capacitance ("load") for each gate.

void staticTiming(NetlistRef N, const SC_Lib& L, const TMap& load, const Vec<GLit>& order, uint approx, /*outputs:*/TMap& arr, TMap& slew);
    // -- Compute slew and arrival time for entire netlist.

void revStaticTiming(NetlistRef N, const SC_Lib& L, const TMap& load, const TMap& slew, const Vec<GLit>& order, uint approx, /*out*/TMap& dep);
    // -- Compute departure time. Assumes load (from 'computeLoads()') and slew (from 'staticTiming()') are already computed.

void dumpCriticalPath(NetlistRef N, const SC_Lib& L, const TMap& load, const TMap& arr, const TMap& slew);
    // -- Quick-and-dirty reporting of the critical path. For debugging mainly.

void reportTiming(NetlistRef N, const SC_Lib& L, uint approx, uint plot_slack = 0, String wire_load_model = "");
    // -- Time the whole design and show the critical path. If 'plot_slack == 1', curve for all
    // gates is plotted, if 'plot_slack == 2' only POs are used.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Low-level:


float fullLookup(const SC_Surface& S, float slew, float load);

macro float lookup(const SC_Surface& S, float slew, float load, uint approx) {
    return (approx != 0) ? S.approx.lookup(slew, load, SC_ApxSurf::Type(approx-1)) : fullLookup(S, slew, load); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Useful helpers:


macro const SC_Cell& cell(Wire w, const SC_Lib& L)
{
    assert(type(w) == gate_Uif);
    return L.cells[attr_Uif(w).sym];
}


macro bool isMultiOutput(Wire w, const SC_Lib& L)
{
    if (type(w) != gate_Uif) return false;
    return cell(w,L).n_outputs > 1;
}


macro bool isBuffer(Wire w, const SC_Lib& L)
{
    if (type(w) != gate_Uif) return false;
    return cell(w,L).n_inputs == 1 && cell(w,L).n_outputs == 1 && (cell(w,L).pins[1].func[0] & 3) == 2;
}


macro bool isInverter(Wire w, const SC_Lib& L)
{
    if (type(w) != gate_Uif) return false;
    return cell(w,L).n_inputs  == 1 && cell(w,L).n_outputs == 1 && (cell(w,L).pins[1].func[0] & 3) == 1;
}


macro bool isBufOrInv(Wire w, const SC_Lib& L)
{
    if (type(w) != gate_Uif) return false;
    return cell(w,L).n_inputs  == 1 && cell(w,L).n_outputs == 1;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
