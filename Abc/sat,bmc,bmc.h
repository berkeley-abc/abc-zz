/**CFile****************************************************************

  FileName    [bmc.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [SAT-based bounded model checking.]

  Synopsis    [External declarations.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: bmc.h,v 1.00 2005/06/20 00:00:00 alanmi Exp $]

***********************************************************************/
 
#ifndef ABC___sat_bmc_BMC_h
#define ABC___sat_bmc_BMC_h


////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "aig,saig,saig.h"
#include "aig,gia,gia.h"

////////////////////////////////////////////////////////////////////////
///                         PARAMETERS                               ///
////////////////////////////////////////////////////////////////////////

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

// unrolling manager 
typedef struct Unr_Man_t_ Unr_Man_t;

typedef struct Saig_ParBmc_t_ Saig_ParBmc_t;
struct Saig_ParBmc_t_
{
    int         nStart;         // starting timeframe
    int         nFramesMax;     // maximum number of timeframes 
    int         nConfLimit;     // maximum number of conflicts at a node
    int         nConfLimitJump; // maximum number of conflicts after jumping
    int         nFramesJump;    // the number of tiemframes to jump
    int         nTimeOut;       // approximate timeout in seconds
    int         nTimeOutGap;    // approximate timeout in seconds since the last change
    int         nTimeOutOne;    // timeout per output in multi-output solving
    int         nPisAbstract;   // the number of PIs to abstract
    int         fSolveAll;      // does not stop at the first SAT output
    int         fStoreCex;      // enable storing CEXes in the MO mode
    int         fUseBridge;     // use bridge interface
    int         fDropSatOuts;   // replace sat outputs by constant 0
    int         nFfToAddMax;    // max number of flops to add during CBA
    int         fSkipRand;      // skip random decisions
    int         nLearnedStart;  // starting learned clause limit
    int         nLearnedDelta;  // delta of learned clause limit
    int         nLearnedPerce;  // ratio of learned clause limit
    int         fVerbose;       // verbose 
    int         fNotVerbose;    // skip line-by-line print-out 
    int         fSilent;        // completely silent
    int         iFrame;         // explored up to this frame
    int         nFailOuts;      // the number of failed outputs
    int         nDropOuts;      // the number of dropped outputs
    abctime     timeLastSolved; // the time when the last output was solved
    int(*pFuncOnFail)(int,Abc_Cex_t*); // called for a failed output in MO mode
};

 
typedef struct Bmc_AndPar_t_ Bmc_AndPar_t;
struct Bmc_AndPar_t_
{
    int         nStart;         // starting timeframe
    int         nFramesMax;     // maximum number of timeframes 
    int         nFramesAdd;     // the number of additional frames
    int         nConfLimit;     // maximum number of conflicts at a node
    int         nTimeOut;       // timeout in seconds
    int         fLoadCnf;       // dynamic CNF loading
    int         fDumpFrames;    // dump unrolled timeframes
    int         fUseSynth;      // use synthesis
    int         fUseOldCnf;     // use old CNF construction
    int         fVerbose;       // verbose 
    int         fVeryVerbose;   // very verbose 
    int         fNotVerbose;    // skip line-by-line print-out 
    int         iFrame;         // explored up to this frame
    int         nFailOuts;      // the number of failed outputs
    int         nDropOuts;      // the number of dropped outputs
};
 
typedef struct Bmc_MulPar_t_ Bmc_MulPar_t;
struct Bmc_MulPar_t_
{
    int         TimeOutGlo;
    int         TimeOutLoc;
    int         TimeOutInc;
    int         TimeOutGap;
    int         TimePerOut;
    int         fUseSyn;
    int         fDumpFinal;
    int         fVerbose;
    int         fVeryVerbose;
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                         ///
////////////////////////////////////////////////////////////////////////

/*=== bmcBmc.c ==========================================================*/
extern int               Saig_ManBmcSimple( Aig_Man_t * pAig, int nFrames, int nSizeMax, int nBTLimit, int fRewrite, int fVerbose, int * piFrame, int nCofFanLit );
/*=== bmcBmc2.c ==========================================================*/
extern int               Saig_BmcPerform( Aig_Man_t * pAig, int nStart, int nFramesMax, int nNodesMax, int nTimeOut, int nConfMaxOne, int nConfMaxAll, int fVerbose, int fVerbOverwrite, int * piFrames, int fSilent );
/*=== bmcBmc3.c ==========================================================*/
extern void              Saig_ParBmcSetDefaultParams( Saig_ParBmc_t * p );
extern int               Saig_ManBmcScalable( Aig_Man_t * pAig, Saig_ParBmc_t * pPars );
/*=== bmcBmcAnd.c ==========================================================*/
extern int               Gia_ManBmcPerform( Gia_Man_t * p, Bmc_AndPar_t * pPars );
/*=== bmcCexCut.c ==========================================================*/
extern Gia_Man_t *       Bmc_GiaTargetStates( Gia_Man_t * p, Abc_Cex_t * pCex, int iFrBeg, int iFrEnd, int fCombOnly, int fGenAll, int fAllFrames, int fVerbose );
extern Aig_Man_t *       Bmc_AigTargetStates( Aig_Man_t * p, Abc_Cex_t * pCex, int iFrBeg, int iFrEnd, int fCombOnly, int fGenAll, int fAllFrames, int fVerbose );
/*=== bmcCexMin.c ==========================================================*/
extern Abc_Cex_t *       Saig_ManCexMinPerform( Aig_Man_t * pAig, Abc_Cex_t * pCex );
/*=== bmcICheck.c ==========================================================*/
extern void              Bmc_PerformICheck( Gia_Man_t * p, int nFramesMax, int nTimeOut, int fEmpty, int fVerbose );
extern void              Bmc_PerformISearch( Gia_Man_t * p, int nFramesMax, int nTimeOut, int fReverse, int fDump, int fVerbose );
/*=== bmcUnroll.c ==========================================================*/
extern Unr_Man_t *       Unr_ManUnrollStart( Gia_Man_t * pGia, int fVerbose );
extern Gia_Man_t *       Unr_ManUnrollFrame( Unr_Man_t * p, int f );
extern void              Unr_ManFree( Unr_Man_t * p );


ABC_NAMESPACE_HEADER_END



#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////

