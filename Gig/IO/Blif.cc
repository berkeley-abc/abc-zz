//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Blif.cc
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Blif writer, intended for transferring LUT mapped designs to ABC
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Aiger.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#define Name BlifVarName

struct Name {
    char chr;
    uint num;
    BlifVarName(char chr_ = 0, uint num_ = 0) : chr(chr_), num(num_) {}
};

template<> fts_macro void write_(Out& out, const Name& v)
{
    out += v.chr, v.num;
}


// Will output combinational netlist; sequential elements are treated as CIs/COs.
void writeBlif(Out& out, Gig& N)
{
    // Generate names:
    WMap<Name> nam;
    nam(N.False()) = Name('c', 0);
    nam(N.True ()) = Name('c', 1);
    For_Gates(N, w){
        switch (w.type()){
        case gate_PI: nam(w) = Name('i', w.num()); break;
        case gate_PO: nam(w) = Name('o', w.num()); break;
        case gate_FF: nam(w) = Name('f', w.num()); nam(w[0]) = Name('s', w.num()); break;
        case gate_Seq: break;
        default: nam(w) = Name('w', w.id); }
    }

    // Write BLIF file:
    FWriteLn(out) ".model main";

    FWrite(out) ".inputs";
    For_Gatetype(N, gate_PI, w)
        FWrite(out) " %_", nam[w];
    For_Gatetype(N, gate_FF, w)
        FWrite(out) " %_", nam[w];
    FNewLine(out);

    FWrite(out) ".outputs";
    For_Gatetype(N, gate_PO, w)
        FWrite(out) " %_", nam[w];
    For_Gatetype(N, gate_FF, w)
        FWrite(out) " %_", nam[w[0]];
    FNewLine(out);

    FWriteLn(out) ".names %_\n 1", nam[N.True ()];
    FWriteLn(out) ".names %_\n 0", nam[N.False()];

    For_UpOrder(N, w){
        assert(nam[w].chr);
        switch (w.type()){
        case gate_PI:
        case gate_FF:
            break;
        case gate_PO:
        case gate_Seq:
            FWriteLn(out) ".names %_ %_", nam[w[0]], nam[w];
            FWriteLn(out) "%_ 1", w[0].sign ? 0 : 1;
            break;

        case gate_And:{
            FWriteLn(out) ".names %_ %_ %_", nam[w[0]], nam[w[1]], nam[w];
            FWriteLn(out) "%_%_ 1", w[0].sign ? 0 : 1, w[1].sign ? 0 : 1;
            break;}

        case gate_Lut6:{
            uint n_inputs = 6;
            while (n_inputs > 0 && w[n_inputs-1] == Wire_NULL)
                n_inputs--;
            for (uint i = 0; i < n_inputs; i++)
                assert(w[i] != Wire_NULL);  // -- must not be gaps between the used input pins, only top pins can be unused

            FWrite(out) ".names";
            for (uint i = 0; i < n_inputs; i++)
                FWrite(out) " %_", nam[w[i]];
            FWriteLn(out) " %_", nam[w];

            bool wrote_something = false;
            for (uint v = 0; v < (1u << n_inputs); v++){
                if (ftb(w) & (1ull << v)){
                    for (uint i = 0; i < n_inputs; i++)
                        out += (bool(v & (1u << i)) ^ w[i].sign) ? '1' : '0';
                    FWriteLn(out) " 1";
                    wrote_something = true;
                }
            }

            if (!wrote_something){
                for (uint i = 0; i < n_inputs; i++)
                    out += '-';
                FWriteLn(out) " 0";    // -- degenerate case (constant zero)
            }

            break;}

        case gate_Lut4:{
            uint n_inputs = 4;
            while (n_inputs > 0 && w[n_inputs-1] == Wire_NULL)
                n_inputs--;
            for (uint i = 0; i < n_inputs; i++)
                assert(w[i] != Wire_NULL);  // -- must not be gaps between the used input pins, only top pins can be unused

            FWrite(out) ".names";
            for (uint i = 0; i < n_inputs; i++)
                FWrite(out) " %_", nam[w[i]];
            FWriteLn(out) " %_", nam[w];

            bool wrote_something = false;
            for (uint v = 0; v < (1u << n_inputs); v++){
                if (w.arg() & (1ull << v)){
                    for (uint i = 0; i < n_inputs; i++)
                        out += (bool(v & (1u << i)) ^ w[i].sign) ? '1' : '0';
                    FWriteLn(out) " 1";
                    wrote_something = true;
                }
            }

            if (!wrote_something){
                for (uint i = 0; i < n_inputs; i++)
                    out += '-';
                FWriteLn(out) " 0";    // -- degenerate case (constant zero)
            }

            break;}

        case gate_F7Mux:
        case gate_F8Mux:
        case gate_Mux:{
            FWriteLn(out) ".names %_ %_ %_ %_", nam[w[0]], nam[w[1]], nam[w[2]], nam[w];
            FWriteLn(out) "%_%_%_ 1", w[0].sign ? 1 : 0, w[1].sign ? 1 : 0, w[2].sign ? 0 : 1;
            FWriteLn(out) "%_%_%_ 1", w[0].sign ? 1 : 0, w[1].sign ? 0 : 1, w[2].sign ? 0 : 1;
            FWriteLn(out) "%_%_%_ 1", w[0].sign ? 0 : 1, w[1].sign ? 0 : 1, w[2].sign ? 1 : 0;
            FWriteLn(out) "%_%_%_ 1", w[0].sign ? 0 : 1, w[1].sign ? 0 : 1, w[2].sign ? 0 : 1;
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Gate type not (yet) supported in 'writeBlif()': %_", w.type();
            assert(false);
        }
    }

    FWriteLn(out) ".end";
}


bool writeBlifFile(String filename, Gig& N)
{
    OutFile out(filename);
    if (out.null()) return false;

    writeBlif(out, N);
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
