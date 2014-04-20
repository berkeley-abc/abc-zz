//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PostProcess.cc
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Post-process mapping to fit particular target constraints.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PostProcess.hh"
#include "ZZ_BFunc.hh"
#include "LutMap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// This procedure will attempt to invert LUTs to preserve 'remap' as much as possible, but no
// gates will be added to track signals (so if 'r[10] = w42' and 'r[20] = ~w42' then 'r[20]'
// will be cleared).
void removeRemapSigns(Gig& N, WMapX<GLit>& remap)
{
    WSeen pos, neg;
    Vec<GLit>& r = remap.base();

    for (uint i = 0; i < r.size(); i++){
        GLit w = r[i];
        if (w.sign) neg.add(w);
        else        pos.add(w);
    }

    WSeen flipped;
    for (uint i = 0; i < r.size(); i++){
        if (!r[i]) continue;

        Wire w = r[i] + N;
        if (neg.has(w)){
            if (w != gate_Lut6 || pos.has(w)){
                if (r[i].sign)
                    r[i] = GLit_NULL;
            }else{
                // Invert LUT:
                if (!flipped.has(w)){
                    ftb(w) = ~ftb(w);
                    flipped.add(w);
                }
                r[i] = ~r[i];
            }
        }
    }

    For_Gates(N, w)
        For_Inputs(w, v)
            if (flipped.has(v))
                w.set(Input_Pin(v), ~v);
}


void removeInverters(Gig& N, WMapX<GLit>* remap)
{
    // Categorize fanout-inverter situation, considering only non-LUT fanouts:
    WSeen pos, neg;
    For_Gates(N, w){
        if (w == gate_Lut6) continue;

        For_Inputs(w, v){
            if (v.sign){
                if (v == gate_Lut6){
                    // LUT has a signed fanout:
                    neg.add(v);
                }else if (v == gate_Const){
                    if      (v == ~N.True ()) w.set(Input_Pin(v), N.False());
                    else if (v == ~N.False()) w.set(Input_Pin(v), N.True ());
                    else                      assert(false);
                }else{
                    // Non-LUT to non-LUT connection with inverters -- need to insert a inverter:
                    Wire u = N.add(gate_Lut6).init(+v);
                    ftb(u) = 0x5555555555555555ull;
                    w.set(Input_Pin(v), u);
                }
            }else if (v == gate_Lut6)
                // LUT has a non-signed fanout:
                pos.add(v);
        }
    }

    // Remove inverters between a non-LUT fed by a LUT:
    WSeen flipped;
    WMap<GLit> neg_copy;
    WSeen added;
    For_Gates(N, w){
        if (added.has(w)) continue;

        For_Inputs(w, v){
            if (!neg.has(v)) continue;

            if (!pos.has(v)){
                // Only negated fanouts; invert FTB:
                if (!flipped.has(v)){
                    ftb(v) = ~ftb(v);
                    flipped.add(v);
                }
                w.set(Input_Pin(v), ~v);

            }else{
                // Both negated and non-negated fanouts, duplicate gate:
                if (!neg_copy[v]){
                    Wire u = N.add(gate_Lut6);
                    added.add(u);
                    for (uint i = 0; i < 6; i++)
                        u.set(i, v[i]);
                    ftb(u) = ~ftb(v);
                    neg_copy(v) = u;
                }

                if (v.sign)
                    w.set(Input_Pin(v), neg_copy[v]);
            }
        }
    }

    // Remove inverted LUTs from 'remap':
    if (remap){
        Vec<GLit>& r = remap->base();
        for (uint i = 0; i < r.size(); i++)
            if (flipped.has(r[i]))
                r[i] = GLit_NULL;
    }

    // Remove inverters on the input side of a LUT:              
    For_Gates(N, w){
        if (w != gate_Lut6) continue;

        For_Inputs(w, v){
            if (v.sign){
                w.set(Input_Pin(v), +v);
                ftb(w) = ftb6_neg(ftb(w), Input_Pin(v));
            }
        }
    }
}


