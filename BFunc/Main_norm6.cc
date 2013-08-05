#include "Prelude.hh"
#include "Ftb6.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    uint64 ftb = ftb6_proj[0][1] | ftb6_proj[0][4];
    for (uint i = 0; i < 6; i++){
        WriteLn "pin %_: %_", i, ftb6_inSup(ftb, i);
    }

    return 0;
}
