//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LinReg.hh
//| Author(s)   : Niklas Een
//| Module      : LinReg
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__LinReg__LinReg_hh
#define ZZ__LinReg__LinReg_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool gaussElim(Vec<Vec<double> >& A, Vec<double>& b, uint* probl_idx = NULL);
bool linearRegression(const Vec<Vec<double> >& data, /*out*/Vec<double>& coeff, uint* probl_idx = NULL);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
