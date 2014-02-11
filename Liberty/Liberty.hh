//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Liberty.hh
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Liberty parser for static timing analysis.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__CellMap__Liberty_hh
#define ZZ__CellMap__Liberty_hh

#include "ZZ_BFunc.hh"
#include "ZZ/Generics/NamedSet.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parse functions:


struct SC_Lib;

enum TimingTablesMode { ttm_Keep, ttm_Merge, ttm_First };

void readLiberty(String filename, SC_Lib& result, TimingTablesMode timing_mode = ttm_Merge);
    // -- 'timing_mode' may tell parser to merge tables with "when" conditions.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Table approximation:


class SC_Surface;


// Coefficients: 1, S, L, S*L, S^2, L^2  (S=slew, L=load)
struct SC_ApxSurf {
    enum Type { LINEAR, SATCHEL, QUADRATIC };
    float coeff[3][6];      // -- 'coeff[0]' only uses 3 first values, 'coeff[1]' 4 first.

    SC_ApxSurf() { for (uint i = 0; i < 3; i++) for (uint j = 0; j < 6; j++) coeff[i][j] = 0.0f; }
    SC_ApxSurf(const SC_ApxSurf& other)            { memcpy(this, &other, sizeof(*this)); }
    SC_ApxSurf& operator=(const SC_ApxSurf& other) { memcpy(this, &other, sizeof(*this)); return *this; }

    float lookupL(float slew, float load) const { return coeff[0][0] + slew*coeff[0][1] + load*coeff[0][2]; }
    float lookupS(float slew, float load) const { return coeff[1][0] + slew*coeff[1][1] + load*coeff[1][2] + slew*load*coeff[1][3]; }
    float lookupQ(float slew, float load) const { return coeff[2][0] + slew*coeff[2][1] + load*coeff[2][2] + slew*load*coeff[2][3] + slew*slew*coeff[2][4] + load*load*coeff[2][5]; }
    float lookup (float slew, float load, Type apx) const { return coeff[apx][0] + slew*coeff[apx][1] + load*coeff[apx][2] + slew*load*coeff[apx][3] + slew*slew*coeff[apx][4] + load*load*coeff[apx][5]; }
        // -- lookup approximation of table value.

    float eval(const SC_Surface& s, Type apx);
        // -- evaluate quality of approximation (standard deviation)
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Standard Cell Data:


enum SC_Dir {
    sc_dir_NULL,
    sc_dir_Input,
    sc_dir_Output,
    sc_dir_InOut,
    sc_dir_Internal,
};


enum SC_TSense {    // -- timing sense, positive-, negative- or non-unate
    sc_ts_NULL,
    sc_ts_Pos,
    sc_ts_Neg,
    sc_ts_Non,
};


struct SC_WireLoad {
    Str    name;
    float  res;        // }- multiply estimation in 'fanout_len[].snd' with this value
    float  cap;        // }
    Vec<Pair<uint,float> > fanout_len; // -- pairs '(#fanouts, est-wire-len)'
};


struct SC_WireLoadSel {
    Str                         name;
    Vec<Trip<float,float,Str> > sel;  // -- triplets '(from-area, upto-area, wire-load-model)'; range is [from, upto[
};


struct SC_TableTempl {
    Str              name;
    Vec<Str>         var;       // -- name of variable (numbered from 0, not 1 as in the Liberty file)
    Vec<Vec<float> > index;     // -- this is the point of measurement in table for the given variable 

    lbool            transp;    // -- if 'l_True', 'var[]' and 'index[]' has size 2 and have been swapped
};


struct SC_Surface : NonCopyable {
    Str              templ_name;
    Vec<float>       index0;    // -- correspondes to "index_1" in the liberty file (for timing: slew)
    Vec<float>       index1;    // -- correspondes to "index_2" in the liberty file (for timing: load)
    Vec<Vec<float> > data;      // -- 'data[i0][i1]' gives value at '(index0[i0], index1[i1])' 
        // -- NOTE! After post-processing, 'index0' and 'index1' is always set by copying the
        // content pointed to by 'templ_name' (if necessary).

    // Post-processed:
    SC_ApxSurf       approx;    // -- fast way of computing approximate table value. NOTE! only initialized in static timing mode.

    void moveTo(SC_Surface& other) {
        mov(templ_name, other.templ_name);
        mov(index0    , other.index0);
        mov(index1    , other.index1);
        mov(data      , other.data);
        mov(approx    , other.approx);
    }
};


struct SC_Timing : NonCopyable {
    Str         related_pin;    // -- related pin
    SC_TSense   tsense;         // -- timing sense (positive_unate, negative_unate, non_unate)
    Str         when_text;      // -- logic condition on inputs triggering this delay model for the output

