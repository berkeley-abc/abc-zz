//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MiniSat.hh
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : MiniSat v1.16 for the ZZ framework.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|________________________________________________________________________________________________

#ifndef ZZ__MiniSat__MiniSat_h
#define ZZ__MiniSat__MiniSat_h

#include "ZZ/Generics/IntSet.hh"
#include "ZZ/Generics/IdHeap.hh"
#include "SatTypes.hh"
#include "Proof.hh"

//#define BUMP_EXPERIMENT

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// MiniSat -- SAT Solver:


template<bool pfl>
class MiniSat {
protected:
  //________________________________________
  //  INTERNAL STATE:

    // Clauses:
    Vec<Lit>        mem_lits;       // Vector of all literals and other clause data (offsets into this vector are used as "pointers").
    uint            n_free_lits;    // How many unused slots are in 'mem_lits'? Controls garbage collection.
    bool            ok;             // If FALSE, the constraints are already unsatisfiable. No part of the solver state may be used!
    Vec<GClause>    clauses;        // List of problem clauses. No literal GClauses.
    Vec<GClause>    learnts;        // List of learnt clauses. No literal GClauses.
    Vec<clause_id>  unit_id;        // In proof-logging mode: the clause IDs for unit literals.
    int             n_bin_clauses;  // }- Keep track of number of binary clauses "inlined" into the watcher lists
    int             n_bin_learnts;  // }  (we do this primarily to get identical behavior to the version without the binary clauses trick).
    double          cla_inc;        // Amount to bump next clause with.
    double          cla_decay;      // INVERSE decay factor for clause activity: stores 1/decay.

    // Decision heuristic:
#if defined(BUMP_EXPERIMENT)
    Vec<uint>       var_count;
#endif
    Vec<double>     activity;       // A heuristic measurement of the activity of a variable.
    Vec<uchar>      polarity;       // Which polarity should we branch on first?
    double          var_inc;        // Amount to bump next variable with.
    double          var_decay;      // INVERSE decay factor for variable activity: stores 1/decay. Use negative value for static variable order.
    IdHeap<double,1>order;          // Keeps track of the decision variable order.
    IdHeap<double,1>order_main;     // When in filtered mode, the main heap is stored here.
    IntZet<Var>*    enabled_vars;   // Solving temporary restricted to these variables.
    uint64          random_seed;    // For the internal random number generator
    Vec<Lit>        assumps;        // Assumptions for this SAT run

    // Variable data:
#if defined(EXPERIMENTAL)
    Vec<lbool> assign_;
#endif
    Vec<MSVarData>  vdata;
    GClause reason    (Var x) const      { return vdata[x].reason; }
    GClause reason    (Lit p) const      { return reason(p.id); }
    void    reason_set(Var x, GClause v) { vdata[x].reason = v; }
    void    reason_set(Lit p, GClause v) { reason_set(p.id, v); }
    uint    level     (Var x) const      { return vdata[x].level ; }
    uint    level     (Lit p) const      { return level(p.id); }
    void    level_set (Var x, uint v)    { vdata[x].level  = v; }
    void    level_set (Lit p, uint v)    { level_set(p.id, v); }
#if defined(EXPERIMENTAL)
    lbool   assign    (Var x) const      { return assign_[x]; }
    lbool   assign    (Lit p) const      { return assign_[p.id] ^ sign(p); }
    void    assign_set(Var x, lbool v)   { assign_[x] = v; }
    void    assign_set(Lit p, lbool v)   { assign_[p.id] = v ^ sign(p); }
#else
    lbool   assign    (Var x) const      { return lbool_new(vdata[x].assign); }
    lbool   assign    (Lit p) const      { return assign(p.id) ^ p.sign; }
    void    assign_set(Var x, lbool v)   { vdata[x].assign = v.value; }
    void    assign_set(Lit p, lbool v)   { assign_set(p.id, v ^ p.sign); }
#endif

    IntZet<Var>     free_vars;

