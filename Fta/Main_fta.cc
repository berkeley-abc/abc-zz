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

    try{
//        readFaultTree("CEA9601.tree", "CEA9601.prob", N, ev_probs, ev_names);
        readFaultTree("baobab1.tree", "baobab1.prob", N, ev_probs, ev_names);       // ~1.68145e-06
//        readFaultTree("simple2.tree", "simple2.prob", N, ev_probs, ev_names);
    }catch (const Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        return 1;
    }

    suppress_profile_output = false;
    setupSignalHandlers();

  #if 0
    WriteLn "Parsed: %_", info(N);
    enumerateModels(N, ev_probs, ev_names);
  #else
    ftaBound(N, ev_probs, ev_names);
  #endif

    return 0;
}
