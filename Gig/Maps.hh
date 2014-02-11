//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Maps.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Maps from 'Wire' to 'T' and related types.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//| The 'clear()' methods of the various WMap:s leave the 'N' netlist and 'nil' null-value intact.
//| The same holds for 'moveTo()' which leaves the source map in the same state as after a
//| call to 'clear()'.
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__Maps_hh
#define ZZ__Gig__Maps_hh

#include "ZZ/Generics/IntMap.hh"
#include "Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Lit-to-Lit map template parameter:


template<class Key_, class Value_>
struct LLMapNorm {
    typedef Key_   Key;
    typedef Value_ Value;
    typedef const Value_ RetValue;

    Value get(Key key, Value val) const { return val ^ sign(key); }
    bool isNormal(Key key) const { return !sign(key); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Standard Wire Map:


// A 'WMap' ignores the sign bit of the input wire ('w' and '~w' are treated identically).

template<class V>
class WMap : public NonCopyable {
    IntMap<GLit, V, MkIndex_Lit<false> > map;

public:
    typedef GLit Key;
    typedef V Value;

    const Gig* N;

    WMap()                     :           N((Gig*)NULL) {}
    WMap(V nil)                : map(nil), N((Gig*)NULL) {}
    WMap(const Gig& N_)        :           N(&N_)  { map.reserve(N->size()); }
    WMap(const Gig& N_, V nil) : map(nil), N(&N_)  { map.reserve(N->size()); }

    V  operator[](GLit p) const { return map[p]; }
    V& operator()(GLit p)       { return map(p); }
    V  operator[](Wire w) const { assert_debug(!N || w.gig() == N); return map[w.lit()]; }
    V& operator()(Wire w)       { assert_debug(!N || w.gig() == N); return map(w.lit()); }

    void reserve(uint cap)           { map.reserve(cap); }
    void clear(bool dispose = false) { map.clear(dispose); }

    void copyTo(WMap& dst) const { map.copyTo(dst.map); dst.N = N; }
    void moveTo(WMap& dst)       { map.moveTo(dst.map); dst.N = N; }

    Vec<V>&       base()       { return map.base(); }   // -- low-level
    const Vec<V>& base() const { return map.base(); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Signed Wire Map:


// A 'WMapS' adds the sign bit of the input wire to the underlying index, mapping 'w' and '~w'
// to different values.

template<class V>
class WMapS : public NonCopyable {
    IntMap<GLit, V, MkIndex_Lit<true> > map;

public:
    typedef GLit Key;
    typedef V Value;

    const Gig* N;

    WMapS()                     :           N(NULL) {}
    WMapS(V nil)                : map(nil), N(NULL) {}
    WMapS(const Gig& N_)        :           N(&N_)  { map.reserve(N->size() * 2); }
    WMapS(const Gig& N_, V nil) : map(nil), N(&N_)  { map.reserve(N->size() * 2); }

    V  operator[](GLit p) const { return map[p]; }
    V& operator()(GLit p)       { return map(p); }
    V  operator[](Wire w) const { assert_debug(!N || w.gig() == N); return map[w.lit()]; }
    V& operator()(Wire w)       { assert_debug(!N || w.gig() == N); return map(w.lit()); }

    void reserve(uint cap)           { map.reserve(cap); }
    void clear(bool dispose = false) { map.clear(dispose); }

    void copyTo(WMapS& dst) const { map.copyTo(dst.map); dst.N = N; }
    void moveTo(WMapS& dst)       { map.moveTo(dst.map); dst.N = N; }

    Vec<V>&       base()       { return map.base(); }   // -- low-level
    const Vec<V>& base() const { return map.base(); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Transfer-sign Wire Map:


// A 'WMapX' requires 'V' to be of literal type. When using 'operator[]', the sign is transferred
// from the key to the value. For 'operator()' it is illegal to use signed wires.

template<class V>
class WMapX : public NonCopyable {
    IntMap<GLit, V, MkIndex_Lit<false>, LLMapNorm<GLit,V> > map;

public:
    typedef GLit Key;
    typedef V Value;

    const Gig* N;

    WMapX()                     :           N(NULL) {}
    WMapX(V nil)                : map(nil), N(NULL) {}
    WMapX(const Gig& N_)        :           N(&N_)  { map.reserve(N->size()); }
    WMapX(const Gig& N_, V nil) : map(nil), N(&N_)  { map.reserve(N->size()); }

    void initBuiltins() { for (uint i = 0; i < gid_FirstUser; i++) map(GLit(i)) = GLit(i); }
        // -- if mapping between two netlists, use this to set up map between builtin
        // gates (constants, Wire_NULL etc.).

    V  operator[](GLit p) const { return map[p]; }
    V& operator()(GLit p)       { return map(p); }
    V  operator[](Wire w) const { assert_debug(!N || w.gig() == N); return map[w.lit()]; }
    V& operator()(Wire w)       { assert_debug(!N || w.gig() == N); return map(w.lit()); }

    void reserve(uint cap)           { map.reserve(cap); }
    void clear(bool dispose = false) { map.clear(dispose); }

    void copyTo(WMapX& dst) const { map.copyTo(dst.map); dst.N = N; }
    void moveTo(WMapX& dst)       { map.moveTo(dst.map); dst.N = N; }

    // Special move-to:
    void moveTo(GigRemap& dst) { map.base().moveTo(dst.new_lit); }

    Vec<V>&       base()       { return map.base(); }   // -- low-level
    const Vec<V>& base() const { return map.base(); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Number-attribute Wire Map:


// A 'WMapN' uses the 'number' attribute of the gate as index. Keys must be restricted to a
// single gate type. The sign of the wire is ignored.

template<class V>
class WMapN : public NonCopyable {
    Vec<V> data;
    V      nil;

    V& update_(True_t , uint idx) { return data(idx, nil); }
    V& update_(False_t, uint idx) { return data(idx); }

public:
    typedef GLit Key;
    typedef V Value;

    const Gig* N;
    GateType type;

    WMapN()                                  :            N(NULL), type(gate_NULL) {}
    WMapN(V nil_)                            : nil(nil_), N(NULL), type(gate_NULL) {}
    WMapN(const Gig& N_)                     :            N(&N_) , type(gate_NULL) {}
    WMapN(const Gig& N_, V nil_)             : nil(nil_), N(&N_) , type(gate_NULL) {}
    WMapN(GateType t)                        :            N(NULL), type(t)         {}
    WMapN(GateType t, V nil_)                : nil(nil_), N(NULL), type(t)         {}
    WMapN(const Gig& N_, GateType t)         :            N(&N_) , type(t)         { assert(isNumbered(t)); data.growTo(N->numbers[type].size()); }
    WMapN(const Gig& N_, GateType t, V nil_) : nil(nil_), N(&N_) , type(t)         { assert(isNumbered(t)); data.growTo(N->numbers[type].size(), nil); }

    V  operator[](GLit p) const;
    V& operator()(GLit p);
    V  operator[](Wire w) const;
    V& operator()(Wire w);

    void reserve(uint cap)           { data.growTo(cap, nil); }
    void clear(bool dispose = false) { data.clear(dispose); }

    void copyTo(WMapN& dst) const { data.copyTo(dst.data); dst.nil = nil; dst.N = N; }
    void moveTo(WMapN& dst)       { data.moveTo(dst.data); dst.nil = nil; dst.N = N; }

    Vec<V>&       base()       { return data; }   // -- low-level
    const Vec<V>& base() const { return data; }
};


template<class V>
inline V WMapN<V>::operator[](GLit p) const
{
    assert_debug(N);
    Wire w = p + *N;
    assert_debug(type == gate_NULL || w.type() == type);
    uint idx = w.num();
    return (idx >= data.size()) ? nil : data[idx];
}


template<class V>
inline V& WMapN<V>::operator()(GLit p)
{
    assert_debug(N);
    Wire w = p + *N;
    assert_debug(type == gate_NULL || w.type() == type);
    return update_(typename IsCopyable<Value>::Result(), w.num());
}


template<class V>
inline V WMapN<V>::operator[](Wire w) const
{
    assert_debug(!N || w.gig() == N);
    assert_debug(type == gate_NULL || w.type() == type);
    uint idx = w.num();
    return (idx >= data.size()) ? nil : data[idx];
}


template<class V>
inline V& WMapN<V>::operator()(Wire w)
{
    assert_debug(!N || w.gig() == N);
    assert_debug(type == gate_NULL || w.type() == type);
    return update_(typename IsCopyable<Value>::Result(), w.num());
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wire Sets:


template<bool use_sign>
class WZet_ : public NonCopyable {
    IntZet<GLit, MkIndex_Lit<use_sign> > set;

public:
    typedef GLit Key;

    const Gig* N;

    WZet_()              : N(NULL)      {}
    WZet_(const Gig& N_) : N(&N_)       { reserve(use_sign ? N->size() * 2 : N->size()); }

    void reserve(uint cap)              { set.reserve(cap); }
    uint size   () const                { return set.size(); }
    void clear  (bool dispose = false)  { set.clear(dispose); }

    void copyTo (WZet_& copy) const     { set.copyTo(copy.set); }
    void moveTo (WZet_& dest)           { set.moveTo(dest.set); }

    bool add    (GLit p)                { return set.add(p); }
    bool add    (Wire w)                { assert_debug(!N || w.gig() == N); return set.add(w.lit()); }
    bool has    (GLit p) const          { return set.has(p); }
    bool has    (Wire w) const          { assert_debug(!N || w.gig() == N); return set.has(w.lit()); }
    bool exclude(GLit p)                { return set.exclude(p); }
    bool exclude(Wire w)                { assert_debug(!N || w.gig() == N); return set.exclude(w.lit()); }

    GLit peekLast()                     { return set.peekLast(); }
    void popLast ()                     { return set.popLast(); }
    GLit popLastC()                     { return set.popLastC(); }

    Vec<GLit>&       list()             { return set.list(); }
    const Vec<GLit>& list() const       { return set.list(); }
    void compact()                      { set.compact(); }

    // Vector interface:
    bool push(GLit p) { return add(p); }
    bool push(Wire w) { return add(w); }
    GLit operator[](uint i) const { return set.list()[i]; }
};


typedef WZet_<false> WZet;
typedef WZet_<true>  WZetS;


// NOTE! 'N' must be set in the 'WZet' for this macro to work.
#define For_WZet(wset, w)                                           \
    if (Wire w = Wire_NULL); else                                   \
    for (uint i__##w = 0; i__##w < (wset).list().size(); i__##w++)  \
        if (!(wset).has((wset).list()[i__##w])); else               \
        if (w = (*(wset).N)[(wset).list()[i__##w]], false); else


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wire Seen (bit-masks):


template<bool use_sign>
class WSeen_ : public NonCopyable {
    IntSeen<GLit, MkIndex_Lit<use_sign> > set;

public:
    typedef GLit Key;

    const Gig* N;

    WSeen_()              : N(NULL)     {}
    WSeen_(const Gig& N_) : N(&N_)      { reserve(use_sign ? N->size() * 2 : N->size()); }

    void reserve(uint cap)              { set.reserve(cap); }
    void clear  (bool dispose = false)  { set.clear(dispose); }

    void copyTo (WSeen_& copy) const    { set.copyTo(copy.set); }
    void moveTo (WSeen_& dest)          { set.moveTo(dest.set); }

    bool add    (GLit p)                { return set.add(p); }
    bool add    (Wire w)                { assert_debug(!N || w.gig() == N); return set.add(w.lit()); }
    bool has    (GLit p) const          { return set.has(p); }
    bool has    (Wire w) const          { assert_debug(!N || w.gig() == N); return set.has(w.lit()); }
    bool exclude(GLit p)                { return set.exclude(p); }
    bool exclude(Wire w)                { assert_debug(!N || w.gig() == N); return set.exclude(w.lit()); }
};


typedef WSeen_<false> WSeen;
typedef WSeen_<true>  WSeenS;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
