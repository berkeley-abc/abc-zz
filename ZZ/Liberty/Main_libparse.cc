#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Scl.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line options:
    cli.add("lib" , "string"            , arg_REQUIRED, "Input Liberty library file.", 0);
    cli.add("ttm" , "{keep,merge,first}", "merge"     , "Timing tables mode.");
    cli.add("dump", "bool"              , "no"        , "Dump parsed data to standard output.");
    cli.add("uifs", "string"            , ""          , "Write UIF map to this file.");
    cli.parseCmdLine(argc, argv);

    String lib_file = cli.get("lib").string_val;
    bool   dump = cli.get("dump").bool_val;
    uint   ttm = cli.get("ttm").enum_val;
    String uifs_file = cli.get("uifs").string_val;

    // Read input:
    SC_Lib L;
    try{
        cpuClock();
        if (hasExtension(lib_file, "lib")){
            readLiberty(lib_file, L, (TimingTablesMode)ttm);
            WriteLn "Reading liberty file: \a*%t\a*", cpuClock();
        }else{
            readSclFile(lib_file, L);
            WriteLn "Reading SCL file: \a*%t\a*", cpuClock();
        }
    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    NewLine;
    WriteLn "CPU time : \a*%t\a*", cpuTime();
    WriteLn "Real time: \a*%t\a*", realTime();
    WriteLn "Memory   : \a*%DB\a*", memUsed();

    if (dump)
        WriteLn "%_", L;

    if (uifs_file != ""){
        OutFile out(uifs_file);
        for (uint i = 0; i < L.cells.size(); i++)
            FWriteLn(out) "%_=%_", i, L.cells[i].name;
    }

    return 0;
}
