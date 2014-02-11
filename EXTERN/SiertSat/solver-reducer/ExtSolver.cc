//
//     MiniRed/GlucoRed
//
//     Siert Wieringa
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
//
#include"ExtSolver.h"

#ifdef MINIRED
using namespace MiniRed;
#elif defined GLUCORED
using namespace GlucoRed;
#endif

// Copy this solver's problem into 'other'
// The variables created in 'other' are NOT decision variables
bool ExtSolver::copyProblem(ExtSolver& other, int offset) {
    assert(ok);
    assert(qhead == trail.size());
    assert(decisionLevel() == 0);
    assert(&other != this);

    // Can only happen in incremental SAT usage
    if ( !other.okay() ) return ok = false;

    // Give 'other' the same number of variables as this
    while( nVars() > other.nVars() )
        other.newVar(true, false); // NOT DECISION VARIABLES!

    // First problem: Give empty solver 'other' the same set of assignments (unit clauses) as this
    if ( solves == 0 ) {
        assert(other.nClauses() == 0);
        assert(other.trail.size() == 0);

        for( int i = 0; i < trail.size(); i++ )
            other.uncheckedEnqueue(trail[i]);
        other.qhead = qhead;
    }
    // For incremental SAT synchronize units and propagate
    else {
        // Copy 'other's unit clauses to here
        for( int i = 0; i < other.trail.size(); i++ ) {
            lbool v = value(other.trail[i]);
            if ( v == l_Undef )
                uncheckedEnqueue(other.trail[i]);
            else if ( v == l_False )
                return ok = false;
        }

        // Propagate here, if leads to conflict state is inconsistent
        if ( propagate() != CRef_Undef ) return ok = false;

        // Copy our unit clauses to 'other'
        for( int i = 0; i < trail.size(); i++ ) {
            lbool v = other.value(trail[i]);
            if ( v == l_Undef )
                other.uncheckedEnqueue(trail[i]);
            else if ( v == l_False )
                return ok = false;
        }

        // Propagate on 'other', if leads to conflict that state is inconsistent
        if ( other.propagate() != CRef_Undef ) return ok = false;
    }

    // Copy clauses starting from 'offset' to 'other'
    other.clauses.capacity(other.clauses.size() + clauses.size() - offset);
    for( int i = offset; i < clauses.size(); i++ ) {
        Clause& c  = ca[clauses[i]];
        assert(c.size()>1);

        if ( value(c[0]) == l_Undef && value(c[1]) == l_Undef ) {
            CRef cr = other.ca.alloc(c, false);
            other.clauses.push_(cr);
            other.attachClause(cr);
        }
        else assert(value(c[0]) == l_True || value(c[1]) == l_True);
    }

#ifdef MINIRED
    other.max_learnts             = other.nClauses() * learntsize_factor;
    other.learntsize_adjust_confl = learntsize_adjust_start_confl;
    other.learntsize_adjust_cnt   = (int)other.learntsize_adjust_confl;
#elif defined GLUCORED
    other.nbclausesbeforereduce = firstReduceDB;
    other.lbdQueue.initSize(sizeLBDQueue);
    other.trailQueue.initSize(sizeTrailQueue);
    other.sumLBD = 0;
#endif

    return true;
}


// Do we prefer attaching a watch pointer to 'p' over 'q'?
bool ExtSolver::litComp(const Lit &p, const Lit &q) {
    assert(p!=q);

    if ( value(p) == l_True || value(q) == l_True )                                          // If either 'p' or 'q' is assigned 'true'
        return value(p) == l_True && (value(q) != l_True || level(var(q)) > level(var(p)));  // ... yes, if 'p' is assigned true at level 'd' and 'q' is not assigned 'true' at level 'd'
    else if ( value(p) == l_Undef || value(q) == l_Undef )                                   // Else, if either 'p' or 'q' is unassigned
        return value(p) == l_Undef && value(q) != l_Undef;                                   // ... yes, if 'p' is unassigned and 'q' is 'false'
    else                                                                                     // Else (both 'p' and 'q' are assigned 'false')
        return level(var(p)) > level(var(q));                                                // ... yes, if 'p' is assigned 'false' at a higher level than 'q'
}

