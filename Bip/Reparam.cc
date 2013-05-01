//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Reparam.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Reparameterization technique based on local transformations.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Reparam.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"
#include "ZZ_BFunc.hh"
#include "ParClient.hh"
/**/#include "Bmc.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Quick but limited reparametrization through local rewrites:



static
Wire buildCover(NetlistRef N, const Vec<uint>& cover, const WZet& ext_pi)
{
    Wire disj = ~N.True();
    for (uint i = 0; i < cover.size(); i++){
        Wire conj = N.True();
        for (uint j = 0; j < ext_pi.size() * 2; j++){
            if (cover[i] & (1u << j)){
                Wire w = N[ext_pi.list()[j>>1]] ^ bool(j & 1);
                conj = (conj == glit_True) ? w : mk_And(conj, w);
            }
        }
        disj = (disj == ~glit_True) ? conj : mk_Or(disj, conj);
    }
    return disj;
}


//**/static
//**/uint countBits(uint v)
//**/{
//**/    uint sz = 0;
//**/    while (v != 0){
//**/        sz++;
//**/        v &= v - 1;
//**/    }
//**/    return sz;
//**/}


static
void analyzeRegion(NetlistRef N, const WZet& area, const WZet& int_pi, const WZet& ext_pi, Wire w_dom, const Params_Reparam& P, /*temporary*/WMap<uchar>& val)
{
    uint n_words = max_(1u, (1u << ext_pi.size()) / 32);
    Vec<uint> on (n_words, 0);
    Vec<uint> off(n_words, 0);
    bool on_empty  = true;
    bool off_empty = true;

    for (uint xval = 0; xval < (1u << ext_pi.size()); xval++){
        for (uint i = 0; i < ext_pi.size(); i++){
            Wire w = N[ext_pi.list()[i]];
            val(w) = bool(xval & (1u << i)); }

        uint seen[2] = {false, false};
        for (uint ival = 0; ival < (1u << int_pi.size()); ival++){
            for (uint i = 0; i < int_pi.size(); i++){
                Wire w = N[int_pi.list()[i]];
                val(w) = bool(ival & (1u << i)); }

            for (uind i = area.size(); i > 0;){ i--;    // -- we use the fact that 'area' is in reverse topological order
                Wire w = area.list()[i];
                if (int_pi.has(w)) continue;
                if (ext_pi.has(w)) continue;

                if (type(w) == gate_And)
                    val(w) = ((bool)val[w[0]] ^ sign(w[0])) & ((bool)val[w[1]] ^ sign(w[1]));
                else assert(w.size() == 1),
                    val(w) = (bool)val[w[0]] ^ sign(w[0]);
            }
            seen[val[area.list()[0]]] = true;
            //**/WriteLn "xval=%b", xval;
            //**/WriteLn "ival=%b", ival;
            //**/WriteLn "val[area.list()[0]] = %d", val[area.list()[0]];
            //**/Dump(area.list()[0]);
        }
        if (!seen[0] || !seen[1]){
            if (int_pi.size() == 1)
                return;     // -- no reduction in #PIs possible, stop early
            if (!seen[0]) on [xval >> 5]  |= 1ull << (xval & 31), on_empty  = false;
            else          off[xval >> 5]  |= 1ull << (xval & 31), off_empty = false;
        }
    }

    // Create new logic with one PI only:
    Get_Pob(N, fanout_count);

    int num = attr_PI(int_pi.list()[0]).number;

    // Collect inputs for 'w_dom' and disconnect them:
    Vec<GLit> ins;
    For_Inputs(w_dom, v){
        ins.push(v);
        w_dom.set(Iter_Var(v), Wire_NULL);
    }

    if (on_empty && off_empty)    // -- simple case
        N.change(w_dom, PI_(num));
    else{
        // Build: '~off & (on | PI)'
        N.change(w_dom, Buf_());
        Wire acc = N.add(PI_(num));

        Vec<uint> cover;
        irredSumOfProd(ext_pi.size(), on, cover, false);       // <<== prova "true" och negera resultat om mindre (analogt för off-set)
        for (uint i = 0; i < cover.size(); i++)
            acc = mk_Or(acc, buildCover(N, cover, ext_pi));

        cover.clear();
        irredSumOfProd(ext_pi.size(), off, cover, false);
        for (uint i = 0; i < cover.size(); i++)
            acc = mk_And(acc, ~buildCover(N, cover, ext_pi));

        w_dom.set(0, acc);
    }

    // Remove redundant logic:
    for (uint i = 0; i < ins.size(); i++){
        Wire v = ins[i] + N;
        if (fanout_count[v] == 0)
            removeUnreach(v, false);
    }
}


ZZ_PTimer_Add(reparam_analyze);
ZZ_PTimer_Add(reparam_dom_area);
ZZ_PTimer_Add(reparam_int_pis);
ZZ_PTimer_Add(reparam_ext_pis);


struct ReparamTmps {
    WZet ext_pi;   // External PIs or nodes to be treated as PIs.
    WZet int_pi;   // Dominated PIs.
    WZet area;     // Region to consider. Will be initialized to dominator region.
    WTmpMap<uint>  count;
    WMap<uchar> val;

    ReparamTmps() : count(0) {}

    void clear() {
        ext_pi.clear();
        int_pi.clear();
        area.clear();
        count.clear();
        // no need to clear 'val'
    }
};


