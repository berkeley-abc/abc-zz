#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "ZZ_Gig.hh"
#include "ZZ_Gig.IO.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
Wire getNode(Gig& N, const CLI_Val& v)
{
    if (v.type == cli_Int){
        if (v.int_val >= N.size() || N[v.int_val].isRemoved()){
            ShoutLn "ERROR! No such gate: w%_", v.int_val;
            exit(1); }
        return N[v.int_val];

    }else if (v.type == cli_String){
        const String& str = v.string_val.c_str();

        if (str.size() > 3 && str[2] == ':'){
            uint num = UINT_MAX;
            try{ num = stringToInt64(str.sub(3)); } catch(...) {}

            if (hasPrefix(str, "ff:")){
                return N(gate_FF, num);
            }else if (hasPrefix(str, "pi:")){
                return N(gate_PI, num);
            }else if (hasPrefix(str, "po:")){
                return N(gate_PO, num);
            }else{
                ShoutLn "ERROR! Invalid ':' prefix. Must be one of: ff, pi, po";
                exit(1);
            }
        }else{
            Ping;
            //GLit g = N.names().lookup(str.c_str());
            //if (g)
            //    return g + N;
        }
        ShoutLn "ERROR! No such gate: %_", v.string_val;
        exit(1);

    }else{
        ShoutLn "ERROR! Unexpected gate id/name: %_", v;
        exit(1);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    // Command line parsing:
    cli.add("input", "string", arg_REQUIRED, "Input GNL or AIGER file.", 0);
    cli.add("output", "string", "", "Output post-script file.", 1);
  #if !defined(__APPLE__)
    cli.add("gv", "string", "gv", "PDF viewer to run on result.");
  #else
    cli.add("gv", "string", "open", "PDF viewer to run on result.");
  #endif
    cli.add("cleanup", "bool", "no", "Remove unreachable nodes.");
    cli.add("human", "bool", "no", "Make logic more human readable (may lose names).");
    cli.add("split", "bool", "no", "Split flops into state inputs/outputs.");
    cli.add("root", "{all} | uint | string | [uint | string]", "all", "Seed nodes for area.");
    cli.add("area", "string", "iii", "Nodes to include. Grow by: i=inputs, o=outputs, b=both.");
    cli.add("lim", "uint", "50", "Maximum number of nodes to include in DOT output.");

    cli.parseCmdLine(argc, argv);
    String input  = cli.get("input").string_val;
    String output = cli.get("output").string_val;

    // Read input:
    Gig N;
    try{
        if (hasExtension(input, "aig"))
            readAigerFile(input, N, false);
        else if (hasExtension(input, "gnl"))
            N.load(input);
        else{
            ShoutLn "ERROR! Unknown file extension: %_", input;
            exit(1);
        }
    }catch (const Excp_Msg& err){
        ShoutLn "PARSE ERROR! %_", err.msg;
        exit(1);
    }


    if (cli.get("cleanup").bool_val)
        removeUnreach(N);

    if (cli.get("human").bool_val){
        introduceMuxes(N);
        //introduceXorsAndMuxes(N);
        //removeUnreach(N);
        //normalizeXors(N);     // <<== later
        //introduceBigAnds(N);
        //introduceOrs(N);
    }

    if (cli.get("split").bool_val){
        For_Gates(N, w){
            For_Inputs(w, v){
                if (v == gate_Seq)
                    w.set(Input_Pin(v), Wire_NULL);
            }
        }
    }
    N.is_frozen = true;

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

    if (output != ""){
        if (region.size() == 0) writeDot(output, N);
        else                    writeDot(output, N, region);

    }else{
        String gv = cli.get("gv").string_val;
        String tmp_dot;
        String tmp_ps;
        close(tmpFile("__bip_show_dot_", tmp_dot));
        close(tmpFile("__bip_show_ps_" , tmp_ps));
        if (region.size() == 0) writeDot(tmp_dot, N);
        else                    writeDot(tmp_dot, N, region);
        String cmd = (FMT "dot -Tpdf %_ > %_; %_ %_; rm %_ %_", tmp_dot, tmp_ps, gv, tmp_ps, tmp_dot, tmp_ps);
        int ignore ___unused = system(cmd.c_str());
    }

    return 0;
}


//void growRegion(Gig& N, WZet& region, String grow_spec, uint lim);
//void writeDot(String filename, Gig& N, Vec<String>* uif_names = NULL);
//void writeDot(String filename, Gig& N, const WZet& region, Vec<String>* uif_names = NULL);
