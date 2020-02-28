//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Gig.cc
//| Author(s)   : Niklas Een
//| Module      : IO
//| Description : Parser for .gig format used by the old Netlist.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Gig.hh"
#include "ZZ/Generics/Map.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Pobs:


struct PobsToAdd {
    bool strash;
    bool fanouts;
    bool fanout_count;
    PobsToAdd() : strash(0), fanouts(0), fanout_count(0) {}
};


static
void readVecWire(In& in, const Map<Str,GLit>& syms, /*out*/Vec<GLit>& result)
{
    Vec<char> buf;
    while (!in.eof()){
        readLine(in, buf);
        ZZ::trim(buf);
        if (buf.size() == 0) continue;
        GLit lit;
        if (!syms.peek(buf.slice(), lit))
            Throw(String) "Unknown gate: %_", buf;
        result.push(lit);
    }
}


static
void readVecVecWire(In& in, const Map<Str,GLit>& syms, /*out*/Vec<Vec<GLit> >& result)
{
    Vec<char> buf;
    while (!in.eof()){
        readLine(in, buf);
        ZZ::trim(buf);
        if (buf.size() == 0) continue;
        if (eq(buf, "--"))
            result.push();
        else{
            GLit lit;
            if (!syms.peek(buf.slice(), lit))
                Throw(String) "Unknown gate: %_", buf;
            result.last().push(lit);
        }
    }
}


