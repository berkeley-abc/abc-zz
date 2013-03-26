//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MiniSat.cc
//| Author(s)   : Niklas Een
//| Module      : MiniSat
//| Description : MiniSat v1.16 for the ZZ framework.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//| This is an improved branch-off of MiniSat v1.14. It has improved memory foot-print, templetized
//| proof-logging, supports variable deletetion (for better incremental SAT) and has more modern
//| restart and polarity heuristics.
//|________________________________________________________________________________________________


/*
TODO:

- Migrate luby restarts into 'search()' (as well as output I suppose) -- make the 'undo(0)' behave
  correctly over multiple short, incremental SAT problems.

*/

#include "Prelude.hh"

#include "MiniSat.hh"
#include "ZZ/Generics/Sort.hh"


#define BLOCKING_LITERALS
//#define KEEP_TOPLEVEL_LITERALS      // <<== experimental! Currently slightly incorrect (what to do with {x}, {~x}?)


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Init / Clear:


template<bool pfl>
void MiniSat<pfl>::init()
{
    // Private state:
    mem_lits.push(lit_Undef);
    mem_lits.push(lit_Undef);    // -- must not use offset 0 of 'mem_lits'
    n_free_lits       = 0;
    ok                = true;
    n_bin_clauses     = 0;
    n_bin_learnts     = 0;
    cla_inc           = 1;
    cla_decay         = 1;
    var_inc           = 1;
    var_decay         = 1;
    order.prio        = &activity;
    enabled_vars      = NULL;
    random_seed       = DEFAULT_SEED;
    qhead             = 0;
    simpDB_assigns    = 0;
    simpDB_props      = 0;
    n_literals        = 0;
    vt                = 0;
    cpu_time0         = cpuTime();

    // Public state:
    variable_decay    = 0.95;
    clause_decay      = 0.999;
    random_var_freq   = 0;
//    random_var_freq   = 0.10;
    verbosity         = 0;
    timeout           = UINT64_MAX;
    timeout_cb        = NULL;
    timeout_cb_data   = NULL;
    cc_cb             = NULL;
    cc_cb_data        = NULL;
    debug_cnf_out     = NULL;
    conflict_id       = clause_id_NULL;
    // -- 'debug_api_out' intentionally not cleared here

    // Initialize temporaries:
    Vec<Lit> dummy(2, lit_Undef);
    propagate_tmpbin = allocClause(false, dummy);
    analyze_tmpbin   = allocClause(false, dummy);
    dummy.pop();
    solve_tmpunit    = allocClause(false, dummy);

    // Add 'lit_Null' and the TRUE literal:
    Lit p = addLit(); assert(p == Lit(0));
    Lit q = addLit(); assert(q == Lit(1)); assert(q.id == var_True);
    addClause(q);
}


