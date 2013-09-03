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
void putLast(GLit* args, uint& sz, uint i, uint j, GLit new_w)
{
    assert(sz >= 2);
    if (sz > 2){
        uint n = 0;
        for (uint k = 0; k < sz; k++){
            if (k != i && k != j)
                args[n++] = args[k];
        }
    }
    sz--;
    args[sz-1] = new_w;
}


static
void putFirst(GLit* args, uint& sz, uint i, uint j, GLit new_w)
{
    putLast(args, sz, i, j, new_w);
    GLit last = args[sz-1];
    for (uint k = sz; k > 1;){ k--;
        args[k] = args[k-1]; }
    args[0] = last;
}


static
GLit buildK(Gig& N, uint sz, const uchar* prog, Vec<GLit>& nodes, GigBinOp f, GLit acc)
{
#if 1
    GLit* args = (GLit*)alloca(sz * sizeof(GLit));
    for (uint i = 0; i < sz; i++)
        args[i] = GET(i);

    while (sz > 1){
      #if 0
        for (uint i = 0; i < sz-1; i++){
            for (uint j = i+1; j < sz; j++){
      #else
        for (uint i = 1; i < sz; i++){
            for (uint j = 0; j < i; j++){
      #endif
                // Try combining inputs 'i' and 'j'; if exist in netlist, keep combination:
                Wire w = f(args[i] + N, args[j] + N, true);
                if (w){
                    putFirst(args, sz, i, j, w);
                    goto Found;
                }
            }
        }
        // No combination exists; just combine any two inputs:
        putFirst(args, sz, 0, 1, f(args[0] + N , args[1] + N, false));

      Found:;
    }

    return args[0];
#endif


#if 0
    GLit* args = (GLit*)alloca(sz * sizeof(GLit));
    for (uint i = 0; i < sz; i++)
        args[i] = GET(i);

    // <<== should make this depth aware
    while (sz > 1){
        for (uint i = sz; i > 1;){ i--;
            for (uint j = i-1; j > 0;){ j--;
                // Try combining inputs 'i' and 'j'; if exist in netlist, keep combination:
                Wire w = f(args[i] + N, args[j] + N, true);
                if (w){
                    args[j] = w;
                    sz--;
                    args[i] = args[sz];
                    goto Found;
                }
            }
        }

        // No combination exists; just combine any two inputs:
      #if 0
        sz--;
        args[sz-1] = f(args[sz] + N , args[sz-1] + N, false);
      #else
        args[0] = f(args[0] + N, args[1] + N, false);
        sz--;
        args[1] = args[sz];
      #endif

      Found:;
    }

    return args[0];
#endif

#if 0
    for (uint i = 0; i < sz; i++)
        acc = f(acc + N, GET(i), false);

    return acc;
#endif
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
// <<== add depth awareness
// <<== add search when (a & b) and (a & c) both exists
void unmap(Gig& N, WMapX<GLit>* remap)
{
    assert(!N.is_frozen);
    N.compact();
    N.is_frozen = false;

    N.strash();

    Params_Dsd P;
    P.use_kary = true;

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

    if (remap)
        xlat.moveTo(*remap);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
