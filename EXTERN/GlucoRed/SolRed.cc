//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#include"SolRed.hh"

#ifdef MINIRED
using namespace MiniRed;
#elif defined GLUCORED
using namespace GlucoRed;
#endif

static IntOption opt_solver_ccmin_mode (VERSION_STRING, "solver-ccmin",  "Conflict clause minimization mode for solver (original -ccmin-mode applies to reducer)", 2, IntRange(0, 2));

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

    , result                         (l_Undef)
    , stop                           (false)
    , nhead                          (0)
    , offset                         (0)
    , reducer                        () // Instance of Reducer which forms the reducer
{
    ccmin_mode = opt_solver_ccmin_mode;

    // Create posix thread objects
    pthread_mutex_init(&mutex, NULL);
    pthread_spin_init(&resultLock, PTHREAD_PROCESS_PRIVATE);
    pthread_cond_init(&cond, NULL);
}

SolRed::~SolRed() {
    pthread_mutex_destroy(&mutex);
    pthread_spin_destroy(&resultLock);
    pthread_cond_destroy(&cond);
    
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

    stop   = false;
    result = l_Undef;
   
    // 'copyProblem' can only return 'false' if solver is used incrementally
    if ( !copyProblem(reducer, offset) ) return l_False;
    pthread_create(&posix, NULL, threadEntry, this);

    // Run the solver in this thread normally, 
    // it will supply clauses to 'work' for the other thread
    lbool sat = Solver::solve_();
    foundResult(sat, true);
    
    // Wait for the other thread to exit
    pthread_join(posix, NULL);

    if ( sat == l_Undef && result != l_Undef  ) {
	assert(result!=l_Undef);

	if ( result == l_True )
	    reducer.model.copyTo(model);
	else
	    reducer.conflict.copyTo(conflict);
    }

    if ( result == l_False && !conflict.size() ) ok = false;

    offset = nClauses();
    // Removing satisfied clauses problem clauses in the future is not safe,
    // because we're using 'offset' to determine which clauses are new in subsequent solver call
    remove_satisfied = false;
    clearInterrupt();

    return result;
}

// Give clause 'c' to the reducer, 'metric' represents some quality measure of the clause for sorting work set
bool SolRed::submitToReducer(const vec<Lit>& c, int metric) {
    if ( stop || !ok ) return ok;

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
   
    pthread_cond_signal(&cond);
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
    while(!stop && reducer.okay()) {
	/////////////////////////
	//// MUTEX PROTECTED ////
	pthread_mutex_lock(&mutex);
	// Add result of previous iteration to set 'reduced'
	if ( lits ) reduced.push(lits);	

	// Wait for work
	while( !stop && !work.available() )
	    pthread_cond_wait(&cond, &mutex);

	// Get work if available
	if (stop || !work.available()) {
	    lits = NULL;
	    reducer.clearInterrupt();
	}
	else lits = work.get();	
       
	pthread_mutex_unlock(&mutex);
	//// MUTEX PROTECTED ////
	/////////////////////////
	
	if ( lits ) {
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

    if ( !reducer.okay() )
	foundResult(l_False, false);
}

// Report result 'sat', interrupts further solving/reducing
void SolRed::foundResult(lbool sat, bool signal) {
    lbool old;

    stop = true;
    interrupt();
    reducer.interrupt();

    pthread_spin_lock(&resultLock);
    old = result;
    if (result == l_Undef) result = sat; 
    pthread_spin_unlock(&resultLock);

    if ( signal && old == l_Undef ) {
	/////////////////////////
	//// MUTEX PROTECTED ////
	pthread_mutex_lock(&mutex);
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	//// MUTEX PROTECTED ////
	/////////////////////////
    }

    assert(old == l_Undef || sat == l_Undef || old == sat);
}
