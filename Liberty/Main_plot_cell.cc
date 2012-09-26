#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Liberty.hh"
#include "Scl.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("lib"   , "string"      , arg_REQUIRED, "Library file.", 0);
    cli.add("cell"  , "string"      , arg_REQUIRED, "Name of standard cell.", 1);
    cli.add("data"  , "string"      , ""          , "Write data to file rather than run GNU-plot.", 2);
    cli.add("in"    , "uint"        , "0"         , "Cell input pin number.");
    cli.add("out"   , "uint"        , "0"         , "Cell output pin number.");
    cli.add("curve" , "{delay,slew}", "delay"     , "Which curve category to plot.");
    cli.add("edge"  , "{rise,fall}" , "rise"      , "Rising or falling edge?");
    cli.add("2d"    , "bool"        , "no"        , "Plot a series of 2D curves instead of 3D surface.");
    cli.add("flip"  , "bool"        , "no"        , "Flip load/slew axes.");
    cli.parseCmdLine(argc, argv);

    String lib_file   = cli.get("lib").string_val;
    String cell_name  = cli.get("cell").string_val;
    String data_file  = cli.get("data").string_val;
    uint   in_pin     = cli.get("in").int_val;
    uint   out_pin    = cli.get("out").int_val;
    uint   table      = cli.get("curve").enum_val * 2 + cli.get("edge").enum_val;
    bool   twoD       = cli.get("2d").bool_val;
    bool   flip       = cli.get("flip").bool_val;

    // Read liberty file:
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
    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    // Get cell and output pin:
    uint idx = UINT_MAX;
    for (uint i = 0; i < L.cells.size(); i++){
        if (eq(L.cells[i].name, cell_name)){
            idx = i;
            break; }
    }
    if (idx == UINT_MAX){
        ShoutLn "ERROR! No such standard cell in library: %_", cell_name;
        exit(1); }

    SC_Cell& cell = L.cells[idx];
    if (cell.unsupp){
        ShoutLn "ERROR! Cell is of unsupported type: %_", cell_name;
        exit(1); }
    if (in_pin >= cell.n_inputs){
        ShoutLn "ERROR! Cell has only %_ inputs.", cell.n_inputs;
        exit(1); }
    if (out_pin >= cell.n_outputs){
        ShoutLn "ERROR! Cell has only %_ outputs.", cell.n_outputs;
        exit(1); }

    SC_Pin& pin = cell.pins[cell.n_inputs + out_pin];
    if (pin.rtiming[in_pin].size() == 0){
        ShoutLn "ERROR! There is no dependency between output '%_' and input '%_'.", out_pin, in_pin;
        exit(1); }
    assert(pin.rtiming[in_pin].size() == 1);

    // Print some info on pin:
    WriteLn "----------------------------------------";
    WriteLn "function    : \a*%_\a*", pin.func;
    WriteLn "max_out_cap : \a*%_\a*", pin.max_out_cap;
    WriteLn "max_out_slew: \a*%_\a*", pin.max_out_slew;
    WriteLn "----------------------------------------";

    // Produce GNU-plot table:
    String gnu_data;
    String gnu_cmds;
    {
        SC_Timing& timing = pin.rtiming[in_pin][0];
        SC_Surface& s = (table == 0) ? timing.cell_rise  :
                        (table == 1) ? timing.cell_fall  :
                        (table == 2) ? timing.rise_trans :
                        /*otherwise*/  timing.fall_trans ;

        if (flip){
            Vec<Vec<float> > data;
            for (uint i = 0; i < s.index0.size(); i++)
                for (uint j = 0; j < s.index1.size(); j++)
                    data(j)(i) = s.data[i][j];
            data.moveTo(s.data);
            swp(s.index0, s.index1);
        }

        int    fd = tmpFile("__plot_data", gnu_data);
        File   wr(fd, WRITE);
        Out    out(wr);

        putF(out, s.index1.size());
        for (uint j = 0; j < s.index1.size(); j++)
            putF(out, s.index1[j]);
        for (uint i = 0; i < s.index0.size(); i++){
            putF(out, s.index0[i]);
            for (uint j = 0; j < s.index1.size(); j++)
                putF(out, s.data[i][j]);
        }
    }
    {
        cchar* table_name[4] = { "delay-rise", "delay-fall", "slew-rise", "slew-fall" };

        String title;

        int    fd = tmpFile("__plot_cmds", gnu_cmds);
        File   wr(fd, WRITE);
        Out    out(wr);

        FWriteLn(out) "set xlabel \"%_\"", flip ? "slew" : "load";
        if (twoD){
            FWrite(title) "\"%_: %_->%_\"", cell.name, cell.pins[in_pin].name, cell.pins[out_pin + cell.n_inputs].name;
            FWriteLn(out) "set ylabel \"%_\"", table_name[table];
            FWriteLn(out) "plot \"%_\" binary with linespoints title %_", gnu_data, title;
        }else{
            FWrite(title) "\"%_: %_->%_  [%_]\"", cell.name, cell.pins[in_pin].name, cell.pins[out_pin + cell.n_inputs].name, table_name[table];
            FWriteLn(out) "set ylabel \"%_\"", flip ? "load" : "slew";
            FWriteLn(out) "splot \"%_\" binary with lines title %_", gnu_data, title;
        }
        FWriteLn(out) "pause -1";
    }

    // Call GNU-plot:
    String cmd;
    if (data_file == ""){
        FWrite(cmd) "gnuplot %_", gnu_cmds;
        int ignore ___unused = system(cmd.c_str());
        remove(gnu_data.c_str());

    }else{
        FWrite(cmd) "mv %_ %_; cat %_ | sed 's/%_/%_/'", gnu_data, data_file, gnu_cmds, gnu_data, data_file;
        WriteLn "-------------------------------------------------------------------------------";
        int ignore ___unused = system(cmd.c_str());
        WriteLn "-------------------------------------------------------------------------------";
    }
    remove(gnu_cmds.c_str());


    return 0;
}
