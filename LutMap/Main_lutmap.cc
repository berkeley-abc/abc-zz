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
#include "Unmap.hh"


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


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input"  , "string", arg_REQUIRED, "Input AIGER, GIG or GNL.", 0);
    cli.add("output" , "string", ""          , "Output GNL file (optional).", 1);
    cli.add("keep"   , "string", ""          , "List of forcable gates (only gor GNL input).");
    cli.add("blif"   , "string", ""          , "Save original input in BLIF format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("pnl"    , "string", ""          , "Save original input in PNL format (for debugging only). Add '@' as last character to skip mapping.");
    cli.add("ftbs"   , "string", ""          , "Write all FTBs to a file (for analysis).");
    cli.add("N"      , "uint"  , "10"        , "Cuts to keep per node.");
    cli.add("iters"  , "uint"  , "4"         , "Number of mapping phases.");
    cli.add("df"     , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("recycle", "bool"  , "yes"       , "Recycle cuts for faster iterations.");
    cli.add("dopt"   , "bool"  , "no"        , "Delay optimize (defaul is area).");
    cli.add("comb"   , "bool"  , "no"        , "Remove white/black boxes and sequential elements (may change delay profile).");
    cli.add("mux"    , "bool"  , "yes"       , "Do MUX and XOR extraction first.");
    cli.add("melt"   , "bool"  , "no"        , "Undo the 3-input LUT mapping of HDL-ICE.");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    String blif   = cli.get("blif").string_val;
    String pnl    = cli.get("pnl").string_val;
    Params_LutMap P;
    P.cuts_per_node = cli.get("N").int_val;
    P.n_rounds      = cli.get("iters").int_val;
    P.delay_factor  = cli.get("df").float_val;
    P.map_for_delay = cli.get("dopt").bool_val;
    P.recycle_cuts  = cli.get("recycle").bool_val;

    // Read input file:
    double  T0 = cpuTime();
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

#if 1
    N.is_frozen = false;
    WriteLn "Info: %_", info(N);
    //WriteLn "Putting into Lut4 form.";
    //putIntoLut4(N);
    WriteLn "Strashing";
    N.strash();
    WriteLn "Info: %_", info(N);
    N.unstrash();

    N.is_frozen = false;
#endif

    if (cli.get("melt").bool_val)
        expandLut3s(N);

    if (cli.get("comb").bool_val)
        makeCombinational(N);

    if (cli.get("mux").bool_val)
        introduceMuxesAsLuts(N);

    N.compact(true, false);
    //**/N.save("tmp.gnl"); writeGigForTechmap("tmp.gig", N); /**/WriteLn "wrote tmp.gnl/gig";

    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;
    Write "Input: %_", info(N);
    if (N.typeCount(gate_Lut4) > 0)
        Write "  (LUT-hist. %_)", infoLut4(N);
    if (keep_sz > 0)
        Write "  #keeps=%_", keep_sz;
    NewLine;

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

  #if 1
    lutMap(N, P, &keep);
  #else
    WMapX<GLit> remap;
    Write "\a/"; For_Gates(N, w) Dump(w); Write "\a/";

    lutMap(N, P, NULL, &remap);

    Write "\a/"; For_Gates(N, w) Dump(w); Write "\a/";
    WriteLn "== REMAP ==";
    for (uint i = 0; i < remap.base().size(); i++)
        WriteLn "%_ -> %_", Lit(i), remap[Lit(i)];
  #endif

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

#if 1
    WriteLn "Unmapping...";
    unmap(N);
    N.unstrash();
    removeUnreach(N);
    N.compact();
    WriteLn "Netlist: %_", info(N);
    putIntoLut4(N);
    lutMap(N, P, &keep);

    WriteLn "Unmapping 2...";
    unmap(N);
    N.unstrash();
    removeUnreach(N);
    N.compact();
    WriteLn "Netlist: %_", info(N);
    putIntoLut4(N);
    lutMap(N, P, &keep);
#endif


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
    For_Gates(N, w){
        if (w == gate_Lut6)
            sizeC[supSize(ftb(w))]++;
    }

    NewLine;
    WriteLn "Statistics:";
    for (uint i = 0; i <= 6; i++)
        if (sizeC[i] > 0)
            WriteLn "    LUT %_: %>11%,d  (%.1f %%)", i, sizeC[i], double(sizeC[i]) / N.typeCount(gate_Lut6) * 100;
    NewLine;
    WriteLn "    Total: %>11%,d", N.typeCount(gate_Lut6);
    WriteLn "    Edges: %>11%,d", sizeC[1] + 2*sizeC[2] + 3*sizeC[3] + 4*sizeC[4] + 5*sizeC[5] + 6*sizeC[6];

    return 0;
}
