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
            if (!wm) continue;

            For_Inputs(w, v)
                wm.set(Input_Pin(v), xlat[v] + M);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Counterexamples:


// Replace Xs with 0 or 1, consistent with initial state.
void completeCex(const Gig& N, Cex& cex)
{
    For_Gatetype(N, gate_FF, w){
        if (cex.ff[w] == l_Undef){
            if (w[1] == GLit_True || w[1] == ~GLit_False)
                cex.ff(w) = l_True;
            else if (w[1] == GLit_False || w[1] == ~GLit_True || +w[1] == GLit_Unbound)
                cex.ff(w) = l_False;
            else
                assert(false);      // -- complex initialization not supported yet
        }
    }

    for (uint d = 0; d < cex.size(); d++){
        For_Gatetype(N, gate_PI, w){
            if (cex.pi[d][w] == l_Undef)
                cex.pi[d](w) = l_False;
        }
    }
}


// Returns TRUE if 'prop' fails at frame 'cex.size() - 1'. NOTE! Must be a complete CEX (no 'l_Undef's).
bool verifyCex(const Gig& N, Prop prop, const Cex& cex)
{
    // Setup initial state for 'val':
    WMap<uchar> val(N, (uchar)0);
    val(GLit_True) = 1;
    For_Gatetype(N, gate_FF, w)
        if (cex.ff[w] == l_True)
            val(w) = 1;

    // Prepare iteration:
    uint64 gtm_ignore = GTM_(Const) | GTM_(PI) | GTM_(FF);
    uint64 gtm_ident  = gtm_CO;
    uint64 gtm_logic  = GTM_(And) | GTM_(Lut4);

    Vec<GLit> order;
    upOrder(N, order);

    // Iterate over unrolling:
    for (uint d = 0; d < cex.size(); d++){
        // Grab new PI values:
        For_Gatetype(N, gate_PI, w)
            val(w) = bool(cex.pi[d][w] == l_True);

        // Simulate one frame:
        for (uint i = 0; i < order.size(); i++){
            Wire w = order[i] + N;
            uint64 t = GTM(w.type());

            if (t & gtm_ignore)
                /*nothing*/;
            else if (t & gtm_ident)
                val(w) = val[w[0]] ^ w[0].sign;
            else if (t & gtm_logic){
                if (w == gate_And)
                    val(w) = (val[w[0]] ^ w[0].sign) & (val[w[1]] ^ w[1].sign);
                else if (w == gate_Lut4){
                    uint idx =  (val[w[0]] ^ w[0].sign)
                             | ((val[w[1]] ^ w[1].sign) << 1)
                             | ((val[w[2]] ^ w[2].sign) << 2)
                             | ((val[w[3]] ^ w[3].sign) << 3);
                    val(w) = bool(w.arg() & (1 << idx));
                }
            }else
                assert(false);
        }

        // Move value from 'Seq' gates to flops:
        For_Gatetype(N, gate_FF, w)
            val(w) = val[w[0]] ^ w[0].sign;
    }

    // Check property value:
    assert(prop.type == pt_Safe);       // <<== for now
    Wire w_prop = N(gate_SafeProp, prop.num);
    return !bool(val[w_prop] ^ w_prop.sign);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
