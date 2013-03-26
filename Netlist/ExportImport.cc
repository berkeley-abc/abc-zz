//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ExportImport.cc
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Read and write various AIG/netlist formats.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ExportImport.hh"
#include "ZZ/Generics/Sort.hh"
#include "ZZ/Generics/ExprParser.hh"
#include "StdLib.hh"
#include "StdPob.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Related functions:


void makeAllOutputsProperties(NetlistRef N)
{
    Add_Pob(N, properties);
    For_Gatetype(N, gate_PO, w)
        properties.push(w);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Text AIG writer:


/*
BNF grammar for text AIG format:

    Letter ::= 's' | 'S' | 'a' | 'A' | 'i' | 'I'
    Number ::= [0-9]+
    Lit    ::= Letter Number
    And    ::= Lit Lit
    PO     ::= ('+' | '-') Number Lit

    Elem   ::= And | PO
    File   ::= '\n' | Elem File

's' = state variable (flop)
'a' = AND-node
'i' = primary input (PI)

Capital letters represent negation.

PIs, POs and Flops are assumed to have distinct "numbers" as part of their external interface.
AND-nodes are given numbers in the order they are created (starting from 1). Special index 0
represents the false gate: 'a0 == False', 'A0 == True'.

A file is a sequence of AND-nodes and primary outputs (POs). If the next letter to parse is a
letter, then it is an AND-node, if it is a '+' or a '-' it is a PO. An AND-node consists of two
consecutive literals (the input to the AND-node). A PO consists of the sign, followed by the
external PO number, followed by the signal declared as PO.
*/


static
void writeTaigNode(Out& out, WMap<uint>& and2num, Wire w)
{
    switch (type(w)){
    case gate_Flop:
        out += (sign(w) ? 'S' : 's'), attr_Flop(w).number;
        break;
    case gate_PI:
        out += (sign(w) ? 'I' : 'i'), attr_PI(w).number;
        break;
    case gate_Const:
        if         (+w == glit_True )  out += sign(w) ? "a0" : "A0";
        else assert(+w == glit_False), out += sign(w) ? "A0" : "a0";
        break;
    case gate_And:
        out += (sign(w) ? 'A' : 'a'), and2num[w];
        break;
    default: assert(false); }
}


void writeTaig(Out& out, NetlistRef N)
{
    Vec<gate_id> order;
    upOrder(N, order);

    WMap<uint> and2num;
    uint andC = 1;      // -- index 0 is reserved for the constant FALSE.
    for (uind n = 0; n < order.size(); n++){
        Wire w = N[order[n]];
        if (type(w) == gate_And){
            writeTaigNode(out, and2num, w[0]);
            writeTaigNode(out, and2num, w[1]);
            and2num(w) = andC++;

        }else if (type(w) == gate_PO){
            out += (sign(w) ? '-' : '+'), attr_PO(w).number;
            writeTaigNode(out, and2num, w[0]);

        }else
            assert(type(w) == gate_Flop || type(w) == gate_PI);
    }
    out += '\n';
}


void writeTaig(String filename, NetlistRef N)
{
    OutFile out(filename);
    writeTaig(out, N);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// AIGER reader:


static
void setNum(Wire w, int num)
{
    if      (type(w) == gate_PI  ) attr_PI  (w).number = num;
    else if (type(w) == gate_PO  ) attr_PO  (w).number = num;
    else if (type(w) == gate_Flop) attr_Flop(w).number = num;
    else assert(false);
}


// Directly after parsing, gate IDs and AIGER indices are lined up as
// 'aiger_index_of_w = id(w) - gid_FirstUser + 1'
//
void readAiger_(In& in, NetlistRef N, bool store_comment)
{
    assert(N.empty());
    Add_Pob(N, flop_init);

    // Parse header:
    Vec<char> buf;
    Vec<Str>  fs;
    readLine(in, buf);
    splitArray(buf.slice(), " ", fs);

    if (!eq(fs[0], "aig") || fs.size() < 6)
        throw Excp_AigerParseError("Not an AIGER file. Must start with 'aig [M I L O A]'.");

    uind n_PIs   = (uind)stringToUInt64(fs[2]); // var index: 1 + pi_index
    uind n_Flops = (uind)stringToUInt64(fs[3]); // var index: 1 + n_PIs + flop_index
    uind n_POs   = (uind)stringToUInt64(fs[4]); // var index: 1 + n_PIs + n_Flops + n_Ands + po_index   (these gates does NOT exist in the AIGER world!)
    uind n_Ands  = (uind)stringToUInt64(fs[5]); // var index: 1 + n_PIs + n_Flops + and_index

    uind n_bad    = 0;  // (we don't keep track of var index for these; don't need it for POs either, strictly speaking)
    uind n_constr = 0;
    uind n_live   = 0;  // -- NOTE! List of lists (one per liveness property).
    uind n_fair   = 0;
    bool new_aiger = fs.size() > 6;
    if (fs.size() > 6) n_bad    = (uind)stringToUInt64(fs[6]);
    if (fs.size() > 7) n_constr = (uind)stringToUInt64(fs[7]);
    if (fs.size() > 8) n_live   = (uind)stringToUInt64(fs[8]);
    if (fs.size() > 9) n_fair   = (uind)stringToUInt64(fs[9]);

    //**/Dump(n_PIs, n_Flops, n_POs, n_Ands, n_bad, n_constr, n_live, n_fair, new_aiger);

    // Create gates:
    Vec<GLit> aig2nl;
    aig2nl.push(~glit_True);

    for (uind i = 0; i < n_PIs  ; i++) aig2nl.push(N.add(PI_  (i)));
    for (uind i = 0; i < n_Flops; i++) aig2nl.push(N.add(Flop_(i)));
    for (uind i = 0; i < n_Ands ; i++) aig2nl.push(N.add(And_ () ));
    for (uind i = 0; i < n_POs  ; i++) aig2nl.push(N.add(PO_  (i)));

    // Read flops/POs:
    for (uind i = 0; i < n_Flops; i++){
        readLine(in, buf);
        splitArray(buf.slice(), " ", fs);
        assert(fs.size() == 1 || fs.size() == 2);
        uind lit    = (uind)stringToUInt64(fs[0]);
        Wire w_flop = N[aig2nl[i + 1 + n_PIs]]; assert_debug(type(w_flop) == gate_Flop);
        Wire w_in   = N[aig2nl[lit >> 1]] ^ (lit & 1);
        w_flop.set(0, w_in);

        if (fs.size() == 2){
            uind lit_init = (uind)stringToUInt64(fs[1]);
            if (lit_init == 0)
                flop_init(w_flop) = l_False;
            else if (lit_init == 1)
                flop_init(w_flop) = l_True;
            else if (lit_init != 2 * (1 + n_PIs + i))
                throw Excp_AigerParseError((FMT "Flop not initialized to 0/1/X: init(flop[%_]) = %Cw%_", i, (lit_init & 1)?'~':0, (lit_init>>1)));
            else
                flop_init(w_flop) = l_Undef;
        }else
            flop_init(w_flop) = l_False;
    }

    for (uind i = 0; i < n_POs; i++){
        uind lit  = (uind)parseUInt64(in); in++;
        Wire w_po = N[aig2nl[i + 1 + n_PIs + n_Flops + n_Ands]]; assert_debug(type(w_po) == gate_PO);
        Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
        w_po.set(0, w_in);
    }

    // Read bad/constr/live/fair:
    if (n_bad > 0){
        Add_Pob(N, properties);
        for (uind i = 0; i < n_bad; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'bad' section");
            uind lit = (uind)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            properties.push(~N.add(PO_(N.typeCount(gate_PO)), w_in));
        }
    }

    if (n_constr > 0){
        Add_Pob(N, constraints);
        for (uind i = 0; i < n_constr; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'constraint' section");
            uind lit = (uind)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            constraints.push(N.add(PO_(N.typeCount(gate_PO)), w_in));
        }
    }

    if (n_live > 0){
        Add_Pob(N, fair_properties);
        for (uind n = 0; n < n_live; n++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'liveness' section");
            fair_properties.push();
            fair_properties.last().setSize((uint)stringToUInt64(fs[0]));
        }

        for (uind n = 0; n < n_live; n++){
            for (uind i = 0; i < fair_properties[n].size(); i++){
                readLine(in, buf);
                splitArray(buf.slice(), " ", fs);
                if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'liveness' section");

                uind lit = (uind)stringToUInt64(fs[0]);
                Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
                fair_properties[n][i] = N.add(PO_(N.typeCount(gate_PO)), w_in);
            }
        }
    }

    if (n_fair > 0){
        Add_Pob(N, fair_constraints);
        for (uind i = 0; i < n_fair; i++){
            readLine(in, buf);
            splitArray(buf.slice(), " ", fs);
            if (fs.size() != 1) throw Excp_AigerParseError("Expected a single number on each line in the 'fairness' section");
            uind lit = (uind)stringToUInt64(fs[0]);
            Wire w_in = N[aig2nl[lit >> 1]] ^ (lit & 1);
            fair_constraints.push(N.add(PO_(N.typeCount(gate_PO)), w_in));
        }
    }


    // Read ANDs:
    for (uind i = 0; i < n_Ands; i++){
        uind delta0 = (uind)getUInt(in);
        uind delta1 = (uind)getUInt(in);
        uind my_id  = i + 1 + n_PIs + n_Flops;
        uind lit0   = 2*my_id - delta0;
        uind lit1   = lit0 - delta1;
        Wire w_and  = N[aig2nl[my_id]]; assert_debug(type(w_and) == gate_And);
        Wire w0     = N[aig2nl[lit0 >> 1]] ^ (lit0 & 1);
        Wire w1     = N[aig2nl[lit1 >> 1]] ^ (lit1 & 1);
        w_and.set(0, w0);
        w_and.set(1, w1);
        //printf("%u = %sx%u & %sx%u\n", my_id, (left&1)?"~":"", left>>1, (right&1)?"~":"", right>>1);
        //printf("%u = %sx%u & %sx%u\n", my_id, sign(w0)?"~":"", id(w0) - gid_FirstUser + 1, sign(w1)?"~":"", id(w1) - gid_FirstUser + 1);
    }

    // Read names:
    Vec<Pair<GLit,int> >    xnums;
    Vec<Pair<GLit,String> > xnames;
    bool                    use_xnums = true;
    while (!in.eof() && *in != 'c'){
        char type  = in++;
        uind index = (uind)parseUInt64(in); in++;
        gets(in, buf, isWS);
        in++;
        buf.push(0);

        GLit p;
        if      (type == 'i') p = aig2nl[1 + index];
        else if (type == 'l') p = aig2nl[1 + n_PIs + index];
        else if (type == 'o') p = aig2nl[1 + n_PIs + n_Flops + n_Ands + index];
        else if (type == 'b' || type == 'c' || type == 'j' || type == 'f') /*ignore*/;
        else throw Excp_AigerParseError("Expected symbol prefix: i l o b c j f");

        xnames.push(tuple(p, String(buf.base())));
        if (buf[0] == '@' && buf[1] == type)
            xnums.push(tuple(p, (int)stringToInt64(&buf[2])));
        else
            use_xnums = false;
    }

    if (use_xnums){
        for (uind i = 0; i < xnums.size(); i++)
            setNum(N[xnums[i].fst], xnums[i].snd);
    }else{
        for (uind i = 0; i < xnames.size(); i++){
            N.names().add(xnames[i].fst, xnames[i].snd.slice()); }
    }

    if (!in.eof() && *in == 'c' && store_comment){
        // Read comment:
        in++;
        Add_Pob(N, aiger_comment);

        while (!in.eof())
            aiger_comment.push(in++);
    }

    if (!new_aiger){
        Add_Pob(N, properties);
        For_Gatetype(N, gate_PO, w)
            properties.push(~w);
    }
}


// Wrapper to catch some more exceptions.
void readAiger(In& in, NetlistRef N, bool store_comment)
{
    try{
        readAiger_(in, N, store_comment);
    }catch (Excp_EOF){
        throw Excp_AigerParseError(String("Unexpected end-of-file."));
    }catch (Excp_ParseNum err){
        throw Excp_AigerParseError(String("Incorrect number encoding: ") + Excp_ParseNum::Type_name[err.type]);
    }
}


void readAigerFile(String filename, NetlistRef N, bool store_comment)
{
    InFile in(filename);
    if (in.null())
        throw Excp_AigerParseError(String("Could not open: ") + filename);

    readAiger(in, N, store_comment);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// AIGER writer:


//=================================================================================================
// -- Initial state handling (must be called before writing file):


static
Wire addMux(NetlistRef N, Wire sel, Wire tt, Wire ff, WZet& ignore)
{
    Wire u = ~N.add(And_(),  sel, tt);
    Wire v = ~N.add(And_(), ~sel, ff);
    Wire ret = ~N.add(And_(), u, v);
    ignore.add(u);
    ignore.add(v);
    ignore.add(ret);
    return ret;
}


// Transform a netlist with flops initialized to 0/1/X (by 'flop_init') into one where all flops
// are originally zero. This will introduce a new PI for each flop initialized to 'X', and invert
// the inputs and outputs of flops that are initialized to '1'. The PIs introduced are given number
// beyond the last existing. The POB 'flop_init' is removed.
void removeFlopInit(NetlistRef N)
{
    if (!Has_Pob(N, flop_init))
        return;
    Get_Pob(N, flop_init);

    bool was_strashed = false;
    if (Has_Pob(N, strash)){
        Remove_Pob(N, strash);
        was_strashed = true;
    }

    // Invert/MUX outputs of flops initialized to 1/X:
    WMapL<Wire> muxes;
    WZet        ignore;
    Wire cycle0 = Wire_NULL;
    uint piC = nextNum_PI(N);
    For_Gates(N, w){
        if (ignore.has(w)) continue;

        For_Inputs(w, v){
            if (type(v) == gate_Flop){
                if (flop_init[v] == l_True)
                    w.set(Input_Pin_Num(v), ~v);

                else if (flop_init[v] == l_Undef){
                    if (cycle0 == Wire_NULL)
                        cycle0 = ~N.add(Flop_(nextNum_Flop(N)), N.True());

                    if (muxes[v] == Wire_NULL){
                        Wire w_pseudo = N.add(PI_(piC++));
                        muxes(v) = addMux(N, cycle0, w_pseudo, +v, ignore);
                    }
                    w.set(Input_Pin_Num(v), muxes[v] ^ sign(v));
                }
            }
        }
    }

    // Invert inputs of flops initialized to '1':
    For_Gatetype(N, gate_Flop, w){
        if (flop_init[w] == l_True){
            if (w[0] != Wire_NULL)
                w.set(0, ~w[0]);
        }
    }

    if (was_strashed)
        Add_Pob0(N, strash);

    Remove_Pob(N, flop_init);
}


//=================================================================================================
// -- Main:


struct GetNum {
    typedef Wire Key;
    int operator() (Wire w) const {
        if      (type(w) == gate_PI)   return attr_PI  (w).number;
        else if (type(w) == gate_PO)   return attr_PO  (w).number;
        else if (type(w) == gate_Flop) return attr_Flop(w).number;
        else{ assert(false); return 0; }
    }
};


template<class GA>
static void writeAigerNumbers(Out& out, const Vec<Wire>& gs, char prefix)
{
    uind c = 0;
    for (uint i = 0; i < gs.size(); i++){
        Wire w = gs[i];
        if (gateAttr<GA>(w).number != num_NULL)
            out += prefix, c, ' ', '@', prefix, gateAttr<GA>(w).number, '\n';
        c++;
    }
}

#if 0
template<class GA>
static void writeAigerNumbers(Out& out, NetlistRef N, char prefix)
{
    uind c = 0;
    For_Gatetype(N, GA::type, w){
        if (gateAttr<GA>(w).number != num_NULL)
            out += prefix, c, ' ', '@', prefix, gateAttr<GA>(w).number, '\n';
        c++;
    }
}
#endif


bool writeAigerFile(String filename, NetlistRef N, Array<uchar> comment)
{
    OutFile out(filename);
    if (out.null()) return false;

    writeAiger(out, N, comment);
    return true;
}


// Returns FALSE if file could not be created.
// PRE-CONDITION: All PIs, POs, and Flops are uniquely numbered (with small numbers).
bool writeAiger(Out& out, NetlistRef N, Array<uchar> comment)
{
    // Compute mapping:
    WMap<uind> n2a;     // -- map wire in 'N' to AIGER literal.
    n2a(N.True()) = 1;

    Vec<gate_id> order;
    {
        WZet seen;
        bool is_ordered = true;
        For_Gates(N, w){    // -- if netlist is already topologically sorted, use that order (reordering can be invasive)
            if (type(w) == gate_And && (!seen.has(w[0]) || !seen.has(w[1]))){
                is_ordered = false;
                break;
            }
            seen.add(w);
            order.push(id(w));
        }
        if (!is_ordered)
            upOrder(N, order);
    }

    Vec<Wire> is, fs, os;
    Vec<Wire> as(reserve_, N.typeCount(gate_And));
    for (uind i = 0; i < order.size(); i++){
        Wire w = N[order[i]];
        switch (type(w)){
        case gate_PI  : is.push(w); break;
        case gate_Flop: fs.push(w); break;
        case gate_PO  : os.push(w); break;
        case gate_And : as.push(w); break;
        default: assert(false); }
    }

    // If numbering is compact, chose that order for the AIGER file:
    if (checkNumberingPIs  (N, true)) sobSort(sob(is, proj_lt(GetNum())));
    if (checkNumberingPOs  (N, true)) sobSort(sob(os, proj_lt(GetNum())));
    if (checkNumberingFlops(N, true)) sobSort(sob(fs, proj_lt(GetNum())));

    uind piC   = 1;
    uind flopC = 1 + is.size();
    uind andC  = 1 + is.size() + fs.size();
    for (uind i = 0; i < is.size(); i++) n2a(is[i]) = 2 * piC++;
    for (uind i = 0; i < fs.size(); i++) n2a(fs[i]) = 2 * flopC++;
    for (uind i = 0; i < as.size(); i++) n2a(as[i]) = 2 * andC++;


    // Write header, flops and POs:
    out += "aig ", is.size() + fs.size() + as.size(), ' ', is.size(), ' ', fs.size(), ' ', os.size(), ' ', as.size(), '\n';

    for (uind i = 0; i < fs.size(); i++){
        if (!fs[i])
            out += '0', '\n';       // -- missing flops are constant 0
        else{
            Wire w0 = fs[i][0];
            out += n2a[w0] ^ uind(sign(w0)), '\n';
        }
    }
    for (uind i = 0; i < os.size(); i++){
        if (!os[i])
            out += '0', '\n';
        else{
            Wire w0 = os[i][0];     // -- missing primary inputs are constant 0
            out += n2a[w0] ^ uind(sign(w0)), '\n';
        }
    }

    for (uind i = 0; i < as.size(); i++){
        Wire w = as[i];
        uind idx_w  = n2a[w];
        uind idx_w0 = n2a[w[0]] ^ uind(sign(w[0]));
        uind idx_w1 = n2a[w[1]] ^ uind(sign(w[1]));
        assert(idx_w > idx_w0); assert(idx_w > idx_w1);    // -- the topological order should assign a higher index to 'w'
        if (idx_w0 < idx_w1) swp(idx_w0, idx_w1);
        putu(out, idx_w - idx_w0);
        putu(out, idx_w0 - idx_w1);
    }

    // Write external gate numbers:
    writeAigerNumbers<PI_  >(out, is, 'i');
    writeAigerNumbers<Flop_>(out, fs, 'l');
    writeAigerNumbers<PO_  >(out, os, 'o');

    // Write comment:
    if (comment){
        out += "c";     // -- standard say 'c' must be followed by newline, but we leave that up to the caller (since ABC doesn't do this)
        for (uind i = 0; i < comment.size(); i++)
            out += (char)comment[i];
    }

    return true;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// BLIF reader:


static
Str dupName(StackAlloc<char>& mem, const Str& name)
{
    char* str = mem.alloc(name.size());
    memcpy(str, name.base(), name.size());
    return Array_new(str, name.size());
}


// Read a line, concatenating the next if it ends in a '\'
static
bool readBlifLine(In& in, String& text, String& temp, uint64& line_no)
{
    do{
        if (in.eof())
            return false;

        readLine(in, text);
        line_no++;
        while (text.last() == '\\'){
            text.pop();
            readLine(in, temp);
            append(text, temp);
            line_no++;
        }
        trim(text);
    }while (text.size() == 0 || text[0] == '#');
    return true;
}


// Check if current line starts with a '.' (skipping over whitespaces, if any)
static
bool endOfTable(In& in)
{
    for(;;){
        skipWS(in);
        if (in.eof()) return true;
        if (*in == '#') skipEol(in);
        else break;
    }

    return *in == '.';
}


// Check that 'text' is of form '10-00  1', with '-' only allowed optionally.
static
bool validateTableLine(const String& text, uint n_inputs, bool allow_dash)
{
    for (uint i = 0; i < n_inputs; i++)
        if (text[i] != '0' && text[i] != '1' && (!allow_dash || text[i] != '-'))
            return false;

    for (uint i = n_inputs; i < text.size() - 1; i++)
        if (!isWS(text[i]))
            return false;

    if (text.last() != '0' && text.last() != '1')
        return false;

    return true;
}


static
ushort readBlifFtb(In& in, uint sz, String& text, String& temp, uint64& line_no)
{
    static const ushort proj0[4] = { 0x5555, 0x3333, 0x0F0F, 0x00FF };
    static const ushort proj1[4] = { 0xAAAA, 0xCCCC, 0xF0F0, 0xFF00 };

    bool   first_line = true;
    bool   on_set = true;
    ushort mask = 0;

    while (!endOfTable(in)){
        readBlifLine(in, text, temp, line_no);

        if (!validateTableLine(text, sz, true))
            Throw(Excp_BlifParseError) "[line %_] Invalid table line: %_", line_no, text;

        ushort m = 0xFFFF;
        for (uint i = 0; i < sz; i++){
            if      (text[i] == '0') m &= proj0[i];
            else if (text[i] == '1') m &= proj1[i];
        }
        mask |= m;

        if (first_line){
            on_set = (text.last() == '1');
            first_line = false;
        }else if (on_set != (text.last() == '1'))
            Throw(Excp_BlifParseError) "[line %_] All lines in table must end with same value.", line_no;
    }

    return on_set ? mask : (mask ^ 0xFFFF);
}


void readBlif(String filename, NetlistRef N, bool expect_aig, bool store_names)
{
    assert(N.empty());

    InFile in(filename);
    if (in.null())
        Throw(Excp_BlifParseError) "Could not open: %_", filename;

    String    text, temp;
    String    model;
    String    input_names;
    String    output_names;
    Vec<Str>  elems;
    uint64    line_no = 0;
    Map<Str,GLit>    name2gate;
    StackAlloc<char> mem;       // -- store node names

    for(;;){
        if (!readBlifLine(in, text, temp, line_no))
            Throw(Excp_BlifParseError) "[line %_] Unexpected end-of-file", line_no;

        if (hasPrefix(text, ".model ")){
            if (model != "")
                Throw(Excp_BlifParseError) "[line %_] Only support flat BLIF files.", line_no;
            model = text.sub(7);
            trim(model);

        }else if (hasPrefix(text, ".inputs ")){
            input_names = text.sub(8);
            splitArray(input_names.slice(), " ", elems);
            for (uind i = 0; i < elems.size(); i++)
                if (name2gate.set(elems[i], N.add(PI_(i))))
                    Throw(Excp_BlifParseError) "[line %_] Name declared twice: %_", line_no, elems[i];

        }else if (hasPrefix(text, ".outputs ")){
            output_names = text.sub(9);
            splitArray(output_names.slice(), " ", elems);
            for (uind i = 0; i < elems.size(); i++){
                temp.clear();
                temp += "@out:";
                printUInt(temp, N.typeCount(gate_PO));
                char* str = mem.alloc(temp.size());
                memcpy(str, temp.base(), temp.size());
                Wire w_po = N.add(PO_(i));
                name2gate.set(Array_new(str, temp.size()), w_po);

                if (!name2gate.has(elems[i])){
                    Wire w = expect_aig ? N.add(And_()) : N.add(Lut4_());
                    w_po.set(0, w);
                    name2gate.set(elems[i], w);
                }
            }

        }else if (hasPrefix(text, ".names ")){
            if (input_names  == "") Throw(Excp_BlifParseError) "[line %_] Expected .inputs directive before any .names directive.", line_no;
            if (output_names == "") Throw(Excp_BlifParseError) "[line %_] Expected .outputs directive before any .names directive.", line_no;
            splitArray(text.slice(), " ", elems);

            for (uind i = 0; i < elems.size(); i++)
                elems[i] = dupName(mem, elems[i]);

            uint sz = elems.size() - 2;
            if (expect_aig){
                //
                // AIG NODE:
                //
                if (sz > 2)
                    Throw(Excp_BlifParseError) "[line %_] An AIG BLIF file was expected. All gates must have two inputs.", line_no;

                // Parse names:
                GLit g[3];
                for (uint i = 0; i <= sz; i++){
                    if (!name2gate.peek(elems[i+1], g[i])){
                        g[i] = N.add(And_());
                        name2gate.set(elems[i+1], g[i]);
                    }
                }
                Wire w = N[g[sz]];
                assert(type(w) == gate_And);
                if (w[0] || w[1])
                    Throw(Excp_BlifParseError) "[line %_] Name declared twice: %_", line_no, elems[3];

                // Read function table:
                if (!readBlifLine(in, text, temp, line_no))
                    Throw(Excp_BlifParseError) "[line %_] Unexpected end-of-file", line_no;

                if (!endOfTable(in))
                    Throw(Excp_BlifParseError) "[line %_] Expected a one-line function table for AIG BLIF file.", line_no;
                if (!validateTableLine(text, sz, false))
                    Throw(Excp_BlifParseError) "[line %_] Invalid table line: %_", line_no, text;

                if (sz == 0){
                    // Constant:
                    if (text.last() == '1'){
                        N[g[0]].set(0, N.True());
                        N[g[0]].set(1, N.True());
                    }else{
                        N[g[0]].set(0, ~N.True());
                        N[g[0]].set(1, ~N.True());
                    }
                }else{
                    // And gate or buffer:
                    bool s0 = (text[0] == '0');
                    bool s1 = (text[1] == '0');
                    if (text.last() == '0')
                        Throw(Excp_BlifParseError) "[line %_] AIG BLIFs must have a '1' at the end of each table line.", line_no;

                    // Assign inputs:
                    w.set(0, N[g[0]] ^ s0);
                    if (sz == 1){
                        if (w.size() != 1)
                            w.set(1, N[g[0]] ^ s0);       // -- treat buffers as AND gates with identical inputs
                    }else
                        w.set(1, N[g[1]] ^ s1);
                }

            }else{
                //
                // LUT NODE:
                //
                if (sz > 4)
                    Throw(Excp_BlifParseError) "[line %_] Currently only support 4-input LUTs in BLIF files.";

                // Parse names:
                GLit g[5];
                for (uint i = 0; i <= sz; i++){
                    if (!name2gate.peek(elems[i+1], g[i])){
                        g[i] = N.add(Lut4_());
                        name2gate.set(elems[i+1], g[i]);
                    }
                }
                Wire w = N[g[sz]];
                if (w[0] || w[1] || w[2] || w[3])
                    Throw(Excp_BlifParseError) "[line %_] Name declared twice: %_", line_no, elems.last();

                // Assign inputs:
                attr_Lut4(w).ftb = readBlifFtb(in, sz, text, temp, line_no);
                for (uint i = 0; i < sz; i++)
                    w.set(i, N[g[i]]);
            }

        }else if (hasPrefix(text, ".latch ")){      // .latch <input gate> <this gate> <init value>
            Assure_Pob(N, flop_init);
            splitArray(text.slice(), " ", elems);
            if (name2gate.has(elems[2]))
                Throw(Excp_BlifParseError) "[line %_] A latch must be declared before it is used: %_", line_no, elems[2];

            Wire w_ff = N.add(Flop_(N.typeCount(gate_Flop)));
            name2gate.set(dupName(mem, elems[2]), w_ff);

            Wire w = expect_aig ? N.add(And_()) : N.add(Lut4_());
            w_ff.set(0, w);
            name2gate.set(dupName(mem, elems[1]), w);

            if (elems[3].size() == 1 && elems[3][0] == '0')
                flop_init(w_ff) = l_False;
            else if (elems[3].size() == 1 && elems[3][0] == '1')
                flop_init(w_ff) = l_True;
            else if (elems[3].size() == 1 && elems[3][0] == '3')
                flop_init(w_ff) = l_Undef;
            else
                Throw(Excp_BlifParseError) "[line %_] Invalid initial value: %_", line_no, elems[3];

        }else if (hasPrefix(text, ".end")){
            break;

        }else{
            Throw(Excp_BlifParseError) "[line %_] Unknown keyword: %_", line_no, text;
        }
    }

    // Store names:
    if (store_names){
        For_Map(name2gate){
            Str  key   = Map_Key(name2gate);
            GLit value = Map_Value(name2gate);
            if (key.size() > 0){
                if (isDigit(key[0])){
                    temp.clear();
                    FWrite(temp) "_%_", key;
                    N.names().add(value, temp.slice());
                }else
                    N.names().add(value, key);
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// SIF parser:


//=================================================================================================
// -- Tokenizer:


// Replace comments with spaces (preserving newlines). Returns FALSE if 'text' contains an
// unterminated block comment.
static
bool removeCstyleComments(Array<char> text)
{
    char* p   = &text[0];
    char* end = &text.end();

    while (p != end){
        if (*p == '/' && p+1 != end && p[1] == '/'){
            // Space out line comment:
            for(;;){
                if (p == end)
                    break;
                else if (*p == '\n'){
                    p++; break;
                }else
                    *p++ = ' ';
            }

        }else if (*p == '/' && p+1 != end && p[1] == '*'){
            // Space out block comment (preserving newlines):
            p[0] = p[1] = ' ';
            p += 2;
            for(;;){
                if (p == end)
                    return false;       // -- unterminated comment
                else if (*p == '\n')
                    p++;
                else if (*p == '*' && p+1 != end && p[1] == '/'){
                    p[0] = p[1] = ' ';
                    p += 2;
                    break;
                }else
                    *p++ = ' ';
            }

        }else
            p++;
    }

    return true;
}


static char sif_name_char[256];
static char sif_token_char[256];


ZZ_Initializer(sif, 0)
{
    for (uint i = 0; i < 256; i++) sif_name_char[i] = sif_token_char[i] = 0;

    sif_name_char['_'] = 1;
    sif_name_char['.'] = 1;
    sif_name_char['#'] = 1;
    sif_name_char['%'] = 1;
    for (uint i = '0'; i <= '9'; i++) sif_name_char[i] = 1;
    for (uint i = 'A'; i <= 'Z'; i++) sif_name_char[i] = 1;
    for (uint i = 'a'; i <= 'z'; i++) sif_name_char[i] = 1;

    sif_token_char['['] = 1;
    sif_token_char[']'] = 1;
    sif_token_char['('] = 1;
    sif_token_char[')'] = 1;
    sif_token_char['='] = 1;
    sif_token_char['^'] = 1;
    sif_token_char['&'] = 1;
    sif_token_char[';'] = 1;
}


class SifToken {
    uchar opc;
    uint  len : 24;
    uind  off;

public:
    SifToken(uind offset, uint len_, char op = 0) { opc = (uchar)op; len = len_; off = offset; }

    bool isOp  ()         const { return opc != 0; }
    bool isName()         const { return !isOp(); }
    Str  name  (Str base) const { return Str(&base[off], len); }
    char op    ()         const { return (char)opc; }
    uind offset()         const { return off; }
};


void sifTokenize(Str text, Vec<SifToken>& elems)
{
    // A token is either a name or one of the following characters: [ ] ( ) = ^ & ;
    // A name is either a sequence containing: [0-9] [a-z] [A-Z] _ . # %
    // or (2) single-quoted: '<arbitary sequence of characters>'.

    cchar* p   = &text[0];
    cchar* p0  = &text[0];
    cchar* end = &text.end();
    uind line_no = 1;

    for(;;){
        // Skip white-space:
        while (p != end && isWS(*p)){
            if (*p == '\n') line_no++;
            p++;
        }

        if (p == end)
            break;      // DONE!

        if (sif_token_char[(uchar)*p]){
            elems.push(SifToken(p - p0, 1, *p));
            p++;

        }else if (*p == '\''){
            p++;
            cchar* start = p;
            uind start_line = line_no;
            while (p != end && *p != '\''){
                if (*p == '\n') line_no++;
                p++;
            }
            if (p == end)
                Throw(Excp_SifParseError) "[line %_] Unterminated quoted name starting at line %_.", line_no, start_line;

            elems.push(SifToken(start - p0, p - start));
            p++;

        }else{
            cchar* start = p;
            while (p != end && sif_name_char[(uchar)*p])
                p++;

            if (p == start)
                Throw(Excp_SifParseError) "[line %_] Unexpected character: %_", line_no, *p;

            elems.push(SifToken(start - p0, p - start));
        }
    }
}


//=================================================================================================
// -- Expression parsing:


struct SifStream : XP_TokenStream {
    NetlistRef N;
    Array<char> text;
    const Vec<SifToken>& elems;
    uind& p;

    SifStream(NetlistRef N_, Array<char> text_, const Vec<SifToken>& elems_, uind& p_) :
        N(N_), text(text_), elems(elems_), p(p_) {}

    GLit  toGLit(void* expr) const { return GLit(packed_, (uint)(uintp)expr); }
    void* toExpr(GLit w)     const { return (void*)(uintp)w.data(); }

    bool parseOp(uint& op_tag, uint& pos, XP_OpType& type, int& prio) {
        if (p < elems.size() && elems[p].isOp()){
            if (elems[p].op() == '^'){
                p++;
                type = xop_PREFIX;
                prio = 1;
                return true;
            }else if (elems[p].op() == '&'){
                p++;
                type = xop_INFIXL;
                prio = 0;
                return true;
            }
        }
        return false;
    }

    bool parseLParen(uint& paren_tag, uint& pos) {
        if (p < elems.size() && elems[p].isOp() && elems[p].op() == '('){
            p++; return true; }
        return false; }

    bool parseRParen(uint& paren_tag, uint& pos) {
        if (p < elems.size() && elems[p].isOp() && elems[p].op() == ')'){
            p++; return true; }
        return false; }

    bool parseAtom(void*& atom_expr, uint& pos) {
        if (p < elems.size() && elems[p].isName()){
            Str name = elems[p].name(text);
            GLit w = N.names().lookup(name);
            if (!w) Throw(Excp_SifParseError) "[line %_] Undefined symbol: %_", countLineNo(text, elems[p].offset()), name;
            atom_expr = toExpr(w);
            p++;
            return true;
        }else
            return false;
    }

    void* applyPrefix(uint, void* expr) {
        return toExpr(~toGLit(expr)); }

    void* applyPostfix(uint, void*) {
        assert(false); }

    void* applyInfix(uint, void* expr0, void* expr1) {
        return toExpr(s_And(toGLit(expr0) + N, toGLit(expr1) + N)); }


    void disposeExpr(void* expr) {
        /*nothing*/ }
};


//=================================================================================================
// -- Statement parsing:


Str sifGetName(const Array<char>& text, const Vec<SifToken>& elems, uind& p, cchar* expected_name = NULL)
{
    if (p >= elems.size())
        Throw(Excp_SifParseError) "Unexpected end-of-file.";
    if (!elems[p].isName())
        Throw(Excp_SifParseError) "[line %_] Expected name not operator: %_", countLineNo(text, elems[p].offset()), elems[p].op();

    Str s = elems[p].name(text);
    p++;

    if (expected_name && !eq(s, expected_name))
        Throw(Excp_SifParseError) "[line %_] Expected \"%_\", not: %_", countLineNo(text, elems[p].offset()), expected_name, s;

    return s;
}


char sifGetOp(const Array<char>& text, const Vec<SifToken>& elems, uind& p, char expected_op = 0, bool just_probe = false)
{
    if (p >= elems.size())
        Throw(Excp_SifParseError) "Unexpected end-of-file.";
    if (!elems[p].isOp())
        Throw(Excp_SifParseError) "[line %_] Expected operator not name: %_", countLineNo(text, elems[p].offset()), elems[p].op();

    char c = elems[p].op();

    if (just_probe){
        if (c == expected_op) p++;
        return c == expected_op;
    }

    p++;
    if (expected_op && c != expected_op)
        Throw(Excp_SifParseError) "[line %_] Expected \"%_\", not: %_", countLineNo(text, elems[p].offset()), expected_op, c;

    return c;
}


/*
Not yet supported:

   LIVELOCK RWADR0;  // property which fails if the following does not hold AG AF !RWADR0 
   FAIRNESS signalF; // constrains a LIVELOCK counterexample: signalF must hold at least once in the failing lasso loop 
   ARRAY {R,W} #address_pins #columns #rows I{0,1,X initial value string of width #columns} {array_name}
   ARRAY_PIN {array_name} {R,W,I} #port_num {E,A,D} #pin_num {signal_name_being_connected};

   && for XOR
   | for OR
*/

void readSif(String filename, NetlistRef N, String* module_name, Vec<String>* liveness_names)
{
    assert(N.empty());
    Add_Pob(N, strash);
    N.names().enableLookup();

    // Read file:
    Array<char> text = readFile(filename);
    if (!text)
        Throw(Excp_SifParseError) "Could not open file: %_", filename;
    removeCstyleComments(text);

    Vec<SifToken> elems;
    sifTokenize(text, elems);

    uind p = 0;
    SifStream ss(N, text, elems, p);
    Vec<Str> ff_reset;

    #define EXPECT_NAME(name) sifGetName(text, elems, p, name)
    #define READ_NAME         sifGetName(text, elems, p)
    #define EXPECT_OP(op)     sifGetOp  (text, elems, p, op)
    #define READ_OP           sifGetOp  (text, elems, p)
    #define PROBE_OP(op)      sifGetOp  (text, elems, p, op, true)
    #define THROW_UNDEF_SYM(name) Throw(Excp_SifParseError) "[line %_] Undefined symbol: %_", countLineNo(text, elems[p].offset()), name;


    EXPECT_NAME("MODULE");
    Str mod = READ_NAME;
    EXPECT_OP(';');
    if (module_name)
        *module_name = mod;

    // Parse statements:
    while (p < elems.size()){
        Str s = READ_NAME;

        if (eq(s, "INPUT")){
            Wire w = N.add(PI_(N.typeCount(gate_PI)));
            N.names().add(w, READ_NAME);

        }else if (eq(s, "ZERO")){
            N.names().add(~N.True(), READ_NAME);

        }else if (eq(s, "STATE_PLUS")){
            Str ff_net = READ_NAME;
            if (PROBE_OP('[')){
                Wire w = N.add(Flop_(N.typeCount(gate_Flop)));  // -- add flop
                N.names().add(w, ff_net);
                ff_reset.push(READ_NAME);
                EXPECT_OP(']');
            }
            if (PROBE_OP('=')){
                Wire w = N.names().lookup(ff_net) + N;
                if (!w) THROW_UNDEF_SYM(ff_net);

                try{
                    Wire w_in = ss.toGLit(ss.parse()) + N;
                    if (!w_in) Throw(Excp_SifParseError) "[line %_] Empty expression.", countLineNo(text, elems[p].offset());
                    w.set(0, w_in);
                }catch (Excp_XP){
                    Throw(Excp_SifParseError) "[line %_] Invalid expression.", countLineNo(text, elems[p].offset());
                }
            }

        }else if (eq(s, "CONSTRAINT")){
            Str constr_net = READ_NAME;
            Wire w = N.names().lookup(constr_net) + N;
            if (!w) THROW_UNDEF_SYM(constr_net);

            Assure_Pob(N, constraints);
            constraints.push(N.add(PO_(N.typeCount(gate_PO)), w));

        }else if (eq(s, "INFINITELY_OFTEN")){
            Str prop_name = READ_NAME;
            if (liveness_names)
                liveness_names->push(String(prop_name));

            Vec<Str> nets;
            nets.push(READ_NAME);
            while (PROBE_OP('&'))
                nets.push(READ_NAME);

            Assure_Pob(N, fair_properties);
            fair_properties.push();
            for (uint i = 0; i < nets.size(); i++){
                Wire w = N.names().lookup(nets[i]) + N;
                if (!w) THROW_UNDEF_SYM(nets[i]);
                fair_properties[LAST].push(N.add(PO_(N.typeCount(gate_PO)), w));
            }

        }else if (eq(s, "TEST")){
            Str test_net = READ_NAME;
            Wire w = N.names().lookup(test_net) + N;
            if (!w) THROW_UNDEF_SYM(test_net);

            Assure_Pob(N, properties);
            properties.push(N.add(PO_(N.typeCount(gate_PO)), w));

        }else if (eq(s, "END")){
            // DONE!
            break;

        }else{
            EXPECT_OP('=');

            try{
                Wire w = ss.toGLit(ss.parse()) + N;
                if (!w) Throw(Excp_SifParseError) "[line %_] Empty expression.", countLineNo(text, elems[p].offset());
                N.names().add(w, s);
            }catch (Excp_XP){
                Throw(Excp_SifParseError) "[line %_] Invalid expression.", countLineNo(text, elems[p].offset());
            }
        }

        EXPECT_OP(';');
    }

    // Add reset logic:
    Add_Pob(N, flop_init);
    Scoped_Pob(N, fanouts);
    Wire reset = Wire_NULL;

    For_Gatetype(N, gate_Flop, w){
        if (w == reset) continue;
        int num = attr_Flop(w).number;

        Wire w_reset = N.names().lookup(ff_reset[num]) + N;
        if (!w_reset) THROW_UNDEF_SYM(ff_reset[num]);

        if (w_reset == N.True())
            flop_init(w) = l_True;
        else if (w_reset == ~N.True())
            flop_init(w) = l_False;
        else if (type(w_reset) == gate_PI && fanouts[w_reset].size() == 1)
            flop_init(w) = l_Undef;
        else{
            if (!reset){
                reset = N.add(Flop_(N.typeCount(gate_Flop)), ~N.True());
                flop_init(reset) = l_True;
            }
            Wire w_new = s_Mux(reset, w_reset, w);
            Fanouts fs = fanouts[w];
            for (uint i = 0; i < fs.size(); i++)
                fs[i].replace(w_new);

            flop_init(w) = l_Undef;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