static
void reparamRegion(Wire w_dom, const Params_Reparam& P, ReparamTmps& tmps)
{
    NetlistRef N = netlist(w_dom);
    Get_Pob(N, fanout_count);

    WZet& ext_pi = tmps.ext_pi;
    WZet& int_pi = tmps.int_pi;
    WZet& area   = tmps.area;
    WTmpMap<uint>& count = tmps.count;
    tmps.clear();

    #define Add(v)                                          \
    do{                                                     \
        if (type(v) != gate_Flop){                          \
            count(v)++;                                     \
            if (count[v] == fanout_count[v]) area.add(+v);  \
        }                                                   \
    }while(0)

    // Compute dominator area:
    ZZ_PTimer_Begin(reparam_dom_area);
    assert(type(w_dom) != gate_Flop);
    area.add(+w_dom);
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        For_Inputs(w, v)
            Add(v);
        // <<== bound here?
    }
    ZZ_PTimer_End(reparam_dom_area);

    // Extract internal PIs:
    ZZ_PTimer_Begin(reparam_int_pis);
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        if (type(w) == gate_PI)
            int_pi.add(w);
    }
    assert(int_pi.size() > 0);
    ZZ_PTimer_End(reparam_int_pis);

    // Extract external PIs:
    ZZ_PTimer_Begin(reparam_ext_pis);
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        For_Inputs(w, v)
            if (!area.has(v))
                ext_pi.add(+v);
    }

    if (ext_pi.size() > P.cut_width){
        // Remove nodes from 'area' not in the fanout cone of an internal PI:
        for (uind i = area.size(); i > 0;){ i--;
            Wire w = area.list()[i];
            if (!int_pi.has(w)){
                For_Inputs(w, v)
                    if (area.has(v))
                        goto Keep;
                area.exclude(w);
              Keep:;
            }
        }
        area.compact();

        // Re-extract external PIs:
        ext_pi.clear();
        for (uind i = 0; i < area.size(); i++){
            Wire w = area.list()[i];
            For_Inputs(w, v)
                if (!area.has(v))
                    ext_pi.add(+v);
        }
    }
    ZZ_PTimer_End(reparam_ext_pis);

    // Analyze and restructure area:
    if (int_pi.size() + ext_pi.size() <= P.cut_width){
        ZZ_PTimer_Begin(reparam_analyze);
        analyzeRegion(N, area, int_pi, ext_pi, w_dom, P, tmps.val);
        ZZ_PTimer_End(reparam_analyze);
    }
}


void reparam(NetlistRef N, const Params_Reparam& P)
{
    double T0 = cpuTime();

    //**/N.write("N.gig");
    Auto_Pob(N, up_order);
    Auto_Pob(N, fanout_count);

    uintg orig_pis  = N.typeCount(gate_PI);
    uintg orig_ands = N.typeCount(gate_And);

    if (!P.quiet) writeHeader("Reparametrization", 79);

    WMap<gate_id> dom;
    computeDominators(N, dom);

    ReparamTmps tmps;
    WZet cands;
    int percent = -1;
    for (uintg i = 0; i < up_order.size(); i++){
        int p = floor((i * 100.0 + 0.5) / up_order.size());
        if (p != percent){
            percent = p;
            if (!P.quiet) Write "\rProgress: %_ %%\f", p;
        }

        Wire w = N[up_order[i]];
        if (deleted(w)) continue;

        if (type(w) == gate_PI){
            gate_id d0 = id(w);
            gate_id d  = dom[N[d0]];
            while (d != d0){
                if (d < N.size()){
                    cands.add(N[d]);
                    d0 = d;
                    d = dom[N[d0]];
                }else
                    break;
            }

        }else if (cands.has(w))
            reparamRegion(w, P, tmps);
    }
    if (!P.quiet) WriteLn "\rPost-processing...";

    For_Gatetype(N, gate_PI, w)
        if (fanout_count[w] == 0)
            remove(w);

    removeBuffers(N);
    Add_Pob0(N, strash);
    removeAllUnreach(N);

    if (!P.quiet){
        WriteLn "CPU-time: %t", cpuTime() - T0;
        WriteLn "PIs : %_ -> %_", orig_pis , N.typeCount(gate_PI);
        WriteLn "ANDs: %_ -> %_", orig_ands, N.typeCount(gate_And);
    }

    if (par){
        Remove_Pob(N, up_order);
        renumberPIs(N);         // <<== TEMPORARY! Should preserve PI numbers, but PAR doesn't handle it correctly yet.
        Add_Pob0(N, up_order);  // <<== should fix 'Auto_Pob' macro to handle this!
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debugging:


void reparamDebug(NetlistRef N)
{
    N.write("orig.gig");
    reparam(N, Params_Reparam());

    Vec<Wire> props;
    {
        Get_Pob(N, properties);
        append(props, properties);
    }
    int depth = INT_MAX;
    Params_Bmc P;
    P.quant_claus = true;
    lbool result = bmc(N, props, P, NULL, &depth);

    if (result != l_False){
        WriteLn "%%shrink failed%%";
        exit(1);
    }

    Netlist M;
    M.read("orig.gig");
    Vec<Wire> props_M;
    {
        Get_Pob(M, properties);
        append(props_M, properties);
    }
    int depth_M = INT_MAX;
    lbool result_M = bmc(M, props_M, P, NULL, &depth_M, NULL, depth + 1);

    if (result_M != l_False || depth_M != depth){
        WriteLn "%%%%shrink success%%%%";
        exit(2);
    }else{
        WriteLn "%%%%shrink failed%%%%";
        exit(1);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
