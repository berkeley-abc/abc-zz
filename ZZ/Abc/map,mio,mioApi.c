/**CFile****************************************************************

  FileName    [mioApi.c]

  PackageName [MVSIS 1.3: Multi-valued logic synthesis system.]

  Synopsis    [File reading/writing for technology mapping.]

  Author      [MVSIS Group]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - September 8, 2003.]

  Revision    [$Id: mioApi.c,v 1.4 2004/06/28 14:20:25 alanmi Exp $]

***********************************************************************/

#include "mioInt.h"

ABC_NAMESPACE_IMPL_START


////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char *            Mio_LibraryReadName          ( Mio_Library_t * pLib )  { return pLib->pName;      }
int               Mio_LibraryReadGateNum       ( Mio_Library_t * pLib )  { return pLib->nGates;     }
Mio_Gate_t *      Mio_LibraryReadGates         ( Mio_Library_t * pLib )  { return pLib->pGates;     }
Mio_Gate_t **     Mio_LibraryReadGateArray     ( Mio_Library_t * pLib )  { return pLib->ppGatesName;}
Mio_Gate_t *      Mio_LibraryReadBuf           ( Mio_Library_t * pLib )  { return pLib->pGateBuf;   }
Mio_Gate_t *      Mio_LibraryReadInv           ( Mio_Library_t * pLib )  { return pLib->pGateInv;   }
Mio_Gate_t *      Mio_LibraryReadConst0        ( Mio_Library_t * pLib )  { return pLib->pGate0;     }
Mio_Gate_t *      Mio_LibraryReadConst1        ( Mio_Library_t * pLib )  { return pLib->pGate1;     }
Mio_Gate_t *      Mio_LibraryReadNand2         ( Mio_Library_t * pLib )  { return pLib->pGateNand2; }
Mio_Gate_t *      Mio_LibraryReadAnd2          ( Mio_Library_t * pLib )  { return pLib->pGateAnd2;  }
float             Mio_LibraryReadDelayInvRise  ( Mio_Library_t * pLib )  { return (float)(pLib->pGateInv?   pLib->pGateInv->pPins->dDelayBlockRise   : 0.0); }
float             Mio_LibraryReadDelayInvFall  ( Mio_Library_t * pLib )  { return (float)(pLib->pGateInv?   pLib->pGateInv->pPins->dDelayBlockFall   : 0.0); }
float             Mio_LibraryReadDelayInvMax   ( Mio_Library_t * pLib )  { return (float)(pLib->pGateInv?   pLib->pGateInv->pPins->dDelayBlockMax    : 0.0); }
float             Mio_LibraryReadDelayNand2Rise( Mio_Library_t * pLib )  { return (float)(pLib->pGateNand2? pLib->pGateNand2->pPins->dDelayBlockRise : 0.0); }
float             Mio_LibraryReadDelayNand2Fall( Mio_Library_t * pLib )  { return (float)(pLib->pGateNand2? pLib->pGateNand2->pPins->dDelayBlockFall : 0.0); }
float             Mio_LibraryReadDelayNand2Max ( Mio_Library_t * pLib )  { return (float)(pLib->pGateNand2? pLib->pGateNand2->pPins->dDelayBlockMax  : 0.0); }
float             Mio_LibraryReadDelayAnd2Max  ( Mio_Library_t * pLib )  { return (float)(pLib->pGateAnd2?  pLib->pGateAnd2->pPins->dDelayBlockMax   : 0.0); }
float             Mio_LibraryReadAreaInv       ( Mio_Library_t * pLib )  { return (float)(pLib->pGateInv?   pLib->pGateInv->dArea   : 0.0); }
float             Mio_LibraryReadAreaBuf       ( Mio_Library_t * pLib )  { return (float)(pLib->pGateBuf?   pLib->pGateBuf->dArea   : 0.0); }
float             Mio_LibraryReadAreaNand2     ( Mio_Library_t * pLib )  { return (float)(pLib->pGateNand2? pLib->pGateNand2->dArea : 0.0); }

