//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : OrdSet.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : An ordered hash set.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| Nodes can be iterated over in the orderer they were inserted. Deleted nodes will also be
//| present unless 'compact()' was called, but true existence can be checked with the 'has()'
//| method.
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__OrdSet_hh
#define ZZ__Generics__OrdSet_hh

#include "Set.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// OrdSet:


template<class Key_, class Hash_ = Hash_default<Key_> >
class OrdSet : public NonCopyable {
protected:
    Set<Key_>   set_;
    Vec<Key_>   list_;

public:
    typedef Key_ Key;

  //________________________________________
  //  Constructors:

    OrdSet()                  : set_()       {}
    OrdSet(uind cap)          : set_(cap)    {}
    OrdSet(Hash_ p)           : set_(p)      {}
    OrdSet(uind cap, Hash_ p) : set_(cap, p) {}

    void reserve(uind cap)             { set_.reserve(cap); list_.reserve(cap); }
    uind size   () const               { return set_.size(); }
    void clear  (bool dispose = false) { set_.clear(); list_.clear(dispose); }

    void copyTo(OrdSet& copy) const { set_.copyTo(copy.set_); list_.copyTo(copy.list_); }
    void moveTo(OrdSet& dest)       { set_.moveTo(dest.set_); list_.moveTo(dest.list_); }

  //________________________________________
  //  Set operations:

    bool add(const Key_& key) {
        // -- Add element unless already in set. Returns TRUE if element already existed.
        bool already_has = set_.add(key);
        if (!already_has)
            list_.push(key);
        return already_has;  }

    bool has(const Key_& key) const {
        // -- Check for existence of element.
        return set_.has(key); }

    bool exclude(const Key_& key) {
        // -- Exclude element if exists (returns TRUE if found). Element is still in list until 
        // 'compact()' is called, but 'has()' will return FALSE.
        return set_.exclude(key); }

  //________________________________________
  //  Iteratation:

    Vec<Key_>&       list()       { return list_; }
    const Vec<Key_>& list() const { return list_; }

    Set<Key_>&       set()       { return set_; }
    const Set<Key_>& set() const { return set_; }

    void compact() {
        // -- Remove exluded elements from the list representation. NOTE! If an element has
        // been repeatedly added and deleted, it will occur at the first add after the last 
        // delete. Eg. 'add(x), exclude(x), add(x), add(x)'; then the second add right after the
        // exclude will determine the position of 'x' (assuming there were more 'add()'s
        // in between).
        Vec<Key_> ks;
        for (uind i = list_.size(); i > 0;){ i--;
            if (has(list_[i])){
                ks.push(list_[i]);
                set_.exclude(list_[i]);
            }
        }
        reverse(ks);
        for (uind i = 0; i < ks.size(); i++)
            set_.add(ks[i]);
        ks.moveTo(list_);
    }


};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
