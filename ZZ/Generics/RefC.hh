//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : RefC.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Intrusive reference counting.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Use 'RefC<T>' as a reference counted pointer to an object of type 'T', which must have a field
//| 'refC'. To get to the object, use '*ref' or 'ref->...'. When last reference to the object of 
//| type 'T' is gone, it will be automatically deleted (by operator 'delete').
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__RefCount_hh
#define ZZ__Generics__RefCount_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Wraps 'Data' with a reference counting mechanism. Assumes 'Data' has a field 'refC' of 
// an unsigned integer type (only 32 or 64 bit makes sense). If the field overflow, an 
// assertion blows (no saturation implemented -- it would require a user-specified allocation
// mechanism to avoid memory leak, which has no convenient implementation for 'dispose()'),
//
template<class Data>
class RefC {
    Data*   ptr;

    static void ref_(Data* p) {
        if (p == NULL) return;
        p->refC++;
        assert(p->refC != 0);   // -- check for overflow
    }

    static void unref_(Data* p) {
        if (p == NULL) return;
        assert(p->refC != 0);
        p->refC--;
        if (p->refC == 0) delete p;
    }

public:
  //________________________________________
  //  Constructors:

    RefC() {
        ptr = NULL; }

    RefC(Tag_empty) {
        ptr = new Data();
        ptr->refC = 1; }

    RefC(Data* data) {
        ptr = data;
        ptr->refC = 1; }

    RefC(const Data& data) {
        ptr = new Data(data);
        ptr->refC = 1; }

  //________________________________________
  //  Reference counting:

   ~RefC() {
        unref_(ptr); }

    RefC(const RefC& other) {
        ptr = other.ptr;
        ref_(other.ptr); }

    RefC& operator=(const RefC& other) {
        if (this == &other) return *this;
        unref_(ptr);
        ptr = other.ptr;
        ref_(ptr);
        return *this;
    }

  //________________________________________
  //  Dereference:

    Data& operator* () const { return *ptr;}
    Data* operator->() const { return ptr; }

    bool  null() const { return ptr == NULL; }
    typedef Data* RefC<Data>::*bool_type;
    operator bool_type() const { return null() ? 0 : &RefC<Data>::ptr; }
};


template<class Data> fts_macro void write_(Out& out, const RefC<Data>& d) {
    out %= "RefC %_", *d; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
