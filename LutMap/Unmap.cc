//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Unmap.cc
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Expand LUTs back to GIG.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"
#include "ZZ_Dsd.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


typedef Wire (*GigBinOp)(Wire, Wire, bool);


#define GET(n) N[nodes[prog[n] & 0x7F] ^ bool(prog[n] & 0x80)]


static
GLit buildK(Gig& N, uint sz, const uchar* prog, Vec<GLit>& nodes, GigBinOp f, GLit acc)
{
    for (uint i = 0; i < sz; i++){
        acc = f(acc + N, GET(i), false);
    }

    return acc;
}


static
GLit build(Gig& N, const Vec<uchar>& prog, Vec<GLit>& nodes)
{

    if (prog.size() == 2 && (prog[1] & 0x7F) == DSD6_CONST_TRUE){
        assert(prog[0] == dsd_End);
        return (prog[1] & 0x80) ? GLit_True : ~GLit_True;
    }

    uint i = 0;
    for(;;){
        switch (prog[i]){
        case dsd_End:  return GET(i+1);
        case dsd_And:  nodes.push(xig_And(GET(i+1), GET(i+2))); i += 3; break;
        case dsd_Xor:  nodes.push(xig_Xor(GET(i+1), GET(i+2))); i += 3; break;
        case dsd_Mux:  nodes.push(xig_Mux(GET(i+1), GET(i+2), GET(i+3))); i += 4; break;
        case dsd_Maj:  nodes.push(xig_Maj(GET(i+1), GET(i+2), GET(i+3))); i += 4; break;
        case dsd_One:  nodes.push(xig_One(GET(i+1), GET(i+2), GET(i+3))); i += 4; break;
        case dsd_Gamb: nodes.push(xig_Gamb(GET(i+1), GET(i+2), GET(i+3))); i += 4; break;
        case dsd_Dot:  nodes.push(xig_Dot(GET(i+1), GET(i+2), GET(i+3))); i += 4; break;
        case dsd_kAnd: nodes.push(buildK(N, (uint)prog[i+1], (const uchar*)&prog[i+2], nodes, aig_And,  GLit_True)); i += prog[i+1] + 2; break;
        case dsd_kXor: nodes.push(buildK(N, (uint)prog[i+1], (const uchar*)&prog[i+2], nodes, xig_Xor, ~GLit_True)); i += prog[i+1] + 2; break;
        default:
            /**/WriteLn "Unxepected type: %d", prog[i];
            assert(false); }
    }

}


#undef GET


// Unmap 6-LUT netlist while considering depth and sharing...
void unmap(Gig& N)
{
    assert(!N.is_frozen);
    N.compact();
    N.is_frozen = false;

    N.strash();

    Params_Dsd P;
    P.use_kary = false;

    WMapX<GLit> xlat;
    xlat.initBuiltins();
    Vec<GLit>   nodes;
    Vec<uchar>  prog;
    For_Gates(N, w){
        if (w == gate_Lut6){
            dsd6(ftb(w), prog, P);

            // Replace LUT with 'prog':
            nodes.setSize(DSD6_FIRST_INTERNAL);
            for (uint i = 0; i < DSD6_FIRST_INTERNAL; i++)
                nodes[i] = xlat[w[i]];

            xlat(w) = build(N, prog, nodes);

        }else if (isCI(w)){
            xlat(w) = w;

        }else{
            For_Inputs(w, v){
                assert(xlat[v]);
                w.set(Input_Pin(v), xlat[v]); }
            xlat(w) = w;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
