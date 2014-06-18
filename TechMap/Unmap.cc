//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Unmap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Expand LUTs back to GIG.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Unmap.hh"
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
uint getLevel(Wire w, WMap<uint>& level)
{
    if (level[w] == UINT_MAX){
        uint lv = 0;
        For_Inputs(w, v)
            newMax(lv, getLevel(v, level));
        level(w) = lv + 1;
    }
    return level[w];
}


static
uint findLevelLim(Gig& N, GLit* args, uint sz, WMap<uint>& level)
{
    assert(sz >= 2);

    // Find smallest level:
    uint best = UINT_MAX;
    uint best_i = UINT_MAX;
    for (uint i = 0; i < sz; i++){
        if (newMin(best, getLevel(args[i] + N, level)))
            best_i = i;
    }
    assert(best_i != UINT_MAX);

    // Find second level:
    best = UINT_MAX;
    for (uint i = 0; i < sz; i++){
        if (i == best_i) continue;
        newMin(best, getLevel(args[i] + N, level));
    }
    assert(best != UINT_MAX);

    return best;
}


static
GLit buildK(Gig& N, uint sz, const uchar* prog, Vec<GLit>& nodes, GigBinOp f, GLit acc, uint64& seed, const Params_Unmap& P, WMap<uint>& level)
{
    GLit* args = (GLit*)alloca(sz * sizeof(GLit));
    for (uint i = 0; i < sz; i++)
        args[i] = GET(i);

    if (P.shuffle)
        shuffle(seed, slice(args[0], args[sz]));

    while (sz > 1){
        uint lv_lim = UINT_MAX;
        if (P.depth_aware)
            lv_lim = findLevelLim(N, args, sz, level);

        if (P.try_share){
            for (uint i = 1; i < sz; i++){
                if (level[args[i]] > lv_lim) continue;
                for (uint j = 0; j < i; j++){
                    // Try combining inputs 'i' and 'j'; if exist in netlist, keep combination:
                    if (level[args[j]] > lv_lim) continue;
                    Wire w = f(args[i] + N, args[j] + N, /*just_try*/true);
                    if (w){
                        putFirst(args, sz, i, j, w);
                        goto Found;
                    }
                }
            }
        }

        uint i, j;
        if (!P.depth_aware){
            i = 0;
            j = 1;
        }else{
            for (i = 0; i < sz-1; i++)
                if (level[args[i]] <= lv_lim) goto Found1;
            assert(false); Found1:;
            for (j = i+1; j < sz; j++)
                if (level[args[j]] <= lv_lim) goto Found2;
            assert(false); Found2:;
        }

        // No combination exists in the netlist (or 'try_share' is false); just combine two inputs:
        if (P.balanced)
            putLast(args, sz, i, j, f(args[i] + N , args[j] + N, false));
        else
            putFirst(args, sz, i, j, f(args[i] + N , args[j] + N, false));

      Found:;
    }

    return args[0];
}


static
GLit build(Gig& N, const Vec<uchar>& prog, Vec<GLit>& nodes, const Params_Unmap& P, WMap<uint>& level)
{

    if (prog.size() == 2 && (prog[1] & 0x7F) == DSD6_CONST_TRUE){
        assert(prog[0] == dsd_End);
        return (prog[1] & 0x80) ? GLit_True : ~GLit_True;
    }

    uint64 seed = DEFAULT_SEED;
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
        case dsd_kAnd: nodes.push(buildK(N, (uint)prog[i+1], (const uchar*)&prog[i+2], nodes, aig_And,  GLit_True, seed, P, level)); i += prog[i+1] + 2; break;
        case dsd_kXor: nodes.push(buildK(N, (uint)prog[i+1], (const uchar*)&prog[i+2], nodes, xig_Xor, ~GLit_True, seed, P, level)); i += prog[i+1] + 2; break;
        default:
            /**/WriteLn "Unxepected type: %d", prog[i];
            assert(false); }
    }
}


#undef GET


struct GigLis_Compact : GigLis {
    WMapX<GLit>* remap;
    GigLis_Compact(WMapX<GLit>* remap_) : remap(remap_) {}

    virtual void compacting(const GigRemap& xlat) {
        if (remap){
            Vec<GLit>& rmap = remap->base();
            for (gate_id i = 0; i < rmap.size(); i++){
                rmap[i] = xlat(rmap[i]); }
        }
    }
};


// Unmap 6-LUT netlist while considering depth and sharing...
// <<== add depth awareness
// <<== add search when (a & b) and (a & c) both exists
void unmap(Gig& N, WMapX<GLit>* remap, const Params_Unmap& PU)
{
    assert(!N.is_frozen);

    if (remap){
        for (uint i = 0; i < N.size(); i++)
            (*remap)(GLit(i)) = GLit(i);
    }

    GigLis_Compact lis(remap);
    N.listen(lis, msg_Compact);

    N.compact();
    N.is_frozen = false;

    N.strash();

    N.unlisten(lis, msg_Compact);

    Params_Dsd P;
    P.use_kary = true;

    WMap<uint> level(UINT_MAX);        // -- only used in depth-aware mode
    if (PU.depth_aware){
        For_Gates(N, w)
            if (isCI(w))
                level(w) = 0;
    }

    WMapX<GLit> xlat;
    xlat.initBuiltins();
    Vec<GLit>   nodes;
    Vec<uchar>  prog;
    For_Gates(N, w){
        if (w == gate_Lut6 || w == gate_Lut4 || w == gate_F7Mux || w == gate_F8Mux){
            uint64 ftb_ = (w == gate_Lut6) ? ftb(w) :
                          (w == gate_Lut4) ? (uint64)w.arg() | ((uint64)w.arg() << 16)  | ((uint64)w.arg() << 32)  | ((uint64)w.arg() << 48) :
                          /*otherwise*/      0xD8D8D8D8D8D8D8D8ull;
            dsd6(ftb_, prog, P);

            // Replace LUT with 'prog':
            nodes.setSize(DSD6_FIRST_INTERNAL);
            for (uint i = 0; i < DSD6_FIRST_INTERNAL; i++)
                nodes[i] = (i < w.size()) ? xlat[w[i]] : GLit_NULL;

            xlat(w) = build(N, prog, nodes, PU, level);

        }else if (isCI(w)){
            xlat(w) = w;

        }else{
            For_Inputs(w, v){
                assert(xlat[v]);
                w.set(Input_Pin(v), xlat[v]); }
            xlat(w) = w;
        }
    }

    if (remap){
        Vec<GLit>& rmap = remap->base();
        for (uint i = 0; i < rmap.size(); i++)
            rmap[i] = xlat[rmap[i]];
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
