//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GenLib.cc
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Convert Liberty files to .genlib files (losing most information).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Liberty.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


macro float mid(const SC_Surface& S) {
    return S.data[S.index0.size() / 2] [S.index1.size() / 2]; }


static
void writeFunc(Out& out, const Vec<uint>& cover, const Vec<Str>& names)
{
    if (cover.size() == 0)
        out += "CONST0";

    else{
        for (uind i = 0; i < cover.size(); i++){
            if (cover[i] == 0)
                out += "CONST1";
            else{
                if (i != 0)
                    out += " + ";
                bool first = true;
                for (uint j = 0; j < 32; j++){
                    if (cover[i] & (1 << j)){
                        if (first) first = false;
                        else out += '*';

                        if (j & 1) out += '!';
                        out += names[j >> 1];
                    }
                }
            }
        }
    }
}


static
String funcText(const SC_Pin& pin, const SC_Cell& cell)
{
    const BoolFun& v = pin.func;

    uint n_words = ((1 << v.nVars()) + 31) >> 5;
    Vec<uint> ftb(n_words);
    for (uint i = 0; i < ftb.size(); i++){
        if ((i & 1) == 0)
            ftb[i] = (uint)v[i>>1];
        else
            ftb[i] = (uint)(v[i>>1] >> 32);
    }

    Vec<uint> cover;
    irredSumOfProd(v.nVars(), ftb, cover);
    reverse(cover);

    String out;
    Vec<Str> names;
    for (uint i = 0; i < cell.n_inputs; i++)
        names.push(cell.pins[i].name);
    writeFunc(out, cover, names);
    return out;
}


bool writeGenlibFile(String filename, const SC_Lib& L, bool quiet = false)
{
    OutFile out(filename);
    if (out.null()) return false;

    uint ignored_unsupp = 0;
    uint ignored_seq    = 0;
    uint ignored_multi  = 0;

    // Sort cells on size (beneficial for ABCs mapper):
    Vec<Pair<float,uint> > sz_idx;
    for (uint i = 0; i < L.cells.size(); i++){
        const SC_Cell& cell = L.cells[i];
        if (cell.unsupp)        { ignored_unsupp++; continue; }
        if (cell.seq)           { ignored_seq++;    continue; }
        if (cell.n_outputs > 1) { ignored_multi++;  continue; }
        sz_idx.push(tuple(cell.area, i));
    }
    sort(sz_idx);

    for (uint i = 0; i < sz_idx.size(); i++){
        const SC_Cell& cell = L.cells[sz_idx[i].snd];
        const SC_Pin&  pin  = cell.pins[cell.n_inputs];

        FWriteLn(out) "GATE %_ %_ %_=%_;", cell.name, cell.area, pin.name, funcText(pin, cell);

        for (uint j = 0; j < cell.n_inputs; j++){
            assert(pin.rtiming[j].size() == 1);     // <<== fails if function does not depend on all inputs
            const SC_Timing& t = pin.rtiming[j][0];
            const SC_Pin&    p = cell.pins[j];

            cchar* phase = (t.tsense == sc_ts_Pos) ? "INV"    :
                           (t.tsense == sc_ts_Neg) ? "NONINV" :
                                                     "UNKNOWN";

            FWriteLn(out) "PIN %_ %_ %_ %_ %_ 0 %_ 0",
                p.name, phase,
                max_(p.rise_cap, p.fall_cap), p.max_out_cap,
                L.ps(mid(t.cell_rise)), L.ps(mid(t.cell_fall));
        }
    }

    if (!quiet){
        if (ignored_unsupp > 0) WriteLn "WARNING! Ignored %_ unsupported (non-logic) cells.", ignored_unsupp;
        if (ignored_seq    > 0) WriteLn "WARNING! Ignored %_ sequential cells.", ignored_seq;
        if (ignored_multi  > 0) WriteLn "WARNING! Ignored %_ multi-output cells.", ignored_multi;

        WriteLn "Successfully processed %_ cells.", sz_idx.size();
    }

    return true;
}


/*
GATE <cell-name> <cell-area> <cell-logic-function>
PIN <pin-name> <phase> <input-load> <max-load>
    <rise-block-delay> <rise-fanout-delay>
    <fall-block-delay> <fall-fanout-delay>

<phase> is one of: INV NONINV UNKNOWN    

Example:

  GATE NR3D8 18.72 ZN=!A1*!A2*!A3 ;
  PIN * INV 1 999999 65.10000 0 65.10000 0
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
