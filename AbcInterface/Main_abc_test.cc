//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_abc_test.cc
//| Author(s)   : Niklas Een
//| Module      : AbcInterface
//| Description :
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_CmdLine.hh"
#include "AbcInterface.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input AIGER or GIG file. May be gzipped.", 0);
    cli.add("cmd" , "string", arg_REQUIRED, "ABC command to run.", 1);
    cli.add("cmd2" , "string", "", "Second ABC command to run.", 2);
    cli.add("k" , "uint", "1", "Times to run first command 'cmd'.");
    cli.add("k2" , "uint", "1", "Times to run second command 'cmd2'.");
    cli.add("show", "bool", "no", "Show output of ABC.");
    cli.add("inv", "bool", "no", "Invert outputs.");
    cli.parseCmdLine(argc, argv);
    String input = cli.get("input").string_val;
    String cmd   = cli.get("cmd"  ).string_val;
    String cmd2  = cli.get("cmd2" ).string_val;
    uint   k     = cli.get("k"    ).int_val;
    uint   k2    = cli.get("k2"   ).int_val;
    bool   show  = cli.get("show" ).bool_val;

    // Read input file:
    Netlist N;
    if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N);
            For_Gatetype(N, gate_PO, w)     // -- invert properties
                w.set(0, ~w[0]);
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

    if (cli.get("inv").bool_val){
        For_Gatetype(N, gate_PO, w)
            w.set(0, ~w[0]);
    }

    if (hasGeneralizedGates(N)){
        WriteLn "NOTE! Expanding generalized AIG gates.";
        expandGeneralizedGates(N); }

    //**/removeUnreach(N, NULL, false);
    //**/Assure_Pob(N, strash);

    // Execute commands:
    FILE* abc_out = show ? NULL : fopen("abc.out", "wb");
    Cex cex;
    for (uint i = 0; i < k; i++){
        if (i != 0) NewLine;
        WriteLn "Running '\a*%_\a*' -- %_", cmd, info(N);
        ZZ::lbool ret = runAbcScript(N, cmd, cex, abc_out);
        WriteLn "Return: %_ %w -- %_", ret, cmd.size(), info(N);
    }

    if (cmd2 != ""){
        for (uint i = 0; i < k2; i++){
            NewLine;
            Write "Running '%_' -- %_\f", cmd2, info(N);
            ZZ::lbool ret = runAbcScript(N, cmd2, cex, abc_out);
            WriteLn "Return: %_ -- %_", ret, info(N);
        }
    }
    if (!show) fclose(abc_out);

    return 0;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
