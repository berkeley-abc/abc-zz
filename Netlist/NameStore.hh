//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : NameStore.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Maps gate IDs to strings (and back if look-up is enabled).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__NameStore_h
#define ZZ__Netlist__NameStore_h

#include "ZZ/Generics/Map.hh"
#include "BasicTypes.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Name Store:


Declare_Exception(Excp_NameClash); // -- thrown if the same name is given to two different signals


// Stores a set of signed names for each gate. If gate 'x12' has a signed name "~top_node", it means
// that it is actually '~x12' that is named "top_node". By default, reverse lookup is turned off.
// Turning it on will activate hashing of names (a bit slower and takes more memory). Although
// a gate can have many names, a name can only be tied to one gate.
//
// NOTE! This implementation doesn't yet compress the names. It should be considered a placeholder
// for a future, more space efficient implementation.
//
class NameStore {
    union Names {
        cchar*       name;   // -- steal two bits of the pointer: bit1=1, bit0=sign
        Vec<cchar*>* names;  // --                                bit1=0, bit0=1
        Names() : name(0) {}
    };

    SlimAlloc<char>   mem;
    Vec<Names>        id2names;
    Map<cchar*,GLit>  name2sid;
    bool              lookup_enabled;
    mutable Vec<char> tmp;

    // Internal helpers:
    static uintp        tag  (cchar*       ptr) { return uintp(ptr) & 3; }
    static cchar*       strip(cchar*       ptr) { return (cchar*)(uintp(ptr) & ~uintp(3)); }
    static Vec<cchar*>* strip(Vec<cchar*>* ptr) { return (Vec<cchar*>*)(uintp(ptr) & ~uintp(3)); }

    static void getName(Names ns, uind index, cchar*& out_name, bool& out_sign) {
        assert(ns.name != 0);
        if (tag(ns.name) & 2){
            assert_debug(index == 0);
            out_name = strip(ns.name);
            out_sign = tag(ns.name) & 1;
        }else{
            Vec<cchar*>& names = *strip(ns.names);
            out_name = strip(names[index]);
            out_sign = tag(names[index]);
        }
    }

    static uind getSize(Names ns) {
        if (ns.name == 0)
            return 0;
        else if (tag(ns.name) & 2)
            return 1;
        else{
            Vec<cchar*>& names = *strip(ns.names);
            return names.size();
        }
    }

    void anonymousName(GLit sid, Vec<char>& out_name) const;
    void invertSid(cchar* name);

public:
  //________________________________________
  //  Name format

    char anonymous_prefix;
        // -- 'get()' will return a name on the form '@123' for unnamed gates, where '@' is the
        // 'anonymous_prefix' and '123' is the gate ID. Default is "@". You may modify this
        // variable at any time.

    char invert_prefix;
        // -- the version of 'get()' without 'out_sign' will prefix inverted gates with this
        // string. Default is "~". You may modify this variable at any time.

    Vec<char> scratch;
        // -- You can use this variable with 'get' (but be careful not to use it in two
        // places simultaneously).

  //________________________________________
  //  Constructor/Destructor

    NameStore(bool enable_lookup = false);
   ~NameStore();

    void clear() {
        this->~NameStore();
        new (this) NameStore(); }

    void moveTo(NameStore& dst);

  //________________________________________
  //  Info

    uind size() const { return id2names.size(); }
        // -- Returns maximum ID of a gate with name plus one.

  //________________________________________
  //  Setting names

    void add(GLit sid, cchar* name);
    void add(GLit sid, Str    name);
        // -- Add a name to a (signed) gate 'sid'. Gates can have many names. However, it is
        // illegal to give the same name to two different gates (or indeed to the same gate twice).
        // The string 'name' is copied by 'add()'. If 'name[0] == invert_prefix', then
        // that character is consumed and 'sid' is negated.

    void invert(GLit sid);
        // -- Invert the polarity of all names tied to gate 'sid' (sign of 'sid' is ignored)

    void clear(GLit sid);

  //________________________________________
  //  Reading names

    uind size(GLit sid) const { return (sid.id >= id2names.size()) ? 0 : getSize(id2names[sid.id]); }
        // -- How many names does the gate have? Passing 'index' greater than or equal to
        // this value to 'get()' will return an anonymous name ('anonymous_prefix' followed by
        // gate ID, possibly prefixed with 'invert_prefix' if 'sid' is inverted).

    char* get(GLit sid, Vec<char>& out_name, uind index = 0) const;
        // -- Fast method for getting names: will clear 'out_name' then push the name plus a
        // terminating zero onto the vector 'out_name'. Also returns a pointer to the first
        // character of 'out_name'.

    String get(GLit sid, uind index = 0) const;
        // -- Slow. Use only for debugging...

  //________________________________________
  //  Reverse lookup

    bool hasLookup() const { return lookup_enabled; }
    void enableLookup();
    void disableLookup();
        // -- enable or disabled reverse lookup (disabled by default: saves lots of memory)

    GLit lookup(cchar* name) const;
    GLit lookup(Str    name) const;
        // -- Returns 'glit_NULL' if name does not exist. If 'name[0] == invert_prefix', then
        // that character is consumed and the result negated.
};


//=================================================================================================
// -- Inlines:


inline void NameStore::add(GLit sid, Str name)
{
    tmp.setSize(name.size() + 1);
    for (uind i = 0; i < name.size(); i++)
        tmp[i] = name[i];
    tmp[name.size()] = 0;
    add(sid, tmp.base());
}


inline GLit NameStore::lookup(Str name) const
{
    tmp.setSize(name.size() + 1);
    for (uind i = 0; i < name.size(); i++)
        tmp[i] = name[i];
    tmp[name.size()] = 0;
    return lookup(tmp.base());
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
