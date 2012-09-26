//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BFunc.hh
//| Author(s)   : Niklas Een
//| Module      : BFunc
//| Description : Template based boolean function class.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__BFunc__BFunc_hh
#define ZZ__BFunc__BFunc_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Template class 'BFunc<n_vars>':


#define Max_BFunc_Size 16       // -- boolean functions with more than this number of variables are not supported

#define Apply_To_All_BFunc_Sizes(Macro) \
    Macro(0)  \
    Macro(1)  \
    Macro(2)  \
    Macro(3)  \
    Macro(4)  \
    Macro(5)  \
    Macro(6)  \
    Macro(7)  \
    Macro(8)  \
    Macro(9)  \
    Macro(10) \
    Macro(11) \
    Macro(12) \
    Macro(13) \
    Macro(14) \
    Macro(15) \
    Macro(16)


#define BFunc_Proj(neg, pin) ((((pin) == 0) ? 0xAAAAAAAAu : ((pin) == 1) ? 0xCCCCCCCCu : ((pin) == 2) ? 0xF0F0F0F0u : ((pin) == 3) ? 0xFF00FF00u : 0xFFFF0000u) ^ ((neg) ? 0xFFFFFFFFu : 0))

static const uint bfunc_proj[2][5] = {
    { 0xAAAAAAAA, 0xCCCCCCCC, 0xF0F0F0F0, 0xFF00FF00, 0xFFFF0000 },
    { 0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF }      // (negated)
};


template<uint n_vars_>
struct BFunc {
    enum { n_vars  = n_vars_ };
    enum { n_words = ((1 << n_vars) + 31) >> 5 };
    enum { used_mask = (n_vars < 5) ? (uint)((1u << ((1u << n_vars)&31)) - 1) : 0xFFFFFFFFu };
    uint ftb[n_words];

    uint nVars() const { return n_vars; }
    uint nWords() const { return n_words; }
    uint usedMask() const { return used_mask; }

    BFunc(Tag_internal) {}      // (don't initialize FTB)

    BFunc(bool v = false) {
        for (uint i = 0; i < n_words; i++)
            ftb[i] = v ? used_mask : 0;
    }

    BFunc(Lit p) {
        assert(p.id < n_vars);
        if (p.id < 5){
            uint mask = bfunc_proj[p.sign][p.id];
            for (uint i = 0; i < n_words; i++)
                ftb[i] = mask;
        }else{
            uint freq = 1u << (p.id - 5);
            uint mask = p.sign ? 0xFFFFFFFFu : 0;
            uint c = freq;
            for (uint i = 0; i < n_words; i++){
                ftb[i] = mask;
                c--;
                if (c == 0){
                    c = freq;
                    mask ^= 0xFFFFFFFFu;
                }
            }
        }
    }

    BFunc(const uint ftb_[n_words]) {
        for (uint i = 0; i < n_words; i++)
            ftb[i] = ftb_[i] & used_mask;
    }

    bool isFalse() const {
        if (n_words == 1)
            return (ftb[0] & used_mask) == 0;
        else{
            for (uint i = 0; i < n_words; i++)
                if (ftb[i] != 0) return false;
            return true;
        }
    }

    bool isTrue() const {
        if (n_words == 1)
            return (ftb[0] & used_mask) == used_mask;
        else{
            for (uint i = 0; i < n_words; i++)
                if (ftb[i] != used_mask) return false;
            return true;
        }
    }

    uint operator[](uint word_num) const { return ftb[word_num]; }

    bool bit(uint i) const { return (ftb[i >> 5] & (1 << (i & 31))) != 0; }
        // -- This is a slow way of reading the FTB, mainly for debugging.
};


template<uint n>
macro BFunc<n+1> operator+(const BFunc<n>& hi, const BFunc<n>& lo)
{
    BFunc<n+1> f(internal_);
    if (n < 5){
        f.ftb[0] = lo.ftb[0] | (hi.ftb[0] << ((1 << n)&31));
    }else{
        uint i = 0;
        for (; i < BFunc<n>::n_words; i++)
            f.ftb[i] = lo.ftb[i];
        for (uint j = 0; j < BFunc<n>::n_words; i++, j++)
            f.ftb[i] = hi.ftb[j];
    }
    return f;
}


template<uint n>
macro bool operator==(const BFunc<n>& f, const BFunc<n>& g)
{
    for (uint i = 0; i < BFunc<n>::n_words; i++)
        if (f.ftb[i] != g.ftb[i]) return false;
    return true;
}


