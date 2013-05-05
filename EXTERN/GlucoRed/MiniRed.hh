//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#ifndef minired_h
#define minired_h
#include"Reducer.hh"
#include"Work.hh"
#include"ExtSolver.hh"
#include<pthread.h>

namespace GlucoRed {

// The extension of the solver that includes the reducer. This class forms the full implementation
// of the solver/reducer architecture for both MiniRed AND GlucoRed.
class MiniRed : public ExtSolver {
public:
    MiniRed ();
    ~MiniRed ();

    // Overloaded 'search' and 'solve_' functions. 
    // Unfortunately these require a modification in the original solver sources to make them virtual
    virtual lbool search (int nof_conflicts);
    virtual lbool solve_ ();

    // Statistics (counted in 'search')
    uint64_t reducer_backtracks,
         reducer_backtracks_tozero,
         reducer_backtrack_levels,
         reducer_backtrack_level_before;

    // Other statistics
    uint64_t reducer_in,
         reducer_in_lits,
         reducer_out,
         reducer_out_lits,
         reducer_notout_lits,
         workset_in,
         workset_in_lits,
         workset_deleted,
         workset_deleted_lits;

    // Entry point for the second thread, should not be called directly by owner of a 'MiniRed' instance
    void threadGo ();
protected:
    // Called by the solver thread to add 'c' to the work set
    bool submitToReducer (const vec<Lit>& c, int metric);
    void foundResult     (lbool sat, bool signal);

    lbool              result;
    volatile bool      stop;
    int                nhead;
    int                offset;

    Reducer            reducer;
    Work               work;
    pthread_t          posix;
    pthread_cond_t     cond;

    vec<vec<Lit>* >    newReduced; // "newReduced" contains clauses the solver has read from "reduced" but has not attached yet
    vec<vec<Lit>* >    reduced;    // "reduced" is the result queue, transporting clauses from reducer to solver

    pthread_mutex_t    mutex;
    pthread_spinlock_t resultLock;
};

};

#endif
