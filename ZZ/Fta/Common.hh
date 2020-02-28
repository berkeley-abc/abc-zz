//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.hh
//| Author(s)   : Niklas Een
//| Module      : Fta
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Fta__Common_hh
#define ZZ__Fta__Common_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void oddEvenSort(Gig& N, Vec<GLit>& fs);
void convertToAig(Gig& N);
void pushNegations(Gig& N);
Wire addCutoff(Gig& N, double cutoff_lo, double cutoff_hi, const Vec<GLit>& vars, const Vec<double>& probs, double quanta);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
