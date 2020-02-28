#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Verilog.hh"
#include "ZZ_Liberty.hh"
#include "TimingRef.hh"
/**/#include "OrgCells.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("design", "string"               , arg_REQUIRED  , "Input structural Verilog file (or .gig).", 0);
    cli.add("lib"   , "string"               , arg_REQUIRED  , "Input Liberty library file.", 1);
    cli.add("apx"   , "{none,lin,satch,quad}", "none"        , "Approximation to use for timing; 'none' uses Liberty tables.");
    cli.add("wmod"  , "string"               , ""            , "Override default selection of wire-load model.");
    cli.add("slack" , "uint"                 , "0"           , "Use GNU-plot to plot the slack for all gates (1) or just POs (2).");
    cli.add("dump"  , "string"               , ""            , "[DEBUG] Write flattened netlist to file.");
    cli.addCommand("all", "Time whole design.");

    CLI cli_one;
    cli_one.add("gate", "string", "", "Gate to time");
    cli_one.add("in-gate", "string", "", "Input gate");
    cli_one.add("out-pin", "uint", "0", "Output pin number");
    cli_one.add("load", "(ufloat,ufloat)", "(0, 0)", "Output capacitance of gate");
    cli_one.add("in-slew", "(ufloat,ufloat)", "(0, 0)", "Input slew of gate");
    cli.addCommand("one", "Time a single gate.", &cli_one);
    cli.parseCmdLine(argc, argv);

    String design_file = cli.get("design").string_val;
    String lib_file = cli.get("lib").string_val;
    uint   approx = cli.get("apx").enum_val;
    String wire_load_model = cli.get("wmod").string_val;
    uint   plot_slack = cli.get("slack").enum_val;
    String dump_file = cli.get("dump").string_val;
    if (hasExtension(design_file, "lib") || hasExtension(design_file, "scl"))
        swp(design_file, lib_file);

    // Read input:
    Vec<VerilogModule> modules;
    SC_Lib L;
    Netlist N;
    bool verilog;
    try{
        cpuClock();
        if (hasExtension(lib_file, "lib")){
            readLiberty(lib_file, L);
            WriteLn "Reading liberty file: \a*%t\a*", cpuClock();
        }else{
            readSclFile(lib_file, L);
            WriteLn "Reading SCL file: \a*%t\a*", cpuClock();
        }

        if (hasExtension(design_file, "v")){
            String prelude;
            genPrelude(L, prelude, true);
            readVerilog(design_file, /*store_names*/true, /*error_level*/VerilogErrors(vel_Ignore), modules, prelude.slice());
            WriteLn "Reading design: \a*%t\a*", cpuClock();
            verilog = true;
        }else{
            N.read(design_file);
            verilog = false;
        }

    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    if (verilog){
        // Map black-boxes onto standard cells:
        IntMap<uint,uint> mod2cell;
        computeUifMap(modules, L, mod2cell);

        // Align input pin order in netlist and library:
        verifyPinsSorted(modules, mod2cell, L);

        // Flatten design:
        Params_Flatten P;
        P.store_names = 2;
        P.strict_aig = false;

        uint top = flatten(modules, N, P);
        if (top == UINT_MAX){
            ShoutLn "ERROR! Could not determine top module!";
            exit(1); }

        // Remap UIFs:
        remapUifs(N, mod2cell);

        WriteLn "Flatten design: \a*%t\a*", cpuClock();
    }

    WriteLn "Design size: %_", info(N);
    NewLine;

    // Dump flattened file?
    if (dump_file != "")
        N.write(dump_file);

    if (cli.cmd == "all"){
        // Compute static timing:
        reportTiming(N, L, approx, plot_slack, wire_load_model);
        WriteLn "Static timing: \a*%t\a*", cpuClock();

    }else if (cli.cmd == "one"){
        // Time a single gate:
        String gate    = cli.get("gate").string_val;
        uint   out_pin = cli.get("out-pin").int_val;
        String in_gate = cli.get("in-gate").string_val;

        N.names().enableLookup();
        Wire w = N.names().lookup(gate.slice()) + N;
        if (!w){
            ShoutLn "ERROR! No such gate: %_", gate;
            exit(1); }
        if (type(w) != gate_Uif){
            ShoutLn "ERROR! Gate is not a standard cell: %_", gate;
            exit(1); }

        uint cell_idx = attr_Uif(w).sym;
        SC_Cell& cell = L.cells[cell_idx];

        if (w.size() + out_pin >= cell.pins.size()){
            ShoutLn "ERROR! Too large output pin given: %_", out_pin;
            exit(1); }

        SC_Pin& pin = cell.pins[w.size() + out_pin];

        Wire w_in = N.names().lookup(in_gate.slice()) + N;
        uint in_pin = UINT_MAX;
        For_Inputs(w, v){
            if (+w_in == +v){
                in_pin = Iter_Var(v);
                break;
            }
        }
        if (in_pin == UINT_MAX){
            ShoutLn "ERROR! Input gate is not a fanin of main gate: %_", in_gate;
            exit(1); }

        SC_Timings& ts = pin.rtiming[in_pin];
        if (ts.size() == 0){
            WriteLn "Output pin does not depend on input pin; nothing to time!";
            exit(0); }
        assert(ts.size() == 1);
        SC_Timing& t = ts[0];

        TValues arr_in, slew_in, load, arr, slew;
        slew_in.rise = cli.get("in-slew")[0].float_val / 1000.0;       // <<== "/1000" is a hack; fix this later
        slew_in.fall = cli.get("in-slew")[1].float_val / 1000.0;
        load   .rise = cli.get("load")   [0].float_val / 1000.0;
        load   .fall = cli.get("load")   [1].float_val / 1000.0;

        timeGate(t, arr_in, slew_in, load, approx, arr, slew);

        NewLine;
        WriteLn "slew=\a/%_\a/   delay=\a/%_\a/   (timing-sense: %_)",
            max_(slew.rise, slew.fall) * 1000, max_(arr.rise, arr.fall) * 1000,     // <<== ditto
            t.tsense;

    }else
        assert(false);

    NewLine;
    WriteLn "CPU time : \a*%t\a*", cpuTime();
    WriteLn "Real time: \a*%t\a*", realTime();
    WriteLn "Memory   : \a*%DB\a*", memUsed();

    return 0;
}
