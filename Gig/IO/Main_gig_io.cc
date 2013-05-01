#include "Prelude.hh"
#include "ZZ_Gig.hh"
#include "Aiger.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


double testtest(const Gig& N)
{
    double T0 = cpuTime();
    for (uint n = 0; n < 10; n++){
        For_Gates(N, w)
            if (w == gate_And)
                w.init(w[1], w[0]);
    }
    double T1 = cpuTime();

    return T1 - T0;
}


int main(int argc, char** argv)
{
    ZZ_Init;

    Dump(sizeof(GLit));
    Dump(sizeof(Wire));
    Dump(sizeof(Gig_data));

    Gig N;
    //WriteLn "Time: %t", testtest(N);

    double T0 = cpuTime();
//    readAigerFile("/home/een/ZZ/Bip/bjrb07amba9andenv_smp.aig", N, true);
    readAigerFile("/home/een/ZZ/Bip/s_00005_0007.aig", N, true);
    WriteLn "Parse time: %t", cpuTime() - T0;

    {
        TimeIt;
        testtest(N);
    }

    upOrderTest(N);
    double T1 = cpuTime();
    N.save("tmp.gnl");
    WriteLn "Save time: %t", cpuTime() - T1;

    return 0;
}
