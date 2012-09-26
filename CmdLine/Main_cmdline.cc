//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_cmdline.cc
//| Author(s)   : Niklas Een
//| Module      : CmdLine
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "CmdLine.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


String validateCommandline(CLI& cli)
{
    const CLI_Val& val = cli.get("even");
    assert(val.type == cli_Int);
    if (val.int_val % 2 != 0)
        return "'-even' takes an even number.";

    return "";
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input graph file.", 0);
    cli.add("result", "string", "", "Output result file.", 1);
    cli.add("seed", "uint | {auto}", "auto", "Random seed.");
    cli.add("assumps", "[int|string]", "[]", "Additional unit assumptions.");

    CLI bisect;
    cli.addCommand("bisect", "Bisect graph", &bisect);
    bisect.add("tol", "float[0+:1-]", "0.1", "Tolerance factor (0.1 means 10% imbalance).");

    cli.parseCmdLine(argc, argv);

#if 0
    cli_hidden.add("depth", "uint | {inf}", "inf", "Max search depth");

    CLI place;
    place.add("name", "string", arg_REQUIRED, "State your name here", 0);

    CLI route;
    route.add("time", "ufloat", "10", "In seconds", 0);

    cli.addCommand("place", "Run placement. Blah blurg, bliffen, bloy. Bliven blav blatt! Blurti blarti blitty?", &place);
    cli.addCommand("route", "Run routing", &route);

    cli.add("input", "string", arg_REQUIRED, "Input graph.", 0);
    cli.add("seed", "uint | {auto}", "auto", "Random seed. 'auto' means generate from clock.");
    cli.add("even", "uint", "1234", "An even number.", 1);

    cli.add("even2", "uint", "1234", "An even number.", 3);
    cli.add("a-very-long-switch-name", "uint", "1234", "An even number.", 2);
    cli.add("something-altogether-different", "uint", "1234", "An even number.", 4);
    cli.add("x", "uint", "1234", "An even number.", 5);
    cli.add("y", "uint", "1234", "An even number.", 6);
    cli.add("z", "uint", "1234", "An even number.", 7);

    CLI inner;
    inner.add("gurka", "int | [int[0:]]", "[12,47]", "The gurks");
    inner.addCommand("shuffle", "The deck", &route);
    cli.embed(inner, "inner-");
    cli.postProcess(validateCommandline);

    CLI deep;
    deep.add("tomato", "{one, two, three} | [string]", "two", "How many tomatos?");
    inner.embed(deep, "deep-");

    cli.parseCmdLine(argc, argv);

    for (uind i = 0; i < cli.args.size(); i++){
        WriteLn "NAME: %_", cli.args[i].x.name;
        WriteLn "POS : %_", cli.args[i].x.pos;
        WriteLn "SIG : %_", cli.args[i].x.sig;
        WriteLn "VAL : %_", cli.args[i].y;
        NewLine;
    }

    for (uind j = 0; j < cli.embedded.size(); j++){
        WriteLn "EMBEDDED: %_\t+\t+\t+\t+", cli.embedded[j].y;
        CLI& cli2 = *cli.embedded[j].x;

        for (uind i = 0; i < cli2.args.size(); i++){
            WriteLn "NAME: %_", cli2.args[i].x.name;
            WriteLn "POS : %_", cli2.args[i].x.pos;
            WriteLn "SIG : %_", cli2.args[i].x.sig;
            WriteLn "VAL : %_", cli2.args[i].y;
            NewLine;
        }

        Write "\t-\t-\t-\t-";
    }
#endif

    return 0;
}
