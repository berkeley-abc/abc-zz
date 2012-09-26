//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CoPdr.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Co-factor based PDR.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| !! WORK IN PROGRESS !!
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__CoPdr_hh
#define ZZ__Bip__CoPdr_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_CoPdr {
    bool     quiet;

    Params_CoPdr() :
        quiet(false)
    {}
};


void copdr(NetlistRef N0, const Vec<Wire>& props, const Params_CoPdr& P);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
