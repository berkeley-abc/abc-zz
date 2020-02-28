//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SmvInterface.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Quick wrapper for calling Cadence SMV as an external tool
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "SmvInterface.hh"
#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Export SMV:


struct SmvName {
    Wire w;
    SmvName(Wire w_) : w(w_) {}
};


void write_(Out& out, SmvName s)
{
    Wire w = s.w;
    if (sign(w))
        out += '!';
    w = +w;

    if (type(w) == gate_Const){
        assert(w == glit_True);
        out += '1';
    }else
        out += 'w', id(w);
}


void exportSmv(Out& out, NetlistRef N)
{
    // <<== flop init netlist
    Get_Pob(N, properties);
    For_Gatetype(N, gate_PO, w)
        properties.push(w);

    out += "MODULE main\n";

    out += "\nVAR\n";
    For_Gates(N, w){
        if (type(w) == gate_PI || type(w) == gate_Flop)
            out += ' ', ' ', SmvName(w), " : boolean;\n";
    }

    out += "\nASSIGN\n";
    For_Gatetype(N, gate_Flop, w){
        // <<= use initialization netlist here!
        out += "  init(", SmvName(w), ") := 0;\n";
        out += "  next(", SmvName(w), ") := ", SmvName(w[0]), ";\n";
    }

    out += "\nDEFINE\n";
    For_Gates(N, w){
        if (type(w) == gate_And)
            out += "  ", SmvName(w), "  := ", SmvName(w[0]), " & ", SmvName(w[1]), ";\n";
        else if (type(w) == gate_PO)    // -- buffer types should go here too
            out += "  ", SmvName(w), " := ", SmvName(w[0]), ";\n";
        else
            assert(type(w) == gate_PI || type(w) == gate_Flop);
    }

    out += "\n";
    for (uint i = 0; i < properties.size(); i++)
        out += "SPEC AG ", SmvName(properties[i][0]), "\n";
}


void exportSmv(String filename, NetlistRef N)
{
    OutFile out(filename);
    exportSmv(out, N);
}


static
bool verifyCex(NetlistRef N, const Cex& cex, const WZetL* abstr0 = NULL)
{
    // Simulate:
    XSimulate xsim(N);
    xsim.simulate(cex, abstr0);

    // Check if at least one property was falsified:
    Get_Pob(N, properties);
    For_Gatetype(N, gate_PO, w)
        properties.push(w);

    for (uind i = 0; i < properties.size(); i++){
        Wire bad = ~properties[i][0];
        if ((xsim[cex.depth()][bad] ^ sign(bad)) == l_True)
            return true;
    }

    return false;
}


lbool callSmv(NetlistRef N, Cex& cex)
{
    // Create SMV file:
    String filename = "zz_smv_XXXXXX";
  #if !defined(_MSC_VER) && !defined(sun)
    int   fd = mkstemp(filename.c_str());
    File  file(fd, WRITE);
  #else
    File  file(filename, "w");
  #endif
    Out   out(file);

    exportSmv(out, N);
    out.finish();
    file.close();

    // Call SMV:
    String cmd;
    if (getenv("ZZ_SMV_SILENT"))
        cmd = stringf("smv -force %s > /dev/null", filename.c_str());
    else if  (getenv("ZZ_SMV_VERBOSE"))
        cmd = stringf("smv -force %s", filename.c_str());
    else
        cmd = stringf("smv -force %s | grep iteration", filename.c_str());
    //**/printf("Issuing command: %s\n", cmd.c_str());
    int dummy ___unused = system(cmd.c_str());    // <<== need to handle Ctrl-C somehow...

    // Parse result:
    InFile    in(stringf("%s.out", filename.c_str()).c_str());
    Vec<char> text;
    lbool     result = l_Undef;
    uint      state  = UINT_MAX;
    cex.clear();
    while (!in.eof()){
        readLine(in, text);
        text.push(0);
        if (text[0] == '#'){
            /*ignore line directive*/;

        }else if (strncmp(text.base(), "/* truth value */ ", 18) == 0){
            if (text[18] == '0')
                result = l_False;
            else if (text[18] == '1')
                result = l_True;
            else
                fprintf(stderr, "ERROR! Could not parse result from calling SMV.\n"), exit(15);

        }else if (strncmp(text.base(), "/* state ", 9) == 0){
            state = atoi(text.base() + 9);
            assert(state >= 1);
            cex.inputs.growTo(state);
            cex.flops. growTo(1);       // -- only store initial value of flops
            state--;

        }else if (text[0] == '\\' && text[1] == 'w'){
            cchar* p   = text.base() + 2;
            uintg  gid = parseUInt(p);
            while (*p != '='){ assert(*p != 0); p++; }
            p++; assert(*p == ' ');
            p++;
            lbool value = (*p == '0') ? l_False : (*p == '1') ? l_True : l_Undef;   // -- what to do with '{}' and '?'?
            Wire  w = N[gid];

            if (type(w) == gate_PI)
                cex.inputs[state](w) = value;
            else if (type(w) == gate_Flop && state == 0)
                cex.flops[state](w) = value;
        }
    }
    if (result == l_Undef) fprintf(stderr, "ERROR! Could not parse result from calling SMV.\n"), exit(15);

    // Verify counter-example:
    if (state != UINT_MAX){
        if (!verifyCex(N, cex))
            fprintf(stderr, "ERROR! Counter-example from SMV did not verify.\n"), exit(16);
        //**/else printf("SMV counter-example verified!\n");

        // <<== if multiple properties, store which ones were falsified
    }

    // Clean up:
    unlink(stringf("%s.warn"  , filename.c_str()).c_str());
    unlink(stringf("%s.update", filename.c_str()).c_str());
    unlink(stringf("%s.stats" , filename.c_str()).c_str());
    unlink(stringf("%s.out"   , filename.c_str()).c_str());
    unlink(filename.c_str());

    unlink(".smv_lock");        // -- What if some other SMV is running in this directory? (seems to work)
    unlink(".smv_history");

    return result;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
