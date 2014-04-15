//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_techmap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Stand alone binary for calling technology mapper.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_BFunc.hh"
#include "TechMap.hh"
#include "GigReader.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
uint sizeLut4(Wire w)
{
    assert(w == gate_Lut4);
    uint sz = 0;
    sz += (uint)ftb4_inSup(w.arg(), 0);
    sz += (uint)ftb4_inSup(w.arg(), 1);
    sz += (uint)ftb4_inSup(w.arg(), 2);
    sz += (uint)ftb4_inSup(w.arg(), 3);
    return sz;
}


static
String infoLut4(const Gig& N)
{
    uint count[5] = {0, 0, 0, 0, 0};
    For_Gates(N, w){
        if (w == gate_Lut4)
            count[sizeLut4(w)]++;
    }

    String out;
    FWrite(out) "#0=%_  #1=%_  #2=%_  #3=%_  #4=%_",
        count[0], count[1], count[2], count[3], count[4];

    return out;
}


#if 0
static
uint supSize(uint64 ftb)
{
    uint sz = 0;
    for (uint i = 0; i < 6; i++)
        if (ftb6_inSup(ftb, i))
            sz++;
    return sz;
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Read input file:


static
void readInput(String input, Gig& N)
{
    double T0 = cpuTime();
    WSeen keep;
    uint keep_sz = 0;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, false);
        else if (hasExtension(input, "gnl")){
            N.load(input);
        }else if (hasExtension(input, "gig"))
            readGigForTechmap(input, N);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    // Add missing 'gate_Seq's:
    {
        uint n_fixes = 0;
        For_Gates(N, w){
            if (w != gate_Box) continue;

            For_Inputs(w, v){
                if (v != gate_Seq){
                    w.set(Iter_Var(v), N.add(gate_Seq).init(v));
                    n_fixes++;
                }
            }
        }
        if (n_fixes > 0)
            WriteLn "Fixed %_ Box/Seq problems", n_fixes;
    }

    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;
    Write "Input: %_", info(N);
    if (N.typeCount(gate_Lut4) > 0)
        Write "  (LUT-hist. %_)", infoLut4(N);
    if (keep_sz > 0)
        Write "  #keeps=%_", keep_sz;
    NewLine;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Setup commandline:
    cli.add("input", "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.add("cec"  , "bool"  , "no"        , "Output files for equivalence checking.");

    cli.add("cost"   , "{unit, wire}", "wire", "Reduce the number of LUTs (\"unit\") or sum of LUT-inputs (\"wire\").");
    cli.add("rounds" , "uint"  , "3"         , "Number of mapping rounds (with unmapping in between).");
    cli.add("N"      , "uint"  , "6"         , "Cuts to keep per node.");
    cli.add("iters"  , "uint"  , "5"         , "Phases in each mapping.");
    cli.add("rc-iter", "int"   , "3"         , "Recycle cuts from this iteration (-1 = no recycling).");
    cli.add("df"     , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("bal"    , "uint"  , "0"         , "Number of balanced implementations (between delay and area optimal).");
    cli.add("dopt"   , "bool"  , "no"        , "Delay optimize (default is area).");
    cli.add("struct" , "bool"  , "no"        , "Use structural mapping (mostly for debugging/comparison).");
    cli.add("un-and" , "bool"  , "no"        , "Unmap to AND gates instead of richer set of gates.");
    cli.add("batch"  , "bool"  , "no"        , "Output summary line at the end (for tabulation).");

    cli.parseCmdLine(argc, argv);

    // Get mapping options:
    uint n_rounds = cli.get("rounds").int_val;;
    Params_TechMap P;
    P.cuts_per_node  = cli.get("N").int_val;
    P.n_iters        = cli.get("iters").int_val;
    P.recycle_iter   = (uint)cli.get("rc-iter").int_val;
    P.delay_factor   = cli.get("df").float_val;
    P.balanced_cuts  = cli.get("bal").int_val;
    P.struct_mapping = cli.get("struct").bool_val;
    P.unmap_to_ands  = cli.get("un-and").bool_val;
    //P.map_for_delay  = cli.get("dopt").bool_val;
    P.batch_output   = cli.get("batch").bool_val;

    if (cli.get("cost").enum_val == 0){
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = 1;
    }else{
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = i;
    }

    // Read input:
    Gig N;
    readInput(cli.get("input").string_val, N);

    if (cli.get("cec").bool_val){
        writeBlifFile("src.blif", N);
        WriteLn "Wrote: \a*src.blif\a*";
    }

    // Map:
    double T0 = cpuTime();
    techMap(N, P, n_rounds);
    if (!P.batch_output)
        WriteLn "Mapper CPU time: %t", cpuTime() - T0;

    if (cli.get("cec").bool_val){
        writeBlifFile("dst.blif", N);
        WriteLn "Wrote: \a*dst.blif\a*";
        N.save("dst.gnl");
        WriteLn "Wrote: \a*dst.gnl\a*";
    }

    return 0;
}
