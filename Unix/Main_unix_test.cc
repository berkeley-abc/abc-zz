#include "Prelude.hh"
#include "Unix.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    WriteLn "home: %_", homeDir();
    WriteLn "user: %_", userName();

    WriteLn "home een: %_", homeDir("een");
    WriteLn "home niklas: %_", homeDir("niklas");

    return 0;
}
