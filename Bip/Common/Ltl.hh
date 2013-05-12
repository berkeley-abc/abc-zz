//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Ltl.hh
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : LTL expression parser (NuSMV style)
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Common__Ltl_hh
#define ZZ__Bip__Common__Ltl_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parsing:


// Takes a zero-terminated string and returns a wire pointing to LTL expression built inside
// 'N'. Atoms are stored as operatorless LTL nodes with the name put in the name store of 'N'.
// Upon parse error, Wire_NULL is returned and 'err_msg' is set to formatted error string.
//
Wire parseLtl(cchar* text, NetlistRef N, String& err_msg);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Printing:


struct FmtLtl {
    Wire w;
    FmtLtl(Wire w_) : w(w_) {}
};


template<> fts_macro void write_(Out& out, const FmtLtl& f)
{
    Wire w = f.w;

    if (type(w) == gate_PO){
        FWrite(out) "%_", FmtLtl(w[0] ^ sign(w));
        return; }

    assert(type(w) == gate_Ltl);
    char op = attr_Ltl(w).op;
    uint scope = attr_Ltl(w).scope;

    if (op == 0)
        FWrite(out) "%s", w;
    else{
        out += '(';
        if (w[0]) FWrite(out) "%_ ", FmtLtl(w[0]);
        if (scope == 0) out += op;
        else            FWrite(out) "%_<%_>", op, scope;
        if (w[1]) FWrite(out) " %_", FmtLtl(w[1]);
        out += ')';
    }
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
