//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ImcPrune.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : SAT based interpolant minimization. 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| WORK IN PROGRESS! Don't used for any real application!
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__ImcPrune_hh
#define ZZ__Bip__ImcPrune_hh

#include "ZZ_Bip.Common.hh"
#include "ZZ_MiniSat.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Incremental interplant simplification:


// NOTE! Only forward case is implemented.


class ImcPrune {
    NetlistRef       N;     // Transition relation. Must not be modified during the lifetime of this object.
    const Vec<Wire>& ff;    // 'ff[num]' is the flop in 'N' with number 'num'.

    Netlist          H;     // "Head": One initialized copy of 'N'.
    Netlist          B;     // "Body": 'N' unrolled for 'k' steps.
    WMap<Wire>       n2h;
    Vec<WMap<Wire> > n2b;

    SatStd           SH;
    SatStd           SB;
    WZet             keep_H;
    WZet             keep_B;
    WMap<Lit>        h2s;
    WMap<Lit>        b2s;
    Clausify<SatStd> CH;
    Clausify<SatStd> CB;

    Netlist          R;     // Netlist used for return value of 'prune()'.

    Wire insertH(Wire w);
    Wire insertB(Wire w, uint d);

  //________________________________________
  //  Public interface:

public:
    ImcPrune(NetlistRef N_, const Vec<Wire>& ff_);
    void init();
        // -- 'N' must have been initialized at this point (then never change).
    Wire prune(Wire w_init, Wire w_itp, uint k);
        // -- 'w_itp' must be a valid 1 + k step interpolant. Returned wire is valid until next call.
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
