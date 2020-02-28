/**CFile****************************************************************

  FileName    [mainInit.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [The main package.]

  Synopsis    [Initialization procedures.]

  Author      [Alan Mishchenko]
  
  Affiliation [UC Berkeley]

  Date        [Ver. 1.0. Started - June 20, 2005.]

  Revision    [$Id: mainInit.c,v 1.3 2005/09/14 22:53:37 casem Exp $]

***********************************************************************/

#include "base,abc,abc.h"
#include "mainInt.h"

ABC_NAMESPACE_IMPL_START

//#define USE_ABC2
//#define USE_ABC85
 
////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

extern void Abc_Init( Abc_Frame_t * pAbc );
extern void Abc_End ( Abc_Frame_t * pAbc );
extern void Io_Init( Abc_Frame_t * pAbc );
extern void Io_End ( Abc_Frame_t * pAbc );
extern void Cmd_Init( Abc_Frame_t * pAbc );
extern void Cmd_End ( Abc_Frame_t * pAbc );
extern void If_Init( Abc_Frame_t * pAbc );
extern void If_End ( Abc_Frame_t * pAbc );
extern void Map_Init( Abc_Frame_t * pAbc );
extern void Map_End ( Abc_Frame_t * pAbc );
extern void Mio_Init( Abc_Frame_t * pAbc );
extern void Mio_End ( Abc_Frame_t * pAbc );
extern void Super_Init( Abc_Frame_t * pAbc );
extern void Super_End ( Abc_Frame_t * pAbc );
extern void Libs_Init( Abc_Frame_t * pAbc );
extern void Libs_End( Abc_Frame_t * pAbc );
extern void Load_Init( Abc_Frame_t * pAbc );
extern void Load_End( Abc_Frame_t * pAbc );
extern void Scl_Init( Abc_Frame_t * pAbc );
extern void Scl_End( Abc_Frame_t * pAbc );
extern void Test_Init( Abc_Frame_t * pAbc );
extern void Test_End( Abc_Frame_t * pAbc );
extern void Abc2_Init( Abc_Frame_t * pAbc );
extern void Abc2_End ( Abc_Frame_t * pAbc );
extern void Abc85_Init( Abc_Frame_t * pAbc );
extern void Abc85_End( Abc_Frame_t * pAbc );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Starts all the packages.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameInit( Abc_Frame_t * pAbc )
{
    Cmd_Init( pAbc );
    Cmd_CommandExecute( pAbc, "set checkread" ); 
    Io_Init( pAbc );
    Abc_Init( pAbc );
    If_Init( pAbc );
    Map_Init( pAbc );
    Mio_Init( pAbc );
    Super_Init( pAbc );
    Libs_Init( pAbc );
    Load_Init( pAbc );
    Scl_Init( pAbc );
    Test_Init( pAbc );
#ifdef USE_ABC2
    Abc2_Init( pAbc );
#endif
#ifdef USE_ABC85
    Abc85_Init( pAbc );
#endif
    EXT_ABC_INIT(pAbc) // plugin for external functionality
}


/**Function*************************************************************

  Synopsis    [Stops all the packages.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Abc_FrameEnd( Abc_Frame_t * pAbc )
{
    Abc_End( pAbc );
    Io_End( pAbc );
    Cmd_End( pAbc );
    If_End( pAbc );
    Map_End( pAbc );
    Mio_End( pAbc );
    Super_End( pAbc );
    Libs_End( pAbc );
    Load_End( pAbc );
    Scl_End( pAbc );
    Test_End( pAbc );
#ifdef USE_ABC2
    Abc2_End( pAbc );
#endif
#ifdef USE_ABC85
    Abc85_End( pAbc );
#endif
    EXT_ABC_END(pAbc) // plugin for external functionality
}


////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////


ABC_NAMESPACE_IMPL_END

