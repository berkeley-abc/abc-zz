//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cube.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Cube_hh
#define ZZ__Bip__Cube_hh

#include "ZZ_Netlist.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


ZZ_PTimer_Declare(Cube_Constr);


struct Cube_Data {
    uint64  abstr;
    uint    sz;
    uint    refC;
    GLit    data[1];
};


// A 'Cube' clause is sorted and static in size, contains a 64-bit abstraction and does
// reference counting for the underlying literal vector.
struct Cube {
    Cube_Data* ptr;

    static uint allocSize(uint n_elems) { return sizeof(Cube_Data) - sizeof(GLit) + sizeof(GLit) * n_elems; }

  //________________________________________
  //  Constructors:

    Cube() : ptr(NULL) {}

    Cube(Array<const GLit> ps) { init(ps); }
    Cube(const Vec<GLit>&  ps) { init(ps.slice()); }
    Cube(GLit singleton)       { init(slice((&singleton)[0], (&singleton)[1])); }

  //________________________________________
  //  Reference counting:

    Cube(const Cube& other) {
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
    }

    Cube& operator=(const Cube& other) {
        if (this == &other) return *this;
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz)); }
        ptr = other.ptr;
        if (other.ptr != NULL) other.ptr->refC++;
        return *this;
    }

   ~Cube() {
        if (ptr != NULL){
            ptr->refC--;
            if (ptr->refC == 0) yfree((uchar*)ptr, allocSize(ptr->sz));
        }
    }

  //________________________________________
  //  Methods:

    uint   size      ()       const { return ptr->sz; }
    GLit   operator[](uint i) const { return ptr->data[i]; }
    uint64 abstr     ()       const { return ptr->abstr; }

    bool   null      ()       const { return ptr == NULL; }
    typedef Cube_Data* Cube::*bool_type;
    operator bool_type() const { return null() ? 0 : &Cube::ptr; }

    Cube operator-(GLit p);     // -- create a copy of this cube without element 'p' (which MUST exist)
    Cube operator+(Cube c);
    Cube sub(uind start, uind end) const { return Cube(slice(ptr->data[start], ptr->data[end])); }

    //*TEMPORARY*/Array<GLit> base() { return slice(ptr->data[0], ptr->data[ptr->sz]); }

protected:
  //________________________________________
  //  Helpers:

    Cube(Cube_Data* p) : ptr(p) {}

    void init(Array<const GLit> ps) {
        ZZ_PTimer_Begin(Cube_Constr);
        ptr = (Cube_Data*)ymalloc<char>(allocSize(ps.size()));
        ptr->abstr = 0;
        ptr->sz = ps.size();
        ptr->refC = 1;
        for (uind i = 0; i < ps.size(); i++){
            ptr->abstr |= uint64(1) << (ps[i].data() & 63);
            ptr->data[i] = ps[i]; }
#if 1
        sort();
#endif
        ZZ_PTimer_End(Cube_Constr);
    }

    void sort();
};


static const Cube Cube_NULL;


template<> fts_macro void write_(Out& out, const Cube& c) {
    if (c.null()) out += "<null>";
    else write_(out, slice(c.ptr->data[0], c.ptr->data[c.ptr->sz])); }


macro bool operator==(const Cube& x, const Cube& y)
{
    if (x.ptr == y.ptr) return true;
    if (x.null() || y.null()) return false;
    if (x.abstr() != y.abstr()) return false;
    return vecEqual(x, y);
}


bool subsumes(const Cube& small_, const Cube& big);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
