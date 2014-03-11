//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Lit.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : A literal is a pair (id : uint, sign : bool).
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Literals have fields:
//|
//|   id   -- a 31-bit unsigned integer
//|   sign -- a 1-bit bool
//|
//| The underlying 32-bit representation can be retrieved by method 'data()'.
//| NOTE! Sign is stored in bit 0, so 'x' and '~x' are adjacent after sorting.
//|
//| Constants:
//|
//|   id_MAX  -- Maximum value for 'id'.
//|   Lit_MAX -- A literal of that ID.
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__Lit_hh
#define ZZ__Generics__Lit_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Literal datatype:


#if defined(sun) && defined(sparc)
struct Lit_data {
    uint id   : 31;
    uint sign : 1;
};
#else
struct Lit_data {
    uint sign : 1;           // -- Visual Studio can't handle a 'bool' here...
    uint id   : 31;
};
#endif

union Lit_union { Lit_data sid; uint32 data; };

struct Lit : Lit_data {
    explicit Lit(uint id_, bool sign_ = false) { sign = sign_; id = id_; }
    Lit() { Lit_union u; u.data = 0; *this = static_cast<Lit&>(u.sid); }  // -- make sure 'id' and 'sign' are set to zero with just one operation
    Lit(const Lit& other) { sign = other.sign; id = other.id; }
    Lit(Tag_packed, uint32 data) { Lit_union u; u.data = data; *this = static_cast<Lit&>(u.sid); }

    Lit operator~() const { return Lit(id, !sign); }
    Lit operator+() const { return Lit(id); }
    Lit operator^(bool s) const { return Lit(id, sign ^ s); }

    bool operator==(Lit g) const {
        Lit_union x; x.sid = *this;
        Lit_union y; y.sid = g;
        return x.data == y.data; }

    bool operator<(Lit g) const {
        Lit_union x; x.sid = *this;
        Lit_union y; y.sid = g;
        return x.data < y.data; }

    uint32 data() const { Lit_union u; u.sid = *this; return u.data; }

    Null_Method(Lit){
        Lit_union x; x.sid = *this;
        return x.data == 0; }
};

macro bool sign(Lit p) { return p.sign; }
macro uint id  (Lit p) { return p.id;   }

macro void swp(Lit& p, Lit& q){ Lit tmp = p; p = q; q = tmp; }


#if defined(ZZ_CONSTANTS_AS_MACROS)
    #define id_MAX   0x7FFFFFFFu
    #define Lit_MAX  Lit(id_MAX, false)
    #define Lit_NULL Lit()
#else
    static const uint id_MAX  = 0x7FFFFFFFu;
    static const Lit  Lit_MAX = Lit(id_MAX, false);
    static const Lit  Lit_NULL;
#endif


template<> fts_macro uint64 hash_<Lit>(const Lit& sid) {
    Lit_union x;
    x.sid = sid;
    return x.data; }


template <bool sign_matters>
struct MkIndex_Lit {
    typedef Lit Key;
    uind operator()(Lit p) const {
        // -- If sign doesn't matter, just use ID. With sign, use 'ID*2 + sign' to get the order: x0, ~x0, x1, ~x1, ...
        return sign_matters ? p.data() : p.id; }
};


template<> fts_macro void write_(Out& out, const Lit& p)
{
    if (p.sign)
        out += '~';
    out += 'x';
    if (p.id == id_MAX) out += "MAX";
    else                out += p.id;
}


template<> fts_macro void write_(Out& out, const Lit& p, Str flags)
{
    assert(flags.size() == 1);

    if (p.sign)
        out += '~';
    out += flags[0];
    if (p.id == id_MAX) out += "MAX";
    else                out += p.id;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
