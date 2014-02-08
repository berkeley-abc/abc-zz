//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : MODULE.hh (for AbcSat)
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


namespace abc_sat {

typedef long long int64;        // -- hack

struct sat_solver_t;
typedef struct sat_solver_t sat_solver;
typedef int lit;


typedef int    lit;
typedef int    cla;

typedef char               lbool;

static const int   var_Undef = -1;
static const lit   lit_Undef = -2;

static const lbool l_Undef   =  0;
static const lbool l_True    =  1;
static const lbool l_False   = -1;

static inline lit  toLit    (int v)        { return v + v; }
static inline lit  toLitCond(int v, int c) { return v + v + (c != 0); }
static inline lit  lit_neg  (lit l)        { return l ^ 1; }
static inline int  lit_var  (lit l)        { return l >> 1; }
static inline int  lit_sign (lit l)        { return l & 1; }
static inline int  lit_print(lit l)        { return lit_sign(l)? -lit_var(l)-1 : lit_var(l)+1; }
static inline lit  lit_read (int s)        { return s > 0 ? toLit(s-1) : lit_neg(toLit(-s-1)); }
static inline int  lit_check(lit l, int n) { return l >= 0 && lit_var(l) < n;                  }

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
