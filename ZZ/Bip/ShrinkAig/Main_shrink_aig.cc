//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_shrink_aig.cc
//| Author(s)   : Niklas Een
//| Module      : ShrinkAig
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Statistics:


uint n_successes = 0;
uint n_failures  = 0;
uint n_timeouts  = 0;
uint n_tries     = 0;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Copy 'N' to 'M' while removing some gates. Numbering of external elements lost (AIGER 
// don't allow for keeping it if we remove PI/PO/Flops).
template<bool strashed>
void shrinkAig(NetlistRef N, NetlistRef M, uint64& seed)
{
    WSeen remove;

    Vec<GLit> all_gates;
    Vec<GLit> all_cos;
    For_Gates(N, w){
        all_gates.push(w);
        if (type(w) == gate_PO || type(w) == gate_Flop)
            all_cos.push(w);
    }
    if (all_gates.size() == 0){
        WriteLn "Empty netlist! Aborting...";
        exit(0);
    }
    assert(all_cos.size() > 0);

    switch (irand(seed, 3)){
    case 0:
        // Remove a single gate:
        remove.add(all_gates[irand(seed, all_gates.size())] + N);
        break;
    case 1:
        // Remove 5% of gates:
        for (uint i = 0; i <= all_gates.size() / 20; i++)
            remove.add(all_gates[irand(seed, all_gates.size())] + N);
        break;
    case 2:
        // Remove 5% of combinational outputs:
        for (uint i = 0; i <= all_gates.size() / 20; i++)
            remove.add(all_cos[irand(seed, all_cos.size())] + N);
        break;
    default: assert(false); }

    M.clear();
    if (strashed)
        Add_Pob(M, strash);

    WMap<Wire> n2m;
    n2m(N.True()) = M.True();
    n2m(N.False()) = M.False();

    Vec<gate_id> order;
    removeUnreach(N, NULL, false);
    upOrder(N, order);
    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        Wire m;

        if (!remove.has(w)){
            // Copy gate:
            if (type(w) == gate_And){
                /**/if (!+n2m[w[0]]) Dump(w[0], n2m[w[0]], w);
                /**/if (!+n2m[w[1]]) Dump(w[1], n2m[w[1]], w);
                if (strashed)
                    m = s_And(n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]));
                else
                    m = M.add(And_(), n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]));

            }else if (type(w) == gate_Lut4){
                m = M.add(Lut4_(), n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]), n2m[w[2]] ^ sign(w[2]), n2m[w[3]] ^ sign(w[3]));
                attr_Lut4(m).ftb = attr_Lut4(w).ftb;

            }else if (type(w) == gate_PO)
                m = M.add(PO_(), n2m[w[0]] ^ sign(w[0]));

            else if (type(w) == gate_PI)
                m = M.add(PI_());

            else if (type(w) == gate_Flop)
                m = M.add(Flop_());

        }else{
            // Remove gate:
            if (type(w) == gate_And){
                switch (irand(seed, 4)){
                case 0: m = n2m[w[0]] ^ sign(w[0]); break;
                case 1: m = n2m[w[1]] ^ sign(w[1]); break;
                case 2: m =  M.True(); break;
                case 3: m = ~M.True(); break;
                default: assert(false); }

            }else if (type(w) == gate_PI){
                switch (irand(seed, 2)){
                case 0: m =  M.True(); break;
                case 1: m = ~M.True(); break;
                default: assert(false); }

            }else if (type(w) == gate_Flop){
                switch (irand(seed, 2)){
                case 0: m =  M.True(); break;
                case 1: m = ~M.True(); break;
                default: assert(false); }

            }else
                m = M.True();
        }

        /**/if (!(+m != Wire_NULL)) Dump(w);
        /**/assert(+m != Wire_NULL);
        n2m(w) = m;
    }

    For_Gatetype(N, gate_Flop, w)
        if (!remove.has(w))
            n2m[w].set(0, n2m[w[0]] ^ sign(w[0]));

//    removeUnreach(M);
    removeUnreach(M, NULL, false);
}


