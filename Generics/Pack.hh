//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pack.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : A pack is a small, immutable set with pass-by-value semantics.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__Pack_hh
#define ZZ__Generics__Pack_hh

#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


template<class T>
struct Pack_Data {
    uint64  abstr;
    uint    sz;
    uint    refC;
    T       data[1];
};


// A 'Pack' clause is sorted and static in size, contains a 64-bit abstraction and does
// reference counting for the underlying literal vector.
template<class T>
struct Pack {
    Pack_Data<T>* ptr;

    static uint allocSize(uint n_elems) { return sizeof(Pack_Data<T>) - sizeof(T) + sizeof(T) * n_elems; }

  //________________________________________
  //  Constructors:

    Pack() : ptr(NULL) {}

    Pack(Array<const T> ps) { init(ps); }
    Pack(const Vec<T>&  ps) { init(ps.slice()); }
    Pack(T singleton)       { init(slice((&singleton)[0], (&singleton)[1])); }

  //________________________________________
  //  Reference counting:

    Pack(const Pack& other) {
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
    }

    Pack& operator=(const Pack& other) {
        if (this == &other) return *this;
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz)); }
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
        return *this;
    }

   ~Pack() {
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz));
        }
    }

  //________________________________________
  //  Methods:

    uint   size      ()       const { return ptr->sz; }
    T      operator[](uint i) const { return ptr->data[i]; }
    uint64 abstr     ()       const { return ptr->abstr; }

    bool   null      ()       const { return ptr == NULL; }
    typedef Pack_Data<T>* Pack::*bool_type;
    operator bool_type() const { return null() ? 0 : &Pack::ptr; }

    Pack operator-(T p);     // -- create a copy of this pack without element 'p' (which MUST exist)
    Pack operator+(const Pack& c);
    Pack sub(uind start, uind end) const { return Pack(slice(ptr->data[start], ptr->data[end])); }

    Array<T> base() const { return slice(ptr->data[0], ptr->data[ptr->sz]); }

protected:
  //________________________________________
  //  Helpers:

    Pack(Pack_Data<T>* p) : ptr(p) {}

    void init(Array<const T> ps) {
        ptr = (Pack_Data<T>*)ymalloc<char>(allocSize(ps.size()));
        ptr->abstr = 0;
        ptr->sz = ps.size();
        ptr->refC = 1;
        for (uind i = 0; i < ps.size(); i++){
            ptr->abstr |= uint64(1) << (defaultHash(ps[i]) & 63);
            ptr->data[i] = ps[i]; }
        sort();
    }

    void sort() {
        Array<T> proxy(ptr->data, ptr->sz);
        ::ZZ::sort(proxy);
        for (uint i = 1; i < this->size(); i++)
            assert(ptr->data[i-1] != ptr->data[i]);     // -- packs must created with unique elements
    }
};


template<class T> fts_macro void write_(Out& out, const Pack<T>& c) {
    if (c.null()) out += "<null>";
    else write_(out, slice(c.ptr->data[0], c.ptr->data[c.ptr->sz])); }


template<class T> fts_macro bool operator==(const Pack<T>& x, const Pack<T>& y) {
    if (x.ptr == y.ptr) return true;
    if (x.null() || y.null()) return false;
    if (x.abstr() != y.abstr()) return false;
    return vecEqual(x, y); }


template<class T> fts_macro bool operator<(const Pack<T>& x, const Pack<T>& y) {
    return vecLessThan(x.base(), y.base()); }


//=================================================================================================
// -- Bigger methods/functions:


template<class T>
Pack<T> Pack<T>::operator-(T p)
{
    uint sz = this->size() - 1;
    Pack_Data<T>* tmp = (Pack_Data<T>*)ymalloc<char>(allocSize(sz));
    tmp->abstr = 0;
    tmp->sz = sz;
    tmp->refC = 1;
    uint j = 0;
    for (uint i = 0; i < this->size(); i++){
        T q = (*this)[i];
        if (q != p){
            assert(j != sz);    // -- if assertion fails, 'p' did not exist in this pack
            tmp->abstr |= uint64(1) << (defaultHash(q) & 63);
            tmp->data[j++] = q;
        }
    }
    assert(j == sz);
    return Pack(tmp);
}


// Temporary inefficient implementation
template<class T>
Pack<T> Pack<T>::operator+(const Pack<T>& c)
{
#if 0
    Vec<T> tmp(reserve_, this->size() + c.size());
    for (uint i = 0; i < this->size(); i++)
        tmp.push((*this)[i]);
    for (uint i = 0; i < c.size(); i++)
        tmp.push(c[i]);
    sortUnique(tmp);

    return Pack<T>(tmp);

#else
    // Determine size of new pack:
    uint i = 0, j = 0, sz = 0;
    Pack<T>& b = *this;
    for(;;){
        if (i >= b.size()){ sz += c.size() - j; break; }
        if (j >= c.size()){ sz += b.size() - i; break; }
        if      (b[i] < c[j]){ i++; }
        else if (c[j] < b[i]){ j++; }
        else                 { i++; j++; }
        sz++;
    }

    // Allocate and build new pack:
    Pack_Data<T>* p = (Pack_Data<T>*)ymalloc<char>(allocSize(sz));
    p->abstr = 0;
    p->sz = sz;
    p->refC = 1;

    i = j = sz = 0;
    for(;;){
        if (i >= b.size()){
            while (j < c.size()){
                T elem = c[j++];
                p->abstr |= uint64(1) << (defaultHash(elem) & 63);
                p->data[sz++] = elem;
            }
            break;
        }
        if (j >= c.size()){
            while (i < b.size()){
                T elem = b[i++];
                p->abstr |= uint64(1) << (defaultHash(elem) & 63);
                p->data[sz++] = elem;
            }
            break;
        }

        T elem;
        if      (b[i] < c[j]){ elem = b[i]; i++; }
        else if (c[j] < b[i]){ elem = c[j]; j++; }
        else                 { elem = b[i]; i++; j++; }

        p->abstr |= uint64(1) << (defaultHash(elem) & 63);
        p->data[sz++] = elem;
    }

    return Pack(p);
#endif
}


template<class T>
fts_macro bool subsumes(const Pack<T>& small_, const Pack<T>& big)
{
    if (small_.abstr() & ~big.abstr()) return false;

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
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
