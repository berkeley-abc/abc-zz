//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Unmap.hh
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Expand LUTs back to GIG.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__TechMap__Unmap_hh
#define ZZ__TechMap__Unmap_hh

#include "ZZ_Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_Unmap {
    bool shuffle;           // -- shuffle the inputs of a k-input AND/XOR before combining them 
    bool try_share;         // -- prefer combining nodes "f*g" that already exists in the netlist
    bool balanced;          // -- prefer combining original nodes before combined nodes.
    bool depth_aware;       // -- only consider depth-optimal alternatives when combining nodes

    void setOptions(uint v) {
        shuffle     = v & 1;
        try_share   = v & 2;
        balanced    = v & 4;
        depth_aware = v & 8;
    }

    uint getOptions() const {
        return (uint)shuffle
             | ((uint)try_share   << 1)
             | ((uint)balanced    << 2)
             | ((uint)depth_aware << 3);
    }

    Params_Unmap() :
        shuffle(false),
        try_share(true),
        balanced(true),
        depth_aware(true)
    {}
};


void unmap(Gig& N, WMapX<GLit>* remap = NULL, const Params_Unmap& P = Params_Unmap());


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
