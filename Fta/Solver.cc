//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Solver.cc
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
#include "Solver.hh"
#include "ZZ_Gip.CnfMap.hh"
#include "ZZ_Gip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for CNF mapping:


static
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
            assert(w.arg() == 3);
            assert(w.size() == 4);
            Wire u0 = mkAnd(w[0], mkAnd(w[2], w[3]));
            Wire u1 = mkAnd(w[1], mkAnd(w[2], w[3]));
            Wire u2 = mkAnd(w[2], mkAnd(w[0], w[1]));
            Wire u3 = mkAnd(w[3], mkAnd(w[0], w[1]));
            xlat(w) = mkOr(mkOr(u0, u1), mkOr(u2, u3));
            break; }

        default:
            ShoutLn "INTERNAL ERROR! Unexpected gate type: %_", w;
            assert(false);
        }
    }

    N.strash();
    N.unstrash();
}


// Takes a fault-tree 'N' (which will be massaged into a more optimized form) and computes
// all satisfying assignments.
void enumerateModels(Gig& N, const Vec<String>& ev_names)
{
    convertToAig(N);
    cnfMap(N, Params_CnfMap(true));

    WriteLn "Mapped: %_", info(N);

    assert(N.enumSize(gate_PO) == 1);
    MiniSat2 S;
    WMapX<Lit> n2s;
    Lit p_top = clausify(N.enumGate(gate_PO, 0), S, n2s);

    lbool result = S.solve(p_top);

    if (result == l_True){
      #if 0
        Write "Model:";
        For_Gatetype(N, gate_PI, w)
            Write " %C%_", (S.value(n2s[w]) == l_False) ? '~' : 0, ev_names[w.num()];
        NewLine;
      #else
        Write "Cutset:";
        For_Gatetype(N, gate_PI, w)
            if (S.value(n2s[w]) == l_True)
                Write " %_", ev_names[w.num()];
        NewLine;
      #endif
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
