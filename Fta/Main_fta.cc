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
    cli.add("profile", "bool", "yes", "Print profiling information at exit.");

    CLI cli_bound;
    cli_bound.add("dump-cover", "bool", "no", "Dump cover upon complete enumeration.");
    cli_bound.add("apx"       , "int[0:3]", "3", "Probability approximation level for SAT regions. 0=no approx, 1=std over approx, 2=use tree nodes, 3=also use support.");
    cli_bound.add("exact"     , "float", "-1", "If not '-1', compute an exact value for the top-event using this timeout.");
    cli.addCommand("bound", "Compute an upper bound on the probability of the top-node.", &cli_bound);

    CLI cli_enum;
    cli_enum.add("cutoff", "float", "1e-12", "Enumerate MCSs downto this probability.");
    cli_enum.add("quant" , "float", "1",     "Quanta to approximate cutoff. Should be a negative power of 2.");
    cli_enum.add("hprob" , "float", "0.75",  "Threshold for \"high probability\" literals to be excluded from MCSs.");
    cli.addCommand("enum", "Enumerate cubes and sum up their probability [experimental].", &cli_enum);

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

    if (cli.get("profile").bool_val){
        suppress_profile_output = false;
        setupSignalHandlers();
    }

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

    }else if (cli.cmd == "bound"){
        Params_FtaBound P;
        P.use_prob_approx = cli_bound.get("apx").int_val >= 1;
        P.use_tree_nodes  = cli_bound.get("apx").int_val >= 2;
        P.use_support     = cli_bound.get("apx").int_val >= 3;
        P.dump_cover      = cli_bound.get("dump-cover").bool_val;
        P.exact_sol       = cli_bound.get("exact").float_val;

        if (P.exact_sol >= 0)
            suppress_profile_output = true;

        ftaBound(N, ev_probs, ev_names, P);

    }else if (cli.cmd == "enum"){
        Params_FtaEnum P;
        P.mcs_cutoff      = cli_enum.get("cutoff").float_val;
        P.cutoff_quant    = cli_enum.get("quant").float_val;
        P.high_prob_thres = cli_enum.get("hprob").float_val;
        enumerateModels(N, ev_probs, ev_names, P);

    }else
        assert(false);

    return 0;
}
