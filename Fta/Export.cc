//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Export.cc
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Export.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void writeXml(String filename, const Gig& N0, const Vec<double>& ev_probs, const Vec<String>& ev_names)
{
    Gig N;
    N0.copyTo(N);
    For_Gates(N, w){
        For_Inputs(w, v){
            if (v.sign)
                w.set(Input_Pin(v), N.add(gate_Not).init(+v));
        }
    }

    // Write header:
    OutFile out(filename);
    FWriteLn(out) "<?xml version=\"1.0\"?> <!DOCTYPE open-psa> <open-psa>";
    FWriteLn(out) "<define-fault-tree name=\"zz-fault-tree\">";     // -- name goes here if we start caring about it...

    FWriteLn(out) "<define-gate name=\"__TRUE__\" > <constant value=\"true\" /> </define-gate >";
    FWriteLn(out) "<define-gate name=\"__FALSE__\" > <constant value=\"false\" /> </define-gate >";

    // Write tree:
    Vec<Str> type_name;
    type_name(gate_Not) = slize("not");
    type_name(gate_And) = slize("and");
    type_name(gate_Conj) = slize("and");
    type_name(gate_Or) = slize("or");
    type_name(gate_Disj) = slize("or");
    type_name(gate_Xor) = slize("xor");
    type_name(gate_Odd) = slize("xor");
    type_name(gate_CardG) = slize("atleast");

    Wire w_top = N(gate_PO, 0)[0];

    String extra;
    For_UpOrder(N, w){
        if (w == gate_PI) continue;
        if (w == gate_PO) continue;

        if (!type_name[w.type()]){
            ShoutLn "INTERNAL ERROR! Unexpected type in 'writeXml()': %_", w;
            assert(false); }

        if (w == gate_CardG)
            FWrite(extra) " min=\"%_\"", w.arg();

        if (w == w_top) FWrite(out) "<define-gate name=\"__TOP__\">";
        else            FWrite(out) "<define-gate name=\"%_\">", w.lit();
        FWrite(out) " <%_%_>", type_name[w.type()], extra;
        extra.clear();

        For_Inputs(w, v){
            if (+v == GLit_True)
                FWrite(out) " <gate name=\"%_\" />", !v.sign ? "__TRUE__" : "__FALSE__";
            else{
                assert(!v.sign);    // -- cannot handle signs yet; need to insert '<not>' gate.
                if (v == gate_PI)
                    FWrite(out) " <basic-event name=\"%_\" />", ev_names[v.num()];
                else
                    FWrite(out) " <gate name=\"%_\" />", v.lit();
            }
        }
        FWriteLn(out) " </%_> </define-gate>", type_name[w.type()];
    }

    // Write probabilities:
    For_Gatetype(N, gate_PI, w)
        FWriteLn(out) "<define-basic-event name=\"%_\"> <float value=\"%_\" /> </define-basic-event>",
            ev_names[w.num()], ev_probs[w.num()];

    // Write footer:
    FWriteLn(out) "</define-fault-tree>";
    FWriteLn(out) "</open-psa>";
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
