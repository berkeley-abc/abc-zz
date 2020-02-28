//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Ftb6.cc
//| Author(s)   : Niklas Een
//| Module      : Ftb6
//| Description : 6-input function table manipulation.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Tables:


uint64 ftb6_proj[2][6] = {  // -- ftb6_proj[0] = ident, ftb6_proj[1] = invert
    { 0xAAAAAAAAAAAAAAAAull, 0xCCCCCCCCCCCCCCCCull, 0xF0F0F0F0F0F0F0F0ull, 0xFF00FF00FF00FF00ull, 0xFFFF0000FFFF0000ull, 0xFFFFFFFF00000000ull },
    { 0x5555555555555555ull, 0x3333333333333333ull, 0x0F0F0F0F0F0F0F0Full, 0x00FF00FF00FF00FFull, 0x0000FFFF0000FFFFull, 0x00000000FFFFFFFFull }
};


uint ftb6_swap_shift[6][6] = {
    { 0, 1, 3, 7,15,31},
    { 1, 0, 2, 6,14,30},
    { 3, 2, 0, 4,12,28},
    { 7, 6, 4, 0, 8,24},
    {15,14,12, 8, 0,16},
    {31,30,28,24,16, 0}
};


uint64 ftb6_swap_rmask[6][6] = {
    {0x0000000000000000ull,0x4444444444444444ull,0x5050505050505050ull,0x5500550055005500ull,0x5555000055550000ull,0x5555555500000000ull},
    {0x4444444444444444ull,0x0000000000000000ull,0x3030303030303030ull,0x3300330033003300ull,0x3333000033330000ull,0x3333333300000000ull},
    {0x5050505050505050ull,0x3030303030303030ull,0x0000000000000000ull,0x0F000F000F000F00ull,0x0F0F00000F0F0000ull,0x0F0F0F0F00000000ull},
    {0x5500550055005500ull,0x3300330033003300ull,0x0F000F000F000F00ull,0x0000000000000000ull,0x00FF000000FF0000ull,0x00FF00FF00000000ull},
    {0x5555000055550000ull,0x3333000033330000ull,0x0F0F00000F0F0000ull,0x00FF000000FF0000ull,0x0000000000000000ull,0x0000FFFF00000000ull},
    {0x5555555500000000ull,0x3333333300000000ull,0x0F0F0F0F00000000ull,0x00FF00FF00000000ull,0x0000FFFF00000000ull,0x0000000000000000ull}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
