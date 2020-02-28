#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_Unix.hh"
#include "GigReader.hh"
#include "Refactor.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Parse commandline:
    cli.add("input", "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.add("coarsen", "bool", "no", "Detect XORs/MUXes before running.");
    cli.add("cec", "bool", "no", "Write AIGER files for equivalence checking.");
    cli.parseCmdLine(argc, argv);

    Params_Refactor P;

    // Read netlist:
    Gig N;
    try{
        String input = cli.get("input").string_val;
        if (hasExtension(input, "aig")){
            readAigerFile(input, N, false);
        }else if (hasExtension(input, "aig.gpg")){
            // Start GPG process:
            int pid;
            int io[3];
            Vec<String> args;
            args += "--batch", "--no-use-agent", "--passphrase-file", homeDir() + "/.gnupg/key.txt", "-d", input;
            startProcess("*gpg", args, pid, io);

            // Read file:
            File file(io[1], READ, false);
            In   in(file);
            readAiger(in, N, false);
            closeChildIo(io);
            waitpid(pid, NULL, 0);
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

    // Cec?
    if (cli.get("cec").bool_val){
        expandXigGates(N);      // -- will change the flow slightly
        writeAigerFile("before.aig", N);
        WriteLn "Wrote: \a*before.aig\a*";
    }

    // Pre-process:
    if (cli.get("coarsen").bool_val)
        introduceXorsAndMuxes(N);
    N.compact();

    // Run refactoring:
    double T0 = cpuTime();
    WMapX<GLit> remap;
    refactor(N, remap, P);
    double T1 = cpuTime();

    // Cec?
    if (cli.get("cec").bool_val){
        expandXigGates(N);
        writeAigerFile("after.aig", N);
        WriteLn "Wrote: \a*after.aig\a*";
    }

    // Print stats:
    WriteLn "CPU Time: %t", T1 - T0;
    WriteLn "Mem used: %DB", memUsed();

    return 0;
}
