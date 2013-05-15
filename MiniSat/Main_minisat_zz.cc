//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_minisat_zz.cc
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : Commandline interface for MiniSat-ZZ.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "MiniSat.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ_CmdLine.hh"

#include <signal.h>
#if defined(__linux__)
  #include <fpu_control.h>
#endif

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// DIMACS Parser:


static int parseInt_(In& in)
{
    int     val = 0;
    bool    neg = false;
    skipWS(in);
    if      (*in == '-') neg = true, in++;
    else if (*in == '+') in++;
    if (*in < '0' || *in > '9') Throw(Excp_ParseError) "Unexpected char: %_\n", *in;
    while (!in.eof() && *in >= '0' && *in <= '9')
        val = val*10 + (*in - '0'),
        in++;
    return neg ? -val : val;
}


static void readClause(In& in, Vec<Lit>& lits, Var vbase)
{
    int parsed_lit;
    Var var;
    lits.clear();
    for (;;){
        parsed_lit = parseInt_(in);
        if (parsed_lit == 0) break;
        var = abs(parsed_lit);
        lits.push( (parsed_lit > 0) ? Lit(var + vbase) : ~Lit(var + vbase) );
    }
}


template<bool pfl>
static Var reserveVars(In& in, MiniSat<pfl>& S)
{
    expect(in, "p cnf ");
    uint n_vars = parseUInt(in);
    skipWS(in);
    parseUInt(in);
    return S.addVars(n_vars);
}


