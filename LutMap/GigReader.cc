//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GigReader.cc
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Partial GIG reader for techmapping experimental purposes only.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"
#include "ZZ/Generics/Map.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Reader:


const uint delay_MemR = 15;
const uint delay_MemW = 13;
const uint delay_PadR = 4;
const uint delay_PadW = 4;
    // -- these don't have to be accurate, it is just for testing anyway


void readGigForTechmap(String filename, Gig& N)
{
    assert(N.isEmpty());

    InFile in(filename);
    if (!in)
        Throw(Excp_Msg) "Could not open file for reading: %_", filename;

    StackAlloc<char> mem_names;
    Map<Str,GLit> name2gate;
    Vec<Pair<GLit,uint> > fgates;
    Vec<Str> fanins;
    Vec<GLit> loops;

    name2gate.set(slize("0"), N.False());
    name2gate.set(slize("1"), N.True());
    name2gate.set(slize("-"), Wire_NULL);

    static const Array<cchar> seps(",");
    Vec<Str> args;
    Vec<char> buf;

    for (uint phase = 0; phase < 2; phase++){
        // phase 0: gate creation
        // phase 1: fanin attachment

        uind line_no = 0;
        while (!in.eof()){
            // Read line:
            readLine(in, buf);
            line_no++;

            uind pos = search(buf, '#');
            if (pos != UIND_MAX)
                buf.shrinkTo(pos);
            Str text = strip(buf.slice());
            if (text.size() == 0)
                continue;

            // Split line into components:
            pos = search(text, '=');
            if (pos == UIND_MAX)
                Throw(Excp_Msg) "[line %_] Missing '=' sign.", line_no;

            Str name = strip(text.slice(0, pos));
            Str expr = strip(text.slice(pos+1));

            Str attr = Str_NULL;
            pos = search(expr, '[');
            if (pos != UIND_MAX){
                attr = expr.slice(pos);
                expr = strip(expr.slice(0, pos));

                if (attr[LAST] != ']')
                    Throw(Excp_Msg) "[line %_] Missing ']' after attribute.", line_no;
                attr = attr.slice(1, attr.size()-1);
            }

            Str gate;
            args.clear();
            pos = search(expr, '(');
            if (pos == UIND_MAX){
                gate = expr;
                expr = Str_NULL;
            }else{
                gate = strip(expr.slice(0, pos));

                expr = expr.slice(pos);

                if (expr[LAST] != ')')
                    Throw(Excp_Msg) "[line %_] Missing ')' after argument list.", line_no;
                expr = expr.slice(1, expr.size()-1);

                strictSplitArray(expr, seps, args);
                forAll(args, trimStr);
            }

            // Create gate:
            Wire w;
            if (phase == 0){
                if      (eq(gate, "Const")) w = N.True() ^ (attr[0] == '0');
                else if (eq(gate, "PI"   )) w = N.add(gate_PI);
                else if (eq(gate, "PO"   )) w = N.add(gate_PO);
                else if (eq(gate, "And"  )) w = N.add(gate_And);
                else if (eq(gate, "FF"   )) w = N.add(gate_FF);
                else if (eq(gate, "FD01" )) w = N.addDyn(gate_Box, args.size());
                else if (eq(gate, "Pin"  )) w = N.add(gate_Sel);
                else if (eq(gate, "MemR" )) w = N.addDyn(gate_Delay, args.size(), delay_MemR);
                else if (eq(gate, "MemW" )) w = N.addDyn(gate_Delay, args.size(), delay_MemW);
                else if (eq(gate, "PadR" )) w = N.addDyn(gate_Delay, args.size(), delay_PadR);
                else if (eq(gate, "PadW" )) w = N.addDyn(gate_Delay, args.size(), delay_PadW);
                else if (eq(gate, "Lut") || eq(gate, "Lut4") || eq(gate, "Loop")){
                    if (attr.size() != 4)
                        Throw(Excp_Msg) "[line %_] Invalid FTB, must be exactly four hexadecimal digits.", line_no;
                    ushort ftb = fromHex(attr[0])
                               | (fromHex(attr[1]) << 4)
                               | (fromHex(attr[2]) << 8)
                               | (fromHex(attr[3]) << 12);
                    w = N.add(gate_Lut4, ftb);
                    if (gate[1] == 'o')
                        loops.push(w);
                }else if (eq(gate, "Delay")){
                    float delay = 0;
                    try{
                        delay = attr ? stringToDouble(attr) : 0;
                    }catch (...){
                        Throw(Excp_Msg) "[line %_] Invalid delay number.", line_no;
                    }
                    w = N.addDyn(gate_Delay, args.size(), delay);
                }else
                    Throw(Excp_Msg) "[line %_] Unhandled gate type: %_", line_no, gate;

                name2gate.set(Array_copy(name, mem_names), w);

            }else{
                bool ok = name2gate.peek(name, w); assert(ok);
                w = w + N;  // -- hash map only stores GLit

                if (args.size() > w.size())
                    Throw(Excp_Msg) "[line %_] Too many fanins given to gate: %_", line_no, w.type();

                for (uint i = 0; i < args.size(); i++){
                    if (args[i].size() == 0)
                        Throw(Excp_Msg) "[line %_] Empty name in fanin list.", line_no;

                    Wire v;
                    if (args[i][0] != '~')
                        ok = name2gate.peek(args[i], v);
                    else{
                        ok = name2gate.peek(args[i].slice(1), v);
                        v = ~v;
                    }
                    if (!ok)
                        Throw(Excp_Msg) "[line %_] Fanin gate does not exist: %_", line_no, args[i];

                    v = v + N;
                    if (+v != Wire_NULL)
                        w.set(i, !isSeqElem(w) ? v : N.add(gate_Seq).init(v));  // -- 'Seq's are added here!
                }
            }
        }

        in.rewind();
    }

    // Post-process 'Loop's:
    for (uint i = 0; i < loops.size(); i++){
        // For loops, pin 0 should be the implicit unit-delayed output signal:
        Wire w = loops[i] + N;
        assert(!w[3]);
        w.set(3, w[2]);
        w.set(2, w[1]);
        w.set(1, w[0]);
        Wire w_seq = N.add(gate_Seq).init(w);
        Wire w_ff  = N.add(gate_FF ).init(w_seq);
        w.set(0, w_ff);
    }
    loops.clear(true);

    // Palladium fixup:
    Add_Gob(N, FanoutCount);
    For_Gates(N, w){
        bool has_fanins = false;
        for (uint i = 0; i < w.size(); i++)
            if (w[i]){
                has_fanins = true;
                break; }

        if (!has_fanins){
            if (nFanouts(w) == 0)
                remove(w);
            else
                change(w, gate_PI);

        }else if (nFanouts(w) == 0){
            if (w.size() == 1){
                Wire v = w[0];
                change(w, gate_PO);
                w.set(0, v);
            }else
                N.add(gate_PO).init(w);
        }
    }

    //N.compact();      // <<== disable this temporarily to test signal tracking
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Writer:


bool writeGigForTechmap(String filename, Gig& N)
{
    OutFile out(filename);
    if (!out) return false;

    For_Gates(N, w){
        FWrite(out) "%w = %_", w.lit(), w.type();
        if (w.size() == 0)
            FNewLine(out);
        else{
            FWrite(out) "(";
            for (uint i = 0; i < w.size(); i++){
                if (i != 0) out += ", ";
                if (w[i] == N.True() || w[i] == ~N.False())
                    FWrite(out) "1";
                else if (w[i] == ~N.True() || w[i] == N.False())
                    FWrite(out) "0";
                else if (w[i])
                    FWrite(out) "%w", w[i].lit();
                else
                    out += '-';
            }
            FWrite(out) ")";

            if (w == gate_Lut6)
                FWrite(out) " [%.16X]", ftb(w);
            else if (w == gate_Lut4)
                FWrite(out) " [%.4X]", w.arg();

            FNewLine(out);
        }
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
