//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cube.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Cube.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


ZZ_PTimer_Add(Cube_Minus);
ZZ_PTimer_Add(Cube_Subsume);
ZZ_PTimer_Add(Cube_Constr);


Cube Cube::operator-(GLit p)
{
    ZZ_PTimer_Begin(Cube_Minus);
    uint sz = this->size() - 1;
    Cube_Data* tmp = (Cube_Data*)ymalloc<char>(allocSize(sz));
    tmp->abstr = 0;
    tmp->sz = sz;
    tmp->refC = 1;
    uint j = 0;
    for (uint i = 0; i < this->size(); i++){
        GLit q = (*this)[i];
        if (q != p){
            assert(j != sz);    // -- if assertion fails, 'p' did not exist in this cube
            tmp->abstr |= uint64(1) << (q.data() & 63);
            tmp->data[j++] = q;
        }
    }
    assert(j == sz);
    ZZ_PTimer_End(Cube_Minus);
    return Cube(tmp);
}


// Temporary inefficient implementation
Cube Cube::operator+(Cube c)
{
    Vec<GLit> tmp(reserve_, this->size() + c.size());
    for (uint i = 0; i < this->size(); i++)
        tmp.push((*this)[i]);
    for (uint i = 0; i < c.size(); i++)
        tmp.push(c[i]);
    sortUnique(tmp);

    return Cube(tmp);
}


void Cube::sort()
{
    Array<GLit> proxy(ptr->data, ptr->sz);
    ::ZZ::sort(proxy);
}


bool subsumes(const Cube& small_, const Cube& big)
{
    if (small_.abstr() & ~big.abstr()) return false;
#if 1
    ZZ_PTimer_Begin(Cube_Subsume);
    uint j = 0;
    for (uint i = 0; i < small_.size();){
        if (j >= big.size())
            return false;
        if (small_[i] == big[j]){
            i++;
            j++;
        }else
            j++;
    }
    ZZ_PTimer_End(Cube_Subsume);
    return true;

#else
    for (uint i = 0; i < small_.size(); i++){
        if (!has(big, small_[i]))
            return false;
    }
    return true;
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
