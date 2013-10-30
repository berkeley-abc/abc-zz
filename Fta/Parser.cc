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


// Zero-probability events will be eliminated during parsing.
void readFaultTree(String tree_file, String prob_file, /*outputs:*/Gig& N, Vec<String>& ev_names)
{
    Map<String, GLit> name2gate;

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

            double prob;
            try{ prob = stringToDouble(fs[2]); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", prob_file, s; }

            Wire w;
            if (prob > 0){
                w = N.add(gate_PI);
                ev_names(w.num()) = fs[0];
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

            Dump(name, gate, fs);
            // AND, OR, NOT, GE_3  (Conj, Disj, (sign), CardG?)

            //g75 = OR(g33, e35, e47, g44, e59, g51, e37, e49, e61, e83, e87, e91, e85, e89, e93)


        }
    }
}

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
