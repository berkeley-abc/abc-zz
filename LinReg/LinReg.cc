//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LinReg.cc
//| Author(s)   : Niklas Een
//| Module      : LinReg
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "LinReg.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gaussian elemination:


// Will transform 'A' (square matrix) into an identity matrix and 'b' into the solution vector.
// Returns TRUE if successful, FALSE if numberic instability was detected (indicating which
// variable was problematic through 'probl_idx').
bool gaussElim(Vec<Vec<double> >& A, Vec<double>& b, uint* probl_idx)
{
    // Check sizes:
    uint n = A.size(); assert(b.size() == n);
    if (n == 0) return true;
    for (uint i = 0; i < n; i++) assert(A[i].size() == n);

    // Eliminate variables downward:
    for (uint i = 0; i < n; i++){
        //**/WriteLn "i=%_ ----ELIM-DOWN---------------------", i; dumpMatrix(A, b, i, i);

        // Look for largest value in the i:th column:
        double best = fabs(A[i][i]);
        uint   idx  = i;
        for (uint j = i+1; j < n; j++)
            if (newMax(best, fabs(A[j][i])))
                idx = j;
        swp(A[i], A[idx]);
        swp(b[i], b[idx]);

        // Ugly hack for numerical problems; should do something better..
        if (fabs(A[i][i]) < 1e-8){
            if (probl_idx) *probl_idx = i;
            return false; }

        for (uint j = i+1; j < n; j++){
            double c = A[j][i] / A[i][i];

            A[j][i] = 0;
            for (uint k = i+1; k < n; k++)
                A[j][k] -= A[i][k] * c;     // <<== measure cancelation here?
            b[j] -= b[i] * c;               // <<== and here
        }
    }

    // Normalize with diagonal:
    for (uint i = 0; i < n; i++){
        //**/WriteLn "i=%_ ----NORM-DIAG---------------------", i; dumpMatrix(A, b, i, i);
        double c = 1.0 / A[i][i];
        A[i][i] = 1;
        for (uint j = i+1; j < n; j++)      // <<== starta på i+1 och sätt 1 direkt på plats
            A[i][j] *= c;
        b[i] *= c;
    }

    // Eliminate variables upward:
    for (uint i = 1; i < n; i++){
        //**/WriteLn "i=%_ ----ELIM-UP-----------------------", i; dumpMatrix(A, b, i, i);

        for (uint j = 0; j < i; j++){
            double c = A[j][i];
            A[j][i] = 0;
            for (uint k = i+1; k < n; k++)
                A[j][k] -= A[i][k] * c;
            b[j] -= b[i] * c;
        }
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Solving minimization problem:


// For memory efficiency, 'data' is stored on form 'data[var#][sample#]' with last "var" being
// the RHS (correct answer). If system is unstable, the problemativ variable is returned through
// 'probl_idx'. Returns TRUE if solution is stable.
//
bool linearRegression(const Vec<Vec<double> >& data, /*out*/Vec<double>& coeff, uint* probl_idx)
{
    uint n = data.size() - 1;       // -- 'n' = number of variables
    uint m = data[0].size();        // -- 'm' = number of sample points
    for (uint i = 1; i < data.size(); i++)
        assert(data[i].size() == m);

    // Create empty matrix:
    Vec<Vec<double> > H(n);     // -- will encode 'A^T * A'
    for (uint i = 0; i < H.size(); i++)
        H[i].growTo(n, 0.0);

    // Fill in lower triangle:
    for (uint i = 0; i < n; i++){
        for (uint j = 0; j <= i; j++){
            double sum = 0;
            for (uint s = 0; s < m; s++)
                sum += data[i][s] * data[j][s];
            H[i][j] = sum;
        }
    }

    // Copy to upper triangle:
    for (uint i = 0; i < n; i++)
        for (uint j = i+1; j < n; j++)
            H[i][j] = H[j][i];

    // Created target column:
    Vec<double>& d = coeff;   // -- will encode 'A^T * b'
    d.setSize(n);
    for (uint i = 0; i < n; i++){
        double sum = 0;
        for (uint s = 0; s < m; s++)
            sum += data[i][s] * data[n][s];     // -- 'data[n]' contains the answers
        d[i] = sum;
    }

    if (!gaussElim(H, d, probl_idx))
        return false;

    return true;
}


bool linearRegression(const Vec<Vec<double> >& data, /*out*/Vec<double>& coeff)
{
    uint probl_idx;
    bool ret = linearRegression(data, coeff, &probl_idx);
    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
