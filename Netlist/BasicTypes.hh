//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BasicTypes.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Defines some basic types of the 'Netlist' ADT.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
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


struct Wire;
struct Netlist;
struct Pec;
struct NlLis;

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constants:


#if defined(ZZ_CONSTANTS_AS_MACROS)
    // Do these as macros for efficiency (why does 'static const' *still* generate worse code??)
    #define nid_NULL       ::ZZ::netlist_id(UINT_MAX)

    #define gid_NULL       ::ZZ::gate_id(0)
    #define gid_ERROR      ::ZZ::gate_id(1)
    #define gid_Unbound    ::ZZ::gate_id(2)   // -- {0,1}, use this constant value for 'X' in 3-valued simulation
    #define gid_Conflict   ::ZZ::gate_id(3)   // -- {}, constant for "no satisfying value"
    #define gid_False      ::ZZ::gate_id(4)   // -- {0}, constant FALSE
    #define gid_True       ::ZZ::gate_id(5)   // -- {1}, constant TRUE
    #define gid_MAX        ::ZZ::gate_id(0x7FFFFFFF)

    #define gid_FirstLegal ::ZZ::gate_id(2)   // -- You must not dereference wires pointing to NULL or ERROR.
    #define gid_FirstUser  ::ZZ::gate_id(6)   // -- Real user gates start at this ID.

    #define glit_NULL      ::ZZ::GLit(gid_NULL    , false)
    #define glit_ERROR     ::ZZ::GLit(gid_ERROR   , false)
    #define glit_Unbound   ::ZZ::GLit(gid_Unbound , false)
    #define glit_Conflict  ::ZZ::GLit(gid_Conflict, false)
    #define glit_False     ::ZZ::GLit(gid_False   , false)
    #define glit_True      ::ZZ::GLit(gid_True    , false)
    #define glit_MAX       ::ZZ::GLit(gid_MAX     , false)

#else
    static const netlist_id nid_NULL = netlist_id(UINT_MAX);

    static const gate_id gid_NULL     = gate_id(0);
    static const gate_id gid_ERROR    = gate_id(1);
    static const gate_id gid_Unbound  = gate_id(2);
    static const gate_id gid_Conflict = gate_id(3);
    static const gate_id gid_False    = gate_id(4);
    static const gate_id gid_True     = gate_id(5);
    static const gate_id gid_MAX      = gate_id(0x7FFFFFFF);

    static const gate_id gid_FirstLegal = gate_id(2);
    static const gate_id gid_FirstUser  = gate_id(6);

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
