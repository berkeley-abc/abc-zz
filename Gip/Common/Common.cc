//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : Common functions and datatypes for the various Gip engines.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Prepare netlist for verification:


// Copies the sequential transitive fanin of 'sinks' from netlist 'N' to 'M' (which must be empty)
void prepareNetlist(Gig& N, const Vec<GLit>& sinks_, Gig& M)
{
    assert(M.isEmpty());

    // Cone-of-influence + topological sort:
    Vec<GLit> sinks(copy_, sinks_);
    Vec<GLit> order;
    uint n = 0;
    do{
        upOrder(N, sinks, order);
        sinks.clear();
        for (; n < order.size(); n++){
            Wire w = order[n] + N;
            if (isSeqElem(w))
                assert(w[0] == gate_Seq),
                sinks.push(w[0]);
        }
    }while (sinks.size() > 0);

    // Copy gates:
    WMapX<GLit> xlat;
    xlat.initBuiltins();
    for (uint i = 0; i < order.size(); i++)
        copyGate(order[i] + N, M, xlat);

    // Tie up flops:
    For_Gates(N, w){
        if (isSeqElem(w)){
            Wire wm = xlat[w] + M;
            For_Inputs(w, v)
                wm.set(Input_Pin(v), xlat[v] + M);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
