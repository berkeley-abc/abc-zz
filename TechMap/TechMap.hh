//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TechMap.hh
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Second generation technology mapper.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Currently targeted at FPGA mapping.
//|________________________________________________________________________________________________

#ifndef ZZ__TechMap__TechMap_hh
#define ZZ__TechMap__TechMap_hh

#include "ZZ_Gig.hh"
#include "Unmap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// These are the gate types the mapper can handle.
macro bool isTechmapLogic(Wire w) {
    uint64 mask = GTM_(And) | GTM_(Xor) | GTM_(Mux) | GTM_(Maj) | GTM_(One) | GTM_(Gamb) | GTM_(Dot) | GTM_(Lut4);
    return (1ull << w.type()) & mask;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_TechMap {
    uint        cut_size;           // -- maximum cut size
    uint        n_iters;            // -- refinement iterations
    uint        recycle_iter;       // -- start recycling cuts from this iteration (faster, but worse quality)
    uint        cuts_per_node;      // -- how many cuts to store for each node in the subject graph
    float       delay_factor;       // -- how far from delay optimal are we willing to go for saving area? (1.0 = delay optimal)
    float       delay_fraction;     // -- delay value in 'gate_Delay' are multiplied by this value
    bool        struct_mapping;     // -- if TRUE, FTBs are not used to reduce cuts (= mapping with structural cuts)
    bool        unmap_to_ands;      // -- if TRUE, XIG gates (XOR, MUX etc.) are turned into ANDs after unmapping.
    bool        use_fmux;           // -- enable F7/F8 MUXes for Xilinx series 7.
    bool        fmux_feeds_seq;     // -- if FALSE, F7/F8 MUXes cannot feed a 'gate_Seq' (which would correspond to a FF, sharing the same resource).
    float       lut_cost[7];        // -- Cost of LUTs of different sizes.
    float       mux_cost;           // -- Cost of F7/F8 MUXes.
    float       slack_util;         // -- How much slack to utilize in non-critical regions (small number means better average slack but worse area)
    bool        exact_local_area;   // -- Post-optimize induced mapping by peep-hole optimization. [no F7Mux support yet]
    bool        refactor;           // -- refactor big-ANDs and big-XORs.
    Params_Unmap unmap;             // -- options for unmapping
    float       est_power;          // -- Exponent to use for fanout estimate blending.
    float       est_const;          // -- Constant to use for fanout estimate blending.
    bool        batch_output;       // -- print a one-line summary at the end of techmapping which can be used to produce tables
    bool        quiet;              // -- suppress print-outs

    Params_TechMap() :
        cut_size        (6),
        n_iters         (5),
        recycle_iter    (3),
        cuts_per_node   (8),
        delay_factor    (1.0),
        delay_fraction  (1.0),
        struct_mapping  (false),
        unmap_to_ands   (false),
        use_fmux        (false),
        fmux_feeds_seq  (false),
        mux_cost        (1),                        // -- selector signal costs one (wire mode)
        slack_util      (FLT_MAX),
        exact_local_area(true),
        refactor        (true),
        est_power       (2.0f),
        est_const       (1.0f),
        batch_output    (false),
        quiet           (false)
    {
        for (uint i = 0; i <= 6; i++)               // -- default LUT cost is "number of inputs + 1" (mixed mode)
            lut_cost[i] = i + 1;
    }
};


void techMap(Gig& N, const Vec<Params_TechMap>& Ps, WMapX<GLit>* remap = NULL);
void techMap(Gig& N, const Params_TechMap& P, uint n_rounds, WMapX<GLit>* remap = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