    // BCP:
    Vec<WHead>      watches;        // 'watches[lit]' is a list of constraints watching 'lit' (will go there if literal becomes true).
    Vec<Lit>        trail;          // Assignment stack; stores all assigments made in the order they were made.
    Vec<int>        trail_lim;      // Separator indices for different decision levels in 'trail'.
    int             qhead;          // Head of queue (as index into the trail -- no more explicit propagation queue in MiniSat).
    uint            simpDB_assigns; // Number of top-level assignments since last execution of 'simplifyDB()'.
    int64           simpDB_props;   // Remaining number of propagations that must be made before next execution of 'simplifyDB()'.
    uint            n_literals;     // Total number of literals in all clauses. Used to seed 'simpDB_props'.

    // Proof-logging:
    Proof           proof;

    // Statistics:
    uint64          vt;             // Virtual time. When reach 'timeout', 'timeout_cb' is called (and 'vt' reset).
    SatStats        stats;          // Statistics about solving process.
    double          cpu_time0;      // CPU time at creation of solver object.

    // Temporaries:
    typedef IntTmpMap<Lit, uchar, MkIndex_Lit<false> > VarAttr;
    struct AnalyzeRec {
        Lit     p0;
        uint    i;      // -- bit31 stores boolean 'quit' in 'analyze_removable()'.
        Clause* c;
        AnalyzeRec(Lit p0_, uint i_, Clause* c_) : p0(p0_), i(i_), c(c_) {}
    };

    IntZet<uint>    analyze_levels;
    VarAttr         analyze_visit;
    Vec<Lit>        analyze_result;
    Vec<Lit>        analyze_order;
    Vec<AnalyzeRec> analyze_stack;
    GClause         propagate_tmpbin;
    GClause         analyze_tmpbin;
    GClause         solve_tmpunit;
    Vec<Lit>        cl_tmp;

  //________________________________________
  //  INTERNAL HELPERS:

    // Watcher list management:
    //
    Array<GClause> wlGet   (Lit p);
    void           wlAdd   (Lit p, GClause c);
    void           wlShrink(Lit p, uint new_size);
    void           wlPop   (Lit p);
    void           wlClear (Lit p);
    bool           wlRemove(Lit p, GClause c);
    void           wlDisposeAll();

    // Clause memory management:
    //
    GClause   allocClause(bool learnt, const Vec<Lit>& ps);
    void      freeClause(GClause c);
    void      compactClauses();

    // Main internal methods:
    //
    void      init             ();
    Var       newVar           ();
    bool      assume           (Lit p);
    void      undo             (uint level);

    clause_id analyze          (Clause* confl, Vec<Lit>& out_learnt);
    bool      analyze_removable(Lit p0, const IntZet<uint>& levels, VarAttr& visit, Vec<Lit>& result);
    void      analyze_logOrder (Lit p0, VarAttr& visit, Vec<Lit>& order);
    void      analyzeFinal     (const Clause& confl, bool skip_first = false);
    bool      enqueue          (Lit fact, GClause from = GClause_NULL);
    Clause*   propagate        ();
    void      reduceDB         ();
    bool      makeDecision     ();
    lbool     search           (int nof_conflicts, int nof_learnts);
    lbool     solve_           (const Vec<Lit>& assumps);

    void      printProgressHeader() const;
    void      printProgressFooter() const;
    void      printProgressLine(bool newline = true) const;

    // Activity:
    //
    void varDecayActivity  () { if (var_decay >= 0) var_inc *= var_decay; }
    void varRescaleActivity();
    void claDecayActivity  () { cla_inc *= cla_decay; }
    void claRescaleActivity();

    // Operations on clauses:
    //
    void backtrack        (Vec<Lit>& ps);
    void newClause        (const Vec<Lit>& ps, clause_id id = clause_id_NULL);
    void claBumpActivity  (Clause* c) { if ( (c->activity() += cla_inc) > 1e20 ) claRescaleActivity(); }
    void removeClause     (GClause c, bool just_dealloc = false, bool remove_watches = true, bool deref_proof = true);
    bool locked           (GClause c) const { GClause r = reason((*c.clause(mem_lits.base()))[0].id); return !r.isLit() && r == c; }
    bool simplifyClause   (GClause c) const;
    void simplifyDB_intern();

    uint dl() const { return trail_lim.size(); }    // -- decision level

public:
  //________________________________________
  //  PUBLIC INTERFACE:

    // Construction:
    //
    MiniSat(ProofIter* iter) : proof(iter ) { assert(pfl || iter == NULL); debug_api_out = NULL; init(); }
    MiniSat(ProofIter& iter) : proof(&iter) { assert(pfl);                 debug_api_out = NULL; init(); }
    MiniSat()                : proof(NULL ) { assert(!pfl);                debug_api_out = NULL; init(); }
   ~MiniSat() { wlDisposeAll(); }

