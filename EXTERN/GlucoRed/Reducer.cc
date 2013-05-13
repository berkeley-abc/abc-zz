//
//     MiniRed/GlucoRed
//
//     Siert Wieringa 
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#include"Reducer.hh"
#include"Version.hh"

#ifdef MINIRED
using namespace MiniRed;
#elif defined GLUCORED
using namespace GlucoRed;
#endif

// Reduce the clause formed by the literals 'lits', 
// returns 'true' if 'lits' is strengthened
bool Reducer::reduce(vec<Lit>& lits) {
    assert(ok);
    conflict.clear();
    
    const int sz = lits.size();

    assumptions.clear();
    assumptions.capacity(sz);
    for( int i = 0; i < lits.size(); i++ )
	assumptions.push_(~(lits[i]));
    
    // Calling 'search' does NOT run a SAT solving algorithm, as this solver 
    // instance has no decision variables. Hence, the result l_True means 
    // that all assumptions could be assigned without reaching a conflict 
    // by unit propagation. The problem is nevertheless guaranteed to be
    // unsatisfiable under these assumptions.
    lbool sat = search(-1);
    if (sat == l_True) {
	// Should be exactly equal because dummy levels are introduced
	assert(decisionLevel() == lits.size());

        // Remove literals corresponding to dummy decision levels	
	int i, j;
	for( i = j = 0; i < lits.size(); i++ ) {
	    if ( trail_lim[i] < trail.size() &&
		 trail[trail_lim[i]] == ~(lits[i]) ) lits[j++] = lits[i];
	}
	lits.shrink(i-j);

	cancelUntil(0);
	addClauseOnFly(lits);
    }
    else if (sat == l_False) {
	conflict.copyTo(lits);
	cancelUntil(0);
    }

    if ( !ok || !lits.size() ) 
	return ok = false;
    else
	return lits.size() < sz;
}


// Remove false literals from clause, returns 'true' iff clause is satisfied
// Note: regardless of current decision level only considers root level assignments!
bool Reducer::shrink(vec<Lit>& lits) {
    int i, j;
    for( i = j = 0; i < lits.size(); i++ ) {
	const Lit l = lits[i];
	if ( value(l) == l_Undef || level(var(l)) > 0 ) {
	    lits[j++] = lits[i];
	}
	else if ( value(l) == l_True ) {
	    lits.clear();
	    lits.push(l);
	    return true;
	}
    }
    lits.shrink(i-j);
    return false;
}
 
