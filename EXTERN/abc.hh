//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : abc.hh
//| Author(s)   : Niklas Een
//| Module      : ABC
//| Description : Master include file for the ABC library.
//| 
//| (C) Copyright 2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This header file should ideally expose all the relevant functionality of ABC. However, 
//| we choose to add things on demand, so it is far from complete.
//|________________________________________________________________________________________________

#ifndef ZZ__EXTERN__abc_hh
#define ZZ__EXTERN__abc_hh


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


$MAKEFILE_DEFINES


#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/gia/gia.h"
#include "proof/abs/abs.h"
#include "proof/abs/absRef.h"
#include "misc/util/abc_global.h"
#include "bdd/cudd/cudd.h"
#include "bdd/cudd/cuddInt.h"

extern "C" void Abc_CommandUpdate9(Abc_Frame_t* pAbc, Gia_Man_t* pNew);

// Get rid of warnings:
static void    Vec_IntUniqify( Vec_Int_t * p ) ___unused;
static void    Vec_WrdUniqify( Vec_Wrd_t * p ) ___unused;
static int     sat_solver_var_value( sat_solver* s, int v )  ___unused;
static int     sat_solver_var_literal( sat_solver* s, int v ) ___unused;
static void    sat_solver_act_var_clear(sat_solver* s) ___unused;
static void    sat_solver_compress(sat_solver* s) ___unused;
static int     sat_solver_final(sat_solver* s, int ** ppArray) ___unused;
static int     sat_solver_set_random(sat_solver* s, int fNotUseRandom) ___unused;
static clock_t sat_solver_set_runtime_limit(sat_solver*, clock_t) ___unused;
//static int     sat_solver_set_runtime_limit(sat_solver*, int) ___unused;

// Undefine some macros:
#ifdef b0
#undef b0
#endif

#ifdef b1
#undef b1
#endif

#ifdef z0
#undef z0
#endif

#ifdef z1
#undef z1
#endif

#ifdef a0
#undef a0
#endif

#ifdef a1
#undef a1
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
#endif
