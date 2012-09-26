//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Flatten.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Flatten a hierarchical circuit.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Flatten.hh"
#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Gate literals in 'pis' and 'pos' are from 'N_flat'.
static
void flatten(uint mod, const Vec<VerilogModule>& modules, NetlistRef N_flat,
             const Vec<GLit>& pis, Vec<GLit>& pos, Vec<char>& name_prefix,
             const Params_Flatten& P)
{
    NetlistRef N = modules[mod].netlist;

    assert(pos.size() == 0);

    WWMap xlat;
    xlat(N.True ()) =  N_flat.True();
    xlat(N.False()) = ~N_flat.True();

    WMap<uint>      uif_idx(UINT_MAX);
    Vec<Vec<GLit> > uif_pos;

    Vec<Pair<GLit,GLit> > backpatch;    // -- list of '(buffer in N_flat, input in N)'.
    Vec<char> nambuf;

    removeAllUnreach(N);
    Assure_Pob(N, up_order);
    For_UpOrder(N, w){
        switch (type(w)){
        case gate_And:
            xlat(w) = N_flat.add(And_(), xlat[w[0]], xlat[w[1]]);
            break;

        case gate_Xor:
            if (!P.strict_aig)
                xlat(w) = N_flat.add(Xor_(), xlat[w[0]], xlat[w[1]]);
            else
                xlat(w) = mk_Xor(xlat[w[0]] + N_flat, xlat[w[1]] + N_flat);
            break;

        case gate_Mux:
            if (!P.strict_aig)
                xlat(w) = N_flat.add(Mux_(), xlat[w[0]], xlat[w[1]], xlat[w[2]]);
            else
                xlat(w) = mk_Mux(xlat[w[0]] + N_flat, xlat[w[1]] + N_flat, xlat[w[2]] + N_flat);
            break;

        case gate_PI:{
            int num = attr_PI(w).number;
            if (num == num_NULL)
                xlat(w) = N_flat.add(PI_());
            else{
                assert((uint)num < pis.size());
                xlat(w) = pis[num];
            }
            break;}

        case gate_PO:{
            int num = attr_PO(w).number;
            pos(num, glit_NULL) = xlat[w[0]];
            break;}

        case gate_Uif:{
            uint submod = attr_Uif(w).sym;
            uint idx = uif_pos.size();
            uif_idx(w) = idx;
            uif_pos.push();

            Vec<GLit> uif_pis(reserve_, w.size());
            For_Inputs(w, v){
                GLit x = xlat[v];
                if (!+x){
                    x = N_flat.add(Buf_());
                    backpatch.push(tuple(x, v));
                }
                uif_pis.push(x);
            }

            if (modules[submod].black_box || (modules[submod].verific_op && P.blackbox_verific)){
                // Keep UIF gate:
                Wire x_uif = N_flat.add(Uif_(submod), w.size());
                if (P.store_names == 2)
                    migrateNames(w, x_uif, name_prefix.slice());
                xlat(w) = x_uif;

                for (uint i = 0; i < w.size(); i++)
                    x_uif.set(i, uif_pis[i]);

            }else{
                // Expand UIF gate:
                uint pfx_sz = name_prefix.size();
                append(name_prefix, slize(N.names().get(w, N.names().scratch)));
                name_prefix.push(P.hier_sep);

                flatten(submod, modules, N_flat, uif_pis, uif_pos.last(), name_prefix, P);
                name_prefix.shrinkTo(pfx_sz);

                for (uint i = 0; i < uif_pos.last().size(); i++){
                    assert(+uif_pos.last()[i]); }
            }
            break;}

        case gate_Pin:{
            uint idx = uif_idx[w[0]];
            int  num = attr_Pin(w).number;
            if (!uif_pos[idx](num, glit_NULL)){
                GLit x_uif = xlat[w[0]]; assert(+x_uif);
                uif_pos[idx][num] = N_flat.add(Pin_(num), x_uif);
            }
            xlat(w) = uif_pos[idx][num];
            break;}

        default: assert(false); }

        // Store names:
        if (P.store_names == 2){
            if (type(w) != gate_PO && type(w) != gate_Uif)
                migrateNames(w, xlat[w] + N_flat, name_prefix.slice());
        }
    }

    // Store name of constants:
    if (P.store_names == 2){
        migrateNames(N.True (),  N_flat.True(), name_prefix.slice());
        migrateNames(N.False(), ~N_flat.True(), name_prefix.slice());
    }

    // Backpatch:
    for (uint i = 0; i < backpatch.size(); i++){
        Wire x = N_flat[backpatch[i].fst];
        x.set(0, xlat[backpatch[i].snd]);
    }
}


