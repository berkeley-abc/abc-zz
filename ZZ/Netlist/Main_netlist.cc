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
#if 0
    Wire a = N.add(Ltl_()); N.names().add(a, "a");
    Wire b = N.add(Ltl_()); N.names().add(b, "b");
    Wire c = N.add(Ltl_()); N.names().add(c, "c");
    Wire f = N.add(Ltl_('U'), a, b);
    Wire g = N.add(Ltl_('F', 20), Wire_NULL, f);
    Wire h = N.add(Ltl_('$', 20), Wire_NULL, g);
    N.write("tmp.gig");
#endif

    Netlist M;
    M.read("tmp.gig");
    M.write("tmp2.gig");

    return 0;
}
