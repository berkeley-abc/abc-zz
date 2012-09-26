//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DynFanouts.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__DynFanouts_hh
#define ZZ__Netlist__DynFanouts_hh

#include "ZZ/Generics/DMemPool.hh"
#include "StdPec.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_DynFanouts -- dynamic fanout lists: 


class Pec_DynFanouts : public Pec, public NlLis {
    DMemPool mem;

    struct Ext {
        uint  cap;
        d_off off;
    };

    struct Outs {
        uint is_ext : 1;
        uint sz     : 31;   // -- fanouts stored 
        uint cnt;           // -- actual number of fanouts (without stale connections or duplicates)
        union {
            CConnect  inl;
            Ext       ext;
        };

        Outs() : is_ext(0), sz(0), cnt(0) { inl.pin = 0; inl.parent = glit_NULL; }
    };

    WMap<Outs> fanouts;

  //________________________________________
  //  Internal helpers:

    void init();
    void addFanout(Outs& o, GLit w, uint pin);
    void clearFanouts(Outs& o);
    void shrinkFanouts(Outs& o);
    void compress(Outs& o, GLit w0);

public:
  //________________________________________
  //  Constructor:

    Pec_DynFanouts(const Pec_base& base);
   ~Pec_DynFanouts();

    void clear() { reconstruct(mem); fanouts.clear(); }

  //________________________________________
  //  Pec interface:

    void moveTo (Pec& /*dst*/      )       { assert(false); }
    void copyTo (Pec& /*dst*/      ) const { assert(false); }
    bool equalTo(const Pec& /*dst*/) const { assert(false); return false; }
    void load   (In&  /*in*/       )       { clear(); init(); }
    void save   (Out& /*out*/      ) const {}
    void read   (In&  /*text_in*/  )       { clear(); init(); }
    void write  (Out& /*text_out*/ ) const {}
    void compact(NlRemap& /*xlat*/)        { assert(false); }

  //________________________________________
  //  Listener interface:

    void update(Wire w, uint pin, Wire w_old, Wire w_new);
    void remove(Wire w);
    void compact(const Vec<gate_id>& new_ids) { assert(false); }

  //________________________________________
  //  Fanout methods:

    uint count(Wire w) const;   // -- return number of fanouts 
    Fanouts operator[](Wire w); // -- return fanouts (will remove stale fanouts from internal representation)

    void trim();    // -- compacts memory usage (apply when netlist becomes stable)

  //________________________________________
  //  Debug:

    uint allocSize(Wire w) const;
};


//=================================================================================================


inline uint Pec_DynFanouts::count(Wire w) const
{
    Outs o = fanouts[w];
    return o.cnt;
}


inline Fanouts Pec_DynFanouts::operator[](Wire w)
{
    NetlistRef N = netlist(Pec::nl); assert_debug(Pec::nl == w.nl());
    Outs& o = fanouts(w);
    if (o.sz > o.cnt)
        compress(o, +w);

    return Fanouts(N, o.is_ext ? (CConnect*)mem.deref(o.ext.off) : &o.inl, o.sz);
}


inline uint Pec_DynFanouts::allocSize(Wire w) const
{
    Outs o = fanouts[w];
    return (!o.is_ext) ? 0 : o.ext.cap;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
