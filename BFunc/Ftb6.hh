//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Ftb6.hh
//| Author(s)   : Niklas Een
//| Module      : Ftb6
//| Description : 6-input function table manipulation.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__CellMap__Ftb6_hh
#define ZZ__CellMap__Ftb6_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


extern uint64 ftb6_proj[2][6];  // -- 'ftb6_proj [sign] [pin]'

extern uint   ftb6_swap_shift[6][6];
extern uint64 ftb6_swap_rmask[6][6];


macro uint64 ftb6_swap(uint64 ftb, uint pin_a, uint pin_b)
{
    if (pin_a == pin_b) return ftb;

    uint64 rmask = ftb6_swap_rmask[pin_a][pin_b];
    uint   shift = ftb6_swap_shift[pin_a][pin_b];
    uint64 lmask = rmask >> shift;
    uint64 imask = (rmask | lmask) ^ 0xFFFFFFFFFFFFFFFFull;

    return (ftb & imask) | ((ftb & rmask) >> shift) | ((ftb & lmask) << shift);
}


macro uint64 ftb6_neg(uint64 ftb, uint pin)
{
    uint64 mask  = ftb6_proj[0][pin];
    uint   shift = (1 << pin);
    return ((ftb &  mask) >> shift)
         | ((ftb & ~mask) << shift);
}


macro bool ftb6_inSup(uint64 ftb, uint pin)
{
    uint64 mask  = ftb6_proj[0][pin];
    uint   shift = (1 << pin);
    return ((ftb & mask) >> shift) != (ftb & ~mask);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
