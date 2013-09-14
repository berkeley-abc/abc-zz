//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Strash.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Structural hashing of AIGs, XIGs, Lut4 and Npn4 netlists.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__Strash_hh
#define ZZ__Gig__Strash_hh

#include "Gig.hh"
#include "ZZ/Generics/Set.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Support types:


enum GateHashType {
    ght_Bin,        // two inputs
    ght_Tri,        // three inputs
    ght_Lut,        // four inputs + argument (FTB or eq-class)
};


template<GateHashType type>
struct GateHash {
    const GigObj& obj;
    GateHash(const GigObj& obj_) : obj(obj_) {}

    uint64 hash(GLit p) const;
    bool   equal(GLit p, GLit q) const;
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Strash:


class GigObj_Strash : public GigObj, public GigLis {
    Set<GLit,GateHash<ght_Bin> >  and_nodes;
    Set<GLit,GateHash<ght_Bin> >  xor_nodes;
    Set<GLit,GateHash<ght_Tri> >  mux_nodes;
    Set<GLit,GateHash<ght_Tri> >  maj_nodes;
    Set<GLit,GateHash<ght_Tri> >  one_nodes;
    Set<GLit,GateHash<ght_Tri> >  gmb_nodes;
    Set<GLit,GateHash<ght_Tri> >  dot_nodes;
    Set<GLit,GateHash<ght_Lut> >  lut_nodes;

    bool initializing;      // -- Set during execution of 'strashNetlist()' to modify the behavior of 'removing()'

    void rehashNetlist();   // -- Assumes a strashed netlist with an empty 'strash' object.
    void strashNetlist();   // -- Rebuilds netlist bottom-up and removes redundant nodes.

public:
    GigObj_Strash(Gig& N_);
   ~GigObj_Strash();

  //________________________________________
  //  Hashed gate creation:

    GLit add_And (GLit u, GLit v, bool just_try);
    GLit add_Xor (GLit u, GLit v, bool just_try);
    GLit add_Mux (GLit s, GLit tt, GLit ff, bool just_try);
    GLit add_Maj (GLit u, GLit v , GLit w, bool just_try);
    GLit add_One (GLit u, GLit v , GLit w, bool just_try);
    GLit add_Gamb(GLit u, GLit v , GLit w, bool just_try);
    GLit add_Dot (GLit u, GLit v , GLit w, bool just_try);
    GLit add_Lut4(GLit d0, GLit d1, GLit d2, GLit d3, ushort ftb, bool just_try);

  //________________________________________
  //  GigObj interface:

    void init()                         { strashNetlist(); }
    void load(In&)                      { rehashNetlist(); }
    void save(Out&) const               {}
    void copyTo(GigObj& dst) const      { static_cast<GigObj_Strash&>(dst).rehashNetlist(); }
    void compact(const GigRemap& remap) { rehashNetlist(); }

  //________________________________________
  //  Listener interface:

    void removing(Wire w, bool);
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate creation:


// AIG:
//
Wire aig_And(Wire x, Wire y, bool just_try = false);

macro Wire aig_Or   (Wire x, Wire y)           { return ~aig_And(~x, ~y); }
macro Wire aig_Mux  (Wire s, Wire d1, Wire d0) { return ~aig_And(~aig_And(s, d1), ~aig_And(~s, d0)); }
macro Wire aig_Xor  (Wire x, Wire y)           { return aig_Mux(x, ~y,  y); }
macro Wire aig_Equiv(Wire x, Wire y)           { return aig_Mux(x,  y, ~y); }


// XIG:
//
Wire xig_Xor (Wire x, Wire y, bool just_try = false);
Wire xig_Mux (Wire s, Wire d1, Wire d0, bool just_try = false);
Wire xig_Maj (Wire x, Wire y, Wire z, bool just_try = false);
Wire xig_Gamb(Wire x, Wire y, Wire z, bool just_try = false);
Wire xig_One (Wire x, Wire y, Wire z, bool just_try = false);
Wire xig_Dot (Wire x, Wire y, Wire z, bool just_try = false);      // (x ^ y) | (x & z) 

macro Wire xig_And  (Wire x, Wire y, bool just_try = false) { return aig_And(x, y, just_try); }

macro Wire xig_Or   (Wire x, Wire y) { return ~xig_And(~x, ~y); }
macro Wire xig_Equiv(Wire x, Wire y) { return ~xig_Xor( x , y); }


// LUT4:
//
Wire lut4_Lut(Gig& N, ushort ftb, GLit w[4], bool just_try = false);    // -- will modify 'w[]'

macro Wire lut4_Lut(Gig& N, ushort ftb, GLit w0 = GLit_NULL, GLit w1 = GLit_NULL, GLit w2 = GLit_NULL, GLit w3 = GLit_NULL) {
    GLit w[4];
    w[0] = w0; w[1] = w1; w[2] = w2; w[3] = w3;
    return lut4_Lut(N, ftb, w); }

// Note that constant LUTs are not allowed (use 'N.True()'), hence at least one input is required.
// Also, netlist is retrieved from 'w0'.
macro Wire lut4_Lut(ushort ftb, Wire w0, Wire w1 = Wire_NULL, Wire w2 = Wire_NULL, Wire w3 = Wire_NULL) {
    GLit w[4];
    w[0] = w0; w[1] = w1; w[2] = w2; w[3] = w3;
    return lut4_Lut(*w0.gig(), ftb, w); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
