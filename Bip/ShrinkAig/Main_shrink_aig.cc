//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_shrink_aig.cc
//| Author(s)   : Niklas Een
//| Module      : ShrinkAig
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
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
    GLit pick = GLit(irand(seed, N.size() - gid_FirstUser) + gid_FirstUser);
    assert(!deleted(N[pick]));

    M.clear();
    if (strashed)
        Add_Pob(M, strash);

    WMap<Wire> n2m;
    n2m(N.True()) = M.True();

    Vec<gate_id> order;
    removeUnreach(N, NULL, false);
    upOrder(N, order);
    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        Wire m;

        if (w != pick){
            // Copy gate:
            if (type(w) == gate_And){
                //**/if (!n2m[w[1]]) Dump(w[1], n2m[w[1]], pick);
                if (strashed)
                    m = s_And(n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]));
                else
                    m = M.add(And_(), n2m[w[0]] ^ sign(w[0]), n2m[w[1]] ^ sign(w[1]));

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
                switch (irand(seed, 3)){
                case 0: m =  M.True(); break;
                case 1: m = ~M.True(); break;
                case 2:
                    if (+w[0] != w) m = n2m[w[0]] ^ sign(w[0]);
                    else            m = M.True();
                    break;
                default: assert(false); }

            }else
                m = M.True();
        }

        n2m(w) = m;
    }

    For_Gatetype(N, gate_Flop, w)
        if (w != pick)
            n2m[w].set(0, n2m[w[0]] ^ sign(w[0]));

//    removeUnreach(M);
    removeUnreach(M, NULL, false);
}


// Returns exit code from script, or INT_MIN if timed out.
int run(NetlistRef N, String script, String filename, uint timeout)
{
    renumber(N);
    bool ok = writeAigerFile(filename, N); assert(ok);

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
        readAigerFile(input, N);
    }catch (Excp_Msg err){
        ShoutLn "ERROR! %_", err;
        exit(1);
    }
    WriteLn "Read: \a*%_\a* -- %_", input, info(N);
    WriteLn "Seed: %_", seed;

    // Give user a way to stop shrinking:
    WriteLn "\n    \a*kill -1 %d\a*\n", getpid();
    OutFile out("kill_shrink.sh");
    out %= "kill -1 %d\n", getpid();
    out.close();
    int ignore ___unused = system("chmod +x kill_shrink.sh");

    // Run shrink:
    int valid_exitcode = run(N, script, "shrunken.aig", timeout);
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
        int ret = run(M, script, "shrunken.aig", timeout);
        n_tries++;
        if (ret == valid_exitcode){
            n_successes++;
            N.clear();
            readAigerFile("shrunken.aig", N);
            n_shrinks++;

            if (fileSize("best.aig") != UINT64_MAX){
                int ret1 ___unused = system("mv -f best.aig best1.aig"); }
            int ret2 ___unused = system("mv -f shrunken.aig best.aig");
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
