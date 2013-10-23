//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Clausify.cc
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This clausifier does a straight-forward Tseitin encoding of the netlist. To get a good CNF,
//| the circuit should be CNF mapped into 4-LUTs.
//|________________________________________________________________________________________________

#ifndef ZZ__Gip__Common__Clausify_hh
#define ZZ__Gip__Common__Clausify_hh

#include "ZZ_Gig.hh"
#include "ZZ_MetaSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void clausify(const Gig& N, const Vec<GLit>& roots, MetaSat& S, WMapX<Lit>& n2s, bool init_ffs = false, Vec<GLit>* new_ffs = NULL);
Lit  clausify(const Gig& N, GLit root             , MetaSat& S, WMapX<Lit>& n2s, bool init_ffs = false, Vec<GLit>* new_ffs = NULL);
void clausify(const Gig& N, const Vec<GLit>& roots, MetaSat& S, Vec<WMapX<Lit> >& n2s, uint depth);
Lit  clausify(const Gig& N, GLit root             , MetaSat& S, Vec<WMapX<Lit> >& n2s, uint depth);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
