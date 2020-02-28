#include "Prelude.hh"
#include "CutSim.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"

#include <csignal>

namespace ZZ { void test(NetlistRef N, uint max_cutsize, uint cuts_per_node, uint heuristic_cutoff, bool weaken_state); }


using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Signal handler:


extern "C" void SIGINT_handler(int signum);
void SIGINT_handler(int signum)
{
    printf("\n*** CTRL-C pressed ***  [cpu-time: %.2f s]  [mem-used: %.1f MB]\n", cpuTime(), memUsed() / 1048576.0);
        // -- this is highly unsafe, but it is temporary...
    fflush(stdout);
    dumpProfileData();
  #if !defined(ZZ_PROFILE_MODE)
    _exit(-1);
  #else
    zzFinalize();
    exit(-1);
  #endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input AIGER or GIG file. May be gzipped.", 0);
    cli.add("output", "string", ""          , "Output AIGER or GIG file.", 1);

    CLI cli_ted;
    cli_ted.add("cyc", "uint", "10", "Maximum cycle length.");
    cli_ted.add("tot", "uint", "20", "Maximum total length (prefix + cycle).");

    CLI cli_sim;
    cli_sim.add("k", "int[0:4]", "4", "Maximum cut-width.");
    cli_sim.add("N", "int[1:]", "8", "Cuts to store per node.");
    cli_sim.add("h", "int[0:8]", "6", "Heuristic cut-off when combining cuts.");
    cli_sim.add("weaken", "bool", "no", "Weaken flop info between cycles.");

    cli.addCommand("ted",  "Temporal decomposition.", &cli_ted);
    cli.addCommand("sim", "Debug.", &cli_sim);

    cli.parseCmdLine(argc, argv);

    // Set up signal handler:
    signal(SIGINT, SIGINT_handler);
  #if !defined(_MSC_VER)
    signal(SIGHUP, SIGINT_handler);
  #endif

    // Read input file:
    Netlist N;
    bool    is_aiger = false;
    String  input  = cli.get("input").string_val;
    String  output = cli.get("output").string_val;
    if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N);
            For_Gatetype(N, gate_PO, w)     // -- invert properties
                w.set(0, ~w[0]);
            is_aiger = true;
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else if (hasExtension(input, "gig")){
        try{
            N.read(input);
        }catch (Excp_NlParseError err){
            ShoutLn "PARSE ERROR [line %_]! %_", err.line_no, err.msg;
            exit(1);
        }

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    if (is_aiger){
        Add_Pob(N, flop_init);
        For_Gatetype(N, gate_Flop, w)
            flop_init(w) = l_False;
    }

    // Define properties:
    if (!Has_Pob(N, properties)){
        if (N.typeCount(gate_PO) == 0){
            ShoutLn "No properties and no POs in design!";
            exit(1); }
        makeAllOutputsProperties(N);
    }

    // Clean up etc:
    removeUnreach(N, NULL, false);
    Assure_Pob0(N, strash);
    WriteLn "Read: %_ -- %_", input, info(N);

    // Check/compact numbering:
    if (!checkNumbering(N, true)){
        renumber(N);
        WriteLn "WARNING! Input file does not number its external elements (PI/PO/Flop) correctly.";
        WriteLn "Renumbering applied.";
    }

    if (cli.cmd == "sim"){
        uint max_cutsize = cli.get("k").int_val;
        uint cuts_per_node = cli.get("N").int_val;
        uint heuristic_cutoff = cli.get("h").int_val;
        bool weaken_state ___unused = cli.get("weaken").bool_val;
        test(N, max_cutsize, cuts_per_node, heuristic_cutoff, weaken_state);

    }else if (cli.cmd == "ted"){
        Netlist M;
#if 0
        /**/Vec<lbool> dummy;
        /**/applyTempDecomp(N, M, 10, 1, dummy);
        /**/M.write("tmp.gig");
        /**/WriteLn "Wrote: tmp.gig";
        /**/exit(0);
#endif
        uint pfx, cyc;
        l_tuple(pfx, cyc) = tempDecomp(N, M, cli.get("cyc").int_val, cli.get("tot").int_val);
        if (output == ""){
            output = input;
            stripSuffix(output, ".gz");
            stripSuffix(output, ".aig");
            stripSuffix(output, ".gig");
            FWrite(output) "_pfx%d_cyc%d.aig", pfx, cyc;

        }

        if (hasSuffix(output, ".gig")){
            M.write(output);
            WriteLn "Wrote: \a*%_\a*", output;
        }else{
            For_Gatetype(M, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            if (writeAigerFile(output, M))
                WriteLn "Wrote: \a*%_\a*", output;
            else
                WriteLn "Failed to write AIGER file!";
        }

    }else
        assert(false);

    return 0;
}
