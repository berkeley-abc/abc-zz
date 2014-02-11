//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Live.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Simple liveness checker based on the Biere and Claessen/Sorensson methods.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//| A liveness counterexample is a lasso-shaped trace 's[0], s[1], ..., s[k], s[k+1], ..., s[n]'
//| where 's[k] == s[n]' and every fairness constraint 'f[0], ..., f[m-1]' holds at least once
//| along the lasso-part.
//|
//| We first reduce the fairness constraints to a single constraint by adding a flop for each
//| constraint to remember if we have seen it yet, starting to monitor this when 'fair_mon' goes
//| high:
//|
//|    init(seen_f_already[i]) = 0
//|    seen_f[i] = (seen_f_already[i] | f[i]) & fair_mon
//|
//|    next(seen_f_already[i]) = seen_f[i] & ~seen_all
//|
//|    seen_all = seen_f[0] & seen_f[1] & ... & seen_f[m-1]
//|
//| (this is done in 'initBmcNetlist()' in 'ZZ/Bip/Common/Common.cc').
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Live.hh"
#include "ZZ_Bip.Common.hh"
#include "Pdr2.hh"
#include "Treb.hh"
#include "Bmc.hh"
#include "Imc.hh"
#include "ParClient.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Assumes on fairness property and no fairness constraints
bool verifyInfCex(NetlistRef N, Cex& cex, Wire loop_start, /*out*/uint* out_loop_frame)
{
    Get_Pob(N, fair_properties);    // <<== ah, funkar inte. lever i N0 inte N

    // Collect fairness signals:
    Vec<Wire> observe;
    append(observe, fair_properties[0]);
    observe.push(loop_start);       // -- not a fairness signal; just to find the loop

    // Fill in the flops of 'cex':
    Vec<Vec<lbool> > obs_val;
    Vec<Wire> empty;
    verifyCex(N, empty, cex, NULL, &observe, &obs_val);

    // Extract loop frame and check that all fairness signals happen in loop:
    uint loop_frame = UINT_MAX;
    Vec<bool> seen(observe.size() - 1, false);
    for (uint d = 0; d < cex.size(); d++){
        if (obs_val[d].last() == l_True && loop_frame == UINT_MAX)
            loop_frame = d;

        if (loop_frame != UINT_MAX){
#if 0   /*DEBUG*/
            Write "Observed %_: ", d;
            for (uint i = 0; i < obs_val[d].size() - 1; i++)
                Write "%_", obs_val[d][i];
            NewLine;
#endif  /*END DEBUG*/
            for (uint i = 0; i < obs_val[d].size() - 1; i++)
                if (obs_val[d][i] == l_True)
                    seen[i] = true;
        }
    }

    if (out_loop_frame) *out_loop_frame = loop_frame;

    for (uint i = 0; i < seen.size(); i++)
        if (!seen[i]){
            /**/WriteLn "=> missing fairness signal %_ in cycle (starting at %_)", i, loop_frame;
            return false; }

    // <<== also verify that loop state and last state are equal? What about COI?

    return loop_frame != UINT_MAX;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
void tidyUp(NetlistRef N)
{
    Get_Pob(N, properties);
    Get_Pob(N, fair_properties);

    // Number flops:
    int numC = nextNum_Flop(N);
    For_Gatetype(N, gate_Flop, w)
        if (attr_Flop(w).number == num_NULL)
            attr_Flop(w).number = numC++;

    // Clean up:
    For_Gatetype(N, gate_PO, w)
        if (w != properties[0] && !has(fair_properties[0], w))
            remove(w);

    removeBuffers(N);
    removeAllUnreach(N);
}


void liveToSafe(NetlistRef N, const Params_Liveness& P, int n_orig_flops, Wire fair_mon, Wire& loop_start)
{
    assert(!Has_Pob(N, constraints));       // -- should have been folded into fairness monitor already

    Get_Pob(N, flop_init);
    Get_Pob(N, init_bad);       // -- 'init_bad[1]' is a PO which will go high if all fairness properties have been seen
    Assure_Pob(N, properties);

    // Add shadow flops:
    Vec<Pair<GLit, GLit> > ff_pairs;

    For_Gatetype(N, gate_Flop, w){
        int num = attr_Flop(w).number;
        if (num != num_NULL && num < n_orig_flops){
            Wire ws = N.add(Flop_());     // -- implicitly initialized to 'l_Undef' (which is what we want)
            ws.set(0, ws);
            ff_pairs.push(tuple(w, ws));
        }
    }

    // Compare shadow flops to current state to generate 'match' signal:
    Wire match = N.True();
    for (uint i = 0; i < ff_pairs.size(); i++)
        match = s_And(match, s_Equiv(ff_pairs[i].fst + N, ff_pairs[i].snd + N));
    /**/N.names().add(match, "match");

    // Produce 'watching' signal (and feed it to 'fair_mon'):
    Wire was_watching = N.add(Flop_());
    flop_init(was_watching) = l_False;
    Wire watching = s_Or(was_watching, match);
    was_watching.set(0, watching);
    fair_mon.set(0, watching);
    /**/N.names().add(was_watching, "was_watching");
    /**/N.names().add(watching, "watching");
    /**/N.names().add(fair_mon, "fair_mon");

    // Has seen all fairness constraints?
#if 0
    Wire has_seen_all = N.add(Flop_());         // <<== add flag to initBmcNetlist() instead? this flop is redundant
    flop_init(has_seen_all) = l_False;
    Wire seen_all = s_Or(init_bad[1][0], has_seen_all);
    has_seen_all.set(0, seen_all);
    /**/N.names().add(has_seen_all, "has_seen_all");
    /**/N.names().add(seen_all, "seen_all");
#else
//    Wire has_seen_all = init_bad[1][0];
    Wire has_seen_all = N.add(Flop_(), init_bad[1][0]);
    flop_init(has_seen_all) = l_False;
#endif

    // If was watching and seen all and match => CEX:
    Remove_Pob(N, init_bad);
    properties.clear();
    properties.push(N.add(PO_(), ~s_And(was_watching, s_And(has_seen_all, match))));

    tidyUp(N);

    loop_start = match;
}


lbool kLive(NetlistRef N, const Params_Liveness& P, Wire fair_mon, uint k, bool incremental)
{
    assert(!Has_Pob(N, constraints));       // -- should have been folded into fairness monitor already

    Get_Pob(N, flop_init);
    Get_Pob(N, init_bad);       // -- 'init_bad[1]' is a PO which will *toggle* every time all fairness properties have been seen
    Assure_Pob(N, properties);

    fair_mon.set(0, N.True());

    Vec<Wire> ts;
    ts(0) = init_bad[1][0];

    for (uint i = 1; i <= k; i++){
        Wire b = N.add(Flop_());
        flop_init(b) = l_False;
        b.set(0, s_Or(b, ts[i-1]));
        ts(i) = s_And(b, ts[i-1]);
    }

    // If toggled 'k' times:
    Remove_Pob(N, init_bad);
    properties.clear();
    properties.push(N.add(PO_(), ~ts[k]));

    tidyUp(N);

    // Incremental:
    if (incremental){
        Vec<Wire> props(1, properties[0]);
        Params_Pdr2 P;
        P.check_klive = true;
        //P.sat_solver = sat_Msc;
        //P.pob_internals = true;
        P.restarts = true;
        /**/P.prop_init = true;

        Cex cex;
        Netlist N_invar;
        bool result = pdr2(N, props, P, &cex, N_invar);

        WriteLn "K-live result: %_", result;

        return lbool_lift(result);
    }

    return l_Undef;
}


lbool liveness(NetlistRef N0, uint fair_prop_no, const Params_Liveness& P, Cex* out_cex, uint* out_loop)
{
    Get_Pob(N0, fair_properties);
    Auto_Pob(N0, fair_constraints);
    Vec<uint> par_props(1, 0);      // -- for now, can only handle singel properties in PAR mode

    Vec<Wire> fairs;
    append(fairs, fair_properties[fair_prop_no]);
    append(fairs, fair_constraints);

    WMap<Wire> xlat;
    Netlist N;
    int     n_orig_flops ___unused = nextNum_Flop(N0); // -- don't introduce shadow registers for liveness monitor flops
    Wire    fair_mon;
    Wire    loop_start = Wire_ERROR;

#if 0
    bool toggle_bad = (P.k != Params_Liveness::L2S);
    initBmcNetlist(N0, fairs, N, true, xlat, &fair_mon, toggle_bad);
#else
    initBmcNetlist(N0, fairs, N, true, xlat, &fair_mon, true);
#endif

    if (P.k == Params_Liveness::L2S){
        liveToSafe(N, P, n_orig_flops, fair_mon, /*out*/loop_start);

    }else{
        if (P.k == Params_Liveness::INC){
            // Incremental:
            lbool ret = kLive(N, P, fair_mon, 0, true);      // <<== use 'eng' parameter here
            if (ret == l_Undef){
                WriteLn "LIVENESS: \a*Inconclusive.\a*";
                if (par) sendMsg_Result_unknown(par_props, 2/*liveness property*/);

            }else{ assert(ret == l_True);
                WriteLn "LIVENESS: \a*No witness exists.\a*";
                if (par) sendMsg_Result_holds(par_props, 2/*liveness property*/);
            }
            return ret;         // -- EXIT POINT!

        }else
            kLive(N, P, fair_mon, P.k, false);
    }

    if (P.gig_output != ""){
        N.write(P.gig_output);
        WriteLn "Wrote: \a*%_\a*", P.gig_output;
    }

    if (P.aig_output != ""){
        WriteLn "AIGER writing not supported yet.";
        //writeAigerFile(P.aig_output, N);    // <<== have to support AIGER 1.9 first...
        //WriteLn "Wrote: \a*%_\a*", P.aig_output;
    }

    // Run safety engine on conversion:
    Get_Pob(N, properties);
    Vec<Wire> props(1, properties[0]);

    Cex     cex;
    int     bug_free_depth;

    Params_Treb P_treb;
    Params_Pdr2 P_pdr2;
    Params_Bmc  P_bmc;
    P_treb.par_send_result = false;
    P_pdr2.par_send_result = false;
    P_bmc .par_send_result = false;

    lbool ret;
    switch (P.eng){
    case Params_Liveness::eng_NULL:
        ret = l_Undef;
        break;

    case Params_Liveness::eng_Bmc:
        ret = bmc(N, props, P_bmc, &cex, &bug_free_depth, NULL, P.bmc_max_depth);
        break;

    case Params_Liveness::eng_Treb:
        ret = treb(N, props, P_treb, &cex, Netlist_NULL, &bug_free_depth, NULL);
        break;

    case Params_Liveness::eng_TrebAbs:
        P_treb.use_abstr = true;
        P_treb.restart_lim = 100;
        ret = treb(N, props, P_treb, &cex, Netlist_NULL, &bug_free_depth, NULL);
        break;

    case Params_Liveness::eng_Pdr2:
        /**/P_pdr2.prop_init = true;
        ret = lbool_lift(pdr2(N, props, P_pdr2, &cex, Netlist_NULL));     // <<== need to add bug-free-depth and invariant to Pdr2
        break;

    case Params_Liveness::eng_Imc:
        imcStd(N, props, Params_ImcStd(), &cex, Netlist_NULL, &bug_free_depth);
        break;

    default: assert(false); }

    // Report result:
    if (out_loop) *out_loop = UINT_MAX;
    if (ret == l_False && loop_start != Wire_ERROR){
        WriteLn "LIVENESS: \a*Witness found.\a*";

        // Verify liveness CEX:
        cex.inputs.pop();   // -- got one extra state because of match detection
        uint loop_frame;
        bool ok = verifyInfCex(N, cex, loop_start, &loop_frame);
        if (out_loop) *out_loop = loop_frame;
        if (!ok)
            WriteLn "INTERNAL ERROR! Liveness CEX did not verify.";
        else
            WriteLn "Liveness CEX verifies correctly: CEX states %_, loop length %_", cex.size(), cex.size() - loop_frame;

        // Write AIGER witness:
        if (P.witness_output != ""){
            OutFile out(P.witness_output);
            FWriteLn(out) "1";
            FWriteLn(out) "j%_", fair_prop_no;

            uint n_ffs = nextNum_Flop(N0);
            uint n_pis = nextNum_PI(N0);

            Get_Pob(N0, flop_init);
            Vec<char> text(n_ffs, 'x');
            For_Gatetype(N0, gate_Flop, w){
                int num = attr_Flop(w).number;
                if (num != num_NULL){
                    if      (flop_init[w] == l_True ) text[num] = '1';
                    else if (flop_init[w] == l_False) text[num] = '0';
                }
            }
            For_Gatetype(N, gate_Flop, w){
                int num = attr_Flop(w).number;
                if (num != num_NULL && (uint)num < n_ffs){
                    if      (cex.flops[0][w] == l_True ){ assert(text[num] != '0'); text[num] = '1'; }
                    else if (cex.flops[0][w] == l_False){ assert(text[num] != '1'); text[num] = '0'; }
                }
            }
            for (uint i = 0; i < text.size(); i++)
                if (text[i] == 'x')
                    text[i] = '0';
            FWriteLn(out) "%_", text;

            for (uint d = 0; d < cex.size(); d++){
                text.reset(n_pis, '0');
                For_Gatetype(N, gate_PI, w){
                    int num = attr_PI(w).number;
                    if (num != num_NULL && (uint)num < n_pis && cex.inputs[d][w] == l_True)
                        text[num] = '1';
                }
                FWriteLn(out) "%_", text;
            }

            FWriteLn(out) ".";

            WriteLn "Wrote: \a*%_\a*", P.witness_output;
        }

        if (par){
            Vec<uint> depths;
            depths.push(cex.depth());
            sendMsg_Result_fails(par_props, 2/*liveness property*/, depths, cex, N, true, loop_frame);
        }

        if (out_cex)
            translateCex(cex, N0, *out_cex, xlat);

    }else if (ret == l_False && loop_start == Wire_ERROR){
        // Inconclusive k-liveness call:
        WriteLn "LIVENESS: \a*Inconclusive.\a*";
        if (par)
            sendMsg_Result_unknown(par_props, 2/*liveness property*/);

    }else if (ret == l_True){
        // Invariant found => no fair witness exists:
        WriteLn "LIVENESS: \a*No witness exists.\a*";
        if (par)
            sendMsg_Result_holds(par_props, 2/*liveness property*/);

    }else{
        if (P.gig_output == "" && P.aig_output == "")
            WriteLn "No output file specified and no engine specified. Nothing done.";
    }

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


/*
  - AIGER 1.9 skrivare
  - SIF parser
  - Stöd i PAR
  - Parametrar till live
  - Direkt anrop till pdr, treb, pdr2, imc, bmc (eller mix därav) efter kLive eller liveToSafe översättning

injecera invarianta kuber vid inkrementel (kan jag räkna till 20?)
benchmarka liveness / propagate fas

-fce=0..3
*/
