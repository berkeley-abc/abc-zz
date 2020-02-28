//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Netlist.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : A netlist is a graph of multi-input, single output gates ("gate-inverter-graph").
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Netlist.hh"
#include "ZZ/Generics/Set.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
/* TODO:


- Copying of gate attributes? Already supported? Document interface better in 'GateDefs.hh'
- Free lists!
- Fanouts
- Loading/saving
- Compaction
- Actually define more gate types...
- Type asserting/checking functions for netlist state


*/
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global variables:


Netlist_data* global_netlists_;
uint          global_netlists_sz_;
netlist_id    global_netlists_first_free_;
bool          global_netlists_frozen_;      // -- if set, it is illegal for the 'global_netlists_' vector to grow.

ZZ_Initializer(global_netlists, -9050){
    global_netlists_ = NULL;
    global_netlists_sz_ = 0;
    global_netlists_first_free_ = nid_NULL;
    global_netlists_frozen_ = false;
}

ZZ_Finalizer(global_netlists, -9050){
    for (netlist_id i = 0; i < global_netlists_sz_; i++){
        if (global_netlists_[i].nl_ != nid_NULL)
            dispose(NetlistRef(i));
        global_netlists_[i].~Netlist_data();
    }
    xfree(global_netlists_);
    global_netlists_ = NULL;
    global_netlists_sz_ = 0;
    global_netlists_first_free_ = nid_NULL;
    global_netlists_frozen_ = false;
}

#include "Netlist_GateTypes.icc"


//=================================================================================================
// -- Pecs:


PecNull  pob_NULL;

PecInfo* registered_pecs   = NULL;
uind     n_registered_pecs = GateType_size;

PobInfo* registered_pobs   = NULL;
uind     n_registered_pobs = GateType_size;

static Set<cchar*> pob_names;   // -- contains a copy of every POB name ever used (shouldn't be too many)


#define Register_Pec_GateAttr(gatetype) Register_Pec_(GateAttr< GateAttr_##gatetype >, gate_##gatetype);
Apply_To_All_GateTypes(Register_Pec_GateAttr)
    // -- register all attributes in 'GateDefs.hh'.


macro cchar* dupPobName(cchar* name) {
    if (pob_names.has(name))
        return *pob_names.get(name);
    cchar* ret = xdupStr(name);
    pob_names.add(ret);
    return ret; }


ZZ_Finalizer(registered_pecs, -9000){      // -- dispose Pecs at end of program; unimportant but makes 'valgrind' happy.
    for (PecInfo* p = registered_pecs; p;){
        PecInfo* q = p;
        p = p->next;
        delete q;
    }

    For_Set(pob_names){
        cchar* name = Set_Key(pob_names);
        xfree(name);
    }
}


