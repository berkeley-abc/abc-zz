//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description : Parsers for fault-tree formats.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
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
void readFaultTree(String tree_file, /*outputs:*/Gig& N, Vec<double>& ev_probs, Vec<String>& ev_names)
{
    Map<String, GLit> name2gate;
    Vec<char> ftext;    // -- fanin names are stored here

    Str text = readFile(tree_file);
    if (!text) Throw (Excp_ParseError) "Could not read fault tree from: %_", tree_file;

    uint split = UINT_MAX;
    for (uint i = 0; i < text.size()-1; i++){
        if (text[i] == '%' && text[i+1] == '%'){
            split = i;
            break; }
    }
    if (split == UINT_MAX) Throw (Excp_ParseError) "File does not contain '%%' splitter: %_", tree_file;

    // Read probabilities:
    {
        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text.slice(split + 2), ";", lines);
        for (uint i = 0; i < lines.size(); i++){
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, "=", fs);
            if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;

            double prob = 0;
            try{ prob = stringToDouble(strip(fs[1])); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s; }

            Wire w;
            if (prob > 0){
                if (prob < 1){
                    w = N.add(gate_PI);
                    ev_names(w.num()) = strip(fs[0]);
                    ev_probs(w.num()) = prob;
                }else
                    w = N.True();
            }else
                w = ~N.True();

            bool exists = name2gate.set(String(strip(fs[0])), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_", fs[0], tree_file;
        }
    }

    // Read tree:
    {
        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text.slice(0, split), ";", lines);
        for (uint i = 0; i < lines.size(); i++){
            // Split line into components:
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, "=", fs);
            if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;

            Str name = strip(fs[0]);
            Str gate;
            Str args;

            if (!has(fs[1], '(')){
                // Assume 'a = b' statement (treat it as a single-input AND):
                gate = slize("AND");
                args = strip(fs[1]);

            }else{
                splitArray(fs[1], "(", fs);
                if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;
                gate = strip(fs[0]);

                args = strip(fs[1]);
                if (args.size() == 0 || args.last() != ')') Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s;
                args.pop();
            }

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
            else if (eq(gate, "XOR"))
                w = N.addDyn(gate_Odd, fs.size());
            else if (pfx(gate, "GE_")){
                w = N.addDyn(gate_CardG, fs.size());
                uint k = 0;
                try{ k = stringToUInt64(gate.slice(3)); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", tree_file, s; }
                w.arg_set(k);
            }else
                Throw (Excp_ParseError) "No such gate type '%_' in: %_", gate, tree_file;

            bool exists = name2gate.set(String(name), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_", name, tree_file;

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
                bool s = false;
                if (name[0] == '~'){
                    name = name.slice(1);
                    s = true; }

                GLit w_in;
                if (name2gate.peek(name, w_in))
                    w.set(i, w_in ^ s);
                else
                    Throw (Excp_ParseError) "Missing gate '%_' used as fanin in: %_", name, tree_file;
            }
        }
    }
    dispose(text);

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


