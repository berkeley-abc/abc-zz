//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Npn4.hh
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

#ifndef ZZ__TechMap__Npn4_hh
#define ZZ__TechMap__Npn4_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Types:


typedef ushort ftb4_t;
typedef uchar  pseq4_t;     // -- bit 0..1 tells where input 0 should go, bit 2..3 where input 1 should go etc.
typedef uchar  perm4_t;     // -- permutation# 0..23 
typedef uchar  negs4_t;     // -- negations: 0..31 (bit 0..3 for inputs, 4 for output)

static const pseq4_t pseq4_NULL = 255;
static const perm4_t perm4_NULL = 255;

struct Npn4Norm {
    uchar   eq_class;   // -- Use 'npn4_repr' to get the representative FTB for this class.
    perm4_t perm;       // -- Then apply permutation...
    negs4_t negs;       // -- ...and then negations to get the original FTB given to 'npn4_norm[]'.
};


macro pseq4_t pseq4Make(uchar i0, uchar i1, uchar i2, uchar i3) {
    return i0 | (i1 << 2) | (i2 << 4) | (i3 << 6); }

macro uchar pseq4Get(pseq4_t pseq, uchar idx) {
    assert_debug(idx < 4);
    return (pseq >> (idx*2)) & 3; }

macro bool validPseq4(pseq4_t pseq) {
    uchar mask = (1u << pseq4Get(pseq, 0)) | (1u << pseq4Get(pseq, 1)) | (1u << pseq4Get(pseq, 2)) | (1u << pseq4Get(pseq, 3));
    return mask == 15; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Tables:


static const ftb4_t lut4_buf[4] = { 0xAAAA, 0xCCCC, 0xF0F0, 0xFF00 };
static const ftb4_t lut4_inv[4] = { 0x5555, 0x3333, 0x0F0F, 0x00FF };

extern Npn4Norm npn4_norm   [65536];
extern ftb4_t   npn4_repr   [222];
extern uchar    npn4_repr_sz[222];  // -- size of support

extern perm4_t pseq4_to_perm4[256];
extern pseq4_t inv_pseq4     [256];
extern pseq4_t perm4_to_pseq4[24];
extern perm4_t inv_perm4     [24];

extern ftb4_t apply_perm4    [24][65536];
extern ftb4_t apply_inv_perm4[24][65536];
extern ftb4_t apply_negs4    [32][65536];

extern uint npn4_just[222][16];     // -- list of minimal justifications for each function

// Some useful NPN classes:
static const uchar npn4_cl_TRUE   = 0;
static const uchar npn4_cl_BUF    = 21;
static const uchar npn4_cl_OR2    = 5;      // -- NOTE! Now lower pin (0 and 1)
static const uchar npn4_cl_OR3    = 2;
static const uchar npn4_cl_OR4    = 1;
static const uchar npn4_cl_EQU2   = 180;
static const uchar npn4_cl_XOR3   = 220;
static const uchar npn4_cl_EQU4   = 221;
static const uchar npn4_cl_MUX    = 109;    // order: (pin0 ? pin2 : pin1)
static const uchar npn4_cl_MAJ    = 83;     // pin0 + pin1 + pin2 >= 2
static const uchar npn4_cl_N_ONE  = 81;     // ~(pin0 + pin1 + pin2 = 1)
static const uchar npn4_cl_N_GAMB = 103;    // ~((x & y & ~z) | (~x & ~y & z))
static const uchar npn4_cl_DOT    = 105;    // (x ^ y) | (x & z)

// Compile time 'pseq4_to_perm4':
#define PERM4_0123 0
#define PERM4_0132 1
#define PERM4_0213 2
#define PERM4_0231 3
#define PERM4_0312 4
#define PERM4_0321 5
#define PERM4_1023 6
#define PERM4_1032 7
#define PERM4_1203 8
#define PERM4_1230 9
#define PERM4_1302 10
#define PERM4_1320 11
#define PERM4_2013 12
#define PERM4_2031 13
#define PERM4_2103 14
#define PERM4_2130 15
#define PERM4_2301 16
#define PERM4_2310 17
#define PERM4_3012 18
#define PERM4_3021 19
#define PERM4_3102 20
#define PERM4_3120 21
#define PERM4_3201 22
#define PERM4_3210 23


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Ftb6 like interface:


static const ftb4_t ftb4_proj[2][4] = { // -- 'ftb4_proj [sign] [pin]' ('lut4_buf' and 'lut4_inv' in one table)
    { 0xAAAA, 0xCCCC, 0xF0F0, 0xFF00 },
    { 0x5555, 0x3333, 0x0F0F, 0x00FF }
};


extern uint   ftb4_swap_shift[4][4];
extern ftb4_t ftb4_swap_rmask[4][4];


macro ftb4_t ftb4_swap(ftb4_t ftb, uint pin_a, uint pin_b)
{
    if (pin_a == pin_b) return ftb;

    ftb4_t rmask = ftb4_swap_rmask[pin_a][pin_b];
    uint   shift = ftb4_swap_shift[pin_a][pin_b];
    ftb4_t lmask = rmask >> shift;
    ftb4_t imask = (rmask | lmask) ^ 0xFFFFFFFFFFFFFFFFull;

    return (ftb & imask) | ((ftb & rmask) >> shift) | ((ftb & lmask) << shift);
}


macro ftb4_t ftb4_neg(ftb4_t ftb, uint pin)
{
    ftb4_t mask  = ftb4_proj[0][pin];
    uint   shift = (1 << pin);
    return ((ftb &  mask) >> shift)
         | ((ftb & ~mask) << shift);
}


macro bool ftb4_inSup(ftb4_t ftb, uint pin)
{
    ftb4_t mask  = ftb4_proj[0][pin];
    uint   shift = (1 << pin);
    return ((ftb & mask) >> shift) != (ftb & ~mask);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
