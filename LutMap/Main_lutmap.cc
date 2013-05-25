#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "LutMap.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input AIGER.", 0);
    cli.add("output", "string", ""          , "Output GNL file (optional).", 1);
    cli.add("N"     , "uint"  , "10"        , "Cuts to keep per node.");
    cli.add("iters" , "uint"  , "3"         , "Number of mapping phases.");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    Params_LutMap P;
    P.cuts_per_node = cli.get("N").int_val;
    P.n_rounds      = cli.get("iters").int_val;

    // Read input file:
    double  T0 = cpuTime();
    Gig N;
    if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N, false);
            For_Gatetype(N, gate_PO, w)     // -- invert properties
                w.set(0, ~w[0]);
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else if (hasExtension(input, "gnl")){
        try{
            N.load(input);
        }catch (const Excp_Msg& err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;

    lutMap(N, P);

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

    if (output != "")
        ;   // M.save(output);

    return 0;
}