void removeMuxViolations(Gig& N, const WMap<float>& arrival, float target_arrival, WMapX<GLit>* remap)
{
    WMap<uchar> mux_fanouts(0);
    mux_fanouts.reserve(N.size());

    uint incomplete_f8s = 0;
    uint input_violation_f8s = 0;
    uint input_violation_f7s = 0;

    // Resolve incomplete F8s:
    For_Gates(N, w){
        if (w == gate_Mux){
            uint pin = 0;
            if (w[2] == gate_Mux && w[1] != gate_Mux)
                pin = 1;
            else if (w[1] == gate_Mux && w[2] != gate_Mux)
                pin = 2;

            if (pin != 0){
                // Add dummy F7:
                Wire v = N.add(gate_Lut6);
                ftb(v) = 0xFFFFFFFFFFFFFFFFull; // -- dummy data input; never used.
                Wire u = N.add(gate_Mux).init(N.True(), w[pin], v);
                w.set(pin, u);
                incomplete_f8s++;
                //**/WriteLn "%%%% F8 incomplete: w=%_ u=%_ pin=%_", w, u, pin;
            }
        }
    }

    // Resolve F8 input violations:
    For_Gates(N, w){
        if (w == gate_Mux){
            for (uint i = 1; i <= 2; i++){
                if (w[i] == gate_Mux){
                    if (mux_fanouts[w[i]] == 0)
                        mux_fanouts(w[i]) = 1;
                    else{
                        // Duplicate input MUX:
                        Wire u = N.add(gate_Mux).init(w[i][0], w[i][1], w[i][2]) ^ w[i].sign;
                        w.set(i, u);
                        input_violation_f8s++;
                        //**/WriteLn "%%%% F8 input: w=%_ u=%_ pin=%_", w, u, i;
                    }
                }
            }
        }
    }

    // Resolve F7 input violations:
    WMap<float> depart;
    For_DownOrder(N, w){
        if (w == gate_Mux){
            newMax(depart(w[0]), depart[w] + 1.0f);

            for (uint i = 1; i <= 2; i++){
                if (w[i] != gate_Mux){
                    assert(w[i] == gate_Lut6);
                    if (mux_fanouts[w[i]] == 0)
                        mux_fanouts(w[i]) = 1;
                    else{
                        assert(w[i] == gate_Lut6);
//                        assert(arrival[w] + depart[w]  <= target_arrival);

                        if (arrival[w] + depart[w] >= target_arrival/* && false*/){
                            // Duplicate input LUT:
                            Wire u = N.add(gate_Lut6) ^ w[i].sign;
                            ftb(u) = ftb(w[i]);
                            for (uint j = 0; j < 6; j++)
                                u.set(j, w[i][j]);
                            w.set(i, u);
                            //**/putchar('*'), fflush(stdout);
                        }else{
                            // Insert buffer (to save wires):
                            Wire u = N.add(gate_Lut6).init(w[i]) ^ w[i].sign;
                            ftb(u) = 0xAAAAAAAAAAAAAAAAull;
                            w.set(i, u);
                            //**/putchar('.'), fflush(stdout);
                        }
                        input_violation_f7s++;
                        newMax(depart(w[i]), depart[w]);
                        //**/WriteLn "%%%% F7 input: w=%_ u=%_ pin=%_", w, u, i;
                    }

                    depart(w[i]) = depart[w];
                }
                For_Inputs(w[i], v)
                    newMax(depart(v), depart[w[i]] + 1.0f);
            }

        }else{
            float delay = (w == gate_Lut6)  ? 1.0f :
                          (w == gate_Delay) ? w.arg() / DELAY_FRACTION :
                          /*otherwise*/       0.0f;
            For_Inputs(w, v)
                newMax(depart(v), depart[w] + delay);
        }
    }

    // Remove inverters (except on input side of F7s, which will be handled later):
    For_Gates(N, w){
        if (w == gate_Mux && w[0].sign){
            // Swap inputs:
            w.set(0, +w[0]);
            Wire w1 = w[1];
            w.set(1, w[2]);
            w.set(2, w1);
            //**/putchar('='), fflush(stdout);
        }
    }

    Auto_Gob(N, FanoutCount);
    WSeen flipped;
    For_Gates(N, w){
        For_Inputs(w, v){
            if (v == gate_Mux && v.sign){
                if (nFanouts(v) == 1){  // <<== if we have more fanouts, we could in principle push negations around to other fanouts
                    // Negate inputs of feeding mux:
                    w.set(Input_Pin(v), +v);
                    v.set(1, ~v[1]);
                    v.set(2, ~v[2]);
                    flipped.add(v);
                    //**/putchar('0' + Input_Pin(v)), fflush(stdout);
                }
                //**/else putchar('e'), fflush(stdout);
            }
        }
    }

    if (remap){
        Vec<GLit>& r = remap->base();
        for (uint i = 0; i < r.size(); i++)
            if (flipped.has(r[i]))
                //**/putchar('!'), fflush(stdout),
                r[i] = GLit_NULL;
    }

    /**/Write "  -- "; Dump(incomplete_f8s, input_violation_f8s, input_violation_f7s);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
