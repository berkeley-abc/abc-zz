//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Reparam.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Reparameterization technique based on local transformations.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Reparam_hh
#define ZZ__Bip__Reparam_hh

#include "ZZ_CmdLine.hh"
#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Reparametrization


struct Params_Reparam {
    uint    cut_width;      // Maximum cut-width to consider
    bool    resynth;        // Resynthesize logic to remove more PIs
    bool    quiet;          // Suppress output.

    Params_Reparam() :
        cut_width(8),
        resynth  (true),
        quiet    (false)
    {}
};


void reparam(NetlistRef N, const Params_Reparam& P);
void reconstructCex(NetlistRef M, CCex& cex);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
