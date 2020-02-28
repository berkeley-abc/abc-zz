#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Liberty.hh"
#include "Parser.hh"
#include "Flatten.hh"
#include "Common.hh"
#include "Cost.hh"
#include "BlifWriter.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Parse commandline:
    cli.add("input", "string", arg_REQUIRED, "Top-level Verilog file. May include other files.", 0);
    cli.add("output", "string", "", "Output file (.gig or .blif).", 1);
    cli.add("lib", "string", "", "Input library file (.scl or .lib).");
    cli.add("include", "string | [string]", "", "Include these Verilog files before parsing main file.");
    cli.add("uif-names", "string", "", "Output name of UIF (gates) into this file.");
    cli.add("delays", "string", "", "File containing gate delay information => compute delay.");
    cli.add("sizes", "string", "", "File containing gate size information => compute area. ");
    cli.add("names", "{no,ext,all}", "ext", "Store no, external or all names?");
    cli.add("muxes", "bool", "yes", "If no, MUXes and XORs are convertet to ANDs.");
    cli.add("verific", "bool", "yes", "If no, Verific operators are black-boxed.");
    cli.add("dump-mods", "bool", "no", "[DEBUG]. Write down individual modules to aiger files.");
    cli.add("dump-sigs", "bool", "no", "[DEBUG]. Write signatures of all modules to stdout.");

    cli.add("undecl-sym", "{ignore, warn, error}", "warn"  , "Warn if nets are used before they are declared, or if they are never declared.");
    cli.add("dang-logic", "{ignore, warn, error}", "warn"  , "Warn if module contains logic that is not feeding into any output.");
    cli.add("unused-out", "{ignore, warn, error}", "ignore", "Warn if output signal from module instantiation is unused.");
    cli.add("no-driver" , "{ignore, warn, error}", "warn"  , "Warn if net has no driver (signal is declared but never defined).");
    cli.add("undef-mod" , "{ignore, warn, error}", "warn"  , "Warn if undefined module is instantiated.");
    cli.add("silent", "bool", "no", "Set all warnings levels to \"ignore\".");
    cli.add("pedantic", "bool", "no", "Set all warnings levels to \"error\".");

    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    String lib_file = cli.get("lib").string_val;
    String output_uif_names = cli.get("uif-names").string_val;
    String delays = cli.get("delays").string_val;
    String sizes  = cli.get("sizes").string_val;
    bool   dump_mods = cli.get("dump-mods").bool_val;
    bool   dump_sigs = cli.get("dump-sigs").bool_val;
    Params_Flatten P;
    P.store_names      = cli.get("names").enum_val;
    P.strict_aig       = !cli.get("muxes").bool_val;
    P.blackbox_verific = !cli.get("verific").bool_val;

    VerilogErrors err;
    err.undeclared_symbols = (VerilogErrorLevel)cli.get("undecl-sym").enum_val;
    err.dangling_logic     = (VerilogErrorLevel)cli.get("dang-logic").enum_val;
    err.unused_output      = (VerilogErrorLevel)cli.get("unused-out").enum_val;
    err.no_driver          = (VerilogErrorLevel)cli.get("no-driver" ).enum_val;
    err.undefined_module   = (VerilogErrorLevel)cli.get("undef-mod" ).enum_val;

    if (cli.get("silent").bool_val)
        err.undeclared_symbols = err.dangling_logic = err.unused_output = err.no_driver = err.undefined_module = vel_Ignore;
    if (cli.get("pedantic").bool_val)
        err.undeclared_symbols = err.dangling_logic = err.unused_output = err.no_driver = err.undefined_module = vel_Error;

    if (output != "" && !(hasSuffix(output, "gig") || hasSuffix(output, "blif"))){
        WriteLn "ERROR! Output file must have extension '.gig' or '.blif'.";
        exit(1); }

    // Include files:
    Vec<String> includes;
    {
        const CLI_Val& v = cli.get("include");
        if (v){
            if (v.choice == 0){
                if (v.string_val != "")
                    includes.push(v.string_val);
            }else{ assert(v.choice == 1);
                for (uint i = 0; i < v.size(); i++)
                    includes.push(v[i].string_val);
            }
        }
    }

    String meta_input;
    if (includes.size() > 0){
        int fd = tmpFile("__vparse_tmp_", meta_input);
        if (fd == -1){
            WriteLn "Could not open temprary file for handling command line includes.";
            exit(1);
        }
        close(fd);

        OutFile out(meta_input);
        for (uint i = 0; i < includes.size(); i++)
            FWriteLn(out) "`include \"%_\"", includes[i];
        FWriteLn(out) "`include \"%_\"", input;
    }

    // Parse Verilog:    
    Vec<VerilogModule> modules;
    SC_Lib L;
    try{
        String prelude;
        if (lib_file != ""){
            if (hasExtension(lib_file, "lib"))
                readLiberty(lib_file, L);
            else
                readSclFile(lib_file, L);
            genPrelude(L, prelude, true);
        }

        readVerilog((meta_input == "") ? input : meta_input, P.store_names, err, modules, prelude.slice());

    }catch(Excp_ParseError err){
        if (meta_input != "") unlink(meta_input.c_str());
        WriteLn "ERROR! %_", err.msg;
        exit(1);
    }
    if (meta_input != "") unlink(meta_input.c_str());
    double T0 = cpuTime();
    WriteLn "Parsing: %t", T0;

    // Dump modules?
    if (dump_mods){
        for (uint i = 0; i < modules.size(); i++){
            NetlistRef N = modules[i].netlist;
            if (N){
                String name = (FMT "mod%d_%_.gig", i, modules[i].mod_name);
                N.write(name);
                WriteLn "Wrote: \a*%_\a*", name;
            }
        }
    }

    if (dump_sigs)
        WriteLn "\n\a*MODULE SIGNATURES:\n\a*\t+\t+%\n_\t-\t-", modules;

    // Map black-boxes onto standard cells:
    IntMap<uint,uint> mod2cell;
    computeUifMap(modules, L, mod2cell);
    verifyPinsSorted(modules, mod2cell, L);

    // Flatten design:
    Netlist N_flat;
    uint top = flatten(modules, N_flat, P);
    if (top == UINT_MAX){
        WriteLn "ERROR! Could not determine top module!";
        exit(1);
    }
    double T1 = cpuTime();
    WriteLn "Flattening: %t", T1-T0;
    WriteLn "Top module: #%_ %_", top, modules[top].mod_name;
    WriteLn "Flattened statistics -- %_", info(N_flat);

    // Remap UIFs:
    remapUifs(N_flat, mod2cell);

    // Write back design in new format:
    if (output != ""){
        if (hasExtension(output, "gig"))
            N_flat.write(output);

        else if (hasExtension(output, "blif")){
            if (L.lib_name == ""){
                ShoutLn "ERROR! You must specify Liberty file when writing BLIF file.";
                exit(1); }
            WriteLn "Writing BLIF file...";
            writeFlatBlifFile(output, modules[top].mod_name, N_flat, L);
        }

        WriteLn "Wrote: \a*%_\a*", output;
    }

    if (output_uif_names != ""){
        OutFile out(output_uif_names);
        for (uint i = 0; i < modules.size(); i++)
            FWriteLn(out) "%_: %_", i, modules[i].mod_name;
    }

    // Compute area and delay:
    Vec<Str> uif_names;
    for (uint i = 0; i < modules.size(); i++)
        uif_names.push(modules[i].mod_name.slice());

    if (delays != "")
        WriteLn "\a*Delay:\a* \a/%_\a/   (using constant gate-delay model)", computeDelays(N_flat, uif_names, delays);
    if (sizes != "")
        WriteLn "\a*Area :\a* \a/%_\a/ ", computeArea(N_flat, uif_names, sizes);

    WriteLn "Mem used: %Db", memUsed();
    WriteLn "CPU time: %t", cpuTime();

    return 0;
}
