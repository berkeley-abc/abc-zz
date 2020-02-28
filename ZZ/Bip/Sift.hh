//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Sift.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Experimental invariant generation through learned clauses of SAT-solver.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Sift_hh
#define ZZ__Bip__Sift_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


lbool sift(NetlistRef N0, const Vec<Wire>& props);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
