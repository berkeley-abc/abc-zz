//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Fanouts.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__Fanouts_hh
#define ZZ__Gig__Fanouts_hh

#include "Maps.hh"
#include "Gig.hh"
#include "Macros.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Connect:


// A 'Connect' is a 'Wire' plus a pin number. It is used to store fanout information ("which gate
// is pointing to me through what pin?"). Although not enforced by the data structure, when used
// for fanouts the sign of the connect will always match that of the 'pin'th input of the gate
// (i.e. 'sign(c[c.pin]) == sign(c)' for a connect 'c').


struct Connect : Wire {
    uint    pin;
    Connect() : Wire(), pin(UINT_MAX) {}
    Connect(Wire w, uint p) : Wire(w), pin(p) {}

  //________________________________________
  //  Operations on the child pointed to:

    Wire get() const {               // -- get the child pointed to by 'parent[pin]'
        return (*this)[pin]; }

    void set(Wire w_new) const {     // -- set the child to 'w_new'
        Wire w = *this;
        w.set(pin, w_new); }

    void replace(Wire w_new) const { // -- set the child sign-aware (must not pass 'Wire_NULL')
        assert(w_new);
        Wire w = *this;
        w.set(pin, w_new ^ w.sign); }

    void disconnect() const {        // -- disconnect the child (set it to 'Wire_NULL')
        Wire w = *this;
        w.disconnect(pin); }

  //________________________________________
  //  Comparison:

    bool operator==(const Connect& other) const {
        Wire w = *this;
        Wire v = other;
        return (w == v) && pin == other.pin; }

    bool operator<(const Connect& other) const {
        Wire w = *this;
        Wire v = other;
        return (w < v) || (w == v && pin < other.pin); }

    bool operator==(const Wire& other) const {
        Wire w = *this;
        Wire v = other;
        return (w == v); }

    bool operator<(const Wire& other) const {
        Wire w = *this;
        Wire v = other;
        return (w < v); }
};


// Special connects:
#if defined(ZZ_CONSTANTS_AS_MACROS)
    #define Connect_NULL  (Connect())
    #define Connect_ERROR (Connect(Wire_ERROR, UINT_MAX))
#else
    static const Connect Connect_NULL  = Connect();
    static const Connect Connect_ERROR = Connect(Wire_ERROR, UINT_MAX);
#endif


// Hashing:
template<> fts_macro uint64 hash_<Connect>(const Connect& c) {
    Wire w = c;
    return hash_(w) ^ (uint64(c.pin) << 32); }


// Printing:
template<> fts_macro void write_(Out& out, const Connect& c) {
    out += (Wire)c;
    out += '(', c.pin, ')'; }


template<> fts_macro void write_(Out& out, const Connect& c, Str flags) {
    write_(out, (const Wire&)c, flags);
    out += '(', c.pin, ')'; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Static fanouts:


//=================================================================================================
// -- Compact connect:


struct CConnect {
    Lit_data  parent;   // -- is signed if the parent has a signed wire pointing to the child
    uint      pin;      // -- bit31 is reserved and is always 0
};


//=================================================================================================
// -- Fanout array:


class Fanouts {
    Gig&        N;
    uint        sz;
    CConnect*   data;

    friend class GigObj_Fanouts;
    Fanouts(Gig& N_, CConnect* data_, uint sz_) : N(N_), sz(sz_), data(data_) {}

public:
    uint size() const { return sz; }
    Connect operator[](uint i) const { assert_debug(i < sz); return Connect(N[static_cast<GLit&>(data[i].parent)], data[i].pin); }
};


template<> fts_macro void write_(Out& out, const Fanouts& fo) {
    out += '{';
    for (uint i = 0; i < fo.size(); i++){
        if (i != 0) out += ',', ' ';
        out += fo[i];
    }
    out += '}';
}


//=================================================================================================
// -- GigObj_Fanouts:


class GigObj_Fanouts : public GigObj {
    struct Ext {
        uint size;      // -- bit31 is always 1
        uint offset;
    };

    union Outs {
        CConnect inl;
        Ext      ext;
    };

    CConnect*    mem;
    uind         mem_sz;
    Array<Outs>  data;

public:
  //________________________________________
  //  Constructor:

    GigObj_Fanouts(Gig& N_) :
        GigObj(N_),
        mem(NULL),
        mem_sz(0)
    {}

   ~GigObj_Fanouts() { clear(); }

  //________________________________________
  //  GigObj interface:

    void init();
    void load(In&) { init(); }
    void save(Out&) const {}
    void copyTo(GigObj& dst) const;
    void compact(const GigRemap& remap) { init(); } // -- it might be faster to actually use 'remap', but this is simpler...

  //________________________________________
  //  Methods:

    void clear() { // -- free all memory; you may call 'init()' to repopulate the fanout database
        xfree(mem); mem = NULL; mem_sz = 0; dispose(data); data.mkNull(); }

    Fanouts get(Wire w) const {
        Outs& f = data[id(w)];
        if (f.ext.size & 0x80000000) // -- external
            return Fanouts(*N, mem + f.ext.offset, f.ext.size & 0x7FFFFFFF);
        else // -- inlined
            return Fanouts(*N, &f.inl, uint(static_cast<GLit&>(f.inl.parent) != GLit_NULL));
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Dynamic fanout count:


struct GigObj_FanoutCount : GigObj, GigLis {
    WMap<uint> n_fanouts;

  //________________________________________
  //  Constructor:

    GigObj_FanoutCount(Gig& N_) :
        GigObj(N_)
    {
        N->listen(*this, msg_Update | msg_Remove);
    }

   ~GigObj_FanoutCount(){
        N->unlisten(*this, msg_Update | msg_Remove);
    }

  //________________________________________
  //  GigObj interface:

    void init();
    void load(In&) { init(); }
    void save(Out&) const {}
    void copyTo(GigObj& dst) const { n_fanouts.copyTo(static_cast<GigObj_FanoutCount&>(dst).n_fanouts); }
    void compact(const GigRemap& remap) { init(); }

  //________________________________________
  //  Listener interface:

    void updating(Wire w, uint pin, Wire w_old, Wire w_new) {
        if (w_old) n_fanouts(w_old)--;
        if (w_new) n_fanouts(w_new)++; }

    void removing(Wire w, bool) {
        For_Inputs(w, v) n_fanouts(v)--; }
};


inline void GigObj_FanoutCount::init()
{
    n_fanouts.clear();
    n_fanouts.reserve(N->size());
    For_Gates(*N, w){
        For_Inputs(w, v)
            n_fanouts(v)++;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
