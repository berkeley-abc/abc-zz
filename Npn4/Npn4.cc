//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Npn4.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Manipulation of 4-input function tables (FTBs).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| The k:th bit of the 16-bit function table represent the output of the function for input value
//| 'k' interpreted as a bitvector of size 4 with input 0 corresponding to bit 0 of the number 'k',
//| input 1 to bit 1 and so forth.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Npn4.hh"
#include "ZZ/Generics/Sort.hh"


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// FTB Layout


/*
x3 x2 x1 x0  |  out
-------------+------
 0  0  0  0  |  f0
 0  0  0  1  |  f1
 0  0  1  0  |   .
 0  0  1  1  |   .
 0  1  0  0  |   .
 0  1  0  1  |
 0  1  1  0  |
 0  1  1  1  |
 1  0  0  0  |
 1  0  0  1  | [FTB]
 1  0  1  0  |
 1  0  1  1  |
 1  1  0  0  |   .
 1  1  0  1  |   .
 1  1  1  0  |   .
 1  1  1  1  |  f15
*/


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Table declarations:


// This table is updatd by 'adjustSupport()' to shift support for <4-input gates
// to the lower pins.
ftb4_t npn4_repr[222] = {
    0xFFFF, 0xFFFE, 0xFFFC, 0xFFF9, 0xFFF8, 0xFFF0, 0xFFE9, 0xFFE8, 0xFFE7, 0xFFE6,
    0xFFE4, 0xFFE1, 0xFFE0, 0xFFC3, 0xFFC2, 0xFFC0, 0xFF96, 0xFF94, 0xFF90, 0xFF81,
    0xFF80, 0xFF00, 0xFEE9, 0xFEE8, 0xFEE7, 0xFEE6, 0xFEE5, 0xFEE4, 0xFEE1, 0xFEE0,
    0xFED3, 0xFED2, 0xFED0, 0xFEC3, 0xFEC2, 0xFEC1, 0xFEC0, 0xFE97, 0xFE96, 0xFE95,
    0xFE94, 0xFE91, 0xFE90, 0xFE81, 0xFE80, 0xFE7F, 0xFE7E, 0xFE7D, 0xFE7C, 0xFE79,
    0xFE78, 0xFE76, 0xFE74, 0xFE70, 0xFE69, 0xFE68, 0xFE67, 0xFE66, 0xFE65, 0xFE64,
    0xFE61, 0xFE60, 0xFE57, 0xFE56, 0xFE55, 0xFE54, 0xFE53, 0xFE52, 0xFE51, 0xFE50,
    0xFE43, 0xFE42, 0xFE41, 0xFE40, 0xFE17, 0xFE16, 0xFE15, 0xFE14, 0xFE11, 0xFE10,
    0xFE01, 0xFCC3, 0xFCC2, 0xFCC0, 0xFCA9, 0xFCA8, 0xFCA7, 0xFCA6, 0xFCA5, 0xFCA4,
    0xFCA1, 0xFCA0, 0xFC97, 0xFC96, 0xFC95, 0xFC94, 0xFC93, 0xFC92, 0xFC91, 0xFC90,
    0xFC83, 0xFC82, 0xFC81, 0xFC3F, 0xFC3E, 0xFC3C, 0xFC3A, 0xFC39, 0xFC38, 0xFC30,
    0xFC2B, 0xFC2A, 0xFC29, 0xFC28, 0xFC27, 0xFC26, 0xFC24, 0xFC23, 0xFC22, 0xFC21,
    0xFC03, 0xF99F, 0xF99E, 0xF99D, 0xF99C, 0xF999, 0xF998, 0xF996, 0xF994, 0xF990,
    0xF98D, 0xF98C, 0xF989, 0xF987, 0xF986, 0xF985, 0xF984, 0xF981, 0xF96F, 0xF96E,
    0xF96C, 0xF969, 0xF968, 0xF960, 0xF94F, 0xF94E, 0xF94D, 0xF94C, 0xF94B, 0xF94A,
    0xF949, 0xF948, 0xF946, 0xF942, 0xF90F, 0xF90E, 0xF90D, 0xF909, 0xF906, 0xF889,
    0xF887, 0xF886, 0xF885, 0xF881, 0xF84F, 0xF84E, 0xF84B, 0xF84A, 0xF849, 0xF843,
    0xF81F, 0xF81E, 0xF81D, 0xF81C, 0xF819, 0xF816, 0xF80F, 0xF80E, 0xF80D, 0xF807,
    0xF00F, 0xE997, 0xE996, 0xE995, 0xE994, 0xE991, 0xE981, 0xE97E, 0xE97C, 0xE979,
    0xE978, 0xE976, 0xE974, 0xE971, 0xE969, 0xE968, 0xE967, 0xE966, 0xE965, 0xE964,
    0xE961, 0xE956, 0xE953, 0xE952, 0xE943, 0xE916, 0xE881, 0xE871, 0xE869, 0xE867,
    0xE865, 0xE853, 0xE817, 0xE718, 0xE61E, 0xE61C, 0xE619, 0xE427, 0xE41B, 0xE11E,
    0xC33C, 0x9669
};

