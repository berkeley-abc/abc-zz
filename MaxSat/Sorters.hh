//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Sorters.hh
//| Author(s)   : Niklas Een
//| Module      : MaxSat
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Parameter 'cmp' should be a functor 'fun(uint i, uint j)' which adds a comparator between line
//| 'i' and line 'j'. Example:
//| 
//|     struct Cmp {
//|         Netlist& N;
//|         Vec<Wire>& result; // -- initialized to input signals; will contain output signals
//| 
//|         Cmp(Netlist& N_, Vec<Wire>& result_) : N(N_), result(result_) {}
//| 
//|         void operator()(uint i, uint j) {
//|             l_tuple(result[i], result[j]) = tuple(mkMin(N, result[i], result[j]), 
//|                                                   mkMax(N, result[i], result[j])); 
//|         }
//|     };
//| 
//| To create the network (which must be of size 2^k):
//|     
//|     inputs.copyTo(outputs);
//|     Cmp cmp(N, outputs);
//|     pwSort(outputs.size(), cmp);
//|     
//|________________________________________________________________________________________________

#ifndef ZZ__MaxSat__Sorters_hh
#define ZZ__MaxSat__Sorters_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Odd-even sorter:


// Only element whose index 'i' meets '(i - lo) % r == 0' is considered.
template<class MkCmp>
void oeMerge(uint lo, uint hi, uint r, MkCmp& cmp)
{
    uint step = r * 2;
    if (step < hi - lo){
        oeMerge(lo, hi-r, step, cmp);
        oeMerge(lo+r, hi, step, cmp);
        for (uint i = lo+r; i < hi-r; i += step)
            cmp(i, i+r);
    }else
        cmp(lo, lo+r);
}


template<class MkCmp>
void oeSortRange(uint lo, uint hi, MkCmp& cmp)
{
    if (hi - lo >= 1){
        uint mid = lo + (hi - lo) / 2;
        oeSortRange(lo, mid, cmp);
        oeSortRange(mid+1, hi, cmp);
        oeMerge(lo, hi, 1, cmp);
    }
}


template<class MkCmp>
void oeSort(uint len, MkCmp& cmp) {
    oeSortRange(0, len-1, cmp); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pair-wise sorter:


// Only element whose index 'i' meets '(i - lo) % r == 0' is considered.
template<class MkCmp>
void pwMerge(uint lo, uint hi, uint r, MkCmp& cmp)
{
    uint step = r * 2;
    if (step < hi - lo){
        pwMerge(lo, hi-r, step, cmp);
        pwMerge(lo+r, hi, step, cmp);
        for (uint i = lo+r; i < hi-r; i += step)
            cmp(i, i+r);
    }
}


template<class MkCmp>
void pwSortRange(uint lo, uint hi, MkCmp& cmp)
{
    if (hi - lo >= 1){
        uint mid = lo + (hi - lo) / 2;
        for (uint i = lo; i <= mid; i++)
            cmp(i, i + (hi - lo + 1) / 2);
        pwSortRange(lo, mid, cmp);
        pwSortRange(mid+1, hi, cmp);
        pwMerge(lo, hi, 1, cmp);
    }
}


template<class MkCmp>
void pwSort(uint len, MkCmp& cmp) {
    pwSortRange(0, len-1, cmp); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
