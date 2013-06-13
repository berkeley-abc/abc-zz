//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cut.hh
//| Author(s)   : Niklas Een
//| Module      : LutMap
//| Description : Cut representation for LUT mapper.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__LutMap__Cut_hh
#define ZZ__LutMap__Cut_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class LutMap_Cut {                // -- this class represents 6-input cuts.
    void extendAbstr(gate_id g) { abstr |= 1u << (g & 31); }

    gate_id inputs[6];
    uint    sz;
public:
    uint    abstr;

    LutMap_Cut(Tag_empty) : sz(0), abstr(0) {}
    LutMap_Cut(gate_id g) : sz(1), abstr(0) { inputs[0] = g; extendAbstr(g); }
    LutMap_Cut()          : sz(7)           {}

    uint    size()                const { return sz; }
    gate_id operator[](int index) const { return inputs[index]; }
    bool    null()                const { return uint(sz) > 6; }
    void    mkNull()                    { sz = 7; }

    void    push(gate_id g) { if (!null()){ inputs[sz++] = g; extendAbstr(g); } }

    bool    operator==(const LutMap_Cut& other) const;
};


inline bool LutMap_Cut::operator==(const LutMap_Cut& other) const
{
    if (abstr != other.abstr) return false;
    if (sz != other.sz) return false;
    for (uint i = 0; i < sz; i++)
        if (inputs[i] != other.inputs[i]) return false;
    return true;
}


template<> fts_macro void write_(Out& out, const LutMap_Cut& v)
{
    if (v.null())
        FWrite(out) "<null>";
    else{
        out += '{';
        if (v.size() > 0){
            out += v[0];
            for (uint i = 1; i < v.size(); i++)
                out += ',', ' ', v[i];
        }
        out += '}';
    }
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


// Check if cut 'c' is a subset of cut 'd'. Cuts must be sorted. FTB is ignored.
macro bool subsumes(const LutMap_Cut& c, const LutMap_Cut& d)
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


macro bool moreThanSixBits(uint a)
{
  #if defined(__GNUC__)
    return __builtin_popcount(a) > 6;
  #else
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    a &= a - 1;
    return a;
  #endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
