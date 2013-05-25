//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Macros.hh
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Defines convenient netlist iteration macros.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Netlist iteration:
//| 
//|   For_Gates(N, w)           -- Iterate over all "user" gates (skipping constants)
//|   For_All_Gates(N, w)       -- Iterate over all legal gates (skipping only Wire_NULL/ERROR)
//|   For_Gatetype(N, type, w)  -- Iterate over all gates of given type (must be a "numbered" type)  
//| 
//|   For_UpOrder(netlist, w)
//| 
//| Fanin iteration:
//| 
//|   For_Inputs(wire, w) 
//|   Input_Pin(w) Iter_Var(w)
//| 
//| Gig objects, expression macros:
//| 
//|   Has_Gob   (N, obj_name)   -- checks if netlist has an object
//|   Add_Gob   (N, obj_name)   -- creates an object that must NOT already exist.
//|   Assure_Gob(N, obj_name)   -- creates an object IF it does not already exist.
//|   Remove_Gob(N, obj_name)   -- removes an object from the netlist
//| 
//| Gig objects, statement macros:
//| 
//|   Scoped_Gob(N, obj_name)   -- 'Add' with automatic 'Remove' at the end of the scope
//|   Auto_Gob  (N, obj_name)   -- 'Assure' with conditional 'Remove' at the end of the scope
//|   Bury_Gob  (N, obj_name)   -- Inverse of 'Auto'; remove object if exists, then reintroduce 
//|                                it at the end of the scope
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__Gig_Macros_hh
#define ZZ__Gig__Gig_Macros_hh

#include "Gig.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// All gates of the netlist:


template<class GIG>
struct Gig_Iter {
    GIG& N;
    uint idx;

    Gig_Iter(GIG& N_, uint idx_) : N(N_), idx(idx_) {}

    operator bool() const { return false; }
};


