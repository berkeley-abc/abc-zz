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

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Quick but limited reparametrization through local rewrites:


#if 0
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
#else


// NOTE! New procedure doesn't work with reconstruction

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
//                conj = mk_And(conj, w);
            }
        }
        disj = (disj == ~glit_True) ? conj : mk_Or(disj, conj);
//        disj = mk_Or(disj, conj);
    }
    return disj;
}


static
void analyzeRegion(NetlistRef N, const WZet& area, const WZet& int_pi, const WZet& ext_pi, Wire w_dom, NetlistRef N_recons)
{
    // Temporary solution; don't even use bit-parallel simulation:
    if (int_pi.size() + ext_pi.size() > 8)
        return;

    WMap<uchar> val;
    uint64 on = 0, off = 0;
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
        if (!seen[0] || !seen[1]){
            if (int_pi.size() == 1)
                return;     // -- no reduction in #PIs possible, stop early
            if (!seen[0]) on  |= 1ull << xval;
            else          off |= 1ull << xval;
        }
    }

    // Create new logic with one PI only:
    Get_Pob(N, fanout_count);

    int num = attr_PI(int_pi.list()[0]).number;
    if (N_recons)
        N_recons.add(PO_(num), copyFormula(w_dom, N_recons));   // <<== temporary! Must be more efficient about this!

  #if 0
    if (pivot){
        // Print on/off set and save area to dot file:
        String filename = "area.dot";
        Write "%_:  (int/ext = %_/%_)  on/off=", filename, int_pi.size(), ext_pi.size();
        for (uint i = 0; i < (1u << ext_pi.size()); i++){
            if      (on  & (1ull << i)) Write "1";
            else if (off & (1ull << i)) Write "0";
            else                        Write ".";
        }
        NewLine;
        writeDot(filename, N, area);
    }
  #endif

    // Collect inputs for 'w_dom' and disconnect them:
    Vec<GLit> ins;
    For_Inputs(w_dom, v){
        ins.push(v);
        w_dom.set(Iter_Var(v), Wire_NULL);
    }

    if (on == 0 && off == 0)    // -- simple case
        N.change(w_dom, PI_(num));
    else{
        // Build: '~off & (on | PI)'
        assert(ext_pi.size() <= 6);

#if 0
        /**/if (pivot){
            Write "Internal PIs:"; for (uind i = 0; i < int_pi.size(); i++) Write " %_", int_pi.list()[i]; NewLine;
            Write "External PIs:"; for (uind i = 0; i < ext_pi.size(); i++) Write " %_", ext_pi.list()[i]; NewLine;
        }
#endif

        N.change(w_dom, Buf_());
        Wire acc = N.add(PI_(num));
        //**/Wire new_pi = acc;

        Vec<uint> cover;
        Vec<uint> ftb(ext_pi.size() == 6 ? 2 : 1);

        ftb[0] = on;
        if (ftb.size() == 2) ftb[1] = on >> 32;
        irredSumOfProd(ext_pi.size(), ftb, cover, false);       // <<== prova "true" och negera resultat om mindre (analogt för off-set)
        for (uint i = 0; i < cover.size(); i++)
            acc = mk_Or(acc, buildCover(N, cover, ext_pi));

        cover.clear();
        ftb[0] = off;
        if (ftb.size() == 2) ftb[1] = off >> 32;
        irredSumOfProd(ext_pi.size(), ftb, cover, false);
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
#endif


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

    // Analyze and restructure area:
    analyzeRegion(N, area, int_pi, ext_pi, w_dom, N_recons);
}


// Will reparametrize 'N' and optionally output a reconstruction AIG 'N_recons' needed
// to translate potential counterexamples.
void reparam(NetlistRef N, const Params_Reparam& P, NetlistRef N_recons)
{
    Auto_Pob(N, up_order);
    Auto_Pob(N, fanout_count);

    if (N_recons){
        N_recons.clear();
        Add_Pob0(N_recons, strash);
    }

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

    removeBuffers(N);
    Add_Pob0(N, strash);
    removeAllUnreach(N);

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
