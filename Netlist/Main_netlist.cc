#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Netlist.hh"
#include "ExportImport.hh"
#include "StdLib.hh"
#include "StdPob.hh"

using namespace ZZ;

namespace ZZ { void test(); }



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    String filename = "test.sif";
    if (argc > 1)
        filename = argv[1];

    Netlist N;
    String module_name;
    Vec<String> liveness_names;
    try{
        readSif(filename, N, &module_name, &liveness_names);
    }catch (Excp_Msg msg){
        WriteLn "PARSE ERROR! %_", msg;
    }

    return 0;
}