// Returns ID of the top-module or UINT_MAX if no unique top (leaving 'N_flat' untouched).
uint flatten(const Vec<VerilogModule>& modules, NetlistRef N_flat, const Params_Flatten& P)
{
    assert(N_flat.empty());

    // Determine top module:
    Vec<uchar> seen;
    for (uint i = 0; i < modules.size(); i++){
        NetlistRef N = modules[i].netlist;
        if (N){
            For_Gatetype(N, gate_Uif, w){
                seen(attr_Uif(w).sym, 0) = 1; }
        }
    }

    uint top = UINT_MAX;
    uint nl_size = 0;
    bool have_warned = false;
    for (uint i = 0; i < modules.size(); i++){
        if (!seen(i, 0) && !modules[i].black_box){
            if (newMax(nl_size, modules[i].netlist.gateCount())){
                if (top != UINT_MAX && !have_warned){
                    WriteLn "WARNING! Top module not uniquely defined. Guessing the largest.";
                    have_warned = true;
                }
                top = i;        // -- guess at the largest netlist...
            }
        }
    }
    if (top == UINT_MAX){
        return UINT_MAX; }

    // Recursively flatten top-level module:
    NetlistRef N = modules[top].netlist;
    Vec<GLit> pis;
    For_Gatetype(N, gate_PI, w){
        int num = attr_PI(w).number;
        if (num != num_NULL){
            assert((uint)num < N.typeCount(gate_PI));
            pis(num, glit_NULL) = N_flat.add(PI_(num));
            if (P.store_names == 1)
                migrateNames(w, pis[num] + N_flat);
        }
    }

    Vec<char> name_prefix;
    Vec<GLit> pos;
    flatten(top, modules, N_flat, pis, pos, name_prefix, P);
    assert(pos.size() == N.typeCount(gate_PO));

    For_Gatetype(N, gate_PO, w){
        int num = attr_PO(w).number; assert((uint)num < N.typeCount(gate_PO));
        Wire x = N_flat.add(PO_(num), N_flat[pos[num]]);
        if (P.store_names != 0)
            migrateNames(w, x);
    }

    // Cleanup: ('removeAllUnreach()' won't do because Uif's are global sinks but we want only what is reachable from POs)
    WZet mark;
    For_Gatetype(N_flat, gate_PO, w)
        mark.add(w);
    for (uint i = 0; i < mark.size(); i++){
        Wire w = mark.list()[i] + N_flat;
        For_Inputs(w, v)
            mark.add(v);
    }
    For_Gates(N_flat, w){
        if (!mark.has(w)){
            For_Inputs(w, v)
                w.set(Iter_Var(v), Wire_NULL);
        }
    }
    For_Gates(N_flat, w)
        if (!mark.has(w))
            remove(w);

    // Remove buffers and Pin's for single output Uifs:
    For_Gatetype(N_flat, gate_Pin, w){
        assert(type(w[0]) == gate_Uif);
        uint n_out_pins = modules[attr_Uif(w[0]).sym].out_gate.size();
        if (n_out_pins == 1)
            N_flat.change(w, Buf_(), w[0]);
    }

    if (!removeBuffers(N_flat))
        Throw(Excp_Msg) "Combinational cycle detected among buffers.";

    return top;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
