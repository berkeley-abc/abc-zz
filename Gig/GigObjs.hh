//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GigObjs.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : List of Gig objects.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__GigObjs_hh
#define ZZ__Gig__GigObjs_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// List of names of Gig objects:


#define Apply_To_All_GigObjs(Macro)             \
    Macro(Fanouts)                              \
    Macro(FanoutCount)                          \
    Macro(DynamicFanouts)                       \
    Macro(Strash)

// NOTE! When a netlist is copied, the Gig objects are copied in the above order.
// An enum will contain all of the above names with 'gigobj_' prefixed to them.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Derive tables (using preprocessor):


#define Prefix_With_gigobj_And_Add_Comma(x) gigobj_##x,
enum GigObjType {
    gigobj_NULL,
    Apply_To_All_GigObjs(Prefix_With_gigobj_And_Add_Comma)
    GigObjType_size
};


#define Prefix_With_class_GigObj_And_Add_Semi(x) class GigObj_##x;
Apply_To_All_GigObjs(Prefix_With_class_GigObj_And_Add_Semi)


struct Gig;
struct GigObj;
typedef void (*GigObj_Factory)(Gig& N, GigObj*& ret, bool init);
extern GigObj_Factory gigobj_factory_funcs[GigObjType_size];


extern cchar* GigObjType_name[GigObjType_size + 1];


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