static
void addPob(Gig& N, Str name, Str type_, In& in, const Map<Str,GLit>& syms, /*out*/PobsToAdd& to_add)
{
    if (eq(name, "strash") && eq(type_, "Strash")){
        to_add.strash = true;

    }else if (eq(name, "flop_init") && eq(type_, "FlopInit")){
        Vec<char> buf;
        while (!in.eof()){
            readLine(in, buf);
            ZZ::trim(buf);
            if (buf.size() == 0) continue;
            uind pos = search(buf, '=');
            if (pos == UIND_MAX) throw String("Missing '='.");
            Str name = buf.slice(0, pos);
            Str text = buf.slice(pos+1);
            trimStr(name);
            trimStr(text);
            GLit lit;
            if (!syms.peek(name, lit))
                Throw(String) "Unknown gate: %_", buf;
            Wire w = lit + N;
            if (w != gate_FF)
                Throw(String) "Wrong gate type: %_", w.type();

            if (w[1])
                Throw(String) "Initial value already specified.";

            if (eq(text, "0")) w.set(1, GLit_False);
            else if (eq(text, "1")) w.set(1, GLit_True);
            else if (eq(text, "?")) w.set(1, GLit_Unbound);
            else if (eq(text, "!")) w.set(1, GLit_Conflict);
            else if (eq(text, "*")) w.set(1, GLit_ERROR);
            else if (eq(text, "-")) w.set(1, GLit_NULL);
            else
                Throw(String) "Unknown initialization value: %_", text;
        }

    }else if (eq(name, "properties") && eq(type_, "VecWire")){
        Vec<GLit> v;
        readVecWire(in, syms, v);
        for (uint i = 0; i < v.size(); i++){
            Wire w = v[i] + N;
            if (w != gate_PO) Throw(String) "Properties are expected to be of type PO (for legacy reasons).";
            Wire w_in = w[0] ^ w.sign;
            change(w, gate_SafeProp).init(w_in);
        }

    }else if (eq(name, "fair_properties") && eq(type_, "VecVecWire")){
        Vec<Vec<GLit> > v;
        readVecVecWire(in, syms, v);
        for (uint j = 0; j < v.size(); j++){
            Wire w_vec = N.addDyn(gate_Vec, v[j].size());
            for (uint i = 0; i < v[j].size(); i++){
                Wire w = v[j][i] + N;
                if (w != gate_PO) Throw(String) "Fairness properties are expected to be of type PO (for legacy reasons).";
                Wire w_in = w[0] ^ w.sign;;

                w_vec.set(i, w_in);
                remove(w);
            }
            N.add(gate_FairProp).init(w_vec);
        }

    }else if (eq(name, "constraints") && eq(type_, "VecWire")){
        Vec<GLit> v;
        readVecWire(in, syms, v);
        for (uint i = 0; i < v.size(); i++){
            Wire w = v[i] + N;
            if (w != gate_PO) Throw(String) "Constraints are expected to be of type PO (for legacy reasons).";
            Wire w_in = w[0] ^ w.sign;
            change(w, gate_SafeCons).init(w_in);
        }

    }else if (eq(name, "fair_constraints") && eq(type_, "VecWire")){
        Vec<GLit> v;
        readVecWire(in, syms, v);
        for (uint i = 0; i < v.size(); i++){
            Wire w = v[i] + N;
            if (w != gate_PO) Throw(String) "Fairness constraints are expected to be of type PO (for legacy reasons).";
            Wire w_in = w[0] ^ w.sign;
            change(w, gate_FairCons).init(w_in);
        }

    }else if (eq(name, "fanout_count") && eq(type_, "FanoutCount")){
        to_add.fanout_count = true;

    }else if (eq(name, "fanouts") && eq(type_, "Fanouts")){
        to_add.fanouts = true;

    }else if (eq(name, "mem_info") && eq(type_, "MemInfo")){
        Throw(String) "MemInfo objects not handled yet.";

    }else
        Throw(String) "Invalid name/type pair for embedded object: %_ : %_", name, type_;

    // Finalize:
    while (!in.eof()){
        if (!isWS(*in))
            Throw(String) "Stray characters in object: %_", *in;
        in++;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// NOTE! This parser was lifted and adopted from 'Netlist/Netlist.cc'; it is not written in the
// "Gig" style.


static
GLit constNameToGLit(Array<char> name)
{
    if (name.size() != 1) return GLit_NULL;
    if (name[0] == '0') return GLit_False;
    if (name[0] == '1') return GLit_True;
    if (name[0] == '?') return GLit_Unbound;
    if (name[0] == '!') return GLit_Conflict;
    return GLit_NULL;
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


static
GateType nameToGatetype(const Array<char>& str, uind line_no)
{
    for (uint i = 1; i < GateType_size; i++){
        if (eq(GateType_name[i], str))
            return GateType(i);
    }

    if (eq(str, "Flop"))
        return gate_FF;

    Throw(Excp_ParseError) "[line %_] Unknown gate type: %_", line_no, str;
    return gate_NULL;   // -- Sun wants this...
}


Wire nameToWire(Array<char> name, Gig& N, Map<Str,GLit>& syms, StackAlloc<char>& mem, gate_id& idC, uind line_no)
{
    if (name.size() == 0)
        Throw(Excp_ParseError) "[line %_] Empty name.", line_no;

    bool s = false;
    if (name[0] == '~'){
        s = true;
        name = name.slice(1); }

    GLit p;
    if (!syms.peek(name, p)){
        p = GLit(idC++);
        syms.set(Array_copy(name, mem), p);
    }

    return (p ^ s) + N;
}


void readGig(In& in, /*out*/Gig& N)
{
    assert(N.isEmpty());

    static const Array<cchar> seps(",");
    Vec<char> buf;
    Vec<Array<char> > ns;
    Vec<Array<char> > is;
    WMap<GLit> id_map; // -- during parsing, names are given IDs as they are parsed, but they are later reshuffled to correspond to the order of declarations in the file

    gate_id idC = gid_FirstUser;
    for (uint i = 0; i < idC; i++)
        id_map(GLit(i)) = GLit(i);

    Map<Str,GLit>    syms;
    StackAlloc<char> mem;
    syms.set(slize("0"), GLit_False);
    syms.set(slize("1"), GLit_True);
    syms.set(slize("?"), GLit_Unbound);
    syms.set(slize("!"), GLit_Conflict);
    syms.set(slize("-"), GLit_NULL);
    syms.set(slize("*"), GLit_ERROR);

    uind line_no = 0;

    try {
        // READ GATES:

        for(;;){
            if (!readLine(in, buf, line_no)) break;

            uind eq_i = search(buf, '=');
            if (eq_i == UIND_MAX){
                trim(buf);
                if (buf.size() == 2 && buf[0] == '%' && buf[1] == '%')
                    break;      // -- Pecs will follow
                else
                    Throw(Excp_ParseError) "[line %_] Missing '=' sign.", line_no;
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
                if (i0 >= rhs.size()) Throw(Excp_ParseError) "[line %_] Empty right-hand side.", line_no;
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
                    Throw(Excp_ParseError) "[line %_] Missing ')' in definition.", line_no;
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
                Throw(Excp_ParseError) "[line %_] Extra characters at end of definition.", line_no;

          ParseAttribute:
            assert(rhs[i] == '[');
            i++;
            while (i < rhs.size() && isWS(rhs[i])) i++;
            i0 = i;
            for(;;){
                if (i == rhs.size())
                    Throw(Excp_ParseError) "[line %_] Missing ']' in definition.", line_no;
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
                Throw(Excp_ParseError) "[line %_] Extra characters at end of definition.", line_no;

          Done:
            // Split args into input names:   (stored in 'is[]')
            strictSplitArray(args, seps, is);
            for (uind j = 0; j < is.size(); j++)
                trim(is[j]);

            // Extra names for constants?
            GLit p_const = constNameToGLit(type_);
            GateType t = gate_NULL;
            Wire w;
            if (p_const != GLit_NULL)
                // Get existing constant gate:  (this is a hack: constants are already created before reading, so we need some way of attaching new names)
                w = N[p_const];
            else{
                // Create gate:
                t = nameToGatetype(type_, line_no);
                if (gatetype_size[t] == DYNAMIC_GATE_SIZE)
                    w = N.addDyn(t, is.size());
                else{
                    if (t != gate_FF && t != gate_Lut4 && t != gate_Npn4 && t != gate_Lut6){
                        if (is.size() != gatetype_size[t])
                            Throw(Excp_ParseError) "[line %_] Wrong number of input given to gate (expected %_).", line_no, gatetype_size[t];
                    }
                    if (isNumbered(t) && attr)
                        w = N.add(t, stringToUInt64((Str)attr));
                    else
                        w = N.add(t);
                }
            }

            // Attach names to gate:
            for (uind j = 0; j < ns.size(); j++){
                Wire v = nameToWire(ns[j], N, syms, mem, idC, line_no);
                if (+id_map[v] != Wire_NULL)
                    Throw(Excp_ParseError) "[line %_] Name used twice: %_", line_no, ns[j];
                id_map(v) = w ^ sign(v);
            }

            // Assign inputs:
            for (uind j = 0; j < is.size(); j++){
                if (is[j].size() != 0 && (is[j].size() != 1 || is[j][0] != '-')){
                    Wire v = nameToWire(is[j], N, syms, mem, idC, line_no);
                    w.set_unchecked(j, v);
                }
            }

            // Attributes:
            if (attr && !isNumbered(t)){
                if (t == gate_Lut4){
                    if (attr.size() != 4)
                        Throw(Excp_ParseError) "[line %_] Lut4 attribute must be four hexadecimal digits.", line_no;
                    w.arg_set((fromHex(attr[0]) << 12) | (fromHex(attr[1]) << 8) | (fromHex(attr[2]) << 4) | fromHex(attr[3]));

                // <<== other attributes (Npn4, Lut6 etc) goes here

                }else
                    Throw(Excp_ParseError) "[line %_] Unexpected attribute.", line_no;

            }
        }

        // Translate gate IDs:
        for (gate_id id = gid_FirstUser; id < N.size(); id++){
            Wire w = N[id];
            for (uint j = 0; j < w.size(); j++){
                if (w[j])
                    w.set_unchecked(j, id_map[w[j]] ^ sign(w[j]));
            }
        }

        // Translate names:
        Map<Str,GLit> old_syms;
        syms.moveTo(old_syms);
        For_Map(old_syms){
            GLit w = Map_Value(old_syms); assert(!w.sign);
            syms.set(Map_Key(old_syms), id_map[w]);
        }


        // GUARD SEQUENTIAL ELEMENTS:

        For_Gates(N, w){
            if (w == gate_FF)
                w.set(0, N.add(gate_Seq).init(w[0]));   // -- second input of FF is "reset" and is combinational
            else if (isSeqElem(w)){
                For_Inputs(w, v){
                    w.set(Input_Pin(v), N.add(gate_Seq).init(v)); }
            }
        }


        // READ POBS:

        PobsToAdd to_add;
        for(;;){        // -- an object has the text representation:  <name> ':' <type> '{' '\n' <data> '}'
            if (!readLine(in, buf, line_no)) break;

            // Read header:
            Vec<char> name, type_;
            {
                In h_in(buf);
                skipWS(h_in); assert(!in.eof());

                gets(h_in, name, IsChar(':'));
                if (*h_in != ':') Throw(Excp_ParseError) "[line %_] Invalid object header; missing ':'.", line_no;
                h_in++;
                trim(name);

                gets(h_in, type_, IsChar('{'));
                if (*h_in != '{') Throw(Excp_ParseError) "[line %_] Invalid object header; missing '{'.", line_no;
                h_in++;
                trim(type_);
            }

            // Read body:
            Vec<char> body;
            for(;;){
                if (in.eof()) Throw(Excp_ParseError) "[line %_] Missing '}' at end of object.", line_no;
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

            // Construct Pob:
            In b_in(body);
            try{
                addPob(N, name.slice(), type_.slice(), b_in, syms, to_add);
            }catch (...){
                Str text = slice(b_in);
                if (b_in.tell() > 0){
                    text.sz = b_in.tell();
                    trimEnd(text);
                    line_no += countLineNo(text, text.sz);
                }
                throw;
            }
            line_no += countLineNo(slice(body), body.size()) - 1;
        }

        if (to_add.strash){
            uint n_gates = N.count();
            N.strash();
            if (n_gates != N.count())
                Throw(String) "Netlist not properly strashed while containing a 'strash' object.";
        }

        if (to_add.fanout_count){
            Add_Gob(N, FanoutCount);
        }

        if (to_add.fanouts){
            N.is_frozen = true;
            Add_Gob(N, Fanouts);
        }

    }catch (Excp_ParseError){
        N.clear();
        throw;

    }catch (String msg){
        N.clear();
        Throw(Excp_ParseError) "[line %_] %_", line_no, msg;

    }catch (...){
        N.clear();
        Throw(Excp_ParseError) "[line %_] Syntax error.", line_no;
    }


}


void readGigFile(String filename, Gig& N)
{
    InFile in(filename);
    if (in.null())
        throw Excp_ParseError(String("Could not open: ") + filename);

    readGig(in, N);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
