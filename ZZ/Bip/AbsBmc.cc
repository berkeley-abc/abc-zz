//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : AbsBmc.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Under-approximate abstractions for BMC.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "AbsBmc.hh"
#include "Pdr.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Key2Index_FlopNum {
    typedef Wire Key;

    uind operator()(const Wire& w) const {
        assert(type(w) == gate_Flop);
        int num = attr_Flop(w).number;
        assert(num >= 0);
        return num;
    }
};

typedef IntZet<Wire, Key2Index_FlopNum> FlopSet;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// 'abstr' contains the set of *concrete* flops.
void createDualRailModel(NetlistRef N, const FlopSet& abstr, /*in+out*/Vec<Wire>& props, /*out*/uint& hi_flop_offset, /*out*/NetlistRef M)
{
    // Setup dual-rail encoded netlist 'M':
    Assure_Pob0(M, strash);

    Add_Pob(M, flop_init);
    Add_Pob(M, reset);
    uint reset_num = nextNum_Flop(N);
    reset = M.add(Flop_(reset_num), ~N.True());
    flop_init(reset) = l_True;

    hi_flop_offset = reset_num + 1;

    WMap<Pair<GLit,GLit> > n2m;
    n2m(N.True ()) = make_tuple( M.True(),  M.True());
    n2m(N.False()) = make_tuple(~M.True(), ~M.True());

    // Copy logic from 'N' to 'M':
    Vec<Pair<GLit,GLit> > flops;
    Auto_Pob(N, up_order);
    Get_Pob2(N, flop_init, flop_init_N);
    For_UpOrder(N, w){
        switch (type(w)){
        case gate_PI:{
            Wire m = M.add(PI_(attr_PI(w).number));
            n2m(w) = make_tuple(m, m);
            break;}

        case gate_Flop:
            if (abstr.has(w)){
                int num = attr_Flop(w).number;
                Wire flop_lo = M.add(Flop_(num));
                Wire flop_hi = M.add(Flop_(num + hi_flop_offset));
                flops(num) = make_tuple(flop_lo.lit(), flop_hi.lit());

                lbool init = flop_init_N[w];
                flop_init(flop_lo) = init;
                flop_init(flop_hi) = init;
                if (init == l_Undef){
                    Wire pseudo = M.add(PI_());
                    flop_lo = s_Mux(reset, pseudo, flop_lo);    // -- must make sure both rails are initialized to the same random value
                    flop_hi = s_Mux(reset, pseudo, flop_hi);
                }
                n2m(w) = make_tuple(flop_lo, flop_hi);

            }else{
                // <<== blanda in 'reset' här...
                n2m(w) = make_tuple(~M.True(), M.True());
            }
            break;

        case gate_And:{
            Wire m0_lo = M[ n2m[w[0]].fst ^ sign(w[0]) ];
            Wire m0_hi = M[ n2m[w[0]].snd ^ sign(w[0]) ];
            if (sign(w[0])) swp(m0_lo, m0_hi);
            Wire m1_lo = M[ n2m[w[1]].fst ^ sign(w[1]) ];
            Wire m1_hi = M[ n2m[w[1]].snd ^ sign(w[1]) ];
            if (sign(w[1])) swp(m1_lo, m1_hi);

            n2m(w) = make_tuple(s_And(m0_lo, m1_lo), s_And(m0_hi, m1_hi));
            break;}

        case gate_PO:{
            // Convert property from dual-rail to single-rail (0 -> 0, 1/X -> 1)
            Wire m = sign(w[0]) ? ~M[n2m[w[0]].fst] : M[n2m[w[0]].snd];
            Wire m_hi = M.add(PO_(attr_PO(w).number), m);
            n2m(w) = make_tuple(m_hi, m_hi);
            break;}

        default:
            ShoutLn "INTERNAL ERROR! Unhandled gate type: %_", GateType_name[type(w)];
            assert(false);
        }
    }

    // Tie flops together:
    For_Gatetype(N, gate_Flop, w){
        if (abstr.has(w)){
            int num = attr_Flop(w).number;
            Wire m_lo  = M[ flops[num].fst ];
            Wire m_hi  = M[ flops[num].snd ];
            Wire m0_lo = M[ n2m[w[0]].fst ^ sign(w[0]) ];
            Wire m0_hi = M[ n2m[w[0]].snd ^ sign(w[0]) ];
            if (sign(w[0])) swp(m0_lo, m0_hi);

            m_lo.set(0, m0_lo);
            m_hi.set(0, m0_hi);
        }
    }

    // Create 'properties' POB:
    Add_Pob(M, properties);
    Get_Pob2(N, properties, properties_N);
    for (uint i = 0; i < properties_N.size(); i++){
        Wire w = properties_N[i];
        Wire m = M[n2m[w].fst] ^ sign(w);        // Both X and 1 counts as TRUE.
        properties.push(m);
    }

    // Update 'props':
    for (uint i = 0; i < props.size(); i++){
        Wire w = props[i];
        props[i] = M[n2m[w].fst] ^ sign(w);
    }

    removeAllUnreach(M);
}

