#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool dfs(Wire w, WMap<uchar>& color)
{
    if (color[w] == 2 || isCI(w))
        return true;

    if (color[w] == 1){
        WriteLn "%f", w;
        return false;
    }else{
        color(w) = 1;
        For_Inputs(w, v){
            if (!dfs(v, color)){
                WriteLn "%f", w;
                return false;
            }
        }
        color(w) = 2;
        return true;
    }
}


int main(int argc, char** argv)
{
    ZZ_Init;

    // Read input:
    cli.add("input", "string", arg_REQUIRED, "Input GNL.", 0);
    cli.parseCmdLine(argc, argv);
    String input = cli.get("input").string_val;

    Gig N;
    try{
        N.load(input);
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    // Find combinational cycle (if any):
    WMap<uchar> color(0);
    For_Gates(N, w)
        if (isCO(w))
            if (!dfs(w, color))
                return 1;

    WriteLn "No combinational cycles detected.";

    return 0;
}
