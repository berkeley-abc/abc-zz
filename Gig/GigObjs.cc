//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GigObjs.cc
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : List of Gig objects.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "GigExtra.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void GigObj_Fanouts_new    (Gig& N, GigObj*& ret, bool init) { ret = new GigObj_Fanouts    (N); if (init) ret->init(); }
void GigObj_FanoutCount_new(Gig& N, GigObj*& ret, bool init) { ret = new GigObj_FanoutCount(N); if (init) ret->init(); }
void GigObj_Strash_new     (Gig& N, GigObj*& ret, bool init) { ret = new GigObj_Strash     (N); if (init) ret->init(); }

GigObj_Factory gigobj_factory_funcs[GigObjType_size] = {
    NULL,
    GigObj_Fanouts_new,
    GigObj_FanoutCount_new,
    NULL,   // <<== dynamic fanouts, not done yet
    GigObj_Strash_new,
};


#define Stringify_And_Add_Comma(x) #x,
cchar* GigObjType_name[GigObjType_size + 1] = {
    "NULL",
    Apply_To_All_GigObjs(Stringify_And_Add_Comma)
    NULL
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
