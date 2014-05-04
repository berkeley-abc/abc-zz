//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MetaSat.cc
//| Author(s)   : Niklas Een
//| Module      : MetaSat
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "MetaSat.hh"
#include "SiertSat.hh"
#include "ZZ_MiniSat.hh"
#include "ZZ_MetaSat.AbcSat.hh"
#include "ZZ_MetaSat.MiniSat2.hh"
#include "ZZ/Generics/Sort.hh"

namespace MS = ::Minisat;
namespace AS = ::abc_sat;
namespace GL = ::Glucose;
namespace GR = ::GlucoRed;
namespace MR = ::MiniRed;

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ZZ MiniSat wrapper:


ZzSat::ZzSat()
{
    S = new MiniSat<false>();
}


ZzSat::~ZzSat()
{
    if (S) delete S;
}


void ZzSat::clear(bool dealloc)
{
    S->clear(dealloc);
}


Lit ZzSat::True() const
{
    return S->True();
}


Lit ZzSat::addLit()
{
    return S->addLit();
}


void ZzSat::addClause_(const Vec<Lit>& ps)
{
    S->addClause(ps);
}


void ZzSat::recycleLit(Lit p)
{
    S->addClause(p);    // -- not supported, just set 'p' to TRUE.
}


void ZzSat::setConflictLim(uint64 n_confl)
{
    assert(false);      // <<== later; need to use callback function for this
}


lbool ZzSat::solve_(const Vec<Lit>& assumps)
{
    return S->solve(assumps);
}


void ZzSat::randomizeVarOrder(uint64 seed)
{
    S->randomizeVarOrder(seed);
}


bool ZzSat::okay() const
{
    return S->okay();
}


lbool ZzSat::value_(uint x) const
{
    return S->value(x);
}


void ZzSat::getModel(Vec<lbool>& m) const
{
    return S->getModel(m);
}


void ZzSat::getConflict(Vec<Lit>& confl)
{
    return S->getConflict(confl);
}


double ZzSat::getActivity(uint x) const
{
    return S->getActivity(x);
}


uint ZzSat::nClauses() const
{
    return S->nClauses();
}


uint ZzSat::nLearnts() const
{
    return S->nLearnts();
}


uint ZzSat::nConflicts() const
{
    return S->statistics().conflicts;
}


uint ZzSat::nVars() const
{
    return S->nVars();

}


void ZzSat::freeze(uint x)
{
    /*nothing*/
}


void ZzSat::thaw(uint x)
{
    /*nothing*/
}


void ZzSat::preprocess(bool /*final_call*/)
{
    S->simplifyDB();
}


void ZzSat::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void ZzSat::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool ZzSat::exportCnf(const String& filename)
{
    S->exportCnf(filename);
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// MiniSat 2.2 wrapper:


typedef MS::vec<MS::Lit> MS_litvec;

macro Lit fromMs(MS::Lit p) { Lit ret; ret.id = MS::var(p); ret.sign = MS::sign(p); return ret; }
macro MS::Lit toMs(Lit p) { return MS::mkLit(p.id, p.sign); }

macro lbool fromMs(MS::lbool v) { return (v == MS::l_True)  ? l_True : (v == MS::l_False) ? l_False : l_Undef; }


MiniSat2::MiniSat2()
{
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
    new (&tmp) MS_litvec;

    S = new MS::Solver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
}


MiniSat2::~MiniSat2()
{
    if (S) delete S;
}


void MiniSat2::clear(bool dealloc)
{
    S->~Solver();
    new (S) MS::Solver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    Lit true_lit = addLit();
    MetaSat::addClause(true_lit);
}


Lit MiniSat2::True() const
{
    return true_lit;
}


Lit MiniSat2::addLit()
{
    return fromMs(MS::mkLit(S->newVar()));
}


void MiniSat2::addClause_(const Vec<Lit>& ps)
{
    MS_litvec& tmp = *(MS_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toMs(ps[i]));

    S->addClause(tmp);
}


void MiniSat2::recycleLit(Lit p)
{
    S->releaseVar(toMs(p));
}


void MiniSat2::setConflictLim(uint64 n_confl)
{
    newMin(n_confl, (uint64)INT64_MAX);
    S->setConfBudget(n_confl);
}


lbool MiniSat2::solve_(const Vec<Lit>& assumps)
{
    MS_litvec& tmp = *(MS_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toMs(assumps[i]));

    lbool ret = fromMs(S->solveLimited(tmp));
    S->budgetOff();
    return ret;
}


void MiniSat2::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool MiniSat2::okay() const
{
    return S->okay();
}


lbool MiniSat2::value_(uint x) const
{
    return fromMs(S->modelValue(x));
}


void MiniSat2::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = fromMs(S->modelValue(i));
}


