//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : PostProcess.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PostProcess.hh"
#include "ZZ_BFunc.hh"


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


// <<== note! could provide slack here and use inverters rather than duplicating gates if timing allows it
void removeInverters(Gig& N, WMapX<GLit>* remap, bool quiet)
{
    uint  luts_added  = 0;
    uint  wires_added = 0;
    uint  invs_added  = 0;

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
                    if (v == ~N.True())
                        w.set(Input_Pin(v), N.False());
                    else if (v == ~N.False())
                        w.set(Input_Pin(v), N.True());
                    else
                        assert(false);
                }else{
                    // Non-LUT to non-LUT connection with inverters -- need to insert a inverter:
                    Wire u = N.add(gate_Lut6).init(+v);
                    ftb(u) = 0x5555555555555555ull;
                    w.set(Input_Pin(v), u);
                    invs_added++;
                }
            }else if (v == gate_Lut6)
                // LUT has a non-signed fanout:
                pos.add(v);
        }
    }

    // Remove inverters between a non-LUT fed by a LUT:
    WSeen flipped;
    WSeen added;
    WMap<GLit> neg_copy;
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
                    luts_added++;
                    Wire u = N.add(gate_Lut6);
                    added.add(u);
                    for (uint i = 0; i < 6; i++){
                        u.set(i, v[i]);
                        if (v[i] != Wire_NULL) wires_added++;
                    }
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

    if (luts_added > 0 && !quiet)
        WriteLn "%_/%_ LUTs/wires added due to inverter removal.", luts_added, wires_added;
    if (invs_added > 0 && !quiet)
        WriteLn "%_ irremovable inverters between non-logic gates.", invs_added;
}


void removeMuxViolations(Gig& N, const WMap<float>& arrival, float target_arrival, float delay_fraction)
{
    WMap<uchar> mux_fanouts(0);
    mux_fanouts.reserve(N.size());

    uint input_violation_f7s = 0;

    // Resolve F7 input violations:
    WMap<float> depart;
    For_DownOrder(N, w){
        if (w == gate_F7Mux){
            newMax(depart(w[0]), depart[w] + 1.0f);

            for (uint i = 1; i <= 2; i++){
                assert(w[i] == gate_Lut6);
                if (mux_fanouts[w[i]] == 0)
                    mux_fanouts(w[i]) = 1;
                else{
                    assert(w[i] == gate_Lut6);

                    if (arrival[w] + depart[w] >= target_arrival){
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

                For_Inputs(w[i], v)
                    newMax(depart(v), depart[w[i]] + 1.0f);
            }

        }else{
            float delay = (w == gate_Lut6)  ? 1.0f :
                          (w == gate_Delay) ? w.arg() * delay_fraction:
                          /*otherwise*/       0.0f;
            For_Inputs(w, v)
                newMax(depart(v), depart[w] + delay);
        }
    }

    // Remove inverters on selectors:
    For_Gates(N, w){
        if (w == gate_F7Mux && w[0].sign){
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
            if (v == gate_F7Mux && v.sign){
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

    /**/Write "  -- "; Dump(input_violation_f7s);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