// Returns exit code from script, or INT_MIN if timed out.
int run(NetlistRef N, String script, String filename, uint timeout)
{
    renumber(N);
    if (hasExtension(filename, "aig")){
        bool ok = writeAigerFile(filename, N); assert(ok);
    }else if (hasExtension(filename, "gig")){
        N.write(filename);
    }else
        assert(false);

    String cmd = (FMT "rm -f __shrink.out; ulimit -t %_; %_ %_ > __shrink.out 2> __shrink.err", timeout, script, filename);
    //**/WriteLn "Command: %_", cmd;
    int ret = system(cmd.c_str());  // <<== this doesn't seem to work! (using hack below instead)

    Str text = readFile("__shrink.out", true);
    //**/WriteLn "text from __shrink.out:\n%_", text;
    if (strstr(text.base(), "%%shrink success%%") != 0)
        ret = 10;
    else if (strstr(text.base(), "%%shrink failed%%") != 0)
        ret = 20;
    else
        ret = INT_MIN;
    dispose(text);

    //**/WriteLn "ret=%_", ret;
    //**/exit(0);
    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Parse commandline:
    cli.add("script", "string", arg_REQUIRED, "Script to run to verify successful shrink. Should return different exit codes for success vs. failure.", 0);
    cli.add("input" , "string", arg_REQUIRED, "Input AIGER file.", 1);
    cli.add("timeout" , "uint | {inf}", "1", "Timeout (in seconds) for script.");
    cli.add("strash" , "bool", "true", "Structurally hash while shrinking.");
    cli.add("seed" , "uint | {auto}", "auto", "Random seed.");

    cli.parseCmdLine(argc, argv);

    String input    = cli.get("input" ).string_val;
    String script   = cli.get("script").string_val;
    uint   timeout  = cli.get("timeout").choice == 0 ? uint(cli.get("timeout").int_val) : UINT_MAX;
    bool   strashed = cli.get("strash").bool_val;
    uint64 seed     = cli.get("seed").choice == 0 ? uint64(cli.get("seed").int_val) : generateSeed();

    // Read input AIGER:
    Netlist N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N);
        else if (hasExtension(input, "gig"))
            N.read(input);
        else{
            ShoutLn "Unknown input format.";
            exit(1);
        }
    }catch (Excp_Msg err){
        ShoutLn "ERROR! %_", err;
        exit(1);
    }
    WriteLn "Read: \a*%_\a* -- %_", input, info(N);
    WriteLn "Seed: %_", seed;

    bool is_aiger = hasExtension(input, "aig");

    // Give user a way to stop shrinking:
    WriteLn "\n    \a*kill -1 %d\a*\n", getpid();
    OutFile out("kill_shrink.sh");
    out %= "kill -1 %d\n", getpid();
    out.close();
    int ignore ___unused = system("chmod +x kill_shrink.sh");

    // Run shrink:
    int valid_exitcode = run(N, script, is_aiger ? "shrunken.aig" : "shrunken.gig", timeout);
    if (valid_exitcode == INT_MIN){
        WriteLn "Timeout set too low; could not run script on original file.";
        exit(1); }
    WriteLn "Successful shrink exit code: %_", valid_exitcode;

    Netlist M;
    uind    empty_sz = M.gateCount();
    uint    n_shrinks = 0;
    for(;;){
        if (N.gateCount() == empty_sz)
            break;

        if (strashed)
            shrinkAig<true>(N, M, seed);
        else
            shrinkAig<false>(N, M, seed);
        int ret = run(M, script, is_aiger ? "shrunken.aig" : "shrunken.gig", timeout);
        n_tries++;
        if (ret == valid_exitcode){
            n_successes++;
            N.clear();
            if (is_aiger)
                readAigerFile("shrunken.aig", N);
            else
                N.read("shrunken.gig");
            n_shrinks++;

            if (fileSize(is_aiger ? "best.aig" : "best.gig") != UINT64_MAX){
                int ret1 ___unused = is_aiger ? system("mv -f best.aig best1.aig") : system("mv -f best.gig best1.gig"); }
            int ret2 ___unused = is_aiger ? system("mv -f shrunken.aig best.aig") : system("mv -f shrunken.gig best.gig");
        }else if (ret == INT_MIN)
            n_timeouts++;
        else
            n_failures++;

        Write "\r";
        if (ret != valid_exitcode) Write "\a*";
        Write "#succ=%_  #fail=%_  #t/o=%_  --  %_", n_successes, n_failures, n_timeouts, info(N);
        if (ret != valid_exitcode) Write "\a*\f";
        else                       Write "\n";
    }


    return 0;
}
