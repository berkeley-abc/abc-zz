//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Flatten.hh
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Flatten a hierarchical circuit.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Verilog__Flatten_hh
#define ZZ__Verilog__Flatten_hh

#include "Parser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Flatten {
    uint store_names;       // 0=no, 1=external only, 2=all
    bool strict_aig;
    bool blackbox_verific;
    char hier_sep;
    Params_Flatten(bool store_names_ = 0, bool strict_aig_ = false, bool blackbox_verific_ = false, char hier_sep_ = '/') :
        store_names(store_names_), strict_aig(strict_aig_), blackbox_verific(blackbox_verific_), hier_sep(hier_sep_) {}
};


uint flatten(const Vec<VerilogModule>& modules, /*out*/NetlistRef N_flat, const Params_Flatten& P); // -- may throw 'Excp_Msg'.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
