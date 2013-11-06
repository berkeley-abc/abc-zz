//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description : Parsers for fault-tree formats.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Parser.hh"
#include "ZZ/Generics/Map.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Zero/one-probability events will be eliminated during parsing.
void readFaultTree(String tree_file, String prob_file, /*outputs:*/Gig& N, Vec<double>& ev_probs, Vec<String>& ev_names)
{
    Map<String, GLit> name2gate;
    Vec<char> ftext;    // -- fanin names are stored here

    // Read probabilities:
    {
        Str text = readFile(prob_file);
        if (!text) Throw (Excp_ParseError) "Could not read probabilities from: %_", prob_file;

        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text, ";", lines);
        for (uint i = 0; i < lines.size(); i++){
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, " ", fs);
            if (fs.size() != 3 || !eq(fs[1], "=")) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", prob_file, s;

            double prob = 0;
            try{ prob = stringToDouble(fs[2]); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", prob_file, s; }

            Wire w;
            if (prob > 0){
                if (prob < 1){
                    w = N.add(gate_PI);
                    ev_names(w.num()) = fs[0];
                    ev_probs(w.num()) = prob;
                }else
                    w = N.True();
            }else
                w = ~N.True();

            bool exists = name2gate.set(String(fs[0]), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_", fs[0], prob_file;
        }
    }

    // Read tree:
    {
        Str text = readFile(tree_file);
        if (!text) Throw (Excp_ParseError) "Could not read fault tree from: %_", tree_file;

        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text, ";", lines);
        for (uint i = 0; i < lines.size(); i++){
            // Split line into components:
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, "=", fs);
            if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;

            Str name = strip(fs[0]);

            splitArray(fs[1], "(", fs);
            if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;
            Str gate = strip(fs[0]);

            Str args = strip(fs[1]);
            if (args.size() == 0 || args.last() != ')') Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;
            args.pop();

            splitArray(args, ",", fs);

            // Create gate:
            Wire w;
            if (eq(gate, "NOT")){
                if (fs.size() != 1) Throw (Excp_ParseError) "NOT operator takes a single argument in: %_\n  -->> %_", tree_file, s;
                w = N.add(gate_Not, fs.size());
            }else if (eq(gate, "AND"))
                w = N.addDyn(gate_Conj, fs.size());
            else if (eq(gate, "OR"))
                w = N.addDyn(gate_Disj, fs.size());
            else if (pfx(gate, "GE_")){
                w = N.addDyn(gate_CardG, fs.size());
                uint k = 0;
                try{ k = stringToUInt64(gate.slice(3)); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", prob_file, s; }
                w.arg_set(k);
            }

            bool exists = name2gate.set(String(name), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_ + %_", name, tree_file, prob_file;

            // Store fanin names:
            for (uint i = 0; i < fs.size(); i++){
                w.set_unchecked(i, GLit(ftext.size()));     // -- bypass fanin checking
                append(ftext, strip(fs[i]));
                ftext.push(0);
            }
        }

        For_Gates(N, w){
            if (w == gate_PI) continue;

            for (uint i = 0; i < w.size(); i++){
                uint offset = w[i].id;
                w.set_unchecked(i, Wire_NULL);

                Str name = slize(&ftext[offset]);
                GLit w_in;
                if (name2gate.peek(name, w_in))
                    w.set(i, w_in);
                else
                    Throw (Excp_ParseError) "Missing gate '%_' used as fanin in: %_", name, tree_file;
            }
        }
    }

    Add_Gob(N, FanoutCount);
    Wire w_top;
    For_Gates(N, w){
        if (nFanouts(w) == 0){
            if (!w_top)
                w_top = w;
            else
                Throw (Excp_ParseError) "Fault-tree does not have a unique top-node: %_ %_", w_top, w;
        }
    }

    if (!w_top) Throw (Excp_ParseError) "Empty fault-tree?";

    N.add(gate_PO).init(w_top);
}

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
