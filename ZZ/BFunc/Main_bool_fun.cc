#include "Prelude.hh"
#include "BoolFun.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    BoolFun f(8, Lit(0));
    Dump(f.nVars());
    BoolFun g(8, ~Lit(6));

    Dump(f.nVars());
    Dump(g.nVars());

    WriteLn "%.16X", f.ftb();
    WriteLn "%.16X", g.ftb();

    g &= f;
    WriteLn "%.16X", g.ftb();

    g.inv();
    WriteLn "%.16X", g.ftb();

    return 0;
}
