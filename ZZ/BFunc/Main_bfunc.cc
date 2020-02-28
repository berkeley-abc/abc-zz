#include "Prelude.hh"
#include "BFunc.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm



#define N 9
#define TESTS 10000


void speedTest()
{
    BFunc<N> f;
    uint64   seed = DEFAULT_SEED;

    // Measure setup time:
    double T0 = realTime();
    uint chk = 0;
    for (uint n = 0; n < TESTS; n++){
        for (uint i = 0; i < f.nWords(); i++){
            f.ftb[i] = irandl(seed) & f.usedMask();
            chk += f.ftb[i];
        }
    }
    double T1 = realTime();

    // Run tests:
    seed = DEFAULT_SEED;
    Vec<uint> cover;
    for (uint n = 0; n < TESTS; n++){
        for (uint i = 0; i < f.nWords(); i++)
            f.ftb[i] = irandl(seed) & f.usedMask();

        cover.clear();
        isop(f, cover);
    }
    double T2 = realTime();


    WriteLn "Setup time: %t    (check-sum: %_)", T1 - T0, chk;
    WriteLn "Run time  : %t", T2 - T1;
    WriteLn "Avg time  : %t", (T2 - T1) / TESTS;
    WriteLn " ~cycles  : %.0f", (T2 - T1) / TESTS * 2.80 * 1000000000;
}


void correctnessTest()
{
    BFunc<8> f;
    f.ftb[0] = 0x11276871;
    f.ftb[1] = 0x1127ABBA;
    f.ftb[2] = 0xFB38AF00;
    f.ftb[3] = 0xD127222A;
    f.ftb[4] = 0x00006D71;
    f.ftb[5] = 0xE1255B7A;
    f.ftb[6] = 0x11F76871;
    f.ftb[7] = 0x4427AAAA;
    Vec<uint> cover;
    isop(f, f, 0, cover);

    Write "0";
    for (uint i = 0; i < cover.size(); i++){
        Write " | (1";
        for (uint j = 0; j < 32; j++){
            if (cover[i] & (1 << j))
                Write " & %Cx%d", (j&1)?'~':0, (j>>1);
        }
        Write ")";
    }
    NewLine;

    //Dump(cover);

    Write "Cover func: ";
    for (uint x7 = 0; x7 < 2; x7++){
    for (uint x6 = 0; x6 < 2; x6++){
    for (uint x5 = 0; x5 < 2; x5++){
    for (uint x4 = 0; x4 < 2; x4++){
    for (uint x3 = 0; x3 < 2; x3++){
    for (uint x2 = 0; x2 < 2; x2++){
    for (uint x1 = 0; x1 < 2; x1++){
    for (uint x0 = 0; x0 < 2; x0++){
        bool v =
            0 | (1 & ~x0 & x4 & ~x5 & x6 & x7) | (1 & x0 & x2 & x3 & ~x4 & x6 & x7) | (1 & ~x0 & x1 & x3 & ~x4 & x6 & x7) | (1 & x2 & ~x3 & x4 & x5 & ~x6 & x7) | (1 & ~x0 & ~x1 & x2 & ~x4 & x5 & ~x6 & x7) | (1 & ~x0 & ~x3 & x5 & ~x6 & x7) | (1 & ~x1 & ~x2 & ~x3 & ~x4 & ~x6 & x7) | (1 & x0 & x1 & ~x3 & ~x4 & x7) | (1 & x0 & ~x1 & ~x3 & x4 & x5 & x6 & ~x7) | (1 & x0 & ~x2 & x4 & ~x5 & x6 & ~x7) | (1 & ~x0 & ~x2 & ~x3 & ~x4 & ~x5 & x6 & ~x7) | (1 & x0 & x1 & x2 & ~x3 & x4 & x6 & ~x7) | (1 & ~x2 & x3 & ~x4 & x5 & ~x6 & ~x7) | (1 & ~x0 & x4 & ~x5 & ~x6 & ~x7) | (1 & x0 & ~x1 & x2 & ~x4 & ~x5 & ~x6 & ~x7) | (1 & x0 & x2 & x3 & ~x4 & ~x6 & ~x7) | (1 & ~x0 & x1 & x3 & ~x4 & ~x6 & ~x7) | (1 & x0 & x1 & x3 & x4 & x5 & ~x7) | (1 & x0 & ~x1 & ~x2 & x4 & x5 & ~x7) | (1 & x0 & ~x1 & ~x2 & ~x3 & ~x4 & ~x5 & ~x7) | (1 & ~x0 & x1 & ~x2 & x3 & x4 & ~x7) | (1 & x0 & x1 & x3 & x4 & x5 & x6) | (1 & x0 & ~x1 & ~x2 & x4 & x5 & x6) | (1 & x0 & x1 & x2 & ~x3 & ~x5 & x6) | (1 & x1 & ~x2 & x3 & x4 & x6) | (1 & ~x0 & ~x1 & x2 & ~x3 & x4 & x6) | (1 & x0 & x1 & ~x3 & ~x4 & x5 & ~x6) | (1 & x0 & x1 & ~x2 & ~x4 & x5 & ~x6) | (1 & ~x0 & x1 & x3 & ~x5 & ~x6) | (1 & ~x0 & x1 & ~x2 & ~x3 & x4 & ~x6) | (1 & ~x0 & x1 & ~x2 & x3 & ~x4 & ~x6) | (1 & ~x0 & ~x1 & x2 & ~x3 & x4 & x5) | (1 & ~x0 & x1 & ~x2 & ~x3 & x4 & x5) | (1 & ~x0 & x2 & x3 & x4 & ~x5) | (1 & ~x0 & x1 & x2 & x4 & ~x5) | (1 & x0 & x2 & x3 & ~x4 & ~x5) | (1 & ~x0 & x1 & ~x2 & x3 & ~x5)
        ;
        Write "%d", (int)v;
    }}}}}}}}
    NewLine;

    Dump(f);
}


int main(int argc, char** argv)
{
    ZZ_Init;

    WriteLn "\a*==== Correctness test:\a*";
    correctnessTest();

    NewLine;
    WriteLn "\a*==== Speed test:\a*";
    speedTest();

    return 0;
}