/*
TODO:

 - Make abstract flops be 0 not X in the first cycle (use 'reset')
 - Check for s = s^; should happen if no X in transitive fanin
*/


lbool uabsPdr(NetlistRef N, const Vec<Wire>& props0, const FlopSet& abstr, uint& hi_flop_offset,
   /*output*/ Cex& cex, NetlistRef M_invar)

{
    Netlist M;
    Vec<Wire> props(copy_, props0);
    createDualRailModel(N, abstr, props, hi_flop_offset, M);

    /**/WriteLn "N: %_", info(N);
    /**/WriteLn "M: %_", info(M);
    //**/Dump(props);
    //**/M.write("M.gig"); WriteLn "Wrote: \a*M.gig\a*";

    Params_Pdr P;
    int        bug_free_depth;
    P.quiet = true;
    double T0 = cpuTime();
    lbool result = propDrivenReach(M, props, P, &cex, M_invar, &bug_free_depth);

    // Debug:
    WriteLn "CPU time: %t", cpuTime() - T0;
    if (result == l_False)
        WriteLn "CEX length: %_", cex.depth();
    else
        WriteLn "Property proved";

    return result;
}


lbool uabsPdr(NetlistRef N, const Vec<Wire>& props0, const FlopSet& abstr)
{
    uint       hf_offset;
    Cex        cex;
    Netlist    M_invar;
    return uabsPdr(N, props0, abstr, hf_offset, cex, M_invar);
}


