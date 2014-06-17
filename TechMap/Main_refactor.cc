#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "GigReader.hh"
#include "Refactor.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.parseCmdLine(argc, argv);

    Gig N;
    try{
        String input = cli.get("input").string_val;
        if (hasExtension(input, "aig")){
            readAigerFile(input, N, false);
        }else if (hasExtension(input, "gnl")){
            N.load(input);
        }else if (hasExtension(input, "gig")){
            readGigForTechmap(input, N);
        }else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    N.compact();
    refactor(N);

    return 0;
}
