//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Saber.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT-based Approximate Backward Reachability
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Saber_hh
#define ZZ__Bip__Saber_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Main:
void deriveConstraints(NetlistRef N, Wire bad, uint target_enl, uint n_flops, /*out*/NetlistRef H);

// Helpers:
Lit  clausifyFtb(const Vec<Lit>& out_lits, const Vec<uint>& ftb, SatStd& S);
void approxPreimage(const Vec<Wire>& outs, const Vec<uint>& ftb, const Vec<Wire>& ins, bool fixed_point, /*out*/Vec<uint>& preimg_ftb);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
