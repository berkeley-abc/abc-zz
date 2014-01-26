#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Parser.hh"
#include "Solver.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input CNF.", 0);
    cli.parseCmdLine(argc, argv);

    MaxSatProb P;
    try{
        parse_DIMACS(cli.get("input").string_val, P);
    }catch (Excp_Msg msg){
        WriteLn "PARSE ERROR! %_", msg;
        exit(1);
    }

    WriteLn "#vars: %_", P.n_vars;
    WriteLn "#clauses: %_", P.size();

    //for (uint i = 0; i < P.size(); i++)
    //    WriteLn "%_ * %_", (P.weight[i] == UINT64_MAX) ? 0 : P.weight[i], P[i];

    naiveMaxSat(P);

    return 0;
}
