#include "Prelude.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

using namespace ZZ;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    Gig N;
    Dump(N.isEmpty());
    return 0;

    Wire sel = N.add(gate_PI);
    Wire d1  = N.add(gate_PI);
    Wire d0  = N.add(gate_PI);

    Wire r1 = N.add(gate_And).init(sel, ~d1);
    Wire r0 = N.add(gate_And).init(~sel, ~d0);
    Wire f  = N.add(gate_And).init(~r0, ~r1);

    Dump(sel, d1, d0);
    if (isMux(f, sel, d1, d0))
        Dump(sel, d1, d0);

    Dump(isReach(N));

    for (uint i = 0; i < N.size(); i++)
        Dump(N[i]);

    return 0;
};