ZZ_Initializer(registered_pobs, -9000){
    assert(n_registered_pobs == GateType_size);
    for (PobInfo* p = registered_pobs; p; p = p->next){
        p->obj_id     = n_registered_pobs++;
        p->class_info = getPecInfo(p->class_name);
        if (!p->class_info){
            fprintf(stderr, "INTERNAL ERROR! Persistent object '%s' registered with unknown class '%s'.\n", p->obj_name, p->class_name);
            exit(255); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Verify 'GLit' bit layout:


ZZ_Initializer(GLit_check, -9100) {
    GLit_union x;
    x.data = 0;
    x.sid.id = 0x7FFFFFFF;
    if (x.data != 0xFFFFFFFE){
        fprintf(stderr, "INTERNAL ERROR! 'GLit' layout mismatch: %.8X   (sizeof(GLit)=%d)\n", x.data, (int)sizeof(GLit));
        exit(255); }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Name of constants and special gates:


cchar* name_of_NULL     = "-";      // -- this one must be only one character
cchar* name_of_ERROR    = "*";
cchar* name_of_Unbound  = "?";
cchar* name_of_Conflict = "!";
cchar* name_of_False    = "0";
cchar* name_of_True     = "1";

static const uint n_named_special_gates = 6;

macro GLit constNameToGLit(Array<char> name)
{
    if (name.size() != 1) return glit_NULL;
    if (name[0] == '0') return glit_False;
    if (name[0] == '1') return glit_True;
    if (name[0] == '?') return glit_Unbound;
    if (name[0] == '!') return glit_Conflict;
    return glit_NULL;
}


void initNames(NameStore& names)
{
    // Give names to special gates:
    names.add(glit_NULL    , "-");
    names.add(glit_ERROR   , "*");
    names.add(glit_Unbound , "?");
    names.add(glit_Conflict, "!");
    names.add(glit_False   , "0");
    names.add(glit_True    , "1");
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate allocation:


static
void allocBlock(Netlist_data& N, GateType type, serial_t serial0, uint size = 0)
{
    if (size == 0)
        N.gate_data[type].push((GLit*)N.mem.alloc());
    else
        N.gate_data[type].push((GLit*)N.mem.allocBig(size));
    GatesHeader& h = *reinterpret_cast<GatesHeader*>(N.gate_data[type].last());
    h.pos = (sizeof(GatesHeader) + sizeof(GLit) - 1) / sizeof(GLit);
    h.type = type;
    h.serial0 = serial0;
    h.nl = N.nl_;
    h.n_inputs = gate_type_n_inputs[type];
    h.elem_size = (gate_type_n_inputs[type] == DYNAMIC_GATE_INPUTS) ? 0 : h.n_inputs + 1;
    h.reciprocal = (h.n_inputs == DYNAMIC_GATE_INPUTS) ? 0 : (uint64(1) << 32) / (h.elem_size * sizeof(GLit)) + 1;
}


// Declares and initializes: lim, base, h, first_call
#define AllocGateCommonPrefix                                           \
    if (N.gate_data[type].size() == 0)                                  \
        allocBlock(N, type, 0);                                         \
    const uint   lim  = NETLIST_INPUT_DATA_ALIGNMENT / sizeof(GLit);    \
    GLit*        base = N.gate_data[type].last();                       \
    GatesHeader& h    = *reinterpret_cast<GatesHeader*>(base);

#define AllocGateCommonSuffix                   \
    if (recycle_id == gid_NULL){                \
        ret[0] = GLit(N.gates.size());          \
        N.gates.push(ret);                      \
    }else{                                      \
        ret[0] = GLit(recycle_id);              \
        N.gates[recycle_id] = ret;              \
        N.type_count[gate_NULL]--;              \
    }                                           \
    return make_tuple(ret + 1, ret[0].id);


// Allocate a gate in netlist 'N', but don't initialize inputs. Returns a pointer to the
// block for the gate where 'Glit[0..size-1]' stores the inputs.
Pair<GLit*,gate_id> allocGate(Netlist_data& N, GateType type, gate_id recycle_id)
{
    AllocGateCommonPrefix
    serial_t sn = N.serial_count[type]++;
    assert_debug(sn == h.serial0 + ( h.pos - ((sizeof(GatesHeader) + sizeof(GLit) - 1) / sizeof(GLit)) ) / h.elem_size);

    // Need to allocate a new block?
    if (h.pos + h.elem_size >= lim){
        allocBlock(N, type, sn);
        base = N.gate_data[type].last();
    }

    // Chop off memory for this gate from block:
    GatesHeader& h0 = *reinterpret_cast<GatesHeader*>(base);
    GLit* ret = base + h0.pos;
    h0.pos += h0.elem_size;
    N.type_count[type]++;

    AllocGateCommonSuffix
}


Pair<GLit*,gate_id> allocDynGate(Netlist_data& N, GateType type, uint n_inputs, gate_id recycle_id)
{
    AllocGateCommonPrefix
    serial_t sn = N.serial_count[type]++;

    const uint max_inputs = (NETLIST_INPUT_DATA_ALIGNMENT - sizeof(GatesHeader)) / sizeof(GLit) - 3;
    if (n_inputs > max_inputs){
        // Allocate big block and put it second to last in 'gate_data[type]':
        allocBlock(N, type, sn, sizeof(GatesHeader) + (n_inputs + 3) * sizeof(GLit));
        uint sz = N.gate_data[type].size(); assert(sz >= 2);
        base = N.gate_data[type].last();
        swp(N.gate_data[type][sz-2], N.gate_data[type][sz-1]);
    }else{
        // Need to allocate a new block?
        if (h.pos + n_inputs + 3 >= lim){
            allocBlock(N, type, sn);
            base = N.gate_data[type].last();
        }
    }

    // Chop off memory for this gate from block:
    GatesHeader& h0 = *reinterpret_cast<GatesHeader*>(base);
    GLit* ret = base + h0.pos + 1;
    ret[-1] = GLit(packed_, n_inputs);
    ret[n_inputs+1] = GLit(packed_, sn);
    h0.pos += n_inputs + 3;
    N.type_count[type]++;

    AllocGateCommonSuffix
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist -- constructor/destructor:


void reserveNetlists(uint count)
{
    global_netlists_frozen_ = false;
    Vec<NetlistRef> Ns;
    for (uint i = 0; i < count; i++)
        Ns.push(Netlist_new());
    for (uint i = count; i > 0;) i--,
        dispose(Ns[i]);
    global_netlists_frozen_ = true;
}


void unreserveNetlists()
{
    global_netlists_frozen_ = false;
}


Netlist::Netlist()
{
    // Look for freed netlist:
    netlist_id nid = global_netlists_first_free_;
    if (nid != nid_NULL){
        global_netlists_first_free_ = global_netlists_[nid].nl_next_free;
    }else{
        // Allocate netlist data:
        assert(!global_netlists_frozen_);
        nid = global_netlists_sz_;
        if ((nid & (nid-1)) == 0){  // -- reached power of 2, time to grow
            if (nid == 0) global_netlists_ = xmalloc<Netlist_data>(1);
            else          global_netlists_ = xrealloc(global_netlists_, 2 * nid);
        }
        new (&global_netlists_[nid]) Netlist_data();
        global_netlists_sz_++;
    }

    init(nid);
}


void Netlist::init(netlist_id nid)
{
    // Setup netlist IDs:
    this->nl_ = nid;
    Netlist_data& N = global_netlists_[nid];
    N.nl_ = nid;
    N.nl_next_free = nid_NULL;
    N.n_user_pobs = 0;

    // Clear gate counts:
    for (uint i = 0; i < GateType_size; i++){
        N.type_count  [i] = 0;
        N.serial_count[i] = 0; }

    // Add special gates:
    addDeletedGate();
    addDeletedGate();

    // Add gate attributes:
    N.pobs.setSize(n_registered_pobs, &pob_NULL); assert(n_registered_pobs >= GateType_size);
    for (PecInfo* p = registered_pecs; p; p = p->next){
        if (p->class_id < GateType_size && gate_type_has_attr[p->class_id]){
            N.pobs[p->class_id] = (Pec*)xmalloc<char>(p->n_bytes);
            p->init(Pec_base(nl_, p, p->class_id, GateType_name[p->class_id]), N.pobs[p->class_id]);
        }
    }

    // Add constant gates:
    allocGate(N, gate_Const);
    allocGate(N, gate_Const);
    allocGate(N, gate_Const);
    allocGate(N, gate_Const);
    Pec_GateAttr<GateAttr_Const>& const_attr = *static_cast<Pec_GateAttr<GateAttr_Const>*>(N.pobs[gate_Const]);
    const_attr(this->operator[](gid_Unbound )).value = l_Undef;
    const_attr(this->operator[](gid_Conflict)).value = l_Error;
    const_attr(this->operator[](gid_False   )).value = l_False;
    const_attr(this->operator[](gid_True    )).value = l_True;

    initNames(N.names);

    // Setup send message proxy:
    N.send_proxy = new SendProxy(N.nl_, N.listeners);
}


Netlist::~Netlist()
{
    Netlist_data& N = global_netlists_[this->nl_];
    N.nl_next_free = global_netlists_first_free_;   // }- put netlist on free list
    global_netlists_first_free_ = N.nl_;            // }
    N.nl_ = nid_NULL;       // -- indicates that the netlist has been freed
    N.mem.clear();
    N.gates.clear(true);

    for (uint i = 0; i < GateType_size; i++)
        N.gate_data[i].clear(true);

    for (uind i = 0; i < N.pobs.size(); i++){
        if (*N.pobs[i]){
            N.pobs[i]->~Pec();
            xfree(N.pobs[i]);
        }
    }
    N.pobs.clear(true);

    for (uint i = 0; i < NlMsg_size; i++){
        Vec<NlLis*>& ls = N.listeners[i];
        for (uind j = 0; j < ls.size(); j++)
            delete ls[j];
        ls.clear(true);
    }

    N.names.clear();
    if (N.send_proxy){
        delete N.send_proxy;
        N.send_proxy = NULL;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Listeners:


void NetlistRef::listen(NlLis& lis, NlMsgs msg_mask) const
{
    if (msg_mask == 0) return;

    for (uint i = 0;; i++){
        assert_debug(i < NlMsg_size);
        if (msg_mask & (1ull << i)){
            msg_mask &= ~(1ull << i);

            deref().listeners[i].push(&lis);

            if (msg_mask == 0) break;
        }
    }
}


void NetlistRef::unlisten(NlLis& lis, NlMsgs msg_mask) const
{
    if (msg_mask == 0) return;

    for (uint i = 0;; i++){
        assert_debug(i < NlMsg_size);
        if (msg_mask & (1ull << i)){
            msg_mask &= ~(1ull << i);

            revPullOut(deref().listeners[i], &lis);     // -- if fail, you tried to remove a listener not registered for this event

            if (msg_mask == 0) break;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Embedded objects:


Pec& NetlistRef::pob(cchar* obj_name) const
{
    Vec<Pec*>& pobs = deref().pobs;
    for (uind i = 0; i < pobs.size(); i++)
        if (*pobs[i] && strcmp(pobs[i]->obj_name, obj_name) == 0)
            return *pobs[i];
    return pob_NULL;
}


Pec& NetlistRef::addPob(cchar* obj_name, cchar* class_name) const
{
    assert(!pob(obj_name));     // -- object must not already exist
    PecInfo* class_info;
    PobInfo* obj_info = getPobInfo(obj_name);

    Vec<Pec*>& pobs = deref().pobs;
    pob_id obj_id;
    if (!obj_info){
        for (uind i = n_registered_pobs; i < pobs.size(); i++){
            if (!*pobs[i]){
                obj_id = i;
                goto FoundFreeSlot;
            }
        }/*else*/{
            obj_id = pobs.size();
            pobs.push(&pob_NULL);
        }
      FoundFreeSlot:;
        class_info = getPecInfo(class_name);
        assert(class_info);     // -- there must be a class of this name
    }else{
        assert(strcmp(obj_info->class_name, class_name) == 0);     // -- types must match for registered objects
        obj_id = obj_info->obj_id;
        class_info = obj_info->class_info;
    }

    return addPob(obj_name, class_info, obj_id);
}


Pec& NetlistRef::addPob(cchar* obj_name, PecInfo* class_info, pob_id obj_id) const
{
    assert(!*deref().pobs[obj_id]);     // -- object must not already exist

    deref().pobs[obj_id] = (Pec*)xmalloc<char>(class_info->n_bytes);
    class_info->init(Pec_base(nl(), class_info, obj_id, obj_name), deref().pobs[obj_id]);

    if (obj_id >= GateType_size)
        deref().n_user_pobs++;

    return *deref().pobs[obj_id];
}


void NetlistRef::removePob(Pec& pob) const
{
    assert(pob.nl == nl_);
    Vec<Pec*>& pobs = deref().pobs;

    pob_id i = pob.obj_id;
    assert(i >= GateType_size);     // -- cannot delete gate attributes
    assert(pob);                    // -- ...nor can you delete 'pob_NULL'
    pob.~Pec();
    xfree((char*)&pob);

    pobs[i] = &pob_NULL;
    while (pobs.size() > n_registered_pobs && !*pobs.last())
        pobs.pop();

    deref().n_user_pobs--;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist -- Loading/saving:


//=================================================================================================
// -- write:


static
uint64 findFirstFreeAutoName(const NameStore& names)
{
    Vec<char> tmp;
    uint64    first_free = 0;
    for (gate_id i = 0; i < names.size(); i++){
        GLit p(i);
        for (uind j = 0; j < names.size(p); j++){
            names.get(p, tmp, j);
            if (tmp[0] == '_' && tmp[1] == '_'){
                uint64 n = -1;
                for (uint k = 2; tmp[k] != 0; k++){
                    if (tmp[k] < 'a' || tmp[k] > 'z') goto NotAutoName;
                    uint64 old_n = n;
                    n = (n+1) * 26 + tmp[k] - 'a';
                    if (n + 1 < old_n + 1)   // -- we wrapped, ignore this name...
                        goto NotAutoName;
                }
                newMax(first_free, n + 1);
              NotAutoName:;
            }
        }
    }
    return first_free;
}


static
void generateAutoName(uint64 n, Vec<char>& out_name)
{
    out_name.clear();
    n++;
    while (n > 0){
        n--;
        out_name.push(uchar(n % 26) + 'a');
        n /= 26;
    }
    out_name.push('_');
    out_name.push('_');
    reverse(out_name);
    out_name.push(0);
}


// PRE-CONDITION: No gate has a child-pointer to a deleted gate.
// NOTE! Anonymous gates will be given auto-generated names.
void NetlistRef::write(Out& out) const
{
    NameStore& names = deref().names;
    Vec<char>  tmp_name;

    // Give internal names to anonymous gates:
    uint64     first_free = UINTG_MAX;
    for (gate_id i = gid_FirstUser; i < size(); i++){
        GLit p(i);
        if (names.size(p) == 0){
            if (first_free == UINTG_MAX)
                first_free = findFirstFreeAutoName(names);
            generateAutoName(first_free, tmp_name);
            first_free++;

            names.add(p, tmp_name.base());
        }
    }

    // Write gates:
    for (gate_id i = gid_FirstLegal; i < size(); i++){
        if (deleted(i)){
            //out += '-', '\n';
            continue; }

        GLit p(i);
        Wire w(this->nl(), i);

        if (i < gid_FirstUser){
            if (names.size(p) > 1){
                // Write synonymous for constant:
                names.get(p, tmp_name, 1);
                out += tmp_name.base();
                if (names.size(p) > 2){
                    for (uind j = 2; j < names.size(p); j++){
                        out += ',', ' ';
                        names.get(p, tmp_name, j);
                        out += tmp_name.base();
                    }
                }

                out += ' ', '=', ' ';
                names.get(p, tmp_name, 0);
                out += tmp_name.base();
                out += '\n';
            }

        }else{
            // Write names:
            names.get(p, tmp_name, 0);
            out += tmp_name.base();
            if (names.size(p) > 1){
                for (uind j = 1; j < names.size(p); j++){
                    out += ',', ' ';
                    names.get(p, tmp_name, j);
                    out += tmp_name.base();
                }
            }

            out += ' ', '=', ' ';

            // Write type and inputs:
            out += GateType_name[type(w)];

            if (w.size() > 0){
                assert(!this->deleted(id(w[0])) || w[0] == Wire_NULL);       // -- pointer to deleted gate not allowed
                out += '(';
                names.get(w[0], tmp_name, 0);
                out += tmp_name.base();
                for (uint j = 1; j < w.size(); j++){
                    assert(!this->deleted(id(w[j])) || w[j] == Wire_NULL);   // -- pointer to deleted gate not allowed
                    out += ',', ' ';
                    names.get(w[j], tmp_name, 0);
                    out += tmp_name.base();
                }
                out += ')';
            }

            // Write attributes:
            if (gate_type_has_attr[type(w)]){
                if (!deref().pobs[type(w)]->attrIsNull(w)){
                    out += ' ', '[';
                    deref().pobs[type(w)]->writeAttr(w, out);
                    out += ']';
                }
            }

            out += '\n';
        }
    }

    // Are there any POBs?
    Vec<Pec*>& pobs = deref().pobs;
    bool has_pobs = false;
    for (uind i = GateType_size; i < pobs.size(); i++){
        if (*pobs[i]){
            has_pobs = true;
            break; } }

    if (has_pobs){
        // Write POBs:
        out += "\n%%";
        for (uind i = GateType_size; i < pobs.size(); i++){
            if (*pobs[i]){
                out += '\n', '\n';
                out += pobs[i]->obj_name;
                out += ' ', ':', ' ';
                out += pobs[i]->class_info->class_name;
                out += ' ', '{', '\n';

                Out b_out;
                pobs[i]->write(b_out);
                bool last_nl = true;
                for (uind j = 0; j < b_out.vec().size(); j++){
                    char c = b_out.vec()[j];
                    if (c == '\n'){
                        out += '\n';
                        last_nl = true;
                    }else{
                        if (last_nl){
                            out += ' ', ' ';
                            last_nl = false; }

                        if      (c == '{') out += '`', '{';
                        else if (c == '}') out += '`', '}';
                        else if (c == '`') out += '`', '`';
                        else if (c == '#') out += '`', '#';
                        else               out += c;
                    }
                }
                if (!last_nl)
                    out += '\n';
                out += '}', '\n';
            }
        }
    }

    out.flush();
}


//=================================================================================================
// -- read:


static
GateType nameToGatetype(Array<char> str, uind line_no)
{
    for (uint i = 1; i < GateType_size; i++){
        if (vecEqual(slize(GateType_name[i]), str))         // <<== slow! this can be improved a lot!
            return GateType(i);
    }

    throw Excp_NlParseError(String("Unknown gate type: ") + str, line_no);
    return gate_NULL;   // -- Sun wants this...
}


Wire nameToWire(Array<char> name, NetlistRef N, NameStore& names, gate_id& idC, uind line_no)
{
    if (name.size() == 0)
        throw Excp_NlParseError("Empty name.", line_no);

    GLit p = names.lookup(name);
    if (p == glit_NULL){
        p = GLit(idC++);
        names.add(p, name);
    }

    return N[p];
}


static
bool readLine(In& in, Vec<char>& buf, uind& line_no)
{
    buf.clear();

    for(;;){
        if      (in.eof())    { return false; }
        else if (*in == '\n') { in++; line_no++; }
        else if (isWS(*in))   { in++; }
        else if (*in == '#')  { skipEol(in); line_no++; }
        else                  { break; }
    }

    gets(in, buf, IsChar2('#', '\n'));
    if (*in == '#') skipEol(in);
    else            in++;
    line_no++;

    return true;
}


// Parser for the GIG format (quick and dirty, but will do for now)
void NetlistRef::read(In& in) const
{
    this->clear();

    static const Array<cchar> seps(",");
    Vec<char> buf;
    Vec<Array<char> > ns;
    Vec<Array<char> > is;
    uind line_no = 0;
    WMap<Wire> id_map(Wire_NULL); // -- during parsing, names are given IDs as they are parsed, but they are later reshuffled to correspond to the order of declarations in the file
    gate_id    idC = gid_FirstUser;

    id_map(Wire_NULL)  = Wire_NULL;
    id_map(Wire_ERROR) = Wire_ERROR;
    id_map(Unbound())  = Unbound();
    id_map(Conflict()) = Conflict();
    id_map(False())    = False();
    id_map(True())     = True();

    deref().names.enableLookup();

    try {
        //
        // READ GATES:
        //

        for(;;){
            if (!readLine(in, buf, line_no)) break;

            uind eq_i = search(buf, '=');
            if (eq_i == UIND_MAX){
                trim(buf);
                if (buf.size() == 2 && buf[0] == '%' && buf[1] == '%')
                    break;      // -- Pecs will follow
                else
                    throw Excp_NlParseError("Missing '=' sign.", line_no);
            }

            Array<char> lhs = slice(buf[0], buf[eq_i]);
            Array<char> rhs = slice(buf[eq_i + 1], buf.end_());

            // Split left-hand side into names:   (stored in 'ns[]')
            strictSplitArray(lhs, seps, ns);
            for (uind i = 0; i < ns.size(); i++)
                trim(ns[i]);

            // Split right-hand side into type, arguments and attributes:   (stored in 'type, args, attr')
            uind i0 = 0;
            for(;;){
                if (i0 >= rhs.size()) throw Excp_NlParseError("Empty right-hand side.", line_no);
                else if (!isWS(rhs[i0])) break;
                i0++;
            }

            Array<char> type_, args, attr;
            uind i = i0;
            for(;;){
                if (i == rhs.size()){
                    type_ = slice(rhs[i0], rhs.end_());
                    trimEnd(type_);
                    goto Done;
                }else if (rhs[i] == '['){
                    type_ = slice(rhs[i0], rhs[i]);
                    trimEnd(type_);
                    goto ParseAttribute;
                }else if (rhs[i] == '('){
                    type_ = slice(rhs[i0], rhs[i]);
                    trimEnd(type_);
                    goto ParseArguments;
                }else
                    i++;
            }

          ParseArguments:
            assert(rhs[i] == '(');
            i++;
            while (i < rhs.size() && isWS(rhs[i])) i++;
            i0 = i;
            for(;;){
                if (i == rhs.size())
                    throw Excp_NlParseError("Missing ')' in definition.", line_no);
                else if (rhs[i] == ')'){
                    args = slice(rhs[i0], rhs[i]);
                    trimEnd(args);
                    i++;
                    break;
                }else
                    i++;
            }
            while (i < rhs.size() && isWS(rhs[i])) i++;
            if (i == rhs.size()) goto Done;
            if (rhs[i] != '[')
                throw Excp_NlParseError("Extra characters at end of definition.", line_no);

          ParseAttribute:
            assert(rhs[i] == '[');
            i++;
            while (i < rhs.size() && isWS(rhs[i])) i++;
            i0 = i;
            for(;;){
                if (i == rhs.size())
                    throw Excp_NlParseError("Missing ']' in definition.", line_no);
                else if (rhs[i] == ']'){
                    attr = slice(rhs[i0], rhs[i]);
                    trimEnd(attr);
                    i++;
                    break;
                }else
                    i++;
            }
            while (i < rhs.size() && isWS(rhs[i])) i++;
            if (i != rhs.size())
                throw Excp_NlParseError("Extra characters at end of definition.", line_no);

          Done:
            // Split args into input names:   (stored in 'is[]')
            strictSplitArray(args, seps, is);
            for (uind j = 0; j < is.size(); j++)
                trim(is[j]);

            // Extra names for constants?
            GLit p_const = constNameToGLit(type_);
            Wire w;
            if (p_const != glit_NULL)
                // Get existing constant gate:  (this is a hack: constants are already created before reading, so we need some way of attaching new names)
                w = (*this)[p_const];
            else
                // Create gate:
                w = this->add_(nameToGatetype(type_, line_no), is.size());

            // Attach names to gate:
            for (uind j = 0; j < ns.size(); j++){
                Wire v = nameToWire(ns[j], *this, deref().names, idC, line_no);
                if (+id_map[v] != Wire_NULL)
                    throw Excp_NlParseError(String("Name used twice: ") + ns[j], line_no);      // <<== allow at least constants to be redefined to the same thing! (maybe all gates?)
                id_map(v) = w ^ sign(v);
            }

            // Assign inputs:
            for (uind j = 0; j < is.size(); j++){
                if (is[j].size() != 0 && (is[j].size() != 1 || is[j][0] != name_of_NULL[0])){
                    Wire v = nameToWire(is[j], *this, deref().names, idC, line_no);
                    w.set(j, v);
                }
            }

            // Attributes:
            if (attr){
                Pec* attr_pob = deref().pobs[type(w)];
                if (attr_pob == NULL)
                    throw Excp_NlParseError("Gate type has no attribute.", line_no);
                attr_pob->readAttr(w, attr);
            }
        }

        // Translate gate IDs:
        for (gate_id id = gid_FirstUser; id < this->size(); id++){
            Wire w = (*this)[id];
            for (uint j = 0; j < w.size(); j++){
                if (w[j])
                    w.set(j, id_map[w[j]] ^ sign(w[j]));
            }
        }

        // Translate names:
        deref().names.disableLookup();
        NameStore old_names;
        deref().names.moveTo(old_names);

        for (gate_id id = 0; id < old_names.size(); id++){
            Wire w = (*this)[id];
            uind sz = old_names.size(w);
            for (uind i = 0; i < sz; i++){
                old_names.get(w, buf, i);
                deref().names.add(id_map[w], buf.base());
            }
        }

        //
        // READ POBS:
        //

        for(;;){        // -- an object has the text representation:  <name> ':' <type> '{' '\n' <data> '}'
            if (!readLine(in, buf, line_no)) break;

            // Read header:
            Vec<char> name, type_;
            {
                In h_in(buf);
                skipWS(h_in); assert(!in.eof());

                gets(h_in, name, IsChar(':'));
                if (*h_in != ':') throw Excp_NlParseError("Invalid object header; missing ':'.", line_no);
                h_in++;
                trim(name);
                name.push(0);

                gets(h_in, type_, IsChar('{'));
                if (*h_in != '{') throw Excp_NlParseError("Invalid object header; missing '{'.", line_no);      // <<== count instead to allow nested netlist
                h_in++;
                trim(type_);
                type_.push(0);
            }

            // Read body:
            Vec<char> body;
            for(;;){
                if (in.eof()) throw Excp_NlParseError("Missing '}' at end of object.", line_no);
                if (*in == '\n'){
                    body.push(in++);
                    if (*in == ' ') in++;
                    if (*in == ' ') in++;
                }else if (*in == '`'){
                    in++;
                    body.push(in++);
                }else if (*in == '}'){
                    in++;
                    break;
                }else if (*in == '#'){
                    in++;
                    while (!in.eof() && *in != '\n') in++;
                }else
                    body.push(in++);
            }

            // Construct Pec and parse it:
            PecInfo* pt = getPecInfo(type_.base());
            if (!pt) throw Excp_NlParseError(String("Unsupported object type: ") + type_, line_no);

            if (this->pob(name.base())) throw Excp_NlParseError(String("Object already defined: ") + name, line_no);

            Pec& pob = this->addPob(dupPobName(name.base()), (type_.base()));
            In b_in(body);
            deref().names.enableLookup();
            try{
                pob.read(b_in);
                while (!b_in.eof()){    // -- check whole pob text was parsed
                    char chr = b_in++;
                    if (!isWS(chr))
                        throw (String)(FMT "Stray characters in object: %_", chr);
                }
            }catch (...){
                Str text = slice(b_in);
                text.sz = b_in.tell();
                trimEnd(text);
                line_no += countLineNo(text, text.sz);
                throw;
            }
            line_no += countLineNo(slice(body), body.size()) - 1;
        }

    }catch (Excp_NlParseError){
        this->clear();
        throw;

    }catch (String msg){
        this->clear();
        throw Excp_NlParseError(msg, line_no);

    }catch (...){
        this->clear();
        throw Excp_NlParseError("Syntax error.", line_no);
    }

    deref().names.disableLookup();
}




//=================================================================================================
// -- save:


//=================================================================================================
// -- load:


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Compaction:


void NetlistRef::compact(const Vec<gate_id>* order_, NlRemap& out_remap) const
{
    NetlistRef N = *this;

    Vec<gate_id> new_id(N.size(), gid_NULL);
    for (uint id = 0; id < gid_FirstUser; id++)
        new_id[id] = id;

    if (order_){
        const Vec<gate_id>& order = *order_;
        for (uind i = 0; i < order.size(); i++){
            assert(!N.deleted(order[i]));
            new_id[order[i]] = i + gid_FirstUser; }
    }

/*
    - Gå igenom 'gates' och kompaktera ID
    - Gå igenom varje grindtyp och kompaktera pekare in i 'gate_data' (och )
    - Listeners?
    - Pobs
*/
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
