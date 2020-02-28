//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BFuncStd.cc
//| Author(s)   : Niklas Een
//| Module      : BFunc
//| Description : Standard functions operating on boolean-functions.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "BFuncStd.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void irredSumOfProd(uint n_vars, const Vec<uint>& ftb, /*out*/Vec<uint>& cover, bool neg)
{
    uint n_words = ((1 << n_vars) + 31) >> 5;
    assert(ftb.size() == n_words);

    switch (n_vars){
    #define CallIsop(n) case n: { BFunc<n> f(ftb.base()); if (neg) f = ~f; isop(f, cover); break; }
    Apply_To_All_BFunc_Sizes(CallIsop)
    #undef CallIsop
    default: assert(false); }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