    void clear(bool dealloc = true, bool clear_stats = false);

    // Statistics: (read-only member variable)
    //
    const SatStats& statistics() const { return stats; }

    uint  nAssigns() const { return trail.size(); }
    uint  nClauses() const { return clauses.size() + n_bin_clauses; }
    uint  nBinary () const { return n_bin_clauses; }
    uint  nLearnts() const { return learnts.size() - (pfl ? n_bin_clauses : 0); }

    // Variable activity:
    //
    void   varBumpActivity(Lit p);
    void   varBumpActivity(Lit p, double amount);
    double varActivity(Lit p) { return activity[p.id] / var_inc; }

    // Mode of operation:
    //
    void setFilter(IntZet<Var>& enabled_vars);
    void clearFilter();         // -- must call this before setting a new filter

    double   variable_decay;
    double   clause_decay;
    double   random_var_freq;
    uint     verbosity;         // -- Verbosity level. 0=silent, 1=report progress

    uint64   timeout;           // -- "time" is not actual time but number of literals inspected during propagation (for deterministic behavior)
    SatCB    timeout_cb;        // -- Callback function. Should return TRUE for continued SAT solving.
    void*    timeout_cb_data;   // -- Passed untouched to 'timeout_cb'.

        // -- When time-out is reached, the callback (if defined) is asked whether solving should
        // continue or not. If it says "yes" (returns TRUE) another call will be issued after
        // another 'timeout' has passed. If callback is undefined, it is considered a FALSE.

    SatCcCB  cc_cb;             // -- Set this variable to receive call-backs for each learned conflict clause
    void*    cc_cb_data;        // -- Data passed to conflict clause call-back.

    // Problem specification:
    //
    Var  addVars(uint n_consecutive);       // -- Will not use recycled variables. Currently returns '2' on first call, but this may change.
    Var  addVar();                          // -- Uses recycled variables (from 'removeVars()').
    Lit  addLit() { return Lit(addVar()); }
    uint nVars () const { return vdata.size(); }    // -- Will count recycled variables as still in use
    uint varCount() const { return nVars() - free_vars.size(); }
    Lit  True() const { return Lit(var_True); }

    void addClause(Lit p) { cl_tmp.setSize(1, lit_Undef); cl_tmp[0] = p; addClause(cl_tmp); }
    void addClause(Lit p, Lit q) { cl_tmp.setSize(2, lit_Undef); cl_tmp[0] = p; cl_tmp[1] = q; addClause(cl_tmp); }
    void addClause(Lit p, Lit q, Lit r) { cl_tmp.setSize(3, lit_Undef); cl_tmp[0] = p; cl_tmp[1] = q; cl_tmp[2] = r; addClause(cl_tmp); }
    void addClause(Lit p, Lit q, Lit r, Lit s) { cl_tmp.setSize(4, lit_Undef); cl_tmp[0] = p; cl_tmp[1] = q; cl_tmp[2] = r; cl_tmp[3] = s; addClause(cl_tmp); }
    void addClause(const Vec<Lit>& ps);
    void addClause(String text_lits);

    void removeVars(const Vec<Var>& xs, /*out*/Vec<Var>& xs_kept);
    void removeVars(IntZet<Var>& xs   , /*out*/Vec<Var>& xs_kept);
        // -- remove all clauses containing at least one variable in 'xs'. NOTE! That clause set
        // must be independently satisfiable from the remaining clauses (unsound otherwise).
    bool hasVar(Var x) { return x < nVars() && !free_vars.has(x); }

    // Solving:
    //
    bool  okay() const { return ok; }     // -- FALSE means solver is in an conflicting state (must never be used again!)
    void  simplifyDB();
    lbool solve(const Vec<Lit>& assumps);
    lbool solve()             { Vec<Lit> tmp; return solve(tmp); }
    lbool solve(Lit p)        { Vec<Lit> tmp; tmp.push(p); return solve(tmp); }
    lbool solve(Lit p, Lit q) { Vec<Lit> tmp; tmp.push(p); tmp.push(q); return solve(tmp); }
    lbool solve(String text_assumps);

