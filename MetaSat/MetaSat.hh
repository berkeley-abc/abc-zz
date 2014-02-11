//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MetaSat.hh
//| Author(s)   : Niklas Een
//| Module      : MetaSat
//| Description : Wrapper for non-proof-logging SAT solvers supporting the assumption interface.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________


#ifndef ZZ__MetaSat__MetaSat_hh
#define ZZ__MetaSat__MetaSat_hh

#include "ZZ/Generics/Lit.hh"


// Forward declarations:
namespace ZZ {
    template<bool pfl> class MiniSat;
};

namespace Minisat {
    struct Solver;
    struct SimpSolver;
}

namespace abc_sat {
    struct sat_solver_t;
    typedef struct sat_solver_t sat_solver;
    typedef int lit;
};

namespace Glucose {
    struct Solver;
}

namespace GlucoRed {
    struct SolRed;
}

namespace MiniRed {
    struct SolRed;
}


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'MetaSat' -- interface class:


struct MetaSat {
  //________________________________________
  //  Construction:

    virtual ~MetaSat() {}
    virtual void clear(bool dealloc = true) = 0;

  //________________________________________
  //  Problem specification:

    virtual Lit   True() const = 0;
    virtual Lit   addLit() = 0;
    virtual void  addClause_(const Vec<Lit>& ps) = 0;
    virtual void  recycleLit(Lit p) = 0;                    // -- For non-supporting solvers, just adds a unit clause.

  //________________________________________
  //  Solving:

    virtual void  setConflictLim(uint64 n_confl) = 0;       // -- Affects the next call to 'solve()' only.
    virtual lbool solve_(const Vec<Lit>& assumps) = 0;
    virtual void  randomizeVarOrder(uint64 seed) = 0;       // -- EXPERIMENTAL.

  //________________________________________
  //  Result extraction:

    virtual bool  okay() const = 0;
    virtual lbool value_(uint x) const = 0;                 // -- Last operation must have been 'solve()', not 'addClause()'.
    virtual void  getModel(Vec<lbool>& m) const = 0;
    virtual void  getConflict(Vec<Lit>& assump_confl) = 0;  // -- Returns a cube (not a clause) which is a subset of 'assumps' passed to 'solve()'.

    virtual double getActivity(uint x) const = 0;

  //________________________________________
  //  Statistics:

    virtual uint  nClauses() const = 0;
    virtual uint  nLearnts() const = 0;
    virtual uint  nConflicts() const = 0;
    virtual uint  nVars () const = 0;

  //________________________________________
  //  Preprocessing:

    virtual void  freeze(uint x) = 0;                       // }- Has no effect except for simplifying solver.
    virtual void  thaw(uint x) = 0;                         // }
    virtual void  preprocess(bool final_call) = 0;          // -- If 'final_call' is TRUE, internal data will be freed to save memory, but no more preprocessing is possible.
    virtual void  getCnf(Vec<Lit>& out_cnf) = 0;            // -- Read back CNF as a sequence of clauses separated by 'lit_Undef's.

  //________________________________________
  //  Debug:

    virtual void  setVerbosity(int verb_level) = 0;         // -- '0' means no output.
    virtual bool  exportCnf(const String& filename) = 0;    // -- to use with external solver for benchmarking purposes (returns FALSE if file could not be created)

  //________________________________________
  //  Convenience:

    void  addClause(const Vec<Lit>& ps)         { addClause_(ps); }
    void  addClause(Lit p)                      { cl_tmp.setSize(1, Lit_NULL); cl_tmp[0] = p; addClause(cl_tmp); }
    void  addClause(Lit p, Lit q)               { cl_tmp.setSize(2, Lit_NULL); cl_tmp[0] = p; cl_tmp[1] = q; addClause(cl_tmp); }
    void  addClause(Lit p, Lit q, Lit r)        { cl_tmp.setSize(3, Lit_NULL); cl_tmp[0] = p; cl_tmp[1] = q; cl_tmp[2] = r; addClause(cl_tmp); }
    void  addClause(Lit p, Lit q, Lit r, Lit s) { cl_tmp.setSize(4, Lit_NULL); cl_tmp[0] = p; cl_tmp[1] = q; cl_tmp[2] = r; cl_tmp[3] = s; addClause(cl_tmp); }

    lbool solve(const Vec<Lit>& assumps) { return solve_(assumps); }
    lbool solve()                        { Vec<Lit> tmp; return solve(tmp); }
    lbool solve(Lit p)                   { Vec<Lit> tmp; tmp.push(p); return solve(tmp); }
    lbool solve(Lit p, Lit q)            { Vec<Lit> tmp; tmp.push(p); tmp.push(q); return solve(tmp); }

    lbool value(uint x) const            { return value_(x); }
    lbool value(Lit  p) const            { return value_(p.id) ^ sign(p); }

protected:
    Vec<Lit> cl_tmp;
};


#define MetaSat_OVERRIDES                                               \
    virtual void  clear(bool dealloc = false);                          \
    virtual Lit   True() const;                                         \
    virtual Lit   addLit();                                             \
    virtual void  addClause_(const Vec<Lit>& ps);                       \
    virtual void  recycleLit(Lit p);                                    \
    virtual void  setConflictLim(uint64 n_confl);                       \
    virtual lbool solve_(const Vec<Lit>& assumps);                      \
    virtual void  randomizeVarOrder(uint64 seed);                       \
    virtual bool  okay() const;                                         \
    virtual lbool value_(uint x) const;                                 \
    virtual void  getModel(Vec<lbool>& m) const;                        \
    virtual void  getConflict(Vec<Lit>& assump_confl);                  \
    virtual double getActivity(uint x) const;                           \
    virtual uint  nClauses() const;                                     \
    virtual uint  nLearnts() const;                                     \
    virtual uint  nConflicts() const;                                   \
    virtual uint  nVars () const;                                       \
    virtual void  freeze(uint x);                                       \
    virtual void  thaw(uint x);                                         \
    virtual void  preprocess(bool final_call);                          \
    virtual void  getCnf(Vec<Lit>& out_cnf);                            \
    virtual void  setVerbosity(int verb_level);                         \
    virtual bool  exportCnf(const String& filename);



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Sub-classes:


