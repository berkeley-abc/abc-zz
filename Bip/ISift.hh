//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ISift.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Experimental invariant generation through interpolation.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__ISift_hh
#define ZZ__Bip__ISift_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool isift(NetlistRef N, const Vec<Wire>& props, Cex* cex = NULL, NetlistRef N_invar = Netlist_NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