void MiniSat2::getConflict(Vec<Lit>& confl)
{
    confl.clear();
    for (int i = 0; i < S->conflict.size(); i++)
        confl.push(~fromMs(S->conflict[i]));
}


double MiniSat2::getActivity(uint x) const
{
    return S->activity[x] / S->var_inc;
}


uint MiniSat2::nClauses() const
{
    return S->nClauses();
}


uint MiniSat2::nLearnts() const
{
    return S->nLearnts();
}


uint MiniSat2::nConflicts() const
{
    return S->conflicts;
}


uint MiniSat2::nVars() const
{
    return S->nVars();

}


void MiniSat2::freeze(uint x)
{
    /*nothing*/
}


void MiniSat2::thaw(uint x)
{
    /*nothing*/
}


void MiniSat2::preprocess(bool /*final_call*/)
{
    /*nothing*/
}


void MiniSat2::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void MiniSat2::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool MiniSat2::exportCnf(const String& filename)
{
    return S->exportCnf(filename.c_str());
}


//=================================================================================================
// -- simplifying version:


MiniSat2s::MiniSat2s()
{
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
    new (&tmp) MS_litvec;

    S = new MS::SimpSolver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
}


MiniSat2s::~MiniSat2s()
{
    if (S) delete S;
}


void MiniSat2s::clear(bool dealloc)
{
    S->~SimpSolver();
    new (S) MS::SimpSolver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    Lit true_lit = addLit();
    MetaSat::addClause(true_lit);
}


Lit MiniSat2s::True() const
{
    return true_lit;
}


Lit MiniSat2s::addLit()
{
    return fromMs(MS::mkLit(S->newVar()));
}


void MiniSat2s::addClause_(const Vec<Lit>& ps)
{
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toMs(ps[i]));

    S->addClause(tmp);
}


void MiniSat2s::recycleLit(Lit p)
{
    S->releaseVar(toMs(p));
}


void MiniSat2s::setConflictLim(uint64 n_confl)
{
    newMin(n_confl, (uint64)INT64_MAX);
    S->setConfBudget(n_confl);
}


lbool MiniSat2s::solve_(const Vec<Lit>& assumps)
{
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toMs(assumps[i]));

    lbool ret = fromMs(S->solveLimited(tmp));
    S->budgetOff();
    return ret;
}


void MiniSat2s::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool MiniSat2s::okay() const
{
    return S->okay();
}


lbool MiniSat2s::value_(uint x) const
{
    return fromMs(S->modelValue(x));
}


void MiniSat2s::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = fromMs(S->modelValue(i));
}


void MiniSat2s::getConflict(Vec<Lit>& confl)
{
    confl.clear();
    for (int i = 0; i < S->conflict.size(); i++)
        confl.push(~fromMs(S->conflict[i]));
}


double MiniSat2s::getActivity(uint x) const
{
    return S->activity[x] / S->var_inc;
}


uint MiniSat2s::nClauses() const
{
    return S->nClauses();
}


uint MiniSat2s::nLearnts() const
{
    return S->nLearnts();
}


uint MiniSat2s::nConflicts() const
{
    return S->conflicts;
}


uint MiniSat2s::nVars() const
{
    return S->nVars();

}


void MiniSat2s::freeze(uint x)
{
    S->setFrozen(x, true);
}


void MiniSat2s::thaw(uint x)
{
    S->setFrozen(x, false);
}


void MiniSat2s::preprocess(bool final_call)
{
    S->eliminate(final_call);
}


