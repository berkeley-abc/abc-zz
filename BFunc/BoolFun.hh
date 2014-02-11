//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BoolFun.cc
//| Author(s)   : Niklas Een
//| Module      : BFunc
//| Description : Represent functions using truth-tables.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Essentially a non-template based version of "BFunc" for representing functions up to 16 
//| variables or so.
//|________________________________________________________________________________________________

#ifndef ZZ__BFunc__BoolFun_hh
#define ZZ__BFunc__BoolFun_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global tables:


extern const uint64 bool_fun_proj[2][6];


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// BoolFun:


// NOTE! For smaller functions than 6 variables, the entire 64-bit word of the single FTB element
// is used, and the FTB is initialized in exactly the same way as if the function had 6 variables.
// The only noticable difference is the return value of 'nVars()'. This is NOT the way 'BFunc' 
// works.
//
class BoolFun {
    uint n_vars;
    uint sz;                // -- size of FTB (in uint64:s)
    union {
        uint64* ext_data;   // -- FTB data
        uint64  inl_data;
    };

  //________________________________________
  //  Internal helpers:

    void dispose();

public:
  //________________________________________
  //  Constructors:

    void init(uint n_vars_);    // -- you may call this directly, but note that FTB data is uninitialized!

    BoolFun() : n_vars(UINT_MAX), sz(0) {}
    Null_Method(BoolFun) { return sz == 0; }

    BoolFun(uint n_vars_, bool val);
    BoolFun(uint n_vars_, Lit  p);

    BoolFun(const BoolFun& src);
    BoolFun& operator=(const BoolFun& src);

   ~BoolFun() { dispose(); }

    void moveTo(BoolFun& dst) { dst.n_vars = n_vars; dst.sz = sz; dst.inl_data = inl_data; sz = 0; n_vars = UINT_MAX; }

  //________________________________________
  //  Boolean operations:

    BoolFun& operator&=(const BoolFun& op);
    BoolFun& operator|=(const BoolFun& op);
    BoolFun& operator^=(const BoolFun& op);
    BoolFun& inv();

    BoolFun operator&(const BoolFun& op) const;
    BoolFun operator|(const BoolFun& op) const;
    BoolFun operator^(const BoolFun& op) const;
    BoolFun operator~() const;

    bool operator==(const BoolFun& op) const;
    bool operator< (const BoolFun& op) const;   // -- sorts first on 'n_vars', then on FTB.

    bool isFalse() const;
    bool isTrue () const;

  //________________________________________
  //  Selectors:

    uint nVars() const { return n_vars; }
    uint size () const { return sz; }       // -- number of 64-bit words representing FTB

    uint64*       data()       { return (sz == 1) ? &inl_data : ext_data; }
    const uint64* data() const { return (sz == 1) ? &inl_data : ext_data; }
        // -- get pointer to FTB; use 'size()' to get bound

    Array<uint64>       ftb()       { return Array<uint64>      (data(), sz); }
    Array<const uint64> ftb() const { return Array<const uint64>(data(), sz); }
        // -- return FTB as one entity: size + pointer to data

    uint64  operator[](uint i) const { assert_debug(i < sz); return data()[i]; }
    uint64& operator[](uint i)       { assert_debug(i < sz); return data()[i]; }
        // -- access a single word; less efficient but more convenient; probably good enough 
        // for most applications

    bool bit(uint i) const { assert_debug(i < (1ull << n_vars)); return (data()[i >> 6] & (1ull << (i & 63))) != 0; }
        // -- access a single bit; even less efficient, but good for printing and debugging.
};


//=================================================================================================
// -- Constructors:


inline void BoolFun::init(uint n_vars_)
{
    n_vars = n_vars_;
    if (n_vars <= 6)
        sz = 1;
    else{
        assert_debug(n_vars < 32);      // -- it is unreasonable to go this high anyway
        sz = 1u << (n_vars - 6);
        ext_data = xmalloc<uint64>(sz);
    }
}


inline void BoolFun::dispose()
{
    if (sz > 1)
        xfree(ext_data);
    sz = 0;
}