template<bool pfl>
void MiniSat<pfl>::clear(bool dealloc, bool clear_stats)
{
    mem_lits .clear(dealloc);

    clauses  .clear(dealloc);
    learnts  .clear(dealloc);
    unit_id  .clear(dealloc);
    activity .clear(dealloc);
    polarity .clear(dealloc);
    order    .clear(dealloc);
    trail    .clear(dealloc);
    trail_lim.clear(dealloc);
    vdata    .clear(dealloc);
    assumps  .clear(dealloc);
    free_vars.clear(dealloc);

    wlDisposeAll();
    watches.clear(dealloc);

    analyze_visit  .clear(dealloc);
    cl_tmp         .clear(dealloc);

    conflict.clear(dealloc);

    proof.clear();
    if (clear_stats)
        stats.clear();

    if (debug_cnf_out)
        debug_cnf_out->flush();
    if (debug_api_out)
        *debug_api_out |= "clear(%_)", (int)dealloc;

    init();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Watcher list management:


#define wl_alloc(sz) ymalloc<GClause>(sz)
#define wl_free(ptr, sz) yfree(ptr, sz)
#define wl_realloc(ptr, old_sz, new_sz) yrealloc(ptr, old_sz, new_sz)


template<bool pfl>
Array<GClause> MiniSat<pfl>::wlGet(Lit p)
{
    WHead& h = watches[p.data()];
    return Array_new(h.get(), h.size());
}


template<bool pfl>
void MiniSat<pfl>::wlAdd(Lit p, GClause c)
{
    WHead& h = watches[p.data()];

    if (h.isExt()){
        // External data:
        if (h.size() == h.cap()){
            // Grow memory block:
            uint new_cap = ((h.cap()*5 >> 2) + 2) & ~1;
            h.ext() = wl_realloc(h.ext(), h.cap(), new_cap);
            h.cap() = new_cap;
        }
        h.ext()[h.size()] = c;

    }else{
        // Inlined data:
        if (h.size() < 3)
            h.inl()[h.size()] = c;
        else{
            // Switch to external mode:
            uint new_cap = 6;
            GClause* ptr = wl_alloc(new_cap);
            ptr[0] = h.inl()[0];
            ptr[1] = h.inl()[1];
            ptr[2] = h.inl()[2];
            ptr[3] = c;
            h.ext() = ptr;
            h.cap() = new_cap;
        }
    }
    h.size()++;
}


template<bool pfl>
void MiniSat<pfl>::wlShrink(Lit p, uint new_size)
{
    WHead& h = watches[p.data()];

    if (h.isExt()){
        // External data:
        GClause* ptr = h.ext();
        if (new_size <= h.lim){
            // Switch to inlined mode:
            uint cap = h.cap();
            for (uint i = 0; i < new_size; i++)
                h.inl()[i] = ptr[i];
            wl_free(ptr, cap);
        }
        // -- else, may shrink allocation here (doesn't seem to be worth it though)
    }
    h.size() = new_size;
}


template<bool pfl>
void MiniSat<pfl>::wlPop(Lit p)
{
    WHead& h = watches[p.data()];
    wlShrink(p, h.size() - 1);
}


template<bool pfl>
void MiniSat<pfl>::wlClear(Lit p)
{
    WHead& h = watches[p.data()];

    if (h.isExt())
        wl_free(h.ext(), h.cap());
    h.size() = 0;
}


template<bool pfl>
bool MiniSat<pfl>::wlRemove(Lit p, GClause c)
{
    Array<GClause> ws = wlGet(p);
    if (ws.size() == 0) return false;

    uint j = 0;
//    for (; ws[j] != c; j++) assert(j < ws.size());
    for (; ws[j] != c; j++) if (j >= ws.size()-1) return false; // <<== super temporary! (fix in removeVars()!)
#if !defined(BLOCKING_LITERALS)
    for (; j < ws.size()-1; j++) ws[j] = ws[j+1];
    wlPop(p);
#else
    if (c.isLit()){
        for (; j < ws.size()-1; j++) ws[j] = ws[j+1];
        wlPop(p);
    }else{
        for (; j < ws.size()-2; j++) ws[j] = ws[j+2];
        wlPop(p);
        wlPop(p);
    }
#endif
    return true;
}


// Deallocate memory allocated for watcher lists (but don't touch the actual 'watches[]' vector).
template<bool pfl>
void MiniSat<pfl>::wlDisposeAll()
{
    for (uint i = 0; i < watches.size(); i++){
        WHead& h = watches[i];
        if (h.isExt())
            wl_free(h.ext(), h.cap());
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Clause memory managements:


#define MEM mem_lits.base()


template<bool pfl>
GClause MiniSat<pfl>::allocClause(bool learnt, const Vec<Lit>& ps)
{
    static_assert(sizeof(Lit)   == sizeof(uint));
    static_assert(sizeof(float) == sizeof(uint));

    uint offset = mem_lits.size();
    uint n = (1 + ps.size() + (uint)learnt + 1 + uint(pfl)) & ~1u;
    mem_lits.growTo(mem_lits.size() + n, lit_Undef);

  #if defined(ZZ_BIG_MODE)
    if ((uint64)mem_lits.size() > (uint64)UINT_MAX)
        Throw (Excp_SatMemOut) "Too many literal occurances created in SAT solver.";
  #endif

    Clause& c = (Clause&)mem_lits[offset];
    c.data[0].head  = (ps.size() << 2) | (uint(learnt) << 1);   // -- Must match representation in 'Clause'
    for (uint i = 0; i < ps.size(); i++) (Lit&)c.data[i+1].lit = ps[i];
    if (learnt) c.activity() = 0;
    if (pfl) c.id() = clause_id_NULL;

    return GClause_new(offset);
}


macro uint allocSize(Clause& c, bool pfl) { return (1 + c.size() + (uint)c.learnt() + 1 + uint(pfl)) & ~1u; }
    // -- in number of literals (32-bit words)


template<bool pfl>
void MiniSat<pfl>::freeClause(GClause gc)
{
    Clause& c = *gc.clause(mem_lits.base());;
    uint size = allocSize(c, pfl);
    n_free_lits += size;

    Lit* ps = (Lit*)&c;
    for (uint i = 0; i < size; i++)
        ps[i] = lit_Free;
}


static inline   // -- (inline because Sun can't handle static functions called from template code correctly)
void updateRoots(Array<GClause> cs, const Vec<uint>& new_offset)
{
    for (uint i = 0; i < cs.size(); i++){
#if !defined(BLOCKING_LITERALS)
        if (cs[i].isLit() || cs[i] == GClause_NULL) continue;
#else
        if (cs[i].isLit() || cs[i].isBLit() || cs[i] == GClause_NULL) continue;
#endif
        cs[i] = GClause_new(new_offset[cs[i].offset() >> 2]);
    }
}


static inline   // -- (inline because Sun can't handle static functions called from template code correctly)
void updateRoots(Vec<MSVarData>& cs, const Vec<uint>& new_offset)
{
    for (uint i = 0; i < cs.size(); i++){
        GClause& r = cs[i].reason;
        if (r.isLit() || r == GClause_NULL) continue;
        r = GClause_new(new_offset[r.offset() >> 2]);
    }
}


// Compress the 'mem_lits' vector, updating the following member variables:
//
//    Vec<GClause>        clauses;
//    Vec<GClause>        learnts;
//    Vec<Vec<GClause> >  watches;
//    Vec<GClause>        reason;
//
template<bool pfl>
void MiniSat<pfl>::compactClauses()
{
    // Worth doing a compaction?
    uint percent_waste = uint(uint64(n_free_lits) * 100 / (mem_lits.size() - 2));
    if (percent_waste < 5) return;

    // Extra check:   (remove later)
    uint c = 0;
    for (uint i = 2; i < mem_lits.size(); i++)
        if (mem_lits[i] == lit_Free)
            c++;
    assert(c == n_free_lits);

    // Compact:
    Vec<uint> new_offset((mem_lits.size() + 3) >> 2);
    uint j = 2;
    for (uint i = 2; i < mem_lits.size();){
        if (mem_lits[i] != lit_Free){
            // Copy active clause:
            uint size = allocSize((Clause&)mem_lits[i], pfl);
            new_offset[i >> 2] = j;
            for (uint n = size; n != 0; n--)
                mem_lits[j++] = mem_lits[i++];
        }else{
            // Skip over deleted clause:
            do i++; while (i < mem_lits.size() && mem_lits[i] == lit_Free);
                // <<== Should fix this. 'lit_Free' now must not overlap with any field in the
                // clause, including clause ID and activity (which is a float).
        }
    }
    mem_lits.shrinkTo(j);
    n_free_lits = 0;

    // Update roots:
    updateRoots(slice(clauses), new_offset);
    updateRoots(slice(learnts), new_offset);
    updateRoots(vdata         , new_offset);
    for (Var x = 0; x < nVars(); x++){
        updateRoots(wlGet( Lit(x)), new_offset);
        updateRoots(wlGet(~Lit(x)), new_offset);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Operations on clauses:


// Puts 'p' on the propagation queue and updates 'assigns[]'. If conflicting value is already
// assigned, FALSE is returned (and queue is unchanged).
template<bool pfl>
inline bool MiniSat<pfl>::enqueue(Lit p, GClause from)
{
    if (value(p) != l_Undef){
        return value(p) != l_False;
    }else{
#if defined(EXPERIMENTAL)
        vdata[var(p)] = MSVarData(from, dl());
        assign_set(p, l_True);
#else
        vdata[p.id] = MSVarData(from, dl(), lbool_lift(!p.sign));
#endif
        trail.push(p);
#if defined(PRE_FETCH)
        __builtin_prefetch(wlGet(p).data);
#endif
        return true;
    }
}


struct LitByLev_gt {
    Vec<MSVarData>& vdata;
    bool operator()(Lit x, Lit y) const { return vdata[x.id].level > vdata[y.id].level; }
    LitByLev_gt(Vec<MSVarData>& d) : vdata(d) {}
};


// Put the two literals with the highest decision levels first in 'ps', then
// backtrack until at least one variable is unbound.
template<bool pfl>
void MiniSat<pfl>::backtrack(Vec<Lit>& ps)
{
    if (ps.size() < 2)
        undo(0);
    else{
      #if 0
        LitByLev_gt gt(vdata);
        sobSort(sob(ps, gt));
      #else
        for (uint k = 0; k < 2; k++){
            uint best_i = k;
            uint best_lv = level(ps[k]);
            for (uint i = k+1; i < ps.size(); i++)
                if (newMax(best_lv, level(ps[i])))
                    best_i = i;
            swp(ps[k], ps[best_i]);
        }
      #endif
        uint lv0 = level(ps[0]);
        uint lv1 = level(ps[1]);
        undo(min_(lv0, lv1));
        if (lv0 == lv1 && lv0 != 0) undo(lv0-1);
    }
}


// Add a clause 'ps' to the current clause database. If 'id' is not 'clause_id_NULL', the clause is
// assumed to be a learned clause. For non-learned (root) clauses, known unit literals are removed
// (eg. adding {~a} then {a, b, c} will result in {~a}, {b, c}). If a conflict is detected, the
// 'ok' flag is to FALSE and the solver is now in an unusable state (must be disposed). Watches
// will be set on the asserting literal and the non-asserting literal with the highest decision
// level.
template<bool pfl>
void MiniSat<pfl>::newClause(const Vec<Lit>& ps_, clause_id id)
{
    if (!ok) return;

#if defined(ZZ_DEBUG)
    for (uind i = 0; i < ps_.size(); i++)
        assert(hasVar(var(ps_[i])));
#endif

    bool learnt = (id != clause_id_NULL);
    vt += 30;       // -- just a crude approximation

    // Clean up clause:
    Vec<Lit>& ps = cl_tmp;
    ps_.copyTo(ps);
    if (learnt && cc_cb)
        cc_cb(ps, cc_cb_data);

    if (!learnt){
        for (uind i = 0; i < ps.size(); i++)
            assert(ps[i].id < nVars());

        if (ps.size() > 0){
            // Remove duplicates:
            sortUnique(ps);

            // Insert variables into decision order:
            for (uind i = 0; i < ps.size(); i++){
                Var x = ps[i].id;
                assert(x < nVars() && !free_vars.has(x));
                if (!order.has(x))
                    order.add(x);
            }

            // Check if clause is satisfied:
            for (uint i = 1; i < ps.size(); i++){
                if (ps[i-1] == ~ps[i])
                    return; }
            for (uint i = 0; i < ps.size(); i++){
                if (topValue(ps[i]) == l_True)
                    return; }

#if !defined(KEEP_TOPLEVEL_LITERALS)
            // Remove false literals:
            uint i, j;
            clause_id id0;
            if (pfl){
                id0 = proof.addRoot(ps);
                proof.beginChain(id0); }
            for (i = j = 0; i < ps.size(); i++){
                if (topValue(ps[i]) != l_False)
                    ps[j++] = ps[i];
                else if (pfl){
                    proof.resolve(unit_id[ps[i].id], ~ps[i]); }
            }
            for (uint n = i - j; n != 0; n--)
                ps.pop();
            if (pfl){
                id = proof.endChain(&ps);
                if (id != id0){
                    // Need to remember original clause for 'removeVars()':
                    GClause gc = allocClause(true, ps_);
                    Clause& c  = *gc.clause(MEM);
                    c.id() = id0;
                    learnts.push(gc);
                }
            }
#else
            if (pfl)
                id = proof.addRoot(ps);
#endif
        }

#if 0
        /*HEUR*/
        double f = var_inc / ps.size() / 16;
        for (uind i = 0; i < ps.size(); i++)
            varBumpActivity(ps[i], f);
        /*END*/
#endif

#if defined(BUMP_EXPERIMENT)
        for (uind i = 0; i < ps.size(); i++)
            var_count[var(ps[i])]++;
#endif
    }
    assert(!pfl || id != clause_id_NULL);

    // Trivial clause?
    if (ps.size() == 0){
        undo(0);
        ok = false;
        conflict.clear();
//        conflict_id = clause_id_NULL;
        conflict_id = id;
        return;
    }else if (ps.size() == 1){
        undo(0);
        if (pfl) unit_id[ps[0].id] = id;
        if (!enqueue(ps[0])) assert(false);
        return;
    }

    // Put the two literals with the highest decision levels first:
    //*HEUR*/shuffle(random_seed, ps);
    backtrack(ps);

    bool is_unit = true;
    for (uint i = 1; i < ps.size(); i++){
        if (value(ps[i]) != l_False){
            is_unit = false;
            break;
        }
    }
#if defined(KEEP_TOPLEVEL_LITERALS)
    if (is_unit && !learnt && pfl)
        unit_id[var(ps[0])] = id;
#endif

    if (!pfl && ps.size() == 2){
        // Create special binary clause watch:
        wlAdd(~ps[0], GClause_new(ps[1]));
        wlAdd(~ps[1], GClause_new(ps[0]));
        if (is_unit){
            bool ret = enqueue(ps[0], GClause_new(~ps[1])); assert(ret); }

        if (learnt) stats.learnts_literals += ps.size();
        else        stats.clauses_literals += ps.size();
        n_literals += ps.size();
        n_bin_clauses++;

    }else{ assert(pfl || ps.size() > 2);
        // Allocate clause:
        GClause gc = allocClause(learnt, ps);
        Clause& c  = *gc.clause(MEM);
        if (pfl) c.id() = id;

        if (is_unit){
            bool ret = enqueue(ps[0], GClause_new(&c, MEM)); assert(ret); }

        if (learnt){
            claBumpActivity(&c);        // -- newly learnt clauses should be considered active
            learnts.push(GClause_new(&c, MEM));
            stats.learnts_literals += c.size();
            if (ps.size() == 2) n_bin_clauses++;
        }else{
            clauses.push(GClause_new(&c, MEM));
            stats.clauses_literals += c.size();
        }
        n_literals += c.size();

        // Watch clause:
        wlAdd(~c[0], GClause_new(&c, MEM));
        wlAdd(~c[1], GClause_new(&c, MEM));
#if defined(BLOCKING_LITERALS)
        wlAdd(~c[0], GClause_newBLit(c[1]));
        wlAdd(~c[1], GClause_newBLit(c[0]));
#endif
    }
}


// Disposes a clauses and removes it from watcher lists. NOTE! Low-level; does NOT change the 'clauses' and 'learnts' vector.
//
template<bool pfl>
void MiniSat<pfl>::removeClause(GClause gc, bool just_dealloc, bool remove_watches, bool deref_proof)
{
    Clause& c = *gc.clause(MEM);

    if (!just_dealloc){
        //**/if (pfl && deref_proof) WriteLn "removed (refC=%_): %_ = %_", proof.refC[c.id()], c.id(), c;
        assert(pfl || c.size() != 2);
        if (remove_watches){
            wlRemove(~c[0], gc);
            wlRemove(~c[1], gc); }

        if (c.learnt()) stats.learnts_literals -= c.size();
        else            stats.clauses_literals -= c.size();
        n_literals -= c.size();

        if (pfl && deref_proof) proof.deleted(c.id());
    }

    freeClause(gc);
}


// Can assume everything has been propagated! (esp. the first two literals are != l_False, unless
// the clause is binary and satisfied, in which case the first literal is true)
// Returns True if clause is satisfied (will be removed), False otherwise.
//
template<bool pfl>
bool MiniSat<pfl>::simplifyClause(GClause gc) const
{
    assert(dl() == 0);
    Clause& c = *gc.clause(mem_lits.base());
    for (uint i = 0; i < c.size(); i++){
        if (value(c[i]) == l_True)
            return true;
    }
    return false;
}


// Simplify the clause database according to the current top-level assigment. Currently, the only
// thing done here is the removal of satisfied clauses.
template<bool pfl>
void MiniSat<pfl>::simplifyDB_intern()
{
    assert(ok);

    if (nAssigns() == simpDB_assigns || simpDB_props > 0)   // -- nothing has changed or preformed a simplification too recently
        return;

    undo(0);
    Clause* confl = propagate();
    if (confl != NULL){
        if (pfl) conflict_id = proof.last();
        ok = false;
        return; }

    // Clear watcher lists:
    if (!pfl){
        for (uint i = simpDB_assigns; i < nAssigns(); i++){
            Lit            p  = trail[i];
            Array<GClause> ws = wlGet(~p);
            for (uint j = 0; j < ws.size(); j++){
                if (ws[j].isLit() && wlRemove(~ws[j].lit(), GClause_new(p))) // -- remove binary GClause from "other" watcher list
                    n_bin_clauses--;
            }

            ws = wlGet(p);
            for (uint j = 0; j < ws.size(); j++){
                if (ws[j].isLit() && wlRemove(~ws[j].lit(), GClause_new(~p))) // -- remove binary GClause from "other" watcher list
                    n_bin_clauses--;
            }
            wlClear( p);
            wlClear(~p);
        }
    }

    // Remove satisfied clauses:
    for (int type = 0; type < 2; type++){
        Vec<GClause>& cs = type ? learnts : clauses;
        uint j  = 0;
        for (uint i = 0; i < cs.size(); i++){
            if (!locked(cs[i]) && simplifyClause(cs[i])){   // -- the test for 'locked()' is currently superfluous, but without it the reason-graph is not correctly maintained for decision level 0
                //**/WriteLn "simplifyDB: removeClause(%_ = %_)", cs[i], *cs[i].clause(MEM);
                removeClause(cs[i]);
            }else
                cs[j++] = cs[i];
        }
        cs.shrinkTo(j);
    }

    simpDB_assigns = nAssigns();
    simpDB_props   = n_literals;

    compactClauses();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Minor methods:


// Creates a new SAT variable in the solver. If 'decision_var' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
template<bool pfl>
Var MiniSat<pfl>::newVar()
{
    if (nVars() > var_Max)
        Throw (Excp_SatMemOut) "Too many variables created in SAT solver (max is %_)", var_Undef;

    Var x;
    x = nVars();
    watches     .push();          // -- list for positive literal
    watches     .push();          // -- list for negative literal
    vdata       .push();
#if defined(EXPERIMENTAL)
    assign_     .push(l_Undef);
#endif
//    activity    .push(1.0/0x10000000 * x);
//    activity    .push(1 - 1.0/0x10000000 * x);
//    activity    .push(var_inc * (1 - 1.0/0x10000000 * x));
    activity    .push(0);
    polarity    .push(1);
    if (pfl) unit_id.push(clause_id_NULL);
    /*HEUR*/order.add(x);
#if defined(BUMP_EXPERIMENT)
    var_count   .push(0);
#endif
    return x;
}


// Returns FALSE if immediate conflict.
template<bool pfl>
bool MiniSat<pfl>::assume(Lit p)
{
    trail_lim.push(trail.size());
    return enqueue(p);
}


// Revert to the state at given level.
template<bool pfl>
void MiniSat<pfl>::undo(uint lv)
{
    if (dl() > lv){
        for (int c = trail.size()-1; c >= trail_lim[lv]; c--){
            Var x  = trail[c].id;
            assign_set(x, l_Undef);
            reason_set(x, GClause_NULL);
/**/            if (c < trail_lim.last())       // HEUR
                polarity[x] = trail[c].sign;
            if (!order.has(x))
                order.add(x);
        }
        trail.shrinkTo(trail_lim[lv]);
        trail_lim.shrinkTo(lv);
        qhead = trail.size();
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Major methods:



// Analyze conflict and produce a reason clause. PRE-CONDITIONS: 'out_learnt' is assumed to be
// cleared; Current decision level must be greater than root level. POST-CONDITION: 'out_learnt[0]'
// is the asserting literal at level 'out_btlevel'.
template<bool pfl>
clause_id MiniSat<pfl>::analyze(Clause* _confl, Vec<Lit>& out_learnt)
{
    VarAttr& visit = analyze_visit;
    visit.clear();

    GClause      confl = GClause_new(_confl, MEM);
    int          pathC = 0;
    Lit          p     = lit_Undef;

    // Generate conflict clause:
    if (pfl) proof.beginChain(_confl->id());
    out_learnt.push(lit_Undef);      // -- leave room for the asserting literal
    int index = trail.size();
    for(;;){
        assert(confl != GClause_NULL);  // -- otherwise should be UIP
        Clause& c = confl.isLit() ? (*analyze_tmpbin.clause(MEM))[1] = ~confl.lit(), *analyze_tmpbin.clause(MEM) : *confl.clause(MEM);
        if (c.learnt())
            claBumpActivity(&c);

        for (uint j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            Lit q = c[j];

            if (visit[q] == 0){
                if (level(q) > 0){
                    varBumpActivity(q);
                    visit(q) = 1;
                    if (level(q) == dl())
                        pathC++;
                    else
                        out_learnt.push(q);
                }else if (pfl){
                    proof.resolve(unit_id[q.id], ~q); }
            }
        }

        // Select next clause to look at:
        do index--; while (visit[trail[index]] == 0);
        p = trail[index];
        confl = reason(p);
        pathC--;
        visit(p) = 0;       // -- we want only the literals of the final conflict-clause to be 1 in 'visit'
        if (pathC == 0) break;

        if (pfl){
            assert_debug(!confl.isLit());
            proof.resolve(confl.clause(MEM)->id(), p); }
    }
    out_learnt[0] = ~p;

    // Simplify conflict clause:
    uint i, j;
    analyze_levels.clear();
    for (i = 1; i < out_learnt.size(); i++)
        analyze_levels.add(level(out_learnt[i]));

    analyze_result.clear();
    for (i = j = 1; i < out_learnt.size(); i++)
        if (reason(out_learnt[i]) == GClause_NULL || !analyze_removable(out_learnt[i], analyze_levels, visit, analyze_result))
            out_learnt[j++] = out_learnt[i];

    stats.max_literals += out_learnt.size();
    out_learnt.shrinkTo(j);
    stats.tot_literals += out_learnt.size();

    // Finilize proof logging with conflict clause minimization steps:
    if (pfl){
        analyze_order.clear();
        for (uind k = 0; k < analyze_result.size(); k++)
            analyze_logOrder(analyze_result[k], visit, analyze_order);

        for (uint k = analyze_order.size(); k > 0;){ k--;
            Lit     q = analyze_order[k]; assert(level(q) > 0);
            GClause r = reason(q); assert_debug(!r.isLit());
            Clause& c = *r.clause(MEM);
            proof.resolve(c.id(), ~q);
            for (uint n = 1; n < c.size(); n++)
                if (level(c[n]) == 0){
                    proof.resolve(unit_id[c[n].id], ~c[n]); }
        }
        return proof.endChain(&out_learnt);
    }else
        return 0;   // -- any value different from 'clause_id_NULL' will do here.
}


template<>
void MiniSat<false>::analyze_logOrder(Lit /*p0*/, VarAttr& /*visit*/, Vec<Lit>& /*order*/) {
    assert(false); }



//-------------------------------------------------------------------------------------------------
#if 0   // (non-recursive variants in 'MiniSat_NonRec.icc')


template<>
void MiniSat<true>::analyze_logOrder(Lit p0, VarAttr& visit, Vec<Lit>& order)
{
    if (visit[p0] & 8)
        return;

    GClause r = reason(p0); assert_debug(!r.isLit());
    Clause& c = *r.clause(MEM);

    for (uint i = 1; i < c.size(); i++){
        Lit p = c[i];
        if (level(p) != 0 && (visit[p] & 1) == 0)
            analyze_logOrder(p, visit, order);
    }

    visit(p0) |= 8;
    order.push(p0);
}


// 'result' will contain all removable literals in the fanin of 'p0' in DFS order, with 'p0' as the
// last element (if removable).
template<bool pfl>
bool MiniSat<pfl>::analyze_removable(Lit p0, const IntZet<uint>& levels, VarAttr& visit, Vec<Lit>& result)
{
    // In 'visit':
    //   - bit 0: True if original conflict clause literal.
    //   - bit 1: Processed by this procedure
    //   - bit 2: 0=non-removable, 1=removable

    if (visit[p0] & 2){
        return bool(visit[p0] & 4); }

    GClause r = reason(p0);
    if (r == GClause_NULL){
        visit(p0) |= 2; return false; }
    Clause& c = r.isLit() ? ((*analyze_tmpbin.clause(MEM))[1] = r.lit(), *analyze_tmpbin.clause(MEM)) : *r.clause(MEM);

    for (uint i = 1; i < c.size(); i++){
        Lit p = c[i];

        if (visit[p] & 1){
            analyze_removable(p, levels, visit, result);

        }else{
            if (level(p) == 0 || visit[p] == 6) continue;        // -- 'p' checked before, found to be removable (or belongs to the toplevel)
            if (visit[p] == 2){ visit(p0) |= 2; return false; }  // -- 'p' checked before, found NOT to be removable

            if (!levels.has(level(p))){ visit(p0) |= 2; return false; } // -- 'p' belongs to a level that cannot be removed

            if (!analyze_removable(p, levels, visit, result)){
                visit(p0) |= 2; return false; }
        }
    }

    if (pfl && visit[p0] & 1){
        result.push(p0); }

    visit(p0) |= 6;
    return true;
}


#else
#include "MiniSat_NonRec.icc"
#endif
//-------------------------------------------------------------------------------------------------


// Specialized analysis procedure to express the final conflict in terms of assumptions.
template<bool pfl>
void MiniSat<pfl>::analyzeFinal(const Clause& confl, bool skip_first)
{
    conflict.clear();
    if (trail_lim.size() == 0){
        /**/WriteLn "analyzeFinal -- top-level";
        if (pfl) conflict_id = proof.last();
        return; }

    VarAttr& visit = analyze_visit;
    visit.clear();

    if (pfl) proof.beginChain(confl.id());
    for (uint i = (uint)skip_first; i < confl.size(); i++){
        Lit p = confl[i];
        if (level(p) > 0)
            visit(p) = 1;
        else if (pfl)
            proof.resolve(unit_id[p.id], ~p);
    }

    int start = (int)trail.size();
    for (int i = start; i > trail_lim[0];){ i--;
        Lit q = trail[i];
        if (visit[q] != 0){
            GClause r = reason(q);
            if (r == GClause_NULL){
                assert(level(q) > 0);
                conflict.push(q);
            }else{
                if (r.isLit()){
                    Lit p = r.lit();
                    if (level(p) > 0)
                        visit(p) = 1;
                }else{
                    Clause& c = *r.clause(MEM);
                    if (pfl) proof.resolve(c.id(), q);
                    for (uint j = 1; j < c.size(); j++){
                        if (level(c[j]) > 0)
                            visit(c[j]) = 1;
                        else if (pfl)
                            proof.resolve(unit_id[c[j].id], ~c[j]);
                    }
                }
            }
        }
    }
    if (pfl) conflict_id = proof.endChain();
    //**/WriteLn "analyzeFinal -- confl=%_  conflict_id=%_", confl.id(), conflict_id;
}


// Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
// otherwise NULL. POST-CONDITION: The propagation queue is empty, even if there was a conflict.
template<bool pfl>
Clause* MiniSat<pfl>::propagate()
{
    Clause* confl = NULL;
    uint    n_inspections = 0;

    while (qhead < (int)trail.size()){
        Lit p = trail[qhead++];     // -- 'p' is enqueued fact to propagate.
        stats.propagations++;
        simpDB_props--;

        Array<GClause> ws = wlGet(p);
        GClause*       i,* j, *end;

#if defined(PRE_FETCH)
        for (i = ws.base(), end = i + ws.size(); i != end; i++)
           __builtin_prefetch(i->clause(MEM));
#endif
        for (i = j = ws.base(), end = i + ws.size();  i != end;){
            n_inspections++;
            if (i->isLit()){
                if (!enqueue(i->lit(), GClause_new(p))){
                    if (dl() == 0)
                        ok = false;
                    confl = propagate_tmpbin.clause(MEM);
                    (*confl)[1] = ~p;
                    (*confl)[0] = ~i->lit();

                    qhead = trail.size();
                    // Copy the remaining watches:
                    while (i < end)
                        *j++ = *i++;
                }else
                    *j++ = *i++;
            }else{
                Clause& c = *i->clause(MEM); i++;
#if defined(BLOCKING_LITERALS)
                assert(i->isBLit());
                Lit block_lit = i->lit();
                if (value(block_lit) == l_True)
                //if (false)
                {
                    *j++ = GClause_new(&c, MEM);
                    *j++ = *i++;
                }else{
                    i++;
#else
                {
#endif
                    assert(pfl || c.size() > 2);
                    // Make sure the false literal is 'c[1]':
                    Lit false_lit = ~p;
                    if (c[0] == false_lit)
                        c[0] = c[1], c[1] = false_lit;
                    assert(c[1] == false_lit);

                    // If 0th watch is true, then clause is already satisfied.
                    Lit   first = c[0];
                    lbool val   = value(first);
                    if (val == l_True){
                        *j++ = GClause_new(&c, MEM);
#if defined(BLOCKING_LITERALS)
                        *j++ = GClause_newBLit(first);
#endif
                    }else{
                        // Look for new watch:
                        for (uint k = 2; k < c.size(); k++){
                            if (value(c[k]) != l_False){
                                c[1] = c[k]; c[k] = false_lit;
                                wlAdd(~c[1], GClause_new(&c, MEM));
#if defined(BLOCKING_LITERALS)
                                wlAdd(~c[1], GClause_newBLit(c[0]));
#endif
                                goto FoundWatch;
                            }
                        }

                        // Did not find watch -- clause is unit under assignment:
                        if (pfl && dl() == 0){
                            // Log production of top-level unit clause:
                            proof.beginChain(c.id());
                            for (uint k = 1; k < c.size(); k++)
                                proof.resolve(unit_id[c[k].id], ~c[k]);
                            clause_id id = proof.endChain();    // <<=== verify result
                            assert(unit_id[first.id] == clause_id_NULL || value(first) == l_False);    // -- if variable already has 'id', it must be with the other polarity and we should have derived the empty clause here
                            if (value(first) != l_False)
                                unit_id[first.id] = id;
                            else{
                                // Empty clause derived:
                                proof.beginChain(unit_id[first.id]);
                                proof.resolve(id, first);
                                proof.endChain();    // <<=== verify result
                            }
                        }

                        *j++ = GClause_new(&c, MEM);
#if defined(BLOCKING_LITERALS)
                        *j++ = GClause_newBLit(first);
#endif
                        if (!enqueue(first, GClause_new(&c, MEM))){
                            if (dl() == 0)
                                ok = false;
                            confl = &c;
                            qhead = trail.size();
                            // Copy the remaining watches:
                            while (i < end)
                                *j++ = *i++;
                        }
                      FoundWatch:;
                    }
                }
            }
        }
        wlShrink(p, ws.size() + j - i);
    }

    stats.inspections += n_inspections;
    vt += n_inspections;
    return confl;
}


struct reduceDB_lt {
    const Lit* base;
    reduceDB_lt(const Lit* b) : base(b) {}
    bool operator () (GClause x, GClause y) const { return x.clause(base)->activity() < y.clause(base)->activity(); }
};


// Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
// clauses are clauses that are reason to some assignment. Binary clauses are never removed.
template<bool pfl>
void MiniSat<pfl>::reduceDB()
{
    uint    i, j;
    double  extra_lim = cla_inc / learnts.size();    // -- remove any clause below this activity

    sobSort(sob(learnts, reduceDB_lt(MEM)));
    for (i = j = 0; i < learnts.size() / 2; i++){
        if (learnts[i].clause(MEM)->size() > 2 && !locked(learnts[i]))
            removeClause(learnts[i]);
        else
            learnts[j++] = learnts[i];
    }
    for (; i < learnts.size(); i++){
        if (learnts[i].clause(MEM)->size() > 2 && !locked(learnts[i]) && learnts[i].clause(MEM)->activity() < extra_lim)
            removeClause(learnts[i]);
        else
            learnts[j++] = learnts[i];
    }
    learnts.shrinkTo(j);
    stats.deleted_clauses += i - j;

    compactClauses();
}


// Returns TRUE if decision was made, FALSE if model was found!
template<bool pfl>
bool MiniSat<pfl>::makeDecision()
{
    Var cand;
    do{    // -- pop assigned variables
        if (order.size() == 0){
            // Model found:
            return false; }
        cand = order.pop();
    }while (assign(cand) != l_Undef);

    Var next;
    double act = activity[cand] / var_inc;
    if (/*HEUR*/act < 0.25 && random_var_freq > 0 && drand(random_seed) < random_var_freq){
        next = irand(random_seed, nVars());
        if (assign(next) != l_Undef)
            next = cand;
        else{
            stats.random_decis++;
            order.add(cand);    // <<== better not pop it if we are not going to use it
        }
    }else
        next = cand;

    assume(Lit(next, polarity[next]));
    return true;
}


// Perform SAT search for 'nof_conflicts' conflicts. The number of learnt clauses are kept
// under 'nof_learnts' (soft limit). Use '-1' for "no limit". Return values are:
//   - 'l_True'  -- a satisfying model was found (stored in 'model[]').
//   - 'l_False' -- UNSAT was proven
//   - 'l_Error' -- 'nof_conflicts' was exceeded
//   - 'l_Undef' -- 'timeout' was reached.
template<bool pfl>
lbool MiniSat<pfl>::search(int nof_conflicts, int nof_learnts)
{
    if (!ok) return l_False;    // GUARD (public method)

    stats.starts++;
    int conflictC = 0;
    var_decay = 1 / variable_decay;
    cla_decay = 1 / clause_decay;

    uint64 print_lim = 0;
    for (;;){
        if (stats.inspections > print_lim){
            if (verbosity >= 1)
                printProgressLine(false);
            print_lim = stats.inspections + 3000000;
        }

        Clause* confl = propagate();
        if (confl != NULL){
            // CONFLICT

            stats.conflicts++; conflictC++;
            Vec<Lit> learnt_clause;
#if 0
// (redundant, just here for testing)
            if (dl() <= assumps.size()){
                // Contradiction found:
                analyzeFinal(*confl);
                return l_False; }
#endif
            if (dl() == 0){
                // Top-level contradiction:
                conflict.clear();
                if (pfl) conflict_id = proof.last();
                return l_False;
            }
            clause_id id = analyze(confl, learnt_clause);
            newClause(learnt_clause, id);
            varDecayActivity();
            claDecayActivity();

            //**/if (pfl) WriteLn "LEARNED: %_", learnt_clause;

        }else{
            // NO CONFLICT

            if (timeout != UINT64_MAX && vt >= timeout){
                // Time to use callback:
                uint64 work = vt;
                vt = 0;
                if (timeout_cb == NULL || !timeout_cb(work, timeout_cb_data))
                    return l_Undef;
            }

            if (nof_conflicts >= 0 && conflictC >= nof_conflicts){
                // Reached bound on number of conflicts:
                /*<<==make this independent so that incremental call accumulate up to this value??*/
#if 0
                /*TEST*/
                Vec<Var> vars;
                for (Var x = 0; x < nVars(); x++)
                    if (!free_vars.has(x))
                        vars.push(x);
                sobSort(ordReverse(sob(vars, proj_lt(brack<double,Var>(activity)))));
                for (uint i = 0; i < trail_lim.size() && vars[i] == trail[trail_lim[i]].id; i++)
                    WriteLn "x%_ = %_", vars[i], activity[vars[i]] / var_inc;
                /*END*/
#endif
                undo(0);
                return l_Error; }

            if (dl() == 0)
                // Simplify the set of problem clauses:
                simplifyDB_intern(), assert(ok);

            if (nof_learnts >= 0 && int(learnts.size()-nAssigns() - (pfl ? n_bin_clauses : 0)) >= nof_learnts)
                // Reduce the set of learnt clauses:
                reduceDB();

            // New variable decision:
            if (dl() < assumps.size()){
                Lit p = assumps[dl()];
                if (!assume(p)){
                    // Analyze final conflict on assumption literal:
                    GClause r = reason(p);
                    if (r != GClause_NULL){
                        clause_id orig_id = clause_id_NULL;
                        if (!pfl && r.isLit()){
                            Clause& c = *solve_tmpunit.clause(MEM);
                            c[0] = p;
                            analyzeFinal(c);
                        }else{
                            orig_id = r.clause(MEM)->id();
                            analyzeFinal(*r.clause(MEM), true);
                        }
                        conflict.push(p);
                        if (pfl && orig_id != conflict_id){
                            for (uind i = 0; i < conflict.size(); i++) conflict[i] = ~conflict[i];
                            newClause(conflict, conflict_id);
                            for (uind i = 0; i < conflict.size(); i++) conflict[i] = ~conflict[i];
                        }else
                            backtrack(conflict);

                    }else{
                        conflict.clear();
                        conflict.push(p);
                        if (pfl) conflict_id = unit_id[p.id], assert(conflict_id != clause_id_NULL);
                    }

                    return l_False;
                }
            }else{
                if (!makeDecision())
                    return l_True;  // -- model found
            }
            stats.decisions++;
        }
    }
}


// Divide all variable activities by 1e100.
//
template<bool pfl>
void MiniSat<pfl>::varRescaleActivity()
{
    for (uint i = 0; i < nVars(); i++)
        activity[i] *= 1e-100;
    var_inc *= 1e-100;
}


// Divide all constraint activities by 1e100.
//
template<bool pfl>
void MiniSat<pfl>::claRescaleActivity()
{
    for (uint i = 0; i < learnts.size(); i++)
        learnts[i].clause(MEM)->activity() *= 1e-20;
    cla_inc *= 1e-20;
}


static inline   // -- (inline because Sun can't handle static functions called from template code correctly)
uint lubyLog(uint x)
{
    uint size, seq;
    for (size = 1, seq = 0; size <= x; seq++, size = 2*size + 1);

    while (x != size - 1){
        size >>= 1;
        seq--;
        if (x >= size) x-= size;
    }

    return seq;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Variable filtering:


template<bool pfl>
void MiniSat<pfl>::setFilter(IntZet<Var>& enabled)
{
    assert(enabled_vars == NULL);
    enabled_vars = &enabled;

    order.moveTo(order_main);
    for (uind i = 0; i < enabled.list().size(); i++)
        order.push(enabled.list()[i]);
    order.heapify();
}


template<bool pfl>
void MiniSat<pfl>::clearFilter()
{
    order_main.moveTo(order);
    enabled_vars = NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Progress output:


#define FR "\a/"
#define H1 "\a*"
#define H2 "\a*"
#define SEP0 "|"
#define SEP "\a0\a/"SEP0"\a0"
#define SEPH1 SEP H1
#define SEPH2 SEP H2


template<bool pfl>
void MiniSat<pfl>::printProgressHeader() const
{
    WriteLn FR "                                                                 [MiniSAT 1.16]\a0";
    WriteLn FR "===============================================================================\a0";
    WriteLn H1 "       SEARCH        " SEPH1 "   ORIGINAL    " SEPH1 "       LEARNED        " SEPH1 "     RESOURCES    \a0";
    WriteLn H2 "Confl  Decis  Props  " SEPH2 " Claus   Lits  " SEPH2 " Units  Claus  Lit/C  " SEPH2 " Memory   CPU Time\a0";
    WriteLn FR "-------------------------------------------------------------------------------\a0";
}


template<bool pfl>
void MiniSat<pfl>::printProgressFooter() const
{
    WriteLn FR "===============================================================================\a0";
}


template<bool pfl>
void MiniSat<pfl>::printProgressLine(bool newline) const
{
    String lit_ratio_text;
    if (nLearnts() == 0) lit_ratio_text = "  n/a";
    else{
        double lit_ratio = (double)stats.learnts_literals / nLearnts();
        lit_ratio_text = (FMT "%>5%.1f", lit_ratio);
        if (lit_ratio_text.size() > 5) lit_ratio_text = (FMT "%>5%,'D", (int64)lit_ratio);
    }

    String text = (FMT "\r%>5%,'D  %>5%,'D  %>5%,'D  "SEP0" %>5%,'D  %>5%,'D  "SEP0" %>5%,'D  %>5%,'D  %_  "SEP0" %>6%^DB%>10%t",
        stats.conflicts,
        stats.decisions,
        stats.propagations,
        nClauses(),
        stats.clauses_literals,
        (trail_lim.size() == 0) ? trail.size() : trail_lim[0],  // -- learnt units
        nLearnts(),
        lit_ratio_text,
        memUsed(),
        cpuTime() - cpu_time0
    );

    for (uint i = 0; i < text.size(); i++){
        char c = text[i];
        if (isDigit(c) || c=='.' || c==',' || c==':' || c==' ')
            std_out += c;
        else if (c == SEP0[0])
            std_out += SEP;
        else
            std_out += "\a*", c, "\a0";
    }

    std_out += (newline ? '\n' : '\f'), FL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Public API:


template<bool pfl>
void MiniSat<pfl>::simplifyDB()
{
    if (debug_api_out)
        *debug_api_out |= "simplifyDB()";

    if (!ok) return;    // GUARD (public method)
    simplifyDB_intern();
}

template<bool pfl>
void MiniSat<pfl>::addClause(const Vec<Lit>& ps)
{
    if (debug_cnf_out){
        for (uint i = 0; i < ps.size(); i++) *debug_cnf_out += toDimacs(ps[i]), ' ';
        *debug_cnf_out += '0', '\n'; }

    if (debug_api_out)
        *debug_api_out |= "addClause(%_)", ps;

    newClause(ps);
}


template<bool pfl>
void MiniSat<pfl>::removeVars(const Vec<Var>& xs, Vec<Var>& kept)
{
    IntZet<Var> ys;
    for (uint i = 0; i < xs.size(); i++)
        ys.add(xs[i]);
    removeVars(ys, kept);
}


template<bool pfl>
void MiniSat<pfl>::removeVars(IntZet<Var>& xs, Vec<Var>& kept)
{
    xs.compact();

    if (debug_api_out){
        Vec<Lit> ps(xs.list().size(), lit_Undef);
        for (uind i = 0; i < xs.list().size(); i++) ps[i] = Lit(xs.list()[i]);
        *debug_api_out |= "removeVars(%_)", ps;
    }

    undo(0);
    uint bin_deleted = 0;

#if !defined(KEEP_TOPLEVEL_LITERALS)
    // Keep variables that are assigned at the top-level:
    for (uind i = 0; i < xs.list().size(); i++){
        Var x = xs.list()[i];
        if (value(x) != l_Undef){
            xs.exclude(x);
            kept.push(x);
            DB{ WriteLn "\a/  %%-- keeping variable assigned at top-level: x%_=%_\a/", x, value(x); }
        }
    }
#endif

    // Tag clauses for deletion and try to remove them from proof:
    for (int type = 0; type < 2; type++){
        Vec<GClause>& cs = type ? learnts : clauses;
        for (uint i = 0; i < cs.size(); i++){
            assert(!cs[i].isLit());
            Clause& c = *cs[i].clause(MEM);

            for (uint j = 0; j < c.size(); j++){
                if (xs.has(c[j].id)){
                    c.tag_set(true);
                    if (pfl){
                        DB{ WriteLn "\a/  %%-- tagged clause %_ for deletion: %_\a/", c.id(), c; }
                        proof.deleted(c.id()); }     // -- dereference clause
                    break;
                }
            }
        }
    }

    // Untag clauses that could not be removed from proof:
    if (pfl){
        for (int type = 0; type < 2; type++){
            Vec<GClause>& cs = type ? learnts : clauses;
            for (uint i = 0; i < cs.size(); i++){
                assert(!cs[i].isLit());
                Clause& c = *cs[i].clause(MEM);

                if (c.tag() && proof.revive(c.id())){
                    c.tag_set(false);
                    stats.stuck_clauses++;
                    DB{ WriteLn "\a/  %%-- untagged clause %_ from deletion: %_\a/", c.id(), c; }
                    for (uint j = 0; j < c.size(); j++){     // -- keep the variables of untagged clauses
                        Var x = c[j].id;
                        if (xs.exclude(x)){
                            DB{ WriteLn "\a/  %%-- not removing variable: x%_\a/", x; }
                            stats.stuck_vars++;
                            kept.push(x); }
                    }
                }
            }
        }
    }

    // Sanity check: (deleted vars should not be top-level reasons at this point)
    for (Var x = 0; x < nVars(); x++){
        if (xs.has(x)) continue;
        GClause r = reason(x);
        if (r == GClause_NULL) continue;
        if (r.isLit()) assert(!xs.has(r.lit().id));
        else           assert(!r.clause(MEM)->tag());
    }

    // Clear watcher lists:
    for (uint i = 0; i < watches.size(); i++){
        Lit p = Lit(packed_, i);
        if (xs.has(p.id)){
            Array<GClause> cs = wlGet(p);
            for (uint j = 0; j < cs.size(); j++)
                if (cs[j].isLit())
                    bin_deleted++;
            wlClear(p);
        }else{
            Array<GClause> cs = wlGet(p);
            uint j = 0;
            for (uind k = 0; k < cs.size(); k++){
#if defined(BLOCKING_LITERALS)
                assert(!cs[k].isBLit());
#endif
                bool keep = cs[k].isLit() ? (!xs.has(cs[k].lit().id)) :
                                            !cs[k].clause(MEM)->tag();
#if !defined(BLOCKING_LITERALS)
                if (keep){
                    cs[j] = cs[k];
                    j++;
                }else if (cs[k].isLit())
                    bin_deleted--;
#else
                if (keep){
                    cs[j] = cs[k];
                    j++;
                    if (!cs[k].isLit()){
                        k++;
                        assert(cs[k].isBLit());
                        cs[j] = cs[k];
                        j++;
                    }
                }else if (cs[k].isLit())
                    bin_deleted--;
                else
                    k++;
#endif
            }
            wlShrink(p, j);
        }
    }

    // Delete clauses:
    for (int type = 0; type < 2; type++){
        Vec<GClause>& cs = type ? learnts : clauses;
        uint j = 0;
        for (uint i = 0; i < cs.size(); i++){
            assert(!cs[i].isLit());
            Clause& c = *cs[i].clause(MEM);
            if (c.tag()){
                removeClause(cs[i], false, false, false);
            }else
                cs[j] = cs[i],
                j++;
        }
        cs.shrinkTo(j);
    }

    // Clear and recycle variables:
    xs.compact();
    for (uint i = 0; i < xs.list().size(); i++){
        Var x = xs.list()[i];
        if (x >= nVars()) continue;

        assert(wlGet( Lit(x)).size() == 0);
        assert(wlGet(~Lit(x)).size() == 0);

        vdata[x] = MSVarData();
        activity[x] = 0;
        polarity[x] = 1;
        if (order.has(x))
            order.remove(x);

        bool was_free = free_vars.add(x);
        /**/if (!(!was_free)) WriteLn "Trying to remove alreade deleted variable: x%_", x;
        assert(!was_free);
    }

    // Remove variables from trail:
    uint j = 0;
    for (uind i = 0; i < trail.size(); i++){
        if (!xs.has(trail[i].id))
            trail[i] = trail[j++];
    }
    trail.shrinkTo(j);
    qhead = trail.size();       // -- don't forget to update propagation pointer...

    n_bin_clauses -= bin_deleted / 2;

    // Consistency check:
  #if defined(ZZ_DEBUG)
    proof.verifyFreeFrom(xs);
  #endif
}


// EXPERIMENTAL!
template<bool pfl>
void MiniSat<pfl>::randomizeVarOrder(uint64& seed, bool rnd_polarity)
{
    // Randomize activity:
#if 0
    for (uint x = 0; x < nVars(); x++){
        if (!hasVar(x)) continue;
        activity[x] = drand(seed);
    }
    var_inc = 1;
    order.heapify();

#else
    Vec<Var> vars(reserve_, nVars());
    for (uint x = 0; x < nVars(); x++){
        if (!hasVar(x)) continue;
        vars.push(x);
        activity[x] = 0;
    }
    shuffle(seed, vars);

    order.clear();
    for (uint i = 0; i < vars.size(); i++)
        order.push(vars[i]);
    order.heapify();
#endif

    if (rnd_polarity){
        // Randomize polarity:
        uint   i = 0;
        uint64 r = irandl(seed);
        for (uint x = 0; x < nVars(); x++){
            polarity[x] = (r & 1);
            i++;
            if (i < 64)
                r >>= 1;
            else{
                i = 0;
                r = irandl(seed);
            }
        }
    }
}


// Top-level solve function. Assumptions are treated as temporary unit clauses added to the clause
// database. If problem is UNSAT (return value 'l_False'). The conflicting assumptions are stored
// in member variable 'conflict' (in the form of a learned clause, so they are negated).  If this
// vector is empty, the SAT solver should not be used anymore (it is forever UNSAT).
template<bool pfl>
lbool MiniSat<pfl>::solve_(const Vec<Lit>& assumps0)
{
    if (debug_api_out)
        *debug_api_out |= "solve(%_)", assumps0;

    if (!ok){ return l_False; }
#if defined(KEEP_TOPLEVEL_LITERALS)
    if (propagate() != NULL){
        conflict.clear();
        conflict_id = clause_id_NULL;
        return l_False;
    }
#endif

    if (timeout_cb != NULL && timeout != UINT64_MAX && vt > 0){
        uint64 work = vt;
        vt = 0;
        if (!timeout_cb(work, timeout_cb_data))
            return l_Undef;
    }

    // Clean up assumptions:
    Vec<Lit> as(copy_, assumps0);
    sortUnique(as);
    for (uind i = 0; i < as.size(); i++)
        assert(as[i].id < nVars());
    for (uint i = 1; i < as.size(); i++){
        if (as[i-1] == ~as[i]){
            conflict.setSize(2, lit_Undef);
            conflict[0] =  as[i];
            conflict[1] = ~as[i];
            conflict_id = clause_id_NULL;   // -- This is the only situation in which 'conflict_id' can be null (conflicting assumptions).
            return l_False;
        }
    }

    if (as.size() == assumps0.size())       // -- if user has been well behaved and have no duplicates, preserve his order of assumptions
        assumps0.copyTo(as);

    // Check if new assumptions are compatible with previous assumptions:
    for (uint i = 0; i < as.size(); i++){
        if (i >= assumps.size() || assumps[i] != as[i] || value(assumps[i]) == l_False){
            undo(i);    // -- undo to the level of discrepancy
            break;
        }
    }

    // Store assumptions:
    as.copyTo(assumps);

    // Make sure variables of assumptions exists:
    for (uint i = 0; i < assumps.size(); i++){
        Var x = assumps[i].id;
        while (x >= nVars()) newVar();
    }

    // Do initial simplification:
    simplifyDB_intern();
    if (!ok){
        assert(!pfl || conflict_id != clause_id_NULL);
        return l_False; }

    // Search:
    if (verbosity >= 1)
        printProgressHeader();

    double nof_conflicts = 100;
    double nof_learnts   = nClauses() / 3;
//    double nof_learnts   = 1000 + nLearnts() * 0.95;
    double conflicts_inc = 1.5;
    double learnts_inc   = 1.1;
    lbool  status        = l_Error;        // -- "error" means "not done". "undef" is used for timeouts.

    double restart_luby_start = 100;
    double restart_luby_inc   = 2;
    double max_nof_conflicts  = 0;
    double orig_nof_conflicts = nof_conflicts;
    double orig_nof_learnts   = nof_learnts;
    double total_conflicts    = 0;
    int    curr_restarts      = 0;

    for(;;){
        nof_conflicts = pow(restart_luby_inc, (double)lubyLog(curr_restarts)) * restart_luby_start;
        total_conflicts += nof_conflicts;
        if (verbosity >= 1 && (newMax(max_nof_conflicts, nof_conflicts) || status != l_Error))
            printProgressLine();

        if (status != l_Error) break;
        status = search((int)nof_conflicts, (int)nof_learnts);
        nof_learnts = orig_nof_learnts * pow(learnts_inc, log(total_conflicts / orig_nof_conflicts) / log(conflicts_inc));
        curr_restarts++;
    }

    if (verbosity >= 1)
        printProgressFooter();

    return status;
}


template<bool pfl>
lbool MiniSat<pfl>::solve(const Vec<Lit>& assumps)
{
    uint64 inspections0 = stats.inspections;
    double time0 = cpuTime();
    lbool  result = solve_(assumps);
    double time1 = cpuTime();

    stats.solves++;
    stats.time += time1 - time0;
    if (result == l_True){
        stats.solves_sat++;
        stats.inspections_sat += stats.inspections - inspections0;
        stats.time_sat += time1 - time0;
    }else if (result == l_False){
        stats.solves_unsat++;
        stats.inspections_unsat += stats.inspections - inspections0;
        stats.time_unsat += time1 - time0;
    }

    return result;
}

//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Text interface (essentially for debugging):


static
void strToLits(String text, /*out*/Vec<Lit>& ps)
{
    cchar* p = text.c_str();
    for(;;){
        while (isWS(*p)) p++;
        if (*p == 0) break;

        bool sign = false;
        if (*p == '-' || *p == '~'){
            sign = true;
            p++; }

        if (isIdentChar0(*p))
            p++;

        uint x = 0;
        try{
            x = parseUInt(p);
        }catch(...){
            Throw(Excp_SatInvalidString) "Not a well-formed list of literals: %_", text;
        }

        ps.push(Lit(x) ^ sign);
    }
}


template<bool pfl>
void MiniSat<pfl>::addClause(String text_lits)
{
    Vec<Lit> ps;
    strToLits(text_lits, ps);
    for (uind i = 0; i < ps.size(); i++){
        Var x = ps[i].id;
        while (x >= nVars())
            addVar();
    }

    addClause(ps);
}


template<bool pfl>
lbool MiniSat<pfl>::solve(String text_assumps)
{
    Vec<Lit> ps;
    strToLits(text_assumps, ps);
    return solve(ps);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


// -- should be 'static' but buggy Sun compiler chokes.
void writeClause(Out& out, Vec<Lit>& ps, uint& n_vars, uint& n_clauses, bool do_output)
{
    for (uind i = 0; i < ps.size(); i++){
        if (do_output){
            if (ps[i].sign) out += '-';
            out += ps[i].id + 1u, ' ';
        }
        n_vars = max_(n_vars, ps[i].id + 1u);
    }

    if (do_output)
        out += '0', ' ', '\n';

    n_clauses++;
}


template<bool pfl>
void MiniSat<pfl>::exportCnf(Out& out, bool exclude_units)
{
    undo(0);

    // Propagate first?
    if (exclude_units)
        if (propagate() != NULL)

    // UNSAT? Write minimal unsatisfiable problem:
    if (!ok){
        out += "p cnf 0 1\n0\n";
        return;
    }

    // Export CNF:
    for (uint do_output = 0; do_output < 2; do_output++){
        Vec<Lit> ps;
        uint     n_vars    = 0;
        uint     n_clauses = 0;

        if (!exclude_units){
            // Write unit clauses:
            for (uint i = 0; i < nVars(); i++){
                if (value(i) != l_Undef){
                    ps.clear();
                    ps.push(Lit(i, (value(i) == l_False)));
                    writeClause(out, ps, n_vars, n_clauses, do_output);
                }
            }
        }

        if (!pfl){
            // Write binary clauses:
            for (uint i = 0; i < 2*nVars(); i++){
                Lit p  = Lit(packed_, i);
                Array<GClause> ws = wlGet(~p);
                for (uint j = 0; j < ws.size(); j++){
                    if (ws[j].isLit() && ws[j].lit() < p){
                        ps.clear();
                        ps.push(p);
                        ps.push(ws[j].lit());
                        writeClause(out, ps, n_vars, n_clauses, do_output);
                    }
                }
            }
        }

        // Write normal clauses:
        for (uind i = 0; i < clauses.size(); i++){
            Clause& c = *clauses[i].clause(MEM);
            for (uint j = 0; j < c.size(); j++)
                if (value(c[j]) == l_True)
                    goto Skip;
            ps.clear();
            for (uint j = 0; j < c.size(); j++){
                if (value(c[j]) == l_Undef)
                    ps.push(c[j]);
            }
            writeClause(out, ps, n_vars, n_clauses, do_output);
          Skip:;
        }

        if (!do_output)
            out += "p cnf ", n_vars, ' ', n_clauses, '\n';
    }
}



template<bool pfl>
void MiniSat<pfl>::dumpState()
{
    WriteLn "ok=%_", ok;

    WriteLn "clauses:";
    for (uint i = 0; i < clauses.size(); i++){
        assert(!clauses[i].isLit());
        Clause& c = *clauses[i].clause(MEM);
        WriteLn "  @%_ : %_", clauses[i].offset(), c;
    }

    WriteLn "learnts:";
    for (uint i = 0; i < learnts.size(); i++){
        assert(!learnts[i].isLit());
        Clause& c = *learnts[i].clause(MEM);
        WriteLn "  @%_ : %_", learnts[i].offset(), c;
    }

    WriteLn "watches:";
    for (uint i = 0; i < watches.size(); i++){
        Lit p = Lit(packed_, i);
        Array<GClause> cs = wlGet(p);
        if (cs.size() == 0) continue;

        Write "  %C%_:", (i & 1) ? '\0' : ' ', p;
        for (uint j = 0; j < cs.size(); j++){
            if (cs[j].isLit()) Write " <%_>", cs[j].lit();
            else               Write " @%_", cs[j].offset();
        }
        NewLine;
    }

    WriteLn "variables:";
    for (Var x = 0; x < nVars(); x++){
        String r;
        if      (reason(x) == GClause_NULL) r += '-';
        else if (reason(x).isLit())         r %= "<%_>", reason(x).lit();
        else                                r %= "@%_" , reason(x).offset();

        WriteLn "  x%_:   value=%c   activity=%.6f   polarity=%_   level=%_   reason=%_",
            x, name(assign(x)), activity[x], polarity[x] ? '-' : '+', level(x), r;
    }

    Write "order:";
    IdHeap<double,1> h;
    order.copyTo(h);
    while (h.size() > 0)
        Write " x%_", h.pop();
    NewLine;

    WriteLn "#binary clauses: %_", n_bin_clauses;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Snapshot:


template<bool pfl>
void MiniSat<pfl>::mergeStatsFrom(MiniSat& other)
{
    // <<== later
}


// Copies everthing except:
//
//   - temporaries
//   - statistics
//   - debug output streams
//
// Note that timeout counters and callbacks ARE copied.
//
template<bool pfl>
void MiniSat<pfl>::copyTo(MiniSat<pfl>& other) const
{
  #if defined(BUMP_EXPERIMENT) || defined(EXPERIMENTAL)
    assert(false);
  #endif
    other.clear(true, true);

    cpy(mem_lits, other.mem_lits);
    cpy(n_free_lits, other.n_free_lits);
    cpy(ok, other.ok);
    cpy(clauses, other.clauses);
    cpy(learnts, other.learnts);
    cpy(unit_id, other.unit_id);
    cpy(n_bin_clauses, other.n_bin_clauses);
    cpy(n_bin_learnts, other.n_bin_learnts);
    cpy(cla_inc, other.cla_inc);
    cpy(cla_decay, other.cla_decay);
    cpy(activity, other.activity);
    cpy(polarity, other.polarity);
    cpy(var_inc, other.var_inc);
    cpy(var_decay, other.var_decay);
    cpy(order, other.order);
    cpy(order_main, other.order_main);
    cpy(enabled_vars, other.enabled_vars);
    cpy(random_seed, other.random_seed);
    cpy(assumps, other.assumps);
    cpy(vdata, other.vdata);
    cpy(free_vars, other.free_vars);
    cpy(watches, other.watches);
    cpy(trail, other.trail);
    cpy(trail_lim, other.trail_lim);
    cpy(qhead, other.qhead);
    cpy(simpDB_assigns, other.simpDB_assigns);
    cpy(simpDB_props, other.simpDB_props);
    cpy(n_literals, other.n_literals);
    cpy(proof, other.proof);
    cpy(variable_decay, other.variable_decay);
    cpy(clause_decay, other.clause_decay);
    cpy(random_var_freq, other.random_var_freq);
    cpy(verbosity, other.verbosity);
    cpy(conflict, other.conflict);
    cpy(conflict_id, other.conflict_id);
    cpy(timeout, other.timeout);
    cpy(timeout_cb, other.timeout_cb);
    cpy(timeout_cb_data, other.timeout_cb_data);
    cpy(cc_cb, other.cc_cb);
    cpy(cc_cb_data, other.cc_cb_data);

    // Copy all external watcher lists:
    for (uind i = 0; i < other.watches.size(); i++){
        WHead& w = other.watches[i];
        if (w.isExt()){
            GClause* src = w.ext_.data;
            uint sz = w.size();
            w.ext_.data = wl_alloc(sz);
            w.ext_.cap  = sz;
            for (uint j = 0; j < sz; j++)
                w.ext_.data[j] = src[j];
        }
    }
}


// Moves everything.
template<bool pfl>
void MiniSat<pfl>::moveTo(MiniSat<pfl>& other)
{
    other.clear(true, true);
    memcpy(&other, this, sizeof(*this));
    new (this) MiniSat<pfl>(proof.iterator());
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Instantiate solvers:


template class MiniSat<false>;
template class MiniSat<true>;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
