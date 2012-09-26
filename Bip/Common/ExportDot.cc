//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ExportDot.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Export a netlist to a GraphViz DOT file.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ExportDot.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// In 'grow_spec', 'i' means grow on input side, 'o' on output side, 'b' on both.
void growRegion(NetlistRef N, WZet& region, String grow_spec, uint lim)
{
    Auto_Pob(N, fanouts);
    WZet tmp;
    for (uint i = 0; i < grow_spec.size(); i++){
        if (grow_spec[i] == 'i' || grow_spec[i] == 'b'){
            for (uint j = 0; j < region.list().size(); j++){
                Wire w = N[region.list()[j]];
                For_Inputs(w, v)
                    tmp.add(v);
            }
        }

        if (grow_spec[i] == 'o' || grow_spec[i] == 'b'){
            for (uint j = 0; j < region.list().size(); j++){
                Wire w = N[region.list()[j]];
                for (uint k = 0; k < fanouts[w].size(); k++)
                    tmp.add(fanouts[w][k]);
            }
        }

        for (uind j = 0; j < tmp.list().size() && region.size() < lim; j++)
            region.add(tmp.list()[j]);
        tmp.clear();
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Export DOT file:


void writeDot(String filename, NetlistRef N, Vec<String>* uif_names)
{
    WZet region;
    For_All_Gates(N, w)
        region.add(w);

    writeDot(filename, N, region, uif_names);
}


static
void writeBox(Out& out, NetlistRef N, Wire w, Vec<char>& namebuf, Vec<String>* uif_names)
{
    String shape = "box";
    String name1 = N.names().get(+w, namebuf);  // -- first line; must not be empty
    String name2 = GateType_name[type(w)];      // -- second line; can be empty
    String color = "";
    String fill  = "";

    // Add a few more names, if present...
    if (N.names().size(w) >= 2)
        name1 = name1 + ", " + N.names().get(+w, namebuf, 1);
    if (N.names().size(w) >= 3)
        name1 = name1 + ", ...";

    // Set attribute and add extra info to name:
    if (type(w) == gate_And){
        shape = "ellipse";
        name2 = "";

    }else if (type(w) == gate_PI || type(w) == gate_PO){
        shape = "plaintext";
        color = "#0000D8";
        int num = (type(w) == gate_PI) ? attr_PI(w).number : attr_PO(w).number;
        if (num != num_NULL)
            FWrite(name2) " %_", num;

    }else if (type(w) == gate_Flop || type(w) == gate_SO){
        if (Has_Pob(N, flop_init)){
            Get_Pob(N, flop_init);
            FWrite(name1) " =%_", flop_init[w];
        }
        color = "#FFFFFF";
        fill = (type(w) == gate_Flop) ? "#0000C8" : "#8000C8";

        int num = (type(w) == gate_Flop) ? attr_Flop(w).number : attr_SO(w).number;
        if (num != num_NULL)
            FWrite(name2) " %_", num;

    }else if (type(w) == gate_Npn4){
        fill = "#FFFFCC";
        name2.clear();
        FWrite(name2) "%d:%.4X", attr_Npn4(w).cl, npn4_repr[attr_Npn4(w).cl];

    }else if (type(w) == gate_Uif){
        uint sym = attr_Uif(w).sym;
        if (uif_names && sym < uif_names->size() && (*uif_names)[sym] != "")
            FWrite(name2) " %_", (*uif_names)[sym];
        else
            FWrite(name2) " %_", sym;
    }

    // Output box:
    FWrite(out) "shape=%_", shape;

    if (color != "")
        FWrite(out) " fontcolor=\"%_\"", color;

    if (fill != "")
        FWrite(out) " style=filled fillcolor=\"%_\"", fill;

    if (name2 != "")
        FWrite(out) " label=\"%_\\n%_\"", name1, name2;
    else
        FWrite(out) " label=\"%_\"", name1;
}


void writeDot(String filename, NetlistRef N, const WZet& region, Vec<String>* uif_names)
{
    // Write header:
    OutFile out(filename);

    FWriteLn(out) "digraph Netlist {";
    FWriteLn(out) "edge [fontcolor=red labeldistance=1.75 arrowsize=0.7 arrowhead=none arrowtail=normal]";
    FNewLine(out);

    // Write gates in region:
    Auto_Pob(N, fanouts);
    WZet      used;
    Vec<char> namebuf;
    for (uind i = 0; i < region.list().size(); i++){
        Wire w = region.list()[i];
        if (!region.has(w)) continue;   // -- may not be compacted
        if (type(w) == gate_Const && fanouts[w].size() == 0) continue;

        // Output node:
        FWrite(out) "w%_ [", id(w);
        writeBox(out, N, w, namebuf, uif_names);
        FWriteLn(out) "]";

        // Output fanins:
        For_Inputs(w, v){
            FWriteLn(out) "w%_->w%_ [taillabel=%_%_]", id(w), id(v), Input_Pin_Num(v), sign(v)?" arrowtail=dotnormal color=\"#00A800\"":"";
            used.add(v);
        }

        // Output fanouts going outside region:
        uint count = 0;
        for (uint i = 0; i < fanouts[w].size(); i++)
            if (!region.has(fanouts[w][i]))
                count++;
        if (count > 0){
            FWriteLn(out) "t%_ [shape=plaintext fontcolor=\"#AAAAAA\" label=\"[%_]\"]", id(w), count;
            FWriteLn(out) "t%_->w%_ [style=bold color=\"#AAAAAA\"]", id(w), id(w);
        }
    }

    // Write '?' for fanins outside region:
    for (uind i = 0; i < used.list().size(); i++){
        Wire w = N[used.list()[i]];
        if (!region.has(w))
            FWriteLn(out) "w%_ [shape=plaintext label=\"?\"]", id(w);
    }

    FWriteLn(out) "}";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
