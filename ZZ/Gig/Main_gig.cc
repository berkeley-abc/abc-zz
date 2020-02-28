#include "Prelude.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

using namespace ZZ;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Gig N;

    Wire sel = N.add(gate_PI);
    Wire d1  = N.add(gate_PI);
    Wire d0  = N.add(gate_PI);

    Wire r1 = N.add(gate_And).init(sel, ~d1);
    Wire r0 = N.add(gate_And).init(~sel, ~d0);
    Wire f ___unused = N.add(gate_And).init(~r0, ~r1);

    N.save("tmp.gnl");

    Gig M;
    M.load("tmp.gnl");

    WriteLn "N: %_", info(N);
    WriteLn "M: %_", info(M);

    return 0;
};
