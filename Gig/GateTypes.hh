//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GateTypes.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description :
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| This file is auto-generated from 'GateTypes.def'. Don't modify directly.
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__GateTypes_hh
#define ZZ__Gig__GateTypes_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Supporting types:


static const uint DYNAMIC_GATE_SIZE = INT_MAX;
    // -- used as "size" specifier for gates with arbitrary number of inputs.


enum GateAttrType {
    attr_NULL,    // -- no argument (allows for three inputs to be inlined)
    attr_Arg,     // -- uninterpreted 32-bit 'uint'.
    attr_LB,      // -- uninterpreted 'lbool'.
    attr_Num,     // -- uniquely numbered (for side tables)
    attr_Enum,    // -- uniquely numbered and enumerable (for fast access to all gates of this type)
    GateAttrType_size
};

macro bool isNumberedAttr(GateAttrType attr_type) {
    return attr_type >= attr_Num; }

static cchar* GateAttrType_name[GateAttrType_size] = {
    "<null>", "Num", "Enum", "Arg", "LB" };

template<> fts_macro void write_(Out& out, const GateAttrType& v){
    out += GateAttrType_name[v]; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Local macros (undefined below):


#define INF DYNAMIC_GATE_SIZE
#define DEF(name, sz, attrib)                   \
    struct GateDef_##name {                     \
        enum { size = sz };                     \
        enum { attr = attr_##attrib };          \
    };


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Definition of gate types:


#define Apply_To_All_GateTypes(Macro) \
    Macro(Const) \
    Macro(Reset) \
    Macro(FF) \
    Macro(PI) \
    Macro(Clk) \
    Macro(PPI) \
    Macro(PO) \
    Macro(SafeProp) \
    Macro(SafeCons) \
    Macro(FairProp) \
    Macro(FairCons) \
    Macro(OrigFF) \
    Macro(Seq) \
    Macro(And) \
    Macro(Xor) \
    Macro(Mux) \
    Macro(Maj) \
    Macro(Buf) \
    Macro(Not) \
    Macro(Or) \
    Macro(Equiv) \
    Macro(Conj) \
    Macro(Disj) \
    Macro(Even) \
    Macro(Odd) \
    Macro(Lut4) \
    Macro(Npn4) \
    Macro(Lut6) \
    Macro(WLut) \
    Macro(Uif) \
    Macro(Delay) \
    Macro(Box) \
    Macro(MFlop) \
    Macro(MemR) \
    Macro(MemW) \
    Macro(MMux) \
    Macro(Vec) \
    Macro(Sel)


// Built-ins:
DEF( Const    , 0    , LB   )
DEF( Reset    , 0    , NULL )   // Signal to be true only for cycle 0.

// State:
DEF( FF       , 2    , Enum )   // Pin0 = data, Pin1 = init (implicitly muxed on 'Reset')

// Inputs:
DEF( PI       , 0    , Enum )
DEF( Clk      , 0    , Enum )
DEF( PPI      , 0    , Arg  )   // Numbering is not enforced for Pseudo-PIs

// Outputs:
DEF( PO       , 1    , Enum )
DEF( SafeProp , 1    , Enum )
DEF( SafeCons , 1    , Enum )
DEF( FairProp , 1    , Enum )   // Takes a bit-vector ('Vec') as input.
DEF( FairCons , 1    , Enum )
DEF( OrigFF   , 1    , Enum )   // Marks the signal corresponding to the original flop value before retiming or initialization transformation

// Special buffer:
DEF( Seq      , 1    , NULL )   // Used to break cycles (topological sort considers these as fanout free, even when they are not)
                                // These are put on the input side of every 'FF'.
// AIG:
DEF( And      , 2    , NULL )

// Extended AIG:
DEF( Xor      , 2    , NULL )
DEF( Mux      , 3    , NULL )
DEF( Maj      , 3    , NULL )   // x + y + z >= 2

// Alternative AIG nodes:
DEF( Buf      , 1    , NULL )
DEF( Not      , 1    , NULL )
DEF( Or       , 2    , NULL )
DEF( Equiv    , 2    , NULL )

// Multi-input AIG:
DEF( Conj     , INF  , NULL )
DEF( Disj     , INF  , NULL )
DEF( Even     , INF  , NULL )
DEF( Odd      , INF  , NULL )

// LUTs:
DEF( Lut4     , 4    , Arg  )   // Argument stores the 16-bit FTB
DEF( Npn4     , 4    , Arg  )   // Argument stores the NPN class number (0..221)
DEF( Lut6     , 6    , Num  )   // 64-bit FTB too big, stored in side-table
DEF( WLut     , INF  , Num  )   // FTB also stored in side-table

// Black-boxes:
DEF( Uif      , INF  , Arg  )   // Combinational box, arg should identify the logic inside the box
DEF( Delay    , INF  , Arg  )   // Combinational box, logic is lost, arg gives delay of box 
DEF( Box      , INF  , Arg  )   // Sequential box, all inputs should go to 'Seq' gates. Outputs may go to 'Sel' if more than one output pin.

// Memories:
DEF( MFlop    , 1    , Enum )
DEF( MemR     , 2    , Num  )
DEF( MemW     , 3    , Num  )
DEF( MMux     , 3    , Num  )

// Bit-vectors:
DEF( Vec      , INF  , NULL )
DEF( Sel      , 1    , Arg  )


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Derive tables (using preprocessor):


#undef DEF
#undef INF


#define Prefix_With_gate_And_Add_Comma(x) gate_##x,
enum GateType {
    gate_NULL,
    Apply_To_All_GateTypes(Prefix_With_gate_And_Add_Comma)
    GateType_size
};

static const uint GATETYPE_BITS = Bits_Needed(GateType_size);

extern cchar* GateType_name[GateType_size + 1];

template<> fts_macro void write_(Out& out, const GateType& v) {
    out += GateType_name[v]; }


extern uint         gatetype_size[GateType_size+1];
extern GateAttrType gatetype_attr[GateType_size+1];


macro bool isNumbered(GateType type) {
    return isNumberedAttr(gatetype_attr[type]); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sub-types:


macro bool seqElem(GateType t) { return t == gate_FF || t == gate_Box || t == gate_MFlop; }
    // -- these are the gates that should have 'Seq' gates on their input side

macro bool combInput (GateType t) { return (t >= gate_FF && t <= gate_PPI) || seqElem(t); }
macro bool combOutput(GateType t) { return t >= gate_PO && t <= gate_Seq; }

macro bool seqInput (GateType t) { return t >= gate_PI && t <= gate_PPI; }
macro bool seqOutput(GateType t) { return t >= gate_PO && t <= gate_OrigFF; }
    // -- excludes 'FF' and 'Seq' from combinational inputs/outputs.

macro bool aigGate (GateType t) { return t <= gate_And; }
macro bool xigGate (GateType t) { return t <= gate_Maj; }
macro bool npn4Gate(GateType t) { return t <= gate_Seq || t == gate_Npn4; }
macro bool lut4Gate(GateType t) { return t <= gate_Seq || t == gate_Lut4; }
macro bool lut6Gate(GateType t) { return t <= gate_Seq || t == gate_Lut6; }
    // -- for efficiency, we allow 'gate_NULL' here.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
