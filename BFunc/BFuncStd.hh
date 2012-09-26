//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BFuncStd.hh
//| Author(s)   : Niklas Een
//| Module      : BFunc
//| Description : Standard functions operating on boolean-functions.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__BFunc__BFuncStd_hh
#define ZZ__BFunc__BFuncStd_hh

#include "BFunc.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void irredSumOfProd(uint n_vars, const Vec<uint>& ftb, Vec<uint>& out_cover, bool neg = false);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debugging:


struct FmtCover {
    const Vec<uint>& cover;
    FmtCover(const Vec<uint>& cover_) : cover(cover_) {}
};


template<> void fts_macro write_(Out& out, const FmtCover& v)
{
    if (v.cover.size() == 0)
        out += '0';

    else{
        for (uind i = 0; i < v.cover.size(); i++){
            if (v.cover[i] == 0)
                out += '1';
            else{
                if (i != 0)
                    out += " + ";
                for (uint j = 0; j < 32; j++){
                    if (v.cover[i] & (1 << j)){
                        if (j & 1) out += char('A' + (j >> 1));
                        else       out += char('a' + (j >> 1));
                    }
                }
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
