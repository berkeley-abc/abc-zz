//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Sift2.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Second attempt at a sifting algorithm for inductive invariant finding.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Sift2_hh
#define ZZ__Bip__Sift2_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool sift2(NetlistRef N, const Vec<Wire>& props, Cex* cex = NULL, NetlistRef N_invar = Netlist_NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
