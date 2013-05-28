#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input file (.gnl or .aig).", 0);
    cli.add("output", "string", arg_REQUIRED, "Output file (.gnl or .blif).", 1);
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;

    // Read input file:
    Gig N;
    if (hasExtension(input, "gnl")){
        try{
            N.load(input);
        }catch (const Excp_Msg& err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N, false);
            For_Gatetype(N, gate_PO, w)     // -- invert properties
                w.set(0, ~w[0]);
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    // Save file:
    if (output != ""){
        if (hasExtension(output, "gnl"))
            N.save(output);
        else if (hasExtension(output, "blif"))
            WriteLn "Wrote: \a*%_\a*", output;
        else{
            ShoutLn "ERROR! Unknown file extension: %_", output;
            exit(1); }

        WriteLn "Wrote: \a*%_\a*", output;
    }

    return 0;
}
