//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_bip.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Commandline interface for the Bip engine.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_Bip.Common.hh"

#include "ZZ/Generics/Set.hh"
#include "ZZ/Generics/Sort.hh"
#include "Abstraction.hh"
#include "Bmc.hh"
#include "Imc.hh"
#include "Pdr.hh"
#include "Treb.hh"
#include "Pdr2.hh"
#include "Pmc.hh"
#include "SmvInterface.hh"
#include "Reparam.hh"
#include "Fixed.hh"
#include "Sift.hh"
#include "Sift2.hh"
#include "ISift.hh"
#include "ParClient.hh"
#include "SemAbs.hh"
#include "BestBwd.hh"
#include "Saber.hh"
#include "AbsBmc.hh"
#include "Live.hh"
#include "LtlCheck.hh"
#include "ConstrExtr.hh"
#include "SimpInvar.hh"
#include "PropCluster.hh"

#define NO_BERKELEY_ABC

#if !defined(NO_BERKELEY_ABC)
#include "Bdd.hh"
#endif

using namespace ZZ;

/**/namespace ZZ { void reparamDebug(NetlistRef N); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Signal handler:


extern "C" void SIGINT_handler(int signum);
void SIGINT_handler(int signum)
{
    printf("\n*** CTRL-C pressed ***  [cpu-time: %.2f s]  [mem-used: %.1f MB]\n", cpuTime(), memUsed() / 1048576.0);
        // -- this is highly unsafe, but it is temporary...
    fflush(stdout);
    dumpProfileData();
  #if !defined(ZZ_PROFILE)
    _exit(-1);
  #else
    zzFinalize();
    exit(-1);
  #endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// Returned string is at most 18 characters.
static
String compileDate()
{
    cchar* today = __DATE__;
    String ret;

    if (strncmp(today, "Jan", 3) == 0) ret += "January ";
    if (strncmp(today, "Feb", 3) == 0) ret += "February ";
    if (strncmp(today, "Mar", 3) == 0) ret += "March ";
    if (strncmp(today, "Apr", 3) == 0) ret += "April ";
    if (strncmp(today, "May", 3) == 0) ret += "May ";
    if (strncmp(today, "Jun", 3) == 0) ret += "June ";
    if (strncmp(today, "Jul", 3) == 0) ret += "July ";
    if (strncmp(today, "Aug", 3) == 0) ret += "August ";
    if (strncmp(today, "Sep", 3) == 0) ret += "September ";
    if (strncmp(today, "Oct", 3) == 0) ret += "October ";
    if (strncmp(today, "Nov", 3) == 0) ret += "November ";
    if (strncmp(today, "Dec", 3) == 0) ret += "December ";

    ret += atoi(today + 4), ", ", atoi(today + 6);
    return ret;
}


static
void writeResourceUsage(double T0, double Tr0)
{
    WriteLn "Memory used: %DB", memUsed();
    WriteLn "Real Time  : %.2f s", realTime() - Tr0;
    WriteLn "CPU Time   : %.2f s", cpuTime () - T0;
}


static
void binToText_3bytes(Out& text_out, uchar byte0, uchar byte1, uchar byte2)
{
    text_out += (char)('0' + (byte0 & 63));
    text_out += (char)('0' + ((byte0 >> 6) | ((byte1 & 15) << 2)));
    text_out += (char)('0' + ((byte1 >> 4) | ((byte2 & 3) << 4)));
    text_out += (char)('0' + (byte2 >> 2));
}


// Convert binary data to text (using 6 bits from each byte) and write it to output stream 'text_out'.
static
void binToText(Array<char> data, Out& text_out)
{
    FWrite(text_out) "%_ ", data.size();

    for (uind i = 0; i < data.size(); i += 3)
        binToText_3bytes(text_out, data[i], (i+1 < data.size()) ? data[i+1] : 0, (i+2 < data.size()) ? data[i+2] : 0);
}


static
uind textToBin(char* text, uind text_sz)
{
    char*  dst = text;
    cchar* src = text;
  #if 1
    uind   sz  = parseUInt64(src);  // -- will advance 'src'
    src++;                          // -- skip space
  #else
    ulong sz;   // -- for ABC
    sscanf(src, "%lu ", &sz);
    while(*src++ != ' ');
  #endif

    for (uind i = 0; i < sz; i += 3){
        dst[0] = (char)( (uint)src[0] - '0')       | (((uint)src[1] - '0') << 6);
        dst[1] = (char)(((uint)src[1] - '0') >> 2) | (((uint)src[2] - '0') << 4);
        dst[2] = (char)(((uint)src[2] - '0') >> 4) | (((uint)src[3] - '0') << 2);
        src += 4;
        dst += 3;
    }

    return sz;
}


// If parsed file was AIGER, get the initial abstraction (if present).
static void getInitialAbstr(NetlistRef N, String filename, /*out*/IntSet<uint>& abstr, bool quiet) ___unused;
static void getInitialAbstr(NetlistRef N, String filename, /*out*/IntSet<uint>& abstr, bool quiet)
{
    if (filename != ""){
        InFile in(filename);
        if (in.null()){ ShoutLn "Could not open: %_", filename; exit(1); }
        Vec<char> line;
        while (!in.eof()){
            readLine(in, line);
            if (strncmp(line.base(), "abstraction: ", 13) == 0){
                for (uind i = 13; i < line.size(); i++)
                    if (line[i] == '1')
                        abstr.add(i - 13);
                break;
            }
        }

    }else if (Has_Pob(N, aiger_comment)){
        Get_Pob(N, aiger_comment);

        if (aiger_comment[0] == 'f'){
            if (!quiet)
                WriteLn "Found abstaction in AIGER file.";

            uint size = uint(aiger_comment[4]) | (uint(aiger_comment[3]) << 8) | (uint(aiger_comment[2]) << 16) | (uint(aiger_comment[1]) << 24);   // -- big-endian for some unspecified reason...
            size /= 4;

            Vec<Wire> flops;
            For_Gatetype(N, gate_Flop, w)
                flops.push(w);
            for (uind i = 0; i < flops.size(); i++)
                assert(attr_Flop(flops[i]).number == (int)i);

            for (uint i = 0; i < size; i++)
                if (aiger_comment[5 + i*4])
                    abstr.add(attr_Flop(flops[i]).number);

            if (!quiet){
                WriteLn "Initial abstraction contains %_ (out of %_) flops.", abstr.size(), flops.size();
                NewLine; }
        }
    }
}


static void getReconstructionAig(NetlistRef M, String filename) ___unused;
static void getReconstructionAig(NetlistRef M, String filename)
{
    InFile in(filename);
    if (in.null()){ ShoutLn "Could not open: %_", filename; exit(1); }

    if (hasSuffix(filename, ".aig")){
        try{
            readAigerFile(filename, M);
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg; exit(1);
        }

    }else if (hasSuffix(filename, ".gig")){
        try{
            M.read(filename);
        }catch (Excp_NlParseError err){
            ShoutLn "PARSE ERROR [line %_]! %_", err.line_no, err.msg; exit(1);
        }

    }else{
        String line;
        while (!in.eof()){
            readLine(in, line);
            if (hasPrefix(line, "cex-recons: ")){
                uind sz = textToBin(&line[12], line.size() - 12);
                In in(&line[12], sz);
                readAiger(in, M, false);
                break;
            }
        }
    }
}


static void getCounterexample(CCex& cex, String filename) ___unused;
static void getCounterexample(CCex& cex, String filename)
{
    InFile in(filename);
    if (in.null()){ ShoutLn "Could not open: %_", filename; exit(1); }
    Vec<char> line;
    while (!in.eof()){
        readLine(in, line);
        if (strncmp(line.base(), "counter-example: ", 17) == 0){
            cex.flops.clear();
            cex.inputs.clear();
            cex.flops.push();
            IntMap<int,lbool>* curr = &cex.flops[0];
            uint num = 0;
            for (uind i = 17; i < line.size(); i++){
                if (line[i] == '0' || line[i] == '1' || line[i] == '?' || line[i] == '!'){
                    (*curr)(num) = (line[i] == '0') ? l_False : (line[i] == '1') ? l_True : (line[i] == '?') ? l_Undef : l_Error;
                    num++;

                }else if (line[i] == ' '){
                    cex.inputs.push();
                    curr = &cex.inputs.last();
                    num = 0;

                }else{
                    ShoutLn "Unexpected character in counterexample: %_", line[i]; exit(1); }
            }
            return;     // Done!
        }
    }

    ShoutLn "File did not contain counterexample: %_", filename;
    exit(1);
}


// Map the subset 'prop_nums' to actual wires 'props'. If 'prop_nums' is empty, all properties are used
// and 'prop_nums' is populated.
static
void setupProperties(NetlistRef N, Vec<int>& prop_nums, bool inv_prop, /*out*/Vec<Wire>& props, bool quiet)
{
    // Define properties:
    if (!Has_Pob(N, properties) && !Has_Pob(N, fair_properties)){
        if (N.typeCount(gate_PO) == 0){
            ShoutLn "No properties and no POs in design!";
            exit(1); }
        makeAllOutputsProperties(N);
        if (!quiet) WriteLn "No properties defined. Treating all POs as properties.";
    }

    Assure_Pob(N, properties);
    bool props_numbered = true;
    for (uind i = 0; i < properties.size(); i++){
        Wire w = properties[i];
        if (attr_PO(w).number == num_NULL){
            props_numbered = false;
            break;
        }
    }
    if (!props_numbered){
        for (uind i = 0; i < properties.size(); i++){
            Wire w = properties[i];
            attr_PO(w).number = i;
        }
        if (!quiet) WriteLn "Properties not numbered; adding numbers 0..%_", properties.size() - 1;
    }

    // Invert properties?
    if (inv_prop){
        for (uind i = 0; i < properties.size(); i++)
            properties[i].set(0, ~properties[i][0]);
    }

    // Select specified subset of properties:
    if (prop_nums.size() == 0){
        for (uind j = 0; j < properties.size(); j++){
            props.push(properties[j]);
            prop_nums.push(attr_PO(properties[j]).number);
        }
    }else{
        for (uind i = 0; i < prop_nums.size(); i++){
            for (uind j = 0; j < i; j++){
                if (prop_nums[j] == prop_nums[i])   // -- skip duplicates
                    goto Found;
            }
            for (uind j = 0; j < properties.size(); j++){
                if (attr_PO(properties[j]).number == prop_nums[i]){
                    props.push(properties[j]);
                    goto Found;
                }
            }
            /*else*/{
                ShoutLn "ERROR! Incorrect property number: %_", prop_nums[i];
                Shout "Available properties are:";
                for (uind j = 0; j < properties.size(); j++) Shout " %_", attr_PO(properties[j]).number;
                ShoutLn "";
                exit(1);
            }
          Found:;
        }
    }
}


static
void setupInitialState(NetlistRef N, bool quiet)
{
    if (!Has_Pob(N, flop_init)){
        if (!quiet) WriteLn "Initial value for flops missing. Assuming all flops to be zero.";
        Add_Pob(N, flop_init);
        For_Gatetype(N, gate_Flop, w)
            flop_init(w) = l_False;
    }
}


static
Wire getNode(NetlistRef N, const CLI_Val& v)
{
    N.names().enableLookup();
    if (v.type == cli_Int){
        if (v.int_val >= N.size() || deleted(N[v.int_val])){
            ShoutLn "ERROR! No such gate: w%_", v.int_val;
            exit(1); }
        return N[v.int_val];

    }else if (v.type == cli_String){
        const String& str = v.string_val.c_str();

        if (str.size() > 3 && str[2] == ':'){
            int num = num_ERROR;
            try{ num = stringToInt64(str.sub(3)); } catch(...) {}

            if (hasPrefix(str, "ff:")){
                For_Gatetype(N, gate_Flop, w)
                    if (attr_Flop(w).number == num)
                        return w;
            }else if (hasPrefix(str, "pi:")){
                For_Gatetype(N, gate_PI, w)
                    if (attr_PI(w).number == num)
                        return w;
            }else if (hasPrefix(str, "po:")){
                For_Gatetype(N, gate_PO, w)
                    if (attr_PO(w).number == num)
                        return w;
            }else{
                ShoutLn "ERROR! Invalid ':' prefix. Must be one of: ff, pi, po";
                exit(1);
            }
        }else{
            GLit g = N.names().lookup(str.c_str());
            if (g)
                return g + N;
        }
        ShoutLn "ERROR! No such gate: %_", v.string_val;
        exit(1);

    }else{
        ShoutLn "ERROR! Unexpected gate id/name: %_", v;
        exit(1);
    }
}


void padInputs(NetlistRef N, uint n_pis)
{
    IntSeen<uint> seen;
    For_Gatetype(N, gate_PI, w)
        seen.add(attr_PI(w).number);

    for (uint i = 0; i < n_pis; i++)
        if (!seen.has(i))
            N.add(PI_(i));
}


struct Ext_lt {
    NetlistRef N;
    Vec<char>& tmp1;
    Vec<char>& tmp2;
    Ext_lt(NetlistRef N_, Vec<char>& tmp1_, Vec<char>& tmp2_) : N(N_), tmp1(tmp1_), tmp2(tmp2_) {}

    void getName(gate_id g, Vec<char>& tmp) const {
        uind sz = N.names().size(GLit(g));
        if (sz > 0)
            N.names().get(GLit(g), tmp);
        else{
            Wire w = N[g];
            int  num;
            if      (type(w) == gate_PI)   num = attr_PI  (w).number;
            else if (type(w) == gate_PO)   num = attr_PO  (w).number;
            else if (type(w) == gate_Flop) num = attr_Flop(w).number;
            else assert(false);

            if (num != num_NULL){
                tmp.clear();
                pushf(tmp, "\001%d", num);
                tmp.push(0);
            }else{
                tmp.clear();
                pushf(tmp, "\377%d", id(w));
                tmp.push(0);
            }
        }
    }

    bool operator()(gate_id g1, gate_id g2) const {
        getName(g1, tmp1);
        getName(g2, tmp2);
        return strcmp(tmp1.base(), tmp2.base()) < 0;
    }
};


void sortExternals(NetlistRef N)
{
    Vec<char> tmp1;
    Vec<char> tmp2;
    Ext_lt    lt(N, tmp1, tmp2);

    Vec<gate_id> gs;
    For_Gatetype(N, gate_PI, w)
        gs.push(id(w));
    sobSort(sob(gs, lt));
    for (uint i = 0; i < gs.size(); i++)
        attr_PI(N[gs[i]]).number = i;

    gs.clear();
    For_Gatetype(N, gate_PO, w)
        gs.push(id(w));
    sobSort(sob(gs, lt));
    for (uint i = 0; i < gs.size(); i++)
        attr_PO(N[gs[i]]).number = i;

    gs.clear();
    For_Gatetype(N, gate_Flop, w)
        gs.push(id(w));
    sobSort(sob(gs, lt));
    for (uint i = 0; i < gs.size(); i++)
        attr_Flop(N[gs[i]]).number = i;

    //**/WriteLn "PI Order:";
    //**/for (uint i = 0; i < gs.size(); i++)
    //**/    WriteLn "  %_ = pi[%_] = \"%_\"", N[gs[i]], attr_PI(N[gs[i]]).number, N.names().get(N[gs[i]], N.names().scratch);
    //**/exit(0);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Output result of commands:


static
cchar* resultToString(lbool result)
{
    if          (result == l_True )  return "proved";
    else if     (result == l_False)  return "failed";
    else if     (result == l_Undef)  return "undetermined";
    else{ assert(result == l_Error); return "error"; }
}


static
void writeCex(Out& out, NetlistRef N, const Cex& cex, uint orig_num_pis)
{
    Vec<Pair<int,GLit> > ffs, pis;
    For_Gatetype(N, gate_Flop, w) ffs.push(tuple(attr_Flop(w).number, w));
    For_Gatetype(N, gate_PI  , w) pis.push(tuple(attr_PI  (w).number, w));
    sort(ffs);
    sort(pis);

    // Hack to remove reset flop introduced by Treb in self-abstracting mode:
    if (Has_Pob(N, reset)){
        Get_Pob(N, reset);
        Wire w = reset;
        assert(type(w) == gate_Flop);
        assert(attr_Flop(w).number == ffs.last().fst);
        ffs.pop();
    }

    out += "counter-example: ";
    for (uind i = 0; i < ffs.size(); i++){
        uind skip = (i == 0) ? ffs[i].fst : ffs[i].fst - ffs[i-1].fst - 1;
        for (uind j = 0; j < skip; j++) out += '!';
        out += name(cex.flops[0][N[ffs[i].snd]]);
    }

    for (int d = 0; d <= cex.depth(); d++){
        out += ' ';
        for (uind i = 0; i < pis.size(); i++){
            uind skip = (i == 0) ? pis[i].fst : pis[i].fst - pis[i-1].fst - 1;
            for (uind j = 0; j < skip; j++) out += '!';
            out += name(cex.inputs[d][N[pis[i].snd]]);
        }
        if (pis.size() > 0){
            for (uint i = pis.last().fst + 1; i < orig_num_pis; i++)
                out += '!';
        }
    }
    out += NL;
}


static
void writeInvar(Out& out, NetlistRef N)
{
    assert(N.typeCount(gate_PO) == 1);
    For_Gatetype(N, gate_PO, w)
        attr_PO(w).number = 0;

    out += "proof-invariant: ";
    writeTaig(out, N);
}


static
void writeAbstr(Out& out, NetlistRef N, const IntSet<uint>& abstr)
{
    Vec<char> a(nextNum_Flop(N), '0');
    For_Gatetype(N, gate_Flop, w){
        uint num = attr_Flop(w).number;
        if (abstr.has(num))
            a[num] = '1';
    }

    out += "abstraction: ";
    for (uind i = 0; i < a.size(); i++) out += a[i];
    out += NL;
}


void outputVerificationResult(
    NetlistRef N, const Vec<Wire>& props,
    lbool result, Cex* cex, uint orig_num_pis, NetlistRef invar, int bug_free_depth, bool check_invar,
    String out_filename, bool quiet,
    double T0, double Tr0, int loop_len = -1)
{
    //
    // TO FILE:
    //
    if (out_filename != ""){
        OutFile out(out_filename);

        // Write result:
        FWriteLn(out) "result: %_", resultToString(result);

        // Write counterexample:
        if (cex != NULL && cex->size() > 0)
            writeCex(out, N, *cex, orig_num_pis);

        // Write invariant:
        if (!invar.null() && !invar.empty())
            writeInvar(out, invar);

        // Write bug-free depth:
        if (bug_free_depth != -1)
            FWriteLn(out) "bug-free-depth: %_", bug_free_depth;

        // Write loop length (for liveness only):
        if (loop_len != -1)
            FWriteLn(out) "loop-length: %_", loop_len;
    }

    //
    // TO STANDARD OUTPUT:
    //
    if (!quiet){
        if (result == l_False){
            WriteLn "----";
            WriteLn "----  Property \a*FAILED\a*";
            WriteLn "----";
            if (N.typeCount(gate_MFlop) > 0)
                WriteLn "Netlist has memories. Counterexample could not be verified (yet).";
            else if (!cex)
                WriteLn "Counterexample not provided.";
            else if (loop_len == -1){
                Vec<uint> fails_at;
                if (verifyCex(N, props, *cex, &fails_at)){
                    WriteLn "Counterexample VERIFIED!";
                    for (uind i = 0; i < fails_at.size(); i++)
                        if (fails_at[i] != UINT_MAX)
                            WriteLn "Property# %_ fails at depth %_", attr_PO(props[i]).number, fails_at[i];
                }else
                    WriteLn "Counterexample is \a*\a/INCORRECT\a0!";
            }

        }else if (result == l_True){
            WriteLn "----";
            WriteLn "----  Property \a*PROVED\a*";
            WriteLn "----";
            if (check_invar){
                WriteLn "Verifying inductive invariant... (intentionally using unoptimized code)";
                double T0_check = cpuTime();
                uint   failed_prop;
                bool   result2 = verifyInvariant(N, props, invar, &failed_prop);
                double T_check = cpuTime() - T0_check;
                if (!result2){
                    Write "\a*\a/Invariant INCORRECT\a0 (%.2f s): ", T_check;
                    if      (failed_prop == VINV_not_inductive) WriteLn "Not inductive!";
                    else if (failed_prop == VINV_not_initial)   WriteLn "Does not imply initial states!";
                    else                                        WriteLn "Does not imply property %_", attr_PO(props[failed_prop]).number;
                }else
                    WriteLn "Invariant checked and found correct (%.2f s)", T_check;
            }

        }else{
            WriteLn "Resource limit reached. Aborted!";
            WriteLn "Bug-free depth: %_", bug_free_depth;
            WriteLn "----";
            WriteLn "----  Property \a*UNDETERMINED\a*";
            WriteLn "----";
        }

        NewLine;
        writeResourceUsage(T0, Tr0);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Quick wrapper around 'cip' (if present) for parsing SIF files:


void parseSif(String input, NetlistRef N)
{
    String tmp_file = (FMT "__tmp_cip_aig.%_", getpid());       // "Thou shalt not create temporary files like this!" (forgive me father for I have sinned)
    String cmd = (FMT "cip ,save-aig %_ -output=%_", input, tmp_file);
    int ret ___unused = system(cmd.c_str());
    try{
        readAigerFile(tmp_file, N);
#if 1
        unlink(tmp_file.c_str());
#else
        WriteLn "Wrote: %_", tmp_file;
        exit(0);
#endif
    }catch (Excp_AigerParseError err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ABC integration:


static
void printAbcCommands(CLI& /*later*/)
{
    WriteLn ",bmc";
    WriteLn ",imc";
    WriteLn ",pdr";
    WriteLn ",treb";
    WriteLn ",abs";
    WriteLn ",reparam";
    WriteLn ",live";
}


static
void printAbcHelp(CLI& cli_, String& orig_msg)
{
    // Remove header and footer:
    char* text = orig_msg.c_str();
    char* p = strstr(text, "ARGUMENTS:\n");
    if (p == NULL){
        Write "%_", orig_msg;
        return;
    }
    text = p + 11;

    p = strstr(text, "\a/\a*--------");
    assert(p != NULL);
    p[-1] = 0;

    // Locate help string for command:
    String cmd_help;
    for (uind i = 0; i < cli_.cmds.size(); i++){
        if (cli_.cmds[i].name == cli_.cmd){
            cmd_help = cli_.cmds[i].help;
            break;
        }
    }

    // Print help:
    String msg(text);
    WriteLn "USAGE: \a*,%s\a* [<switches>]  \a/==  %_\a0", cli_.cmd, cmd_help;
    NewLine;
    Write "%_", msg;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Increase stack size:


void increaseStackSize()
{
    const rlim_t stack_size = 64 * rlim_t(1024*1024);
    struct rlimit r;
    int st;

    st = getrlimit(RLIMIT_STACK, &r);
    if (st == 0)    {
        if (r.rlim_cur < stack_size){
            r.rlim_cur = stack_size;
            st = setrlimit(RLIMIT_STACK, &r);
            if (st != 0)
                ShoutLn "WARNING! Stack size could not be changed.";
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


struct GLitAct {
    NetlistRef N;
    const WMap<uint>& activity;
    GLitAct(NetlistRef N_, const WMap<uint>& activity_) : N(N_), activity(activity_) {}

    bool operator()(GLit x, GLit y) const { return activity[N[x]] < activity[N[y]]; }
};


int main(int argc, char** argv)
{
#if 0
    {
        //DEBUG
        ZZ_Init;

        Netlist N;
        Wire x = N.add(PI_(0));
        Wire y = N.add(PI_(1));
        Wire s0 = N.add(Flop_(0), y);
        Wire s1 = N.add(Flop_(1), s0);
        Wire f = N.add(And_(), x, s0);
        Wire g = N.add(And_(), f, s1);
        Wire top = N.add(PO_(0), ~g);
        N.names().add(x  , "x");
        N.names().add(y  , "y");
        N.names().add(s0 , "s0");
        N.names().add(s1 , "s1");
        N.names().add(f  , "f");
        N.names().add(g  , "g");
        N.names().add(top, "top");

        String bin, text;
        writeAiger(bin, N);
        binToText(bin.slice(), text);
        writeFile("dump.txt", text.slice());
        textToBin(text.vec());

        Dump(bin.size());
        Dump(text.size());

        for (uind i = 0; i < bin.size(); i++)
            WriteLn "%_: %.2X %.2X", i, (uint)bin[i], (uint)(uchar)text[i];

        exit(0);
    }
#endif

    // Special handling for call from ABC:
    bool abc_plugin_call = false;
    if (argc >= 2 && (strcmp(argv[1], "-abc") == 0 || strcmp(argv[1], "--abc") == 0)){
        abc_plugin_call = true;
        for (int i = 1; i < argc-1; i++)
            argv[i] = argv[i+1];
        argc--;

      #if defined(__linux__)
        // Kill 'bip' if parent is dead:
        prctl(PR_SET_PDEATHSIG, SIGINT);

        // We may have already missed this signal, so check if the process has been adopted by INIT:
        if(getppid() == 1)
            exit(1);
      #endif
    }

    // ZZ Initialization:
    ZZ_Init;

    //**/WriteLn "Running TEST..."; void test(); test(); exit(0);

    increaseStackSize();

    // Command line -- common:
    if (abc_plugin_call){
        cli_hidden.add("input" , "string", "", "");
        cli_hidden.add("output", "string", "", "");
        cli_hidden.add("logo", "bool", "no", "");
        cli_hidden.add("list-commands", "bool", "no", "");
    }else{
        cli.add("input" , "string", arg_REQUIRED, "Input AIGER or GIG file. May be gzipped.", 0);
        cli.add("output", "string", "", "Output result to this file.", 1);
        cli.add("logo", "bool", "yes", "Show BIP header.");
    }
    cli.add("pre", "bool", "yes", "Preprocess netlist for verification commands.");
    cli.add("inv", "bool", "no", "Invert property(ies).");
    cli.add("sort-ext", "bool", "no", "Sort external elements on name (if present).");
    cli.add("prop", "uint | [uint] | {all}", "all", "Properties to work on (by PO number).");
    cli.add("conjoin", "bool", "no", "Conjoin all properties (for save commands).");
    cli.add("abstr", "string", "", "File containing initial abstraction (selected engines).");
    cli.add("check", "bool", "no", "Validate invariants.");
    cli.add("vt", "ufloat | {inf}", "inf", "Virtual timeout (in \"seconds\").");
    cli.add("timeout", "ufloat | {inf}", "inf", "CPU timeout (in seconds).");
    cli.add("fpu", "bool", "no", "Leave FPU in native state (may affect variable activities).");
    cli.add("old-sif", "bool", "no", "Use old SIF parsing through 'cip'.");
    cli.add("quiet", "bool", "no", "Run without progress output.");

    cli_hidden.add("profile", "bool", "no", "Activate profiling.");

    // Command line -- new property driven reachability (The Trebuchet):
    CLI cli_treb;
    addCli_Treb(cli_treb);
    cli.addCommand("treb", "Trebuchet version of PDR.", &cli_treb);

    // Command line -- new property driven reachability:
    CLI cli_pdr2;
    addCli_Pdr2(cli_pdr2);
    cli.addCommand("pdr2", "Two-frame version of PDR.", &cli_pdr2);

    // Command line -- property driven reachability:
    CLI cli_pdr;
    cli_pdr.add("seed", "uint", "0", "Seed to randomize SAT solver with. 0 means no randomization.");
    cli_pdr.add("min-cex", "bool", "no", "Produce minimal counterexamples (has performance penalty).");
    cli_pdr.add("qc", "bool", "yes", "Quantification based clausification.");
    cli_pdr.add("st", "bool", "no", "Simple binary Tseitin clausification.");
    cli_pdr.add("stats", "{clauses, time, vars}", "clauses", "Control progress output.");
    cli_pdr.add("dump-invar", "bool", "no", "Dump invariant in clause form.");
    cli.addCommand("pdr", "Property driven reachability analysis.", &cli_pdr);

    // Command line -- PMC:
    CLI cli_pmc;
    addCli_Pmc(cli_pmc);
    cli.addCommand("pmc", "Pounded Model Checking.", &cli_pmc);

    // Command line -- Sifting for k-inductive invariant:
    CLI cli_sift;
    cli.addCommand("sift", "k-inductive invariant sifting.", &cli_sift);

    // Command line -- Sifting for k-inductive invariant:
    CLI cli_isift;
    cli.addCommand("isift", "interpolation based invariant sifting.", &cli_isift);

    // Command line -- interpolation:
    CLI cli_imc;
    cli_imc.add("k", "uint", "0", "Start value for k.");
    cli_imc.add("bwd", "bool", "no", "Backward interpolation.");
    cli_imc.add("simp", "bool", "yes", "Simplify interpolants.");
    cli_imc.add("prune", "bool", "no", "Prune interpolants stochastically.");
    cli_imc.add("qc", "bool", "no", "Quantification based clausification.");
    cli_imc.add("st", "bool", "no", "Simple binary Tseitin clausification.");
    cli_imc.add("spin", "bool", "no", "Spin interpolant to minimize it.");
    // <<== experimental options here for turning off variable removal or recycling
    cli.addCommand("imc", "Interpolation based modelchecking.", &cli_imc);

    // Command line -- BMC:
    CLI cli_bmc;
    cli_bmc.add("qc", "bool", "yes", "Quantification based clausification.");
    cli_bmc.add("st", "bool", "no", "Simple binary Tseitin clausification.");
    cli_bmc.add("la", "int[1:]", "1", "Number of look-ahead frames.");
    cli_bmc.add("la-decay", "ufloat", "0.8", "Smaller numbers mean later frames are given less time. '1' = all frames have equal time.");
    cli_bmc.add("sat", "{zz, msc, abc, glu, glr, msr}", "msc", "SAT-solver to use.");

    cli.addCommand("bmc", "Bounded model checking", &cli_bmc);

    // Command line -- ping-pong interpolation:
    cli.addCommand("pp" , "Experimental interpolation based MC.");

    // Command line -- property driven reachability:
#if !defined(NO_BERKELEY_ABC)
    CLI cli_bdd;
    cli_bdd.add("reorder", "bool", "no", "Enable dynamic variable reordering.");
    cli_bdd.add("debug", "bool", "no", "Enable debug output.");
    cli.addCommand("bdd", "BDD based reachability [EXPERIMENTAL].", &cli_bdd);
#endif

    // Command line -- abstraction:
    CLI cli_abs;
    if (abc_plugin_call)
        cli_abs.add("aig", "string", "", "Instantiate abstraction to this file.");
    else
        cli_abs.add("aig", "string", "abstr_bip.aig", "Instantiate abstraction to this file.");
    cli_abs.add("renumber", "bool", "no", "Renumber PIs and FFs in AIGER file (otherwise dummy PIs/FFs are tied to zero).");
    cli_abs.add("confl", "uint | {inf}", "inf", "Stop after this many conflicts.");
    cli_abs.add("depth", "uint | {inf}", "inf", "Stop at this depth.");
    cli_abs.add("stable", "uint", "0", "Must be used together with '-depth'. Stopping criteria now is: \"depth\" is reached AND no counterexample has been found for \"stable\" number of steps.");
    cli_abs.add("bob", "uint | {inf}", "inf", "Go upto this depth, then stop if \"stable-steps >= current-depth / 2\".");
    cli_abs.add("sat-verbosity", "uint", "0", "[Debug]. Show progress of individual SAT runs.");
    cli_abs.add("randomize", "bool", "no", "[Experimental]. Randomize variable order after UNSAT.");
    cli_abs.add("dwr", "string", "", "Dump while running (abstract models in AIGER format). If the given string ends with a '%', it is used as a filename prefix with '%' replaced by 1, 2, 3 etc. Otherwise, the string is used directly as a filename, overwriting the same file repeatedly.");

    cli.addCommand("abs", "Localization abstraction.", &cli_abs);

    // Command line -- reparametrization:
    CLI cli_reparam;
    cli_reparam.add("width", "uint" , "8", "Maximum cut width (over 10 may be very slow).");
    cli_reparam.add("resyn", "bool" , "yes", "Resynthesize logic to remove more inputs'.");
    cli_reparam.add("aig", "string" , "", "Save result as AIGER file.");
    cli_reparam.add("gig", "string" , "", "Save result as GIG file.");
    cli.addCommand("reparam", "Remove inputs through reparametrization. (work in progress)", &cli_reparam);

    // Command line -- simplify:
    cli.addCommand("simp", "Simplify AIG.");

    // Command line -- static BMC:
    CLI cli_static_bmc;
    cli_static_bmc.add("k", "uint", arg_REQUIRED, "BMC depth in number of transitions.");
    cli_static_bmc.add("k0", "uint", "0", "First cycle to check for a bad state.");
    cli_static_bmc.add("init", "bool", "yes", "Output initialized unrolling.");
    cli_static_bmc.add("simp", "bool", "yes", "Simplify AIG after unrolling.");
    cli_static_bmc.add("qc", "bool", "yes", "Quantification based clausification.");
    cli_static_bmc.add("st", "bool", "no", "Simple binary Tseitin clausification.");
    cli.addCommand("static-bmc", "Unroll BMC to fixed length and output AIG or CNF.", &cli_static_bmc);

    // Command line -- Fixed:
    CLI cli_fix;
    cli_fix.add("depth", "uint", "0", "[EXPERIMENTAL]. Depth of image.");
    cli.addCommand("fix", "Run SAT based fixed point iteration.", &cli_fix);

    // Command line -- Best-first Backward Reachability:
    CLI cli_bbr;
    //cli_fix.add("depth", "uint", "0", "[EXPERIMENTAL]. Depth of image.");
    cli.addCommand("bbr", "Best-first backward reachability.", &cli_bbr);

    // Command line -- Semantic Abstraction:
    cli.addCommand("sem", "Semantic Abstraction.");

    // Command line -- Saber:
    CLI cli_saber;
    cli_saber.add("k", "uint", "4", "Target enlargement steps.");
    cli_saber.add("N", "int[1:16]", "10", "Flops to consider in one abstract reachability step.");
    cli.addCommand("saber", "SAT based approximate reachability (constraint extraction)", &cli_saber);

    // Command line -- abstract BMC:
    CLI cli_absbmc;
    cli.addCommand("absbmc", "Under-approximate BMC.", &cli_absbmc);

    // Command line -- liveness:
    CLI cli_live;
    cli_live.add("k", "uint | {inc, l2s}", "l2s", "Bound for k-liveness ('inc' = incremental) or 'l2s' for liveness-to-safety algorithm.");
    cli_live.add("aig", "string", "", "Output AIGER file after conversion.");
    cli_live.add("gig", "string", "", "Output GIG file after conversion.");
    cli_live.add("wit", "string", "", "Output AIGER 1.9 witness.");
    cli_live.add("eng", "{none, bmc, treb, treb-abs, pdr2, imc}", "treb", "Proof-engine to apply to conversion.");
    cli_live.add("bmc-depth", "uint | {inf}", "inf", "For '-eng=bmc' only; bound the depth.");
    cli.addCommand("live", "Liveness checking.", &cli_live);

    // Command line -- LTL:
    CLI cli_ltl;
    cli_ltl.add("spec", "string", "", "File containing LTL specification(s).");
    cli_ltl.add("inv", "bool", "no", "Negate LTL formula.");
    cli_ltl.add("eng", "{klive, bmc, pdr, auto}", "auto", "Which liveness engine to use. 'auto == pdr' unless '-final-gig' is given.");
    cli_ltl.add("names", "bool | {auto}", "auto", "Migrate names through transformation for debugging.");
    cli_ltl.add("spec-gig", "string", "", "Save internal representation of spec.");
    cli_ltl.add("mon-gig", "string", "", "Save synthesized monitor.");
    cli_ltl.add("final-gig", "string", "", "Save monitor + design.");
    cli_ltl.add("fuzz", "bool", "no", "Produce output for fuzzer.");
    cli_ltl.add("fv", "bool", "no", "Allow free-variables in specification (will introduce pseudo-inputs).");
    cli_ltl.add("wit", "string", "", "Output AIGER 1.9 witness.");
    cli.addCommand("ltl", "LTL model checking.", &cli_ltl);

    // Command line -- constraint extraction:
    CLI cli_constr;
    cli_constr.add("k", "uint", "0", "Temporal decomposition frame.");
    cli_constr.add("l", "uint", "0", "Target enlargement frame.");
    cli.addCommand("constr", "Constraint extraction.", &cli_constr);

    // Command line -- show:
    CLI cli_show;
  #if !defined(__APPLE__)
    cli_show.add("gv", "string", "gv", "PDF viewer to run on result.");
  #else
    cli_show.add("gv", "string", "open", "PDF viewer to run on result.");
  #endif
    cli_show.add("names", "bool", "no", "Show names instead of wire IDs.");
    cli_show.add("cleanup", "bool", "no", "Remove unreachable nodes.");
    cli_show.add("human", "bool", "no", "Make logic more human readable (may lose names).");
    cli_show.add("split", "bool", "no", "Split flops into state inputs/outputs.");
    cli_show.add("root", "{all} | uint | string | [uint | string]", "all", "Seed nodes for area.");
    cli_show.add("area", "string", "iii", "Nodes to include. Grow by: i=inputs, o=outputs, b=both.");
    cli_show.add("lim", "uint", "50", "Maximum number of nodes to include in DOT output.");
    cli_show.add("uifs", "string", "", "File containing gate names for UIFs.");
    cli.addCommand("show", "Show netlist using 'dot' and 'gv'.", &cli_show);

    // Command line -- check-invar:
    CLI cli_invar;
    cli_invar.add("invar", "string", arg_REQUIRED, "Invariant in clausal form.");
    cli_invar.add("add-prop", "bool", "yes", "Add property to invariant.");
    cli.addCommand("check-invar", "Verify an invariant", &cli_invar);

    // Command line -- simp-invar:
    CLI cli_simp_invar;
    cli_simp_invar.add("invar", "string", arg_REQUIRED, "Invariant in clausal form.");
    cli_simp_invar.add("fast" , "bool"  , "no"        , "In fast mode, only whole clauses are considered for removal.");
    cli.addCommand("simp-invar", "Simplify an invariant", &cli_simp_invar);

    // Command line -- simp-invar:
    CLI cli_cluster;
    cli_cluster.add("n",      "int[1:]", "4",   "Number of clusters to partition properties into.");
    cli_cluster.add("pivots", "int[1:]", "256", "Number of state variables to track in support computation.");
    cli_cluster.add("seq",    "int[1:]", "5",   "Sequential depth of support analysis.");
    cli.addCommand("cluster", "Cluster properties according to support.", &cli_cluster);

    // Command line -- miscellaneous:
    CLI cli_save_gig;
    cli_save_gig.add("names", "bool", "no", "Keep names from input file.");
    cli.addCommand("save-smv", "Convert input to SMV format.");
    cli.addCommand("save-gig", "Convert input to GIG format.", &cli_save_gig);
    cli.addCommand("save-aig", "Convert input to AIGER format.");
    cli.addCommand("info"    , "Show some netlist statistics.");

    // Command line -- PARSE!
    if (!abc_plugin_call)
        cli.parseCmdLine(argc, argv);
    else{
        String msg;
        if (!cli.parseCmdLine(argc, argv, msg, getConsoleWidth() - 1)){
            printAbcHelp(cli, msg);
            return 0; }
        if (cli.get("list-commands").bool_val){
            printAbcCommands(cli);
            return 0; }
    }
    String input      = cli.get("input").string_val;
    String output     = cli.get("output").string_val;
    bool   inv_prop   = cli.get("inv").bool_val;
    bool   sort_ext   = cli.get("sort-ext").bool_val;
    uint64 vtimeout   = (cli.get("vt").choice == 0) ? uint64(cli.get("vt").float_val * SEC_TO_VIRT_TIME) : UINT64_MAX;
    double timeout    = (cli.get("timeout").choice == 0) ? cli.get("timeout").float_val : DBL_MAX;
    bool   quiet      = cli.get("quiet").bool_val;
    bool   show_logo  = cli.get("logo").bool_val;
    bool   preprocess = cli.get("pre").bool_val;
    bool   old_sif    = cli.get("old-sif").bool_val;

    if (!cli.get("fpu").bool_val){
      #if defined(__linux__)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
      #endif
    }

    suppress_profile_output = !cli_hidden.get("profile").bool_val;

    Vec<int> prop_nums;    // -- an empty list encodes "all properties"
    if (cli.get("prop").choice == 0)
        prop_nums.push((int)cli.get("prop").int_val);
    else if (cli.get("prop").choice == 1){
        for (uind i = 0; i < cli.get("prop").size(); i++)
            prop_nums.push((int)cli.get("prop")[i].int_val);
    }

    // Pre-handling of commands:
    if (input == "@@"){
        startPar();
        WriteLn "Bip started with process id: %_", getpid();
        Write   "    and command line:";
        for (int i = 1; i < argc; i++)
            Write " %_" ,argv[i];
        NewLine;
    }

    if (cli.cmd == "info")
        quiet = true;

    // Read input file:
    Netlist N;
    bool    is_aiger = false;
    if (hasExtension(input, "aig")){
        try{
            readAigerFile(input, N);
            //*aiger*/For_Gatetype(N, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            is_aiger = true;
        }catch (Excp_AigerParseError err){
            ShoutLn "PARSE ERROR! %_", err.msg;
            exit(1);
        }

    }else if (hasExtension(input, "gig")){
        try{
            N.read(input);
        }catch (Excp_NlParseError err){
            ShoutLn "PARSE ERROR [line %_]! %_", err.line_no, err.msg;
            exit(1);
        }

    }else if (hasExtension(input, "sif")){
        if (old_sif)
            parseSif(input, N);
        else{
            try{
                readSif(input, N);
            }catch (Excp_SifParseError err){
                ShoutLn "PARSE ERROR! %_", err.msg;
                exit(1);
            }
        }

    }else if (hasExtension(input, "out")){     // -- Read ASCII AIG from result file (e.g. from reparam)
        try{
            InFile in(input);
            if (!in){ ShoutLn "ERROR! Could not open: %_", input; exit(1); }

            String line;
            while (!in.eof()){
                readLine(in, line);
                if (hasPrefix(line, "aig: ")){
                    uind sz = textToBin(&line[5], line.size() - 5);
                    In in2(&line[5], sz);
                    try{
                        readAiger(in2, N, false);
                        //*aiger*/For_Gatetype(N, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
                    }catch (Excp_AigerParseError err){
                        ShoutLn "PARSE ERROR! %_", err.msg;
                        exit(1);
                    }
                    goto Found;
                }
            }
            ShoutLn "ERROR! No 'aig: <data>' line found in input file."; exit(1);
          Found:;
            is_aiger = true;

        }catch (Excp_NlParseError err){
            ShoutLn "PARSE ERROR [line %_]! %_", err.line_no, err.msg;
            exit(1);
        }

    }else if (input == "@@"){
        // PAR mode: read netlist from stdin
        String params;
        try{
            receiveMsg_Task(N, params);
        }catch (Excp_Msg msg){
            ShoutLn "ERROR! %_", msg;
            exit(0);
        }
        /**/WriteLn "Received  : %_", info(N);
        /**/WriteLn "Parameters: %_", params;

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    // Sorting:
    if (sort_ext)
        sortExternals(N);

    // Run non-verification commands:
    if (cli.cmd == "info"){
        WriteLn "\a*%_\a* -- %_", input, info(N);
        if (!is_aiger || Has_Pob(N, constraints) || Has_Pob(N, fair_properties) || Has_Pob(N, fair_constraints))
            WriteLn "  %_", verifInfo(N);
        return 0;

    }else if (cli.cmd == "show"){
        if (is_aiger){
            //*aiger*/For_Gatetype(N, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
        }

        if (!cli.get("names").bool_val){
            N.clearNames();
            nameByCurrentId(N);
        }
        if (cli.get("cleanup").bool_val){
            removeUnreach(N, NULL, false); }

        if (cli.get("human").bool_val){
            introduceXorsAndMuxes(N);
            removeAllUnreach(N);
            normalizeXors(N);
            introduceBigAnds(N);
            introduceOrs(N);
        }

        if (cli.get("split").bool_val)
            splitFlops(N, true);

        WZet region;
        if (cli.get("root").choice != 0){     // -- choice 0 == all nodes
            if (cli.get("root").choice <= 2)
                region.add(getNode(N, cli.get("root")));
            else{
                const Vec<CLI_Val>& v = *cli.get("root").sub;
                for (uint i = 0; i < v.size(); i++)
                    region.add(getNode(N, v[i]));
            }

            growRegion(N, region, cli.get("area").string_val, cli.get("lim").int_val);
        }

        Vec<String> uif_names;
        if (cli.get("uifs").string_val != ""){
            InFile in(cli.get("uifs").string_val);
            if (!in){
                ShoutLn "ERROR! Could not open: %_", cli.get("uifs").string_val;
                exit(1); }

            Vec<char>   buf;
            Vec<Str>    fields;
            while (!in.eof()){
                readLine(in, buf);
                trim(buf);
                if (buf.size() == 0) continue;

                splitArray(buf.slice(), "=", fields);
                if (fields.size() != 2){
                    ShoutLn "ERROR! Incorrect format of \"uifs\" file.";
                    exit(1); }

                uif_names(stringToUInt64(fields[0])) = fields[1];
            }
        }

        if (output != ""){
            if (region.size() == 0) writeDot(output, N, &uif_names);
            else                    writeDot(output, N, region, &uif_names);
        }else{
            String gv = cli.get("gv").string_val;
            String tmp_dot;
            String tmp_ps;
            close(tmpFile("__bip_show_dot_", tmp_dot));
            close(tmpFile("__bip_show_ps_" , tmp_ps));
            if (region.size() == 0) writeDot(tmp_dot, N, &uif_names);
            else                    writeDot(tmp_dot, N, region, &uif_names);
            String cmd = (FMT "dot -Tpdf %_ > %_; %_ %_; rm %_ %_", tmp_dot, tmp_ps, gv, tmp_ps, tmp_dot, tmp_ps);
            int ignore ___unused = system(cmd.c_str());
        }
        return 0;
    }

    uint orig_num_pis = nextNum_PI(N);
    IntSet<uint> abstr;
    Vec<Wire> props;
    if (preprocess){
        // Check/compact numbering:
        if (!checkNumbering(N, false)){
            renumber(N);
            WriteLn "WARNING! Input file does not number its external elements (PI/PO/Flop) correctly.";
            WriteLn "Renumbering applied.";
        }

        if (!quiet) WriteLn "Parsed:  %_ -- %_", input, info(N);

        // Clean up etc:
        removeAllUnreach(N);
        if (hasGeneralizedGates(N)){
            WriteLn "NOTE! Expanding generalized AIG gates.";
            expandGeneralizedGates(N); }
        Assure_Pob0(N, strash);
        removeAllUnreach(N);

        if (!quiet) WriteLn "Cleaned: %_ -- %_", input, info(N);

        // Setup properties:
        //getInitialAbstr(N, cli.get("abstr").string_val, abstr, quiet);

        // <<== rather check for liveness properties here, and switch default command
        if (cli.cmd != "live" && cli.cmd != "ltl")
            setupProperties(N, prop_nums, inv_prop, props, quiet || is_aiger);
        setupInitialState(N, quiet || is_aiger);

        // Conjoin properties:
        if (cli.get("conjoin").bool_val){
            Wire w_conj = N.True();
            for (uind i = 0; i < props.size(); i++){
                Wire w = props[i]; assert(type(w) == gate_PO);
                w_conj = s_And(w_conj, w[0]);
            }
            For_Gatetype(N, gate_PO, w)
                remove(w);
            props.clear();
            props.push(N.add(PO_(0), w_conj));
        }
    }

    // Set up signal handler:
    setupSignalHandlers();

    // Write logo:
    if (show_logo && !quiet){
        WriteLn "\a*_______________________________________________________________________________\a0";
        WriteLn " \a/__  ___ __\a/                                                                    ";
        WriteLn " \a/|_)  |  |_)\a/             Author: \a*Niklas Een\a*                (C) UC Berkeley 2014";
        WriteLn " \a/|_) _|_ |\a/               Built:  \a*%<18%_\a*            Licence: \a*MIT/X11\a*", compileDate();
        WriteLn "\a*_______________________________________________________________________________\a0";
        NewLine;
    }

    // Run command:
    double T0  = cpuTime();
    double Tr0 = realTime();

    if (cli.cmd == "save-aig"){
        if (output == ""){
            output = setExtension(input, "aig");
            if (input == output){ ShoutLn "ERROR! To overwrite input, specify output name explicitly."; exit(1); }
        }

        foldConstraints(N);
        removeFlopInit(N);
        padInputs(N, orig_num_pis);
        if (!is_aiger && Has_Pob(N, properties)){
            Get_Pob(N, properties);
            for (uint i = 0; i < properties.size(); i++)
                properties[i].set(0, ~properties[i][0]);
        }
        writeAigerFile(output, N);
        WriteLn "Wrote: \a*%_\a*", output;

    }else if (cli.cmd == "save-smv"){
        if (output == ""){
            output = setExtension(input, "smv");
            if (input == output){ ShoutLn "ERROR! To overwrite input, specify output name explicitly."; exit(1); }
        }
        exportSmv(output, N);
        WriteLn "Wrote: \a*%_\a*", output;

    }else if (cli.cmd == "save-gig"){
        if (output == ""){
            output = setExtension(input, "gig");
            if (input == output){ ShoutLn "ERROR! To overwrite input, specify output name explicitly."; exit(1); }
        }
        if (!cli_save_gig.get("names").bool_val){
            N.clearNames();
            nameByCurrentId(N);
        }
        N.write(output);
        WriteLn "Wrote: \a*%_\a*", output;

    }else if (cli.cmd == "abs"){
        // Localization abstraction:
        Params_LocalAbstr P;
        P.max_conflicts = (cli_abs.get("confl").choice == 0) ? cli_abs.get("confl").int_val : UINT64_MAX;
        P.max_inspects  = vtimeout;
        P.max_depth     = (cli_abs.get("depth").choice == 0) ? (uint)cli_abs.get("depth").int_val : UINT_MAX;
        P.stable_lim    = (uint)cli_abs.get("stable").int_val;
        P.bob_stable    = (cli_abs.get("bob").choice == 0) ? (uint)cli_abs.get("bob").int_val : UINT_MAX;
        P.cpu_timeout   = timeout;
        P.quiet         = quiet;
        P.sat_verbosity = cli_abs.get("sat-verbosity").bool_val;
        P.randomize     = cli_abs.get("randomize").bool_val;
        P.renumber      = cli_abs.get("renumber").bool_val;
        P.dump_prefix   = cli_abs.get("dwr").string_val;

        Cex cex;
        int bug_free_depth;
        localAbstr(N, props, P, abstr, &cex, bug_free_depth);

        // Verify counterexample:
        if (!cex.null()){
            if (verifyCex(N, props, cex)) WriteLn "Counterexample VERIFIED!";
            else                          WriteLn "Counterexample is \a*\a/INCORRECT\a0!";
        }

        // Write result file:
        if (output != ""){
            OutFile out(output);
            if (abstr.size() > 0){
                FWriteLn(out) "result: %_", resultToString(l_Undef);
                writeAbstr(out, N, abstr);
            }else if (!cex.null()){
                FWriteLn(out) "result: %_", resultToString(l_False);
                writeCex(out, N, cex, orig_num_pis);
            }else
                FWriteLn(out) "result: %_", resultToString(l_Error);
            FWriteLn(out) "bug-free-depth: %_", bug_free_depth;
        }

        // Write AIGER file:
        String aig_output = cli_abs.get("aig").string_val;
        if (aig_output != "") writeAbstrAiger(N, abstr, aig_output, P.renumber, quiet);
        if (!quiet) writeResourceUsage(T0, Tr0);

    }else if (cli.cmd == "bmc"){
        Params_Bmc P;
        P.quant_claus    = cli_bmc.get("qc").bool_val;
        P.simple_tseitin = cli_bmc.get("st").bool_val;
        P.la_steps       = cli_bmc.get("la").int_val;
        P.la_decay       = cli_bmc.get("la-decay").float_val;
        P.quiet          = cli.get("quiet").bool_val;
        P.sat_solver = (cli.get("sat").enum_val == 0) ? sat_Zz :
                       (cli.get("sat").enum_val == 1) ? sat_Msc :
                       (cli.get("sat").enum_val == 2) ? sat_Abc :
                       (cli.get("sat").enum_val == 3) ? sat_Glu :
                       (cli.get("sat").enum_val == 4) ? sat_Glr :
                       (cli.get("sat").enum_val == 5) ? sat_Msr : (assert(false), sat_NULL);

        EffortCB_Timeout cb(vtimeout, timeout);
        Cex   cex;
        int   bug_free_depth;
        lbool result = bmc(N, props, P, &cex, &bug_free_depth, &cb);

        outputVerificationResult(N, props, result, &cex, orig_num_pis, NetlistRef(), bug_free_depth, false, output, quiet, T0, Tr0);

    }else if (cli.cmd == "imc"){
        Params_ImcStd P;
        P.fwd            = !cli_imc.get("bwd").bool_val;
        P.first_k        = (uint)cli_imc.get("k").int_val;
        P.simplify_itp   = cli_imc.get("simp").bool_val;
        P.prune_itp      = cli_imc.get("prune").bool_val;
        P.quant_claus    = cli_imc.get("qc").bool_val;
        P.simple_tseitin = cli_imc.get("st").bool_val;
        P.spin           = cli_imc.get("spin").bool_val;
        P.quiet          = cli.get("quiet").bool_val;
        EffortCB_Timeout cb(vtimeout, timeout);
        Cex     cex;
        Netlist N_inv;
        int     bug_free_depth;
        lbool   result = imcStd(N, props, P, &cex, N_inv, &bug_free_depth, &cb);

        outputVerificationResult(N, props, result, &cex, orig_num_pis, N_inv, bug_free_depth, cli.get("check").bool_val, output, quiet, T0, Tr0);

    }else if (cli.cmd == "pdr"){
        Params_Pdr P;
        P.seed = cli.get("seed").int_val;
        P.minimal_cex = cli.get("min-cex").bool_val;
        P.quiet = cli.get("quiet").bool_val;
        P.dump_invariant = cli.get("dump-invar").bool_val;
        if      (cli.get("stats").string_val == "clauses") P.output_stats = pdr_Clauses;
        else if (cli.get("stats").string_val == "time"   ) P.output_stats = pdr_Time;
        else if (cli.get("stats").string_val == "vars"   ) P.output_stats = pdr_Vars;
        else assert(false);
        EffortCB_Timeout cb(vtimeout, timeout);
        Cex     cex;
        Netlist N_inv;
        int     bug_free_depth;
        lbool   result = propDrivenReach(N, props, P, &cex, N_inv, &bug_free_depth, &cb);

        outputVerificationResult(N, props, result, &cex, orig_num_pis, N_inv, bug_free_depth, cli.get("check").bool_val, output, quiet, T0, Tr0);

    }else if (cli.cmd == "treb"){
        Params_Treb P;
        setParams(cli, P);
        EffortCB_Timeout cb(vtimeout, timeout);
        Cex     cex;
        Netlist N_inv;
        int     bug_free_depth;
        lbool   result = treb(N, props, P, &cex, N_inv, &bug_free_depth, &cb);

        outputVerificationResult(N, props, result, &cex, orig_num_pis, N_inv, bug_free_depth, cli.get("check").bool_val, output, quiet, T0, Tr0);

    }else if (cli.cmd == "pdr2"){
        Params_Pdr2 P;
        setParams(cli, P);
        Cex     cex;
        Netlist N_inv;
        bool    result = pdr2(N, props, P, &cex, N_inv);

        outputVerificationResult(N, props, lbool_lift(result), &cex, orig_num_pis, N_inv, 0, cli.get("check").bool_val, output, quiet, T0, Tr0);

    }else if (cli.cmd == "pmc"){
        Params_Pmc P;
        setParams(cli, P);
        Cex     cex;
        bool    result = pmc(N, props, P, &cex);

        outputVerificationResult(N, props, lbool_lift(result), &cex, orig_num_pis, Netlist_NULL, 0, cli.get("check").bool_val, output, quiet, T0, Tr0);

    }else if (cli.cmd == "sift"){
        bool result ___unused = sift2(N, props);

    }else if (cli.cmd == "isift"){
        bool result ___unused = isift(N, props);

    }else if (cli.cmd == "pp"){
        imcPP(N, props);
        if (!quiet) writeResourceUsage(T0, Tr0);

#if !defined(NO_BERKELEY_ABC)
    }else if (cli.cmd == "bdd"){
        Params_BddReach P;
        P.var_reorder = cli.get("reorder").bool_val;
        P.debug_output = cli.get("debug").bool_val;
        P.quiet = quiet;
      #if !defined(_MSC_VER)
        //bddReach(N, props, P);
        if (!quiet) writeResourceUsage(T0, Tr0);
      #else
        WriteLn "BDD reachability not available under Windows (yet!)";
      #endif
#endif

    }else if (cli.cmd == "simp"){
        if (N.typeCount(gate_PO) != 1){
            ShoutLn "ERROR! Can only handle netlist with exactly one PO.";
            exit(1); }

        Netlist M;
        Add_Pob0(M, strash);
        For_Gatetype(N, gate_PO, w_po)
            M.add(PO_(0), copyAndSimplify(w_po[0], M));

        if (output != ""){
            //*aiger*/For_Gatetype(M, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            writeAigerFile(output, M);
            WriteLn "Wrote AIGER: \a*%_\a*", output;
        }
        WriteLn "\a*Original:\a*    %_", info(N);
        WriteLn "\a*Simplified:\a*  %_", info(M);

    }else if (cli.cmd == "static-bmc"){
        uint k0   = cli_static_bmc.get("k0").bool_val;
        uint k1   = cli_static_bmc.get("k").bool_val;
        bool init = cli_static_bmc.get("init").bool_val;
        bool simp = cli_static_bmc.get("simp").bool_val;

        Netlist M;
        Wire m_out = staticBmc(N, props, k0, k1, init, simp, M);

        WriteLn "Unrolled size:  %_", info(M);

        if (hasExtension(output, "aig")){
            //*aiger*/For_Gatetype(M, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            writeAigerFile(output, M);
            WriteLn "Wrote AIGER: \a*%_\a*", output;

        }else if (output == "" || hasExtension(output, "cnf")){
            Add_Pob(M, fanout_count);
            WZet keep;
            For_Gates(M, w)
                if (fanout_count[w] > 1)
                    keep.add(w);

            MiniSat2s S;
            WMap<Lit> m2s;
            Clausify<MetaSat> CM(S, M, m2s, keep);
            CM.quant_claus    = cli_static_bmc.get("qc").bool_val;
            CM.simple_tseitin = cli_static_bmc.get("st").bool_val;
            S.addClause(CM.clausify(m_out));

            if (output == ""){
                S.setVerbosity(1);
                bool result = (S.solve() == l_True);
                WriteLn "Result: \a*%_\a*", result ? "SAT" : "UNSAT";
                NewLine;
                writeResourceUsage(T0, Tr0);
            }else{
                S.exportCnf(output);
                WriteLn "Wrote DIMACS: \a*%_\a*", output;
            }

        }else{
            ShoutLn "Unknown extension: %_", output;
            exit(1);
        }

    }else if (cli.cmd == "fix"){
        //testQuantify();
        Params_Fixed P;
        P.depth = cli.get("depth").int_val;
        P.quiet = cli.get("quiet").bool_val;
        EffortCB_Timeout cb(vtimeout, timeout);
        Cex     cex;
        Netlist N_invar;
        int     bug_free_depth;
        lbool   result ___unused = fixed(N, props, P, &cex, N_invar, &bug_free_depth, &cb);

    }else if (cli.cmd == "bbr"){
        Params_Bbr P;
        //P.depth = cli.get("depth").int_val;
        //P.quiet = cli.get("quiet").bool_val;
        Cex     cex;
        Netlist N_invar;
        int     bug_free_depth;
        lbool   result ___unused = bbr(N, props, P, &cex, N_invar, &bug_free_depth);

    }else if (cli.cmd == "sem"){
        //Cex     cex;
        //Netlist N_invar;
        //int     bug_free_depth;
        lbool   result ___unused = semAbs(N, props);

    }else if (cli.cmd == "reparam"){
        Params_Reparam P;
        P.cut_width = cli.get("width").int_val;
        P.resynth   = cli.get("resyn").bool_val;
        P.quiet = quiet;
        //**/reparamDebug(N);
        //**/exit(0);

        Netlist M;
        reparam(N, P);

        String filename = cli.get("aig").string_val;
        if (filename != ""){
            removeFlopInit(N);
            writeAigerFile(filename, N);
            WriteLn "Wrote: \a*%_\a*", filename;
        }

        filename = cli.get("gig").string_val;
        if (filename != ""){
            nameByCurrentId(N);
            N.write(filename);
            WriteLn "Wrote: \a*%_\a*", filename;
        }

        if (output != ""){
            OutFile out(output);

            String aiger;
            //*aiger*/For_Gatetype(N, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            writeAiger(aiger, N);
            //*aiger*/For_Gatetype(N, gate_PO, w) w.set(0, ~w[0]);    // -- invert properties
            FWrite(out) "aig: ";
            binToText(aiger.slice(), out);
            FNewLine(out);
            aiger.clear();

            writeAiger(aiger, M);
            FWrite(out) "cex-recons: ";
            binToText(aiger.slice(), out);
            FNewLine(out);
            aiger.clear();
        }

    }else if (cli.cmd == "check-invar"){
        Netlist N_invar;
        String  filename = cli.get("invar").string_val;
        if (!cli.get("add-prop").bool_val)
            props.clear();

        if (!readInvariant(filename, props, N_invar)){
            ShoutLn "ERROR! Could not read file: %_", filename;
            exit(1); }
        uint failed_prop;
        if (verifyInvariant(N, props, N_invar, &failed_prop))
            WriteLn "Invariant is CORRECT.";
        else{
            if (failed_prop == VINV_not_initial)
                WriteLn "Invariant is not implied by initial states!";
            else if (failed_prop == VINV_not_inductive)
                WriteLn "Invariant is not inductive!";
            else
                WriteLn "Invariant does not imply property %_", failed_prop;
        }

    }else if (cli.cmd == "simp-invar"){
        Vec<Vec<Lit> > invar;
        String filename = cli.get("invar").string_val;
        bool   fast     = cli.get("fast").bool_val;

        if (!readInvariant(filename, invar)){
            ShoutLn "ERROR! Could not read file: %_", filename;
            exit(1); }
        simpInvariant(N, props, invar, output, fast);

    }else if (cli.cmd == "cluster"){
        uint n_clusters = cli.get("n").int_val;
        uint n_pivots   = cli.get("pivots").int_val;
        uint seq_depth  = cli.get("seq").int_val;
        Vec<Vec<uint> > clusters;
        clusterProperties(N, n_clusters, n_pivots, seq_depth, clusters);

    }else if (cli.cmd == "saber"){
        uint target_enl = cli.get("k").int_val;
        uint n_flops    = cli.get("N").int_val;

        Netlist M;
        initBmcNetlist(N, props, M, true);
        Get_Pob(M, init_bad);

        Netlist H;
        deriveConstraints(M, init_bad[1], target_enl, n_flops, H);
        /**/H.write("H.gig"); WriteLn "Wrote: \a*H.gig\a*";

    }else if (cli.cmd == "absbmc"){
        absBmc(N, props);

    }else if (cli.cmd == "live"){
        Get_Pob(N, fair_properties);
        if (prop_nums.size() == 0 && fair_properties.size() == 1)
            prop_nums.push(0);

        if (prop_nums.size() != 1){
            ShoutLn "ERROR! Liveness algorithm can only check one property at a time.";
            exit(1); }

        uint prop_no = (uint)prop_nums[0];
        if (prop_no >= fair_properties.size()){
            ShoutLn "ERROR! Invalid property number.";
            ShoutLn "Use '-prop={%_..%_}", 0, fair_properties.size() - 1;
            exit(1); }

        Params_Liveness P;
        P.k = (cli.get("k").choice == 0) ? cli.get("k").int_val :
                                           cli.get("k").enum_val == 0 ? Params_Liveness::INC : Params_Liveness::L2S;
        P.aig_output = cli.get("aig").string_val;
        P.gig_output = cli.get("gig").string_val;
        P.witness_output = cli.get("wit").string_val;
        P.eng = (Params_Liveness::Engine)cli.get("eng").enum_val;
        P.bmc_max_depth = (cli.get("bmc-depth").choice == 0) ? (uint)cli.get("bmc-depth").int_val : UINT_MAX;

        Cex cex;
        uint loop_len;
        lbool result = liveness(N, prop_no, P, &cex, &loop_len);

        if (result != l_Undef)
            outputVerificationResult(N, props, result, &cex, orig_num_pis, NetlistRef(), -1, false, output, quiet, T0, Tr0, (result == l_True) ? -1 : (int)loop_len);
        if (!quiet) writeResourceUsage(T0, Tr0);

    }else if (cli.cmd == "ltl"){
        String spec_file = cli_ltl.get("spec").string_val;
        if (spec_file == ""){
            String nam = setExtension(input, "ltl");
            if (fileExists(nam))
                spec_file = nam;
            else
                spec_file = input + ".ltl";
        }

        Params_LtlCheck P;
        P.spec_gig    = cli.get("spec-gig").string_val;
        P.monitor_gig = cli.get("mon-gig").string_val;
        P.final_gig   = cli.get("final-gig").string_val;
        bool some_output = (P.spec_gig != "" || P.monitor_gig != "" || P.final_gig != "");

        P.free_vars = cli.get("fv").bool_val;
        P.fuzz_output = cli.get("fuzz").bool_val;
        P.debug_names = (cli.get("names").choice == 0) ? cli.get("names").bool_val :
                        /*otherwise*/some_output;

        String eng = cli.get("eng").string_val;
        P.eng = (eng == "klive") ? Params_LtlCheck::eng_KLive  :
                (eng == "bmc")   ? Params_LtlCheck::eng_L2sBmc :
                (eng == "pdr")   ? Params_LtlCheck::eng_L2sPdr :
                !some_output     ? Params_LtlCheck::eng_L2sPdr :
                /*otherwise*/      Params_LtlCheck::eng_NULL;

        P.witness_output = cli.get("wit").string_val;
        P.inv = cli.get("inv").bool_val;

        ltlCheck(N, spec_file, (prop_nums.size() == 0) ? 0 : prop_nums[0], P);     // -- only check one property
        if (!quiet) writeResourceUsage(T0, Tr0);

    }else if (cli.cmd == "constr"){
        uint k = cli.get("k").int_val;
        uint l = cli.get("l").int_val;

        Vec<GLit> bad;
        for (uint i = 0; i < props.size(); i++)
            bad.push(~props[i]);

        Vec<Cube> eq_classes;
        constrExtr(N, bad, k, l, eq_classes);
    }

    return 0;
}


/*
- Generic file writer? In case abstraction instantiation should be in GIG or SMV
*/
