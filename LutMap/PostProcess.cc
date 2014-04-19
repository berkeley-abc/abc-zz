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


void removeMuxViolations(Gig& N)
{
    WMap<uchar> mux_fanouts(0);
    mux_fanouts.reserve(N.size());

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
                Wire u = N.add(gate_Mux).init(N.True(), w[pin], N.True());
                w.set(pin, u);
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
                        Wire u = N.add(gate_Mux).init(w[0], w[1], w[2]);
                        w.set(i, u);
                    }
                }
            }
        }
    }

    // Resolve F7 input violations:
    For_Gates(N, w){
        if (w == gate_Mux){
            for (uint i = 1; i <= 2; i++){
                if (w[i] != gate_Mux){
                    assert(w[i] == gate_Lut6 || w[i] == gate_Const);
                    if (mux_fanouts[w[i]] == 0)
                        mux_fanouts(w[i]) = 1;
                    else{
                        // Duplicate input LUT:
                        Wire u = N.add(gate_Lut6);
                        for (uint j = 0; j < 6; j++)
                            w.set(j, w[j]);
                        w.set(i, u);
                    }
                }
            }
        }
    }

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
