#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_Unix.hh"
#include "ZZ_BFunc.hh"
#include "ZZ/Generics/Sort.hh"
#include "TechMap.hh"
#include "GigReader.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cost:


struct Cost {
    float area;
    float delay;
    float runtime;
};


struct ExtCost : Cost {
    float luts;
    float wires;
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Read input file:


static
void readInput(String input, Gig& N)
{
    try{
        if (hasExtension(input, "aig")){
            readAigerFile(input, N, false);
        }else if (hasExtension(input, "aig.gpg") || hasExtension(input, "gnl.gpg")){
            // Start GPG process:
            int pid;
            int io[3];
            Vec<String> args;
            args += "--batch", "--no-use-agent", "--passphrase-file", homeDir() + "/.gnupg/key.txt", "-d", input;
            startProcess("*gpg", args, pid, io);

            // Read file:
            File file(io[1], READ, false);
            In   in(file);
            if (hasExtension(input, "aig.gpg"))
                readAiger(in, N, false);
            else
                N.load(in);
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

    N.compact();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Eval:


static
uint supSize(uint64 ftb)
{
    uint sz = 0;
    for (uint i = 0; i < 6; i++)
        if (ftb6_inSup(ftb, i))
            sz++;
    return sz;
}


static
ExtCost eval(const Gig& N, float runtime)
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
                depth(w) = d + w.arg() * 1.0;     // -- should use DELAY_FRACTION here
            else
                depth(w) = d;
        }
        newMax(max_delay, depth[w]);
    }

    ExtCost ret;
    ret.luts = N.typeCount(gate_Lut6);
    ret.wires = sizeC[1] + 2*sizeC[2] + 3*sizeC[3] + 4*sizeC[4] + 5*sizeC[5] + 6*sizeC[6] + N.typeCount(gate_Mux);
    ret.area = (ret.luts * 4 + ret.wires) / 2;
    ret.delay = max_delay;
    ret.runtime = runtime;

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Globals:


Array<String> bench;

bool          use_f7 = false;
uint          cut_size = 6;
uint          n_rounds = 3;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Script:


struct Script {
    Vec<Params_TechMap> Ps;

    uint size() const { return Ps.size(); }
    Params_TechMap& operator[](uint i) { return Ps[i]; }

    Script() { Ps.growTo(n_rounds); }

    Script(const Script& other)            { other.Ps.copyTo(Ps); }
    Script& operator=(const Script& other) { other.Ps.copyTo(Ps); return *this; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Script randomization:


#define RND(from, upto) (irand(seed, (upto) - (from) + 1) + (from))
#define FLIP bool(irand(seed, 2))


void setDefaultParams(Params_TechMap& P, uint round)
{
    P = Params_TechMap();
    P.quiet = true;

    P.exact_local_area = (round == n_rounds - 1);
    P.refactor = (round < 2);
}


void legalize(Script& script)
{
    if (use_f7)
        script.Ps[LAST].use_fmux = true;
    script.Ps[LAST].cut_size = cut_size;
}


static
void printOptions(Params_TechMap& P, uint round, Out& out)
{
    Params_TechMap P0;
    setDefaultParams(P0, round);

    if (P.unmap.getOptions() != P0.unmap.getOptions())
        FWriteLn(out) "unmap           : %_", P.unmap.getOptions();
    if (P.refactor != P0.refactor)
        FWriteLn(out) "refactor        : %_", P.refactor;
    if (P.cut_size != P0.cut_size)
        FWriteLn(out) "cut_size        : %_", P.cut_size;
    if (P.n_iters != P0.n_iters)
        FWriteLn(out) "n_iters         : %_", P.n_iters;
    if (P.recycle_iter != P0.recycle_iter)
        FWriteLn(out) "recycle_iter    : %_", P.recycle_iter;
    if (P.cuts_per_node != P0.cuts_per_node)
        FWriteLn(out) "cuts_per_node   : %_", P.cuts_per_node;
    if (P.delay_factor != P0.delay_factor)
        FWriteLn(out) "delay_factor    : %_", P.delay_factor;
    if (P.unmap_to_ands != P0.unmap_to_ands)
        FWriteLn(out) "unmap_to_ands   : %_", P.unmap_to_ands;
    if (P.use_fmux != P0.use_fmux)
        FWriteLn(out) "use_fmux        : %_", P.use_fmux;
    if (!vecEqual(Array_new(P.lut_cost, 7), Array_new(P0.lut_cost, 7)))
        FWriteLn(out) "lut_cost        : %_", Array_new(P.lut_cost, 7);
    if (P.mux_cost != P0.mux_cost)
        FWriteLn(out) "mux_cost        : %_", P.mux_cost;
    if (P.slack_util != P0.slack_util){
        if (P.slack_util == FLT_MAX)
            FWriteLn(out) "slack_util      : INF", P.slack_util;
        else
            FWriteLn(out) "slack_util      : %_", P.slack_util;
    }
    if (P.exact_local_area != P0.exact_local_area)
        FWriteLn(out) "exact_local_area: %_", P.exact_local_area;
}


void randomizeParams(Params_TechMap& P, uint64 seed)
{
    setDefaultParams(P, 0);

    P.cut_size         = 6;     // -- for now
    P.n_iters          = RND(1, 6);
    P.recycle_iter     = RND(1, P.n_iters);
    P.cuts_per_node    = RND(2, 10);
    P.delay_factor     = FLIP ? -(int)RND(0, 2) : RND(100, 150) / 100.0;
    P.unmap_to_ands    = FLIP;
    P.use_fmux         = use_f7 && FLIP;
    P.slack_util       = FLIP ? RND(0, 3) : FLT_MAX;
    P.exact_local_area = FLIP;
    P.refactor         = FLIP;
    P.unmap.setOptions(RND(0, 15));

    switch (RND(0, 4)){
    case 0: // LUT cost
        for (uint i = 2; i <= 6; i++) P.lut_cost[i] = 1;
        P.mux_cost = 0;
        break;
    case 1: // Edge cost
        for (uint i = 2; i <= 6; i++) P.lut_cost[i] = i;
        P.mux_cost = 1;
        break;
    case 2: // Mixed cost
        for (uint i = 2; i <= 6; i++) P.lut_cost[i] = 1 + i;
        P.mux_cost = 1;
        break;
    case 3:{// Sorted random
        for (uint i = 2; i <= 6; i++)
            P.lut_cost[i] = float((int)RND(0, 100) - 20) / 10;
        Array<float> arr(P.lut_cost, 7);
        ZZ::sort(arr);
        P.mux_cost = float((int)RND(0, 100) - 20) / 10;
        break;}
    case 4: // Random
        for (uint i = 2; i <= 6; i++)
            P.lut_cost[i] = float((int)RND(0, 100) - 20) / 10;
        P.mux_cost = float((int)RND(0, 100) - 20) / 10;
        break;
    default: assert(false); }
}


void mutateParams(Params_TechMap& P, uint64 seed)
{
    uint choice = RND(0, 15);
    switch (choice){
    case 0: P.n_iters          = RND(1, 6); break;
    case 1: P.recycle_iter     = RND(1, P.n_iters); break;
    case 2: P.cuts_per_node    = RND(2, 10); break;
    case 3: P.delay_factor     = FLIP ? -(int)RND(0, 2) : RND(100, 150) / 100.0; break;
    case 4: P.unmap_to_ands    = !P.unmap_to_ands; break;
    case 5: P.use_fmux         = use_f7 && (!P.use_fmux); break;
    case 6: P.slack_util       = FLIP ? RND(0, 3) : FLT_MAX; break;
    case 7: P.exact_local_area = !P.exact_local_area; break;
    case 8: P.refactor         = !P.refactor; break;
    case 9: P.unmap.setOptions(RND(0, 15)); break;
    default:
        choice -= 10;
        if (choice <= 6)
            P.lut_cost[choice + 2] = float((int)RND(0, 100) - 20) / 10;
        else
            P.mux_cost = float((int)RND(0, 100) - 20) / 10;
    }
}



/*
Ideas:
  - script mutation: add or delete rounde; duplicate round parameters into another existing round
  -  apply mutation to one or all rounds
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Run:


ExtCost run(String input, Script script)
{
    // Read netlist:
    Gig N;
    readInput(input, N);

    // Construct parameters:    
    assert(script.Ps.size() == n_rounds);
    Vec<Params_TechMap>& Ps = script.Ps;

    // Run mapper:
    double T0 = cpuTime();
    techMap(N, Ps);
    double T1 = cpuTime();

    return eval(N, T1-T0);
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input", "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.parseCmdLine(argc, argv);
    String design = cli.get("input").string_val;

    uint64 seed = realTimeAbs();
    WriteLn "Using seed: %_", seed;
    NewLine;

    WriteLn "---------+---------------------+------------------------";
    WriteLn "Runtime  | Delay         Area  |       LUTs        Wires";
    WriteLn "---------+---------------------+------------------------";

    Script script;
    for (uint i = 0; i < script.size(); i++)
        setDefaultParams(script[i], i);

    ExtCost cost0 = run(design, script);
    ExtCost best = cost0;
    WriteLn "%>7%t  |%>6%_ %>12%,d  |%>11%,d %>12%,d%s\a0", cost0.runtime, cost0.delay, (uint64)cost0.area, (uint64)cost0.luts, (uint64)cost0.wires, "    (reference)";

    for (;;){
        Script old_script = script;

        //for (uint n = irand(seed, 5) + 1; n != 0; n--)
        {
            uint k = irand(seed, script.size());
            mutateParams(script[k], seed);
        }
        legalize(script);

        ExtCost cost = run(design, script);
        String comment;
        if (cost.delay <= best.delay && cost.area < best.area){
            best = cost;
            Write "\a/";

            OutFile out("winner_script.txt");
            for (uint i = 0; i < script.size(); i++){
                if (i != 0) FNewLine(out);
                FWriteLn(out) "[Round %_]", i;
                printOptions(script[i], i, out);
            }

            FWrite(comment) "    area -%.2f %%", (cost0.area - best.area) / cost0.area * 100;

        }else
            script = old_script;

        WriteLn "%>7%t  |%>6%_ %>12%,d  |%>11%,d %>12%,d%s\a0", cost.runtime, cost.delay, (uint64)cost.area, (uint64)cost.luts, (uint64)cost.wires, comment;
    }

    return 0;
}
