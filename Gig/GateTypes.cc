//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GateTypes.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "GateTypes.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#define Stringify_And_Add_Comma(x) #x,
cchar* GateType_name[GateType_size + 1] = {
    "NULL",
    Apply_To_All_GateTypes(Stringify_And_Add_Comma)
    NULL
};


#define Macro_GateType_Size(x) GateDef_##x::size,
uint gatetype_size[GateType_size + 1] = {
    0,
    Apply_To_All_GateTypes(Macro_GateType_Size)
    0
};


#define Macro_GateType_Attr(x) (GateAttrType)GateDef_##x::attr,
GateAttrType gatetype_attr[GateType_size + 1] = {
    attr_NULL,
    Apply_To_All_GateTypes(Macro_GateType_Attr)
    attr_NULL
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