// Iterate over all non-deleted gates, starting from either 'gid_FirstUser' (i.e. excluding 
// constants) or 'gid_FirstLegal (excluding only 'Wire_NULL' and 'Wire_ERROR').
// NOTE! 'netlist' is used as an expression, 'w' as a name
//
#define For_Gates_(netlist, w, start)                                               \
    if (Gig_Iter<const Gig> i__##w = Gig_Iter<const Gig>((netlist), (start))); else \
    if (Wire w = Wire_NULL); else                                                   \
    for (; i__##w.idx < i__##w.N.size(); i__##w.idx++)                              \
        if ((w = i__##w.N[i__##w.idx]), w.isRemoved()); else

#define For_Gates(netlist, w)     For_Gates_((netlist), w, gid_FirstUser)
#define For_All_Gates(netlist, w) For_Gates_((netlist), w, gid_FirstLegal)


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// All gates of a particular type (must be of type 'attr_Enum'):


template<class GIG>
struct Gig_IterType {
    GIG&     N;
    GateType type;
    uint     idx;
    gate_id  id;

    Gig_IterType(GIG& N_, GateType type_) : N(N_), type(type_), idx(0) {}

    operator bool() const { return false; }
};


// Iterate over all gates of a given NUMBERED type (e.g. PIs, POs, FFs; not ANDs or LUTs).
// NOTE! 'netlist' and 'gate_type' are used as expressions, but 'w' as a name.
//
#define For_Gatetype(netlist, gate_type, w)                                                        \
    if (Gig_IterType<const Gig> i__##w = Gig_IterType<const Gig>((netlist), (gate_type))); else    \
    if (Wire w = Wire_NULL); else                                                                  \
    for (; i__##w.idx < i__##w.N.type_list[i__##w.type].size(); i__##w.idx++)                      \
        if (i__##w.id = i__##w.N.type_list[i__##w.type][i__##w.idx], i__##w.id == gid_NULL); else  \
        if (w = i__##w.N[i__##w.id], assert_debug(!w.isRemoved()), false); else


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Inputs of a wire (this macro is optimized to generate good code with GCC):


// For some reason, GCC is really bad at optimizing out 'Wire::null()' tests that should be
// known compile time. This is a work around.
struct MacroWire {
    Wire w;
    MacroWire(uint) : w(Wire()) {}
    operator bool() const { return false; }
    operator Wire&() { return w; }
};


// Iterate over all non-NULL inputs of a given gate 'wire'.
// NOTE 'wire' is an expression, but 'w' is a name.
//
#define For_Inputs(wire, w)                                     \
    if (MacroWire w0__##w = 0); else                            \
    if ((Wire&)w0__##w = (wire), false); else                   \
                                                                \
    if (const Gig* N__##w = NULL); else                         \
    if (N__##w = ((Wire&)w0__##w).gig(), false); else           \
                                                                \
    if (Array<const GLit> a__##w = Array<const GLit>()); else   \
    if (a__##w = ((Wire&)w0__##w).fanins(), false); else        \
                                                                \
    if (uint i__##w = 0); else                                  \
    for (Wire w; i__##w < a__##w.size(); i__##w++)              \
        if (a__##w[i__##w].null()); else                        \
        if (w = (*N__##w)[ a__##w[i__##w] ], false); else


#define Input_Pin(w) Iter_Var(w)
    // -- apply this to the name used for 'w' in 'For_Inputs' to get the pin number
    // of the current input wire.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Topological order:


// We need a 'Vec<GLit>' that is considered FALSE for the if statement in the macro.
struct MacroVecGLit {
    Vec<GLit> v;
    operator bool() const { return false; }
    MacroVecGLit(int) {}
    MacroVecGLit(const MacroVecGLit&) {}
};

#define For_UpOrder(netlist, w)                                     \
    if (const Gig* N__##w = NULL); else                             \
    if (N__##w = &(netlist), false); else                           \
                                                                    \
    if (MacroVecGLit order__##w = 0); else                          \
    if (upOrder(*N__##w, order__##w.v), false); else                \
                                                                    \
    if (Wire w = Wire_NULL); else                                   \
    for (uint i__##w = 0; i__##w < order__##w.v.size(); i__##w++)   \
        if ((w = order__##w.v[i__##w] + *N__##w), false); else

#define For_DownOrder(netlist, w)                                   \
    if (const Gig* N__##w = NULL); else                             \
    if (N__##w = &(netlist), false); else                           \
                                                                    \
    if (MacroVecGLit order__##w = 0); else                          \
    if (upOrder(*N__##w, order__##w.v), false); else                \
                                                                    \
    if (Wire w = Wire_NULL); else                                   \
    for (uint i__##w = order__##w.v.size(); i__##w > 0;)            \
        if ((w = order__##w.v[--i__##w] + *N__##w), false); else


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Convenience macros for adding/removing Gig-objects:


#define Has_Gob(   N, obj_name) ((N).hasObj(gigobj_##obj_name))
#define Add_Gob(   N, obj_name) ((N).addObj(gigobj_##obj_name))
#define Remove_Gob(N, obj_name) ((N).removeObj(gigobj_##obj_name))
#define Assure_Gob(N, obj_name) (Has_Gob((N), obj_name) || (Add_Gob((N), obj_name), false))


struct GigObjDisposer {
    Gig&       N;
    GigObjType obj_type;
    bool       free;
    GigObjDisposer(Gig& N_, GigObjType obj_type_, bool free_) : N(N_), obj_type(obj_type_), free(free_) {}
   ~GigObjDisposer() { if (free) N.removeObj(obj_type); }
};


struct GigObjReviver {
    Gig&       N;
    GigObjType obj_type;
    bool       revive;
    GigObjReviver(Gig& N_, GigObjType obj_type_, bool revive_) : N(N_), obj_type(obj_type_), revive(revive_) {}
   ~GigObjReviver() { if (revive) N.addObj(obj_type); }
};


#define Scoped_Gob(N, obj_name)                                                 \
    GigObjDisposer gigobj_disposer_##obj_name((N), gigobj_##obj_name, true);    \
    Add_Gob(N, obj_name);

#define Auto_Gob(N, obj_name)                                                                   \
    GigObjDisposer gigobj_disposer_##obj_name((N), gigobj_##obj_name, !Has_Gob(N, obj_name));   \
    Assure_Gob(N, obj_name);

#define Bury_Gob(N, obj_name)                                                                   \
    GigObjReviver gigobj_revivier_##obj_name((N), gigobj_##obj_name, Has_Gob(N, obj_name));     \
    if (Has_Gob(N, obj_name)) Remove_Gob(N, obj_name);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
