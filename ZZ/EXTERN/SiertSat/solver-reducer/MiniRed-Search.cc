//
//     MiniRed/GlucoRed
//
//     Siert Wieringa
//     siert.wieringa@aalto.fi
// (c) Aalto University 2012/2013
//
#ifdef MINIRED
#include"SolRed.h"
#include"utils/Options.h"

using namespace MiniRed;
static BoolOption opt_rsort (VERSION_STRING, "rsort", "Sort reducer inputs (by size)", true);

// This is copied from MiniSAT's Solver::search, with some modifications to handle the reducer
lbool SolRed::search(int nof_conflicts)
{
    // SW121211
    if ( !ok ) return l_False;

    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    starts++;

    for (;;){
        CRef confl = propagate();

        // SW121211
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
            if (decisionLevel() == 0) return l_False;

            learnt_clause.clear();
            analyze(confl, learnt_clause, backtrack_level);
            cancelUntil(backtrack_level);

            int metric;
            if (learnt_clause.size() == 1){
                uncheckedEnqueue(learnt_clause[0]);
                metric = 0;
            }else{
                CRef cr = ca.alloc(learnt_clause, true);
                learnts.push(cr);
                attachClause(cr);
                claBumpActivity(ca[cr]);
                uncheckedEnqueue(learnt_clause[0], cr);

                // SW130104
                metric = opt_rsort ? learnt_clause.size(): 0;
            }
            if ( !submitToReducer(learnt_clause, metric) ) return l_False;

            varDecayActivity();
            claDecayActivity();

            if (--learntsize_adjust_cnt == 0){
                learntsize_adjust_confl *= learntsize_adjust_inc;
                learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
                max_learnts             *= learntsize_inc;

                if (verbosity >= 1)
                    printf("| %9d | %7d %8d %8d | %8d %8d %6.0f | %6.3f %% |\n",
                           (int)conflicts,
                           (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals,
                           (int)max_learnts, nLearnts(), (double)learnts_literals/nLearnts(), progressEstimate()*100);
            }

        }else{
            // SW121212
            assert(nhead == newReduced.size());
            newReduced.clear();
            nhead = 0;

            // NO CONFLICT
            if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()){
                // Reached bound on number of conflicts:
                progress_estimate = progressEstimate();
                cancelUntil(0);
                return l_Undef; }

            // Simplify the set of problem clauses:
            if (decisionLevel() == 0 && !simplify())
                return l_False;

            if (learnts.size()-nAssigns() >= max_learnts)
                // Reduce the set of learnt clauses:
                reduceDB();

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

                if (next == lit_Undef)
                    // Model found:
                    return l_True;
            }

            // Increase decision level and enqueue 'next'
            newDecisionLevel();
            uncheckedEnqueue(next);
        }
    }
}
#endif
