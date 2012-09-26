#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "Liberty.hh"
#include "Scl.hh"
#include "GenLib.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input Liberty library file.", 0);
    cli.add("output", "string", arg_REQUIRED, "Output file ('.scl' or '.genlib').", 1);
    cli.parseCmdLine(argc, argv);

    SC_Lib L;
    try{
        WriteLn "Reading: %_", cli.get("input").string_val;
        readLiberty(cli.get("input").string_val, L);
    }catch (Excp_Msg& msg){
        ShoutLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    String output = cli.get("output").string_val;
    bool   ok = true;
    if (hasExtension(output, "scl"))
        ok = writeSclFile(output, L);
    else if (hasExtension(output, "genlib"))
        ok = writeGenlibFile(output, L);
    else{
        ShoutLn "ERROR! Unsupported file extension: %_", output; exit(1); }

    if (!ok){
        ShoutLn "ERROR! Could not write: %_", output; exit(1); }
    else
        WriteLn "Wrote: \a*%_\a*", output;

    return 0;
}