template<bool pfl>
static void parse_DIMACS(In& in, MiniSat<pfl>& S)
{
    Vec<Lit> lits;
    bool     p_line = false;
    Var      vbase = var_Undef;
    for(;;){
        skipWS(in);
        if (in.eof())
            break;
        else if (*in == 'c')
            skipEol(in);
        else if (*in == 'p'){
            vbase = reserveVars(in, S) - 1;     // -- subtract one because DIMACS variables start at '1' not '0'.
            p_line = true;
        }else{
            if (!p_line)
                Throw(Excp_ParseError) "Mising 'p cnf <#vars> <#clauses>' line.";
            readClause(in, lits, vbase);
            S.addClause(lits);
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Statistics:


void printStats(const SatStats& stats, double real_time, double cpu_time)
{
    // <<== fix printout when time is 0 or very close to 0 (don't (9,223,372,036,854,775,808 /sec))
    uint64  mem_used  = memUsed();
    WriteLn "restarts:     %>15%,d", stats.starts;
    WriteLn "conflicts:    %>15%,d    (%,d /sec)", stats.conflicts   , uint64(stats.conflicts    / cpu_time);
    Write   "decisions:    %>15%,d    (%,d /sec)", stats.decisions   , uint64(stats.decisions    / cpu_time);
    if (stats.random_decis > 0) WriteLn "  (%.2f %% random)", 100.0*stats.random_decis / stats.decisions;
    else                        NewLine;
    WriteLn "propagations: %>15%,d    (%,d /sec)", stats.propagations, uint64(stats.propagations / cpu_time);
    WriteLn "inspections:  %>15%,d    (%,d /sec)", stats.inspections , uint64(stats.inspections  / cpu_time);
    WriteLn "confl. literals: %>12%,d    (%.2f %% deleted)", stats.tot_literals, (stats.max_literals - stats.tot_literals)*100.0 / stats.max_literals;
    WriteLn "deleted clauses: %>12%,d", stats.deleted_clauses;
    WriteLn "Memory used:  %>14%^DB", mem_used;
    WriteLn "Real time:    %>13%.2f s", real_time;
    WriteLn "CPU time:     %>13%.2f s", cpu_time;
}


extern "C" {
    static void SIGINT_handler(int /*signum*/)
    {
        printf("\n"); printf("*** INTERRUPTED ***\n");
        fflush(stdout);
        exit(1);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Replay trace:


static
void parseLitList(In& in, Vec<Lit>& result)
{
    result.clear();

    skipWS(in);
    if (*in != '{') return;

    expect(in, "{ ");
    if (*in == '}'){ in++; return; }

    for(;;){
        bool sign;
        if (*in == '~'){ sign = true; in++; }
        else           { sign = false; }
        expect(in, " x");
        uint var = parseUInt(in);
        result.push(Lit(var, sign));
        skipWS(in);
        if (*in == '}'){ in++; break; }
        expect(in, "; ");
    }
}


/*
====================
MiniSat Trace
====================

FORMAT:

  addVar()
  addVars(<#vars>)
  addClause(<clause>)     -- clause on form: '{~x3; x5; x10}'
  solve(<assumps>)        -- assumptions has same syntax as a clause

  clear(<dealloc>)        -- 'clear(0)' will dealloc, 'clear(1)' will recycle memory
  simplifyDB()
  removeVars(<lits>)      -- "lits" has same syntax as a clause (but all literals are unsigned)

EXAMPLE:

  addVars(1872)            # Line comments start with '#'
  addClause({x11; ~x5; x10})
  addClause({x9; ~x16})
  addClause({~x53; x405; ~x391; ~x406; ~x408; x409; x410})
  solve({x3})
  addClause({~x412})
  addClause({~x1464; ~x16; ~x409})
  addClause({x1464; x16; x409})
  addClause({x1487; x9})
  addClause({~x1487; ~x9})
  solve({x1871})
*/


// 'output' will recreate the log as it is being replay. This may catch mismatches when debugging.
// 'echo': 0=silent, 1=short, 2=long
template<bool pfl>
void replayTrace(MiniSat<pfl>& S, String input, OutFile* out, uint echo)
{
    S.verbosity = (echo <= 1) ? 0 : 1;

    InFile    in(input);
    if (in.null()) Throw(Excp_ParseError) "Could not open file: %_", input;
    Vec<char> buf;
    Vec<Lit>  ps;

    uint line_no = 0;
    try{
        while (!in.eof()){
            readLine(in, buf);
            line_no++;

            uind k = search(buf, '#');
            if (k != UIND_MAX)
                buf.shrinkTo(k);
            trim(buf);
            if (buf.size() == 0) continue;

            if (echo == 2)
                WriteLn "%_", buf;

            if (hasPrefix(buf, "clear(")){
                if (echo == 1) std_out += 'c', FL;

                In in2(&buf[6], buf.size()-6);
                skipWS(in2);
                uint v = parseUInt(in2);
                expect(in2, " ) ");
                expectEof(in2);

                S.clear(v);

            }else if (hasPrefix(buf, "simplifyDB(")){
                if (echo == 1) std_out += 'S', FL;

                In in2(&buf[11], buf.size()-11);
                expect(in2, " ) ");
                expectEof(in2);

                S.simplifyDB();

            }else if (hasPrefix(buf, "addVar(")){
                if (echo == 1) std_out += 'v', FL;

                In in2(&buf[7], buf.size()-7);
                expect(in2, " ) ");
                expectEof(in2);

                S.addVar();

            }else if (hasPrefix(buf, "addVars(")){
                if (echo == 1) std_out += 'V', FL;

                In in2(&buf[8], buf.size()-8);
                skipWS(in2);
                uint n = parseUInt(in2);
                expect(in2, " ) ");
                expectEof(in2);

                S.addVars(n);

            }else if (hasPrefix(buf, "addClause(")){
                if (echo == 1) std_out += 'a', FL;

                In in2(&buf[10], buf.size()-10);
                parseLitList(in2, ps);
                expect(in2, " ) ");
                expectEof(in2);

                S.addClause(ps);

            }else if (hasPrefix(buf, "removeVars(")){
                if (echo == 1) std_out += 'r', FL;

                In in2(&buf[11], buf.size()-11);
                parseLitList(in2, ps);
                expect(in2, " ) ");
                expectEof(in2);

                Vec<Var> xs(ps.size());
                for (uind i = 0; i < ps.size(); i++) xs[i] = ps[i].id;
                Vec<Var> kept;
                S.removeVars(xs, kept);

            }else if (hasPrefix(buf, "solve(")){
                if (echo == 1) std_out += 's', FL;

                In in2(&buf[6], buf.size()-6);
                parseLitList(in2, ps);
                expect(in2, " ) ");
                expectEof(in2);

                lbool result = S.solve(ps);
                if (echo >= 2)
                    std_out %= "Result: %_\n", result;

            }else if (hasPrefix(buf, "clearLearnts(")){
                if (echo == 1) std_out += 'C', FL;

                In in2(&buf[13], buf.size()-13);
                expect(in2, " ) ");
                expectEof(in2);

                S.clearLearnts();

            }else if (hasPrefix(buf, "randomizeVarOrder(")){
                if (echo == 1) std_out += 'R', FL;

                WriteLn "randomizeVarOrder() not implemented in replay yet!";
                exit(1);

            }else
                Throw(Excp_ParseError) "Unknown API call: %_", buf;
        }

    }catch (Excp_Msg msg){
        ShoutLn "ERROR [line %_]: %_", line_no, msg;
        exit(40);
    }

    if (out) delete out;

    if (echo == 1) NewLine;
    Write "\a/";
    WriteLn "REPLAY FINISHED:";
    WriteLn "  ok=%_", S.okay();
    WriteLn "  #vars=%_", S.nVars();
    WriteLn "  #clauses=%_", S.nClauses();
    WriteLn "  #learnts=%_", S.nLearnts();
    Write "\a/";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof checking:


void checkProof(SatStd&  , ProofCheck&   , const Vec<Lit>&        ) { assert(false); }
void checkProof(SatPfl& S, ProofCheck& pc, const Vec<Lit>& assumps)
{
    S.proofTraverse();

    if (pc.clauses.size() == 0)
        WriteLn "Empty proof!";
    else{
        const Vec<Lit>& c = pc.clauses[S.conflict_id];
        WriteLn "  -- #resolution chains : %,d", pc.n_chains;
        WriteLn "  -- #binary resolutions: %,d", pc.n_bin_res;
        WriteLn "Final clause: %_", c;

        for (uind i = 0; i < c.size(); i++){
            if (!has(S.conflict, ~c[i])){
                WriteLn "\a*FAILED!\a* Final clause contain literal '%_' not in 'conflict'.", c[i];
                return;
            }
        }
        for (uind i = 0; i < S.conflict.size(); i++){
            if (!has(c, ~S.conflict[i])){
                WriteLn "\a*FAILED!\a* 'conflict contain literal '%_' not in final clause.", assumps[i];
                return;
            }
        }
        WriteLn "Looks OK!";
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Random testing:


void runRandomTests(SatStd&  , uint        , uint          , ProofCheck&   ){ assert(false); }
void runRandomTests(SatPfl& S, uint n_tests, uint n_assumps, ProofCheck& pc)
{
    WriteLn "Running \a*%_\a* tests with \a*%_\a* random assumptions each...", n_tests, n_assumps;

    S.verbosity = 0;
    uint64 seed = DEFAULT_SEED;
    for (uint n = 0; n < n_tests; n++){
        Vec<Lit> assumps;
        for (uint k = 0; k < n_assumps; k++)
            assumps.push(Lit(packed_, irand(seed, S.nVars() * 2)));

        lbool ret = S.solve(assumps);
        if (ret == l_True){
            WriteLn "Test %_:  \a*SAT\a*", n;

        }else{
            Write "Test %_:  conflict=\a*%_\a*\f", n, S.conflict;
            if (S.conflict_id == clause_id_NULL)
                WriteLn "  (no proof)";
            else{
                S.proofTraverse();
                WriteLn "  proved=\a*%_\a*", pc.clauses[S.conflict_id];

                for (uind i = 0; i < S.conflict.size(); i++){
                    if (!has(pc.clauses[S.conflict_id], ~S.conflict[i])){
                        WriteLn "\a*MISMATCH!\a*";
                        exit(255);
                    }
                }
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Run SAT solver


void flushMst(void* data)
{
    ((File*)data)->close();
}


template<bool pfl>
void runSat(const CLI& C)
{
    ProofCheck   pc;
    MiniSat<pfl> S(pfl ? &pc : NULL);
    OutFile*     mst_out = NULL;

    try{
        String input  = C.get("input").string_val;
        String output = C.get("output").string_val;
        String mst    = C.get("mst").string_val;

        if (mst != ""){
            mst_out = new OutFile(mst);
            S.debug_api_out = mst_out;
            atExit(x_Always, flushMst, &mst_out->writer);
        }

        S.verbosity = (uint)C.get("verbosity").int_val;

        if (hasSuffix(input, ".mst"))
            replayTrace(S, input, mst_out, (uint)C.get("echo").int_val);

        else{
            // Parse assumptions:
            Vec<Lit> assumps;
            bool     assumps_fmt = false;   // (dimacs)
            Vec<CLI_Val>& as = *C.get("assumps").sub;
            for (uind i = 0; i < as.size(); i++){
                if (as[i].type == cli_Int){
                    int p = (int)as[i].int_val;
                    if (p ==0) Throw(Excp_Msg) "Invalid literal: %_", p;
                    assumps.push((p > 0) ? Lit(p) : ~Lit(-p));
                }else{
                    bool sgn = false;
                    char* t = as[i].string_val.c_str();
                    if (t[0] == '~'){
                        sgn = true;
                        t++; }
                    if (t[0] != 'x') Throw(Excp_Msg) "Invalid literal: %_", t;
                    try{
                        uint x = (uint)stringToUInt64(t+1);
                        assumps.push(Lit(x, sgn));
                    }catch (...){
                        Throw(Excp_Msg) "Invalid literal: %_", t;
                    }
                    assumps_fmt = true;
                }
            }

            // Parse CNF:
            double T0_parse = cpuTime();
            {
                InFile in(input);
                if (in.null()) Throw(Excp_ParseError) "Could not open file: %_", input;
                if (S.verbosity > 0) WriteLn "Reading: \a*%_\a*", input;
                parse_DIMACS(in, S);
            }
            double T1_parse = cpuTime();

            if (pfl && C.get("proof").string_val != "") Throw(Excp_Msg) "Writing proofs not implemented yet.";   // <<==

            // Output info:
            if (S.verbosity > 0){
                WriteLn "  -- parse time: %t", T1_parse - T0_parse;
                WriteLn "  -- #vars:      %,d", S.nVars();
                WriteLn "  -- #clauses:   %,d", S.nClauses();
            }

            // Random debug mode?
            if (pfl && C.get("random").sub->get(0).int_val > 0){
                runRandomTests(S, (uint)C.get("random").sub->get(0).int_val, (uint)C.get("random").sub->get(1).int_val, pc);

            }else{
                // Run solver:
                double T0c = cpuTime(), T0r = realTime();
                lbool  ret = S.solve(assumps);
                double T1c = cpuTime(), T1r = realTime();

                // Output result:
                if (S.verbosity > 0){
                    printStats(S.statistics(), T1r - T0r, T1c - T0c);
                    NewLine;
                }

                if (ret == l_True){
                    if (S.verbosity > 0 || output == "")
                        WriteLn "\a*SATISFIABLE\a*";

                    if (output != ""){
                        OutFile out(output);
                        out += "SATISFIABLE\n";
                        out %= "cpu-time: %.3f\n", T1c - T0c;
                        out %= "real-time: %.3f\n", T1r - T0r;
                        out += "model: ";
                        for (Var x = 0; x < S.nVars(); x++)
                            out += name(S.value(x));
                        out += '\n';
                    }

                }else if (ret == l_False){
                    if (S.verbosity > 0 || output == "")
                        WriteLn "\a*UNSATISFIABLE\a*";

                    if (assumps.size() > 0){
                        Write "Assumptions used:";
                        for (uind i = 0; i < S.conflict.size(); i++){
                            if (assumps_fmt) Write " %_", S.conflict[i];
                            else             Write " %_", toDimacs(S.conflict[i]);
                        }
                        NewLine;
                    }

                    if (output != ""){
                        OutFile out(output);
                        out += "UNSATISFIABLE\n";
                        out %= "cpu-time: %.3f\n", T1c - T0c;
                        out %= "real-time: %.3f\n", T1r - T0r;
                        out += "conflict: ";
                        for (uind i = 0; i < S.conflict.size(); i++)
                            out += toDimacs(S.conflict[i]);
                        out += '\n';
                    }

                }else{
                    if (S.verbosity > 0 || output == "")
                        WriteLn "\a*UNDECIDED\a*";

                    if (output != ""){
                        OutFile out(output);
                        out += "UNDECIDED\n";
                        out %= "cpu-time: %.3f\n", T1c - T0c;
                        out %= "real-time: %.3f\n", T1r - T0r;
                    }
                }

                if (pfl && C.get("check").bool_val){
                    NewLine;
                    if (ret == l_True)
                        WriteLn "No proof to check!";
                    else{
                        WriteLn "Verifying resolution proof...";
                        checkProof(S, pc, assumps);
                    }
                }
            }
        }

    }catch (Excp_Msg err){
        if (mst_out) delete mst_out;
        ShoutLn "FATAL ERROR! %_", err;
        exit(255);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Fix missing 'p cnf' header:


void fixCnfFile(String filename)
{
    try{
        Vec<Lit> lits;
        bool     p_line = false;
        uint     p_vars = 0, p_clas = 0;
        uint     n_vars = 0, n_clas = 0;
        bool     gzipped;
        {
            InFile in(filename);
            if (in.null()) Throw(Excp_ParseError) "Could not open file: %_", filename;
            gzipped = in.gzipped;
            if (gzipped && !hasSuffix(filename, ".gz"))
                filename += ".gz";

            for(;;){
                skipWS(in);
                if (in.eof())
                    break;
                else if (*in == 'c')
                    skipEol(in);
                else if (*in == 'p'){
                    expect(in, "p cnf ");
                    p_vars = parseUInt(in);
                    skipWS(in);
                    p_clas = parseUInt(in);
                    p_line = true;
                }else{
                    readClause(in, lits, 0);
                    for (uind i = 0; i < lits.size(); i++)
                        newMax(n_vars, lits[i].id);
                    n_clas++;
                }
            }
        }

        if (p_line){
            if (n_vars != p_vars || n_clas != p_clas){
                WriteLn "Existing header:  \a*p cnf %_ %_\a*", p_vars, p_clas;
                WriteLn "Correct header :  \a*p cnf %_ %_\a*", n_vars, n_clas;
                NewLine;
                WriteLn "Please remove 'p cnf' header and re-run!";
                exit(255);
            }else
                WriteLn "Nothing to fix. A correct header is already in place!";

        }else{
            String new_filename;
            int fd = tmpFile("tmp_dimacs_", new_filename);
            if (fd == -1) Throw(Excp_ParseError) "Could not open temporary file: \a*%_\a*", new_filename;
            File out_file(fd, WRITE); assert(!out_file.null());
            Out out(out_file, gzipped ? Z_DEFAULT_COMPRESSION : Out::NO_GZIP);

            out %= "p cnf %_ %_\n", n_vars, n_clas;
            InFile in(filename);
            while (!in.eof())
                out.push(in++);

            out.finish();
            remove(filename.c_str());
            rename(new_filename.c_str(), filename.c_str());
            WriteLn "Added \a*'p cnf %_ %_'\a* to: \a*%_\a*", n_vars, n_clas, filename;
        }

    }catch (Excp_Msg err){
        ShoutLn "FATAL ERROR! %_", err;
        exit(255);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input" , "string", arg_REQUIRED, "Input CNF/MST file. May be gzipped.", 0);
    cli.add("output", "string", "", "Output result to this file (either model or final conflict).", 1);
    cli.add("assumps", "[int|string]", "[]", "Additional unit assumptions as DIMACS integers '[-1,3]' or MST literals '[~x0,x2].");
    cli.add("mst", "string", "", "Save problem as a MiniSAT-trace (\".mst\" file).");
    cli.add("fpu", "bool", "no", "Leave FPU in native state (may affect variable activities).");
    cli.add("verbosity", "int[0:1]", "1", "Set output verbosity.");
    cli.add("echo", "int[0:2]", "0", "For .mst replay, set echo level.");

    // <<== resource limits (confl, secs, inspects)

    CLI cli_pfl;
    cli_pfl.add("proof", "string", "", "Write resolution proof to this file.");
    cli_pfl.add("check", "bool", "no", "DEBUGGING. Verify resolution proof.");
    cli_pfl.add("random", "(uint,uint)", "(0,0)", "DEBUGGING. Do 'N' incremental SAT calls with 'k' random "
                                                  "assumptions before each and verify the proof. Argument is pair '(N, k)'.");
    // <<== second random mode with some random clauses added at each call (or partial insertion of the original problem)

    cli.addCommand("std", "Standard SAT-solving.");
    cli.addCommand("pfl", "Proof-logging SAT-solving.", &cli_pfl);
    cli.addCommand("fix", "Add 'p cnf' DIMACS header to input file.");

    cli.parseCmdLine(argc, argv);

    // Temporary floating point fix (will switch to soft-floats later):
    if (!cli.get("fpu").bool_val){
      #if defined(__linux__)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
      #endif
    }

    // Temporary signal handling (will be part of ZZ framework later):
    signal(SIGINT, SIGINT_handler);
  #if !defined(_MSC_VER)
    signal(SIGHUP, SIGINT_handler);
  #endif

    // Run command:
    if (cli.cmd == "std")
        runSat<false>(cli);
    else if (cli.cmd == "pfl")
        runSat<true>(cli);
    else assert(cli.cmd == "fix"),
        fixCnfFile(cli.get("input").string_val);
}
