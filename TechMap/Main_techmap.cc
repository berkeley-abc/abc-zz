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
#include "ZZ_Unix.hh"
#include "TechMap.hh"
#include "GigReader.hh"
#include "PostProcess.hh"

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


static
uint supSize(uint64 ftb)
{
    uint sz = 0;
    for (uint i = 0; i < 6; i++)
        if (ftb6_inSup(ftb, i))
            sz++;
    return sz;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Read input file:


static
void readInput(String input, Gig& N)
{
    double T0 = cpuTime();
    WSeen keep;
    uint keep_sz = 0;
    try{
        if (hasExtension(input, "aig")){
            readAigerFile(input, N, false);
        }else if (hasExtension(input, "aig.gpg")){
            // Start GPG process:
            int pid;
            int io[3];
            Vec<String> args;
            args += "--batch", "--no-use-agent", "--passphrase-file", homeDir() + "/.gnupg/key.txt", "-d", input;
            startProcess("*gpg", args, pid, io);

            // Read file:
            File file(io[1], READ, false);
            In   in(file);
            readAiger(in, N, false);
            closeChildIo(io);
            waitpid(pid, NULL, 0);

        }else if (hasExtension(input, "gnl")){
            N.load(input);
        }else if (hasExtension(input, "gig")){
            readGigForTechmap(input, N);
        }else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    // Add missing 'gate_Seq's:
#if 0
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
#endif

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


void printStats(const Gig& N, float delay_fraction)
{
    Vec<uint> sizeC(7, 0);
    WMap<uint> depth;
    uint max_delay = 0;
    For_UpOrder(N, w){
        if (w == gate_Lut6)
            sizeC[supSize(ftb(w))]++;

        if (isCI(w))
            depth(w) = 0;
        else if (w == gate_F7Mux || w == gate_F8Mux)
            depth(w) = max_(depth[w[0]] + 1, max_(depth[w[1]], depth[w[2]]));
        else{
            uint d = 0;
            For_Inputs(w, v)
                newMax(d, depth[v]);
            if (isLogicGate(w))
                depth(w) = d + 1;
            else if (w == gate_Delay)
                depth(w) = d + w.arg() * delay_fraction;     // -- should use DELAY_FRACTION here
            else
                depth(w) = d;
        }
        newMax(max_delay, depth[w]);
    }

    NewLine;
    WriteLn "Statistics:";
    for (uint i = 0; i <= 6; i++)
        if (sizeC[i] > 0)
            WriteLn "    LUT %_: %>11%,d  (%.1f %%)", i, sizeC[i], double(sizeC[i]) / N.typeCount(gate_Lut6) * 100;
    if (N.typeCount(gate_F7Mux) > 0)
        WriteLn "\n    F7MUX: %>11%,d", N.typeCount(gate_F7Mux);
    if (N.typeCount(gate_F8Mux) > 0)
        WriteLn "    F8MUX: %>11%,d", N.typeCount(gate_F8Mux);
    NewLine;
    WriteLn "    LUTs : %>11%,d", N.typeCount(gate_Lut6);
    WriteLn "    Wires: %>11%,d", sizeC[1] + 2*sizeC[2] + 3*sizeC[3] + 4*sizeC[4] + 5*sizeC[5] + 6*sizeC[6] + N.typeCount(gate_Mux);
    WriteLn "    Delay: %>11%,d", max_delay;
    NewLine;
}


void mapWithSignals(Gig& N, const Params_TechMap& P, uint n_rounds, uint sig)
{
    WMapX<GLit> remap;
    Gig N_orig;
    N.copyTo(N_orig);

    // Store signals that feed into non-logic (and check that they are remapped):
    WZet sink_logic;
    For_Gates(N, w){
        if (!isTechmapLogic(w)){
            For_Inputs(w, v){
                if (isTechmapLogic(v)){
                    sink_logic.add(+v); }
            }
        }
    }

    techMap(N, P, n_rounds, &remap);

    if (sig == 2){      // -- 2 == 'pos' (no negative literals)     // <<== move this into mapper itself (as an option)?
        double T0 = cpuTime();
        uint n_luts = N.typeCount(gate_Lut6);
        removeRemapSigns(N, remap);
        removeInverters(N, &remap);
        WriteLn "Inverter removal time: %t", cpuTime() - T0;
        WriteLn "LUTs added: %_", N.typeCount(gate_Lut6) - n_luts;

        For_Gates(N, w)
            For_Inputs(w, v)
                assert(!v.sign);
    }

    // Check that sinks are remapped:
    for (uint i = 0; i < sink_logic.size(); i++){
        Wire w = sink_logic[i] + N_orig;
        if (!remap[w]){
            ShoutLn "INTERNAL ERROR! Missing gate in remap: %f", w;
            For_Gates(N_orig, w0){
                if (!isTechmapLogic(w0)){
                    For_Inputs(w0, v){
                        if (+v == w)
                            ShoutLn "  -- feeding: %f   (remapped to: %_)", w0, remap[w0];
                    }
                }
            }
            //assert(false);
        }
    }

    // Internal points verification -- turn combinational inputs into PIs and add POs to every internal point:
    For_Gatetype(N_orig, gate_PO, w)
        remove(w);
    For_Gatetype(N, gate_PO, w)
        remove(w);

    /**/WSeen vs, ns;
    For_Gates(N_orig, w){
        if (w == gate_PI || w == gate_PO) continue;

        if (isLogicGate(w)){
            For_Inputs(w, v){
                if (!isLogicGate(v) && v != gate_PI && v != gate_Const){
                    Wire n = remap[v] + N; assert(+n); assert(n != gate_PI && n != gate_Const);
                    /**/if (vs.has(v)) WriteLn "Duplicate in v: %_", v;
                    /**/if (ns.has(n)) WriteLn "Duplicate in n: %_", n;
                    /**/vs.add(v); ns.add(n);
                    change(v, gate_PI);
                    change(n, gate_PI);
                    assert(v.num() == n.num());
                }
            }
        }else{
            Wire m = remap[w] + N; assert(+m);
            assert(w.size() == m.size());
            for (uint i = 0; i < w.size(); i++){
                if (!w[i]){ assert(!m[i]); continue; }
                assert(m[i]);

                if (!isLogicGate(w[i])) continue;

                N_orig.add(gate_PO).init(w[i]);
                N     .add(gate_PO).init(m[i]);
            }
        }
    }

    For_UpOrder(N_orig, w)
        if (!isLogicGate(w) && w != gate_PI && w != gate_PO)
            remove(w);

    For_UpOrder(N, m)
        if (!isLogicGate(m) && m != gate_PI && m != gate_PO)
            remove(m);

    For_Gates(N_orig, w)
        For_Inputs(w, v){
            /**/if (!(!v.isRemoved())) Dump(w);
            assert(!v.isRemoved());
        }

    For_Gates(N, w)
        For_Inputs(w, v)
            if (v.isRemoved()){ remove(w); break; }

    WriteLn "Signal tracking verificaton:";
    WriteLn "  Transformed original: %_", info(N_orig);
    WriteLn "  Transformed mapped  : %_", info(N);

    uint count = 0;
    Vec<GLit>& v = remap.base();
    for (uint i = gid_FirstUser; i < v.size(); i++){
        if (v[i] && isLogicGate(v[i] + N)){
            count++;
            N_orig.add(gate_PO).init(N_orig[i]);
            N.add(gate_PO).init(v[i]);
        }
    }

    WriteLn "  Signals retained: %_", count;

    writeBlifFile("src.blif", N_orig);
    WriteLn "Wrote: \a*src.blif\a*";

    writeBlifFile("dst.blif", N);
    WriteLn "Wrote: \a*dst.blif\a*";
}


int main(int argc, char** argv)
{
    ZZ_Init;

    setupSignalHandlers();

    // Setup commandline:
    cli.add("input"   , "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.add("output"  , "string", "",           "Output GNL.", 1);
    cli.add("cec"     , "bool"  , "no"        , "Output files for equivalence checking.");
    cli.add("sig"     , "{off,full,pos}","off", "[DEBUG] Map with signal tracking? (\"full\" includes negative literals).");
    cli.add("cost"    , "{unit, wire, mix}", "wire",
                                                "Reduce the number of LUTs (\"unit\") or sum of LUT-inputs (\"wire\").");
    cli.add("mux-cost", "float" , "-1"        , "Cost of a mux; -1 means use default depending on 'cost'.");
    cli.add("rounds"  , "uint"  , "3"         , "Number of mapping rounds (with unmapping in between).");
    cli.add("N"       , "uint"  , "8"         , "Cuts to keep per node.");
    cli.add("iters"   , "uint"  , "5"         , "Phases in each mapping.");
    cli.add("rc-iter" , "int"   , "3"         , "Recycle cuts from this iteration (-1 = no recycling).");
    cli.add("df"      , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("struct"  , "bool"  , "no"        , "Use structural mapping (mostly for debugging/comparison).");
    cli.add("un-and"  , "bool"  , "no"        , "Unmap to AND gates instead of richer set of gates.");
    cli.add("fmux"    , "bool"  , "no"        , "Use F7/F8 MUXes.");
    cli.add("fmux-ff" , "bool"  , "no"        , "F7/F8 MUXes can feed flip-flops ('Seq' gate).");
    cli.add("slack"   , "{max} | float", "max", "Slack utilization. Smaller values means better average slack (but worse area).");
    cli.add("ela"     , "bool"  , "no"        , "Exact local area.");
    cli.add("refact"  , "bool"  , "no"        , "Refactoring (applied after unmapping)..");
    cli.add("unmap"   , "int[0:15]", "14"     , "Unmap options; see 'Unmap.hh'.");
    cli.add("batch"   , "bool"  , "no"        , "Output summary line at the end (for tabulation).");

    cli.parseCmdLine(argc, argv);

    // Get mapping options:
    uint n_rounds = cli.get("rounds").int_val;;
    Params_TechMap P;
    P.cuts_per_node    = cli.get("N").int_val;
    P.n_iters          = cli.get("iters").int_val;
    P.recycle_iter     = (uint)cli.get("rc-iter").int_val;
    P.delay_factor     = cli.get("df").float_val;
    P.struct_mapping   = cli.get("struct").bool_val;
    P.unmap_to_ands    = cli.get("un-and").bool_val;
    P.use_fmux         = cli.get("fmux").bool_val;
    P.fmux_feeds_seq   = cli.get("fmux-ff").bool_val;
    P.exact_local_area = cli.get("ela").bool_val;
    P.refactor         = cli.get("refact").bool_val;
    P.unmap.setOptions(cli.get("unmap").int_val);
    P.batch_output     = cli.get("batch").bool_val;
    if (cli.get("slack").choice == 1)
        P.slack_util = cli.get("slack").float_val;

    if (cli.get("cost").enum_val == 0){
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = 1;
        P.mux_cost = 0;
    }else if (cli.get("cost").enum_val == 1){
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = i;
        P.mux_cost = 1;
    }else{
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = i + 1;
        P.mux_cost = 1;
    }
    float mux_cost = cli.get("mux-cost").float_val;
    if (mux_cost != -1)
        P.mux_cost = mux_cost;

    // Read input:
    Gig N;
    readInput(cli.get("input").string_val, N);

    if (cli.get("cec").bool_val){
        writeBlifFile("src.blif", N);
        WriteLn "Wrote: \a*src.blif\a*";
    }

    // Map:
    double T0 = cpuTime();
    if (cli.get("sig").enum_val == 0)
        techMap(N, P, n_rounds);
    else
        mapWithSignals(N, P, n_rounds, cli.get("sig").enum_val);

    if (!P.batch_output){
        printStats(N, P.delay_fraction);
        WriteLn "CPU time: %t", cpuTime() - T0;
    }

    if (cli.get("cec").bool_val){
        writeBlifFile("dst.blif", N);
        WriteLn "Wrote: \a*dst.blif\a*";
        N.save("dst.gnl");
        WriteLn "Wrote: \a*dst.gnl\a*";
    }

    if (cli.get("output").string_val != ""){
        N.save(cli.get("output").string_val);
        WriteLn "Wrote: \a*%_\a*", cli.get("output").string_val;
    }

    return 0;
}
