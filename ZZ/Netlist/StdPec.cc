//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : StdPec.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Standard PECs (Persistent Embedded Classes -- types that resides in a netlist)
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "StdPec.hh"
#include "StdLib.hh"
#include "StdPob.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_RawData -- a vector of characters


Register_Pec(RawData);


static uchar quote_char[256];   // -- 0 for normal characters (need no quoting), 255 for unquotable characters
static uchar unquote_char[256]; // -- only defined for non-0 and non-255 elements in 'quote_char[]'


ZZ_Initializer(Pec_RawData, -9000)
{
    for (uind i =   0; i <  32; i++) quote_char[i] = 255;
    for (uind i =  32; i < 128; i++) quote_char[i] = 0;
    for (uind i = 128; i < 256; i++) quote_char[i] = 255;

    quote_char[(uchar)'\\'] = '\\';
    quote_char[(uchar)'"'] = '"';
    quote_char[(uchar)'\0'] = '0';
    quote_char[(uchar)'\a'] = 'a';
    quote_char[(uchar)'\b'] = 'b';
    quote_char[(uchar)'\t'] = 't';
    quote_char[(uchar)'\n'] = 'n';
    quote_char[(uchar)'\v'] = 'v';
    quote_char[(uchar)'\f'] = 'f';
    quote_char[(uchar)'\r'] = 'r';

    for (uind i = 0; i < 256; i++) unquote_char[i] = 0;
    unquote_char[(uchar)'\\'] = '\\';
    unquote_char[(uchar)'"'] = '"';
    unquote_char[(uchar)'0'] = '\0';
    unquote_char[(uchar)'a'] = '\a';
    unquote_char[(uchar)'b'] = '\b';
    unquote_char[(uchar)'t'] = '\t';
    unquote_char[(uchar)'n'] = '\n';
    unquote_char[(uchar)'v'] = '\v';
    unquote_char[(uchar)'f'] = '\f';
    unquote_char[(uchar)'r'] = '\r';
}


//=================================================================================================


void Pec_RawData::load(In& in)
{
    Vec<uchar>& v = vec(*this);
    uind n = (uind)getu(in);
    v.setSize(n);
    for (uind i = 0; i < n; i++)
        v[i] = getc(in);
}


void Pec_RawData::save(Out& out) const
{
    const Vec<uchar>& v = cvec(*this);
    putu(out, v.size());
    for (uind i = 0; i < v.size(); i++)
        putc(out, v[i]);
}


void Pec_RawData::read(In& in)
{
    Vec<uchar>& v = vec(*this); assert(v.size() == 0);

    skipWS(in);
    if (*in != '"') throw String("RawData must start with a double-quote '\"'.");
    in++;

    for(;;){
        char c = in++;
        if (c == '"') break;
        if (c != '\\')
            v.push(c);
        else{
            c = in++;
            if (c != 'x')
                v.push(unquote_char[uchar(c)]);
            else{
                c  = fromHex(in++) << 4;
                c |= fromHex(in++);
                v.push(c);
            }
        }
    }
}


void Pec_RawData::write(Out& out) const
{
    const Vec<uchar>& v = cvec(*this);

    out += '"';
    for (uind i = 0; i < v.size(); i++){
        uchar c = v[i];
        uchar q = quote_char[c];
        if (q == 0)
            out += char(c);
        else if (q != 255)
            out += '\\', char(q);
        else
            out += '\\', 'x', toHex(c >> 4), toHex(c & 15);
    }
    out += '"', '\n';
}



//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Wire -- A single wire:


Register_Pec(Wire);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_VecWire -- A vector of wires (automatically updates the wires on 'compact()')


Register_Pec(VecWire);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_VecVecWire -- A vector of vector of wires: 


Register_Pec(VecVecWire);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Strash -- structural hashing object:


Register_Pec(Strash);


void Pec_Strash::hashNetlist()
{
    assert(nodes.size() == 0);
    nodes.reserve(defaultCapacity());
    For_Gatetype(netlist(nl), gate_And, w){
        bool stat = nodes.add(w);
        assert(!stat); }
}


