//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : CutSim.hh
//| Author(s)   : Niklas Een
//| Module      : CutSim
//| Description : Generalization of ternary simulation.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__CutSim__CutSim_hh
#define ZZ__CutSim__CutSim_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


Pair<uint,uint> tempDecomp(NetlistRef N, NetlistRef M, uint max_cycle_len = 10, uint max_total_len = 20);
    // -- Produce 'M' which is a temporaly decomposed and phase abstracted version of 'M' with
    // an added 'reset' flop that can be used to remove initialization logic (by forcing to 0). 
    // In the new circuit 'M' all flops except 'reset' will be uninitialized.

void applyTempDecomp(NetlistRef N, NetlistRef M, uint pfx, uint cyc, const Vec<lbool>& cyc_start_state);
    // -- Mostly an internal method used by 'tempDecomp()', but you can call it directly if 
    // you know what you are doing.

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
