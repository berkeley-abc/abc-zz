//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MetaSat.cc
//| Author(s)   : Niklas Een
//| Module      : MetaSat
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "MetaSat.hh"
#include "minisat2.hh"

namespace MS = ::Minisat;

namespace ZZ {
using namespace std;


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
    Lit true_lit = addLit();
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
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
    tmp.clear();
    for (uint i = 0; i < ps.size(); i++)
        tmp.push(toMs(ps[i]));

    S->addClause(tmp);
}


void MiniSat2::setConflictLim(uint64 n_confl)
{
    S->setConfBudget(n_confl);
}


lbool MiniSat2::solve_(const Vec<Lit>& assumps)
{
    MS_litvec& tmp  = *(MS_litvec*)(void*)&tmp_lits;
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


uint MiniSat2::nClauses() const
{
    return S->nClauses();
}


uint MiniSat2::nLearnts() const
{
    return S->nLearnts();
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


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
