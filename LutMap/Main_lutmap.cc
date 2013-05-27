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
    cli.add("blif"  , "string", ""          , "Save original input in BLIF format (for debugging only).");
    cli.add("N"     , "uint"  , "10"        , "Cuts to keep per node.");
    cli.add("iters" , "uint"  , "4"         , "Number of mapping phases.");
    cli.add("delay" , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("speed" , "bool"  , "no"        , "Optimize for delay (defaul is area).");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    String blif   = cli.get("blif").string_val;
    Params_LutMap P;
    P.cuts_per_node = cli.get("N").int_val;
    P.n_rounds      = cli.get("iters").int_val;
    P.delay_factor  = cli.get("delay").float_val;
    P.map_for_area  = !cli.get("speed").bool_val;

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

    if (blif != ""){
        writeBlifFile(blif, N);
        WriteLn "Wrote: \a*%_\a*", blif;
    }

    lutMap(N, P);

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

    if (output != ""){
        if (hasExtension(output, "blif")){
            writeBlifFile(output, N);
            WriteLn "Wrote: \a*%_\a*", output;
        }else if (hasExtension(output, "gnl")){
            N.save(output);
            WriteLn "Wrote: \a*%_\a*", output;
        }else{
            ShoutLn "ERROR! Unknown file extension: %_", output;
            exit(1);
        }
    }

    return 0;
}
