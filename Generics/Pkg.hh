//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Pkg.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Binary chunk class with pass-by-value/reference counting semantics.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__Pkg_hh
#define ZZ__Generics__Pkg_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'Pkg':


// A "package" is an immutable, reference counted array. Once created it can efficiently be passed 
// by value, and will be disposed automatically when the last reference is gone.


struct Pkg_Data  {
    uint    refC;
    uint    sz;
    uchar   data[1];
};


struct Pkg {
    Pkg_Data* ptr;

    static uint allocSize(uint n_elems) { return sizeof(Pkg_Data) + n_elems - 1; }

  //________________________________________
  //  Constructors:

    Pkg() : ptr(NULL) {}
    Pkg(const uchar* data, uint sz){ init(data, sz); }
    Pkg(const Array<uchar>& data)  { init(&data[0], data.size()); }
    Pkg(const Vec<uchar>&   data)  { init(&data[0], data.size()); }

  //________________________________________
  //  Reference counting:

    Pkg(const Pkg& other);
    Pkg& operator=(const Pkg& other);
   ~Pkg();

  //________________________________________
  //  Methods:

    uint         size      ()       const { return ptr->sz; }
    const uchar& operator[](uint i) const { return ptr->data[i]; }
    const uchar& end_      ()       const { return ptr->data[ptr->sz]; }

    bool   null() const { return ptr == NULL; }
    typedef Pkg_Data* Pkg::*bool_type;
    operator bool_type() const { return null() ? 0 : &Pkg::ptr; }

    void mkNull() { this->~Pkg(); new (this) Pkg(); }
        // -- Release this reference and put package in null state.

    const uchar*       base () const { return &ptr->data[0]; }
    Array<const uchar> slice() const { return ::ZZ::slice(this->operator[](0), this->end_()); }

protected:
  //________________________________________
  //  Helpers:

    Pkg(Pkg_Data* p) : ptr(p) {}
    void init(const uchar* data, uint sz);
};


static const Pkg Pkg_NULL;


template<> fts_macro void write_(Out& out, const Pkg& v)
{
    if (!v)
        Write "Pkg_NULL";
    else
        Write "Pkg<%_>", v.size();
}


//=================================================================================================
// -- Implementation:


inline void Pkg::init(const uchar* data, uint sz)
{
    ptr = (Pkg_Data*)xmalloc<uchar>(allocSize(sz));
    ptr->sz = sz;
    ptr->refC = 1;
    for (uint i = 0; i < sz; i++)
        ptr->data[i] = data[i];
}


inline Pkg::~Pkg()
{
    if (ptr != NULL){
        ptr->refC--;
        if (ptr->refC == 0)
            free((void*)ptr);
    }
}


inline Pkg::Pkg(const Pkg& other)
{
    ptr = other.ptr;
    if (other.ptr != NULL)
        other.ptr->refC++;
}


inline Pkg& Pkg::operator=(const Pkg& other)
{
    if (this == &other)
        return *this;

    if (ptr != NULL){
        ptr->refC--;
        if (ptr->refC == 0)
            free((void*)ptr);
    }

    ptr = other.ptr;
    if (other.ptr != NULL)
        other.ptr->refC++;

    return *this;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