    SC_Surface  cell_rise;      // -- Used to compute pin-to-pin delay
    SC_Surface  cell_fall;
    SC_Surface  rise_trans;     // -- Used to compute output slew
    SC_Surface  fall_trans;

    void moveTo(SC_Timing& other) {
        mov(related_pin, other.related_pin);
        mov(tsense     , other.tsense);
        mov(when_text  , other.when_text);
        mov(cell_rise  , other.cell_rise);
        mov(cell_fall  , other.cell_fall);
        mov(rise_trans , other.rise_trans);
        mov(fall_trans , other.fall_trans);
    };
};


struct SC_Timings : Vec<SC_Timing> {
    Str name;   // -- the 'related_pin' field
};


struct SC_Pin {
    Str     name;

    SC_Dir  dir;
    float   cap;            // -- this value is used if 'rise_cap' and 'fall_cap' is missing (copied by 'postProcess()').
    float   rise_cap;       // }- used for input pins ('cap' too).
    float   fall_cap;       // }

    float   max_out_cap;    // }
    float   max_out_slew;   // }- used only for output pins (max values must not be exceeded or else mapping is illegal)
    Str     func_text;      // }
    BoolFun func;           // }

    Vec<SC_Timing> timing;          // -- for output pins

    // Post-processed:
    NamedSet<SC_Timings> rtiming;   // -- sorted on "related_pin" attribute (for output pins)

    SC_Pin() : dir(sc_dir_NULL), cap(0), rise_cap(0), fall_cap(0), max_out_cap(0), max_out_slew(-1) {}
};


struct SC_Cell {
    Str     name;

    bool    seq;            // -- set to TRUE by parser if a sequential element
    bool    unsupp;         // -- set to TRUE by parser if cell contains information we cannot handle

    float   area;
    uint    drive_strength; // -- some library files provide this field (currently unused, but may be a good hint for sizing)

    NamedSet<SC_Pin> pins;

    // Post-processed:
    uint    n_inputs;       // -- 'pins[0 .. n_inputs-1]' are input pins
    uint    n_outputs;      // -- 'pins[n_inputs .. n_inputs+n_outputs-1]' are output pins

    SC_Cell() : seq(false), unsupp(false), area(0), drive_strength(0), n_inputs(0), n_outputs(0) {}

    SC_Pin&       outPin(uint i)       { return pins[n_inputs + i]; }
    const SC_Pin& outPin(uint i) const { return pins[n_inputs + i]; }
};


struct SC_Lib {
    enum {
        NULL_GATE = 0,      // }- these are special, empty cells put in the beginning of 'cells' 
        PI_GATE   = 1,      // }
        N_RESERVED_GATES
    };

    Str     lib_name;

    Str     default_wire_load;
    Str     default_wire_load_sel;
    float   default_max_out_slew;   // -- 'default_max_transition'; this is copied to each output pin where 'max_transition' is not defined

    // Units:
    int              unit_time;     // -- Valid 9..12. Unit is '10^(-val)' seconds (e.g. 9=1ns, 10=100ps, 11=10ps, 12=1ps)
    Pair<float,int>  unit_cap;      // -- First part is a multiplier, second either 12 or 15 for 'pf' or 'ff'.

    // Wire capacity estimation:
    NamedSet<SC_WireLoad>    wire_load;
    NamedSet<SC_WireLoadSel> wire_load_sel;

    // Standard cells:
    NamedSet<SC_TableTempl>  templ;
    NamedSet<SC_Cell>        cells;

    // Constructor:
    SC_Lib() : default_max_out_slew(-1), unit_time(9), unit_cap(tuple(1,12)) {
        cells.add(slize("NULL_GATE")); cells.add(slize("PI_GATE")); cells[0].unsupp = cells[1].unsupp = true; }
   ~SC_Lib() { dispose(text); }
    Array<char> text;       // -- all 'Str's refer to substrings of this array

    // Gate access:
    SC_Cell&       operator[](uint i)       { return cells[i]; }
    const SC_Cell& operator[](uint i) const { return cells[i]; }
    uint size() const { return cells.size(); }

