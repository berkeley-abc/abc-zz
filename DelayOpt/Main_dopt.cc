#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Verilog.hh"
#include "ZZ_Liberty.hh"
#include "TimingRef.hh"
#include "OrgCells.hh"
#include "DelayOpt.hh"
#include "DelayOpt2.hh"
#include "DelaySweep.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("design", "string", arg_REQUIRED , "Input structural Verilog file.", 0);
    cli.add("lib"   , "string", ""           , "Input Liberty library file.", 1);
    cli.add("output", "string", ""           , "Output Verilog file.");
    cli.add("test"  , "bool"  , "no"         , "[INTERNAL]. Run test function.");
    cli.add("old"   , "bool"  , "no"         , "[INTERNAL]. Run old optimizer.");

    addCli_DelayOpt(cli);
    cli.parseCmdLine(argc, argv);

    String design_file = cli.get("design").string_val;
    String lib_file    = cli.get("lib").string_val;
    String output_file = cli.get("output").string_val;

    Params_DelayOpt P;
    setParams(cli, P);

    if (hasExtension(design_file, "lib") || hasExtension(design_file, "scl"))
        swp(design_file, lib_file);

    // Read input:
    Vec<VerilogModule> modules;
    SC_Lib L;
    try{
        cpuClock();
        if (hasExtension(lib_file, "lib")){
            readLiberty(lib_file, L);
            WriteLn "Reading liberty file: \a*%t\a*", cpuClock();
        }else{
            readSclFile(lib_file, L);
            WriteLn "Reading SCL file: \a*%t\a*", cpuClock();
        }

        if (design_file != ""){
            String prelude;
            genPrelude(L, prelude, true);
            readVerilog(design_file, /*store_names*/true, /*error_level*/VerilogErrors(vel_Ignore), modules, prelude.slice());
            WriteLn "Reading design: \a*%t\a*", cpuClock();
        }

    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    // No design? Just output some library info:
    if (design_file == ""){
        Netlist N;
        Vec<Vec<uint> > groups;
        groupCellTypes(L, groups);
        filterGroups(N, L, groups);
        return 0;
    }

    // Map black-boxes onto standard cells:
    IntMap<uint,uint> mod2cell;
    computeUifMap(modules, L, mod2cell);

    // Align input pin order in netlist and library:
    verifyPinsSorted(modules, mod2cell, L);

    // Flatten design:
    Params_Flatten PF;
    PF.store_names = 2;
    PF.strict_aig = false;

    Netlist N;
    uint top = flatten(modules, N, PF);
    if (top == UINT_MAX){
        ShoutLn "ERROR! Could not determine top module!";
        exit(1); }

    WriteLn "Flatten design: \a*%t\a*", cpuClock();
    WriteLn "After processing: %_", info(N);
    NewLine;

    // Remap UIFs:
    remapUifs(N, mod2cell);

    // Compute loads:
    Vec<float> wire_cap;
    Str model_chosen;
    try{
        getWireLoadModel(N, L, wire_cap, &model_chosen);
    }catch (Excp_Msg err){
        ShoutLn "ERROR! %_", err;
        exit(1);
    }
    WriteLn "Using wire load model   : \a*%_\a*", model_chosen;
    WriteLn "Sum of gate-sizes (area): \a*%,d\a*", (uint64)getTotalArea(N, L);

    // Run test function?
    if (cli.get("test").bool_val){
        initialBufAndSize(N, L, wire_cap, 0/*approx*/);
        writeFlatVerilogFile(output_file, modules[top].mod_name, N, L);
        WriteLn "Wrote: \a*%_\a*", output_file;
        return 0;
    }

    // Run delay optimization:
    if (cli.get("old").bool_val)
        optimizeDelay(N, L, wire_cap, cli.get("prebuf").int_val, cli.get("forget").bool_val);
    else
        optimizeDelay2(N, L, wire_cap, P);

    NewLine;
    WriteLn "Memory used: %DB", memUsed();
    WriteLn "Real Time  : %.2f s", realTime();
    WriteLn "CPU Time   : %.2f s", cpuTime();

    // Write optimized netlist:
    if (output_file != ""){
        writeFlatVerilogFile(output_file, modules[top].mod_name, N, L);
        WriteLn "Wrote: \a*%_\a*", output_file;
    }

    return 0;
}
