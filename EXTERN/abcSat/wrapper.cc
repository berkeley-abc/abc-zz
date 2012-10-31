#include "abc_global.h"


namespace abc_sat {


char* vnsprintf(const char* format, va_list args)
{
    unsigned n;
    char*    ret;
    va_list  args_copy;

    static FILE* dummy_file = NULL;
    if (!dummy_file)
    {
#if !defined(_MSC_VER)
        dummy_file = fopen("/dev/null", "wb");
#else
        dummy_file = fopen("NUL", "wb");
#endif
    }

#if defined(__va_copy)
    __va_copy(args_copy, args);
#else
  #if defined(va_copy)
    va_copy(args_copy, args);
  #else
    args_copy = args;
  #endif
#endif
    n = vfprintf(dummy_file, format, args);
    ret = ABC_ALLOC( char, n + 1 );
    ret[n] = (char)255;
    args = args_copy;
    vsprintf(ret, format, args);
#if !defined(__va_copy) && defined(va_copy)
    va_end(args_copy);
#endif
    assert(ret[n] == 0);
    return ret;
}


char* nsprintf(const char* format, ...)
{
    char* ret;
    va_list args;
    va_start(args, format);
    ret = vnsprintf(format, args);
    va_end(args);
    return ret;
}


void Abc_MergeSortCostMerge( int * p1Beg, int * p1End, int * p2Beg, int * p2End, int * pOut )
{
    int nEntries = (p1End - p1Beg) + (p2End - p2Beg);
    int * pOutBeg = pOut;
    while ( p1Beg < p1End && p2Beg < p2End )
    {
        if ( p1Beg[1] == p2Beg[1] )
            *pOut++ = *p1Beg++, *pOut++ = *p1Beg++, *pOut++ = *p2Beg++, *pOut++ = *p2Beg++;
        else if ( p1Beg[1] < p2Beg[1] )
            *pOut++ = *p1Beg++, *pOut++ = *p1Beg++;
        else // if ( p1Beg[1] > p2Beg[1] )
            *pOut++ = *p2Beg++, *pOut++ = *p2Beg++;
    }
    while ( p1Beg < p1End )
        *pOut++ = *p1Beg++, *pOut++ = *p1Beg++;
    while ( p2Beg < p2End )
        *pOut++ = *p2Beg++, *pOut++ = *p2Beg++;
    assert( pOut - pOutBeg == nEntries );
}


void Abc_MergeSortCost_rec( int * pInBeg, int * pInEnd, int * pOutBeg )
{
    int nSize = (pInEnd - pInBeg)/2;
    assert( nSize > 0 );
    if ( nSize == 1 )
        return;
    if ( nSize == 2 )
    {
         if ( pInBeg[1] > pInBeg[3] )
         {
             pInBeg[1] ^= pInBeg[3];
             pInBeg[3] ^= pInBeg[1];
             pInBeg[1] ^= pInBeg[3];
             pInBeg[0] ^= pInBeg[2];
             pInBeg[2] ^= pInBeg[0];
             pInBeg[0] ^= pInBeg[2];
         }
    }
    else if ( nSize < 8 )
    {
        int temp, i, j, best_i;
        for ( i = 0; i < nSize-1; i++ )
        {
            best_i = i;
            for ( j = i+1; j < nSize; j++ )
                if ( pInBeg[2*j+1] < pInBeg[2*best_i+1] )
                    best_i = j;
            temp = pInBeg[2*i];
            pInBeg[2*i] = pInBeg[2*best_i];
            pInBeg[2*best_i] = temp;
            temp = pInBeg[2*i+1];
            pInBeg[2*i+1] = pInBeg[2*best_i+1];
            pInBeg[2*best_i+1] = temp;
        }
    }
    else
    {
        Abc_MergeSortCost_rec( pInBeg, pInBeg + 2*(nSize/2), pOutBeg );
        Abc_MergeSortCost_rec( pInBeg + 2*(nSize/2), pInEnd, pOutBeg + 2*(nSize/2) );
        Abc_MergeSortCostMerge( pInBeg, pInBeg + 2*(nSize/2), pInBeg + 2*(nSize/2), pInEnd, pOutBeg );
        memcpy( pInBeg, pOutBeg, sizeof(int) * 2 * nSize );
    }
}


int * Abc_MergeSortCost( int * pCosts, int nSize )
{
    int i, * pResult, * pInput, * pOutput;
    pResult = (int *) calloc( sizeof(int), nSize );
    if ( nSize < 2 )
        return pResult;
    pInput  = (int *) malloc( sizeof(int) * 2 * nSize );
    pOutput = (int *) malloc( sizeof(int) * 2 * nSize );
    for ( i = 0; i < nSize; i++ )
        pInput[2*i] = i, pInput[2*i+1] = pCosts[i];
    Abc_MergeSortCost_rec( pInput, pInput + 2*nSize, pOutput );
    for ( i = 0; i < nSize; i++ )
        pResult[i] = pInput[2*i];
    free( pOutput );
    free( pInput );
    return pResult;
}


}
