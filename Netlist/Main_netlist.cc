#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Netlist.hh"
#include "ExportImport.hh"
#include "StdLib.hh"
#include "StdPob.hh"

using namespace ZZ;

namespace ZZ { void test(); }



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct EchoLis : NlLis {
    void update(Wire w, uint pin, Wire w_old, Wire w_new) { WriteLn "update(w=%_, pin=%_, w_old=%_, w_new=%_)", w, pin, w_old, w_new; }
    void add   (Wire w)                                   { WriteLn "add(w=%_)", w; }
    void remove(Wire w)                                   { WriteLn "remove(w=%_)", w; }
    void compact(const Vec<gate_id>& new_ids)             { WriteLn "compact(...)"; }
    void eqMove(Wire w_src, Wire w_dst)                   { WriteLn "eqMove(w_src=%_, w_dst=%_)", w_src, w_dst; }

    EchoLis(NetlistRef N) : NlLis(N.nl()) {}
};


void randomizeInputs(Wire w, const Vec<Wire>& ws, uint64& seed)
{
    for (uind i = 0; i < w.size(); i++)
        w.set(i, ws[irand(seed, ws.size())] ^ irand(seed, 2));
}


/*
namespace ZZ {
    ZZ_Decl_Initializer(REG_PEC_MemInfo);

    static uintp apa() {
        return (uintp)(void*)&ZZ_Initializer_REG_PEC_MemInfo_instance;
    }
}
*/


int main(int argc, char** argv)
{
    ZZ_Init;

#if 0
    cli.add("input" , "string", arg_REQUIRED, "Input AIGER or GIG file. May be gzipped.", 0);
    cli.add("output", "string", ""          , "Output AIGER or GIG file.", 1);
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

    }else if (hasExtension(input, "gig")){
        try{
            N.read(input);
        }catch (Excp_NlParseError err){
            ShoutLn "PARSE ERROR [line %_]! %_", err.line_no, err.msg;
            exit(1);
        }

    }else{
        ShoutLn "ERROR! Unknown file extension: %_", input;
        exit(1);
    }

    // Do something...
    WriteLn "After parsing   : %_", info(N);
#endif

#if 0
    Netlist N;
    Wire x = N.add(PI_(0));
    Wire y = N.add(PI_(1));
    Wire f = N.add(And_(), x, y);
    Wire g = N.add(And_(), x, f);

    Add_Pob(N, dyn_fanouts);
    Vec<Wire> ps;
    for (uint i = 0; i < 100; i++)
        ps.push(N.add(PO_(i), g));
    WriteLn "%_:  cnt=%_  cap=%_   mem=%DB", g, dyn_fanouts.count(g), dyn_fanouts.allocSize(g), memUsed();

    for (uint i = 0; i < ps.size(); i++)
        ps[i].disconnect(0);
    WriteLn "%_:  cnt=%_  cap=%_   mem=%DB", g, dyn_fanouts.count(g), dyn_fanouts.allocSize(g), memUsed();
    dyn_fanouts.trim();
    WriteLn "%_:  cnt=%_  cap=%_   mem=%DB   fs=%_", g, dyn_fanouts.count(g), dyn_fanouts.allocSize(g), memUsed(), dyn_fanouts[g];

    for (uint i = 0; i < ps.size(); i++)
        ps[i].set(0, g);
    WriteLn "%_:  cnt=%_  cap=%_   mem=%DB   fs=%_", g, dyn_fanouts.count(g), dyn_fanouts.allocSize(g), memUsed(), dyn_fanouts[g];

    dyn_fanouts.trim();
    WriteLn "%_:  cnt=%_  cap=%_   mem=%DB", g, dyn_fanouts.count(g), dyn_fanouts.allocSize(g), memUsed();
    exit(0);


    For_All_Gates(N, w) WriteLn "%_:  cnt=%_  fs=%_   (cap=%_)", w, dyn_fanouts.count(w), dyn_fanouts[w], dyn_fanouts.allocSize(w);
    NewLine;

    g.set(0, x);
    For_All_Gates(N, w) WriteLn "%_:  cnt=%_  fs=%_   (cap=%_)", w, dyn_fanouts.count(w), dyn_fanouts[w], dyn_fanouts.allocSize(w);
    NewLine;

    remove(g);
    For_All_Gates(N, w) WriteLn "%_:  cnt=%_  fs=%_   (cap=%_)", w, dyn_fanouts.count(w), dyn_fanouts[w], dyn_fanouts.allocSize(w);
    NewLine;
#endif

    Netlist N;
    Wire f = N.add(Uif_(42), 10);
    Wire g = N.add(Uif_(4711), 20);
    Wire h = N.add(Uif_(666), 30);

    remove(h);
    remove(f);
    remove(g);

    For_Gatetype(N, gate_Uif, w)
        WriteLn "%_: %_ %_", w, w.size(), attr_Uif(w).sym;

    return 0;
}
