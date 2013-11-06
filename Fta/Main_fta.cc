#include "Prelude.hh"
#include "Parser.hh"
#include "Solver.hh"
#include "Solver2.hh"
#include "Common.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Gig N;
    Vec<String> ev_names;
    Vec<double> ev_probs;

#if 0   /*DEBUG*/
    Wire x0 = N.add(gate_PI);
    Wire x1 = N.add(gate_PI);
    Wire x2 = N.add(gate_PI);
    Wire f = mkAnd(x0, x1);
    Wire g = mkAnd(f, x2);
    Wire t0 = N.add(gate_PO).init( g);
    Wire t1 = N.add(gate_PO).init(~g);

    pushNegations(N);

    For_UpOrder(N, w){
        if (w == gate_PI)
            WriteLn "%_ = PI [%_]", w.lit(), w.num();
        else if (w == gate_And)
            WriteLn "%_ = And(%_, %_)", w.lit(), w[0].lit(), w[1].lit();
        else if (w == gate_Or)
            WriteLn "%_ = Or(%_, %_)", w.lit(), w[0].lit(), w[1].lit();
        else { assert(w == gate_PO);
            WriteLn "%_ = PO(%_) [%_]", w.lit(), w[0].lit(), w.num(); }
    }

    return 0;
#endif  /*END DEBUG*/

    try{
        readFaultTree("CEA9601.tree", "CEA9601.prob", N, ev_probs, ev_names);
    }catch (const Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        return 1;
    }

  #if 0
    WriteLn "Parsed: %_", info(N);
    enumerateModels(N, ev_probs, ev_names);
  #else
    ftaBound(N, ev_probs, ev_names);
  #endif

    return 0;
}
