//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : NamedSet.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Hash-set owning elements of type 'T', where 'T' is assumed to have a field 'name'.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| The field 'name' should be assignable by a 'Str' and a 'Str' should be extractable from it 
//| through 'slice(name)'. IMPORTANT! The string must stay fixed in memory while this set is alive.
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__NamedSet_hh
#define ZZ__Generics__NamedSet_hh

#include "Map.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


template<class T>
class NamedSet {
    Vec<T>        data;
    Map<Str,uind> index;

public:
  //________________________________________
  //  Add/get by name:

    bool add(Str name);
        // -- Adds a new default constructed 'T' element of given name. Returns TRUE if 
        // successful, FALSE if an element of that name already exists.

    T& last() { return data.last(); }
        // -- Access last element added.

    T& addWeak(Str name);
        // -- If 'name' exists, return a reference to that element, otherwise create a new element
        // and return a reference to it.

    uind idx(Str name) const;
        // -- Returns the index of the 'operator[]' or UIND_MAX if doesn't exist.

    bool has(Str name) const { return index.has(name); }
        // -- Is there a 'T' element of given name?

  //________________________________________
  //  Add anonymous, then name it:

    void push() { assert(data.size() == 0 || data.last().name); data.push(); }
        // -- Adds a new default constructed 'T' but does not yet add it to the hash table.
        // It is illegal to do another 'push()' or 'add()' before calling 'hash()'.

    bool hash(Str name);
        // -- Give a name to the last element. If there is a collision, FALSE is returned and the
        // last anonymous element deleted.

  //________________________________________
  //  Iteration:

    T&       operator[](uind i)       { return data[i]; }
    const T& operator[](uind i) const { return data[i]; }
    uind size() const { return data.size(); }
        // -- Vector interface; mustn't change the 'name' field, or else call 'rehash()' when done.

    Vec<T>&       list()       { return data; }
    const Vec<T>& list() const { return data; }

    void rehash();  // -- Recomputes hash table; it is illegal to have name collision (assertion failure).

  //________________________________________
  //  Whole set manipulation:

    void clear() { data.clear(true); index.clear(); }
    void moveTo(NamedSet& other) { data.moveTo(other.data); index.moveTo(other.index); }
};


template<class T>
bool NamedSet<T>::add(Str name)
{
    uind idx = data.size();

    data.push();
    data.last().name = name;
    Str key = strSlice(data.last().name);

    uind* idx_ptr;
    if (index.get(key, idx_ptr)){
        data.pop();
        return false;
    }
    *idx_ptr = idx;

    return true;
}


template<class T>
uind NamedSet<T>::idx(Str name) const
{
    uind idx;
    if (!index.peek(name, idx))
        return UIND_MAX;

    return idx;
}


template<class T>
T& NamedSet<T>::addWeak(Str name)
{
    uind i = idx(name);
    if (i != UIND_MAX)
        return data[i];
    else{
        bool ok = add(name); assert(ok);
        return last();
    }
}


template<class T>
bool NamedSet<T>::hash(Str name)
{
    data.last().name = name;
    Str key = strSlice(data.last().name);

    uind* idx_ptr;
    if (index.get(key, idx_ptr)){
        data.pop();
        return false;
    }
    *idx_ptr = data.size()-1;

    return true;
}


template<class T>
void NamedSet<T>::rehash()
{
    index.clear();
    for (uind i = 0; i < data.size(); i++){
        Str key = strSlice(data[i].name);
        bool collision = index.set(key, i); assert(!collision);
    }
}


//=================================================================================================
// -- Formated output:


template<class T>
fts_macro void write_(Out& out, const NamedSet<T>& v) {
    write_(out, v.list()); }

template<class T>
fts_macro void write_(Out& out, const NamedSet<T>& v, Str flags) {
    write_(out, v.list(), flags); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
