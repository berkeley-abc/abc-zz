#include "Prelude.hh"
#include "Parser.hh"

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

    return 0;
}