struct minisat2_vec_data {
    void* data;
    int   sz;
    int   cap;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct ZzSat : MetaSat {
    ZzSat();
    virtual ~ZzSat();

    MetaSat_OVERRIDES

private:
    MiniSat<false>* S;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct MiniSat2 : MetaSat {
    MiniSat2();
    virtual ~MiniSat2();

    MetaSat_OVERRIDES

private:
    ::Minisat::Solver* S;
    Lit true_lit;
    minisat2_vec_data tmp_lits;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct MiniSat2s : MetaSat {
    MiniSat2s();
    virtual ~MiniSat2s();

    MetaSat_OVERRIDES

private:
    ::Minisat::SimpSolver* S;
    Lit true_lit;
    minisat2_vec_data tmp_lits;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct AbcSat : MetaSat {
    AbcSat();
    virtual ~AbcSat();

    MetaSat_OVERRIDES

private:
    ::abc_sat::sat_solver* S;
    Lit                    true_lit;
    Vec<abc_sat::lit>      tmp;
    uint64                 confl_lim;
    bool                   ok;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct GluSat : MetaSat {
    GluSat();
    virtual ~GluSat();

    MetaSat_OVERRIDES

private:
    ::Glucose::Solver* S;
    Lit true_lit;
    minisat2_vec_data tmp_lits;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct GlrSat : MetaSat {
    GlrSat();
    virtual ~GlrSat();

    MetaSat_OVERRIDES

private:
    ::GlucoRed::SolRed* S;
    Lit true_lit;
    minisat2_vec_data tmp_lits;
};


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


struct MiniRedSat : MetaSat {
    MiniRedSat();
    virtual ~MiniRedSat();

    MetaSat_OVERRIDES

private:
    ::MiniRed::SolRed* S;
    Lit true_lit;
    minisat2_vec_data tmp_lits;
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'MultiSat' -- Dynamically become anyone of the wrapped solvers:


enum SolverType {
    sat_NULL,
    sat_Zz,         // The ZZ framework's version of MiniSat (based on v1.14 + improvements)
    sat_Msc,        // MiniSat 2.2, core version
    sat_Mss,        // MiniSat 2.2, simplifying version
    sat_Abc,        // ABC's MiniSat
    sat_Glu,        // Glucose 2.2
    sat_Glr,        // GlucoRed by Siert Wieringa
    sat_Msr,        // MiniRed by Siert Wieringa
};


struct MultiSat : MetaSat {
    MetaSat*   S;
    SolverType type;

    MultiSat(SolverType t = sat_NULL) : S(NULL), type(sat_NULL) { selectSolver(t); }
   ~MultiSat() { selectSolver(sat_NULL); }

    void selectSolver(SolverType type); // -- pass 'sat_NULL' to dispose the current solver.

    virtual void   clear(bool dealloc = false)         { S->clear(dealloc); }
    virtual Lit    True() const                        { return S->True(); }
    virtual Lit    addLit()                            { return S->addLit(); }
    virtual void   addClause_(const Vec<Lit>& ps)      { S->addClause_(ps); }
    virtual void   recycleLit(Lit p)                   { S->recycleLit(p); }
    virtual void   setConflictLim(uint64 n_confl)      { S->setConflictLim(n_confl); }
    virtual lbool  solve_(const Vec<Lit>& assumps)     { return S->solve_(assumps); }
    virtual void   randomizeVarOrder(uint64 seed)      { S->randomizeVarOrder(seed); }
    virtual bool   okay() const                        { return S->okay(); }
    virtual lbool  value_(uint x) const                { return S->value_(x); }
    virtual void   getModel(Vec<lbool>& m) const       { S->getModel(m); }
    virtual void   getConflict(Vec<Lit>& assump_confl) { S->getConflict(assump_confl); }
    virtual double getActivity(uint x) const           { return S->getActivity(x); }
    virtual uint   nClauses() const                    { return S->nClauses(); }
    virtual uint   nLearnts() const                    { return S->nLearnts(); }
    virtual uint   nConflicts() const                  { return S->nConflicts(); }
    virtual uint   nVars () const                      { return S->nVars(); }
    virtual void   freeze(uint x)                      { S->freeze(x); }
    virtual void   thaw(uint x)                        { S->thaw(x); }
    virtual void   preprocess(bool final_call)         { S->preprocess(final_call); }
    virtual void   getCnf(Vec<Lit>& out_cnf)           { S->getCnf(out_cnf); }
    virtual void   setVerbosity(int verb_level)        { S->setVerbosity(verb_level); }
    virtual bool   exportCnf(const String& filename)   { return S->exportCnf(filename); }
};


inline void MultiSat::selectSolver(SolverType type)
{
    if (S) delete S;

    switch (type){
    case sat_NULL: S = NULL            ; break;
    case sat_Zz:   S = new ZzSat()     ; break;
    case sat_Msc:  S = new MiniSat2()  ; break;
    case sat_Mss:  S = new MiniSat2s() ; break;
    case sat_Abc:  S = new AbcSat()    ; break;
    case sat_Glu:  S = new GluSat()    ; break;
    case sat_Glr:  S = new GlrSat()    ; break;
    case sat_Msr:  S = new MiniRedSat(); break;
    default: assert(false); }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
