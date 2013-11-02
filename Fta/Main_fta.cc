#include "Prelude.hh"
#include "Parser.hh"
#include "Solver.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Gig N;
    Vec<String> ev_names;

    try{
        readFaultTree("CEA9601.tree", "CEA9601.prob", N, ev_names);
    }catch (const Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        return 1;
    }

    WriteLn "Parsed: %_", info(N);
    enumerateModels(N, ev_names);

    return 0;
}