// Zero/one-probability events will be eliminated during parsing.
void readFtp(String ftp_file, /*outputs:*/Gig& N, Vec<double>& ev_probs, Vec<String>& ev_names, uint proc_no)
{
    Map<String, GLit> name2gate;
    Vec<char> ftext;    // -- fanin names are stored here

    Str text = readFile(ftp_file);
    if (!text) Throw (Excp_ParseError) "Could not read fault tree from: %_", ftp_file;

    uint split = UINT_MAX;
    for (uint i = 0; i < text.size()-5; i++){
        if (text[i] == 'I' && text[i+1] == 'M' && text[i+2] == 'P' && text[i+3] == 'O' && text[i+4] == 'R' && text[i+5] == 'T'){
            split = i;
            break; }
    }
    if (split == UINT_MAX) Throw (Excp_ParseError) "File does not contain 'IMPORT' followed by probabilities: %_", ftp_file;

    // Read probabilities:
    {
        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text.slice(split + 6), "\r\n", lines);
        for (uint i = 0; i < lines.size(); i++){
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, " ", fs);
            if (fs.size() > 0 && pfx(fs[0], "LIMIT")) continue;

            if (fs.size() != 2) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", ftp_file, s;

            double prob = 0;
            try{ prob = stringToDouble(strip(fs[0])); }catch (...){ Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", ftp_file, s; }

            Wire w;
            if (prob > 0){
                if (prob < 1){
                    w = N.add(gate_PI);
                    ev_names(w.num()) = strip(fs[1]);
                    ev_probs(w.num()) = prob;
                }else
                    w = N.True();
            }else
                w = ~N.True();

            bool exists = name2gate.set(String(strip(fs[1])), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_", fs[1], ftp_file;
        }
    }

    // Read tree:
    Vec<GLit> top;
    {
        Vec<Str> lines;
        Vec<Str> fs;
        strictSplitArray(text.slice(0, split), "\r\n", lines);
        for (uint i = 0; i < lines.size(); i++){
            // Split line into components:
            Str s = strip(lines[i]);
            if (s.size() == 0) continue;

            splitArray(s, " ", fs);
            if (fs.size() > 0 && pfx(fs[0], "ENDTREE")) continue;

            if (fs.size() > 0 && pfx(fs[0], "PROCESS")){
                // Top node(s):
                for (uint i = 1; i < fs.size(); i++){
                    GLit w;
                    if (name2gate.peek(fs[i], w))
                        top.push(w);
                    else
                        Throw (Excp_ParseError) "PROCESS lists missing gate '%_' in: %_", fs[i], ftp_file;
                }
                continue;
            }

            if (fs.size() < 3) Throw (Excp_ParseError) "Invalid line in: %_\n  -->> %_", ftp_file, s;

            Str name = strip(fs[0]);
            Str gate = fs[1];

            // Create gate:
            Wire w;
            if (eq(gate, "*"))
                w = N.addDyn(gate_Conj, fs.size() - 2);
            else if (eq(gate, "+"))
                w = N.addDyn(gate_Disj, fs.size() - 2);
            else
                Throw (Excp_ParseError) "No such gate type '%_' in: %_", gate, ftp_file;

            bool exists = name2gate.set(String(name), w);
            if (exists) Throw (Excp_ParseError) "Event '%_' listed twice in: %_", name, ftp_file;

            // Store fanin names:
            for (uint i = 2; i < fs.size(); i++){
                w.set_unchecked(i-2, GLit(ftext.size()));     // -- bypass fanin checking
                append(ftext, fs[i]);
                ftext.push(0);
            }
        }

        For_Gates(N, w){
            if (w == gate_PI) continue;

            for (uint i = 0; i < w.size(); i++){
                uint offset = w[i].id;
                w.set_unchecked(i, Wire_NULL);

                Str name = slize(&ftext[offset]);
                bool s = false;
                if (name[0] == '-'){
                    name = name.slice(1);
                    s = true; }

                GLit w_in;
                if (name2gate.peek(name, w_in))
                    w.set(i, w_in ^ s);
                else
                    Throw (Excp_ParseError) "Missing gate '%_' used as fanin in: %_", name, ftp_file;
            }
        }
    }
    dispose(text);

    if (top.size() == 0)
        Throw (Excp_ParseError) "No PROCESS nodes listed: %_", ftp_file;

    if (proc_no == UINT_MAX){
        Wire w_top = N.addDyn(gate_Disj, top.size());
        for (uint i = 0; i < top.size(); i++)
            w_top.set(i, top[i] + N);
        N.add(gate_PO).init(w_top);

    }else{
        if (proc_no >= top.size())
            Throw (Excp_ParseError) "No such process '%_'. Max is '%_'.", proc_no, top.size() - 1;
        N.add(gate_PO).init(top[proc_no] + N);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
