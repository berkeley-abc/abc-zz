#include "Prelude.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_CmdLine.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input AIGER file. May be gzipped.", 0);
    cli.add("output", "string", arg_REQUIRED, "Output AIGER file.", 1);
    cli.add("seed"  , "uint"  , "0"         , "Seed to use for shuffling; 0 means generate a seed.");
    cli.add("pi"    , "bool"  , "yes"       , "Shuffle the order of primary inputs.");
    cli.add("ff"    , "bool"  , "yes"       , "Shuffle the order of flip-flops.");
    cli.add("po"    , "bool"  , "yes"       , "Shuffle the order of primary outputs.");
    cli.add("and"   , "bool"  , "yes"       , "Randomly swap left and right child of AND gates.");
    cli.parseCmdLine(argc, argv);

    // Read input file:
    Netlist N;
    String  input  = cli.get("input").string_val;
    String  output = cli.get("output").string_val;
    if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N);
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }
    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }
    WriteLn "Read: %_ -- %_", input, info(N);

    // Setup random seed:
    uint64 seed = cli.get("seed").int_val;
    if (seed == 0){
        seed = generateSeed();
        WriteLn "Generated seed: %_", seed;
    }

    // Shuffle AIG:
    Vec<GLit> pis, pos, ffs;
    if (cli.get("pi").bool_val){
        For_Gatetype(N, gate_PI, w) pis.push(w);
        shuffle(seed, pis);
        for (uint i = 0; i < pis.size(); i++) attr_PI(pis[i] + N) = i;
    }

    if (cli.get("po").bool_val){
        For_Gatetype(N, gate_PO, w) pos.push(w);
        shuffle(seed, pos);
        for (uint i = 0; i < pos.size(); i++) attr_PO(pos[i] + N) = i;
    }

    if (cli.get("ff").bool_val){
        For_Gatetype(N, gate_Flop, w) ffs.push(w);
        shuffle(seed, ffs);
        for (uint i = 0; i < ffs.size(); i++) attr_Flop(ffs[i] + N) = i;
    }

    if (cli.get("and").bool_val){
        For_Gatetype(N, gate_And, w){
            if (irand(seed, 2)){
                Wire v = w[0];
                w.set(0, w[1]);
                w.set(1, v);
            }
        }
    }

    writeAigerFile(output, N);
    WriteLn "Wrote: %_", output;

    return 0;
}
