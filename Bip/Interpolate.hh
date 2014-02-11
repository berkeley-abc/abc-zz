//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Interpolate.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Proof iterator for producing interpolants.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Interpolate_hh
#define ZZ__Bip__Interpolate_hh

#include "ZZ_Netlist.hh"
#include "ZZ_MiniSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper types:


typedef int vtype;
static const vtype vtype_Undef = 0;
static const vtype vtype_A = 1;   // A-clauses consists of these (and common variables)
static const vtype vtype_B = 2;   // B-clauses consists of these (and common variables)
    // -- negative numbers are used for variables common to both A and B; '~num' gives
    // the flop number to use when building interpolant (flops are used as variables 
    // in interpolation).


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Proof-traversal and Interpolation:


class ProofItp : public ProofIter {
  //________________________________________
  //  State:

    Netlist    G;      // Netlist with all incrementally computed interpolants.
    Wire       last;   // Points to the top of the last interpolant.
    Vec<Wire>  id2g;   // Maps a clause ID to a node in 'G' ("sub-interpolant").
    Vec<Wire>  vars;   // Vector of variables (repr. as flops) constructed in 'G' (index on their number attribute).

    const Vec<vtype>& var_type;

    Vec<Wire> acc;     // Temporary used in 'chain()'.

  //________________________________________
  //  Helpers:

    bool isConn(Lit p) { return var_type[var(p)] < 0; }
    bool isHead(Lit p) { return var_type[var(p)] == vtype_A; }
    Wire getVar(Lit p);

public:
  //________________________________________
  //  Public interface:

    ProofItp(const Vec<vtype>& var_type_);
    void flushNetlist();    // -- you must also call the SAT solver's 'proofClearVisited()' after calling this method
    Wire getInterpolant() { return last; }
    NetlistRef netlist() const { return G; }

  //________________________________________
  //  ProofIter interface:

    void root   (clause_id id, const Vec<Lit>& c);
    void chain  (clause_id id, const Vec<clause_id>& cs, const Vec<Lit>& ps);
    void end    (clause_id id);
    void recycle(clause_id id);
    void clear  ();
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
