//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : AlignedAlloc.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Allocate aligned memory blocks.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| (see AlignedAlloc.hh)
//|________________________________________________________________________________________________

#include "Prelude.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


size_t total_waste = 0;
size_t total_alloc = 0;
bool   profile_aligned_alloc = false;


ZZ_Initializer(aligned_alloc, 0) {
    profile_aligned_alloc = (getenv("PROFILE_ALIGNED_ALLOC") != NULL);
}


ZZ_Finalizer(aligned_alloc, 0) {
    if (profile_aligned_alloc){
        printf("\nALIGNED ALLOC STATS:\n");
        printf("  %u bytes wasted\n", (uint)total_waste);
        printf("  %u useful bytes allocated\n", (uint)total_alloc);
        printf("  %u total bytes allocated\n", (uint)total_alloc + (uint)total_waste);
        printf("  waste ratio: %.2f %%\n", total_waste * 100.0 / (total_alloc + total_waste));
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
