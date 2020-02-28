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
    cli.addCommand("core", "Core based solver.");
    cli.addCommand("sort", "Sorter based solver.");
    cli.addCommand("sort-down", "Sorter based solver, starting with SAT solution [still buggy!].");
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

    if (cli.cmd == "core")
        coreMaxSat(P);
    else if (cli.cmd == "sort")
        sorterMaxSat(P, false);
    else if (cli.cmd == "sort-down")
        sorterMaxSat(P, true);
    else
        assert(false);

    return 0;
}
