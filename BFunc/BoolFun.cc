//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BoolFun.cc
//| Author(s)   : Niklas Een
//| Module      : BFunc
//| Description : Represent functions using truth-tables.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Essentially a non-template based version of "BFunc" for representing functions up to 16 
//| variables or so.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "BoolFun.hh"
#include "BFuncStd.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global tables:


const uint64 bool_fun_proj[2][6] = {
    { 0xAAAAAAAAAAAAAAAAull, 0xCCCCCCCCCCCCCCCCull, 0xF0F0F0F0F0F0F0F0ull, 0xFF00FF00FF00FF00ull, 0xFFFF0000FFFF0000ull, 0xFFFFFFFF00000000ull },
    { 0x5555555555555555ull, 0x3333333333333333ull, 0x0F0F0F0F0F0F0F0Full, 0x00FF00FF00FF00FFull, 0x0000FFFF0000FFFFull, 0x00000000FFFFFFFFull }
        // -- second line is inverted
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pretty-printing:


void write_BoolFun(Out& out, const BoolFun& v)
{
    uint n_words = ((1 << v.nVars()) + 31) >> 5;
    Vec<uint> ftb(n_words);
    for (uint i = 0; i < ftb.size(); i++){
        if ((i & 1) == 0)
            ftb[i] = (uint)v[i>>1];
        else
            ftb[i] = (uint)(v[i>>1] >> 32);
    }

    Vec<uint> cover;
    irredSumOfProd(v.nVars(), ftb, cover);
    reverse(cover);
    FWrite(out) "%_", FmtCover(cover);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