uchar    npn4_repr_sz[222];  // -- size of support
Npn4Norm npn4_norm[65536];

perm4_t pseq4_to_perm4[256];
pseq4_t inv_pseq4     [256];
pseq4_t perm4_to_pseq4[24];
perm4_t inv_perm4     [24];

ftb4_t apply_perm4    [24][65536];
ftb4_t apply_inv_perm4[24][65536];
ftb4_t apply_negs4    [32][65536];

uint npn4_just[222][16];     // -- list of minimal justifications for each function


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Internal helpers:


// For reference.
static ftb4_t permutePseq4(ftb4_t ftb, pseq4_t pseq) ___unused;
static ftb4_t permutePseq4(ftb4_t ftb, pseq4_t pseq)
{
    assert_debug(validPseq4(pseq));

    ushort new_ftb = 0;
    for (uint i = 0; i < 16; i++){
        uint j = ((i & 1)        << pseq4Get(pseq, 0))
               | (((i & 2) >> 1) << pseq4Get(pseq, 1))
               | (((i & 4) >> 2) << pseq4Get(pseq, 2))
               | (((i & 8) >> 3) << pseq4Get(pseq, 3));
        new_ftb |= ((ftb & (1 << i)) >> i) << j;
    }

    return new_ftb;
}


// About 5 times slower than the table driven permutation (not too bad)
static
ftb4_t permute4(ftb4_t f, perm4_t perm)
{
    switch (perm){
    case  0: return (f & 0xFFFF);
    case  1: return ((f & 0x0F00) >> 4) | (f & 0xF00F) | ((f & 0x00F0) << 4);
    case  2: return ((f & 0x3030) >> 2) | (f & 0xC3C3) | ((f & 0x0C0C) << 2);
    case  3: return ((f & 0x0300) >> 6) | ((f & 0x0C00) >> 4) | ((f & 0x3000) >> 2) | (f & 0xC003) | ((f & 0x000C) << 2) | ((f & 0x0030) << 4) | ((f & 0x00C0) << 6);
    case  4: return ((f & 0x3000) >> 6) | ((f & 0x0300) >> 4) | ((f & 0x0030) >> 2) | (f & 0xC003) | ((f & 0x0C00) << 2) | ((f & 0x00C0) << 4) | ((f & 0x000C) << 6);
    case  5: return ((f & 0x3300) >> 6) | (f & 0xCC33) | ((f & 0x00CC) << 6);
    case  6: return ((f & 0x4444) >> 1) | (f & 0x9999) | ((f & 0x2222) << 1);
    case  7: return ((f & 0x0400) >> 5) | ((f & 0x0900) >> 4) | ((f & 0x0200) >> 3) | ((f & 0x4004) >> 1) | (f & 0x9009) | ((f & 0x2002) << 1) | ((f & 0x0040) << 3) | ((f & 0x0090) << 4) | ((f & 0x0020) << 5);
    case  8: return ((f & 0x1010) >> 3) | ((f & 0x2020) >> 2) | ((f & 0x4040) >> 1) | (f & 0x8181) | ((f & 0x0202) << 1) | ((f & 0x0404) << 2) | ((f & 0x0808) << 3);
    case  9: return ((f & 0x0100) >> 7) | ((f & 0x0200) >> 6) | ((f & 0x0400) >> 5) | ((f & 0x0800) >> 4) | ((f & 0x1000) >> 3) | ((f & 0x2000) >> 2) | ((f & 0x4000) >> 1) | (f & 0x8001) | ((f & 0x0002) << 1) | ((f & 0x0004) << 2) | ((f & 0x0008) << 3) | ((f & 0x0010) << 4) | ((f & 0x0020) << 5) | ((f & 0x0040) << 6) | ((f & 0x0080) << 7);
    case 10: return ((f & 0x1000) >> 7) | ((f & 0x2000) >> 6) | ((f & 0x0100) >> 4) | ((f & 0x0210) >> 3) | ((f & 0x0020) >> 2) | ((f & 0x4000) >> 1) | (f & 0x8001) | ((f & 0x0002) << 1) | ((f & 0x0400) << 2) | ((f & 0x0840) << 3) | ((f & 0x0080) << 4) | ((f & 0x0004) << 6) | ((f & 0x0008) << 7);
    case 11: return ((f & 0x1100) >> 7) | ((f & 0x2200) >> 6) | ((f & 0x4400) >> 1) | (f & 0x8811) | ((f & 0x0022) << 1) | ((f & 0x0044) << 6) | ((f & 0x0088) << 7);
    case 12: return ((f & 0x4040) >> 3) | ((f & 0x1010) >> 2) | ((f & 0x0404) >> 1) | (f & 0x8181) | ((f & 0x2020) << 1) | ((f & 0x0808) << 2) | ((f & 0x0202) << 3);
    case 13: return ((f & 0x0400) >> 7) | ((f & 0x0100) >> 6) | ((f & 0x0800) >> 4) | ((f & 0x4200) >> 3) | ((f & 0x1000) >> 2) | ((f & 0x0004) >> 1) | (f & 0x8001) | ((f & 0x2000) << 1) | ((f & 0x0008) << 2) | ((f & 0x0042) << 3) | ((f & 0x0010) << 4) | ((f & 0x0080) << 6) | ((f & 0x0020) << 7);
    case 14: return ((f & 0x5050) >> 3) | (f & 0xA5A5) | ((f & 0x0A0A) << 3);
    case 15: return ((f & 0x0500) >> 7) | ((f & 0x0A00) >> 4) | ((f & 0x5000) >> 3) | (f & 0xA005) | ((f & 0x000A) << 3) | ((f & 0x0050) << 4) | ((f & 0x00A0) << 7);
    case 16: return ((f & 0x1000) >> 9) | ((f & 0x2100) >> 6) | ((f & 0x4210) >> 3) | (f & 0x8421) | ((f & 0x0842) << 3) | ((f & 0x0084) << 6) | ((f & 0x0008) << 9);
    case 17: return ((f & 0x1000) >> 9) | ((f & 0x0100) >> 7) | ((f & 0x2000) >> 6) | ((f & 0x0200) >> 4) | ((f & 0x4000) >> 3) | ((f & 0x0010) >> 2) | ((f & 0x0400) >> 1) | (f & 0x8001) | ((f & 0x0020) << 1) | ((f & 0x0800) << 2) | ((f & 0x0002) << 3) | ((f & 0x0040) << 4) | ((f & 0x0004) << 6) | ((f & 0x0080) << 7) | ((f & 0x0008) << 9);
    case 18: return ((f & 0x4000) >> 7) | ((f & 0x1000) >> 6) | ((f & 0x0400) >> 5) | ((f & 0x0100) >> 4) | ((f & 0x0040) >> 3) | ((f & 0x0010) >> 2) | ((f & 0x0004) >> 1) | (f & 0x8001) | ((f & 0x2000) << 1) | ((f & 0x0800) << 2) | ((f & 0x0200) << 3) | ((f & 0x0080) << 4) | ((f & 0x0020) << 5) | ((f & 0x0008) << 6) | ((f & 0x0002) << 7);
    case 19: return ((f & 0x4400) >> 7) | ((f & 0x1100) >> 6) | ((f & 0x0044) >> 1) | (f & 0x8811) | ((f & 0x2200) << 1) | ((f & 0x0088) << 6) | ((f & 0x0022) << 7);
    case 20: return ((f & 0x5000) >> 7) | ((f & 0x0500) >> 4) | ((f & 0x0050) >> 3) | (f & 0xA005) | ((f & 0x0A00) << 3) | ((f & 0x00A0) << 4) | ((f & 0x000A) << 7);
    case 21: return ((f & 0x5500) >> 7) | (f & 0xAA55) | ((f & 0x00AA) << 7);
    case 22: return ((f & 0x1000) >> 9) | ((f & 0x4000) >> 7) | ((f & 0x0100) >> 6) | ((f & 0x0400) >> 4) | ((f & 0x0010) >> 3) | ((f & 0x2000) >> 2) | ((f & 0x0040) >> 1) | (f & 0x8001) | ((f & 0x0200) << 1) | ((f & 0x0004) << 2) | ((f & 0x0800) << 3) | ((f & 0x0020) << 4) | ((f & 0x0080) << 6) | ((f & 0x0002) << 7) | ((f & 0x0008) << 9);
    case 23: return ((f & 0x1000) >> 9) | ((f & 0x4100) >> 7) | ((f & 0x0400) >> 5) | ((f & 0x2010) >> 2) | (f & 0x8241) | ((f & 0x0804) << 2) | ((f & 0x0020) << 5) | ((f & 0x0082) << 7) | ((f & 0x0008) << 9);
    default: assert(false); return 0; }
}