    // Formatting with units:
    double ff(double cap) const;      // }- Input is in units specified by Liberty file; output
    double ps(double time) const;     // }  in femto-farad or pico-seconds.
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


template<> fts_macro void write_(Out& out, const SC_WireLoad& v)
{
    FWriteLn(out) "WireLoad(%_) {\t+\t+\t+\t+", v.name;
    FWriteLn(out) "res = %_", v.res;
    FWriteLn(out) "cap = %_", v.cap;
    FWriteLn(out) "fanout_len = %_", v.fanout_len;
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_WireLoadSel& v)
{
    FWriteLn(out) "WireLoadSel(%_) {\t+\t+\t+\t+", v.name;
    FWriteLn(out) "%\r_", v.sel;
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_TableTempl& v)
{
    FWrite(out) "TableTempl(%_) {var=%_; index=%_}", v.name, v.var, v.index;
}


template<> fts_macro void write_(Out& out, const SC_Dir& v)
{
    switch (v){
    case sc_dir_NULL:     out += "<null>"; break;
    case sc_dir_Input:    out += "Input"; break;
    case sc_dir_Output:   out += "Output"; break;
    case sc_dir_InOut:    out += "InOut"; break;
    case sc_dir_Internal: out += "Internal"; break;
    default:              out += "<error>";
    }
}


template<> fts_macro void write_(Out& out, const SC_TSense& v)
{
    switch (v){
    case sc_ts_NULL: out += "<null>"; break;
    case sc_ts_Pos:  out += "Pos"; break;
    case sc_ts_Neg:  out += "Neg"; break;
    case sc_ts_Non:  out += "Non"; break;
    default:         out += "<error>";
    }
}


template<> fts_macro void write_(Out& out, const SC_Surface& v)
{
    FWriteLn(out) "Surface() {\t+\t+\t+\t+";
    FWriteLn(out) "templ_name = %_", v.templ_name;
    FWriteLn(out) "index0 = %_", v.index0;
    FWriteLn(out) "index1 = %_", v.index1;
    FWriteLn(out) "data = {\t+\t+\t+\t+";
    for (uind i = 0; i < v.data.size(); i++)
        FWriteLn(out) "%_", v.data[i];
    FWriteLn(out) "\t-\t-\t-\t-}";
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_Timing& v)
{
    FWriteLn(out) "Timing() {\t+\t+\t+\t+";
    FWriteLn(out) "related_pin = %_", v.related_pin;
    FWriteLn(out) "tsense = %_", v.tsense;
    FWriteLn(out) "when_text = %_", v.when_text;
    FWriteLn(out) "";
    FWriteLn(out) "cell_rise = %_", v.cell_rise;
    FWriteLn(out) "cell_fall = %_", v.cell_fall;
    FWriteLn(out) "rise_trans = %_", v.rise_trans;
    FWrite  (out) "fall_trans = %_", v.fall_trans;
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_Timings& v)
{
    FWriteLn(out) "TimingRelatedTo(%_) {\t+\t+\t+\t+", v.name;
    FWrite  (out) "%\r_", static_cast<const Vec<SC_Timing>&>(v);
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_Pin& v)
{
    FWriteLn(out) "Pin(%_) {\t+\t+\t+\t+", v.name;
    FWriteLn(out) "dir = %_", v.dir;
    FWriteLn(out) "cap = %_", v.cap;
    FWriteLn(out) "rise_cap = %_", v.rise_cap;
    FWriteLn(out) "fall_cap = %_", v.fall_cap;
    FWriteLn(out) "max_out_cap = %_", v.max_out_cap;
    FWriteLn(out) "max_out_slew = %_", v.max_out_slew;
    FWriteLn(out) "func_text = %_", v.func_text;
    FWriteLn(out) "";
    if (v.timing.size() > 0)
        FWrite(out) "%\r_", v.timing;
    else if (v.rtiming.size() > 0)
        FWrite(out) "%\r_", v.rtiming;
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_Cell& v)
{
    FWriteLn(out) "Cell(%_) {\t+\t+\t+\t+", v.name;
    FWriteLn(out) "seq = %_", v.seq;
    FWriteLn(out) "unsupp = %_", v.unsupp;
    FWriteLn(out) "area = %_", v.area;
    FWriteLn(out) "drive_strength = %_", v.drive_strength;
    FWrite  (out) "% _", v.pins;
    FWriteLn(out) "\t-\t-\t-\t-}";
}


template<> fts_macro void write_(Out& out, const SC_Lib& v)
{
    FWriteLn(out) "Library(%_) {\t+\t+\t+\t+", v.lib_name;
    FWriteLn(out) "default_wire_load = %_", v.default_wire_load;
    FWriteLn(out) "default_wire_load_sel = %_", v.default_wire_load_sel;
    FWriteLn(out) "";

    FWriteLn(out) "% _", v.wire_load.list();
    FWriteLn(out) "% _", v.wire_load_sel.list();

    FWriteLn(out) "%\n_", v.templ.list();
    FWrite  (out) "%\r_", v.cells.list();

    FWriteLn(out) "\t-\t-\t-\t-}";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
