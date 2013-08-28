#include "Prelude.hh"
#include "StdLib.hh"
#include "ZZ_Npn4.hh"

using namespace ZZ;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    ftb4_t x = ftb4_proj[0][0];
    ftb4_t y = ftb4_proj[0][1];
    ftb4_t z = ftb4_proj[0][2];
//    ftb4_t f = (x & ~y & ~z) | (~x & y & ~z) | (~x & ~y & z);
//    ftb4_t f = (x & y & z) | (~x & ~y & ~z);

    ftb4_t f = (x ^ y) | (x & z);

    Npn4Norm n = npn4_norm[f];
    WriteLn "eq_class=%d", n.eq_class;
    WriteLn "perm=%d", n.perm;
    WriteLn "negs=%d", n.negs;

    return 0;
};
