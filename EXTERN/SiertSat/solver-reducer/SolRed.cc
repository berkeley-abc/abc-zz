//
//     MiniRed/GlucoRed
//
//     Siert Wieringa
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#include"SolRed.h"

#ifdef MINIRED
using namespace MiniRed;
#elif defined GLUCORED
using namespace GlucoRed;
#endif

static IntOption  opt_solver_ccmin_mode (VERSION_STRING, "solver-ccmin", "Conflict clause minimization mode for solver (original -ccmin-mode applies to reducer)", 2, IntRange(0, 2));

// Second thread entry helper
static void* threadEntry(void *t) {
    ((SolRed*) t)->threadGo();
    return NULL;
}

SolRed::SolRed()
    : reducer_backtracks             (0)
    , reducer_backtracks_tozero      (0)
    , reducer_backtrack_levels       (0)
    , reducer_backtrack_level_before (0)

    , reducer_in                     (0)
    , reducer_in_lits                (0)
    , reducer_out                    (0)
    , reducer_out_lits               (0)
    , reducer_notout_lits            (0)
    , workset_in                     (0)
    , workset_in_lits                (0)
    , workset_deleted                (0)
    , workset_deleted_lits           (0)

    , reducerOk                      (true)
    , pauseReducer                   (false)
    , nhead                          (0)
    , offset                         (0)
    , reducer                        () // Instance of Reducer which forms the reducer
{
    ccmin_mode = opt_solver_ccmin_mode;

    // Create posix thread objects
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&workC, NULL);
    pthread_cond_init(&pausedC, NULL);

    // Start reducer
    pthread_create(&posix, NULL, threadEntry, this);
}

SolRed::~SolRed() {
    // Stop reducer
    reducer.interrupt();
    pthread_mutex_lock(&mutex);
    reducerOk = false;
    pthread_cond_signal(&workC);
    pthread_mutex_unlock(&mutex);
    pthread_join(posix, NULL);

    // Destroy posix thread objects
    pthread_cond_destroy(&pausedC);
    pthread_cond_destroy(&workC);
    pthread_mutex_destroy(&mutex);

    // Delete left over work
    while( work.available() )
        delete work.get();

    // Delete left over reduced clauses
    for ( int i = nhead; i < newReduced.size(); i++ )
        delete newReduced[i];
    for ( int i = 0; i < reduced.size(); i++ )
        delete reduced[i];
}


// Overloaded 'solve_' function, will handle solver and reducer
lbool SolRed::solve_() {
    if ( !ok ) return l_False;
    if ( asynch_interrupt ) return l_Undef;
    
    pthread_mutex_lock(&mutex);
    if ( reducerOk && workset_in > 0 ) {
	pauseReducer = true;
	pthread_cond_signal(&workC);
	pthread_cond_wait(&pausedC, &mutex);
	pauseReducer = false;
    }

    // 'copyProblem' can only return 'false' if solver is used incrementally
    ok = reducerOk && copyProblem(reducer, offset);
    pthread_mutex_unlock(&mutex);

    if ( !ok ) return l_False;

    // Remove satisfied may have been disabled in a previous iteration, 
    // as we're done using 'offset' it can now be enabled again.
    remove_satisfied = true;

    // Run the solver in this thread normally,
    // it will supply clauses to 'work' for the other thread
    lbool sat = Solver::solve_();

    // If the user calls 'simplify' then problem clauses may not be
    // removed because we use 'offset' to keep track of new additions
    remove_satisfied = false;
    offset = nClauses();

    return sat;
}

// Give clause 'c' to the reducer, 'metric' represents some quality measure of the clause for sorting work set
bool SolRed::submitToReducer(const vec<Lit>& c, int metric) {
    vec<Lit>* tmp = new vec<Lit>();
    c.copyTo(*tmp);
    workset_in++;
    workset_in_lits+= tmp->size();

    /////////////////////////
    //// MUTEX PROTECTED ////
    pthread_mutex_lock(&mutex);
    tmp = work.insert(tmp, metric);

    newReduced.capacity(newReduced.size() + reduced.size());
    for( int i = 0; i < reduced.size(); i++ )
	newReduced.push_(reduced[i]);
    reduced.clear();

    if ( !reducerOk )
	ok = false; 
    else 
	pthread_cond_signal(&workC);
    pthread_mutex_unlock(&mutex);
    //// MUTEX PROTECTED ////
    /////////////////////////

    if ( tmp ) {
        workset_deleted++;
        workset_deleted_lits+=tmp->size();
        delete tmp;
    }

    return ok;
}

// Second POSIX thread main loop
void SolRed::threadGo() {
    vec<Lit>* lits = NULL;

    // Reducer thread main loop
    while(1) {
        /////////////////////////
        //// MUTEX PROTECTED ////
        pthread_mutex_lock(&mutex);

	// Add result of previous iteration to set 'reduced'
	if ( lits ) {
	    reduced.push(lits);
	    lits = NULL;
	}
	
	// Reducer derived unsat, stop everything
	if ( !reducer.okay() )
	    reducerOk = false;

	// Wait for work
	while( reducerOk && (pauseReducer || !work.available()) ) {
	    if ( pauseReducer ) pthread_cond_signal(&pausedC);
	    pthread_cond_wait(&workC, &mutex);
	}

	// Get work
	if ( reducerOk ) 
	    lits = work.get();
	// If requested to signal 'paused' then this must still be done now that we're about to exit
	else if ( pauseReducer ) 
	    pthread_cond_signal(&pausedC);

	assert( (lits != NULL) == reducerOk );
        pthread_mutex_unlock(&mutex);
        //// MUTEX PROTECTED ////
        /////////////////////////

	if ( !lits ) return; // Stop because reducer either derived UNSAT, or is stopped by class destructor

	// Reduce
	const int sz = lits->size();
	reducer_in++;
	reducer_in_lits+= sz;

	if ( reducer.reduce(*lits) ) {
	    reducer_out++;
	    reducer_out_lits+= lits->size();
	}
	else {
	    reducer_notout_lits+= sz;
	    delete lits;
	    lits = NULL;
	}
    }
}
