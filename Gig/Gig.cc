//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Gig.cc
//| Author(s)   : Niklas Een
//| Module      : Gig
//| Description : Second version of the generic netlist.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Rules of a Gig:
//| 
//|   - All cycles in the graph must contain (be broken by) a gate of type 'Seq', typically placed
//|     on the input side of a 'FF' (flip-flop). For that reason, 'Seq' counts as a 'CO' 
//|     (combinational output) and 'FF' as a 'CI' (combinational input). 
//|   - A 'Seq' can not feed itself. Self-loops cause problems.
//|   - It is illegal to delete a gate who is currently in the fanin of another gate.
//|   - The Gig starts in "free form" mode, but can be restricted to AIG mode or other modes.
//|   - In strashed mode, affected gates must be created using factory functions in 'Strash.hh'.
//|   - Only listeners who are also Gig objects may be present when copying a Gig.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "StdLib.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Enum names:


cchar* GigMsgIdx_name[GigMsgIdx_size] = {
    "Update",
    "Add",
    "Remove",
    "Compact",
    "Subst",
};


cchar* GigMode_name[GigMode_size] = {
    "FreeForm",
    "Aig",
    "Xig",
    "Npn4",
    "Lut4",
    "Lut6",
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gig -- implementation:


// Returns TRUE if netlist is in pristine state (i.e. was just created or just cleared)
bool Gig::isEmpty() const
{
    if (size() != gid_FirstUser)
        return false;

    for (uint i = 0; i < GigObjType_size; i++)
        if (objs[i])
            return false;

    for (uint i = 0; i < GigMsgIdx_size; i++)
        if (listeners[i].size() > 0)
            return false;

    return true;
}


void Gig::clear(bool reinit)
{
    // Free fanin memory:
    for (uint id = 0; id < size(); id++){

        Gate& g = getGate(*this, id);
        if (g.is_ext)
            mem.free(g.ext, g.size);
    }
    mem.clear(reinit);

  #if defined(ZZ_GIG_PAGED)
    // Free pages:
    for (uint i = 0; i < pages.size(); i++)
        xfree(pages[i]);
  #else
    gates.clear(true);
  #endif

    // Free vectors:
    for (uint i = 0; i < numbers.size(); i++)
        numbers.clear(true);
    numbers.clear(true);
    type_list.clear(true);
    type_count.clear(true);
    freelist.clear();

    // Dispose objects:
    if (objs){
        for (uint i = 0; i < GigObjType_size; i++)
            if (objs[i])
                delete objs[i];
        xfree(objs);
        objs = NULL;
    }

    // Dispose listeners:
    for (uint i = 0; i < GigMsgIdx_size; i++)
        listeners[i].clear(true);

    // Zero members:
    size_ = 0;
    use_freelist = false;
    frozen = false;
    mode_ = gig_FreeForm;
    mode_mask = ~0ull;

    // Initialize netlist:
    if (reinit){
        objs = xmalloc<GigObj*>(GigObjType_size);
        for (uint i = 0; i < GigObjType_size; i++)
            objs[i] = NULL;

        numbers   .growTo(GateType_size);
        type_list .growTo(GateType_size);
        type_count.growTo(GateType_size, 0);

        addInternal(gate_NULL, 0, 0);  // -- Wire_NULL
        addInternal(gate_NULL, 0, 0);  // -- Wire_ERROR

        add(gate_Const, l_Undef.value);
        add(gate_Const, l_Error.value);
        add(gate_Const, l_False.value);
        add(gate_Const, l_True .value);
        add(gate_Reset);
        add(gate_NULL);         // -- reserved slot (want first user gate to be 8, nice and round)
    }

    mode_mask = ~3ull;          // -- disallow construction of NULL gates and constant gates
    strash_mask = mode_mask;    // -- in normal mode, these masks must be identical
}


gate_id Gig::addInternal(GateType type, uint sz, uint attr, bool strash_normalized)
{
    if (hasObj(gigobj_Strash) && !strash_normalized){
        if (!((1ull << type) & strash_mask)){
            ShoutLn "INTERNAL ERROR! Trying to bypass strashing for gate of type: %_", type;
            assert(false); }
        // -- make sure gates controlled by strashing is not created directly by 'add()'
    }else{
        if (!((1ull << type) & mode_mask)){
            ShoutLn "INTERNAL ERROR! Trying to create illegal gate in mode '%_': %_", mode_, type;
            assert(false); }
    }

    // Determine gate id:
    gate_id id;
    if (use_freelist && freelist.size() > 0){
        id = freelist.popC();
        type_count[gate_NULL]--;
    }else{
        id = size_++;
      #if defined(ZZ_GIG_PAGED)
        if ((id & (ZZ_GIG_PAGE_SIZE - 1)) == 0)    // -- alloc new page
            pages.push(xmalloc<Gate>(ZZ_GIG_PAGE_SIZE));
      #else
        gates.push();
      #endif
    }

    // Initialize gate:
    Gate& g = getGate(*this, id);
    assert((uint)type <= Gate::MAX_TYPE);
    assert(sz <= Gate::MAX_SIZE);

    g.type = type;
    g.size = sz;
    g.inl[0] = 0;       // }- not necessary, but still nice to keep (potentially) unused memory initialized
    g.inl[1] = 0;       // }
    g.inl[2] = attr;    // -- attribute is stored here; may be overwritten below by third input (if ternary gate without attribute)

    uint* inputs;
    if (sz + (uint)(gatetype_attr[type] != attr_NULL) > 3){
        // External inputs:
        g.is_ext = true;
        g.ext = mem.alloc(sz);
        inputs = g.ext;
    }else{
        // Inlined inputs:
        g.is_ext = false;
        inputs = g.inl;
    }

    for (uint i = 0; i < sz; i++)
        inputs[i] = 0;      // -- here we are assuming that 'GLit_NULL' has the underlying representation of '0'

    // Bookkeeping:
    if (gatetype_attr[type] == attr_Enum)
        type_list[type](attr, gid_NULL) = id;
    type_count[type]++;

    // Side-tables (gate specific):
    if (isNumbered(type)){
        if (type == gate_Lut6)
            lut6_ftb.growTo(attr + 1, 0ull);
    }

    // Listeners
    Vec<GigLis*>& lis = listeners[msgidx_Add];
    for (uint i = 0; i < lis.size(); i++)
        lis[i]->adding(Wire(this, GLit(id)));

    return id;
}


// Used to create gates in 'load()' method.
void Gig::loadGate(GateType type, uint sz)
{
    // Determine gate id:
    gate_id id;
    id = size_++;
  #if defined(ZZ_GIG_PAGED)
    if ((id & (ZZ_GIG_PAGE_SIZE - 1)) == 0)    // -- alloc new page
        pages.push(xmalloc<Gate>(ZZ_GIG_PAGE_SIZE));
  #else
    gates.push();
  #endif

    // If NULL gate, add to free list:
    if (type == gate_NULL && use_freelist)
        freelist.push(id);

    // Initialize gate:
    Gate& g = getGate(*this, id);
    assert((uint)type <= Gate::MAX_TYPE);
    assert(sz <= Gate::MAX_SIZE);

    g.type = type;
    g.size = sz;
    g.inl[0] = 0;
    g.inl[1] = 0;
    g.inl[2] = 0;

    uint* inputs;
    if (sz + (uint)(gatetype_attr[type] != attr_NULL) > 3){
        // External inputs:
        g.is_ext = true;
        g.ext = mem.alloc(sz);
        inputs = g.ext;
    }else{
        // Inlined inputs:
        g.is_ext = false;
        inputs = g.inl;
    }

    for (uint i = 0; i < sz; i++)
        inputs[i] = 0;      // -- here we are assuming that 'GLit_NULL' has the underlying representation of '0'
}


void Gig::remove(gate_id id, bool recreated)
{
    Vec<GigLis*>& lis = listeners[msgidx_Remove];
    for (uint i = 0; i < lis.size(); i++)
        lis[i]->removing(Wire(this, GLit(id)), recreated);

    Gate& g = getGate(*this, id);
    GateType type = (GateType)g.type;

    if (g.is_ext)
        mem.free(g.ext, g.size);

    if (use_freelist)
        freelist.push(id);

    if (isNumbered(type)){
        uint num = g.inl[2];
        numbers[type].free(num);
        if (gatetype_attr[type] == attr_Enum)
            type_list[type][num] = gid_NULL;

        // Side-tables (gate specific):
        if (type == gate_Lut6)  // -- clear FTB
            lut6_ftb[num] = 0;
    }

    type_count[type]--;
    type_count[gate_NULL]++;

    memset(&g, 0, sizeof(Gate));    // -- will implicitly set 'type' to' 'gate_NULL'
}


void Gig::listen(GigLis& lis, uint msg_mask)
{
    if (msg_mask == 0) return;

    for (uint i = 0; i < GigMsgIdx_size; i++){
        if (msg_mask & (1u << i)){
            msg_mask &= ~(1u << i);

            listeners[i].push(&lis);

            if (msg_mask == 0) break;
        }
    }
}


void Gig::unlisten(GigLis& lis, uint msg_mask)
{
    if (msg_mask == 0) return;

    for (uint i = 0; i < GigMsgIdx_size; i++){
        if (msg_mask & (1u << i)){
            msg_mask &= ~(1u << i);

            revPullOut(listeners[i], &lis);     // -- if fail, you tried to remove a listener not registered for this event

            if (msg_mask == 0) break;
        }
    }
}


void Gig::clearNumbering(GateType type)
{
    assert(typeCount(type) == 0);
    assert(isNumbered(type));

    numbers[type].clear(true);
    type_list[type].clear(true);
}


//=================================================================================================
// -- Mode control:


// Verify invariants of current mode. Throw assertion if fails.
void Gig::assertMode() const
{
    For_Gates(*this, w){
        bool in_mode = (1ull << w.type()) & mode_mask;
        if (!in_mode){
            ShoutLn "INTERNAL ERROR! Disallowed gate type in mode '%_': %_", mode_, w.type();
            assert(false);
        }
    }

    if (mode_ == gig_Aig || mode_ == gig_Xig || mode_ == gig_Npn4){
        For_Gates(*this, w){
            for (uint i = 0; i < w.size(); i++){    // -- Buggy GCC 4.4.3 doesn't like 'For_Inputs()' here (generates spurious warning)
                Wire v = w[i];
                if (+v == GLit_False){
                    ShoutLn "INTERNAL ERROR! Constant 'False' may not be used in mode '%_': %_[%_] = %_", mode_, w, i, v;
                    assert(false); }
            }
        }
    }

    if (mode_ == gig_Lut4){
        // In LUT mode, inputs must not be negated (for LUTs):
        For_Gates(*this, w){
            if (w.type() != gate_Lut4) continue;
            For_Inputs(w, v){
                if (v.sign){
                    ShoutLn "INTERNAL ERROR! 'Lut4's are not allowed to have inverted inputs: %_[%_] = %_", w, Input_Pin(v), v;
                    assert(false); }
            }
        }
    }

    if (mode_ == gig_Lut6){
        // In LUT mode, inputs must not be negated (for LUTs):
        For_Gates(*this, w){
            if (w.type() != gate_Lut6) continue;
            For_Inputs(w, v){
                if (v.sign){
                    ShoutLn "INTERNAL ERROR! 'Lut6's are not allowed to have inverted inputs: %_[%_] = %_", w, Input_Pin(v), v;
                    assert(false); }
            }
        }
    }
}


void Gig::setMode(GigMode m)
{
    static uint64 mode_masks[GigMode_size] = { 0ull,  0ull,  0ull,  0ull,  0ull };

    if (m != mode_){
        Bury_Gob(*this, Strash);

        mode_ = m;
        if (mode_masks[m] == 0){
            switch (m){
            case gig_FreeForm:
                mode_mask = ~0ull;
                break;

            case gig_Aig:
                mode_mask = 0ull;
                for (uint i = 0; i < GateType_size; i++)
                    if (aigGate(GateType(i)))
                        mode_mask |= 1ull << i;
                break;

            case gig_Xig:
                mode_mask = 0ull;
                for (uint i = 0; i < GateType_size; i++)
                    if (xigGate(GateType(i)))
                        mode_mask |= 1ull << i;
                break;

            case gig_Npn4:
                mode_mask = 0ull;
                for (uint i = 0; i < GateType_size; i++)
                    if (npn4Gate(GateType(i)))
                        mode_mask |= 1ull << i;
                break;

            case gig_Lut4:
                mode_mask = 0ull;
                for (uint i = 0; i < GateType_size; i++)
                    if (lut4Gate(GateType(i)))
                        mode_mask |= 1ull << i;
                break;

            case gig_Lut6:
                mode_mask = 0ull;
                for (uint i = 0; i < GateType_size; i++)
                    if (lut6Gate(GateType(i)))
                        mode_mask |= 1ull << i;
                break;

            default: assert(false);}

            mode_mask &= ~3ull;
            mode_masks[m] = mode_mask;      // -- cache result

        }else
            mode_mask = mode_masks[m];
    }

  #if defined(ZZ_DEBUG)
    assertMode();
  #endif
}


//=================================================================================================
// -- Copying / Moving:


void Gig::moveTo(Gig& M)
{
    M.clear(false);

    // Migrate state:
    mem.moveTo(M.mem, false);
    mov(frozen      , M.frozen);
    mov(mode_       , M.mode_);
    mov(mode_mask   , M.mode_mask);
  #if defined(ZZ_GIG_PAGED)
    mov(pages       , M.pages);
  #else
    mov(gates       , M.gates);
  #endif
    mov(numbers     , M.numbers);
    mov(type_list   , M.type_list);
    mov(type_count  , M.type_count);
    mov(size_       , M.size_);
    mov(use_freelist, M.use_freelist);

    mov(lut6_ftb    , M.lut6_ftb);

    mov(objs        , M.objs);
    for (uint i = 0; i < GigObjType_size; i++)
        if (M.objs[i])
            M.objs[i]->N = &M;
    for (uint i = 0; i < GigMsgIdx_size; i++)
        mov(listeners[i], M.listeners[i]);

    // Patch 'N' (netlist) field in objects to refer to the new netlist:
    for (uint i = 0; i < GigObjType_size; i++){
        if (M.objs[i])
            M.objs[i]->N = &M;
    }

    // Zero this class:
    new (this) Gig();
}


void Gig::copyTo(Gig& M) const
{
    M.clear(false);

    // Copy state:
    cpy(frozen   , M.frozen);
    cpy(mode_    , M.mode_);
    cpy(mode_mask, M.mode_mask);

  #if defined(ZZ_GIG_PAGED)
    M.pages.growTo(pages.size());
    for (uint i = 0; i < pages.size(); i++){
        M.pages[i] = xmalloc<Gate>(ZZ_GIG_PAGE_SIZE);
        memcpy(M.pages[i], pages[i], sizeof(Gate) * ZZ_GIG_PAGE_SIZE);
    }
  #else
    cpy(gates, M.gates);
  #endif

    cpy(numbers     , M.numbers);
    cpy(type_list   , M.type_list);
    cpy(type_count  , M.type_count);
    cpy(size_       , M.size_);
    cpy(use_freelist, M.use_freelist);

    // Side-tables:
    cpy(lut6_ftb, M.lut6_ftb);

    // Copy Gig objects:
    M.objs = xmalloc<GigObj*>(GigObjType_size);
    for (uint i = 0; i < GigObjType_size; i++){
        if (objs[i]){
            gigobj_factory_funcs[i](M, M.objs[i], false);
            objs[i]->copyTo(*M.objs[i]);
        }else
            M.objs[i] = NULL;
    }

    // Make sure that listeners were copied by their respect Gig objects (no other listeners are allowed!):
    for (uint i = 0; i < GigMsgIdx_size; i++)
        assert(listeners[i].size() == M.listeners[i].size());
}


//=================================================================================================
// -- Compaction:


void Gig::compact(GigRemap& remap, bool remove_unreach, bool set_canonical)
{
    assert(!isFrozen());
    Gig& N = *this;

    // Initialize 'remap':
    Vec<GLit>& rmap = remap.new_lit;
    rmap.setSize(N.size());
    for (gate_id i = 0; i < gid_FirstUser; i++)
        rmap[i] = GLit(i);
    for (gate_id i = gid_FirstUser; i < N.size(); i++)
        rmap[i] = GLit_NULL;

    // Get topological order:
    Vec<GLit> order;
    if (remove_unreach)
        removeUnreach(N, NULL, &order);
    else
        upOrder(N, order);

    // Fill in 'remap':
    for (gate_id i = 0; i < order.size(); i++){
        gate_id j = order[i].id; assert_debug(j >= gid_FirstUser);
        rmap[j] = ~GLit(gid_FirstUser + i);     // -- a negation marks the node as "not moved to its right place yet"
    }

    Gate tmp;
    for (gate_id i = 0; i < rmap.size(); i++){
        if (rmap[i].sign){
            rmap[i] = +rmap[i];
            tmp = getGate(N, i);
            uint j = i;
            for(;;){
                assert(!rmap[j].sign);
                j = rmap[j].id;
                swp(tmp, getGate(N, j));
                if (rmap[j].sign)
                    rmap[j] = +rmap[j];
                else
                    break;
            }
        }
    }

    // Shrink gate table:
    uint new_size = gid_FirstUser + order.size();
    type_count[gate_NULL] -= size_ - new_size;
    assert_debug(type_count[gate_NULL] == 3);     // -- right now we have three NULL objects (NULL/ERROR/Reserved); may change...

  #if defined(ZZ_GIG_PAGED)
    size_ = new_size;
    uint n_pages = (size_ + ZZ_GIG_PAGE_SIZE - 1) / ZZ_GIG_PAGE_SIZE;
    while (pages.size() > n_pages)
        xfree(pages.popC());
  #else
    size_ = new_size;
    gates.shrinkTo(size_);
    gates.trim();
  #endif

    // Empty free list:
    freelist.clear(true);

    // Update fanins:
    For_Gates(N, w)
        For_Inputs(w, v)
            w.set_unchecked(Input_Pin(v), remap(v));

    // Update type lists:
    for (uint i = 0; i < type_list.size(); i++)
        remap.applyTo(type_list[i]);

    // Tell netlist objects and listeners:
    for (uint i = 0; i < GigObjType_size; i++)
        if (objs[i])
            objs[i]->compact(remap);

    for (uint j = 0; j < listeners[msgidx_Compact].size(); j++)
        listeners[msgidx_Compact][j]->compacting(remap);

    // Finish up:
    if (set_canonical)
        frozen = 2;     // -- now in canonical mode
}


void Gig::compact(bool remove_unreach, bool set_canonical) {
    GigRemap remap;
    compact(remap, remove_unreach, set_canonical); }


//=================================================================================================
// -- Loading / Saving:


static const uchar gig_file_tag[] = {0xAC, 0x1D, 0x0FF, 0x1C, 0xEC, 0x0FF, 0xEE, 0x61, 0x60 };
static const uchar gig_format_version = 2;


void Gig::flushRle(Out& out, uchar type, uint count, uint end)
{
    assert(count != 0);
    assert(type < 64);

    /**/Dump(GateType(type), count, end);
    if (count <= 3)
        putb(out, type | (count << 6));
    else{
        putb(out, type);
        putu(out, count);
    }

    if (gatetype_size[type] == DYNAMIC_GATE_SIZE){
        for (uint i = end - count; i < end; i++)
            putu(out, operator[](i).size());
    }
}


// Saving and reloading a netlist will NOT put the netlist in the exact same state. In particular:
//
//   - Order of elemens in freelists may change.
//   - Whether freelists are turned on or off is not recorded (turned off for loaded netlists).
//   - Freelist of ID repository for numbered gates may change.
//   - Listeners are completely ignored.
//
void Gig::save(Out& out)
{
    // Write header:
    for (uint i = 0; i < elemsof(gig_file_tag); i++)
        putc(out, gig_file_tag[i]);
    putc(out, gig_format_version);

    // Write state:
    putu(out, frozen);
    putu(out, (uint)mode_);
    putu(out, mode_mask);
    putu(out, strash_mask);
    putb(out, use_freelist);

    // Establish mapping between "gate-type name" and "enum value":
    putu(out, GateType_size);
    for (uint i = 0; i < GateType_size; i++){
        putz(out, GateType_name[i]);
        putu(out, gatetype_size[i]);        // }- Store this data for validation
        putu(out, (uint)gatetype_attr[i]);  // }
    }

    // Write gate types in RLE:
    putu(out, size());
    if (size() > gid_FirstUser){
        uchar t0 = (uchar)(*this)[gid_FirstUser].type();
        uint  tC = 1;
        for (gate_id i = gid_FirstUser+1; i < size(); i++){
            uchar t = (uchar)(*this)[i].type();
            if (t == t0)
                tC++;
            else{
                // Use upper two bits of type to store: many, 1, 2, 3
                flushRle(out, t0, tC, i);
                tC = 1;
                t0 = t;
            }
        }
        flushRle(out, t0, tC, size());
    }

    // Write gate fanins:
    for (gate_id i = gid_FirstUser; i < size(); i++){
        Wire  w = (*this)[i];
        uchar t = (uint)w.type();

        if (t != gate_NULL){
            Array<const GLit> inputs = w.fanins();

#if 1   /*DEBUG*/
            for (uint j = 0; j < inputs.size(); j++)
                WriteLn "puti %_ : %_", (int64)(2*i) - (int64)inputs[j].data(), inputs[j].data();

            if (gatetype_attr[t] != attr_NULL)
                WriteLn "putu %_", w.gate().inl[2];
#endif  /*END DEBUG*/

            for (uint j = 0; j < inputs.size(); j++)
                puti(out, int64(2*i) - (int64)inputs[j].data());

            if (gatetype_attr[t] != attr_NULL)
                putu(out, w.gate().inl[2]);     // -- low-level access to attribute
        }
    }

    // Write side tables:
    putu(out, lut6_ftb.size());
    for (uint i = 0; i < lut6_ftb.size(); i++)
        putu(out, lut6_ftb[i]);

    // Write Gig objects:
    for (uint i = 0; i < GigObjType_size; i++){
        if (objs[i]){
            putz(out, GigObjType_name[i]);
            objs[i]->save(out);
        }
    }
    putz(out, ".");     // -- marks end of objects
}


// Throws a 'Excp_Msg' on parse error.
void Gig::load(In& in)
{
    assert(isEmpty());

    // Read header:
    for (uint i = 0; i < elemsof(gig_file_tag); i++)
        if (getb(in) != gig_file_tag[i])
            Throw(Excp_Msg) "Not a .gnl file.";

    uchar version = getc(in);
    if (version != gig_format_version)
        Throw(Excp_Msg) "Unsupported version of format: %_ (expected %_)", version, gig_format_version;

    // Read state:
    frozen = getu(in);
    mode_ = (GigMode)getu(in);
    mode_mask = getu(in);
    strash_mask = getu(in);
    use_freelist = getb(in);

    // Establish mapping between "gate-type name" and "enum value":
    Vec<uint> type_map;
    uint n_types = getu(in);
    Vec<char> buf;
    for (uint i = 0; i < n_types; i++){
        getz(in, buf);
        uint size = getu(in);
        GateAttrType attr = (GateAttrType)getu(in);

        uint j;
        for (j = 0; j < GateType_size; j++)
            if (eq(GateType_name[j], buf))
                break;

        if (j == GateType_size){
            if (!getenv("ZZ_GIG_IGNORE_UNKNOWN_TYPES"))
                Throw (Excp_Msg) "Unknown gate type: %_\n(set environment variable ZZ_GIG_IGNORE_UNKNOWN_TYPES to ignore)", buf;
            j = UINT_MAX;

        }else{
            if (gatetype_size[j] != size) Throw (Excp_Msg) "Size has changed for gate: %_", buf;
            if (gatetype_attr[j] != attr) Throw (Excp_Msg) "Attribute has changed for gate: %_", buf;
        }
        type_map(i, UINT_MAX) = j;
    }

    // Read gate types and create gates:
    uint n_gates = getu(in);
    while (size() != n_gates){
        uchar d = getb(in);
        GateType type = GateType(d & 63);
        uint n = d >> 6;
        if (n == 0)
            n = getu(in);
        /**/Dump(type, n);

        if (gatetype_size[type] == DYNAMIC_GATE_SIZE){
            for (uint i = 0; i < n; i++)
                loadGate(type, getu(in));
        }else{
            for (uint i = 0; i < n; i++)
                loadGate(type, gatetype_size[type]);
        }
    }

    // Read and connect gate fanins:
    for (gate_id i = gid_FirstUser; i < size(); i++){
        Wire  w = (*this)[i];
        uchar t = (uint)w.type();

        if (t != gate_NULL){
            Array<GLit> inputs = w.fanins();

            for (uint j = 0; j < inputs.size(); j++){
                /**/int64 v = geti(in);
                uint data = uint(int64(2*i) - v);
                /**/WriteLn "geti %_ : %_", v, data;
                inputs[j] = GLit(packed_, data);
            }

            if (gatetype_attr[t] != attr_NULL){
                /**/uint v = getu(in);
                w.gate().inl[2] = v;     // -- low-level access to attribute
                /**/WriteLn "getu %_", v;

                if (gatetype_attr[t] == attr_Enum)
                    type_list[t](v, gid_NULL) = i;
            }
        }
    }

    // Read side tables:
    lut6_ftb.setSize(getu(in));
    for (uint i = 0; i < lut6_ftb.size(); i++)
        lut6_ftb[i] = getu(in);

    // Read Gig objects:
    for(;;){
        getz(in, buf);
        if (eq(buf, ".")) break;

        uint j;
        for (j = 0; j < GigObjType_size; j++)
            if (eq(GigObjType_name[j], buf))
                break;

        if (j == GigObjType_size)
            Throw(Excp_Msg) "Unknown Gig object: %_", buf;

        gigobj_factory_funcs[j](*this, objs[j], false);
        objs[j]->load(in);
    }

#if 1   /*DEBUG*/
    WriteLn "%_", info(*this);
    For_Gates(*this, w){
        WriteLn "%_:", w;
        For_Inputs(w, v)
            WriteLn "  %_", v;
        NewLine;
    }
#endif  /*END DEBUG*/

#if 0
#endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
