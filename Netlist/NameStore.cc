//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : NameStore.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Maps gate IDs to strings (and back if look-up is enabled).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "NameStore.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// TEMPORARY (will be part of Prelude soon):        <<==


static
void format_cstr(cchar* x, Vec<char>& out)
{
    while (*x) out.push(*x++);
}


static
void format_int64(int64 x, Vec<char>& out)
{
    if (x == INT64_MIN)
        format_cstr("-9223372036854775808", out);

    else if (x == 0)
        out.push('0');

    else{
        if (x < 0){
            out.push('-');
            x = -x; }

        uind i = out.size();
        while (x != 0){
            out.push('0' + uchar(x % 10));
            x /= 10; }

        reverse(slice(out[i], out.end_()));
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


NameStore::NameStore(bool enable_lookup)
{
    lookup_enabled = enable_lookup;
    anonymous_prefix = '@';
    invert_prefix    = '~';
}


NameStore::~NameStore()
{
    disableLookup();
    for (gate_id i = 0; i < id2names.size(); i++)
        clear(GLit(i));
}


void NameStore::moveTo(NameStore& dst)
{
    dst.clear();
    mem.moveTo(dst.mem);
    id2names.moveTo(dst.id2names);
    name2sid.moveTo(dst.name2sid);
    dst.lookup_enabled = lookup_enabled;
    clear();
}


void NameStore::add(GLit sid, cchar* name)
{
    if (name[0] == invert_prefix){
        name++;
        sid = ~sid; }

    Names& ns = id2names(sid.id, Names());
    size_t len = strlen(name);
    char* dup = mem.alloc(len + 1);
    memcpy(dup, name, len + 1);
    if (sid.sign)
        dup = (char*)(uintp(dup) | uintp(1));

    if (ns.name == 0)
        ns.name = (cchar*)(uintp(dup) | uintp(2));
    else if (tag(ns.name) & 2){
        cchar* name0 = (cchar*)(uintp(ns.name) & ~uintp(2));
        ns.names = (Vec<cchar*>*)mem.alloc(sizeof(Vec<cchar*>));
        new (ns.names) Vec<cchar*>();
        ns.names->push(name0);
        ns.names->push(dup);
        ns.name = (cchar*)(uintp(ns.name) | uintp(1));
    }else
        strip(ns.names)->push(dup);

    if (lookup_enabled){
        GLit* v;
        if (name2sid.get(strip(dup), v))
            throw Excp_NameClash(String(name));
        *v = sid;
    }
}


void NameStore::invertSid(cchar* name)
{
    if (lookup_enabled){
        GLit* v;
        if (!name2sid.get(name, v)) assert(false);  // -- name is missing but inverse lookup is turned on
        v->sign ^= 1;
    }
}


void NameStore::invert(GLit sid)
{
    if (sid.id >= id2names.size()) return;
    Names& ns = id2names[sid.id];

    if (ns.name == 0)
        return;
    else if (tag(ns.name) & 2){
        ns.name = (cchar*)(uintp(ns.name) ^ uintp(1));
        invertSid(strip(ns.name));
    }else{
        Vec<cchar*>& names = *strip(ns.names);
        for (uind i = 0; i < names.size(); i++){
            names[i] = (cchar*)(uintp(names[i]) ^ uintp(1));
            invertSid(strip(names[i]));
        }
    }
}


void NameStore::clear(GLit sid)
{
    if (sid.id >= id2names.size()) return;
    Names ns = id2names[sid.id];
    uind  ns_size = getSize(ns);
    for (uind j = 0; j < ns_size; j++){
        cchar* name;
        bool   sign;
        getName(ns, j, name, sign);
        if (lookup_enabled){ bool found = name2sid.exclude(name); assert(found); }
        mem.free((char*)name, strlen(name) + 1);
    }
    if ((tag(ns.name) & 3) == 1)
        mem.free((char*)strip(ns.names), 1);

    id2names[sid.id].name = 0;
}


void NameStore::anonymousName(GLit sid, Vec<char>& out_name) const
{
    out_name.clear();
    if (sid.sign)
        out_name.push(invert_prefix);
    out_name.push(anonymous_prefix);
    format_int64(int64(sid.id), out_name);  // <<== fix!
    out_name.push(0);
}


char* NameStore::get(GLit sid, Vec<char>& out_name, uind index) const
{
    if (sid.id >= id2names.size()) {
        anonymousName(sid, out_name);
        return out_name.base(); }

    Names ns = id2names[sid.id];
    if (index >= getSize(ns)){
        anonymousName(sid, out_name);
        return out_name.base(); }

    cchar* name;
    bool   sign;
    getName(ns, index, name, sign);

    out_name.clear();
    if (sign ^ sid.sign)
        out_name.push(invert_prefix);
    while (*name)
        out_name.push(*name++);
    out_name.push(0);

    return out_name.base();
}


String NameStore::get(GLit sid, uind index) const
{
    Vec<char> buf;
    get(sid, buf, index);
    return String(buf.base());
}


void NameStore::enableLookup()
{
    if (!lookup_enabled){
        lookup_enabled = true;
        for (gate_id i = 0; i < id2names.size(); i++){
            Names ns = id2names[i];
            uind  ns_size = getSize(ns);
            for (uind j = 0; j < ns_size; j++){
                cchar* name;
                bool   sign;
                getName(ns, j, name, sign);

                GLit* v;
                if (name2sid.get(name, v))
                    throw Excp_NameClash(String(name));
                *v = GLit(i, sign);
            }
        }
    }
}


void NameStore::disableLookup()
{
    if (lookup_enabled){
        lookup_enabled = false;
        name2sid.clear();
    }
}


GLit NameStore::lookup(cchar* name) const
{
    GLit v;
    if (name[0] == invert_prefix){
        if (name2sid.peek(name + 1, v))
            return ~v;
    }else{
        if (name2sid.peek(name, v))
            return v;
    }
    return glit_NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
