//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : AbcInterface.hh
//| Author(s)   : Niklas Een
//| Module      : AbcInterface
//| Description : Convert 'Netlist' to ABC's 'Gia' and call a script, then get the result back.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__AbcInterface__AbcInterface_hh
#define ZZ__AbcInterface__AbcInterface_hh

#include "ZZ_Netlist.hh"
#include "ZZ_Bip.Common.hh"     // -- get type declaration for 'Cex' 

struct Gia_Man_t_;
typedef struct Gia_Man_t_ Gia_Man_t;

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void       initAbc();
void       resetAbc();


struct GiaNums {
    Vec<int> pi;    // -- maps 'Gia' input# into PI 'number'; negative numbers are resreved for
                    //    pseudo-inputs introduced for X initialized flops (use ~ to restore)
    Vec<int> po;    // -- maps 'Gia' output# into PO 'number'
    Vec<int> ff;    // -- maps 'Gia' flop# into Flop 'number'; negative numbers are reserved
                    //    for '1' initialized flops (which have inverters put on both sides)
    int      reset; // -- flop# in 'Gia' for reset flop or 'INT_MIN' if none were added
};


Gia_Man_t* createGia(NetlistRef N, /*out:*/GiaNums& nums);
void       giaToNetlist(Gia_Man_t* G, GiaNums& nums, NetlistRef N);

lbool      runAbcScript(NetlistRef N, String cmd, /*out*/Cex& cex, /*in*/FILE* redirect_stdout = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
