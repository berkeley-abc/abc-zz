//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Liberty.cc
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Liberty parser for static timing analysis.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Liberty.hh"
#include "ZZ_LinReg.hh"
#include "BoolExpr.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
/*


========================================
LIBERTY SYNTAX:
========================================

Current guesses:

  - Logical line continues on next physical line if ending in '\'.
  - Comments are like in C (using '/' followed by '*' etc.).
  - Strings start and stop with double-quotes. No escaping used. There seem to be no distinction
    between strings and identifiers.


Four kind of statements:
  - group
  - simple_attr
  - complex_attr
  - define 

BNF for these:
  
    group ::= group_name '(' name, [names...] ')' '{'  statements '}'
        -- NOTE! The "name" and the opening "{" must be on the same line.
        
    simple_attr  ::= attribute_name ':' attribute_value ';'
        -- Example: function : "X + Y" ;

    complex_attr ::= attribute_name '(' parameter1 [, parameter2, parameter3 ...] ')' ';'
        -- Example: line (1, 2, 6, 8);

    define ::= 'define' '('attribute_name ',' group_name ',' attribute_type ')' ';'
        -- where attribute_type is one of: boolean, string, integer, float (or bool, int??)
        Defines a new attribute type.

File starts with group statement "library( <name> ) {".

       
*/
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// Returns first occurance of character 'c0' or 'c1', or UINT_MAX if neither occurs in 'text'.
// Strings are ignored ("..." with no backslash escaping in the ... part).
static
uint scanFor(const Str& text, char c0, char c1)
{
    assert(c0 != '"' && c1 != '"');

    bool ignore = false;
    for (uint i = 0; i < text.size(); i++){
        if (!ignore){
            if (text[i] == '"')
                ignore = true;
            else if (text[i] == c0 || text[i] == c1)
                return i;
        }else{
            if (text[i] == '"')
                ignore = false;
        }
    }
    return UINT_MAX;
}


macro uint scanFor(const Str& text, char c) {
    return scanFor(text, c, c); }


macro Str stripQuotes(Str text)
{
    trimStr(text);
    if (text && text.size() >= 2 && text[0] == '"' && text.last() == '"')
        return text.slice(1, text.size()-1);
    else
        return text;
}


static
void parseNumVector(Str args, Vec<float>& out_list, Vec<Str>& tmp)
{
    splitArray(stripQuotes(args), ",", tmp);
    forAll(tmp, trimStr);
    for (uint i = 0; i < tmp.size(); i++)
        out_list.push(stringToDouble(tmp[i]));
}


