#include "Prelude.hh"
#include "ZZ_Gig.hh"
#include "Gig.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    if (argc != 2){
        WriteLn "Expected input file as first argument.";
        exit(2); }

    Gig N;
    try{
        readGigFile(argv[1], N);
    }catch (Excp_Msg msg){
        WriteLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    For_Gates(N, w)
        WriteLn "%f", w;

    return 0;
}
