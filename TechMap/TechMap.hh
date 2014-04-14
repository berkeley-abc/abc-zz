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

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Params_TechMap {
    uint        cut_size;           // -- maximum cut size
    uint        n_iters;            // -- refinement iterations
    uint        cuts_per_node;      // -- how many cuts to store for each node in the subject graph
    float       delay_factor;       // -- how far from delay optimal are we willing to go for saving area? (1.0 = delay optimal)
    float       delay_fraction;     // -- delay value in 'gate_Delay' are multiplied by this value
    uint        balanced_cuts;      // -- #cuts to compute between "best area" and "best delay"
    float       delta_delay;        // -- minimum delay improvement to consider when computing balanced cuts.
    bool        struct_mapping;     // -- if TRUE, FTBs are not used to reduce cuts (= mapping with structural cuts)
    bool        batch_output;       // -- print a one-line summary at the end of techmapping which can be used to produce tables
    Vec<float>  lut_cost;

    Params_TechMap() :
        cut_size      (6),
        n_iters       (4),
        cuts_per_node (10),
        delay_factor  (1.0),
        delay_fraction(1.0),
        balanced_cuts (2),
        delta_delay   (1),
        struct_mapping(false),
        batch_output  (false)
    {
        for (uint i = 0; i <= cut_size; i++)        // -- default LUT cost is "number of inputs"
            lut_cost.push(i);
    }

    Params_TechMap(const Params_TechMap& other) {
        memcpy(this, &other, sizeof(Params_TechMap));
        new (&lut_cost) Vec<float>();
        other.lut_cost.copyTo(lut_cost);
    }
};


void techMap(Gig& N, const Vec<Params_TechMap>& Ps);
void techMap(Gig& N, const Params_TechMap& P, uint n_rounds);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