// Insert a clause on the fly, will perform minimal backtracking required for consistent state
// Returns a reference to a clause that is empty under the current assignment if a conflict occurs.
CRef ExtSolver::addClauseOnFly(vec<Lit>& lits) {
    assert(ok);
    assert(qhead == trail.size());

#ifdef GLUCORED
    tmp_lbd.clear();
    tmp_lbd.growTo(decisionLevel(), false);
    unsigned lbd = 0;
#endif
    int w0 = -1;
    int w1 = -1;
    int i, j;
    int maxl = 0;
    for( i = j = 0; i < lits.size(); i++ ) {
        const lbool v = value(lits[i]);
        const int   l = level(var(lits[i]));

        // If literal is not assigned at decision level zero
        if (v == l_Undef || l > 0 ) {
#ifdef GLUCORED
            // Everytime we see a new decision level increment LBD counter,
            // each unassigned literal adds one to the LBD (in that case this is not a true LBD)
            if ( v == l_Undef )
                lbd++;
            else if ( !tmp_lbd[l-1] ) {
                tmp_lbd[l-1] = true;
                lbd++;
            }
#endif

            // Check if literal lits[i] is the most prefered for a watch pointer
            if ( w0 < 0 || litComp(lits[i], lits[w0]) ) {
                w1 = w0; // Previously most prefered becomes second most prefered
                w0 = j;
            }
            // Else, check if literal lits[i] is the second most prefered for a watch pointer
            else if ( w1 < 0 || litComp(lits[i], lits[w1]) ) w1 = j;

            lits[j++] = lits[i];
            if ( l > maxl ) maxl = l;
        }
        // Literal is assigned true at decision level zero, clause is satisfied, abort clause insertion
        else if (v == l_True && l == 0) return CRef_Undef;
    }
    // Shrink 'lits' to only include literals that are unassigned at decision level zero.
    lits.shrink(i-j);

    // Case 1: Clause is empty, return false
    if ( !lits.size() ) {
        cancelUntil(0);
        ok = false;
        return CRef_Undef;
    }
    // Case 2: Clause is a unit clause
    else if ( lits.size() == 1 ) {
        cancelUntil(0);
        assert(value(lits[0])==l_Undef);
        uncheckedEnqueue(lits[0]);
        return propagate();
    }
    assert( w0 >= 0 && w1 >= 0 && w0 != w1 );

    // Case 3: Clause has more than one literal after BCP
    const Lit lw0 = lits[w0];
    const Lit lw1 = lits[w1];

    // Backtrack if necessary
    //
    // SW130731 This has been fixed after Alexander Ivrii reported incorrect behavior for adding clauses with one satisfied literal
    if ( value(lw1) == l_False ) {
        const int ll0 = (value(lw0) == l_Undef) ? (decisionLevel()+1) : level(var(lw0));
        const int ll1 = level(var(lw1));

        assert(ll0>0 && ll1>0);
        if ( ll0 > ll1 )
            cancelUntil(ll1);
        else if ( value(lw0) == l_False && ll0 == ll1 )
            cancelUntil(ll1-1);	
    }
    else assert(value(lw0)!=l_False);

    // Swap literals to assign watch pointers
    lits[w0]       = lits[0];
    lits[0]        = lw0;
    lits[w1?w1:w0] = lits[1]; // If 'w1' equals 0, then lits[w1] has just been moved to lits[w0]
    lits[1]        = lw1;

    // Learn
    CRef cr = ca.alloc(lits, true);
    learnts.push(cr);
    attachClause(cr);
    claBumpActivity(ca[cr]);
#ifdef GLUCORED
    ca[cr].setLBD(lbd);
#endif

    // Enqueue literal if clause is unit under current assignment
    if ( value(lits[0]) == l_Undef && value(lits[1]) == l_False ) {
        uncheckedEnqueue(lits[0], cr);

        assert(ok);
        return propagate();
    }
    else return CRef_Undef;    
}

