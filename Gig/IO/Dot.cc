//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Dot.cc
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Quickly ported from the implementation for the old netlist; needs to be looked over!
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Dot.hh"
#include "ZZ_Npn4.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// In 'grow_spec', 'i' means grow on input side, 'o' on output side, 'b' on both.
void growRegion(Gig& N, WZet& region, String grow_spec, uint lim)
{
    Auto_Gob(N, Fanouts);
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
                Fanouts fs = fanouts(w);
                for (uint k = 0; k < fs.size(); k++)
                    tmp.add(fs[k]);
            }
        }

        for (uind j = 0; j < tmp.list().size() && region.size() < lim; j++)
            region.add(tmp.list()[j]);
        tmp.clear();
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Export DOT file:


void writeDot(String filename, Gig& N, Vec<String>* uif_names)
{
    WZet region;
    For_All_Gates(N, w)
        region.add(w);

    writeDot(filename, N, region, uif_names);
}


static
void writeBox(Out& out, Gig& N, Wire w, Vec<char>& namebuf, Vec<String>* uif_names)
{
    String shape = "box";
  #if 0
    String name1 = N.names().get(+w, namebuf);  // -- first line; must not be empty
  #else
    String name1;
    FWrite(name1) "%_", +w;
  #endif
    String name2 = GateType_name[w.type()];     // -- second line; can be empty
    String color = "";
    String fill  = "";

  #if 0
    // Add a few more names, if present...
    if (N.names().size(w) >= 2)
        name1 = name1 + ", " + N.names().get(+w, namebuf, 1);
    if (N.names().size(w) >= 3)
        name1 = name1 + ", ...";
  #endif

    // Set attribute and add extra info to name:
    if (w == gate_And){
        shape = "ellipse";
        name2 = "";

    }else if (w == gate_PI || w == gate_PO){
        shape = "plaintext";
        color = "#0000D8";
        FWrite(name2) " %_", w.num();

    }else if (w == gate_FF){
        color = "#FFFFFF";
        fill  = "#0000C8";

        FWrite(name2) " %_", w.num();

    }else if (w == gate_Npn4){
        fill = "#FFFFCC";
        name2.clear();
        FWrite(name2) "%d:%.4X", w.arg(), npn4_repr[w.arg()];

    }else if (w == gate_Uif){
        uint sym = w.arg();
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


template<class T>
struct ScopedSetVar {
    T& var;
    T  old;
    ScopedSetVar(T& var_, T new_) : var(var_), old(var_) { var = new_; }
   ~ScopedSetVar() { var = old; }
};


void writeDot(String filename, Gig& N, const WZet& region, Vec<String>* uif_names)
{
    ScopedSetVar<bool> dummy(N.is_frozen, true);

    // Write header:
    OutFile out(filename);

    FWriteLn(out) "digraph Netlist {";
    FWriteLn(out) "edge [fontcolor=red labeldistance=1.75 arrowsize=0.7 arrowhead=none arrowtail=normal dir=both]";
    FNewLine(out);

    // Write gates in region:
    Auto_Gob(N, Fanouts);
    WZet      used;
    Vec<char> namebuf;
    for (uind i = 0; i < region.list().size(); i++){
        Wire w = region.list()[i] + N;
        if (!region.has(w)) continue;   // -- may not be compacted
        if (w == gate_Const && fanouts(w).size() == 0) continue;

        // Output node:
        FWrite(out) "w%_ [", id(w);
        writeBox(out, N, w, namebuf, uif_names);
        FWriteLn(out) "]";

        // Output fanins:
        For_Inputs(w, v){
            FWriteLn(out) "w%_->w%_ [taillabel=%_%_]", id(w), id(v), Input_Pin(v), sign(v)?" arrowtail=dotnormal color=\"#00A800\"":"";
            used.add(v);
        }

        // Output fanouts going outside region:
        uint count = 0;
        Fanouts fs = fanouts(w);
        for (uint i = 0; i < fs.size(); i++)
            if (!region.has(fs[i]))
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
