//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : SatTypes.hh
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : Basic types for the SAT solver.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________


#ifndef ZZ__MiniSat__SatTypes_h
#define ZZ__MiniSat__SatTypes_h

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;

//#define EXPERIMENTAL


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Exceptions:


Declare_Exception(Excp_SatMemOut);
Declare_Exception(Excp_SatInvalidString);   // -- Thrown by text interface.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// SAT Statistics:


struct SatStats {
    // General statistics:
    uint64  starts, decisions, propagations, conflicts, inspections, random_decis;
    uint64  clauses_literals, learnts_literals, max_literals, tot_literals, deleted_clauses;
    uint64  stuck_vars, stuck_clauses;
    // Incremental SAT statistics:
    uint64  solves, solves_sat, solves_unsat;
    uint64  inspections_sat, inspections_unsat;
    double  time, time_sat, time_unsat;

    void clear() {
        starts = decisions = propagations = conflicts = inspections = 0, random_decis = 0;
        clauses_literals = learnts_literals = max_literals = tot_literals = deleted_clauses = 0;
        stuck_vars = stuck_clauses = 0;
        solves = solves_sat = solves_unsat = 0;
        inspections_sat = inspections_unsat = 0;
        time = time_sat = time_unsat = 0.0;
    }

    SatStats() { clear(); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Variables, literals, clause IDs:


// NOTE! Variables are just integers. No abstraction here. They should be chosen from 0..N,
// so that they can be used as array indices.

typedef uint Var;
const Var var_Undef     = 0u;     // -- index 0 is reserved for the NULL var
const Var var_True      = 1u;     // -- index 1 is reserved for a variable that is always TRUE in the solver
const Var var_FirstUser = 2u;     // -- first variable index that can be used by user
const Var var_Max = (1u << 28)-1; // -- highest variable index that can be used.

const Lit lit_Undef(var_Undef, false);  // }
const Lit lit_Error(var_Undef, true );  // }- Useful special constants.
const Lit lit_Free (id_MAX   , false);  // }

inline int toDimacs(Lit p) { assert(p.id != 0); return p.sign ? -p.id : p.id; }

macro Var var (Lit p) { return id(p); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clause -- a simple class for representing a clause:


typedef uint clause_id;
static const clause_id clause_id_NULL = (UINT_MAX >> 1);
static const clause_id clause_id_FLAG = (1u << 31);     // -- steal one bit for marking clauses


struct Clause {
    union { Lit_data lit; float act; uint head; uint num; } data[1];

    uint      size      ()       const { return data[0].head >> 2; }        // }
    bool      learnt    ()       const { return data[0].head  & 2; }        // }- 'allocClause()' relies on this representation
    bool      tag       ()       const { return data[0].head  & 1; }        // }
    void      tag_set   (bool v)       { data[0].head &= ~1u; data[0].head |= uint(v); }
    Lit       operator[](uint i) const { return (Lit&)data[i+1].lit; }
    Lit&      operator[](uint i)       { return (Lit&)data[i+1].lit; }
    float&    activity  ()             { return data[size() + 1].act; }
    uint&     id        ()             { return data[size() + 1 + uint(learnt())].num; }
    uint      id        ()       const { return data[size() + 1 + uint(learnt())].num; }
};


template<> fts_macro void write_(Out& out, const Clause& c) {
    write_(out, slice(((Lit*)&c)[1], ((Lit*)&c)[c.size() + 1])); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GClause -- Generalize clause:


// Either a pointer to a clause or a literal.
struct GClause_data {
    friend struct WHead;
    uint data;
};

class GClause : public GClause_data {
    GClause(uint d) { data = d; }
public:
    GClause() { data = 0; }
    GClause(const GClause& other) { data = other.data; }
    GClause& operator=(const GClause& other) { data = other.data; return *this; }

    friend GClause GClause_new(Lit p);
    friend GClause GClause_new(uint offset);
    friend GClause GClause_newBLit(Lit p);

    bool    isLit     ()                const { return (data & 3) == 1; }
    bool    isBLit    ()                const { return (data & 3) == 3; }
    Lit     lit       ()                const { return Lit(packed_, ((uint)data) >> 2); }
    Clause* clause    (const Lit* base) const { return (Clause*)(base + data); }
    uint    offset    ()                const { return data; }      // -- low-level (used in compaction)
    bool    operator==(GClause c)       const { return data == c.data; }
    bool    operator!=(GClause c)       const { return data != c.data; }
};

inline GClause GClause_new(Lit p) {
    return GClause((uint(p.data()) << 2) + 1); }

inline GClause GClause_newBLit(Lit p) {
    return GClause((uint(p.data()) << 2) + 3); }

inline GClause GClause_new(uint offset) {
    assert((offset & 1) == 0);
    return GClause(offset); }

inline GClause GClause_new(Clause* c, Lit* base) {
  #if defined(ZZ_LP64)
    assert(uintp(c) - uintp(base) <= UINT_MAX);
  #endif
    return GClause_new((Lit*)c - base); }

#define GClause_NULL GClause_new((uint)0)


template<> fts_macro void write_(Out& out, const GClause& c) {
    if (c.isLit()) out += "GClause_lit[", c.lit(), ']';
    else           out += "GClause_offset[", c.offset(), ']'; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// WHead -- Watcher list header:


struct WHeadInl {
    GClause_data data[3];
    uint         sz;
};

struct WHeadExt {
    GClause* data;
    uint     cap;
    // "uint sz" -- read thru 'WHeadInl' interface
};

struct WHead {
    union {
        WHeadInl inl_;
        WHeadExt ext_;
    };

    static const uint lim = 3;

    uint&     size () { return inl_.sz; }
    bool      isExt() { return size() > lim; }
    uint&     cap  () { return ext_.cap; }    // -- must only be used in external mode
    GClause*  inl  () { return (GClause*)&inl_.data[0]; }
    GClause*& ext  () { return ext_.data; }
    GClause*  get  () { return (size() < 4) ? inl() : ext(); }

    WHead() { inl_.sz = 0; }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Variable data:


#define MS_MAX_LEVEL (UINT_MAX >> 2)


struct MSVarData {
    GClause reason;
#if defined(EXPERIMENTAL)
    uint    level;
    MSVarData(GClause reason_, uint level_) : reason(reason_), level(level_) {}
    MSVarData() : reason(GClause_NULL), level(MS_MAX_LEVEL) {}
#else
    uchar   assign : 2;     // <<== move out?
    uint    level  : 30;
    MSVarData(GClause reason_, uint level_, lbool assign_) : reason(reason_), assign(assign_.value), level(level_) {}
    MSVarData() : reason(GClause_NULL), assign(l_Undef.value), level(MS_MAX_LEVEL) {}
#endif
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Callback function:


typedef bool (*SatCB)(uint64 work, void* data);
    // -- 'work' is the number of implications ("inspects") that has happened since last call
    // to the callback.

typedef void (*SatCcCB)(Vec<Lit>& learned_clause, void* data);
    // -- Callback for learned (conflict) clauses. Can be used as a "poor-man's" proof-logging,
    // or to use domain specific knowledge to minimize learned clauses further by updating
    // the 'learned_clause' argument (NOT in proof-logging mode though!).


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
