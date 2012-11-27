//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Live.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Simple liveness checker based on the Biere and Claessen/Sorensson methods.
//| 
//| (C) Copyright 2012, The Regents of the University of California
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

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
void tidyUp(NetlistRef N)
{
    Get_Pob(N, properties);

    // Number flops:
    int numC = nextNum_Flop(N);
    For_Gatetype(N, gate_Flop, w)
        if (attr_Flop(w).number == num_NULL)
            attr_Flop(w).number = numC++;

    // Clean up:
    For_Gatetype(N, gate_PO, w)
        if (w != properties[0])
            remove(w);

    removeBuffers(N);
    removeAllUnreach(N);
}


void liveToSafe(NetlistRef N, const Params_Liveness& P, int n_orig_flops, Wire fair_mon)
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
#endif

    // If was watching and seen all and match => CEX:
    Remove_Pob(N, init_bad);
    properties.clear();
    properties.push(N.add(PO_(), ~s_And(was_watching, s_And(has_seen_all, match))));

    tidyUp(N);
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


lbool liveness(NetlistRef N0, uint fair_prop_no, const Params_Liveness& P)
{
    Get_Pob(N0, fair_properties);
    Auto_Pob(N0, fair_constraints);

    Vec<Wire> fairs;
    append(fairs, fair_properties[fair_prop_no]);
    append(fairs, fair_constraints);

    Netlist N;
    int     n_orig_flops ___unused = nextNum_Flop(N0); // -- don't introduce shadow registers for liveness monitor flops
    Wire    fair_mon;

    if (P.k == Params_Liveness::L2S){
        initBmcNetlist(N0, fairs, N, true, &fair_mon, false);
        liveToSafe(N, P, n_orig_flops, fair_mon);

    }else{
        initBmcNetlist(N0, fairs, N, true, &fair_mon, true);
        if (P.k == Params_Liveness::INC)
            return kLive(N, P, fair_mon, 0, true);      // <<== use 'eng' parameter here
        else
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

    lbool ret;
    switch (P.eng){
    case Params_Liveness::eng_NULL:
        ret = l_Undef;
        break;

    case Params_Liveness::eng_Bmc:
        ret = bmc(N, props, Params_Bmc(), &cex, &bug_free_depth);
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

    // <<== extract and verify liveness CEX (if ret == l_False)

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
