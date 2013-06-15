#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.IO.hh"
#include "LutMap.hh"
#include "GigReader.hh"


using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Returns TRUE if 'w' is the top AND-gate of a balanced, 3 AND-gate tree making up a MUX.
// Note that XOR is a special case.
static
bool isMux(Wire w, Wire& sel, Wire& d1, Wire& d0)
{
    assert(!w.sign);
    if (w != gate_And)
        return false;

    Wire x = w[0];
    Wire y = w[1];
    if (x != gate_And || y != gate_And)
        return false;
    if (!x.sign || !y.sign)
        return false;

    Wire xx = x[0];
    Wire yx = x[1];
    Wire xy = y[0];
    Wire yy = y[1];
    if      (xx == ~xy){ sel = xx, d1 = ~yx, d0 = ~yy; return true; }
    else if (yx == ~xy){ sel = yx, d1 = ~xx, d0 = ~yy; return true; }
    else if (xx == ~yy){ sel = xx, d1 = ~yx, d0 = ~xy; return true; }
    else if (yx == ~yy){ sel = yx, d1 = ~xx, d0 = ~xy; return true; }

    return false;
}


static
void introduceMuxes(Gig& N)
{
    Wire sel, d1, d0;
    N.setMode(gig_FreeForm);

    For_DownOrder(N, w){
        if (isMux(w, sel, d1, d0))
            change(w, gate_Lut4, 0x5353).init(d0, d1, sel);     // <<== unverified + need to remove signs
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("input"  , "string", arg_REQUIRED, "Input AIGER.", 0);
    cli.add("output" , "string", ""          , "Output GNL file (optional).", 1);
    cli.add("blif"   , "string", ""          , "Save original input in BLIF format (for debugging only).");
    cli.add("N"      , "uint"  , "10"        , "Cuts to keep per node.");
    cli.add("iters"  , "uint"  , "4"         , "Number of mapping phases.");
    cli.add("df"     , "float" , "1.0"       , "Delay factor; optimal delay is multiplied by this factor to produce target delay.");
    cli.add("recycle", "bool"  , "yes"       , "Recycle cuts for faster iterations.");
    cli.add("dopt"   , "bool"  , "no"        , "Delay optimize (defaul is area).");
    cli.add("mux"    , "bool"  , "yes"       , "Do MUX and XOR extraction first.");
    cli.add("a"      , "bool"  , "no"        , "[EXPERIMENTAL] Auto-tune.");
    cli.parseCmdLine(argc, argv);

    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;
    String blif   = cli.get("blif").string_val;
    Params_LutMap P;
    P.cuts_per_node = cli.get("N").int_val;
    P.n_rounds      = cli.get("iters").int_val;
    P.delay_factor  = cli.get("df").float_val;
    P.map_for_delay = cli.get("dopt").bool_val;
    P.recycle_cuts  = cli.get("recycle").bool_val;

    // Read input file:
    double  T0 = cpuTime();
    Gig N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, false);
        else if (hasExtension(input, "gnl"))
            N.load(input);
        else if (hasExtension(input, "gig"))
            readGigForTechmap(input, N);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }

    if (cli.get("mux").bool_val)
        introduceMuxes(N);

    N.compact();

    double T1 = cpuTime();
    WriteLn "Parsing: %t", T1-T0;

    if (blif != ""){
        writeBlifFile(blif, N);
        WriteLn "Wrote: \a*%_\a*", blif;
    }

    if (cli.get("a").bool_val)
        lutMapTune(N, P);
    else
        lutMap(N, P);

    double T2 = cpuTime();
    WriteLn "Mapping: %t", T2-T1;

    if (output != ""){
        if (hasExtension(output, "blif")){
            writeBlifFile(output, N);
            WriteLn "Wrote: \a*%_\a*", output;
        }else if (hasExtension(output, "gnl")){
            N.save(output);
            WriteLn "Wrote: \a*%_\a*", output;
        }else{
            ShoutLn "ERROR! Unknown file extension: %_", output;
            exit(1);
        }
    }

    return 0;
}
