#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "ZZ_Unix.hh"
#include "ZZ_Npn4.hh"
#include "ZZ_BFunc.hh"
#include "ZZ/Generics/Map.hh"
#include "ZZ/Generics/Sort.hh"
#include "LutMap.hh"
#include "GigReader.hh"
#include "PostProcess.hh"


using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


static
void introduceMuxesAsLuts(Gig& N)
{
    Wire sel, d1, d0;
    For_DownOrder(N, w){
        if (isMux(w, sel, d1, d0)){
            if (d0 == ~d1)  // (sel ? ~d0 : d0) == (sel ^ d0)
                change(w, gate_Lut4, 0x6666).init(d0, sel);
            else
                change(w, gate_Lut4, 0xCACA).init(d0, d1, sel);
        }
    }
}


static
void makeCombinational(Gig& N)
{
    Vec<GLit> ins;
    For_Gates(N, w){
        if (isCI(w)){
            if (w != gate_PI)
                change(w, gate_PI);

        }else if (isCO(w)){
            if (w != gate_PO){
                assert(w.size() == 1);
                Wire v = w[0];
                change(w, gate_PO);
                w.set(0, v);
            }

        }else if (w == gate_Delay){
            if (w.size() == 0)
                change(w, gate_PI);
            else{
                vecCopy(w, ins);
                Wire acc = ins[0] + N;
                for (uint i = 1; i < ins.size(); i++)
                    acc = mkXor(acc, ins[i] + N);

                change(w, gate_And);
                w.set(0, acc);
                w.set(1, acc);
            }

        }else if (w == gate_Sel){
            Wire v = w[0];
            change(w, gate_And);
            w.set(0, N.add(gate_PI));
            w.set(1, v);
        }
    }
}


macro uint sizeLut4(Wire w)
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


// Replace gate 'w' by two 'And' gates (the top one "changing" 'w').
static
void synthAnd3(Wire w, uchar ftb)
{
    bool s0 = false;
    bool s1 = false;
    bool s2 = false;

    if (ftb <  16){ s0 = true; ftb <<= 4; }
    if (ftb <  64){ s1 = true; ftb <<= 2; }
    if (ftb < 128){ s2 = true; ftb <<= 1; }
    assert(ftb == 128);

    Wire w12 = mkAnd(w[1] ^ s1, w[2] ^ s2);
    Wire w0 = w[0] ^ s0;
    change(w, gate_And).init(w0, w12);
}


