//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BlifWriter.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Write a parsed and flattened Verilog file.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "BlifWriter.hh"
#include "Parser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Outputs a single, combinational netlist (with standard cells) in structual BLIF format.
void writeFlatBlif(Out& out, String module_name, NetlistRef N, const SC_Lib& L)
{
    Vec<char> buf, buf2, buf3, buf4;

    if (N.typeCount(gate_Pin) != 0){
        ShoutLn "INTERNAL ERROR! Blif writer cannot handle multi-output gates.";
        assert(false); }

    FWriteLn(out) ".model %_", module_name;

    FWrite(out) ".inputs";
    For_Gatetype(N, gate_PI, w)
        FWrite(out) " %_", N.names().get(w, buf);
    FNewLine(out);

    FWrite(out) ".outputs";
    For_Gatetype(N, gate_PO, w)
        FWrite(out) " %_", N.names().get(w, buf);
    FNewLine(out);

    // AIG part:
    For_Gatetype(N, gate_And, w){
        FWriteLn(out) ".names %_ %_ %_", N.names().get(+w[0], buf), N.names().get(+w[1], buf2), N.names().get(w, buf3);
        FWriteLn(out) "%c%c 1", sign(w[0])?'0':'1', sign(w[1])?'0':'1';
    }

    For_Gatetype(N, gate_Xor, w){
        FWriteLn(out) ".names %_ %_ %_", N.names().get(+w[0], buf), N.names().get(+w[1], buf2), N.names().get(w, buf3);
        if (sign(w[0]) == sign(w[1]))
            FWriteLn(out) "01 1\n10 1";
        else
            FWriteLn(out) "00 1\n11 1";
    }

    For_Gatetype(N, gate_Mux, w){
        // Mux: pin0 ? pin1 : pin2
        FWriteLn(out) ".names %_ %_ %_ %_", N.names().get(+w[0], buf), N.names().get(+w[1], buf2), N.names().get(+w[2], buf3), N.names().get(w, buf4);
        FWriteLn(out) "%c%c- 1", sign(w[0])?'0':'1', sign(w[1])?'0':'1';
        FWriteLn(out) "%c-%c 1", sign(w[0])?'1':'0', sign(w[2])?'0':'1';
    }

    For_Gatetype(N, gate_PO, w)
        FWriteLn(out) ".short %_ %_", N.names().get(+w[0], buf), N.names().get(w, buf2);

    // Mapped part:
    For_Gatetype(N, gate_Uif, w){
        const SC_Cell& cell = L.cells[attr_Uif(w).sym];
        FWrite(out) ".gate %_", cell.name;
        For_Inputs(w, v){
            FWrite(out) " %_=%_", cell.pins[Iter_Var(v)].name, N.names().get(v, buf); }
        FWriteLn(out) " %_=%_", cell.pins[cell.n_inputs].name, N.names().get(w, buf);
    }

    FWriteLn(out) ".end";
}


bool writeFlatBlifFile(String filename, String module_name, NetlistRef N, const SC_Lib& L)
{
    OutFile out(filename);
    if (out.null()) return false;

    writeFlatBlif(out, module_name, N, L);
    return true;

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
