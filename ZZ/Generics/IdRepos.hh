//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : IdRepos.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Provides a mechanism for allocating and returning IDs (integers 0, 1, 2...).
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__IdRepos_hh
#define ZZ__Generics__IdRepos_hh

#include "IntSet.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class IdRepos : public NonCopyable{
    friend void write_IdRepos(Out& out, const IdRepos& v);

    uint          sz;
    IntZet<uint>  freed;

public:
  //________________________________________
  //  Construction:

    IdRepos() : sz(0) {}
    void clear(bool dispose = false) { freed.clear(dispose); sz = 0; }

    void moveTo(IdRepos& dst)       { freed.moveTo(dst.freed); dst.sz = sz; sz = 0; }
    void copyTo(IdRepos& dst) const { freed.copyTo(dst.freed); dst.sz = sz; }

  //________________________________________
  //  Repository interface:

    uint get();               // -- Get an unused ID.
    void pick(uint id);       // -- Reserve a specific ID. It is an error to reserve an ID already in use.

    void free(uint id);       // -- Give an ID back to the repository (must be in use).  

    uint size() const;        // -- Returns the index of the highest ID ever used plus one ('0' if no ID has been used).
    uint count() const;       // -- Returns the number of active IDs.

    bool used(uint id) const; // -- Is given ID in use?  
};


//=================================================================================================
// -- Implementation:


inline uint IdRepos::get() {
#if 1
    return (freed.size() > 0) ? freed.popLastC() : sz++;
#else
    // Experimental version -- pops the tail of freed IDs before using free list (affects the order)
    if (freed.size() == 0)
        return sz++;
    else{
        for(;;){
            uint ret = freed.popLastC();
            if (ret != sz - 1 || freed.size() == 0)
                return ret;
            sz--;
        }
    }
#endif
}

inline void IdRepos::pick(uint id) {
    if (id < sz){
        bool ok = freed.exclude(id); assert(ok);    // -- fails if trying to pick an ID already in use
    }else{
        while (sz < id) freed.add(sz++);
        sz++;
    }
}

inline bool IdRepos::used(uint id) const {
    return id < sz && !freed.has(id); }

inline void IdRepos::free(uint id) {
    assert(used(id));
    freed.add(id); }

inline uint IdRepos::size() const {
    return sz; }

inline uint IdRepos::count() const {
    return sz - freed.size(); }


//=================================================================================================
// -- Debug:


inline void write_IdRepos(Out& out, const IdRepos& v)
{
    FWrite(out) "{sz=%_; freed=[", v.sz;
    bool first = true;
    for (uind i = 0; i < v.freed.list().size(); i++){
        uint key = v.freed.list()[i];
        if (v.freed.has(key)){
            if (first) first = false;
            else out += ' ';

            out += key;
        }
    }
    out += "]}";
}

template<> fts_macro void write_(Out& out, const IdRepos& v) {
    write_IdRepos(out, v); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