// Not verified for functional correctness yet!
static
void expandLut3s(Gig& N)
{
    normalizeLut4s(N);

    uint n_lut3 = 0;
    uint n_and3 = 0;
    uint n_or3 = 0;
    WSeen inverted;
    For_Gates(N, w){
        if (w == gate_Lut4 && sizeLut4(w) == 3){
            ushort ftb = w.arg();
            assert((ftb & 255) == (w.arg() >> 8));

            // AND3:
            ftb &= 255;
            if ((ftb & (ftb-1)) == 0){
                synthAnd3(w, ftb);
                n_and3++; }

            // OR3:
            ftb ^= 255;
            if ((ftb & (ftb-1)) == 0){
                synthAnd3(w, ftb);
                inverted.add(w);
                n_or3++; }
            ftb ^= 255;

            // MUX:
            //...

            n_lut3++;
        }
    }

    For_Gates(N, w){
        For_Inputs(w, v)
            if (inverted.has(v))
                w.set(Iter_Var(v), ~v);
    }

    WriteLn "MELT: %_ out of %_ 3-input Lut4s changed into 'And(x, And(y, z))'.", n_and3 + n_or3, n_lut3;

    //**/Dump(n_lut3, n_and3, n_or3);
    //**/WriteLn "FTBs:";
    //**/for (uint i = 0; i < fs.size(); i++)
    //**/    WriteLn "  %.8b", (fs[i] & 255);
    //**/exit(0);
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
// PNL export:


static
void putu_(Out& out, uint64 val)
{
    if (val < 0x20000000){
        uint    v = (uint)val;
        if (v < 0x80)
            out.push(v);
        else{
            if (v < 0x2000)
                out.push(0x80 | (v >> 8)),
                out.push((uchar)v);
            else if (v < 0x200000)
                out.push(0xA0 | (v >> 16)),
                out.push((uchar)(v >> 8)),
                out.push((uchar)v);
            else
                out.push((v >> 24) | 0xC0),
                out.push((uchar)(v >> 16)),
                out.push((uchar)(v >> 8)),
                out.push((uchar)v);
        }
    }else
        out.push(0xE0),
        out.push((uchar)(val >> 56)),
        out.push((uchar)(val >> 48)),
        out.push((uchar)(val >> 40)),
        out.push((uchar)(val >> 32)),
        out.push((uchar)(val >> 24)),
        out.push((uchar)(val >> 16)),
        out.push((uchar)(val >> 8)),
        out.push((uchar)val);
}

macro uint64 encode64(int64  val) {
    return (val >= 0) ? (uint64)val << 1 : (((uint64)(~val) << 1) | 1); }

macro void puti_(Out& out, int64 val) {
    putu_(out, encode64(val)); }

macro void puts_(Out& out, cchar* text) {
    putu_(out, strlen(text));
    for (cchar* p = text; *p != 0; p++)
        out.push(*p);
}


macro void putWire(Out& out, Wire w)
{
    uint id;
    if      (w.id == gid_NULL)  id = 0;
    else if (w.id == gid_True)  id = 1;
    else if (w.id == gid_False) id = 2;
    else{
        assert(w.id >= gid_FirstUser);
        id = w.id - gid_FirstUser + 3;
    }

    putu_(out, (id << 1) | (uint)(bool)w.sign);
}


void writePnlFile(String filename, Gig& N)
{
    OutFile out(filename);

    // Format version:
    putu_(out, 0xAC1D0FF1CEC0FFEEull);   // tag for new format
    putu_(out, 1); putu_(out, 2);        // version 1.2

    // Gate types:
    putu_(out, 7);   // -- #gate-types
    puts_(out, "And");      // 1 from: And
    puts_(out, "Lut");      // 2 from: Lut4
    puts_(out, "PI");       // 3 from: PI
    puts_(out, "PO");       // 4 from: PO
    puts_(out, "FD01");     // 5 from: Seq
    puts_(out, "Pin");      // 6 from: Sel
    puts_(out, "Delay");    // 7 from: Box, Delay


    // Save gates:
    putu_(out, N.size() - gid_FirstUser + 3);
    for (uint i = gid_FirstUser; i < N.size(); i++){
        Wire w = N[i];
        //**/if (i < 20) WriteLn "%f", w;
        switch (w.type()){
        case gate_And:   putu_(out, /*gate-type*/1); break;
        case gate_Lut4:  putu_(out, /*gate-type*/2); putu_(out, w.arg()); break;
        case gate_PI:    putu_(out, /*gate-type*/3); puti_(out, w.num()); break;
        case gate_PO:    putu_(out, /*gate-type*/4); puti_(out, w.num()); break;
        case gate_Seq:   putu_(out, /*gate-type*/5); putu_(out, /*dyn-inputs*/w.size()); break;
        case gate_Sel:   putu_(out, /*gate-type*/6); puti_(out, w.arg()); break;
        case gate_Box:   putu_(out, /*gate-type*/7); putu_(out, /*dyn-inputs*/w.size()); puti_(out, 0); break;
        case gate_Delay: putu_(out, /*gate-type*/7); putu_(out, /*dyn-inputs*/w.size()); puti_(out, w.arg()); break;
        default: assert(false); }

        // Output fanins:
        for (uint i = 0; i < w.size(); i++)
            putWire(out, w[i]);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// LutMap parameters:


bool stringToBool(Str text)
{
    if (!text || text.size() == 0) return true;

    if      (eq (text, "1"    )) return true;
    else if (eq (text, "0"    )) return false;
    else if (eqi(text, "true" )) return true;
    else if (eqi(text, "false")) return false;
    else if (eqi(text, "on"   )) return true;
    else if (eqi(text, "off"  )) return false;
    else if (eqi(text, "yes"  )) return true;
    else if (eqi(text, "no"   )) return false;

    throw 0;
    return false;
}


void readParams(String filename, const Params_LutMap& P_default, /*out*/Vec<Params_LutMap>& Ps)
{
    InFile in(filename);
    if (!in){
        ShoutLn "ERROR! Could not open params file: %_", filename;
        exit(1); }

    Vec<char> buf;
    Vec<Str>  fs, gs;
    while (!in.eof()){
        readLine(in, buf);
        Str line = strip(buf.slice());
        if (line.size() == 0)
            continue;

        Ps.push(P_default);
        if (eq(line, "-")){
            continue; }

        Params_LutMap& P = Ps.last();

        splitArray(line, " ", fs);
        for (uint i = 0; i < fs.size(); i++){
            Str s = fs[i];
            if (s[0] == '-')
                s = s.slice(1);

            Str v = Str("");
            uind j = search(s, '=');
            if (j != UIND_MAX){
                v = s.slice(j+1);
                s = s.slice(0, j);
            }

            try{
                if (eq(s, "cuts_per_node") || eq(s, "N")){
                    P.cuts_per_node = stringToUInt64(v);
                }else if (eq(s, "n_rounds") || eq(s, "iters")){
                    P.n_rounds = stringToUInt64(v);
                }else if (eq(s, "delay_factor") || eq(s, "df")){
                    P.delay_factor = stringToDouble(v);
                }else if (eq(s, "map_for_delay") || eq(s, "dopt")){
                    P.map_for_delay = stringToBool(v);
                }else if (eq(s, "recycle_cuts") || eq(s, "recycle")){
                    P.recycle_cuts = stringToBool(v);
                }else if (eq(s, "lut_costs")){
                    splitArray(v, ",", gs);
                    if (gs.size() != 7){
                        ShoutLn "ERROR! Expected 7 values given to 'lut_costs'.";
                        exit(1); }
                    for (uint i = 0; i <= 6; i++)
                        P.lut_cost[i] = stringToUInt64(gs[i]);
                }else if (eq(s, "cost")){
                    if (eq(v, "unit")){
                        for (uint i = 0; i <= 6; i++)
                            P.lut_cost[i] = 1;
                        P.mux_cost = 1;
                    }else if (eq(v, "wire")){
                        for (uint i = 0; i <= 6; i++)
                            P.lut_cost[i] = i;
                        P.mux_cost = 2;
                    }else{
                        ShoutLn "ERROR! Invalid value given to parameter 'cost': %_", v;
                        exit(1); }
                }else if (eq(s, "mux_cost")){
                    P.mux_cost = stringToUInt64(v);
                }else if (eq(s, "use_ela") || eq(s, "ela")){
                    P.use_ela = stringToBool(v);
                }else if (eq(s, "use_fmux") || eq(s, "fmux")){
                    P.use_fmux = stringToBool(v);
                }else{
                    throw 0;
                }
            }catch (...){
                ShoutLn "ERROR! Invalid parameter: %_", fs[i];
                exit(1);
            }
        }
    }

#if 0
    WriteLn "Parameters:";
    for (uint i = 0; i < Ps.size(); i++){
        WriteLn "Iteration %_:", i;
        WriteLn "  cuts_per_node: %_", Ps[i].cuts_per_node;
        WriteLn "  n_rounds     : %_", Ps[i].n_rounds;
        WriteLn "  delay_factor : %_", Ps[i].delay_factor;
        WriteLn "  map_for_delay: %_", Ps[i].map_for_delay;
        WriteLn "  recycle_cuts : %_", Ps[i].recycle_cuts;
        WriteLn "  lut_cost     : %_", slice(Ps[i].lut_cost[0], Ps[i].lut_cost[7]);
        WriteLn "  mux_cost     : %_", Ps[i].mux_cost;
        WriteLn "  use_ela      : %_", Ps[i].use_ela;
        WriteLn "  use_fmux     : %_", Ps[i].use_fmux;
    }
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main:


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input"   , "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.add("output"  , "string", ""          , "Output GNL, GIG or BLIF file (optional).", 1);
    cli.add("keep"    , "string", ""          , "List of forcable gates (only for GNL input).");
    cli.add("blif"    , "string", ""          , "Save original input in BLIF format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("pnl"     , "string", ""          , "Save original input in PNL format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("gnl"     , "string", ""          , "Save original input in GNL format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("gig"     , "string", ""          , "Save original input in GIG format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("prot"    , "bool"  , "no"        , "Protect fanout-free gates by adding a PO to each one.");
    cli.add("strash"  , "bool"  , "no"        , "Apply structural hashing before mapping.");
    cli.add("melt"    , "bool"  , "no"        , "Undo the 3-input LUT mapping of HDL-ICE.");
    cli.add("comb"    , "bool"  , "no"        , "Remove white/black boxes and sequential elements (may change delay profile).");
    cli.add("mux"     , "bool"  , "no"        , "Do MUX and XOR extraction first.");
    cli.add("compact" , "bool"  , "yes"       , "Compact netlist at end of pre-processing.");
    cli.add("ftbs"    , "string", ""          , "Write all FTBs to a file (for analysis).");
    cli.add("rounds"  , "uint"  , "3"         , "Number of mapping rounds (with unmapping in between).");
    cli.add("N"       , "uint"  , "10"        , "Cuts to keep per node.");
    cli.add("iters"   , "uint"  , "4"         , "Phases in each mapping.");
    cli.add("df"      , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("dopt"    , "bool"  , "no"        , "Delay optimize (default is area).");
    cli.add("recycle" , "bool"  , "yes"       , "Recycle cuts for faster iterations.");
    cli.add("ela"     , "bool"  , "yes"       , "Use exact-local-area post-processing after each mapping phase.");
    cli.add("reprio"  , "bool"  , "yes"       , "Use reprioritization during induce-phase.");
    cli.add("fmux"    , "bool"  , "no"        , "Use special F7MUX and F8MUX in Xilinx series 7.");
    cli.add("cost"    , "{unit, wire}", "wire", "Reduce the number of LUTs (\"unit\") or sum of LUT-inputs (\"wire\").");
    cli.add("mux-cost", "int", "-1"           , "Cost of a F7 MUX (-1 encodes 2 or 1 depending on 'cost').");
    cli.add("params"  , "string", ""          , "A file containing options '-N' through '-cost'; one line per round (so number of lines sets '-rounds'). Omitted options will take values set by command line (use \"-\" for empty lines = all options are default)");
    cli.add("batch"   , "bool"  , "no"        , "Add last line summary for batch jobs.");
    cli.add("sig"     , "{off,full,pos}","off", "Map with signal tracking? (\"full\" includes negative literals).");
    cli.add("remap"   , "string", ""          , "Write signal mapping to this file (requires 'sig' to be set).");
    cli.add("verif"   , "bool"  , "no"        , "Signal tracking verification -- output files for equivalence checking.");
    cli.add("unmap"   , "bool"  , "no"        , "Unmap final mapped design.");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    String blif   = cli.get("blif").string_val;
    String pnl    = cli.get("pnl").string_val;
    String gnl    = cli.get("gnl").string_val;
    String gig    = cli.get("gig").string_val;
    Params_LutMap P;
    P.cuts_per_node  = cli.get("N").int_val;
    P.n_rounds       = cli.get("iters").int_val;
    P.delay_factor   = cli.get("df").float_val;
    P.map_for_delay  = cli.get("dopt").bool_val;
    P.recycle_cuts   = cli.get("recycle").bool_val;
    P.use_ela        = cli.get("ela").bool_val;
    P.reprio         = cli.get("reprio").bool_val;
    P.use_fmux       = cli.get("fmux").bool_val;
    P.end_with_unmap = cli.get("unmap").bool_val;

    if (cli.get("cost").enum_val == 0){
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = 1;
        P.mux_cost = 1;
    }else{
        for (uint i = 0; i <= 6; i++)
            P.lut_cost[i] = i;
        P.mux_cost = 2;
    }
    if (cli.get("mux-cost").int_val != -1)
        P.mux_cost = cli.get("mux-cost").int_val;

    // Read input file:
    double T0 = cpuTime();
    Gig N;
    WSeen keep;
    uint keep_sz = 0;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, false);
        else if (hasExtension(input, "aig.gpg")){
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
            if (cli.get("keep").string_val != ""){
                Str text = readFile(cli.get("keep").string_val);
                Vec<Str> fs;
                splitArray(text, " \n", fs);
                for (uint i = 0; i < fs.size(); i++)
                    if (!keep.add(N[stringToUInt64(fs[i])]))
                        keep_sz++;
            }
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

    uint orig_netlist_size = N.size();
    if (cli.get("remap").string_val != ""){
        if (cli.get("strash").bool_val){ ShoutLn "ERROR! Must not use '-strash' with '-remap'."; exit(1); }
        if (cli.get("melt")  .bool_val){ ShoutLn "ERROR! Must not use '-melt' with '-remap'."; exit(1); }
        if (cli.get("comb")  .bool_val){ ShoutLn "ERROR! Must not use '-comb' with '-remap'."; exit(1); }
        if (cli.get("mux")   .bool_val){ ShoutLn "ERROR! Must not use '-mux' with '-remap'."; exit(1); }
        if (cli.get("sig").enum_val == 0){ ShoutLn "ERROR! Must use '-sig' with '-remap'."; exit(1); }
    }

    // Pre-process netlist:
    N.is_frozen = false;
    WriteLn "Info: %_", info(N);

    if (cli.get("prot").bool_val){
        Assure_Gob(N, FanoutCount);
        For_Gates(N, w)
            if (nFanouts(w) == 0 && w != gate_PO)
                N.add(gate_PO).init(w);
    }

    if (cli.get("strash").bool_val){
        WriteLn "Strashing";
        N.strash();
        WriteLn "Info: %_", info(N);
        N.unstrash();
        N.is_frozen = false;
    }

    if (cli.get("melt").bool_val)
        expandLut3s(N);

    if (cli.get("comb").bool_val)
        makeCombinational(N);

    if (cli.get("mux").bool_val)
        introduceMuxesAsLuts(N);

    Vec<uint> protectors;
    if (cli.get("compact").bool_val)
        N.compact();
    else{
        // Cannot have fanout free gates in mapper; protect them, then remove protectors after mapping:
        Assure_Gob(N, FanoutCount);
        For_Gates(N, w)
            if (nFanouts(w) == 0 && w != gate_PO)
                protectors.push(N.add(gate_PO).init(w).num());
    }

#if 0   /*DEBUG*/
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
#endif  /*END DEBUG*/

    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;
    Write "Input: %_", info(N);
    if (N.typeCount(gate_Lut4) > 0)
        Write "  (LUT-hist. %_)", infoLut4(N);
    if (keep_sz > 0)
        Write "  #keeps=%_", keep_sz;
    NewLine;

    // Optionally output parsed netlist:
    if (blif != ""){
        bool quit = false;
        if (blif.last() == '@'){
            quit = true;
            blif.pop(); }

        writeBlifFile(blif, N);
        WriteLn "Wrote: \a*%_\a*", blif;

        if (quit) return 0;
    }

    if (pnl != ""){
        bool quit = false;
        if (pnl.last() == '@'){
            quit = true;
            pnl.pop(); }

        writePnlFile(pnl, N);
        WriteLn "Wrote: \a*%_\a*", pnl;

        if (quit) return 0;
    }

    if (gnl != ""){
        bool quit = false;
        if (gnl.last() == '@'){
            quit = true;
            gnl.pop(); }

        N.save(gnl);
        WriteLn "Wrote: \a*%_\a*", gnl;

        if (quit) return 0;
    }

    if (gig != ""){
        bool quit = false;
        if (gig.last() == '@'){
            quit = true;
            gig.pop(); }

        writeGigForTechmap(gig, N);
        WriteLn "Wrote: \a*%_\a*", gig;

        if (quit) return 0;
    }

    // Techmap
    Vec<Params_LutMap> Ps;
    if (cli.get("params").string_val != "")
        readParams(cli.get("params").string_val, P, Ps);
    else
        Ps.growTo(cli.get("rounds").int_val, P);

    if (cli.get("sig").enum_val == 0){
        // No signal tracking:
        lutMap(N, Ps);

    }else{
        // With signal tracking (for debugging)
        WMapX<GLit> remap;
        Gig N_orig;
        N.copyTo(N_orig);

        lutMap(N, Ps, &remap);
        /**/For_Gates(N_orig, w){ if (!isLogicGate(w)) if (!(+remap[w])){ Dump(w); assert(false); } }

        if (cli.get("sig").enum_val == 2){      // -- 2 == 'pos' (no negative literals)
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

        // Internal points verification -- turn combinational inputs into PIs and add POs to every internal point:
        if (cli.get("verif").bool_val){
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
                          #if 1
                            /**/if (vs.has(v)) WriteLn "Duplicate in v: %_", v;
                            /**/if (ns.has(n)) WriteLn "Duplicate in n: %_", n;
                            /**/vs.add(v); ns.add(n);
                            change(v, gate_PI);
                            change(n, gate_PI);
                            assert(v.num() == n.num());
                          #endif
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
                    /**/if (!(!v.isRemoved()))Dump(w);
                    assert(!v.isRemoved());
                }

            For_Gates(N, w)
                For_Inputs(w, v)
                    assert(!v.isRemoved());

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

        // Write signal map to file:
        String remap_file = cli.get("remap").string_val;
        if (remap_file != ""){
            OutFile out(remap_file);
            for (uint i = 0; i < orig_netlist_size; i++)
                if (remap[GLit(i)].id != 0)
                    FWriteLn(out) "%_ %C%_", i, remap[GLit(i)].sign ? '~' : 0, remap[GLit(i)].id;
            WriteLn "Wrote: \a*%s\a*", remap_file;
        }
    }

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

    // Remove protectors:
    if (protectors.size() > 0){
        for (uint i = 0; i < protectors.size(); i++)
            remove(N(gate_PO, protectors[i]));
        WriteLn "No protectors: %_", info(N);
    }

    // Optionally write mapped result to file:
    if (output != ""){
        if (hasExtension(output, "blif")){
            //N.setMode(gig_Lut6);
            writeBlifFile(output, N);
            WriteLn "Wrote: \a*%_\a*", output;
        }else if (hasExtension(output, "gnl")){
            N.save(output);
            WriteLn "Wrote: \a*%_\a*", output;
        }else if (hasExtension(output, "gig")){
            writeGigForTechmap(output, N);
            WriteLn "Wrote: \a*%_\a*", output;
        }else if (hasExtension(output, "aig")){
            expandXigGates(N);
            WriteLn "Expanded to AIG: %_", info(N);
            writeGigForTechmap(output, N);      // <<<== temporary
            WriteLn "Wrote: \a*%_\a*", output;
            WriteLn "NOTE! AIGER writer not finished yet; file is in .gig format for now!!";
        }else{
            ShoutLn "ERROR! Unknown file extension: %_", output;
            exit(1);
        }
    }

    if (cli.get("ftbs").string_val != ""){
        Map<uint64,uint> ftbs;
        uint* res;
        For_Gates(N, w){
            if (w == gate_Lut6){
                if (ftbs.get(ftb(w), res))
                    *res += 1;
                else
                    *res = 1;
            }
        }

        Vec<Pair<uint, uint64> > list;
        For_Map(ftbs)
            list.push(tuple(Map_Value(ftbs), Map_Key(ftbs)));
        sort_reverse(list);

        OutFile out(cli.get("ftbs").string_val);
        for (uint i = 0; i < list.size(); i++)
            FWriteLn(out) "%.16x %_", list[i].snd, list[i].fst;

        WriteLn "Wrote: \a*%_\a*", cli.get("ftbs").string_val;
    }

    // Print some statistics:
    Vec<uint> sizeC(7, 0);
    WMap<uint> depth;
    uint max_delay = 0;
    For_UpOrder(N, w){
        if (w == gate_Lut6)
            sizeC[supSize(ftb(w))]++;

        if (isCI(w))
            depth(w) = 0;
        else{
            if (w == gate_Mux){
                depth(w) = max_(depth[w[0]] + 1, max_(depth[w[1]], depth[w[2]]));
            }else{
                uint d = 0;
                For_Inputs(w, v)
                    newMax(d, depth[v]);
                if (isLogicGate(w))
                    depth(w) = d + 1;
                else if (w == gate_Delay)
                    depth(w) = d + w.arg() * DELAY_FRACTION;
                else
                    depth(w) = d;
            }
        }
        newMax(max_delay, depth[w]);
    }


    if (!P.end_with_unmap){
        NewLine;
        WriteLn "Statistics:";
        for (uint i = 0; i <= 6; i++)
            if (sizeC[i] > 0)
                WriteLn "    LUT %_: %>11%,d  (%.1f %%)", i, sizeC[i], double(sizeC[i]) / N.typeCount(gate_Lut6) * 100;
        NewLine;
        WriteLn "    LUTs : %>11%,d", N.typeCount(gate_Lut6);
        if (P.use_fmux)
            WriteLn "    MUXs : %>11%,d", N.typeCount(gate_Mux);
        WriteLn "    Edges: %>11%,d", sizeC[1] + 2*sizeC[2] + 3*sizeC[3] + 4*sizeC[4] + 5*sizeC[5] + 6*sizeC[6] + N.typeCount(gate_Mux);
        WriteLn "    Delay: %>11%,d", max_delay;
        NewLine;
        WriteLn "    CPU-time: %t", cpuTime();

        if (cli.get("batch").bool_val){
            Write "%>11%,d    ", N.typeCount(gate_Lut6);
            Write "%>11%,d    ", sizeC[1] + 2*sizeC[2] + 3*sizeC[3] + 4*sizeC[4] + 5*sizeC[5] + 6*sizeC[6] + N.typeCount(gate_Mux);
            Write "%>6%d    ", max_delay;
            Write "%>10%t", cpuTime();
            NewLine;
        }

      #if 1   /*DEBUG*/
        if (P.use_fmux && !P.quiet && !cli.get("batch").bool_val)
            checkFmuxes(N);

        if (getenv("DUMP_MUXES")){
            For_Gates(N, w)
                if (w == gate_Mux)
                    Dump(w);
        }
      #endif  /*END DEBUG*/
    }

    return 0;
}


    //N.save(<gnlfile>);
    // cuts_per_node=10 n_rounds=5    (one line per phase)
    //cmd = <bin> -params=<filename> -sig=pos -remap=<remapfile> <gnlfile> <gnl-out>
    //int stat = system(cmd);
    // read remap, read mapped netlist
