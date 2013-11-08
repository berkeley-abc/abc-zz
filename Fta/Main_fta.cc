#include "Prelude.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_CmdLine.hh"
#include "Parser.hh"
#include "Solver.hh"
#include "Solver2.hh"
#include "Common.hh"
#include "Export.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input fault-tree (.tree file).", 0);
    cli.add("output", "string", "", "Output file.");

    CLI cli_bound;
    cli_bound.add("dump-cover", "bool", "no", "Dump cover upon complete enumeration.");
    cli.addCommand("bound", "Compute an upper bound on the probability of the top-node.", &cli_bound);

    cli.addCommand("save-xml", "Save fault-tree in OpenPSA XML format.");
    cli.addCommand("save-dot", "Save fault-tree in 'dotty' format (no probabilities).");

    cli.parseCmdLine(argc, argv);
    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;

    Gig N;
    Vec<String> ev_names;
    Vec<double> ev_probs;

    try{
        readFaultTree(input, N, ev_probs, ev_names);
    }catch (const Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        return 1;
    }

    /**/Dump(cli.cmd);

    if (cli.cmd == "save-dot"){
        if (output == "")
            output = setExtension(input, "dot");
        writeDot(output, N);
        WriteLn "Wrote: \a*%_\a*", output;

    }else if (cli.cmd == "save-xml"){
        if (output == "")
            output = setExtension(input, "xml");
        writeXml(output, N, ev_probs, ev_names);
        WriteLn "Wrote: \a*%_\a*", output;

    }else{ assert(cli.cmd == "bound");
        suppress_profile_output = false;
        setupSignalHandlers();

        ftaBound(N, ev_probs, ev_names);
    }

    return 0;
}