inline BoolFun::BoolFun(uint n_vars_, bool val)
{
    init(n_vars_);
    uint64* d = data();
    uint64  v = val ? 0xFFFFFFFFFFFFFFFFull : 0ull;

    for (uint i = 0; i < sz; i++)
        d[i] = v;
}


inline BoolFun::BoolFun(uint n_vars_, Lit p)
{
    assert(p.id < n_vars_);

    init(n_vars_);
    uint64* d = data();

    if (p.id < 6){
        uint64 v = bool_fun_proj[p.sign][p.id];
        for (uint i = 0; i < sz; i++)
            d[i] = v;
    }else{
        uint   freq = 1u << (p.id - 6);
        uint64 v = p.sign ? 0xFFFFFFFFFFFFFFFFull : 0ull;
        uint   c = freq;
        for (uint i = 0; i < sz; i++){
            d[i] = v;
            c--;
            if (c == 0){
                c = freq;
                v ^= 0xFFFFFFFFFFFFFFFFull;
            }
        }
    }
}


inline BoolFun::BoolFun(const BoolFun& src)
{
    init(src.n_vars);
    uint64* d = data();

    const uint64* src_d = src.data();
    for (uint i = 0; i < sz; i++)
        d[i] = src_d[i];
}


inline BoolFun& BoolFun::operator=(const BoolFun& src)
{
    if (!src){
        dispose();
        return *this; }

    if (n_vars != src.n_vars){
        dispose();
        init(src.n_vars);
        assert(sz == src.sz);
    }

    uint64* d = data();
    const uint64* src_d = src.data();
    for (uint i = 0; i < sz; i++)
        d[i] = src_d[i];

    return *this;
}


//=================================================================================================
// Boolean operations:


inline BoolFun& BoolFun::operator&=(const BoolFun& op)
{
    assert(n_vars == op.n_vars);

    uint64* d = data();
    const uint64* op_d = op.data();
    for (uint i = 0; i < sz; i++)
        d[i] &= op_d[i];

    return *this;
}


inline BoolFun& BoolFun::operator|=(const BoolFun& op)
{
    assert(n_vars == op.n_vars);

    uint64* d = data();
    const uint64* op_d = op.data();
    for (uint i = 0; i < sz; i++)
        d[i] |= op_d[i];

    return *this;
}


inline BoolFun& BoolFun::operator^=(const BoolFun& op)
{
    assert(n_vars == op.n_vars);

    uint64* d = data();
    const uint64* op_d = op.data();
    for (uint i = 0; i < sz; i++)
        d[i] ^= op_d[i];

    return *this;
}


inline BoolFun& BoolFun::inv()
{
    uint64* d = data();
    for (uint i = 0; i < sz; i++)
        d[i] = ~d[i];

    return *this;
}


inline BoolFun BoolFun::operator&(const BoolFun& op) const {
    BoolFun ret(*this);
    ret &= op;
    return ret; }


inline BoolFun BoolFun::operator|(const BoolFun& op) const {
    BoolFun ret(*this);
    ret |= op;
    return ret; }



inline BoolFun BoolFun::operator^(const BoolFun& op) const {
    BoolFun ret(*this);
    ret ^= op;
    return ret; }


inline BoolFun BoolFun::operator~() const {
    BoolFun ret(*this);
    ret.inv();
    return ret; }


inline bool BoolFun::operator==(const BoolFun& op) const
{
    assert(n_vars == op.n_vars);

    const uint64* d    = data();
    const uint64* op_d = op.data();
    for (uint i = 0; i < sz; i++)
        if (d[i] != op_d[i])
            return false;

    return true;
}


inline bool BoolFun::operator<(const BoolFun& op) const
{
    if (n_vars < op.n_vars) return true;
    if (n_vars > op.n_vars) return false;

    const uint64* d    = data();
    const uint64* op_d = op.data();
    for (uint i = sz; i > 0;){ i--;
        if (d[i] < op_d[i]) return true;
        if (d[i] > op_d[i]) return false;
    }
    return false;
}


//=================================================================================================
// -- Pretty-printing:


void write_BoolFun(Out& out, const BoolFun& v);

template<> fts_macro void write_(Out& out, const BoolFun& v) {
    write_BoolFun(out, v); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