void MiniSat2s::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void MiniSat2s::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool MiniSat2s::exportCnf(const String& filename)
{
    return S->exportCnf(filename.c_str());
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ABC SAT wrapper:


typedef Vec<abc_sat::lit> AS_litvec;

macro Lit fromAs(AS::lit p) { Lit ret; ret.id = AS::lit_var(p); ret.sign = AS::lit_sign(p); return ret; }
macro AS::lit toAs(Lit p) { return AS::toLitCond(p.id, p.sign); }

macro lbool fromAsLbool(AS::lbool v) { return (v == AS::l_True) ? l_True : (v == AS::l_False) ? l_False : l_Undef; }


AbcSat::AbcSat()
{
    S = NULL;
    clear(true);
}


AbcSat::~AbcSat()
{
    if (S){ AS::sat_solver_delete(S); S = NULL; }
}


void AbcSat::clear(bool dealloc)
{
    if (dealloc){
        if (S){ AS::sat_solver_delete(S); S = NULL; }
        S = AS::sat_solver_new();
    }else
        AS::sat_solver_rollback(S);

    ok = true;
    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
    confl_lim = UINT64_MAX;
}


Lit AbcSat::True() const
{
    return true_lit;
}


Lit AbcSat::addLit()
{
    int n_vars = AS::sat_solver_nvars(S);
    AS::sat_solver_setnvars(S, n_vars+1);
    return Lit(n_vars);
}


void AbcSat::addClause_(const Vec<Lit>& ps)
{
    if (!ok) return;

    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toAs(ps[i]));

    ok &= AS::sat_solver_addclause(S, &tmp[0], &tmp.end_());
}


void AbcSat::recycleLit(Lit p)
{
    if (!ok) return;

    MetaSat::addClause(p);      // -- ABC SAT doesn't support recycling, just set 'p' to TRUE.
}


void AbcSat::setConflictLim(uint64 n_confl)
{
    confl_lim = n_confl;
}


lbool AbcSat::solve_(const Vec<Lit>& assumps_)
{
    ok &= AS::sat_solver_simplify(S);
    if (!ok) return l_False;

    Vec<Lit> assumps(copy_, assumps_);
    sortUnique(assumps);

    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toAs(assumps[i]));

    lbool result = fromAsLbool(sat_solver_solve(S, &tmp[0], &tmp.end_(), (confl_lim == UINT64_MAX) ? 0 : confl_lim, 0, 0, 0));

    AS::lit* lits;
    if (result == l_False && AS::sat_solver_final(S, &lits) == 0)
        ok = false;

    return result;
}


void AbcSat::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool AbcSat::okay() const
{
    return ok;
}


lbool AbcSat::value_(uint x) const
{
    return lbool_lift(sat_solver_var_value(S, x));
}


void AbcSat::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = lbool_lift(sat_solver_var_value(S, i));
}


void AbcSat::getConflict(Vec<Lit>& confl)
{
    AS::lit* lits;
    uint sz = AS::sat_solver_final(S, &lits);

    confl.clear();
    for (uint i = 0; i < sz; i++)
        confl.push(~fromAs(lits[i]));
}


double AbcSat::getActivity(uint x) const
{
    return AS::sat_solver_get_activity(S, x);
}


uint AbcSat::nClauses() const
{
    return AS::sat_solver_nclauses(S);
}


uint AbcSat::nLearnts() const
{
    return AS::sat_solver_nlearnts(S);
}


uint AbcSat::nConflicts() const
{
    return AS::sat_solver_nconflicts(S);
}


uint AbcSat::nVars() const
{
    return AS::sat_solver_nvars(S);
}


void AbcSat::freeze(uint x)
{
    /*nothing*/
}


void AbcSat::thaw(uint x)
{
    /*nothing*/
}


void AbcSat::preprocess(bool /*final_call*/)
{
    ok &= AS::sat_solver_simplify(S);
}


void AbcSat::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void AbcSat::setVerbosity(int verb_level)
{
    AS::sat_solver_set_verbosity(S, verb_level);
}


