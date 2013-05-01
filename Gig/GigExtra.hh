//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GigExtra.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Contains the more advanced functionality of the netlist.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| The separation into 'Gig.hh' and 'GigExtra.hh' is done to break include cycles.
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__GigExtra_hh
#define ZZ__Gig__GigExtra_hh

#include "Gig.hh"
#include "Macros.hh"
#include "Fanouts.hh"
#include "Strash.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Requires Gig-object "Fanouts".
macro Fanouts fanouts(Wire w)
{
    Gig& N = *w.gig();
    return static_cast<GigObj_Fanouts&>(N.getObj(gigobj_Fanouts)).get(w);
}


// Requires Gig-object "FanoutCount".
macro uint nFanouts(Wire w)
{
    Gig& N = *w.gig();
    return static_cast<GigObj_FanoutCount&>(N.getObj(gigobj_FanoutCount)).n_fanouts[w];
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
