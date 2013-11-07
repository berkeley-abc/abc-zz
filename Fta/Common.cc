//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description :
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for CNF mapping:


void convertToAig(Gig& N)
{
    WMapX<GLit> xlat;
    xlat.initBuiltins();

    For_UpOrder(N, w){
        // Translate inputs:
        For_Inputs(w, v)
            w.set(Iter_Var(v), xlat[v]);

        // Turn gate into ANDs:
        switch (w.type()){
        case gate_PI:
        case gate_PO:
            xlat(w) = w;
            break;

        case gate_Not:
            xlat(w) = ~w[0];
            break;

        case gate_Conj:{
            Wire acc = N.True();
            For_Inputs(w, v)
                acc = mkAnd(acc, v);
            xlat(w) = acc;
            break; }

        case gate_Disj:{
            Wire acc = N.True();
            For_Inputs(w, v)
                acc = mkAnd(acc, ~v);
            xlat(w) = ~acc;
            break; }

        case gate_CardG:{
            // Temporary:
            assert(w.arg() == 2 || w.arg() == 3);
            assert(w.size() == 4);
            if (w.arg() == 2){
                Wire u0 = mkAnd(w[0], w[1]);
                Wire u1 = mkAnd(w[0], w[2]);
                Wire u2 = mkAnd(w[0], w[3]);
                Wire u3 = mkAnd(w[1], w[2]);
                Wire u4 = mkAnd(w[1], w[3]);
                Wire u5 = mkAnd(w[2], w[3]);
                xlat(w) = mkOr(mkOr(mkOr(u0, u1), mkOr(u2, u3)), mkOr(u4, u5));
            }else{
                Wire u0 = mkAnd(w[0], mkAnd(w[2], w[3]));
                Wire u1 = mkAnd(w[1], mkAnd(w[2], w[3]));
                Wire u2 = mkAnd(w[2], mkAnd(w[0], w[1]));
                Wire u3 = mkAnd(w[3], mkAnd(w[0], w[1]));
                xlat(w) = mkOr(mkOr(u0, u1), mkOr(u2, u3));
            }
            break; }

        default:
            ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", w;
            assert(false);
        }
    }

    removeUnreach(N);
    N.strash();
    N.unstrash();
}


static
GLit pushNeg(Wire w, WMap<GLit>& pmap, WMap<GLit>& nmap)
{
    if (!w.sign){
        if (!pmap[w]){
            // Reuse this AND gate:
            assert(w == gate_And);
            w.set(0, pushNeg(w[0], pmap, nmap));
            w.set(1, pushNeg(w[1], pmap, nmap));
            pmap(w) = w;
        }
        return pmap[w];

    }else{
        if (!nmap[w]){
            // Add OR gate:   <<== for now
            assert(w == gate_And);
            Wire wn = gig(w).add(gate_And);
            wn.set(0, ~pushNeg(~w[0], pmap, nmap));
            wn.set(1, ~pushNeg(~w[1], pmap, nmap));
            nmap(w) = ~wn;
        }
        return nmap[w];
    }
}


// Assumes 'N' is an AIG; will push negations to the leaves, introducing ORs (coded as ANDs + 
// inverters). Leaves will also be negation free; instead a new variable will be introduced
// for negative PIs (doubling the number of PIs).
void pushNegations(Gig& N)
{
    WMap<GLit> pmap, nmap;
    pmap(GLit_True) = GLit_True;
    nmap(GLit_True) = ~GLit_True;

    // Add PIs for negated original PIs:
    uint n_pis = N.enumSize(gate_PI);
    for (uint i = 0 ; i < n_pis; i++){
        Wire w = N(gate_PI, i);
        Wire w_neg = N.add(gate_PI);
        assert(w_neg.num() - w.num() == n_pis);

        pmap(w) = w;
        nmap(w) = w_neg;
    }

    For_Gatetype(N, gate_PO, w)
        w.set(0, pushNeg(w[0], pmap, nmap));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
