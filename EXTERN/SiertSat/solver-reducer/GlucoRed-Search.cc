//
//     MiniRed/GlucoRed
//
//     Siert Wieringa
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
#ifdef GLUCORED
#include"SolRed.h"
#include"utils/Options.h"

using namespace GlucoRed;

static IntOption opt_rsort (VERSION_STRING, "rsort", "Sort reducer inputs (0=off, 1=by size, 2=by LBD)", 2, IntRange(0, 2));

// This is copied from Glucose's Solver::search, with some modifications to handle the reducer
lbool SolRed::search(int nof_conflicts)
{
    // SW121217
    if ( !ok ) return l_False;

    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    unsigned int nblevels;
    bool blocked=false;
    starts++;
    for (;;){
        CRef confl = propagate();
        // SW121217
        while (confl == CRef_Undef && nhead < newReduced.size() ) {
            const int d = decisionLevel();
            confl = addClauseOnFly(*(newReduced[nhead]));

            if ( decisionLevel() < d ) {
                reducer_backtracks++;
                reducer_backtrack_level_before+= d;
                reducer_backtrack_levels+= d - decisionLevel();
                if ( decisionLevel() == 0 ) reducer_backtracks_tozero++;
            }

            if ( !ok ) return l_False;

            delete newReduced[nhead++];
        }

        if (confl != CRef_Undef){
            // CONFLICT
            conflicts++; conflictC++;

            if (verbosity >= 1 && conflicts%verbEveryConflicts==0){
                printf("c | %8d   %7d    %5d | %7d %8d %8d | %5d %8d   %6d %8d | %6.3f %% |\n",
                       (int)starts,(int)nbstopsrestarts, (int)(conflicts/starts),
                       (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals,
                       (int)nbReduceDB, nLearnts(), (int)nbDL2,(int)nbRemovedClauses, progressEstimate()*100);
            }
            if (decisionLevel() == 0) {
                return l_False;

            }

            trailQueue.push(trail.size());
            if( conflicts>LOWER_BOUND_FOR_BLOCKING_RESTART && lbdQueue.isvalid()  && trail.size()>R*trailQueue.getavg()) {
                lbdQueue.fastclear();
                nbstopsrestarts++;
                if(!blocked) {lastblockatrestart=starts;nbstopsrestartssame++;blocked=true;}
            }

            learnt_clause.clear();
            analyze(confl, learnt_clause, backtrack_level,nblevels);

            lbdQueue.push(nblevels);
            sumLBD += nblevels;


            cancelUntil(backtrack_level);

            int metric;
            if (learnt_clause.size() == 1){
                uncheckedEnqueue(learnt_clause[0]);nbUn++;
                metric = 0;
            }else{
                CRef cr = ca.alloc(learnt_clause, true);
                ca[cr].setLBD(nblevels);
                if(nblevels<=2) nbDL2++; // stats
                if(ca[cr].size()==2) nbBin++; // stats
                learnts.push(cr);
                attachClause(cr);

                claBumpActivity(ca[cr]);
                uncheckedEnqueue(learnt_clause[0], cr);

                // SW121217
                metric = opt_rsort ? ((opt_rsort == 1) ? ca[cr].size(): ca[cr].lbd()) : 0;
            }
            if ( !submitToReducer(learnt_clause, metric) ) return l_False;

            varDecayActivity();
            claDecayActivity();
        }else{
            // SW121217
            assert(nhead == newReduced.size());
            newReduced.clear();
            nhead = 0;

            // Our dynamic restart, see the SAT09 competition compagnion paper
            if (( lbdQueue.isvalid() && ((lbdQueue.getavg()*K) > (sumLBD / conflicts))) || !withinBudget()) {
                lbdQueue.fastclear();
                progress_estimate = progressEstimate();
                cancelUntil(0);
                return l_Undef; }


            // Simplify the set of problem clauses:
            if (decisionLevel() == 0 && !simplify()) {
                // printf("c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
                return l_False;
            }
            // Perform clause database reduction !
            if(conflicts>=curRestart* nbclausesbeforereduce) {
                assert(learnts.size()>0);
                curRestart = (conflicts/ nbclausesbeforereduce)+1;
                reduceDB();
                nbclausesbeforereduce += incReduceDB;
            }

            Lit next = lit_Undef;
            while (decisionLevel() < assumptions.size()){
                // Perform user provided assumption:
                Lit p = assumptions[decisionLevel()];
                if (value(p) == l_True){
                    // Dummy decision level:
                    newDecisionLevel();
                }else if (value(p) == l_False){
                    analyzeFinal(~p, conflict);
                    return l_False;
                }else{
                    next = p;
                    break;
                }
            }

            if (next == lit_Undef){
                // New variable decision:
                decisions++;
                next = pickBranchLit();

                if (next == lit_Undef){
                    // printf("c last restart ## conflicts  :  %d %d \n",conflictC,decisionLevel());
                    // Model found:
                    return l_True;
                }
            }

            // Increase decision level and enqueue 'next'
            newDecisionLevel();
            uncheckedEnqueue(next);
        }
    }
}
#endif