template<uint n>
macro BFunc<n-1> lo(const BFunc<n>& src)
{
    BFunc<n-1> f(internal_);
    if (n <= 5){
        f.ftb[0] = src.ftb[0] & BFunc<n-1>::used_mask;
    }else{
        for (uint i = 0; i < BFunc<n-1>::n_words; i++)
            f.ftb[i] = src.ftb[i];
    }
    return f;
}


template<uint n>
macro BFunc<n-1> hi(const BFunc<n>& src)
{
    BFunc<n-1> f(internal_);
    if (n <= 5){
        f.ftb[0] = src.ftb[0] >> ((1 << (n-1))&31);
    }else{
        uint i = BFunc<n-1>::n_words;
        for (uint j = 0; j < BFunc<n-1>::n_words; i++, j++)
            f.ftb[j] = src.ftb[i];
    }
    return f;
}


template<uint n>
macro BFunc<n> operator&(const BFunc<n>& g, const BFunc<n>& h)
{
    BFunc<n> f(internal_);
    for (uint i = 0; i < BFunc<n>::n_words; i++)
        f.ftb[i] = g.ftb[i] & h.ftb[i];
    return f;
}


template<uint n>
macro BFunc<n> operator|(const BFunc<n>& g, const BFunc<n>& h)
{
    BFunc<n> f(internal_);
    for (uint i = 0; i < BFunc<n>::n_words; i++)
        f.ftb[i] = g.ftb[i] | h.ftb[i];
    return f;
}


template<uint n>
macro BFunc<n> operator^(const BFunc<n>& g, const BFunc<n>& h)
{
    BFunc<n> f(internal_);
    for (uint i = 0; i < BFunc<n>::n_words; i++)
        f.ftb[i] = g.ftb[i] ^ h.ftb[i];
    return f;
}


template<uint n>
macro BFunc<n> operator~(const BFunc<n>& g)
{
    BFunc<n> f(internal_);
    for (uint i = 0; i < BFunc<n>::n_words; i++)
        f.ftb[i] = g.ftb[i] ^ BFunc<n>::used_mask;
    return f;
}


template<uint n_vars>
fts_macro void write_(Out& out, const BFunc<n_vars>& v)
{
    FWrite(out) "<bfunc:%_ ", n_vars;
    for (uint i = (1u << n_vars); i > 0;){ i--;
        out += v.bit(i) ? '1' : '0';
        if ((i & 31) == 0 && i != 0) out += " : ";
        else if ((i & 7) == 0 && i != 0) out += ':';
    }
    out += '>';
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Irredundant sum-of-product:


template<uint n> BFunc<n> isop(const BFunc<n>& L, const BFunc<n>& U, uint acc, /*out*/Vec<uint>& cover);


template<>
fts_macro BFunc<0u> isop(const BFunc<0u>& L, const BFunc<0u>& U, uint acc, /*out*/Vec<uint>& cover)
{
    if (L.isFalse()) return BFunc<0>(false);
    else{ assert(U.isTrue()); cover.push(acc); return BFunc<0>(true); }
}


template<uint n>
BFunc<n> isop(const BFunc<n>& L, const BFunc<n>& U, uint acc, /*out*/Vec<uint>& cover)
{
    assert_debug((L | U) == U);
    assert_debug((L & U) == L);

    if (L.isFalse()) return BFunc<n>(false);
    if (U.isTrue()){ cover.push(acc); return BFunc<n>(true); }

    BFunc<n-1> L0 = lo(L), L1 = hi(L), U0 = lo(U), U1 = hi(U);

    uint pos_x = 1 << (2*(n-1));
    uint neg_x = pos_x << 1;
    BFunc<n-1> C0 = isop(L0 & ~U1, U0, acc | neg_x, cover);
    BFunc<n-1> C1 = isop(L1 & ~U0, U1, acc | pos_x, cover);
    BFunc<n-1> Ln = (L0 & ~C0) | (L1 & ~C1);
    BFunc<n-1> C_ = isop(Ln, U0 & U1, acc, cover);

    return (C1 | C_) + (C0 | C_);
}


// Produces an irredundant sum-of-products for up to 16 variables. A cover is represented
// as a vector of integers, where each bit of an integer corresponds to a literal:
// bit0 = x0, bit1 = ~x0, bit2 = x1, bit = ~x1 ...
//
template<uint n>
BFunc<n> isop(const BFunc<n>& F, /*out*/Vec<uint>& cover)
{
    return isop(F, F, 0, cover);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
