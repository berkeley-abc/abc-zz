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

    Netlist N;
    String module_name;
    Vec<String> liveness_names;
    try{
        readSif("../Bip/constr.sif", N, &module_name, &liveness_names);
    }catch (Excp_Msg msg){
        WriteLn "PARSE ERROR! %_", msg;
    }

    return 0;
}
