#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Md5.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input file.", 0);
    cli.parseCmdLine(argc, argv);

    String input = cli.get("input").string_val;
    Array<char> data = readFile(input);
    if (!data){
        ShoutLn "ERROR! Could not read %_", input;
        exit(1); }

    md5_hash h = md5(data);
    WriteLn "%.16x%.16x", h.snd, h.fst;

    return 0;
}
