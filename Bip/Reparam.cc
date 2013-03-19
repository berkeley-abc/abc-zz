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
#include "ParClient.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Quick but limited reparametrization through local rewrites:


// Returns TRUE if top-element ('order[0]') can be replaced by a primary input, FALSE if it cannot,
// or if the region is to big to analyze.
static
bool analyzeRegion(NetlistRef N, const WZet& area, const WZet& int_pi, const WZet& ext_pi)
{
    // Temporary solution; don't even use bit-parallel simulation:
    if (int_pi.size() + ext_pi.size() > 8)
        return false;

    WMap<uchar> val;
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
        }
        if (!seen[0] || !seen[1])
            return false;
    }

    return true;
}


static
void reparamRegion(Wire w_dom, NetlistRef N_recons)
{
    NetlistRef N = netlist(w_dom);
    Get_Pob(N, fanout_count);

    WZet       ext_pi;   // External PIs or nodes to be treated as PIs.
    WZet       int_pi;   // Dominated PIs.
    WZet       area;     // Region to consider. Will be initialized to dominator region.
    WMap<uint> count(0);

    #define Add(v)                                          \
    do{                                                     \
        if (type(v) != gate_Flop){                          \
            count(v)++;                                     \
            if (count[v] == fanout_count[v]) area.add(+v);  \
        }                                                   \
    }while(0)

    // Compute dominator area:
    assert(type(w_dom) != gate_Flop);
    area.add(+w_dom);
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        For_Inputs(w, v)
            Add(v);
    }

    // Extract internal PIs:
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        if (type(w) == gate_PI)
            int_pi.add(w);
    }
    assert(int_pi.size() > 0);

    // Store current external PIs:
    WZet tmp;
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        For_Inputs(w, v)
            if (!area.has(v))
                tmp.add(+v);
    }

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


    // Extract external PIs:
    for (uind i = 0; i < area.size(); i++){
        Wire w = area.list()[i];
        For_Inputs(w, v)
            if (!area.has(v))
                ext_pi.add(+v);
    }

    if (tmp.size() < ext_pi.size()){
        swp(tmp, ext_pi); }

    // <<== Grow area here to capture more reconvergence

    bool result = analyzeRegion(N, area, int_pi, ext_pi);
    if (result){
        int num = attr_PI(int_pi.list()[0]).number;
        if (N_recons)
            N_recons.add(PO_(num), copyFormula(w_dom, N_recons));   // <<== temporary! Must be more efficient about this!

        For_Inputs(w_dom, v){
            w_dom.set(Iter_Var(v), Wire_NULL);
            if (fanout_count[v] == 0)
                removeUnreach(v);
        }
        N.change(w_dom, PI_(num));
    }
}


// Will reparametrize 'N' and optionally output a reconstruction AIG 'N_recons' needed
// to translate potential counterexamples.
void reparam(NetlistRef N, const Params_Reparam& P, NetlistRef N_recons)
{
    Auto_Pob(N, up_order);
    Auto_Pob(N, fanout_count);

    N_recons.clear();
    Add_Pob0(N_recons, strash);

    uintg orig_pis  = N.typeCount(gate_PI);
    uintg orig_ands = N.typeCount(gate_And);

    if (!P.quiet) writeHeader("Reparametrization", 79);

    WMap<gate_id> dom;
    computeDominators(N, dom);

    WZet cands;
    for (uintg i = 0; i < up_order.size(); i++){
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
            reparamRegion(w, N_recons);
    }

    For_Gatetype(N, gate_PI, w)
        if (fanout_count[w] == 0)
            remove(w);

    if (!P.quiet){
        WriteLn "PIs : %_ -> %_", orig_pis , N.typeCount(gate_PI);
        WriteLn "ANDs: %_ -> %_", orig_ands, N.typeCount(gate_And);
    }

    if (par){
        Remove_Pob(N, up_order);
        renumberPIs(N);         // }- <<== TEMPORARY! Should preserve PI numbers, but PAR doesn't handle it correctly yet.
        renumberPIs(N_recons);  // }
        sendMsg_Reparam(N, N_recons);
        Add_Pob0(N, up_order);  // <<== should fix 'Auto_Pob' macro to handle this!
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counterexamlpe reconstruction:


// 'N' is the reparametrized netlist, 'M' the reconstruction netlist.
void reconstructCex(NetlistRef M, CCex& cex)  // + output netlist!
{
    // Temporary! Clean up netlist (until we've fixed AIGER parser/writer):
    {
        Auto_Pob(M, fanout_count);
        For_Gatetype(M, gate_PI, w)
            if (fanout_count[w] == 0)
                remove(w);
        For_Gatetype(M, gate_PO, w)
            if (type(w[0]) == gate_Const)
                remove(w);
    }

    // Setup clausification:
    SatStd    S;
    WMap<Lit> m2s;
    WZet      keep;
    Clausify<SatStd> C(S, M, m2s, keep);
    Auto_Pob(M, fanout_count);
    For_Gates(M, w)
        if (fanout_count[w] > 1)
            keep.add(w);

    // Reconstruct counterexample:
    for (uind d = 0; d < cex.size(); d++){
        Vec<Lit> assumps;
        For_Gatetype(M, gate_PO, w){
            int   num = attr_PO(w).number;
            lbool val = cex.inputs[d][num];
            assert(val == l_False || val == l_True);
            Lit   p   = C.clausify(w);
            assumps.push(p ^ (val == l_False));
        }

        lbool result = S.solve(assumps); assert(result == l_True);

        For_Gatetype(M, gate_PI, w){
            Lit   p   = C.clausify(w);
            lbool val = S.value(p);
            cex.inputs[d](attr_PI(w).number) = (val != l_Undef) ? val : l_False;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
