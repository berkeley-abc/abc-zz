//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_cnfmap.cc
//| Author(s)   : Niklas Een
//| Module      : CnfMap
//| Description : Stand-alone executable for Techmap for CNF module.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________


#include "Prelude.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_CmdLine.hh"
#include "CnfMap.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Params_CnfMap P;
    cli.add("input" , "string", arg_REQUIRED               , "Input AIGER or GIG file. May be gzipped.", 0);
    cli.add("output", "string", ""                         , "Output GNL file (optional).", 1);
    cli.add("N"     , "uint"  , (FMT"%_", P.cuts_per_node) , "Cuts to keep per node.");
    cli.add("iters" , "uint"  , (FMT"%_", P.n_rounds)      , "Number of mapping phases.");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
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
        }catch (Excp_Msg err){
            ShoutLn "PARSE ERROR %_", err.msg;
            exit(1);
        }

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    WriteLn "Input netlist: %_", info(N);
    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;

    // Techmap:    
    //**/nameByCurrentId(N);
    //**/N.write("latest_cnfmap.gig");

    cnfMap(N, P);

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

    WriteLn "Output netlist: %_", info(N);
    if (output != "")
        N.save(output);

    return 0;
}
