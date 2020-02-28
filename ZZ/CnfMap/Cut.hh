//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cut.hh
//| Author(s)   : Niklas Een
//| Module      : CnfMap
//| Description : Represents small cuts (size <= 4). 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__CnfMap__Cut_hh
#define ZZ__CnfMap__Cut_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Cut' class:


const struct Tag_cut_empty { Tag_cut_empty(){} } cut_empty_;


struct Cut {    // -- this class represents 4-input cuts.
protected:
    void absExt(GLit p) { abstr |= 1u << (p.id & 31); }

    GLit    inputs[4];
    ushort  sz;
public:
    ushort  ftb;
    uint    abstr;

    Cut(Tag_cut_empty, bool const_true) : sz(0), ftb(const_true ? 0xFFFF : 0x0000), abstr(0) {}
    Cut(GLit p) : sz(1), abstr(0) { inputs[0] = +p; ftb = p.sign ? 0x5555 : 0xAAAA; absExt(p); }
    Cut() : sz(32768) {}

    uint   size()              const { return sz; }
    GLit   operator[](int idx) const { return inputs[idx]; }
    void   push(GLit p)              { inputs[sz++ & 3] = +p; absExt(p); }
    bool   null()              const { return uint(sz) > 4; }
    void   mkNull()                  { sz = 32768; }
    void   mkEmpty(bool const_true)  { sz = 0; ftb = (const_true ? 0xFFFF : 0x0000); abstr = 0; }

    void   trim();        // -- Remove inputs not in the semantic support.
};


//static const Cut Cut_NULL;        // -- It is sooooo sad that this line leads to a significant performance loss!
#define Cut_NULL Cut()


//=================================================================================================
// -- Support functions:


macro bool operator==(const Cut& c0, const Cut& c1)
{
    if (c0.size() != c1.size()) return false;
    for (uint i = 0; i < c0.size(); i++)
        if (c0[i] != c1[i]) return false;
    return true;
}


template<> fts_macro void write_(Out& out, const Cut& c)
{
    if (c.null())
        out += "<null>";
    else{
        out += '{';
        for (uint i = 0; i < c.size(); i++){
            if (i != 0) out += ',', ' ';
            out += c[i];
        }
        out += '}';
        FWrite(out) ":%.4X", c.ftb;
        //FWrite(out) "^%.16:b", c.abstr;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper functions:


macro bool moreThanFourBits(uint a)
{
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    return a;
}


// Check if cut 'c' is a subset of cut 'd'. Cuts must be sorted. FTB is ignored.
macro bool subsumes(const Cut& c, const Cut& d)
{
    assert_debug(!c.null());
    assert_debug(!d.null());

    if (d.size() < c.size())
        return false;

    if (c.abstr & ~d.abstr)
        return false;

    if (c.size() == d.size()){
        for (uint i = 0; i < c.size(); i++)
            if (c[i] != d[i])
                return false;
    }else{
        uint j = 0;
        for (uint i = 0; i < c.size(); i++){
            while (c[i] != d[j]){
                j++;
                if (j == d.size())
                    return false;
            }
        }
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut enumeration:


Cut combineCuts_And(const Cut& cut1, const Cut& cut2, bool inv1, bool inv2);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