bool AbcSat::exportCnf(const String& filename)
{
    assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Glucose wrapper:


typedef GL::vec<GL::Lit> GL_litvec;

macro Lit fromGl(GL::Lit p) { Lit ret; ret.id = GL::var(p); ret.sign = GL::sign(p); return ret; }
macro GL::Lit toGl(Lit p) { return GL::mkLit(p.id, p.sign); }

macro lbool fromGl(GL::lbool v) { return (v == (GL::lbool((uint8_t)0)))  ? l_True : (v == (GL::lbool((uint8_t)1))) ? l_False : l_Undef; }


GluSat::GluSat()
{
    GL_litvec& tmp  = *(GL_litvec*)(void*)&tmp_lits;
    new (&tmp) GL_litvec;

    S = new GL::Solver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
}


GluSat::~GluSat()
{
    if (S) delete S;
}


void GluSat::clear(bool dealloc)
{
    S->~Solver();
    new (S) GL::Solver;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    Lit true_lit = addLit();
    MetaSat::addClause(true_lit);
}


Lit GluSat::True() const
{
    return true_lit;
}


Lit GluSat::addLit()
{
    return fromGl(GL::mkLit(S->newVar()));
}


void GluSat::addClause_(const Vec<Lit>& ps)
{
    GL_litvec& tmp = *(GL_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toGl(ps[i]));

    S->addClause(tmp);
}


void GluSat::recycleLit(Lit p)
{
    S->addClause(toGl(p));    // -- not supported, just set 'p' to TRUE.
}


void GluSat::setConflictLim(uint64 n_confl)
{
    S->setConfBudget(n_confl);
}


lbool GluSat::solve_(const Vec<Lit>& assumps)
{
    GL_litvec& tmp = *(GL_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toGl(assumps[i]));

    lbool ret = fromGl(S->solveLimited(tmp));
    S->budgetOff();
    return ret;
}


void GluSat::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool GluSat::okay() const
{
    return S->okay();
}


lbool GluSat::value_(uint x) const
{
    return fromGl(S->modelValue(x));
}


void GluSat::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = fromGl(S->modelValue(i));
}


void GluSat::getConflict(Vec<Lit>& confl)
{
    confl.clear();
    for (int i = 0; i < S->conflict.size(); i++)
        confl.push(~fromGl(S->conflict[i]));
}


double GluSat::getActivity(uint x) const
{
    return S->activity[x] / S->var_inc;
}


uint GluSat::nClauses() const
{
    return S->nClauses();
}


uint GluSat::nLearnts() const
{
    return S->nLearnts();
}


uint GluSat::nConflicts() const
{
    return S->conflicts;
}


uint GluSat::nVars() const
{
    return S->nVars();

}


void GluSat::freeze(uint x)
{
    /*nothing*/
}


void GluSat::thaw(uint x)
{
    /*nothing*/
}


void GluSat::preprocess(bool /*final_call*/)
{
    /*nothing*/
}


void GluSat::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void GluSat::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool GluSat::exportCnf(const String& filename)
{
    assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GlucoRed wrapper:


typedef GR::vec<GR::Lit> GR_litvec;

macro Lit fromGr(GR::Lit p) { Lit ret; ret.id = GR::var(p); ret.sign = GR::sign(p); return ret; }
macro GR::Lit toGr(Lit p) { return GR::mkLit(p.id, p.sign); }

macro lbool fromGr(GR::lbool v) { return (v == (GR::lbool((uint8_t)0)))  ? l_True : (v == (GR::lbool((uint8_t)1))) ? l_False : l_Undef; }


GlrSat::GlrSat()
{
    GR_litvec& tmp  = *(GR_litvec*)(void*)&tmp_lits;
    new (&tmp) GR_litvec;

    S = new GR::SolRed;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
}


GlrSat::~GlrSat()
{
    if (S) delete S;
}


void GlrSat::clear(bool dealloc)
{
    S->~SolRed();
    new (S) GR::SolRed;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    Lit true_lit = addLit();
    MetaSat::addClause(true_lit);
}


Lit GlrSat::True() const
{
    return true_lit;
}


Lit GlrSat::addLit()
{
    return fromGr(GR::mkLit(S->newVar()));
}


void GlrSat::addClause_(const Vec<Lit>& ps)
{
    GR_litvec& tmp = *(GR_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toGr(ps[i]));

    S->addClause(tmp);
}


void GlrSat::recycleLit(Lit p)
{
    S->addClause(toGr(p));    // -- not supported, just set 'p' to TRUE.
}


void GlrSat::setConflictLim(uint64 n_confl)
{
    S->setConfBudget(n_confl);
}


lbool GlrSat::solve_(const Vec<Lit>& assumps)
{
    GR_litvec& tmp = *(GR_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toGr(assumps[i]));

    lbool ret = fromGr(S->solveLimited(tmp));
    S->budgetOff();
    return ret;
}


void GlrSat::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool GlrSat::okay() const
{
    return S->okay();
}


lbool GlrSat::value_(uint x) const
{
    return fromGr(S->modelValue(x));
}


void GlrSat::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = fromGr(S->modelValue(i));
}


void GlrSat::getConflict(Vec<Lit>& confl)
{
    confl.clear();
    for (int i = 0; i < S->conflict.size(); i++)
        confl.push(~fromGr(S->conflict[i]));
}


double GlrSat::getActivity(uint x) const
{
    return S->activity[x] / S->var_inc;
}


uint GlrSat::nClauses() const
{
    return S->nClauses();
}


