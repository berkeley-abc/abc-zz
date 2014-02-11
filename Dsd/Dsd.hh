//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Dsd.hh
//| Author(s)   : Niklas Een
//| Module      : Dsd
//| Description : Disjoint Support Decomposition of 6-input functions from truth table.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Dsd__Dsd_hh
#define ZZ__Dsd__Dsd_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// DSD programs:


enum { DSD6_FIRST_INTERNAL = 6 };
enum { DSD6_CONST_TRUE = 127 };


struct Params_Dsd {
    bool use_box3;      // -- if TRUE, ternary functions are represented using 'dsd_Box3' followed by FTB
    bool only_muxes;    // -- no other ternary function is used
    bool cofactor;      // -- remove all 'dsd_Box#' by cofactoring
    bool use_kary;      // -- post-process decomposition and extract k-ary ANDs and XORs (no 'dsd_And' or 'dsd_Xor' will be present afterwards, not even for binary gates)

    Params_Dsd() :
        use_box3(false),
        only_muxes(false),
        cofactor(true),
        use_kary(true)
    {}
};


void dsd6(uint64 ftb, Vec<uchar>& prog, Params_Dsd P = Params_Dsd());


/*
A DSD program is a 'Vec<uchar>' where the vector is a sequence of variable-length instructions.
Each instruction starts with a op-code 'DsdOp' followed by up to 6 arguments. Each argument is
a literal with its sign bit stored in bit 7 and its ID in bits 6..0. If the ID is smaller than
'DSD6_FIRST_INTERNAL', then it is a primary input, if not it refers to the output of instruction
number 'ID - DSD6_FIRST_INTERNAL'. The last instruction is always of type 'dsd_End' and points
to the top of the formula. Special ID 'DSD6_CONST_TRUE' is only used for the constant functions.

If 'use_box3' is enabled, all 3-input functions are stored as a 'dsd_Box3', otherwise MAJ, ONE,
GAMB, MUX, or DOT are used.

If 'only_muxes' is enabled, MUX is the only 3-input function used during decomposition (which
may be useful in conjunction with 'cofactor' to have a target with only AND/XOR/MUXes).

If 'cofactor' is enabled, the program can be a DAG and will never contain boxes (except 'dsd_Box3'
if 'use_box3' is enabled). If 'cofactor' is disabled, the program is always a tree and may contain
boxes.

Finally, if 'use_kary' is enabled, a post-processing phase is run to collect 2-input ANDs and XORs
into k-ary gates. No 'dsd_And' or 'dsd_Xor' will be present in the output, even for binary gates.
*/


enum DsdOp {
    dsd_End,    // -- 1 input, the top of the formula
    dsd_And,    // -- 2 inputs
    dsd_Xor,    // -- 2 inputs, second argument always unsigned
    dsd_Maj,    // -- 3 inputs: at least two inputs are true
    dsd_One,    // -- 3 inputs: exactly one input is true
    dsd_Gamb,   // -- 3 inputs: all or none of the inputs are true
    // Non-symmetric:
    dsd_Mux,    // -- 3 inputs: selector, true-branch, false-branch
    dsd_Dot,    // -- 3 inputs: a^b | ac ("diff-or-true")
    // Boxes:
    dsd_Box3,   // -- 3 inputs followed by a 1-byte FTB
    dsd_Box4,   // -- 4 inputs followed by a 2-byte FTB
    dsd_Box5,   // -- 5 inputs followed by a 4-byte FTB
    dsd_Box6,   // -- 6 inputs followed by a 8-byte FTB
    // k-ary:
    dsd_kAnd,   // -- k inputs; first byte is 'k'
    dsd_kXor,   // -- k inputs; first byte is 'k'

    dsd_Box2,   // -- DEBUGGING: 2 inputs followed by a 1-byte FTB

    DsdOp_size
};

extern const uint DsdOp_isize[DsdOp_size];


macro Lit dsdLit(uchar byte) { return Lit(byte & 0x7F, byte >> 7); }


// Functions on DSD programs:
uint64 eval   (const Vec<uchar>& prog);
uint   nLeafs (const Vec<uchar>& prog);
bool   hasBox (const Vec<uchar>& prog);
void   dumpDsd(const Vec<uchar>& prog);     // -- for debugging


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
