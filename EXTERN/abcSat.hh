//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : abcSat.hh
//| Author(s)   : Niklas Een
//| Module      : ABC SAT
//| Description : ABC's version of MiniSat pulled out as a separate library.
//| 
//| (C) Copyright 2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__EXTERN__abcSat_hh
#define ZZ__EXTERN__abcSat_hh


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


$MAKEFILE_DEFINES

namespace abc_sat {

struct sat_solver_t;
typedef struct sat_solver_t sat_solver;
typedef int lit;

sat_solver* sat_solver_new           ();
void        sat_solver_delete        (sat_solver* s);
void        sat_solver_rollback      (sat_solver* s);   // -- clear method

void        sat_solver_setnvars      (sat_solver* s, int n);
int         sat_solver_addclause     (sat_solver* s, lit* begin, lit* end);
int         sat_solver_solve         (sat_solver* s, lit* begin, lit* end,
                                      int64 nConfLimit, int64 nInsLimit, int64 nConfLimitGlobal, int64 nInsLimitGlobal);

int         sat_solver_var_value     (sat_solver* s, int v);
int         sat_solver_final         (sat_solver* s, lit** ppArray);
int         sat_solver_count_assigned(sat_solver* s);
int         sat_solver_nvars         (sat_solver* s);
int         sat_solver_nclauses      (sat_solver* s);
int         sat_solver_nlearnts      (sat_solver* s);
int         sat_solver_nconflicts    (sat_solver* s);
int         sat_solver_get_activity  (sat_solver* s, int v);

int         sat_solver_simplify      (sat_solver* s);
double      sat_solver_memory        (sat_solver* s);

void        sat_solver_set_verbosity (sat_solver* s, int level);

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
#endif
