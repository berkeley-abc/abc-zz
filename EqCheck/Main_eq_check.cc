//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_eq_check.cc
//| Author(s)   : Niklas Een
//| Module      : EqCheck
//| Description : Reads two input files and pass them to 'eqCheck()'.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Verilog.hh"
#include "ZZ_Liberty.hh"
#include "EqCheck.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void readVerilog(const String& design, const SC_Lib& L, NetlistRef N_out)
{
    // Add declaration for standard-cell elements to Verilog prelude:
    String prelude;
    genPrelude(L, prelude, true);

    // Read structural Veriolg:
    Vec<VerilogModule> modules;
    readVerilog(design, /*store_names*/true, /*error_level*/VerilogErrors(vel_Ignore), modules, prelude.slice());

    // Map black-boxes onto standard cells:
    IntMap<uint,uint> mod2cell;
    computeUifMap(modules, L, mod2cell);

    // Flatten design:
    Params_Flatten P;
    P.store_names = 2;
    P.strict_aig = false;

    uint top = flatten(modules, N_out, P);
    if (top == UINT_MAX){
        ShoutLn "ERROR! Could not determine top module!";
        exit(1); }

    // Remap UIFs:
    remapUifs(N_out, mod2cell);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("design1", "string", arg_REQUIRED, "First input: Verilog, AIGER or GIG file.", 0);
    cli.add("design2", "string", arg_REQUIRED, "Second input: Verilog, AIGER or GIG file.", 1);
    cli.add("lib"    , "string", "", "Input Liberty library file (if design contains UIFs/standard cells).", 2);
    cli.add("aig"    , "string", "", "Write single-output AIGER file containing mitered circuits.");
    cli.parseCmdLine(argc, argv);

    String design1 = cli.get("design1").string_val;
    String design2 = cli.get("design2").string_val;
    String lib_file = cli.get("lib").string_val;
    String aig_file = cli.get("aig").string_val;
    if (hasExtension(design1, "lib") || hasExtension(design1, "scl"))
        swp(design1, design2);
    if (hasExtension(design2, "lib") || hasExtension(design2, "scl"))
        swp(design2, lib_file);

    // Read input:
    SC_Lib  L;
    Netlist N1, N2;
    String  curr_file = "";
    try{
        cpuClock();
        if (lib_file != ""){
            curr_file = lib_file;
            if (hasExtension(lib_file, "lib")){
                readLiberty(lib_file, L);
                WriteLn "Reading liberty file: \a*%t\a*", cpuClock();
            }else{
                readSclFile(lib_file, L);
                WriteLn "Reading SCL file: \a*%t\a*", cpuClock();
            }
        }

        curr_file = design1;
        if (hasExtension(design1, "v"))
            readVerilog(design1, L, N1);
        else if (hasExtension(design1, "gig"))
            N1.read(design1);
        else if (hasExtension(design1, "aig"))
            readAigerFile(design1, N1);
        else{
            ShoutLn "ERROR! Unsupported input file format: %_", design1;
            exit(1); }
        WriteLn "Reading design 1: \a*%t\a*", cpuClock();

        curr_file = design2;
        if (hasExtension(design2, "v"))
            readVerilog(design2, L, N2);
        else if (hasExtension(design2, "gig"))
            N2.read(design2);
        else if (hasExtension(design2, "aig"))
            readAigerFile(design2, N2);
        else{
            ShoutLn "ERROR! Unsupported input file format: %_", design2;
            exit(1); }
        WriteLn "Reading design 2: \a*%t\a*", cpuClock();

    }catch (Excp_Msg& msg){
        ShoutLn "%_: PARSE ERROR! %_", curr_file, msg;
        exit(1);
    }

    WriteLn "Design 1: %_", info(N1);
    WriteLn "Design 2: %_", info(N2);

    eqCheck(N1, N2, L, aig_file);

    return 0;
}
