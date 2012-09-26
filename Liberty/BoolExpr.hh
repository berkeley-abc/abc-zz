//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BoolExpr.hh
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Parser for boolean expressions in Liberty files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__BoolExpr_hh
#define ZZ__Liberty__BoolExpr_hh

#include "ZZ_BFunc.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


BoolFun parseBoolExpr(Str text, const Vec<Str>& var_names);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