static
ftb4_t negate4(ftb4_t f, negs4_t negs)
{
    switch (negs){
    case  0: return (f & 0xFFFF);
    case  1: return ((f & 0xAAAA) >> 1) | ((f & 0x5555) << 1);
    case  2: return ((f & 0xCCCC) >> 2) | ((f & 0x3333) << 2);
    case  3: return ((f & 0x8888) >> 3) | ((f & 0x4444) >> 1) | ((f & 0x2222) << 1) | ((f & 0x1111) << 3);
    case  4: return ((f & 0xF0F0) >> 4) | ((f & 0x0F0F) << 4);
    case  5: return ((f & 0xA0A0) >> 5) | ((f & 0x5050) >> 3) | ((f & 0x0A0A) << 3) | ((f & 0x0505) << 5);
    case  6: return ((f & 0xC0C0) >> 6) | ((f & 0x3030) >> 2) | ((f & 0x0C0C) << 2) | ((f & 0x0303) << 6);
    case  7: return ((f & 0x8080) >> 7) | ((f & 0x4040) >> 5) | ((f & 0x2020) >> 3) | ((f & 0x1010) >> 1) | ((f & 0x0808) << 1) | ((f & 0x0404) << 3) | ((f & 0x0202) << 5) | ((f & 0x0101) << 7);
    case  8: return ((f & 0xFF00) >> 8) | ((f & 0x00FF) << 8);
    case  9: return ((f & 0xAA00) >> 9) | ((f & 0x5500) >> 7) | ((f & 0x00AA) << 7) | ((f & 0x0055) << 9);
    case 10: return ((f & 0xCC00) >> 10) | ((f & 0x3300) >> 6) | ((f & 0x00CC) << 6) | ((f & 0x0033) << 10);
    case 11: return ((f & 0x8800) >> 11) | ((f & 0x4400) >> 9) | ((f & 0x2200) >> 7) | ((f & 0x1100) >> 5) | ((f & 0x0088) << 5) | ((f & 0x0044) << 7) | ((f & 0x0022) << 9) | ((f & 0x0011) << 11);
    case 12: return ((f & 0xF000) >> 12) | ((f & 0x0F00) >> 4) | ((f & 0x00F0) << 4) | ((f & 0x000F) << 12);
    case 13: return ((f & 0xA000) >> 13) | ((f & 0x5000) >> 11) | ((f & 0x0A00) >> 5) | ((f & 0x0500) >> 3) | ((f & 0x00A0) << 3) | ((f & 0x0050) << 5) | ((f & 0x000A) << 11) | ((f & 0x0005) << 13);
    case 14: return ((f & 0xC000) >> 14) | ((f & 0x3000) >> 10) | ((f & 0x0C00) >> 6) | ((f & 0x0300) >> 2) | ((f & 0x00C0) << 2) | ((f & 0x0030) << 6) | ((f & 0x000C) << 10) | ((f & 0x0003) << 14);
    case 15: return ((f & 0x8000) >> 15) | ((f & 0x4000) >> 13) | ((f & 0x2000) >> 11) | ((f & 0x1000) >> 9) | ((f & 0x0800) >> 7) | ((f & 0x0400) >> 5) | ((f & 0x0200) >> 3) | ((f & 0x0100) >> 1) | ((f & 0x0080) << 1) | ((f & 0x0040) << 3) | ((f & 0x0020) << 5) | ((f & 0x0010) << 7) | ((f & 0x0008) << 9) | ((f & 0x0004) << 11) | ((f & 0x0002) << 13) | ((f & 0x0001) << 15);
    case 16: return 0xFFFF^( (f & 0xFFFF) );
    case 17: return 0xFFFF^( ((f & 0xAAAA) >> 1) | ((f & 0x5555) << 1) );
    case 18: return 0xFFFF^( ((f & 0xCCCC) >> 2) | ((f & 0x3333) << 2) );
    case 19: return 0xFFFF^( ((f & 0x8888) >> 3) | ((f & 0x4444) >> 1) | ((f & 0x2222) << 1) | ((f & 0x1111) << 3) );
    case 20: return 0xFFFF^( ((f & 0xF0F0) >> 4) | ((f & 0x0F0F) << 4) );
    case 21: return 0xFFFF^( ((f & 0xA0A0) >> 5) | ((f & 0x5050) >> 3) | ((f & 0x0A0A) << 3) | ((f & 0x0505) << 5) );
    case 22: return 0xFFFF^( ((f & 0xC0C0) >> 6) | ((f & 0x3030) >> 2) | ((f & 0x0C0C) << 2) | ((f & 0x0303) << 6) );
    case 23: return 0xFFFF^( ((f & 0x8080) >> 7) | ((f & 0x4040) >> 5) | ((f & 0x2020) >> 3) | ((f & 0x1010) >> 1) | ((f & 0x0808) << 1) | ((f & 0x0404) << 3) | ((f & 0x0202) << 5) | ((f & 0x0101) << 7) );
    case 24: return 0xFFFF^( ((f & 0xFF00) >> 8) | ((f & 0x00FF) << 8) );
    case 25: return 0xFFFF^( ((f & 0xAA00) >> 9) | ((f & 0x5500) >> 7) | ((f & 0x00AA) << 7) | ((f & 0x0055) << 9) );
    case 26: return 0xFFFF^( ((f & 0xCC00) >> 10) | ((f & 0x3300) >> 6) | ((f & 0x00CC) << 6) | ((f & 0x0033) << 10) );
    case 27: return 0xFFFF^( ((f & 0x8800) >> 11) | ((f & 0x4400) >> 9) | ((f & 0x2200) >> 7) | ((f & 0x1100) >> 5) | ((f & 0x0088) << 5) | ((f & 0x0044) << 7) | ((f & 0x0022) << 9) | ((f & 0x0011) << 11) );
    case 28: return 0xFFFF^( ((f & 0xF000) >> 12) | ((f & 0x0F00) >> 4) | ((f & 0x00F0) << 4) | ((f & 0x000F) << 12) );
    case 29: return 0xFFFF^( ((f & 0xA000) >> 13) | ((f & 0x5000) >> 11) | ((f & 0x0A00) >> 5) | ((f & 0x0500) >> 3) | ((f & 0x00A0) << 3) | ((f & 0x0050) << 5) | ((f & 0x000A) << 11) | ((f & 0x0005) << 13) );
    case 30: return 0xFFFF^( ((f & 0xC000) >> 14) | ((f & 0x3000) >> 10) | ((f & 0x0C00) >> 6) | ((f & 0x0300) >> 2) | ((f & 0x00C0) << 2) | ((f & 0x0030) << 6) | ((f & 0x000C) << 10) | ((f & 0x0003) << 14) );
    case 31: return 0xFFFF^( ((f & 0x8000) >> 15) | ((f & 0x4000) >> 13) | ((f & 0x2000) >> 11) | ((f & 0x1000) >> 9) | ((f & 0x0800) >> 7) | ((f & 0x0400) >> 5) | ((f & 0x0200) >> 3) | ((f & 0x0100) >> 1) | ((f & 0x0080) << 1) | ((f & 0x0040) << 3) | ((f & 0x0020) << 5) | ((f & 0x0010) << 7) | ((f & 0x0008) << 9) | ((f & 0x0004) << 11) | ((f & 0x0002) << 13) | ((f & 0x0001) << 15) );
    default: assert(false); return 0; }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Table initialization:


static
void adjustSupport()
{
    for (uint cl = 0; cl < 222; cl++){
        ushort ftb = npn4_repr[cl];
        for(uint n = 0; n < 3; n++){
            if (!ftb4_inSup(ftb, 0)) ftb = ftb4_swap(ftb, 0, 1);
            if (!ftb4_inSup(ftb, 1)) ftb = ftb4_swap(ftb, 1, 2);
            if (!ftb4_inSup(ftb, 2)) ftb = ftb4_swap(ftb, 2, 3);
        }
        npn4_repr[cl] = ftb;

        // Store support size:
        npn4_repr_sz[cl] = 0;
        for (uint pin = 4; pin > 0;){ pin--;
            if (ftb4_inSup(ftb, pin)){
                npn4_repr_sz[cl] = pin + 1;
                break;
            }
        }
    }

}


static
void genPseqMaps()
{
    uint p = 0;
    for (uchar i0 = 0; i0 < 4; i0++)
    for (uchar i1 = 0; i1 < 4; i1++)
    for (uchar i2 = 0; i2 < 4; i2++)
    for (uchar i3 = 0; i3 < 4; i3++){
        uchar i   = pseq4Make(i0, i1, i2, i3);
        if (validPseq4(i)){
            uchar inv = pseq4Make((i0 == 0) ? 0 : (i1 == 0) ? 1 : (i2 == 0) ? 2 : 3,
                                  (i0 == 1) ? 0 : (i1 == 1) ? 1 : (i2 == 1) ? 2 : 3,
                                  (i0 == 2) ? 0 : (i1 == 2) ? 1 : (i2 == 2) ? 2 : 3,
                                  (i0 == 3) ? 0 : (i1 == 3) ? 1 : (i2 == 3) ? 2 : 3);
            pseq4_to_perm4[i] = p;
            perm4_to_pseq4[p] = i;
            inv_pseq4[i] = inv;
            p++;
        }else{
            pseq4_to_perm4[i] = perm4_NULL;
            inv_pseq4[i] = pseq4_NULL;
        }
    }

    for (uint i = 0; i < 24; i++)
        inv_perm4[i] = pseq4_to_perm4[inv_pseq4[perm4_to_pseq4[i]]];
}


static
void genApplyPerm()
{
    for (perm4_t perm = 0; perm < 24; perm++){
        for (uint ftb = 0; ftb < 65536; ftb++){
            ftb4_t f = permute4(ftb, perm);
            apply_perm4[perm][ftb] = f;
            apply_inv_perm4[perm][f]   = ftb;
        }
    }
}


static
void genApplyNegs()
{
    for (negs4_t negs = 0; negs < 32; negs++){
        for (uint ftb = 0; ftb < 65536; ftb++){
            ftb4_t f = negate4(ftb, negs);
            apply_negs4[negs][ftb] = f;
        }
    }
}


static
void genNpnNorm()
{
    for (uint i = 0; i < 65536; i++)
        npn4_norm[i].eq_class = 255;

    for (uint cl = 0; cl < 222; cl++){
        ftb4_t ftb = npn4_repr[cl];
        for (negs4_t negs = 0; negs < 32; negs++){
            for (perm4_t perm = 0; perm < 24; perm++){
                ftb4_t f = apply_negs4[negs][apply_perm4[perm][ftb]];
                if (npn4_norm[f].eq_class == 255){
                    npn4_norm[f].eq_class = cl;
                    npn4_norm[f].perm = perm;
                    npn4_norm[f].negs = negs;
                }
            }
        }
    }

    for (uint i = 0; i < 65536; i++)
        assert(npn4_norm[i].eq_class != 255);
}


static
bool minSup(ftb4_t ftb, uint a, uchar sup, Vec<uchar>& all_sup)
{
    // Support too small?
    bool val_a = ftb & (1u << a);
    for (uint i = 0; i < 16; i++){
        if ((a & sup) == (i & sup)){
            bool val_i = ftb & (1u << i);
            if (val_a != val_i)
                return false;
        }
    }

    // Support minimal?
    bool minimal = true;
    for (uint b = 0; b < 4; b++){
        if (sup & (1u << b)){
            uchar new_sup = sup & ((1u << b) ^ 15);
            if (minSup(ftb, a, new_sup, all_sup))
                minimal = false;
        }
    }

    if (minimal)
        all_sup.push(sup);

    return true;
}


static
void minSup(ftb4_t ftb, uint a, Vec<uchar>& all_sup) {
    minSup(ftb, a, 15, all_sup);
    sortUnique(all_sup); }


static
void genNpnJust()   // -- takes about 1.2 ms
{
    Vec<uchar> all;
    for (uint cl = 0; cl < 222; cl++){
        for (uint a = 0; a < 16; a++){
            all.clear();
            minSup(npn4_repr[cl], a, all);

            uint mask = 0;
            for (uint i = 0; i < all.size(); i++)
                mask |= uint(all[i]) << (i * 4);

            npn4_just[cl][a] = mask;
        }
    }
}


ZZ_Initializer(npn4, -9500) {
    adjustSupport();
    genPseqMaps();
    genApplyPerm();
    genApplyNegs();
    genNpnNorm();
    genNpnJust();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ftb6 like interface:


uint ftb4_swap_shift[4][4] = {
    { 0, 1, 3, 7,},
    { 1, 0, 2, 6,},
    { 3, 2, 0, 4,},
    { 7, 6, 4, 0,}
};


ftb4_t ftb4_swap_rmask[4][4] = {
    {0x0000,0x4444,0x5050,0x5500},
    {0x4444,0x0000,0x3030,0x3300},
    {0x5050,0x3030,0x0000,0x0F00},
    {0x5500,0x3300,0x0F00,0x0000}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