static
void parseNumArray(Str args, Vec<Vec<float> >& out_data, Vec<Str>& tmp)
{
    for(;;){
        uint pos = scanFor(args, ',');
        if (pos == UINT_MAX){
            out_data.push();
            parseNumVector(args, out_data.last(), tmp);
            return;
        }

        Str val = args.slice(0, pos);
        args = args.slice(pos+1);

        out_data.push();
        parseNumVector(val, out_data.last(), tmp);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Table approximation:


float SC_ApxSurf::eval(const SC_Surface& s, SC_ApxSurf::Type apx)
{
    float err = 0;
    for (uint i = 0; i < s.index0.size(); i++){         // -- slew
        for (uint j = 0; j < s.index1.size(); j++){     // -- load
            float ans = s.data[i][j];
            float val = lookup(s.index0[i], s.index1[j], apx);
            err += (ans - val) * (ans - val);
        }
    }
    return sqrt(err / (s.index0.size() * s.index1.size()));
}


static
bool findApprox(SC_Surface& S)
{
    SC_ApxSurf Z;

    // Linear regression:
    Vec<Vec<double> > data(7);     // -- 'data[var#][sample#]', with last "var" being the answer column
    for (uint i = 0; i < S.index0.size(); i++){
        for (uint j = 0; j < S.index1.size(); j++){
            data[0].push(1);
            data[1].push(S.index0[i]);
            data[2].push(S.index1[j]);
            data[3].push(S.index0[i] * S.index1[j]);
            data[4].push(S.index0[i] * S.index0[i]);
            data[5].push(S.index1[j] * S.index1[j]);
            data[6].push(S.data[i][j]);
        }
    }

    // Quadratic:
    Vec<double> coeff;
    bool stable = linearRegression(data, coeff);
    for (uint i = 0; i < 6; i++) S.approx.coeff[SC_ApxSurf::QUADRATIC][i] = coeff[i];

    // Satchel:
    data[6].moveTo(data[4]);
    data.shrinkTo(5);
    stable &= linearRegression(data, coeff);
    for (uint i = 0; i < 4; i++) S.approx.coeff[SC_ApxSurf::SATCHEL][i] = coeff[i];

    // Linear:
    data[4].moveTo(data[3]);
    data.shrinkTo(4);
    stable &= linearRegression(data, coeff);
    for (uint i = 0; i < 3; i++) S.approx.coeff[SC_ApxSurf::LINEAR][i] = coeff[i];

    return stable;
}


static
void approxCellTables(SC_Lib& L)
{
    String title;

    for (uint i = 0; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        if (cell.unsupp || cell.seq) continue;

        for (uint j = 0; j < cell.n_outputs; j++){
            SC_Pin& pin = cell.pins[cell.n_inputs + j];

            assert(pin.rtiming.size() == cell.n_inputs);
            for (uint k = 0; k < cell.n_inputs; k++){
                if (pin.rtiming[k].size() == 0) continue;
                assert(pin.rtiming[k].size() == 1);

                SC_Timing& timing = pin.rtiming[k][0];

                title.clear();
                FWrite(title) "%_:%_->%_", cell.name, pin.name, pin.rtiming[k].name;

                if (!findApprox(timing.cell_rise)) ShoutLn "WARNING! Failed to approximate table data: %_ cell rise", title;
                if (!findApprox(timing.cell_fall)) ShoutLn "WARNING! Failed to approximate table data: %_ cell fall", title;
                if (!findApprox(timing.rise_trans)) ShoutLn "WARNING! Failed to approximate table data: %_ rise transition", title;
                if (!findApprox(timing.fall_trans)) ShoutLn "WARNING! Failed to approximate table data: %_ fall transition", title;
            }
        }
    }
}




//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'LibertyListener' :


//#define LIBERTY_DEBUG


//=================================================================================================
// -- Listener:


// This listener digs out the actual information we want from the file. It is served the
// content in a convenient way.
class LibertyListener {
    enum Scope {
        S_NULL,
        S_ROOT,
        S_IGNORE,
        S_wire_load,
        S_wire_load_selection,
        S_lu_table_template,
        S_cell,
        S_cell___pin,
        S_cell___pin___timing,
        S_cell___pin___timing___cell_rise,
        S_cell___pin___timing___cell_fall,
        S_cell___pin___timing___rise_transition,
        S_cell___pin___timing___fall_transition,
    };

    Vec<Scope>  scope;
    SC_Lib&     lib;

    Vec<Str>    tmp;

public:
    LibertyListener(SC_Lib& lib_);

    void group(Str name, Str args);
    void endGroup();
    void simpleAttr(Str name, Str val);
    void complexAttr(Str name, Str args);
    void end();
};


LibertyListener::LibertyListener(SC_Lib& lib_) :
    lib(lib_)
{
    scope.push(S_NULL);
}


void LibertyListener::group(Str name, Str args)
{
    if (scope.size() == 1){
        if (!eq(name, "library")) throw String("File must start with 'library( <name> ) {'");
        scope.push(S_ROOT);
        lib.lib_name = args;

    }else{
        switch (scope.last()){
        case S_ROOT:
            if (eq(name, "wire_load")){
                scope.push(S_wire_load);
                if (!lib.wire_load.add(stripQuotes(args)))
                    throw String((FMT "Wire load declared twice: %_", args));

            }else if (eq(name, "wire_load_selection")){
                scope.push(S_wire_load_selection);
                if (!lib.wire_load_sel.add(stripQuotes(args)))
                    throw String((FMT "Wire load selection declared twice: %_", args));

            }else if (eq(name, "lu_table_template")){
                scope.push(S_lu_table_template);
                if (!lib.templ.add(stripQuotes(args)))
                    throw String((FMT "Lookup table template declared twice: %_", args));

            }else if (eq(name, "cell")){
                scope.push(S_cell);
                if (!lib.cells.add(stripQuotes(args)))
                    throw String((FMT "Cell declared twice: %_", args));

            }else
                scope.push(S_IGNORE);
            break;

        case S_cell:
            if (eq(name, "pin")){
                scope.push(S_cell___pin);
                if (!lib.cells.last().pins.add(stripQuotes(args)))
                    throw String((FMT "Cell pin declared twice: %_", args));

            }else if (eq(name, "ff")){
                SC_Cell& cell = lib.cells.last();
                cell.seq = true;
                scope.push(S_IGNORE);

            }else
                scope.push(S_IGNORE);
            break;

        case S_cell___pin:
            if (eq(name, "timing")){
                scope.push(S_cell___pin___timing);
                lib.cells.last().pins.last().timing.push();
            }else
                scope.push(S_IGNORE);
            break;

        case S_cell___pin___timing:{
            SC_Timing& timing = lib.cells.last().pins.last().timing.last();
            if (eq(name, "cell_rise")){
                scope.push(S_cell___pin___timing___cell_rise);
                timing.cell_rise.templ_name = stripQuotes(args);
            }else if (eq(name, "cell_fall")){
                scope.push(S_cell___pin___timing___cell_fall);
                timing.cell_fall.templ_name = stripQuotes(args);
            }else if (eq(name, "rise_transition")){
                scope.push(S_cell___pin___timing___rise_transition);
                timing.rise_trans.templ_name = stripQuotes(args);
            }else if (eq(name, "fall_transition")){
                scope.push(S_cell___pin___timing___fall_transition);
                timing.fall_trans.templ_name = stripQuotes(args);
            }else
                scope.push(S_IGNORE);
            break;}

        default:
            scope.push(S_IGNORE);
        }
    }

  #if defined(LIBERTY_DEBUG)
    if (scope.last() != S_IGNORE)
        WriteLn "%_(%_) {\t+\t+", name, args;
  #endif
}


void LibertyListener::endGroup()
{
    if (scope.size() == 1) throw String("Extra '}' not matching any '{'.");

  #if defined(LIBERTY_DEBUG)
    static cchar* name[] = {
        "ROOT",
        "IGNORE",
        "wire_load",
        "wire_load_selection",
        "lu_table_template",
        "cell",
        "cell/pin",
        "cell/pin/timing",
        "cell/pin/timing/cell_rise",
        "cell/pin/timing/cell_fall",
        "cell/pin/timing/rise_transition",
        "cell/pin/timing/fall_transition",
    };

    if (scope.last() != S_IGNORE)
        WriteLn "\t-\t-}    // -- %_", name[scope.last()];
  #endif

    scope.pop();
}


void LibertyListener::simpleAttr(Str name, Str val)
{
  #if defined(LIBERTY_DEBUG)
    if (scope.last() != S_IGNORE)
        WriteLn "%_ : %_", name, val;
  #endif

    if (scope.last() == S_ROOT){
        if (eq(name, "default_wire_load"))
            lib.default_wire_load = stripQuotes(val);
        else if (eq(name, "default_wire_load_selection"))
            lib.default_wire_load_sel = stripQuotes(val);
        else if (eq(name, "time_unit")){
            val = stripQuotes(val);
            if      (eq(val, "1ns"  )) lib.unit_time = 9;
            else if (eq(val, "100ps")) lib.unit_time = 10;
            else if (eq(val, "10ps" )) lib.unit_time = 11;
            else if (eq(val, "1ps"  )) lib.unit_time = 12;
            else
                throw String("'time_unit' must be one of:  1ps, 10ps, 100ps, 1ns");
        }else if (eq(name, "default_max_transition"))
            lib.default_max_out_slew = stringToDouble(val);

    }else if (scope.last() == S_wire_load){
        if (eq(name, "resistance"))
            lib.wire_load.last().res = stringToDouble(val);
        else if (eq(name, "capacitance"))
            lib.wire_load.last().cap = stringToDouble(val);

    }else if (scope.last() == S_lu_table_template){
        if (pfx(name, "variable_")){
            uint n = (uint)stringToUInt64(name.slice(9), 0, UINT_MAX); assert(n > 0);
            n--;
            lib.templ.last().var(n, Str_NULL) = val;
        }

    }else if (scope.last() == S_cell){
        if (eq(name, "area"))
            lib.cells.last().area = stringToDouble(val);
        else if (eq(name, "drive_strength"))
            lib.cells.last().drive_strength = (uint)stringToUInt64(val, 0, UINT_MAX);

    }else if (scope.last() == S_cell___pin){
        SC_Cell& cell = lib.cells.last();
        SC_Pin&  pin  = cell.pins.last();

        if (eq(name, "state_function")){
            cell.seq = true;

        }else if (eq(name, "direction")){
            if (eq(val, "input"))
                pin.dir = sc_dir_Input;
            else if (eq(val, "output"))
                pin.dir = sc_dir_Output;
            else if (eq(val, "inout")){
                pin.dir = sc_dir_InOut;
                cell.unsupp = true;     // -- don't know how to deal with these pins
            }else if (eq(val, "internal")){
                pin.dir = sc_dir_Internal;
                cell.unsupp = true;     // -- don't know how to deal with these pins
            }else
                throw String((FMT "Unsupported pin direction: %_", val));

        }else if (eq(name, "capacitance")){
            pin.cap = stringToDouble(val);

        }else if (eq(name, "rise_capacitance")){
            pin.rise_cap = stringToDouble(val);

        }else if (eq(name, "fall_capacitance")){
            pin.fall_cap = stringToDouble(val);

        }else if (eq(name, "max_capacitance")){
            pin.max_out_cap = stringToDouble(val);

        }else if (eq(name, "max_transition")){
            pin.max_out_slew = stringToDouble(val);

        }else if (eq(name, "function")){
            pin.func_text = stripQuotes(val);
        }

    }else if (scope.last() == S_cell___pin___timing){
        SC_Timing& timing = lib.cells.last().pins.last().timing.last();

        if (eq(name, "related_pin")){
            if (timing.related_pin) throw String((FMT "Only one 'related_pin' can be specified: %_ vs. %_", timing.related_pin, stripQuotes(val)));
            timing.related_pin = stripQuotes(val);

        }else if (eq(name, "timing_sense")){
            if (eq(val, "positive_unate"))
                timing.tsense = sc_ts_Pos;
            else if (eq(val, "negative_unate"))
                timing.tsense = sc_ts_Neg;
            else if (eq(val, "non_unate"))
                timing.tsense = sc_ts_Non;
            else
                throw String((FMT "Unsupported timing sense: %_", val));

        }else if (eq(name, "when")){
            timing.when_text = stripQuotes(val);

        }else if (eq(name, "timing_type")){
            if (!eq(stripQuotes(val), "combinational"))
                lib.cells.last().unsupp = true;      // -- we only support "combinatinal" timing types
        }
    }
}


void LibertyListener::complexAttr(Str name, Str args)
{
  #if defined(LIBERTY_DEBUG)
    if (scope.last() != S_IGNORE)
        WriteLn "%_(%_)", name, args;
  #endif

    if (scope.last() == S_ROOT){
        if (eq(name, "capacitive_load_unit")){
            splitArray(stripQuotes(args), ",", tmp);
            forAll(tmp, trimStr);
            if (tmp.size() != 2) throw String("'capacitive_load_unit' should have two arguments, e.g.: (1,pf)");

            lib.unit_cap.fst = stringToDouble(tmp[0]);
            if      (eq(tmp[1], "ff")) lib.unit_cap.snd = 15;
            else if (eq(tmp[1], "pf")) lib.unit_cap.snd = 12;
            else
                throw String("'capacitive_load_unit' second component must be one of: pf, ff");
        }

    }else if (scope.last() == S_wire_load){
        if (eq(name, "fanout_length")){
            splitArray(stripQuotes(args), ",", tmp);
            forAll(tmp, trimStr);
            if (tmp.size() != 2) throw String("'fanout_length' should have two arguments.");

            uint   n_fanouts = (uint)stringToUInt64(tmp[0], 0, UINT_MAX);
            double wire_size = stringToDouble(tmp[1]);
            lib.wire_load.last().fanout_len.push(tuple(n_fanouts, wire_size));
        }

    }else if (scope.last() == S_wire_load_selection){
        if (eq(name, "wire_load_from_area")){
            splitArray(stripQuotes(args), ",", tmp);
            forAll(tmp, trimStr);
            if (tmp.size() != 3) throw String("'wire_load_from_area' should have three arguments.");

            double from = stringToDouble(tmp[0]);
            double upto = stringToDouble(tmp[1]);
            Str    sel  = stripQuotes(tmp[2]);
            lib.wire_load_sel.last().sel.push(tuple(from, upto, sel));
        }

    }else if (scope.last() == S_lu_table_template){
        if (pfx(name, "index_")){
            uint n = (uint)stringToUInt64(name.slice(6), 0, UINT_MAX); assert(n > 0);
            n--;
            Vec<float>& list = lib.templ.last().index(n);
            parseNumVector(args, list, tmp);
        }

    }else if (scope.last() == S_cell){
        if (eq(name, "statetable"))
            lib.cells.last().seq = true;

    }else if (scope.last() == S_cell___pin___timing___cell_rise){
        SC_Surface& surface = lib.cells.last().pins.last().timing.last().cell_rise;
        if (eq(name, "index_1"))
            parseNumVector(args, surface.index0, tmp);
        else if (eq(name, "index_2"))
            parseNumVector(args, surface.index1, tmp);
        else if (eq(name, "values"))
            parseNumArray(args, surface.data, tmp);

    }else if (scope.last() == S_cell___pin___timing___cell_fall){
        SC_Surface& surface = lib.cells.last().pins.last().timing.last().cell_fall;
        if (eq(name, "index_1"))
            parseNumVector(args, surface.index0, tmp);
        else if (eq(name, "index_2"))
            parseNumVector(args, surface.index1, tmp);
        else if (eq(name, "values"))
            parseNumArray(args, surface.data, tmp);

    }else if (scope.last() == S_cell___pin___timing___rise_transition){
        SC_Surface& surface = lib.cells.last().pins.last().timing.last().rise_trans;
        if (eq(name, "index_1"))
            parseNumVector(args, surface.index0, tmp);
        else if (eq(name, "index_2"))
            parseNumVector(args, surface.index1, tmp);
        else if (eq(name, "values"))
            parseNumArray(args, surface.data, tmp);

    }else if (scope.last() == S_cell___pin___timing___fall_transition){
        SC_Surface& surface = lib.cells.last().pins.last().timing.last().fall_trans;
        if (eq(name, "index_1"))
            parseNumVector(args, surface.index0, tmp);
        else if (eq(name, "index_2"))
            parseNumVector(args, surface.index1, tmp);
        else if (eq(name, "values"))
            parseNumArray(args, surface.data, tmp);
    }
}


void LibertyListener::end()
{
    if (scope.size() != 1) throw String((FMT "Missing '}'; %_ scope(s) not closed.", scope.size()));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Liberty Parser:


//=================================================================================================
// -- Class 'LibertyParser':


class LibertyParser {
    Array<char> text;   // Contains text after preprocessing (comments and backslashes replaced by space)
    Vec<Str>    lines;  // Contains text separated into logical lines

    uint getLocation(cchar* pos) const;
    void throwIllegalChar(cchar* p) const;
    void preprocess(String filename);
    void parse(LibertyListener& lis);
    void postProcess(SC_Lib& lib, TimingTablesMode timing_mode);

public:
    LibertyParser() {}

    void read(String filename, SC_Lib& lib, TimingTablesMode timing_mode);
};


void LibertyParser::read(String filename, SC_Lib& lib, TimingTablesMode timing_mode)
{
    preprocess(filename);

    LibertyListener lis(lib);
    parse(lis);
    postProcess(lib, timing_mode);
    lib.text = text;    // -- 'lib' takes ownership of this string; will be freed by its destructor
}


//=================================================================================================
// -- Post-process parsed data:


static
void adjustIndices(SC_Surface& s, NamedSet<SC_TableTempl>& templ)
{
    uint i = templ.idx(s.templ_name);
    if (i == UINT_MAX) throw String(("Reference to non-existing 'lu_table_template': %_", s.templ_name));
    if (templ[i].index.size() != 2) throw String(("'lu_table_template' has wrong dimensions (should be 2): %_", s.templ_name));

    // Order of variables should be: input_net_transition, total_output_net_capacitance
    // If swapped, transpose table:
    if (templ[i].transp == l_Undef){
        if (eq(templ[i].var[0], "total_output_net_capacitance") && eq(templ[i].var[1], "input_net_transition")){
            swp(templ[i].var  [0], templ[i].var  [1]);
            swp(templ[i].index[0], templ[i].index[1]);
            templ[i].transp = l_True;
        }else
            templ[i].transp = l_False;
    }

    if (templ[i].transp == l_True)
        swp(s.index0, s.index1);

    if (s.index0.size() == 0)
        templ[i].index[0].copyTo(s.index0);

    if (s.index1.size() == 0)
        templ[i].index[1].copyTo(s.index1);

    if (templ[i].transp == l_True){
        // Transpose table:
        Vec<Vec<float> > data(s.data[0].size());
        for (uint i = 0; i < data.size(); i++){
            data[i].growTo(s.data.size());
            for (uint j = 0; j < data[i].size(); j++)
                data[i][j] = s.data[j][i];
        }
        swp(s.data, data);
    }

    if (s.data.size() != s.index0.size()) throw String("Rows in table does not match dimension of 'index_1'.");
    if (s.data[0].size() != s.index1.size()) throw String("Columns in table does not match dimension of 'index_2'.");
}


template<class STR, class T>
macro uint pinIndex(const STR& name, const NamedSet<T>& list)
{
    for (uint k = 0; k < list.size(); k++)
        if (eq(name, list[k].name))
            return k;
    return UINT_MAX;
}


// Order the pins in the library 'L' with inputs first, then outputs (otherwise same order they
// appear in the liberty file).
static
void sortPins(SC_Lib& L)
{
    for (uint i = 0; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        if (cell.unsupp) continue;

        uint pinC = 0;
        for (uint j = 0; j < cell.pins.size(); j++){
            if (cell.pins[j].dir == sc_dir_Input){
                for (uint k = j; k > pinC; k--)
                    swp_mem(cell.pins[k], cell.pins[k-1]);
                pinC++;
            }
        }
        cell.n_inputs = pinC;

        for (uint j = pinC; j < cell.pins.size(); j++){
            if (cell.pins[j].dir == sc_dir_Output){
                for (uint k = j; k > pinC; k--)
                    swp_mem(cell.pins[k], cell.pins[k-1]);
                pinC++;
            }
        }
        cell.n_outputs = pinC - cell.n_inputs;

        assert(pinC == cell.pins.size());   // -- otherwise "unsupp" should be set
        cell.pins.rehash();

        if (cell.n_outputs == 0)    // -- mark gates without outputs as unsupported
            cell.unsupp = true;
    }
}


static
void mergeSurface(SC_Surface& a, const SC_Surface& b)
{
    if (!vecEqual(a.index0, b.index0) || !vecEqual(a.index1, b.index1)){
        ShoutLn "ERROR! Liberty file contains related tables using different indices.";
        exit(1); }

    uint rows = a.index0.size();
    uint cols = a.index1.size();

    assert(a.data.size() == rows);
    assert(b.data.size() == rows);
    for (uint i = 0; i < rows; i++){
        assert(a.data[i].size() == cols);
        assert(b.data[i].size() == cols);
        for (uint j = 0; j < cols; j++)
            newMax(a.data[i][j], b.data[i][j]);
    }
}


static
void mergeTiming(SC_Timing& a, const SC_Timing& b)
{
    if (a.tsense != b.tsense || b.tsense == sc_ts_Non)
        a.tsense = sc_ts_Non;

    mergeSurface(a.cell_rise , b.cell_rise);
    mergeSurface(a.cell_fall , b.cell_fall);
    mergeSurface(a.rise_trans, b.rise_trans);
    mergeSurface(a.fall_trans, b.fall_trans);
}


// If 'timing_mode' is set to "merge", all tables for the same input/output pair (with different
// "when" conditions) are merged into a worst-case table. In this case, the list of tables
// 'cell.pins[output_pin].rtiming[input_pin]' is always of size 0 or 1. NOTE! No tables mean
// the output pin doesn't depend on the input pin (e.g. in a multi-output cell or a constant cell).
static
void normalizeTables(SC_Lib& L, TimingTablesMode timing_mode)
{
    // Make sure 'index0' and 'index1' is set for each timing table, plus organize the timing
    // tables in groups (each group containg all the tables for one output):
    for (uint i = 0; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        if (cell.unsupp) continue;

        NamedSet<SC_Pin>& pins = cell.pins;
        for (uint j = 0; j < pins.size(); j++){
            SC_Pin& pin = pins[j];

            if (pin.rise_cap == 0.0) pin.rise_cap = pin.cap;    // -- use 'cap' if 'rise_cap' is missing
            if (pin.fall_cap == 0.0) pin.fall_cap = pin.cap;

            for (uint k = 0; k < pin.timing.size(); k++){
                SC_Timing& timing = pin.timing[k];
                if (!timing.related_pin)
                    Throw(Excp_ParseError) "Cell '%_', pin '%_' has timing section without 'related_pin'.", cell.name, pin.name;

                try {
                    adjustIndices(timing.cell_rise, L.templ);
                    adjustIndices(timing.cell_fall, L.templ);
                    adjustIndices(timing.rise_trans, L.templ);
                    adjustIndices(timing.fall_trans, L.templ);
                }catch (String msg){
                    Throw(Excp_ParseError) "Cell '%_', pin '%_', related_pin '%_': %_", cell.name, pin.name, timing.related_pin, msg;
                }

                SC_Timings& timings = pin.rtiming.addWeak(timing.related_pin);
                timings.push();
                timing.moveTo(timings.last());
            }
            pin.timing.clear(true);

            if (pin.max_out_slew == -1){
                if (L.default_max_out_slew != -1){
                    pin.max_out_slew = L.default_max_out_slew;
                }else{
                    float hi = -1;
                    for (uint k = 0; k < pin.rtiming.size(); k++){
                        for (uint n = 0; n < pin.rtiming[k].size(); n++){
                            newMax(hi, pin.rtiming[k][n].rise_trans.data.last().last());
                            newMax(hi, pin.rtiming[k][n].fall_trans.data.last().last());
                        }
                    }
                    pin.max_out_slew = hi;
                }
            }
        }
    }

    // Sort the entries in each group on "related_pin" plus add missing entries (can happen
    // if an output does not depend on an input). In  static timing mode, also merge tables.
    for (uint i = 0; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        if (cell.unsupp) continue;

        for (uint j = 0; j < cell.pins.size(); j++){
            SC_Pin& pin = cell.pins[j];
            if (pin.dir != sc_dir_Output) continue;

            for (uint k = 0; k < cell.pins.size(); k++){
                SC_Pin& pin2 = cell.pins[k];
                if (pin2.dir != sc_dir_Input) continue;

                uint r = pinIndex(pin2.name, pin.rtiming);
                if (r == UINT_MAX){
                    // Output 'pin' does not depend on input 'pin2'; add empty list of tables:
                    r = pin.rtiming.size();
                    pin.rtiming.add(pin2.name);
                }
                swp_mem(pin.rtiming[k], pin.rtiming[r]);
            }
            pin.rtiming.rehash();

            if (timing_mode != ttm_Keep){
                // Merge tables (unateness and timing values):
                for (uint k = 0; k < pin.rtiming.size(); k++){
                    SC_Timings& tables = pin.rtiming[k];
                    if (timing_mode == ttm_Merge){
                        for (uint n = 1; n < tables.size(); n++)
                            mergeTiming(tables[0], tables[n]);
                    }
                    tables.shrinkTo(1);
                    if (tables.size() == 1)
                        tables[0].when_text = Str_NULL;
                }
            }
        }
    }
}


static
void extractFunctions(SC_Lib& L)
{
    for (uint i = 0; i < L.cells.size(); i++){
        SC_Cell& cell = L.cells[i];
        if (cell.unsupp) continue;

        Vec<Str> var_names;
        for (uint j = 0; j < cell.n_inputs; j++){
            SC_Pin& pin = cell.pins[j]; assert(pin.dir == sc_dir_Input);
            var_names.push(pin.name);
        }

        for (uint j = 0; j < cell.n_outputs; j++){
            SC_Pin& pin = cell.pins[cell.n_inputs + j]; assert(pin.dir == sc_dir_Output);

            try{
                pin.func = parseBoolExpr(pin.func_text, var_names);
            }catch (Excp_Msg err){
                Throw(Excp_ParseError) "In '%_/%_': %_", cell.name, pin.name, err;
            }
        }
    }
}


void LibertyParser::postProcess(SC_Lib& L, TimingTablesMode timing_mode)
{
    sortPins(L);
    normalizeTables(L, timing_mode);
    extractFunctions(L);
    if (timing_mode != ttm_Keep)
        approxCellTables(L);
}


//=================================================================================================
// -- Helpers:


uint LibertyParser::getLocation(cchar* pos) const
{
    if (pos == NULL)
        return 0;

    String file  = "";
    uint   line  = 1;
    cchar* p     = text.base();

    while (*p){
        if (p >= pos)
            return line;

        if (*p == '\n' || *p == '\f') line++;
        p++;
    }
    assert(false);
}


//=================================================================================================
// -- Preprocessor:


// Replace comments with spaces. Returns FALSE if 'text' contains an unterminated block comment.
static bool removeComments(Array<char>& text)
{
    char* p   = &text[0];
    char* end = &text.end_();

    while (p != end){
        if (*p == '"'){
            // Scan for end of string; ignore \":
            p++;
            bool ignore = false;
            for(;;){
                if (p == end)
                    return true;        // -- unterminated string (will be caught by lexer)
                else if (*p == '\\'){
                    p++;
                    ignore = true;
                }else if (*p == '"' && !ignore){
                    p++;
                    break;
                }else{
                    p++;
                    ignore = false;
                }
            }

        }else if (*p == '/' && p+1 != end && p[1] == '/'){
            // Space out line comment:
            for(;;){
                if (p == end)
                    break;
                else if (*p == '\n'){
                    p++; break;
                }else
                    *p++ = ' ';
            }

        }else if (*p == '/' && p+1 != end && p[1] == '*'){
            // Space out block comment (preserving newlines):
            p[0] = p[1] = ' ';
            p += 2;
            for(;;){
                if (p == end)
                    return false;       // -- unterminated comment
                else if (*p == '\n')
                    p++;
                else if (*p == '*' && p+1 != end && p[1] == '/'){
                    p[0] = p[1] = ' ';
                    p += 2;
                    break;
                }else
                    *p++ = ' ';
            }

        }else
            p++;
    }

    return true;
}


macro void removeSemi(Str& text)
{
    if (text.null() || text.size() == 0)
        return;

    if (text.last() == ';'){
        text.pop();
        trimEnd(text);
    }
}


void LibertyParser::preprocess(String file)
{
    text = readFile(file, /*add null?*/true);
    if (!text)
        Throw(Excp_ParseError) "Could not open: %_", file;

    if (!removeComments(text))
        Throw(Excp_ParseError) "Unterminated multi-line comment in file: %_", file;

    // Replace all whitespaces except newlines with space:
    for (uint i = 0; i < text.size(); i++)
        if (text[i] <= 13 && (text[i] == 9 || text[i] >= 11))
            text[i] = 32;

    // Separate file into logical lines (and remove backslashes):
    uint start = 0;
    for (uint i = 0; i < text.size()-1; i++){
        if (text[i] == '\\' && text[i+1] == '\n'){
            text[i] = ' ';
            i++;

        }else if (text[i] == '\n'){
            lines.push(strip(text.slice(start, i)));
            start = i + 1;

        }else if ((text[i] == '{' || text[i] == '}' || text[i] == ';') && text[i+1] != '\n'){
            lines.push(strip(text.slice(start, i + 1)));
            start = i + 1;
        }
    }
    lines.push(strip(text.slice(start, text.size()-1)));    // -- '-1' because of the null at the end

    forAll(lines, removeSemi);
    filterOut(lines, isEmpty<Str>);
}


//=================================================================================================
// -- Parser:


void LibertyParser::parse(LibertyListener& lis)
{
    lines.push(Str_NULL);

    Str*   p = &lines[0];
    cchar* bol = NULL;
    try{
        while (*p != Str_NULL){
            Str text = strip(*p++); assert(text.size() != 0);
            bol = &text[0];

            if (text[0] == '}'){
                // End of group statement:
                assert(text.size() == 1);
                lis.endGroup();

            }else{
                // Get attribute name:
                uint pos = scanFor(text, ':', '(');
                if (pos == UINT_MAX)
                    Throw(Excp_ParseError) "[%_] Invalid statement: %_", getLocation(bol), text;

                Str name = strip(text.slice(0, pos));

                if (text[pos] == ':'){
                    // Simple attribute:
                    lis.simpleAttr(name, strip(text.slice(pos+1)));

                }else{
                    // Complex or group attribute:
                    if (text.last() == ')')
                        lis.complexAttr(name, strip(text.slice(pos+1, text.size()-1)));
                    else if (text.last() == '{'){
                        text.pop();
                        trimEnd(text);
                        if (text.last() != ')')
                            Throw(Excp_ParseError) "[%_] Expected ')' before '{'.", getLocation(&text.last());
                        lis.group(name, strip(text.slice(pos+1, text.size()-1)));
                    }else
                        Throw(Excp_ParseError) "[%_] Invalid statement: %_", getLocation(bol), text;
                }
            }
        }
        lis.end();

    }catch (const String& msg){
        Throw(Excp_ParseError) "[%_] %_", getLocation(bol), msg;
    }catch (const Excp_ParseNum& err){
        Throw(Excp_ParseError) "[%_] Invalid number: %_", getLocation(bol), Excp_ParseNum::Type_name[err.type];
    }
}


//=================================================================================================
// -- Formatting with units:


double SC_Lib::ff(double cap) const
{
    return cap * unit_cap.fst * pow(10.0, 15 - unit_cap.snd);
}


double SC_Lib::ps(double time) const
{
    return time * pow(10.0, 12 - unit_time);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper function:


void readLiberty(String filename, SC_Lib& out_lib, TimingTablesMode timing_mode)
{
    LibertyParser p;

    p.read(filename, out_lib, timing_mode);
}


/*
    capacitive_load_unit (1,pf) ;   -- can only be 'ff' or 'pf' (1,pf seems to be default)
    time_unit : "1ns";  -- valid values: 1ps, 10ps, 100ps, 1ns (default)
*/

/*
Todo:

  - Verify unateness from function expression (correct or ignore invalid gates?)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
