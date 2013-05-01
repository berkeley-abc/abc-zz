//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BasicTypes.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Supporting types and constants for the netlist representation.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__BasicTypes_hh
#define ZZ__Gig__BasicTypes_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compile-time parameters:


#define ZZ_GIG_PAGE_SIZE_LOG2 12
#define ZZ_GIG_PAGE_SIZE      (1 << ZZ_GIG_PAGE_SIZE_LOG2)


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Typedefs:


typedef uint gate_id;   // -- currently, only 31 bits must be used (top-bit must be zero)
typedef uint pec_id;    // -- 'Pec' is the class.
typedef uint pob_id;    // -- 'Pob' is the object (instance of a 'Pec').
typedef Lit  GLit;      // -- typedef for documentation rather than abstraction


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Constants:


// Hate to use defines for this, but GCC just won't generate the right code otherwise....
#define gid_NULL       gate_id(0)
#define gid_ERROR      gate_id(1)
#define gid_Unbound    gate_id(2)
#define gid_Conflict   gate_id(3)
#define gid_False      gate_id(4)
#define gid_True       gate_id(5)
#define gid_Reset      gate_id(6)
#define gid_Reserved   gate_id(7)
#define gid_MAX        gate_id(0x7FFFFFFF)
#define gid_FirstLegal gate_id(2)
#define gid_FirstUser  gate_id(8)

#define GLit_NULL      GLit(gid_NULL)
#define GLit_ERROR     GLit(gid_ERROR)
#define GLit_Unbound   GLit(gid_Unbound)
#define GLit_Conflict  GLit(gid_Conflict)
#define GLit_False     GLit(gid_False)
#define GLit_True      GLit(gid_True)
#define GLit_Reset     GLit(gid_Reset)
#define GLit_Reserved  GLit(gid_Reserved)
#define GLit_MAX       GLit(gid_MAX)

#define ZZ_GLIT_NULL_GATES 3        // NULL, ERROR, Reserved


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
