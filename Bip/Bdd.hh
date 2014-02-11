//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bdd.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Simple BDD reachability engine.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Bdd_hh
#define ZZ__Bip__Bdd_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_BddReach {
    bool    var_reorder;
    bool    quiet;
    bool    debug_output;

    Params_BddReach() :
        var_reorder (false),
        quiet       (false),
        debug_output(false)
    {}
};


lbool bddReach(NetlistRef N0, const Vec<Wire>& props, const Params_BddReach& P = Params_BddReach());


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
