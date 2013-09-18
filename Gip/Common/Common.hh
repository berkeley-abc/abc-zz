//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.hh
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : Common functions and datatypes for the various Gip engines.
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Gip__Common__Common_hh
#define ZZ__Gip__Common__Common_hh

#include "ZZ/Generics/Lit.hh"
#include "ZZ/Generics/Pack.hh"
#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Basic types:


typedef Pack<Lit> Cube;
typedef Pack<Lit> Clau;         // -- clause (same as cube but different interpretation)
static const Cube Cube_NULL;
static const Clau Clau_NULL;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Properties, counterexamples etc.:


enum PropType {
    pt_NULL,
    pt_Safe,
    pt_Live,
};


struct Prop {
    PropType type;
    uint     num;
    Prop(PropType type_ = pt_NULL, uint num_ = 0) : type(type_), num(num_) {}
};


struct Cex {
    Prop                prop;
    Vec<WMapN<lbool> >  pi;
    WMapN<lbool>        ff;
    uint                cycle_len;      // -- only for liveness

    Cex(uint size = 0)                     { init(size, 0, 0); }
    Cex(uint size, uint n_pis, uint n_ffs) { init(size, n_pis, n_ffs); }
    Cex(uint size, const Gig& N)           { init(size, N.enumSize(gate_PI), N.enumSize(gate_FF)); }

    void init(uint size, uint n_pis, uint n_ffs) {  // -- size (=number of states) is one more than depth (=number of transitions)
        cycle_len = 0;
        pi.growTo(size);
        for (uint i = 0; i < pi.size(); i++) pi[i].reserve(n_pis);
        ff.reserve(n_ffs);
    }

    uint size() const { return pi.size(); }
};


enum InvarType {
    it_NULL,
    it_Clauses,
    it_Netlist,
};


struct Invar {
    Prop      prop;
    InvarType type;
    Vec<Clau> C;
    Gig*      N;

    Invar() : type(it_NULL), N(NULL) {}
};


//=================================================================================================
// -- Printing:


template<> fts_macro void write_(Out& out, const PropType& v)
{
    if      (v == pt_NULL) out += "NULL";
    else if (v == pt_Safe) out += "Safe";
    else if (v == pt_Live) out += "Live";
    else assert(false);
}


template<> fts_macro void write_(Out& out, const Prop& v)
{
    FWrite(out) "%_[%_]", v.type, v.num;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Engine reporter -- base class:


struct EngRep {
    Out* out;   // -- must be set by subclass
    operator Out&() { return *out; }

    virtual void bugFreeDepth(Prop prop, uint depth) = 0;
    virtual void cex         (Prop prop, Cex& cex) = 0;         // -- engine cannot rely on CEX to be untouched!
    virtual void proved      (Prop prop, Invar* invar = NULL) = 0;
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


void prepareNetlist(Gig& N, const Vec<GLit>& sinks, Gig& M);
void completeCex(const Gig& N, Cex& cex);
bool verifyCex(const Gig& N, Prop prop, const Cex& cex);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