    lbool value(Var x) const { return assign(x); }
    lbool value(Lit p) const { return assign(p); }
    void  getModel(Vec<lbool>& m) const { m.setSize(nVars()); for (Var x = 0; x < nVars(); x++) m[x] = value(x); }
        // -- If 'solver()' returns SAT, you read the model using these methods.
        // NOTE! You cannot add clauses and then read the model afterwards (some variable will be unbound).
        // NOTE! Variables not occuring in any clause will be 'l_Undef' (you may want to change them into 'l_False')
    void  getConflict(Vec<Lit>& assump_confl) { conflict.copyTo(assump_confl); }
        // -- Harmonize interface with 'MetaSat'.

    double getActivity(uint x) { return activity[x] / var_inc; }

    lbool topValue(Var x) const { return (level(x) == 0) ? assign(x) : l_Undef; }
    lbool topValue(Lit p) const { return (level(p) == 0) ? assign(p) : l_Undef; }
        // If 'x' has been proven (at the top-level) to be constant, return that value.

    Vec<Lit>  conflict;     // -- A cube (not a clause): If 'solve()' returned 'l_False', contains the assumption literals used.
    clause_id conflict_id;  // -- In proof-logging mode, the ID of the final conflict clause (the negation of the conjunction 'conflict'). Subsequent calls to 'solve()' may recycle this ID.

    void  proofTraverse    () { proof.iterate(conflict_id); }
    void  proofClearVisited() { proof.clearVisited(); }

    void  randomizeVarOrder(uint64& seed, bool rnd_polarity = true);
    void  clearLearnts() {
        stats.deleted_clauses -= learnts.size();
        for (uint i = 0; i < learnts.size(); i++) removeClause(learnts[i]);
        learnts.clear();
        compactClauses(); }

    // Snapshots:
    //
    void copyTo(MiniSat<pfl>& other) const;
    void moveTo(MiniSat<pfl>& other);
    void mergeStatsFrom(MiniSat& other);

    // Debug:
    //
    void exportCnf(Out&   out     , bool exclude_units = true);
    void exportCnf(String filename, bool exclude_units = true) { OutFile out(filename); exportCnf(out, exclude_units); }

    void dumpState();

    Out* debug_cnf_out;     // -- If set, 'addClause()' will echo all orignal clauses to this stream.
    Out* debug_api_out;     // -- If set, all non-const external API calls will be logged to this stream.
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Inlined member functions:

template<bool pfl>
inline void MiniSat<pfl>::varBumpActivity(Lit p)
{
    if (var_decay < 0) return;     // -- negative decay means static variable order -- don't bump
#if defined(BUMP_EXPERIMENT)
    if ( (activity[p.id] += var_inc * var_count[p.id]) > 1e100 ) varRescaleActivity();
#else
    if ( (activity[p.id] += var_inc) > 1e100 ) varRescaleActivity();
#endif
    if (order.has(p.id)) order.update(p.id);
    if (enabled_vars && order_main.has(p.id)) order_main.update(p.id);
}


template<bool pfl>
inline void MiniSat<pfl>::varBumpActivity(Lit p, double amount)
{
    if (var_decay < 0) return;
    if ( (activity[p.id] += var_inc * amount) > 1e100 ) varRescaleActivity();
    if (order.has(p.id)) order.update(p.id);
    if (enabled_vars && order_main.has(p.id)) order_main.update(p.id);
}


template<bool pfl>
inline Var MiniSat<pfl>::addVars(uint n_consecutive)
{
    if (debug_api_out)
        *debug_api_out |= "addVars(%_)", n_consecutive;

    Var ret = nVars();
    for(; n_consecutive != 0; n_consecutive--)
        newVar();
    return ret;
}


template<bool pfl>
inline Var MiniSat<pfl>::addVar()
{
    if (debug_api_out)
        *debug_api_out &= "addVar()";

    Var ret;
#if 1   // <<== turn of variable recycling temporarily (debugging)
    if (free_vars.size() > 0)
        ret = free_vars.popLastC();
#endif
    {
        ret = nVars();
        newVar();
    }

    if (debug_api_out)
        *debug_api_out |= " # x%_", (uint)ret;

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Instantiations:


typedef MiniSat<false> SatStd;
typedef MiniSat<true>  SatPfl;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