// 'props' are POs in 'M'.
void uabsRefine(NetlistRef M, const Vec<Wire>& props, /*in+out*/FlopSet& abstr, uint hi_flop_offset, NetlistRef M_invar)
{
    /*DEBUG*/
    WriteLn "--------------------";
    nameByCurrentId(M); M.write("M.gig");
    nameByCurrentId(M_invar); M_invar.write("M_invar.gig");
    WriteLn "M props: %_", props;
    Write "Abstr:";
    For_Gatetype(M, gate_Flop, w){
        if (abstr.has(w))
            Write " ff:%_",attr_Flop(w).number;
    }
    NewLine;
    /*END*/


    SatStd S;
    WMap<Lit> i2s;
    WMap<Lit> h2s;
    WMap<Lit> m2s;
    Clausify<SatStd> CI(S, M_invar, i2s);   // -- invariant in second frame
    Clausify<SatStd> CH(S, M_invar, h2s);   // -- invariant in first frame
    Clausify<SatStd> CM(S, M, m2s);
    Vec<Lit> target;

    // Collect flops of 'M':
    Vec<Wire> ffs;
    For_Gatetype(M, gate_Flop, w){
        int num = attr_Flop(w).number;
        ffs(num, Wire_NULL) = w;
    }

    // Insert invariant in second frame:
    assert(M_invar.typeCount(gate_PO) == 1);
    assert(M_invar.typeCount(gate_PI) == 0);
    For_Gatetype(M_invar, gate_PO, w)
        target.push(~CI.clausify(w));

    // Insert invariant fanin in first frame:
    For_Gatetype(M_invar, gate_Flop, w){
        Lit p = i2s[w];
        if (p != lit_Undef){
            int num = attr_Flop(w).number;
            assert(ffs[num] != Wire_NULL);
            Lit q = CM.clausify(ffs[num][0]);
            S.addClause(~p, q);     // -- tie frames together
            S.addClause(~q, p);
        }
    }

    // Add (and assert) invariant in first frame:
    For_Gatetype(M_invar, gate_PO, w)
        S.addClause(CH.clausify(w));

    // Tie first frame invariant to flops of M:
    For_Gatetype(M_invar, gate_Flop, w){
        Lit p = h2s[w];
        if (p != lit_Undef){
            int num = attr_Flop(w).number;
            assert(ffs[num] != Wire_NULL);
            Lit q = CM.clausify(ffs[num]);
            S.addClause(~p, q);
            S.addClause(~q, p);
        }
    }

    // Insert properties in first frame:
    for (uint i = 0; i < props.size(); i++)
        target.push(~CM.clausify(props[i]));

    // Add 's_lo -> s_hi' constraints:
    For_Gatetype(M, gate_Flop, w){
        int num = attr_Flop(w).number;
        if ((uint)num < hi_flop_offset){
            Wire w_hi = ffs[num + hi_flop_offset];
            if (w_hi != Wire_NULL){
                Lit p = m2s[w];
                Lit q = m2s[w_hi];
                if (p != lit_Undef && q != lit_Undef)
                    S.addClause(~p, q);
            }
        }
    }

    // Find a counterexample that violates the invariant (or property):
    S.addClause(target);
    lbool result ___unused = S.solve();

    // Scan:
    Vec<uint> cands;
    For_Gatetype(M, gate_Flop, w){
        if (abstr.has(w)) continue;

        int num = attr_Flop(w).number;
        if ((uint)num < hi_flop_offset){
            Wire w_hi = ffs[num + hi_flop_offset];
            if (w_hi != Wire_NULL){
                Lit p = m2s[w];
                Lit q = m2s[w_hi];
                if (p != lit_Undef && S.value(p) == l_True){
                    WriteLn "  -- candidate TRUE flop[%_]", num;
                    cands.push(num); }
                if (q != lit_Undef && S.value(q) == l_False){
                    WriteLn "  -- candidate FALSE flop[%_]", num;
                    cands.push(num); }
            }
        }
    }

    if (cands.size() == 0){
        //Dump(result);
        ShoutLn "INTERNAL ERROR! Could not refine counterexampe (property true?)";
        /*DEBUG*/
        NewLine;
        WriteLn "Model of M:";
        For_Gates(M, w){
            Lit p = m2s[w];
            if (p)
                WriteLn "  %_ = %_", w, S.value(m2s[w]) ^ sign(m2s[w]);
        }
        /*END*/
        exit(1);

        /*HACK*/
        Vec<Wire> fs;
        For_Gatetype(M, gate_Flop, w){
            int num = attr_Flop(w).number;
            if ((uint)num < hi_flop_offset && !abstr.has(w)){
                //**/Dump(w);
                abstr.add(w);
                return;
            }
        }
        /*END*/

        exit(1);
    }

  #if 1
    abstr.add(ffs[cands[0]]);
  #else
    for (uint i = 0; i < cands.size(); i++)
        abstr.add(ffs[cands[i]]);
  #endif
}


void absBmc(NetlistRef N, const Vec<Wire>& props)
{
    //**/addReset(N, nextNum_Flop(N)); removeAllUnreach(N);

    // Create a full dual-rail encoding for refinement:
    Netlist M_full;
    FlopSet abstr_full;
    uint hf_offset_full;
    Vec<Wire> props_full(copy_, props);
    For_Gatetype(N, gate_Flop, w)
        abstr_full.add(w);
    createDualRailModel(N, abstr_full, props_full, hf_offset_full, M_full);

    // Abstraction refinement loop:
    FlopSet abstr;
    for(;;){
        uint       hf_offset;
        Cex        cex;
        Netlist    M_invar;
        lbool result = uabsPdr(N, props, abstr, hf_offset, cex, M_invar);
        assert(hf_offset == hf_offset_full);

        if (result == l_False){
            WriteLn "Counterexample found.";
            // <<== verify it here
            return;
        }

        uabsRefine(M_full, props_full, abstr, hf_offset, M_invar);
    }

#if 0
    // Start from full abstraction:
    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number; assert(num >= 0);
        abstr.add(w);
    }

    // Expensive reduction to minimal abstraction:
    lbool result = uabsPdr(N, props, abstr);
    if (result != l_False){
        WriteLn "Original problem has no CEX. Aborting.";
        return; }

    WriteLn "\a/Original abstraction size: \a*%_\a0", abstr.size();
    For_Gatetype(N, gate_Flop, w){
        abstr.exclude(w);
        lbool result = uabsPdr(N, props, abstr);
        if (result == l_True)
            abstr.add(w);
        else
            WriteLn "\a/Current abstraction size: \a*%_\a0", abstr.size();
    }
#endif
}


    //**/if (i != 10 && i != 11 && i != 12 && i != 13 && i != 14)   // ibm001.aig


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
