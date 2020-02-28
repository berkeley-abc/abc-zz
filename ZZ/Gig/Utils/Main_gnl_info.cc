#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.hh"
#include "ZZ_Gig.IO.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input GNL or AIGER file.", 0);
    cli.parseCmdLine(argc, argv);
    String input  = cli.get("input").string_val;

    // Read input:
    Gig N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, true);
        if (hasExtension(input, "gnl"))
            N.load(input);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    // Write statistics:
    WriteLn "%_", info(N);

    return 0;
}