uint GlrSat::nLearnts() const
{
    return S->nLearnts();
}


uint GlrSat::nConflicts() const
{
    return S->conflicts;
}


uint GlrSat::nVars() const
{
    return S->nVars();

}


void GlrSat::freeze(uint x)
{
    /*nothing*/
}


void GlrSat::thaw(uint x)
{
    /*nothing*/
}


void GlrSat::preprocess(bool /*final_call*/)
{
    /*nothing*/
}


void GlrSat::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void GlrSat::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool GlrSat::exportCnf(const String& filename)
{
    assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// MiniRed wrapper:


//typedef MR::vec<MR::Lit> MR_litvec;
typedef MR::vec<MR::Lit> MR_litvec;

macro Lit fromMr(MR::Lit p) { Lit ret; ret.id = MR::var(p); ret.sign = MR::sign(p); return ret; }
macro MR::Lit toMr(Lit p) { return MR::mkLit(p.id, p.sign); }

macro lbool fromMr(MR::lbool v) { return (v == (MR::lbool((uint8_t)0)))  ? l_True : (v == (MR::lbool((uint8_t)1))) ? l_False : l_Undef; }


MiniRedSat::MiniRedSat()
{
    MR_litvec& tmp  = *(MR_litvec*)(void*)&tmp_lits;
    new (&tmp) MR_litvec;

    S = new MR::SolRed;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    true_lit = addLit();
    MetaSat::addClause(true_lit);
}


MiniRedSat::~MiniRedSat()
{
    if (S) delete S;
}


void MiniRedSat::clear(bool dealloc)
{
    S->~SolRed();
    new (S) MR::SolRed;

    Lit null_lit = addLit(); assert(null_lit.id == 0);
    Lit true_lit = addLit();
    MetaSat::addClause(true_lit);
}


Lit MiniRedSat::True() const
{
    return true_lit;
}


Lit MiniRedSat::addLit()
{
    return fromMr(MR::mkLit(S->newVar()));
}


void MiniRedSat::addClause_(const Vec<Lit>& ps)
{
    MR_litvec& tmp = *(MR_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toMr(ps[i]));

    S->addClause(tmp);
}


void MiniRedSat::recycleLit(Lit p)
{
    S->addClause(toMr(p));    // -- not supported, just set 'p' to TRUE.
}


void MiniRedSat::setConflictLim(uint64 n_confl)
{
    S->setConfBudget(n_confl);
}


lbool MiniRedSat::solve_(const Vec<Lit>& assumps)
{
    MR_litvec& tmp = *(MR_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < assumps.size(); i++)
        tmp.push(toMr(assumps[i]));

    lbool ret = fromMr(S->solveLimited(tmp));
    S->budgetOff();
    return ret;
}


void MiniRedSat::randomizeVarOrder(uint64 seed)
{
    /*nothing yet*/
}


bool MiniRedSat::okay() const
{
    return S->okay();
}


lbool MiniRedSat::value_(uint x) const
{
    return fromMr(S->modelValue(x));
}


void MiniRedSat::getModel(Vec<lbool>& m) const
{
    m.setSize(nVars());
    for (uint i = 0; i < nVars(); i++)
        m[i] = fromMr(S->modelValue(i));
}


void MiniRedSat::getConflict(Vec<Lit>& confl)
{
    confl.clear();
    for (int i = 0; i < S->conflict.size(); i++)
        confl.push(~fromMr(S->conflict[i]));
}


double MiniRedSat::getActivity(uint x) const
{
    return S->activity[x] / S->var_inc;
}


uint MiniRedSat::nClauses() const
{
    return S->nClauses();
}


uint MiniRedSat::nLearnts() const
{
    return S->nLearnts();
}


uint MiniRedSat::nConflicts() const
{
    return S->conflicts;
}


uint MiniRedSat::nVars() const
{
    return S->nVars();

}


void MiniRedSat::freeze(uint x)
{
    /*nothing*/
}


void MiniRedSat::thaw(uint x)
{
    /*nothing*/
}


void MiniRedSat::preprocess(bool /*final_call*/)
{
    /*nothing*/
}


void MiniRedSat::getCnf(Vec<Lit>& out_cnf)
{
    assert(false);  // <<== later
}


void MiniRedSat::setVerbosity(int verb_level)
{
    S->verbosity = verb_level;
}


bool MiniRedSat::exportCnf(const String& filename)
{
    assert(false);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