/**Function*************************************************************

  Synopsis    [Returns the longest gate name.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Mio_LibraryReadGateNameMax( Mio_Library_t * pLib )
{
    Mio_Gate_t * pGate;
    int LenMax = 0, LenCur;
    Mio_LibraryForEachGate( pLib, pGate )
    {
        LenCur = strlen( Mio_GateReadName(pGate) );
        if ( LenMax < LenCur )
            LenMax = LenCur;
    }
    return LenMax;
}

/**Function*************************************************************

  Synopsis    [Read Mvc of the gate by name.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Mio_Gate_t * Mio_LibraryReadGateByName( Mio_Library_t * pLib, char * pName, char * pOutName )      
{ 
    Mio_Gate_t * pGate;
    if ( ! st__lookup( pLib->tName2Gate, pName, (char **)&pGate ) )
        return NULL;
    if ( pOutName == NULL )
        return pGate;
    if ( !strcmp(pGate->pOutName, pOutName) )
        return pGate;
    if ( pGate->pTwin && !strcmp(pGate->pTwin->pOutName, pOutName) )
        return pGate->pTwin;
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Read Mvc of the gate by name.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Mio_LibraryReadSopByName( Mio_Library_t * pLib, char * pName )      
{ 
    Mio_Gate_t * pGate;
    if ( st__lookup( pLib->tName2Gate, pName, (char **)&pGate ) )
        return pGate->pSop;
    return NULL;
}

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char *            Mio_GateReadName    ( Mio_Gate_t * pGate )            { return pGate->pName;     }
char *            Mio_GateReadOutName ( Mio_Gate_t * pGate )            { return pGate->pOutName;  }
double            Mio_GateReadArea    ( Mio_Gate_t * pGate )            { return pGate->dArea;     }
char *            Mio_GateReadForm    ( Mio_Gate_t * pGate )            { return pGate->pForm;     }
Mio_Pin_t *       Mio_GateReadPins    ( Mio_Gate_t * pGate )            { return pGate->pPins;     }
Mio_Library_t *   Mio_GateReadLib     ( Mio_Gate_t * pGate )            { return pGate->pLib;      }
Mio_Gate_t *      Mio_GateReadNext    ( Mio_Gate_t * pGate )            { return pGate->pNext;     }
Mio_Gate_t *      Mio_GateReadTwin    ( Mio_Gate_t * pGate )            { return pGate->pTwin;     }
int               Mio_GateReadPinNum  ( Mio_Gate_t * pGate )            { return pGate->nInputs;   }
double            Mio_GateReadDelayMax( Mio_Gate_t * pGate )            { return pGate->dDelayMax; }
char *            Mio_GateReadSop     ( Mio_Gate_t * pGate )            { return pGate->pSop;      }
word              Mio_GateReadTruth   ( Mio_Gate_t * pGate )            { return pGate->nInputs <= 6 ? pGate->uTruth : 0;   }
word *            Mio_GateReadTruthP  ( Mio_Gate_t * pGate )            { return pGate->nInputs <= 6 ? NULL: pGate->pTruth; }
int               Mio_GateReadValue   ( Mio_Gate_t * pGate )            { return pGate->Value;     }
void              Mio_GateSetValue    ( Mio_Gate_t * pGate, int Value ) { pGate->Value = Value;    }

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char *            Mio_PinReadName           ( Mio_Pin_t * pPin )      { return pPin->pName;           }
Mio_PinPhase_t    Mio_PinReadPhase          ( Mio_Pin_t * pPin )      { return pPin->Phase;           }
double            Mio_PinReadInputLoad      ( Mio_Pin_t * pPin )      { return pPin->dLoadInput;      }
double            Mio_PinReadMaxLoad        ( Mio_Pin_t * pPin )      { return pPin->dLoadMax;        }
double            Mio_PinReadDelayBlockRise ( Mio_Pin_t * pPin )      { return pPin->dDelayBlockRise; }
double            Mio_PinReadDelayFanoutRise( Mio_Pin_t * pPin )      { return pPin->dDelayFanoutRise;}
double            Mio_PinReadDelayBlockFall ( Mio_Pin_t * pPin )      { return pPin->dDelayBlockFall; }
double            Mio_PinReadDelayFanoutFall( Mio_Pin_t * pPin )      { return pPin->dDelayFanoutFall;}
double            Mio_PinReadDelayBlockMax  ( Mio_Pin_t * pPin )      { return pPin->dDelayBlockMax;          }
Mio_Pin_t *       Mio_PinReadNext           ( Mio_Pin_t * pPin )      { return pPin->pNext;           }

/**Function*************************************************************

  Synopsis    []

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
char * Mio_GateReadPinName( Mio_Gate_t * pGate, int iPin )
{
    Mio_Pin_t * pPin;
    int i = 0;
    Mio_GateForEachPin( pGate, pPin )
        if ( i++ == iPin )
            return Mio_PinReadName(pPin);
    return NULL;
}
float Mio_GateReadPinDelay( Mio_Gate_t * pGate, int iPin )
{
    Mio_Pin_t * pPin;
    int i = 0;
    Mio_GateForEachPin( pGate, pPin )
        if ( i++ == iPin )
            return 0.5 * pPin->dDelayBlockRise + 0.5 * pPin->dDelayBlockFall;
    return ABC_INFINITY;
}

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