void Pec_Strash::strashNetlist()
{
    assert(nodes.size() == 0);

    NetlistRef   N = netlist(nl);
    Vec<gate_id> order;
    upOrder(N, order, false, false);    // -- no strict sinks
    WMap<Wire>   repr;

    repr(N.True ()) =  N.True();
    repr(N.False()) = ~N.True();
    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        if (type(w) == gate_And){
            Wire x = repr[w[0]] ^ sign(w[0]); assert(+x);   // }- These may fail if a PO is used as input to another gate, for instance.
            Wire y = repr[w[1]] ^ sign(w[1]); assert(+y);   // }
            if (y < x) swp(x, y);   // -- make sure 'x < y'.
            if (+x == glit_True)
                repr(w) = sign(x) ? x : y;
            else if (+x == +y)
                repr(w) = (x == y) ? x : ~N.True();
            else{
                uind idx = nodes.index_(Hash_Strash::hash(x.lit(), y.lit()));
                Wire w_old = lookup_helper(x, y, idx);
                if (w_old)
                    repr(w) = w_old;
                else{
                    w.set(0, x);
                    w.set(1, y);
                    nodes.newEntry(idx, w.lit());
                    repr(w) = w;
                }
            }
        }else if (type(w) == gate_PO)
            w.set(0, repr[w[0]] ^ sign(w[0]));
        else
            repr(w) = w;
    }

    For_Gatetype(N, gate_Flop, w)
        w.set(0, repr[w[0]] ^ sign(w[0]));

    Vec<uchar> seen(N.size(), false);
    computeReach(N, seen);
    For_Gates(N, w){
        if (!seen[id(w)] && !isGlobalSource(w)){
            if (type(w) == gate_And)
                nodes.exclude(w);
            w.remove();
        }
    }
}


bool Pec_Strash::checkConsistency() const
{
    For_Gatetype(netlist(nl), gate_And, w)
        if (!lookup(w))
            return false;
    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Fanouts -- static fanout lists:


void Pec_Fanouts::recompute()
{
    NetlistRef N = netlist(this->nl);
    clear();

    // Count number of fanouts for each node:
    WMap<uint> n_fanouts;
    n_fanouts.reserve(N.size(), 0);

    For_All_Gates(N, w)
        For_Inputs(w, v)
            n_fanouts(v)++;

    // Calculate memory needed for external fanouts:
    uind mem_sz = 0;
    For_All_Gates(N, w)
        if (n_fanouts[w] > 1)
            mem_sz += n_fanouts[w];
  #if defined(ZZ_BIG_MODE)
    assert(mem_sz <= UINT_MAX);     // -- we don't support more than 2^32 fanouts in total
  #endif

    // Allocate memory:
    mem  = xmalloc<CConnect>(mem_sz);
    data = Array_alloc<Outs>(N.size());

    // Divvy up memory and assign empty fanout nodes:
    uint offset = 0;
    For_All_Gates(N, w){
        Outs& o = data[id(w)];
        if (n_fanouts[w] == 0){
            o.inl.parent = glit_NULL;
            o.inl.pin    = 0;

        }else if (n_fanouts[w] == 1){
            n_fanouts(w) = UINT_MAX;

        }else{
            o.ext.size   = 0x80000000 | n_fanouts[w];
            o.ext.offset = offset;
            offset += n_fanouts[w];
            n_fanouts(w) = 0;
        }
    }
    assert(offset == mem_sz);

    // Populate single fanout and multiple fanout nodes:
    For_All_Gates(N, w){
        For_Inputs(w, v){
            Outs& o = data[id(v)];
            if (n_fanouts[v] == UINT_MAX){
                o.inl.parent = w.lit() ^ sign(v);
                o.inl.pin = Input_Pin_Num(v);

            }else{
                CConnect& c = mem[o.ext.offset + n_fanouts[v]];
                c.parent = w.lit() ^ sign(v);
                c.pin    = Input_Pin_Num(v);
                n_fanouts(v)++;
            }
        }
    }
}


Register_Pec(Fanouts);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_FanoutCount -- dynamic fanout count:


Register_Pec(FanoutCount);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_UpOrder -- static topological sort:


Register_Pec(UpOrder);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_Netlist -- embedded netlist:


Register_Pec(Netlist);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_LProp:


Register_Pec(FlopInit);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pec_MemInfo:


Register_Pec(MemInfo);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm

Register_Pob(strash, Strash);
Register_Pob(fanouts, Fanouts);
Register_Pob(fanout_count, FanoutCount);
Register_Pob(up_order, UpOrder);
Register_Pob(flop_init, FlopInit);
Register_Pob(aiger_comment, RawData);
Register_Pob(properties, VecWire);
Register_Pob(constraints, VecWire);
Register_Pob(fair_properties, VecVecWire);
Register_Pob(fair_constraints, VecWire);
Register_Pob(init_bad, VecWire);
Register_Pob(init_nl, Netlist);
Register_Pob(reset, Wire);
Register_Pob(mem_info, MemInfo);

}
