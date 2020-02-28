//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BasicTypes.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Defines some basic types of the 'Netlist' ADT.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__BasicTypes_h
#define ZZ__Netlist__BasicTypes_h

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Typedefs:


typedef uint uintg;             // -- This integer is large enough to hold the maximum number of gates
#define UINTG_MAX UINT_MAX

typedef uintg gate_id;          // -- currently, only 31 bits must be used (top-bit must be zero)
typedef uintg serial_t;         // -- serial# is a gate-type unique ID (eg. every And gate has a unique serial#, but they may overlap with serial# of other gate-types)
typedef uint  netlist_id;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate literals (used to be different from 'Lit'):


typedef Lit       GLit;
typedef Lit_union GLit_union;
typedef Lit_data  GLit_data;

template <bool sign_matters> struct MkIndex_GLit : MkIndex_Lit<sign_matters> {};
        // -- If sign doesn't matter, just use ID. With sign, use 'ID*2 + sign' to get the order: x0, ~x0, x1, ~x1, ...

#if (id_NULL != 0)
#error "Inconsistent definition of id_NULL and gid_NULL"
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Forward declarations:


class Wire;
struct Netlist;
struct Pec;
struct NlLis;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constants:


enum {
    nid_NULL = (UINT_MAX),
};

enum {
    gid_NULL       = (0),
    gid_ERROR      = (1),
    gid_Unbound    = (2),
    gid_Conflict   = (3),
    gid_False      = (4),
    gid_True       = (5),
    gid_MAX        = (0x7FFFFFFF),
    gid_FirstLegal = (2),
    gid_FirstUser  = (6),
};

#if defined(ZZ_CONSTANTS_AS_MACROS)
    #define glit_NULL      GLit(::ZZ::gid_NULL    , false)
    #define glit_ERROR     GLit(::ZZ::gid_ERROR   , false)
    #define glit_Unbound   GLit(::ZZ::gid_Unbound , false)
    #define glit_Conflict  GLit(::ZZ::gid_Conflict, false)
    #define glit_False     GLit(::ZZ::gid_False   , false)
    #define glit_True      GLit(::ZZ::gid_True    , false)
    #define glit_MAX       GLit(::ZZ::gid_MAX     , false)
#else
    static const GLit glit_NULL     = GLit(gid_NULL    , false);
    static const GLit glit_ERROR    = GLit(gid_ERROR   , false);
    static const GLit glit_Unbound  = GLit(gid_Unbound , false);
    static const GLit glit_Conflict = GLit(gid_Conflict, false);
    static const GLit glit_False    = GLit(gid_False   , false);
    static const GLit glit_True     = GLit(gid_True    , false);
    static const GLit glit_MAX      = GLit(gid_MAX     , false);
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
