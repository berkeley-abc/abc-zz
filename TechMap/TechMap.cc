//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TechMap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description :
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
/*


- Each node stores multiple types of cuts:
  - Several cuts on a trade-off between area/delay
  - Cuts that corresponds to different points in target architecture (FMUX7, inverted output in standard cell etc.)

- FTB computation and semantic cuts/constant propagation.

- Native BigAnd/BigXor with dynamic internal points discovery?

- More compact cut-set representation


Cut
Organized CutSet

CutMap

struct CutSet
F7s, F8s


*/
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cuts:


/* 
Cut layout:

sig    : 1 uint
size   : 1 uint
inputs : 'size' uints
pad    : 0 or 1 uint (to get to 64-bit align)
ftb    : '2^size / 64' uint64s, rounded up.
*/

class TechMap_Cut {
    typedef StackAlloc<uint64> Alloc;

    uint64* base;

    TechMap_Cut& me() const { return *const_cast<TechMap_Cut*>(this); }

    uint&   sig_()         { return ((uint*)&base[0])[0]; }
    uint&   size_()        { return ((uint*)&base[0])[1]; }
    uint&   input_(uint i) { return ((uint*)&base[1])[i]; }
    uint64& ftb_(uint i)   { return base[(size_() + 3) >> 1]; }

    void init(Array<gate_id> inputs, Array<uint64> ftb, TechMap_Cut::Alloc& mem);

public:
    TechMap_Cut() : base(NULL) {}
    TechMap_Cut(Array<gate_id> inputs, Array<uint64> ftb, Alloc& mem) { init(inputs, ftb, mem); }
    TechMap_Cut(Array<gate_id> inputs, uint64        ftb, Alloc& mem) { uint64* f = &ftb; init(inputs, slice(f[0], f[1]), mem); }

    Null_Method(TechMap_Cut) { return base == NULL; }
    uint sig()                        const { return me().sig_(); }
    uint size()                       const { return me().size_(); }
    gate_id operator[](int input_num) const { return me().input_(input_num); }
    uint64 ftb(uint word_num = 0)     const { return me().ftb_(word_num); }
};


void TechMap_Cut::init(Array<gate_id> inputs, Array<uint64> ftb, TechMap_Cut::Alloc& mem)
{
    base = mem.alloc(((inputs.size() + 3) >> 1) + ftb.size());

    uint sig_mask = 0;
    for (uint i = 0; i < inputs.size(); i++)
        sig_mask |= 1ull << (inputs[i] & 31);
    sig_() = sig_mask;

    size_() = inputs.size();
    for (uint i = 0; i < inputs.size(); i++)
        input_(i) = inputs[i];

    for (uint i = 0; i < ftb.size(); i++)
        ftb_(i) = ftb[i];

  #if 0
    WriteLn "sig:    %_", (uint*)&sig_() - (uint*)base;
    WriteLn "size:   %_", (uint*)&size_() - (uint*)base;
    WriteLn "inputs: %_", (uint*)&input_(0) - (uint*)base;
    WriteLn "ftb:    %_", (uint*)&ftb_(0) - (uint*)base;

    NewLine;
    WriteLn "alloc:  %_", 2 * (((inputs.size() + 3) >> 1) + ftb.size());
  #endif
}


//=================================================================================================
// -- helper functions:


// Check if the support of 'c' is a subset of the support of 'd'. Does NOT assume cuts to be 
// sorted. The FTB is not used.
macro bool subsumes(const TechMap_Cut& c, const TechMap_Cut& d)
{
    assert_debug(c);
    assert_debug(d);

    if (d.size() < c.size())
        return false;

    if (c.sig() & ~d.sig())
        return false;

    for (uint i = 0; i < c.size(); i++){
        if (!has(d, c[i]))
            return false;
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Testing:

void test()
{
    Vec<gate_id> inputs;
    inputs += 1, 2, 3, 35;

    Vec<uint64> ftb;
    ftb += 0xABBA;

    StackAlloc<uint64> mem;

    TechMap_Cut c(inputs.slice(), ftb.slice(), mem);
    inputs.pop();
    inputs.push(3);
    TechMap_Cut d(inputs.slice(), ftb.slice(), mem);

    Dump(c.sig(), d.sig());
    WriteLn "%_", subsumes(c, d);
    WriteLn "%_", subsumes(d, c);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
