//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Various support functions for combining Verilog and Liberty files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// UIF remapping:


void computeUifMap(const Vec<VerilogModule>& modules, const SC_Lib& L, /*out*/IntMap<uint,uint>& mod2cell)
{
    assert(mod2cell.size() == 0);
    mod2cell.nil = UINT_MAX;

    for (uint i = 0; i < modules.size(); i++){
        if (modules[i].black_box){
            uind idx = L.cells.idx(modules[i].mod_name.slice());
            if (idx == UIND_MAX){
                ShoutLn "ERROR! Design uses unknown standard cell gate: %_", modules[i].mod_name;
                exit(1);
            }

            mod2cell(i) = (uint)idx;
        }
    }
}


void remapUifs(NetlistRef N, const IntMap<uint,uint>& mod2cell)
{
    For_Gatetype(N, gate_Uif, w){
        uint sym = attr_Uif(w).sym;
        assert(mod2cell[sym] != UIND_MAX);
        attr_Uif(w).sym = mod2cell[sym];
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Consistency check:


template<class STR, class T>
macro uint pinIndex(const STR& name, const NamedSet<T>& list)
{
    for (uint k = 0; k < list.size(); k++)
        if (eq(name, list[k].name))
            return k;
    return UINT_MAX;
}


void verifyPinsSorted(const Vec<VerilogModule>& modules, const IntMap<uint,uint>& mod2cell, SC_Lib& L)
{
    for (uint i = 0; i < modules.size(); i++){
        if (mod2cell[i] == UINT_MAX) continue;

        const VerilogModule& mod = modules[i];
        SC_Cell& cell = L.cells[mod2cell[i]];
        if (cell.unsupp) continue;

        // Check input order:
        for (uint j = 0; j < mod.in_name.size(); j++){
            uint k = pinIndex(mod.in_name[j], cell.pins);
            assert(k == j); }

        // Check output order:
        for (uint i = 0; i < mod.out_name.size(); i++){
            uint j = i + mod.in_name.size();
            uint k = pinIndex(mod.out_name[i], cell.pins);
            assert(k == j); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Verilog library declarations:


// Generates verilog code (actually "black box" extension) for standard cell library.
void genPrelude(const SC_Lib& L, String& out, bool print_warnings/* = false*/)
{
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
//      if (cell.unsupp) continue;

        uind rollback = out.size();

        // Header:
        if (i != 0) FNewLine(out);
        FWriteLn(out) "// [-BLACK BOX-]";
        FWrite(out) "module %_(", cell.name;
        for (uint j = 0; j < cell.pins.size(); j++){
            if (j != 0) FWrite(out) ", ";
            FWrite(out) "%_", cell.pins[j].name;
        }
        FWriteLn(out) ");";

        // Types:
        for (uint j = 0; j < cell.pins.size(); j++){
            const SC_Pin& pin = cell.pins[j];
            if (pin.dir == sc_dir_Input)
                FWrite(out) "  input ";
            else if (pin.dir == sc_dir_Output)
                FWrite(out) "  output ";
            else{
                if (print_warnings)
                    WriteLn "WARNING! Ignoring gate (special pins): \a*%_\a*", cell.name;
                out.shrinkTo(rollback);
                goto Continue;
            }

            FWriteLn(out) "%_;", pin.name;
        }

        // Footer:
        FWriteLn(out) "endmodule";

      Continue:;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
