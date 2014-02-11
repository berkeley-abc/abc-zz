//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.cc
//| Author(s)   : Niklas Een
//| Module      : Verilog
//| Description : Parser for a subset of structural Verilog.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Parser.hh"
#include "ZZ/Generics/RefC.hh"

//#define PARSER_DEBUG

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Global variables:


cchar* verilog_module_output_prefix = "out`";
cchar* verilog_instance_prefix = "inst`";


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Supporting Types:


enum VerilogParser_TokType {
    tok_NULL,

    tok_Ident,
    tok_PreProc,
    tok_String,
    tok_Num,

    tok_LParen,
    tok_RParen,
    tok_LBrack,
    tok_RBrack,
    tok_LBrace,
    tok_RBrace,
    tok_Comma,
    tok_Semi,
    tok_Period,
    tok_Colon,
    tok_At,
    tok_Assign,
    tok_Hash,
    tok_Quest,

    tok_Neg,        // -- bit operators
    tok_And,
    tok_Or,
    tok_Xor,
    tok_Xnor,

    tok_Pseudo,     // -- not a real token, but a quick hack to handle "1'bx"

    TokType_size
};
cchar* VerilogParser_TokType_name[TokType_size] = {
    "<null>",
    "Ident",
    "PreProc",
    "String",
    "Num",
    "LParen",
    "RParen",
    "LBrack",
    "RBrack",
    "LBrace",
    "RBrace",
    "Comma",
    "Semi",
    "Period",
    "Colon",
    "At",
    "Assign",
    "Hash",
    "Quest",
    "Neg",
    "And",
    "Or",
    "Xor",
    "Xnor",
    "Pseudo",
};

macro cchar* tokName(VerilogParser_TokType t) { return VerilogParser_TokType_name[t]; }
macro cchar* tokName(uint                  t) { return VerilogParser_TokType_name[t]; }


struct VerilogParser_Tok {
    uchar   type;
    ushort  len;
    uint    offset;
    VerilogParser_Tok(uchar type_, ushort len_, uint offset_) : type(type_), len(len_), offset(offset_) {}
};


struct VerilogParser_Loc {
    String file;
    uint   line;
    VerilogParser_Loc(String file_, uint line_) : file(file_), line(line_) {}
};


template<> fts_macro void write_(Out& out, const VerilogParser_Loc& v)
{
    FWrite(out) "%_:%_", v.file, v.line;
}


//=================================================================================================
// -- Verilog Expressions:


/*
Currently we support expressions of the type:

    expr ::= pseudo-input
           | sig-name
           | sig-name '[' simple-num ']'
           | sig-name '[' simple-num ':' simple-num ']'
           | const-bit-vec                  -- eg. from "100'b111000...0101"
           | concat-list-of-expr            -- concat op., eg. from "{ a, b, 1b1 }"
           | assign-expr                    -- including '&'s, '~'s and '? :'s

    assign-expr ::= pseudo                  -- the string: 1'bx
                  | '~' expr
                  | expr '&' expr
                  | expr '?' expr ':' expr

The "assign expressions" only occur in the RHS of continuous assignments, so they need not be
handled elsewhere.

Example Expressions:
    n_306
    pc[8]
    1'b1
    {t6_, t5_, t4_, t3_}
    n12 ? n38[3] : 1'bx
    n435 = n401 & ~n408

We don't support hierarchical names (no '.' in expressions).
*/


/*
Expressions are constructed through one of:

    VExpr_Var   (Str sig_name)
    VExpr_Bitsel(Str array_name, uint idx)
    VExpr_Slice (Str array_name, uint lft, uint rht)
    VExpr_Const (Str bin_text)
    VExpr_Concat(const Vec<VExpr>& elems)

    VExpr_Pseudo()
    VExpr_And(VExpr expr0, VExpr expr1)
    VExpr_ITE(VExpr sel, VExpr tt, VExpr ff)

There is one expression mutator (use ONLY during construction of the expression):

    VExpr_NegBang(VExpr& expr)              -- changes the sign of an expression

and accessed through selector functions:

    VExprType   type (VExpr e)              -- (see 'VExprType' below)
    bool        sign (VExpr e)              -- TRUE = inverter, FALSE = no inverter
    Str         name (VExpr e)              -- Var, Bitsel
    uint        index(VExpr e)              -- Bitsel
    (uint,uint) range(VExpr e)              -- Slice
    uint        nbits(VExpr e)              -- Const
    bool        bit  (VExpr e, uint idx)    -- Const
    uint        size (VExpr e)              -- Concat, And, ITE
    VExpr       elem (VExpr e, uint idx)    -- Concat, And, ITE
*/


enum VExprType {
    vx_NULL,
    vx_Pseudo,
    vx_Var,
    vx_Bitsel,
    vx_Slice,
    vx_Const,
    vx_Concat,
    vx_And,
    vx_Mux,
    VExprType_size
};


macro bool isComposite(VExprType type) { return type >= vx_Concat; }


struct VExpr_data {
    uchar  type;
    uchar  sign;
    ushort name_sz;     // size of 'sig_name' (only used for 'Var' and 'Bitsel')
    uint   extra;       // one of: bit selection index, constant width (in bits), number of concat elements
    uint   extra2;      // used for slices (second range value)
    uint   refC;

    union {
        cchar* sig_name;        // -- NOT owned by this data structure (points into preprocessed program text)
        uint*  const_bits;      // -- owned by this data structure
        void*  elems;           // -- owned by this data structure, really of type 'VExpr*'
    };


    VExpr_data() { memset(this, 0, sizeof(*this)); }
   ~VExpr_data();
};


typedef RefC<VExpr_data> VExpr;


VExpr_data::~VExpr_data()
{
    if (type == vx_Const)
        xfree(const_bits);
    else if (isComposite((VExprType)type)){
        VExpr* data = (VExpr*)elems;
        for (uint i = 0; i < extra; i++)
            data[i].~VExpr();
        xfree(data);
    }
}


static void VExpr_NegBang(VExpr& expr) ___unused;
static void VExpr_NegBang(VExpr& expr)
{
    expr->sign = !expr->sign;
}


static VExpr VExpr_NULL() ___unused;
static VExpr VExpr_NULL()
{
    VExpr_data* vx = new VExpr_data;
    vx->type     = vx_NULL;
    return VExpr(vx);
}


static VExpr VExpr_Pseudo() ___unused;
static VExpr VExpr_Pseudo()
{
    VExpr_data* vx = new VExpr_data;
    vx->type     = vx_Pseudo;
    return VExpr(vx);
}


// 'sig_name' is the name of a signal (either a single bit or a bit-array)
static VExpr VExpr_Var(Str sig_name) ___unused;
static VExpr VExpr_Var(Str sig_name)
{
    assert(sig_name.size() <= 65535);

    VExpr_data* vx = new VExpr_data;
    vx->type     = vx_Var;
    vx->name_sz  = sig_name.size();
    vx->sig_name = sig_name.base();
    return VExpr(vx);
}


// 'array_name' is name of a bit-array, 'idx' the constant index into that array (selecting a bit)
static VExpr VExpr_Bitsel(Str array_name, uint idx) ___unused;
static VExpr VExpr_Bitsel(Str array_name, uint idx)
{
    assert(array_name.size() <= 65535);

    VExpr_data* vx = new VExpr_data;
    vx->type     = vx_Bitsel;
    vx->name_sz  = array_name.size();
    vx->sig_name = array_name.base();
    vx->extra    = idx;
    return VExpr(vx);
}


// 'bin_text' is a binary constant in ASCII format without prefix (but possibly with underscores,
// which are ignored). Example: "1100_1010" (rightmost bit is LSB).
static VExpr VExpr_Const(Str bin_text) ___unused;
static VExpr VExpr_Const(Str bin_text)
{
    uint underscores = 0;
    for (uint i = 0; i < bin_text.size(); i++)
        underscores += (bin_text[i] == '_') ? 1 : 0;

    uint width = bin_text.size() - underscores;
    uint asize = (width + 31) / 32;

    VExpr_data* vx = new VExpr_data;
    vx->type       = vx_Const;
    vx->const_bits = xmalloc<uint>(asize);
    vx->extra      = width;

    uint* data = vx->const_bits;
    uint  off = 0;
    uint  bit = 0;
    for (uint i = 0; i < asize; i++)
        data[i] = 0;
    for (uint i = 0; i < bin_text.size(); i++){
        if (bin_text[i] != '_'){
            if (bin_text[i] == '1')
                data[off] |= 1 << bit;
            else
                assert(bin_text[i] == '0');

            bit++;
            if (bit == 32){
                off++;
                bit = 0;
            }
        }
    }

    return VExpr(vx);
}


static VExpr_data* VExpr_Composite(const Array<const VExpr>& elems, VExprType type) ___unused;
static VExpr_data* VExpr_Composite(const Array<const VExpr>& elems, VExprType type)
{
    VExpr* data = xmalloc<VExpr>(elems.size());

    VExpr_data* vx = new VExpr_data;
    vx->type  = type;
    vx->elems = data;
    vx->extra = elems.size();

    for (uint i = 0; i < elems.size(); i++)
        new (&data[i]) VExpr(elems[i]);

    return vx;
}


static VExpr VExpr_Concat(const Vec<VExpr>& elems) ___unused;
static VExpr VExpr_Concat(const Vec<VExpr>& elems)
{
    return VExpr(VExpr_Composite(elems.slice(), vx_Concat));
}


static VExpr VExpr_And(const VExpr& e0, const VExpr& e1) ___unused;
static VExpr VExpr_And(const VExpr& e0, const VExpr& e1)
{
    VExpr es[2];
    es[0] = e0;
    es[1] = e1;
    return VExpr(VExpr_Composite(Array_new(es, 2), vx_And));
}


static VExpr VExpr_Mux(const VExpr& sel, const VExpr& tt, const VExpr& ff) ___unused;
static VExpr VExpr_Mux(const VExpr& sel, const VExpr& tt, const VExpr& ff)
{
    VExpr es[3];
    es[0] = sel;
    es[1] = tt;
    es[2] = ff;
    return VExpr(VExpr_Composite(Array_new(es, 3), vx_Mux));
}


// 'array_name' is name of a bit-array, 'idx' the constant index into that array (selecting a bit)
static VExpr VExpr_Slice(Str array_name, uint lft, uint rht) ___unused;
static VExpr VExpr_Slice(Str array_name, uint lft, uint rht)
{
    assert(array_name.size() <= 65535);

    VExpr_data* vx = new VExpr_data;
    vx->type     = vx_Slice;
    vx->name_sz  = array_name.size();
    vx->sig_name = array_name.base();
    vx->extra    = lft;
    vx->extra2   = rht;
    return VExpr(vx);
}


macro VExprType       type (const VExpr& e)           { return (VExprType)e->type; }
macro bool            sign (const VExpr& e)           { return e->sign; }
macro Str             name (const VExpr& e)           { assert(type(e) == vx_Var || type(e) == vx_Bitsel || type(e) == vx_Slice); return Str(e->sig_name, e->name_sz); }
macro uint            index(const VExpr& e)           { assert(type(e) == vx_Bitsel); return e->extra; }
macro Pair<uint,uint> range(const VExpr& e)           { assert(type(e) == vx_Slice); return tuple(e->extra, e->extra2); }
macro uint            nbits(const VExpr& e)           { assert(type(e) == vx_Const); return e->extra; }
macro bool            bit  (const VExpr& e, uint idx) { assert(type(e) == vx_Const);  assert(idx < e->extra); return (e->const_bits[idx / 32] >> (idx & 31)) & 1; }
macro uint            size (const VExpr& e)           { assert(isComposite(type(e))); return e->extra; }
macro VExpr           elem (const VExpr& e, uint idx) { assert(isComposite(type(e))); assert(idx < e->extra); return ((VExpr*)e->elems)[idx]; }


template<> fts_macro void write_(Out& out, const VExpr& e)
{
    if (!e)
        FWrite(out) "<null>";
    else{
        if (e->sign)
            FWrite(out) "~";

        switch (type(e)){
        case vx_NULL:
            FWrite(out) "<null>";
            break;
        case vx_Pseudo:
            FWrite(out) "<pseudo>";
            break;
        case vx_Var:
            FWrite(out) "%_", name(e);
            break;
        case vx_Bitsel:
            FWrite(out) "%_[%_]", name(e), index(e);
            break;
        case vx_Slice:
            FWrite(out) "%_[%_:%_]", name(e), range(e).fst, range(e).snd;
            break;
        case vx_Const:
            for (uint i = nbits(e); i > 0;) i--,
                out += (bit(e, i) ? '1' : '0');
            break;
        case vx_Concat:
            /*note: concat can also be preceded by a simple number for replication*/
            out += '{';
            for (uint i = 0; i < size(e); i++){
                if (i != 0) out += ';', ' ';
                out += elem(e, i); }
            out += '}';
            break;
        case vx_And:
            FWrite(out) "(%_ & %_)", elem(e, 0), elem(e, 1);
            break;
        case vx_Mux:
            FWrite(out) "(%_ ? %_ : %_)", elem(e, 0), elem(e, 1), elem(e, 2);
            break;
        default: assert(false); }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Class 'VerilogParser':


struct ParserListener;

class VerilogParser {
  //________________________________________
  //  Local types:

    typedef VerilogParser_TokType TokType;
    typedef VerilogParser_Tok     Tok;
    typedef VerilogParser_Loc     Loc;

    enum { BEFORE_MATCH, UNDER_MATCH, AFTER_MATCH };

  //________________________________________
  //  Internal state:

    Vec<String>      pp_files;  // A list of files currently being read (to avoid cyclic includes)
    Map<String,uint> pp_sym;    // Defined preprocessor symbols (with counter).
    Vec<uchar>       pp_nest;   // Each element corresponds to a nested `ifdef and describes its state.

    Vec<char>   text;   // Contains text after preprocessing
    Vec<Tok>    toks;   // Tokenized input; contain offsets into 'text'

  //________________________________________
  //  Major internal methods:

    void preprocess(String filename, String from_file = "", uint from_line = 0);
    void preprocessText(String filename, Array<char> raw);  // -- parse 'raw' (using 'filename' only for error messages)
    void tokenize();
    void parse(Vec<VerilogModule>& result);

  //________________________________________
  //  Internal helpers:

    Loc  getLocation(cchar* pos) const;
    void throwIllegalChar(cchar* p) const;

    void symDefine  (Str sym);
    bool symUndefine(Str sym);
    bool symExists  (Str arg) const;

    Pair<uint,uint> parseOptionalRange  (Tok*& p);
    void            parseListOfVariables(Tok*& p, /*out:*/Vec<Str>& vars);
    VExpr           parseExpr           (Tok*& p);
    VExpr           parseSimpleExpr     (Tok*& p);  // -- no: Mux, And, Neg
    Pair<Str,VExpr> parseConnect        (Tok*& p);

    void parse(ParserListener& pl);

public:
  //________________________________________
  //  Public interface:

    bool          store_names;
    VerilogErrors error_levels;

    VerilogParser() : store_names(true) {}

    void read(String filename, /*out*/Vec<VerilogModule>& result, /*in*/Array<char> prelude);

  //________________________________________
  //  Debug:

    void dumpTokens();
};



//=================================================================================================
// -- Helpers:


macro bool validDigit(char c, char base)
{
    if      (base == 'd') return (c >= '0' && c <= '9');
    else if (base == 'b') return c == '0' || c == '1';
    else if (base == 'h') return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    else if (base == 'o') return (c >= '0' && c <= '7');
    assert(false);
}


// Check that 'arg' is a valid Verilog identifier (same as a C identifier except for allowing '$'
// as a non-first character).
macro bool validIdent(Str arg)
{
    if (arg.size() == 0) return false;
    if (!isIdentChar0(arg[0])) return false;
    for (uind i = 1; i < arg.size(); i++)
        if (!isIdentChar(arg[i]) && arg[i] != '$')
            return false;
    return true;
}


// Returns TRUE if 'text' is a simple, unsized number '[0-9]+'; no sign, no base, no size.
macro bool simpleNumber(Str text)
{
    for (uind i = 0; i < text.size(); i++)
        if (!isDigit(text[i]))
            return false;
    return true;
}


VerilogParser_Loc VerilogParser::getLocation(cchar* pos) const
{
    String file  = "";
    uint   line  = 1;
    cchar* p     = text.base();

    for(;;){
        if (p >= pos)
            return Loc(file, line);
        if (*p == 0)
            break;

        if (*p == '`'){
            // Line statement:
            assert(p == text.base() || p[-1] == '\n');      // -- should be guaranteed by preprocessor

            // Get line number:
            p += 2;
            try{
                line = parseUInt(p);
            }catch (Excp_ParseNum){
                assert(false);      // -- preprocessor should make sure this line is syntactically correct...
            }

            // Get file name:
            assert(*p == ' ' && p[1] == '"');
            p += 2;
            cchar* q = p;
            while (*q != '"'){ assert(*q != 0); q++; } assert(q[1] == '\n');
            file = slice(*p, *q);
            p = q + 2;    // -- skip over the newline

        }else{
            if (*p == '\n') line++;
            p++;
        }
    }
    assert(false);
}


void VerilogParser::throwIllegalChar(cchar* p) const
{
    if (*p < 32)
        throw Excp_ParseError((FMT "[%_] Illegal character: <%.2X>", getLocation(p), (uchar)*p));
    else
        throw Excp_ParseError((FMT "[%_] Illegal character: %_", getLocation(p), *p));
}


void VerilogParser::symDefine(Str arg)
{
    uint* count;
    if (pp_sym.get(arg, count))
        *count += 1;
    else
        *count = 1;
};


// Returns TRUE if successful, FALSE if symbol is already undefined.
bool VerilogParser::symUndefine(Str arg)
{
    uint* count;
    if (pp_sym.get(arg, count)){
        *count -= 1;
        if (*count == 0)
            pp_sym.exclude(arg);
        return true;
    }else
        return false;

};


bool VerilogParser::symExists(Str arg) const
{
    uint* count;
    if (pp_sym.peek(arg, count)){
        assert(*count > 0);
        return true;
    }else
        return false;
}


//=================================================================================================
// -- Public methods:


void VerilogParser::read(String filename, /*out*/Vec<VerilogModule>& result, /*in*/Array<char> prelude)
{
    if (prelude)
        preprocessText("<prelude>", prelude);
    preprocess(filename);
    tokenize();
    parse(result);
}


//=================================================================================================
// -- Preprocessor:


// NOTE! The special comment "// Verific Verilog Description of OPERATOR" will be replaced
// by a fictitious keyword '__verific_operator__'. Same thing for '// [-BLACK BOX-]'
// (becomes '__black_box__').

// Replace comments with spaces. Returns FALSE if 'text' contains an unterminated block comment.
static bool removeComments(Array<char> text)
{
    char* p   = &text[0];
    char* end = &text.end_();

    while (p != end){
        if (*p == '"'){
            // Scan for end of string; ignore \":
            p++;
            bool ignore = false;
            for(;;){
                if (p == end)
                    return true;        // -- unterminated string (will be caught by lexer)
                else if (*p == '\\'){
                    p++;
                    ignore = true;
                }else if (*p == '"' && !ignore){
                    p++;
                    break;
                }else{
                    p++;
                    ignore = false;
                }
            }

        }else if (*p == '/' && p+1 != end && p[1] == '/'){
            // Check for special Verific or Black-box comments:
            if (p + 42 < end && strncmp(p, "// Verific Verilog Description of OPERATOR", 42) == 0){
                strcpy(p, "__verific_operator__");
                p += 20;
            }

            if (p + 16 < end && strncmp(p, "// [-BLACK BOX-]", 16) == 0){
                strcpy(p, "__black_box__");
                p += 13;
            }

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


void VerilogParser::preprocess(String file_, String from_file, uint from_line)
{
    String file = (file_[0] == '/') ? file_ : (dirName(from_file) + "/" + file_);

    for (uind i = 0; i < pp_files.size(); i++)
        if (eq(file, pp_files[i]))
            throw Excp_ParseError((FMT "[%_:%_] Recursive include statement.", from_file, from_line));
    pp_files.push(file);

    Array<char> raw = readFile(file);
    if (!raw){
        if (from_file != "") throw Excp_ParseError((FMT "[%_:%_] Missing include file: %_", from_file, from_line, file));
        else                 throw Excp_ParseError((FMT "Could not open: %_", file));
    }

    preprocessText(file, raw);
    dispose(raw);

    pp_files.pop();
}


void VerilogParser::preprocessText(String file, Array<char> raw)
{
    #define Push_Location(line, file)            \
        tmp.clear();                             \
        FWrite(tmp) "` %_ \"%_\"\n", line, file; \
        append(text, tmp);
    String tmp;

    if (!removeComments(raw))
        throw Excp_ParseError((FMT "Unterminated multi-line comment in file: %_", file));

    Push_Location(1, file);

    uint      line = 0;
    uint      nominal_line = 0;         // -- for `line statements
    String    nominal_file = file;
    cchar*    p   = &raw[0];
    cchar*    end = &raw.end_();
    while (p != end){
        // Scan one line:
        cchar* q = p;
        for(;;){
            if (q == end || *q == '\n') break;
            q++;
        }
        Str buf = slice(*p, *q);
        p = q;
        if (*p == '\n') p++;
        trimStr(buf);

        line++;
        nominal_line++;

        // Parse the line:
        bool was_line_statement = false;
        if (buf.size() > 0 && buf[0] == '`'){
            // Preprocessor directive:
            uind i = 1;
            while (i < buf.size() && isAlpha(buf[i])) i++;      // -- if we supported macros, 'isAlpha' would not be enough here
            Str cmd = buf.slice(1, i);

            while (i < buf.size() && isWS(buf[i])) i++;
            Str arg = buf.slice(i);

            if (pp_nest.size() == 0 || pp_nest.last() == UNDER_MATCH){
                if (eq(cmd, "include")){
                    if (arg[0] != '"' || arg.last() != '"')
                        throw Excp_ParseError((FMT "[%_:%_] Invalid include name: %_", file, line, arg));
                    arg.data++;
                    arg.sz -= 2;

                    Push_Location(1, arg);
                    preprocess(arg, file, line);
                    Push_Location(nominal_line, nominal_file);

                }else if (eq(cmd, "define")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `define: %_", file, line, arg));
                    symDefine(arg);

                }else if (eq(cmd, "undef")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `undef: %_", file, line, arg));
                    if (!symUndefine(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Symbol not defined in `undef: %_", file, line, arg));

                }else if (eq(cmd, "ifdef")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `ifdef: %_", file, line, arg));
                    pp_nest.push(symExists(arg) ? UNDER_MATCH : BEFORE_MATCH);

                }else if (eq(cmd, "ifndef")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `ifndef: %_", file, line, arg));
                    pp_nest.push(symExists(arg) ? BEFORE_MATCH : UNDER_MATCH);

                }else if (eq(cmd, "elsif")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `elsif: %_", file, line, arg));
                    if (pp_nest.size() == 0)
                        throw Excp_ParseError((FMT "[%_:%_] Misplaced `elsif.", file, line));
                    pp_nest.last() = AFTER_MATCH;

                }else if (eq(cmd, "else")){
                    if (pp_nest.size() == 0)
                        throw Excp_ParseError((FMT "[%_:%_] Misplaced `else.", file, line));
                    pp_nest.last() = AFTER_MATCH;

                }else if (eq(cmd, "endif")){
                    if (pp_nest.size() == 0)
                        throw Excp_ParseError((FMT "[%_:%_] Misplaced `endif.", file, line));
                    pp_nest.pop();

                }else if (eq(cmd, "line")){     // <line-no> [ "<file>" ]
                    Vec<Str> fields;
                    splitArray(arg, " \t", fields);
                    if (fields.size() == 0 || fields.size() > 2)
                        throw Excp_ParseError((FMT "[%_:%_] Line statements take 1 or 2 arguments.", file, line));
                    try{
                        nominal_line = stringToUInt64(fields[0]);
                    }catch (Excp_ParseNum){
                        throw Excp_ParseError((FMT "[%_:%_] Invalid line given in line statements: %_", file, line, fields[0]));
                    }
                    if (fields.size() == 2){
                        if (fields[1][0] != '"' || fields[1].last() != '"')
                            throw Excp_ParseError((FMT "[%_:%_] Invalid file name given in line statement: %_", file, line, fields[1]));
                        fields[1].data++;
                        fields[1].sz -= 2;
                        nominal_file = fields[1];
                    }
                    Push_Location(nominal_line, nominal_file);
                    was_line_statement = true;
                    nominal_line--;

                }else{
                    throw Excp_ParseError((FMT "[%_:%_] Macro call or unsupported directive: %_", file, line, cmd));
                }

            }else if (pp_nest.last() == BEFORE_MATCH){
                if (eq(cmd, "ifdef") || eq(cmd, "ifndef")){
                    pp_nest.push(AFTER_MATCH);  // -- skip recursive ifdefs

                }else if (eq(cmd, "elsif")){
                    if (!validIdent(arg))
                        throw Excp_ParseError((FMT "[%_:%_] Only simple symbols supported in `elsif: %_", file, line, arg));
                    if (symExists(arg))
                        pp_nest.last() = UNDER_MATCH;

                }else if (eq(cmd, "else")){
                    pp_nest.last() = UNDER_MATCH;

                }else if (eq(cmd, "endif")){
                    pp_nest.pop();
                }

            }else if (pp_nest.last() == AFTER_MATCH){
                if (eq(cmd, "ifdef") || eq(cmd, "ifndef")){
                    pp_nest.push(AFTER_MATCH);  // -- skip recursive ifdefs
                }else if (eq(cmd, "endif")){
                    pp_nest.pop();
                }
            }

        }else if (pp_nest.size() == 0 || pp_nest.last() == UNDER_MATCH){
            // Program text:
            for (uind i = 0; i < buf.size(); i++){
                char c = buf[i];
                if (c == 0)
                    throw Excp_ParseError((FMT "[%_:%_] Input file cannot contain characer 0.", file, line));
                if (c == '`')
                    throw Excp_ParseError((FMT "[%_:%_] Stray preprocessor character: `", file, line));
                text.push(c);
            }
        }

        if (!was_line_statement)    // -- special case where the "`line [...]" line doesn't count toward the line number
            text.push('\n');
    }

    #undef Push_Location
}


//=================================================================================================
// -- Tokenizer:


void VerilogParser::tokenize()
{
    if (text.last() != 0)
        text.push(0);

    cchar* p = text.base();
    cchar* p0 = p;
    while (*p){
        if (isWS(*p)){
            p++;

        }else if (*p == '`' && p[1] == ' '){
            p += 2;
            while (*p != '\n') p++;
            p++;

        }else if (isIdentChar0(*p)){
            // Parse identifier: ([a-zA-Z_] [a-zA-Z_0-9$]*)
            cchar* q = p+1;
            while (isIdentChar(*q) || *q == '$') q++;
            toks.push(Tok(tok_Ident, q-p, p-p0));
            p = q;

        }else if (*p == '\\'){
            // Parse quoted identifier:
            p++;
            cchar* q = p;
            while (*q && !isWS(*q)){
                if (*q < 32) throwIllegalChar(q);
                q++; }
            toks.push(Tok(tok_Ident, q-p, p-p0));
            p = q;

        }else if (isDigit(*p)){
            if (p[0] == '1' && p[1] == '\'' && p[2] == 'b' && (p[3] == 'x' || p[3] == 'X')){
                // Ugly hack to handle "1'bx" which is used by Verific.
                toks.push(Tok(tok_Pseudo, 4, p-p0));
                p += 4;
            }else{
                // Parse number: (either "123" or "16'd123", where base 'd' is one of: d, h, o, b)
                cchar* q = p+1;
                while (isDigit(*q) || *q == '_') q++;
                if (*q == '\''){
                    q++;
                    if (*q != 'd' && *q != 'h' && *q != 'o' && *q != 'b') throwIllegalChar(q);
                    char base = * q;
                    q++;
                    while (*q == '_' || validDigit(*q, base)) q++;
                    if (isAlpha(*q) || isDigit(*q)) throwIllegalChar(q);
                    /*might want to allow 'x' and 'z' here at some point...*/
                }
                toks.push(Tok(tok_Num, q-p, p-p0));
                p = q;
            }

        }else{
            switch (*p){
            case '(': toks.push(Tok(tok_LParen, 1, p-p0)); break;
            case ')': toks.push(Tok(tok_RParen, 1, p-p0)); break;
            case '[': toks.push(Tok(tok_LBrack, 1, p-p0)); break;
            case ']': toks.push(Tok(tok_RBrack, 1, p-p0)); break;
            case '{': toks.push(Tok(tok_LBrace, 1, p-p0)); break;
            case '}': toks.push(Tok(tok_RBrace, 1, p-p0)); break;
            case ',': toks.push(Tok(tok_Comma , 1, p-p0)); break;
            case ';': toks.push(Tok(tok_Semi  , 1, p-p0)); break;
            case '.': toks.push(Tok(tok_Period, 1, p-p0)); break;
            case ':': toks.push(Tok(tok_Colon , 1, p-p0)); break;
            case '@': toks.push(Tok(tok_At    , 1, p-p0)); break;
            case '=': toks.push(Tok(tok_Assign, 1, p-p0)); break;
            case '#': toks.push(Tok(tok_Hash  , 1, p-p0)); break;
            case '?': toks.push(Tok(tok_Quest , 1, p-p0)); break;

            case '&':
                if (p[1] == '&') throwIllegalChar(p+1);     // -- don't support '&&' operator yet
                toks.push(Tok(tok_And, 1, p-p0));
                break;
            case '|':
                if (p[1] == '|') throwIllegalChar(p+1);     // -- don't support '||' operator yet
                toks.push(Tok(tok_Or, 1, p-p0));
                break;
            case '~':
                if (p[1] == '&' || p[1] == '|') throwIllegalChar(p+1);     // -- don't support '~&' or '~|' operator yet
                if (p[1] == '^'){
                    toks.push(Tok(tok_Xnor, 2, p-p0));
                    p++;
                }else
                    toks.push(Tok(tok_Neg, 1, p-p0));
                break;
            case '^':
                if (p[1] == '~'){
                    toks.push(Tok(tok_Xnor, 2, p-p0));
                    p++;
                }else
                    toks.push(Tok(tok_Xor, 1, p-p0));
                break;

            default: throwIllegalChar(p); }
            p++;
        }
    }

    toks.push(Tok(tok_NULL, 0, p-p0));      // -- add terminator token
}


//=================================================================================================
// -- Parser: Basic types


enum VItemType {
    i_NULL,
    i_Arg,        // -- formal argument declaration
    i_Net,        // -- net declaration
    i_Assign,     // -- continuous assignment
    i_Gate,       // -- gate instantiation
    i_Unsupp,     // -- recognized but unsupported symbol "module item"
    i_End,        // -- Fictitious item ("endmodule") for faster parsing
    VSymType_size
};

enum VArgKind {
    a_NULL,
    a_Input,
    a_Output,
    a_Unsupp,
    VArgKind_size
};

enum VNetKind {
    n_NULL,
    n_Wire,
    n_Unsupp,
    VNetKind_size
};

enum VGateKind {
    g_NULL,
    g_And,
    g_Nand,
    g_Or,
    g_Nor,
    g_Xor,
    g_Xnor,
    g_Not,
    g_Buf,
    g_Unsupp,
    VGateKind_size
};

struct ModuleItem {
    cchar*  sym;    // Symbol
    uint    item;   // Item type
    uint    sub;    // Sub-type
};


static const ModuleItem module_items[] = {
    { "input"    , i_Arg    , a_Input  },
    { "output"   , i_Arg    , a_Output },
    { "inout"    , i_Arg    , a_Unsupp },

    { "wire"     , i_Net    , n_Wire   },
    { "tri"      , i_Net    , n_Unsupp },
    { "tri1"     , i_Net    , n_Unsupp },
    { "supply0"  , i_Net    , n_Unsupp },
    { "wand"     , i_Net    , n_Unsupp },
    { "triand"   , i_Net    , n_Unsupp },
    { "tri0"     , i_Net    , n_Unsupp },
    { "supply1"  , i_Net    , n_Unsupp },
    { "wor"      , i_Net    , n_Unsupp },
    { "trior"    , i_Net    , n_Unsupp },
    { "trireg"   , i_Net    , n_Unsupp },

    { "assign"   , i_Assign , 0        },

    { "and"      , i_Gate   , g_And    },
    { "nand"     , i_Gate   , g_Nand   },
    { "or"       , i_Gate   , g_Or     },
    { "nor"      , i_Gate   , g_Nor    },
    { "xor"      , i_Gate   , g_Xor    },
    { "xnor"     , i_Gate   , g_Xnor   },
    { "not"      , i_Gate   , g_Not    },
    { "buf"      , i_Gate   , g_Buf    },
    { "bufif0"   , i_Gate   , g_Unsupp },
    { "bufif1"   , i_Gate   , g_Unsupp },
    { "notif0"   , i_Gate   , g_Unsupp },
    { "notif1"   , i_Gate   , g_Unsupp },
    { "pulldown" , i_Gate   , g_Unsupp },
    { "pullup"   , i_Gate   , g_Unsupp },
    { "nmos"     , i_Gate   , g_Unsupp },
    { "rnmos"    , i_Gate   , g_Unsupp },
    { "pmos"     , i_Gate   , g_Unsupp },
    { "rpmos"    , i_Gate   , g_Unsupp },
    { "cmos"     , i_Gate   , g_Unsupp },
    { "rcmos"    , i_Gate   , g_Unsupp },
    { "tran"     , i_Gate   , g_Unsupp },
    { "rtran"    , i_Gate   , g_Unsupp },
    { "tranif0"  , i_Gate   , g_Unsupp },
    { "rtranif0" , i_Gate   , g_Unsupp },
    { "tranif1"  , i_Gate   , g_Unsupp },
    { "rtranif1" , i_Gate   , g_Unsupp },

    { "parameter", i_Unsupp , 0        },
    { "reg"      , i_Unsupp , 0        },
    { "time"     , i_Unsupp , 0        },
    { "integer"  , i_Unsupp , 0        },
    { "real"     , i_Unsupp , 0        },
    { "event"    , i_Unsupp , 0        },
    { "defparam" , i_Unsupp , 0        },
    { "specify"  , i_Unsupp , 0        },
    { "initial"  , i_Unsupp , 0        },
    { "always"   , i_Unsupp , 0        },
    { "task"     , i_Unsupp , 0        },
    { "function" , i_Unsupp , 0        },

    { "endmodule", i_End    , 0        },
};



//=================================================================================================
// -- Parser: Listener


typedef Pair<uint, uint> VRange;
typedef Pair<Str, VExpr> VConn;

struct VType {
    VArgKind kind;
    VRange   range;

    VType(VArgKind kind_ = a_NULL, VRange range_ = VRange(0,0)) : kind(kind_), range(range_) {}
};

template<> fts_macro void write_(Out& out, const VType& v)
{
    if      (v.kind == a_NULL  ) FWrite(out) "<null>";
    else if (v.kind == a_Input ) FWrite(out) "input";
    else if (v.kind == a_Output) FWrite(out) "output";
    else if (v.kind == a_Unsupp) FWrite(out) "<unsupp>";
    else assert(false);

    FWrite(out) "[%_:%_]", v.range.fst, v.range.snd;
}


struct ParserListener {
    enum Mode { FULL, ARGS_ONLY, SKIP };

    virtual Mode newModule(Str mod_name, const Vec<Str>& formals, bool verific_op, bool black_box) = 0;
        // -- Marks the beginning of a new module definition. Return value determines parse mode.

    virtual void endModule() {}
        // -- Reached the end of a module.

    virtual void argDecl(VArgKind kind, VRange range, const Vec<Str>& vars) {}
        // -- Type declaration of a formal ("input, output, inout").

    virtual void netDecl(VNetKind kind, VRange range, const Vec<Str>& vars) {}
        // -- Type declaration of a module-local net ("wire").

    virtual void instance(Str mod_name, Str inst_name, const Vec<VConn>& conns) {}
        // -- Module instantiation. Either all connections should be named or unnamed,
        // but this is not checked for by parser. If unnamed, the 'Str' part 'of 'conns'
        // is the NULL array 'Str()'.

    virtual void assign(Str lhs_name, uint lhs_index, const VExpr& rhs) {}
        // -- Continuous assignment. If 'lhs_index == UINT_MAX', then there is no array indexing.

    virtual void gate(VGateKind type, Str name, const Vec<VExpr>& args) {}
        // -- Gate instantiation.

    virtual void end() {}
        // -- Reached the end of the file.
};


//=================================================================================================
// -- Parser: Listener -- Base class for Scan and Build


struct VArgData {
    Map<Str,uint> name2id;    // -- Argument name to local ID (position in list of formals).
    Vec<Str>      id2name;    // -- Reverse of above.
    Vec<VType>    id2type;    // -- Input or output + range.
    Vec<uint>     id2num;     // -- Map "position in formals" to input# or output# (depending on type).
    Vec<uint>     inum2id;    // -- Reverse for above (inputs).
    Vec<uint>     onum2id;    // -- Reverse for above (outputs).

    bool verific_op;    // -- Is this module a Verific operator?
    uint black_box;     // -- Is this module black boxed? (0=no, 1=yes, explicit, 2=yes, implicit)
};


struct PL_Base : ParserListener {
    Map<Str,uint>       mod_name2id;
    Vec<Str>            mod_id2name;
    Vec<VArgData>       args;           // -- 'args[mod_id]' give all information on formal arguments
    Vec<NetlistRef>     netlists;       // -- 'netlists[mod_id]' gives netlist for that module

    void moveTo(PL_Base& dst) {
        mod_name2id.moveTo(dst.mod_name2id);
        mod_id2name.moveTo(dst.mod_id2name);
        args.moveTo(dst.args);
        netlists.moveTo(dst.netlists);
    }
};


//=================================================================================================
// -- Parser: Listener -- Scan Modules


struct ScanModules : PL_Base {
    // ParserListener interface:
    Mode newModule(Str mod_name, const Vec<Str>& formals, bool verific_op, bool black_box);
    void argDecl(VArgKind kind, Pair<uint,uint> range, const Vec<Str>& vars);
    void endModule();
};


ParserListener::Mode ScanModules::newModule(Str mod_name, const Vec<Str>& formals, bool verific_op, bool black_box)
{
    // Add module:
    uint mod_id = mod_id2name.size();
    mod_id2name.push(mod_name);

    if (mod_name2id.set(mod_name, mod_id))
        throw String((FMT "Module defined twice: %_", mod_name));

    args.push();
    VArgData& arg = args.last();

    // Add netlist:
    assert(mod_id == netlists.size());
    netlists.push(Netlist_new());

    // Add formals to module:
    for (uind i = 0; i < formals.size(); i++){
        uint var_id = arg.id2name.size();
        arg.id2name.push(formals[i]);

        if (arg.name2id.set(formals[i], var_id))
            throw String((FMT "Formal argument declared twice: %_", formals[i]));
    }

    // Remember status:
    arg.verific_op = verific_op;
    arg.black_box  = black_box ? 1 : 0;

    return ARGS_ONLY;
}


void ScanModules::argDecl(VArgKind kind, Pair<uint,uint> range, const Vec<Str>& vars)
{
    VArgData& arg = args.last();

    if (range.snd != 0)
        throw String((FMT "Ranges must be of type [hi:0], not [%_:%_] for: %_", range.fst, range.snd, vars[0]));

    for (uind i = 0; i < vars.size(); i++){
        uint var_id;
        if (!arg.name2id.peek(vars[i], var_id))
            throw String((FMT "Input/output variable is not in the list of formals: %_", vars[i]));

        arg.id2type(var_id) = VType(kind, range);

        if (kind == a_Input){
            arg.id2num(var_id, UINT_MAX) = arg.inum2id.size();
            arg.inum2id.push(var_id);
        }else if (kind == a_Output){
            arg.id2num(var_id, UINT_MAX) = arg.onum2id.size();
            arg.onum2id.push(var_id);
        }else
            throw String((FMT "Argument must be of type 'input' or 'output': %_ (in module %_)", vars[i], mod_id2name.last()));
    }
}


void ScanModules::endModule()
{
    VArgData& arg = args.last();

    for (uind i = 0; i < arg.id2name.size(); i++){
        if (arg.id2type(i).kind == a_NULL)
            throw String((FMT "Formal is missing type declaration: %_ (in module %_)", arg.id2name[i], mod_id2name.last()));
    }
}


//=================================================================================================
// -- Parser: Listener -- Build Modules


struct BM_Net {
    int     id;         // -- negative IDs are used for actual 'Wire's (use '~id' to get 'gate_id'); positive IDs are 'BuildModules' IDs.
    GLit    fanout;
    uint    pin;
    BM_Net(int id_, GLit fanout_, uint pin_) : id(id_), fanout(fanout_), pin(pin_) {}
};


struct BuildModules : PL_Base {
    Vec<VerilogModule>  mods;       // -- list of all modules
    uint                mod_id;     // -- active module

    Map<Str,uint>       lname2id;   // -- local name ("lname") to ID; different from argument name ID in 'args'.
    uint                idC;        // -- ID counter. NOTE! Increased by 'width' when a gate is seen to leave room for all bits of the signal.
    IntMap<uint,uint>   width;      // -- Width of a signal. For 'width > 1' all but the first element is 'UINT_MAX'.
    IntMap<uint,GLit>   gates;      // -- Netlist wire built for signal.
    Vec<BM_Net>         nets;       // -- Nets are attached at the end (in 'endModule()').
    String              scratch;    // -- Scratch pad for storing names.

    // Options:
    bool            store_names;
    VerilogErrors   error_levels;

    BuildModules(PL_Base& scan, bool store_names_, VerilogErrors error_levels_);

    uint addName(Str lname, uint width_);
    uint lookup(Str lname);
    void storeName(GLit gate, Str name, uint index, cchar* prefix = NULL);

    uint getSimpleNet(const VExpr& e, bool forbid_negation = true);
    int  getExtendedNet(const VExpr& e);
    void getCompositeNets(const VExpr& e, /*out*/Vec<int>& ns, /*out*/Vec<Pair<Str,uint> >& names);

    // ParserListener interface:
    Mode newModule(Str mod_name, const Vec<Str>& formals, bool verific_op, bool black_box);
    void endModule();
    void end();

    void argDecl(VArgKind kind, Pair<uint,uint> range, const Vec<Str>& vars);
    void netDecl(VNetKind kind, Pair<uint,uint> range, const Vec<Str>& vars);

    void instance(Str mod_name, Str inst_name, const Vec<Pair<Str,VExpr> >& conns);
    void assign(Str lhs_name, uint lhs_index, const VExpr& rhs);
    void gate(VGateKind type, Str name, const Vec<VExpr>& args);
};


BuildModules::BuildModules(PL_Base& scan, bool store_names_, VerilogErrors error_levels_) :
    width(UINT_MAX),
    gates(glit_NULL),
    store_names(store_names_),
    error_levels(error_levels_)
{
    scan.moveTo(*this);
    mod_id = UINT_MAX;
    idC = 0;
}


inline uint BuildModules::addName(Str lname, uint width_)
{
    uint* id;
    if (!lname2id.get(lname, id)){
        *id = idC;
        idC += width_;
        width(*id) = width_;
    }else{
        if (width[*id] != width_)
            throw String((FMT "Net declared twice with different ranges: %_[%_:0] vs. [%_:0]", lname, width[*id]-1, width_-1));
    }
    return *id;
}


inline uint BuildModules::lookup(Str lname)
{
    uint id;
    if (!lname2id.peek(lname, id)){
        if (error_levels.undeclared_symbols == vel_Ignore){
            id = addName(lname, 1);
        }else if (error_levels.undeclared_symbols == vel_Warning){
            WriteLn "WARNING! Net '\a/%_\a/' in module '\a*%_\a*' not declared.  (assumed to be single bit)", lname, mods[mod_id].mod_name;
            id = addName(lname, 1);
        }else
            throw String((FMT "Net not declared: %_", lname));
    }
    return id;
}


void BuildModules::storeName(GLit gate, Str name, uint index, cchar* prefix)
{
    NetlistRef N = mods[mod_id].netlist;

    if (index == UINT_MAX){
        if (!prefix)
            N.names().add(gate, name);
        else{
            scratch.clear();
            FWrite(scratch) "%_%_", prefix, name;
            N.names().add(gate, scratch.c_str());
        }
    }else{
        scratch.clear();
        if (!prefix)
            FWrite(scratch) "%_[%_]", name, index;
        else
            FWrite(scratch) "%_%_[%_]", prefix, name, index;
        N.names().add(gate, scratch.c_str());
    }
}


// Must be of form 'x' (a single bit) or 'x[42]'. Returns a proper 'BuildModules' ID.
uint BuildModules::getSimpleNet(const VExpr& e, bool forbid_negation)
{
    if (sign(e) && forbid_negation) throw String((FMT "Net cannot be negated in this context: %_", e));

    if (type(e) == vx_Var){
        uint id = lookup(name(e));
        if (width[id] != 1) throw String((FMT "Net must be a single bit in this context: %_", e));
        return id;

    }else if (type(e) == vx_Bitsel){
        uint id = lookup(name(e)); assert(width[id] != UINT_MAX);
        uint k = index(e);
        if (k > width[id]) throw String((FMT "Index out of range: %_", e));
        return id + k;

    }else
        throw String((FMT "Must be simple expression in this context: %_", e));
}


// 'e' is either a simple net, a 1-bit constant or a pseudo-input (will be added to current netlist)
// Constants and pseudos are returned as negative gate IDs (using 'N.False()' for constant zero).
int BuildModules::getExtendedNet(const VExpr& e)
{
    VerilogModule& M = mods(mod_id);
    NetlistRef     N = M.netlist;

    if (type(e) == vx_Pseudo){
        Wire w = N.add(PI_());
        M.pseudos.push(id(w));
        return ~id(w);

    }else if (type(e) == vx_Const){
        if (nbits(e) != 1)
            throw String((FMT "Only 1-bit constants are allowed in assign expressions: %_", e));
        return bit(e, 0) ? ~gid_True : ~gid_False;

    }else
        return getSimpleNet(e);
}


// Returns a vector of 'BuildModules' IDs. For constants, '~gid_True' and '~gid_False' are used
// (compatible with 'BM_Net'). The returned names may contain 'Str_NULL' (if the 'uint' is UINT_MAX,
// then there is no index).
void BuildModules::getCompositeNets(const VExpr& e, /*out*/Vec<int>& ns, /*out*/Vec<Pair<Str,uint> >& names)
{
    switch (type(e)){
    case vx_Var:{
        uint var_id = lookup(name(e));
        for (uint j = 0; j < width[var_id]; j++){
            ns.push(var_id + j);
            names.push(tuple(name(e), (width[var_id] > 1) ? j : UINT_MAX)); }
        break;}

    case vx_Bitsel:
        ns.push(getSimpleNet(e));
        names.push(tuple(name(e), index(e)));
        break;

    case vx_Slice:{
        uint var_id = lookup(name(e));
        Pair<uint,uint> rng = range(e);
        if (rng.fst < rng.snd)
            throw String((FMT "Range in wrong order: [%_,%_]", rng.fst, rng.snd));
        if (rng.fst >= width[var_id])
            throw String((FMT "Upper limit of range exceed variable width: [%_,%_]", rng.fst, rng.snd));

        for (uint j = rng.snd; j <= rng.fst; j++){
            ns.push(var_id + j);
            names.push(tuple(name(e), j)); }
        break;}

    case vx_Const:
        for (uint j = 0; j < nbits(e); j++){
            ns.push(bit(e, j) ? ~gid_True : ~gid_False);
            names.push(tuple(Str_NULL, UINT_MAX)); }
        break;

    case vx_Concat:
        for (uint j = 0; j < size(e); j++)
            getCompositeNets(elem(e, j), ns, names);
        break;

    case vx_NULL:   throw String((FMT "Empty expressions are not allowed in arguments."));
    case vx_Pseudo: throw String((FMT "Pseudo-inputs (\"1'bx\") are not allowed in arguments."));
    case vx_And:    throw String((FMT "ANDs (\"&\") are not allowed in arguments."));
    case vx_Mux:    throw String((FMT "MUXes (\"? :\") are not allowed in arguments."));

    default: assert(false); }
}


ParserListener::Mode BuildModules::newModule(Str mod_name, const Vec<Str>& formals, bool /*verific_op*/, bool /*black_box*/)
{
    bool ok = mod_name2id.peek(mod_name, mod_id); assert(ok);
    VerilogModule& M = mods(mod_id);

    M.mod_id   = mod_id;
    M.mod_name = mod_name;
    M.netlist  = netlists[mod_id];

    NetlistRef N = M.netlist;

    VArgData& arg = args[mod_id];

    // Create PIs and POs:
    for (uint n = 0; n < arg.inum2id.size(); n++){
        uint   arg_id = arg.inum2id[n];
        Str    arg_name = arg.id2name[arg_id];
        VRange rng = arg.id2type[arg_id].range;
        uint   width = rng.fst + 1;

        if (rng.snd != 0)
            throw String((FMT "Ranges must be of type [hi:0], not [%_:%_] for: %_ (in module %_)", rng.fst, rng.snd, arg_name, mod_name));

        M.in_name.push(arg_name);
        M.in_width.push(width);                 // -- add width to 'Module'
        uint sym_id = addName(arg_name, width); // -- add input symbol to temporary parsing tables
        //**/WriteLn "gave symbol '%_' id %_   (width=%_)", arg_name, sym_id, width;

        for (uint i = 0; i < width; i++){
            Wire w = N.add(PI_(M.in_gate.size()));  // -- add PI gate to netlist
            M.in_gate.push(id(w));                  // -- add individual gates to 'Module'
            gates(sym_id + i) = w;                  // -- add individual gates to temporary parsing tables
            //**/WriteLn "Added PI with id=%_: %_", sym_id + i, w;

            if (store_names)
                storeName(w, arg_name, (width == 1) ? UINT_MAX : i);
        }
    }

    for (uint n = 0; n < arg.onum2id.size(); n++){
        uint   arg_id = arg.onum2id[n];
        Str    arg_name = arg.id2name[arg_id];
        VRange rng = arg.id2type[arg_id].range;
        uint   width = rng.fst + 1;

        if (rng.snd != 0)
            throw String((FMT "Ranges must be of type [hi:0], not [%_:%_] for: %_ (in module %_)", rng.fst, rng.snd, arg_name, mod_name));

        M.out_name.push(arg_name);
        M.out_width.push(width);    // -- add width to 'Module'
        addName(arg_name, width);   // -- add output symbol to temporary parsing tables

        for (uint i = 0; i < width; i++){
            Wire w = N.add(PO_(M.out_gate.size())); // -- add PO gate to netlist
            M.out_gate.push(id(w));                 // -- add individual gates to 'Module'
                // -- unlike for PIs, we have no "gates(id) = w.lit()" here; there will be an internal signal with that name

            if (store_names)
                storeName(w, arg_name, (width == 1) ? UINT_MAX : i, verilog_module_output_prefix);
        }
    }

    return FULL;
}


void BuildModules::endModule()
{
    VerilogModule& M   = mods[mod_id];
    VArgData&      arg = args[mod_id];
    NetlistRef     N   = M.netlist;

    // Make sure all nets have a driver:
    For_Map(lname2id){
        Str   name   = Map_Key(lname2id);
        uint  sym_id = Map_Value(lname2id);

        for (uint i = 0; i < width[sym_id]; i++){
            if (!gates[sym_id + i]){
                if (!arg.black_box){
                    if (error_levels.no_driver == vel_Warning)
                        WriteLn "WARNING! Net '\a/%_%_\a/' in module '\a*%_\a*' has no driver.  (added pseudo-input)", name, (width[sym_id] == 1) ? String() : String((FMT "[%_]", i)), M.mod_name;
                    else if (error_levels.no_driver == vel_Error)
                        throw String((FMT "Net has no driver: %_[%_]", (width[sym_id] == 1) ? String() : String((FMT "[%_]", i)) ));
                }
                Wire w = N.add(PI_());       // -- add pseudo-input
                M.pseudos.push(id(w));
                gates(sym_id + i) = w.lit();
            }
        }
    }

    // Attach nets:
    for (uind i = 0; i < nets.size(); i++){
        Wire w = N[nets[i].fanout];
        int  id = nets[i].id;
        Wire w_in = ((id >= 0) ? N[gates[id]] : N[~id]) ^ sign(w);
        //**/Dump(nets[i].id, nets[i].fanout, nets[i].pin, w, w_in);
        w.set(nets[i].pin, w_in);
    }

    // Attach POs:
    uint num = 0;
    for (uint n = 0; n < arg.onum2id.size(); n++){
        uint arg_id   = arg.onum2id[n];
        Str  arg_name = arg.id2name[arg_id];
        uint sym_id = lookup(arg_name);

        for (uint i = 0; i < width[sym_id]; i++){
            Wire w = N[gates[sym_id + i]];
            Wire w_po = N[M.out_gate[num]];
            if (!w){
                if (error_levels.no_driver == vel_Warning){
                    WriteLn "WARNING! Missing definition for output signal: \a/%_%_\a/  (added pseudo-input)", arg_name, (width[sym_id] == 1) ? String() : String((FMT "[%_]", i));
                }else if (error_levels.no_driver == vel_Error)
                    throw String((FMT "Missing definition for output signal: %_%_", arg_name, (width[sym_id] == 1) ? String() : String((FMT "[%_]", i)) ));
                w = N.add(PI_());       // -- add pseudo-input
                M.pseudos.push(id(w));
            }
            num++;
            w_po.set(0, w);
        }
    }

    // Store status:
    M.verific_op = arg.verific_op;
    M.black_box  = arg.black_box;

    // Check for dangling locic:
    Auto_Pob(N, fanout_count);
    if (!arg.black_box){
        For_Gates(N, w){
            if (type(w) != gate_PO && fanout_count[w] == 0){
                if (error_levels.dangling_logic == vel_Warning)
                    WriteLn "WARNING! Module contains dangling logic: \a*%_\a*  (gate: \a/%_\a/)", M.mod_name, N.names().get(w);
                else if (error_levels.dangling_logic == vel_Error)
                    throw String((FMT "Module contains dangling logic: %_  (gate: %_)", M.mod_name, N.names().get(w)));
            }
        }
    }

    if (!removeBuffers(N))
        throw String((FMT "Module contains infinte loop among buffers/inverters: %_", M.mod_name));

    // Clean up local data:
    mod_id = UINT_MAX;
    idC = 0;
    lname2id.clear();
    width.clear();
    gates.clear();
    nets.clear();
}


void BuildModules::argDecl(VArgKind, Pair<uint,uint>, const Vec<Str>&)
{
    /*handled in the scan phase*/
}


void BuildModules::netDecl(VNetKind /*kind*/, Pair<uint,uint> range, const Vec<Str>& vars)
{
    assert(vars.size() > 0);
    if (range.snd != 0)
        throw String((FMT "Ranges must be of type [hi:0], not [%_:%_] for: %_", range.fst, range.snd, vars[0]));

    for (uind i = 0; i < vars.size(); i++)
        addName(vars[i], range.fst + 1);
}


void BuildModules::instance(Str mod_name, Str inst_name, const Vec<Pair<Str,VExpr> >& conns)
{
    typedef Pair<Str,uint> Nam;

    VerilogModule& M = mods[mod_id];
    NetlistRef     N = M.netlist;

    // Get submodule under instantiation:
    uint submod_id;
    if (!mod_name2id.peek(mod_name, submod_id)){
        assert(conns.size() > 0);
        if (error_levels.undefined_module == vel_Error)
            throw String((FMT "Module not defined: %_", mod_name));
        else{
            // Unknown module, guessing interface:
            if (error_levels.undefined_module == vel_Warning)
                WriteLn "WARNING! Module '\a*%_\a*' used in '\a*%_\a*' is not defined.  (guessing interface)", mod_name, M.mod_name;

            // Add empty module:
            submod_id = mod_id2name.size();
            mod_id2name.push(mod_name);

            if (mod_name2id.set(mod_name, submod_id)) assert(false);

            args.push();
            VArgData& arg = args.last();

            assert(submod_id == netlists.size());
            netlists.push(Netlist_NULL);

            // Add formal arguments to empty module:
            for (uind i = 0; i < conns.size(); i++){
                if (!conns[i].fst)
                    throw String((FMT "Undeclared modules must be called with named arguments: %_", mod_name));

                uint var_id = arg.id2name.size();
                arg.id2name.push(conns[i].fst);

                if (arg.name2id.set(conns[i].fst, var_id))
                    throw String((FMT "Argument declared twice: %_", conns[i].fst));

                Vec<int> conn_nets;
                Vec<Nam> conn_names;
                getCompositeNets(conns[i].snd, conn_nets, conn_names);
                VRange rng(conn_nets.size()-1, 0);

                VArgKind kind = a_Input;;
                for (uint j = 0; j < conn_nets.size(); j++){
                    if (!gates[conn_nets[j]])
                        kind = a_Output; }

                arg.id2type.push(VType(kind, rng));
                //**/WriteLn "Derived type for '%_': %_", conns[i].fst, arg.id2type.last();

                if (kind == a_Input){
                    arg.id2num(var_id, UINT_MAX) = arg.inum2id.size();
                    arg.inum2id.push(var_id);
                }else if (kind == a_Output){
                    arg.id2num(var_id, UINT_MAX) = arg.onum2id.size();
                    arg.onum2id.push(var_id);
                }
            }

            // Remember status:
            arg.verific_op = false;
            arg.black_box  = 2;

            // Add incomplete "stub" to list of output modules:
            VerilogModule& M_sub = mods(submod_id);
            M_sub.mod_id = submod_id;
            M_sub.mod_name = mod_name;
            M_sub.netlist = Netlist_NULL;
            M_sub.verific_op = arg.verific_op;
            M_sub.black_box = arg.black_box;

            for (uint n = 0; n < arg.inum2id.size(); n++){
                uint   arg_id = arg.inum2id[n];
                Str    arg_name = arg.id2name[arg_id];
                VRange rng = arg.id2type[arg_id].range;
                uint   width = rng.fst + 1;
                M_sub.in_name.push(arg_name);
                M_sub.in_width.push(width);
            }
            for (uint n = 0; n < arg.onum2id.size(); n++){
                uint   arg_id = arg.onum2id[n];
                Str    arg_name = arg.id2name[arg_id];
                VRange rng = arg.id2type[arg_id].range;
                uint   width = rng.fst + 1;
                M_sub.out_name.push(arg_name);
                M_sub.out_width.push(width);
            }
        }
    }

    // Expand connection expressions to individual bits:
    Vec<Vec<int> > conn_nets;
    Vec<Vec<Nam> > conn_names;
    for (uint i = 0; i < conns.size(); i++){
        conn_nets .push();
        conn_names.push();
        getCompositeNets(conns[i].snd, conn_nets.last(), conn_names.last());
        //**/Dump(conns[i].snd, conn_nets.last(), conn_names.last());
    }

    // Sort the individual bits into a PI and a PO list:
    VArgData& subarg = args[submod_id];
    Vec<uint> inputs (subarg.inum2id.size(), UINT_MAX);   // }
    Vec<uint> outputs(subarg.onum2id.size(), UINT_MAX);   // }- Holds indices into 'conn_nets' (outer vector)
    for (uint i = 0; i < conns.size(); i++){
        if ((conns[i].fst && !conns[0].fst) || (!conns[i].fst && conns[0].fst))
            throw String((FMT "Either all or none of the arguments must be named in a module instantiation."));

        uint arg_id;
        if (conns[i].fst){
            if (!subarg.name2id.peek(conns[i].fst, arg_id))
                throw String((FMT "No such formal argument: %_", conns[i].fst));
        }else
            arg_id = i;

        VType arg_type = subarg.id2type[arg_id];
        uint  arg_width;
        uint  arg_pos;
        bool  is_input;

        if (arg_type.kind == a_Input)           is_input = true;
        else assert(arg_type.kind == a_Output), is_input = false;

        assert(arg_type.range.snd == 0);
        arg_width = arg_type.range.fst + 1;
        arg_pos = subarg.id2num[arg_id];

        if (arg_width != conn_nets[i].size())
            throw String((FMT "Incorrect arity of argument %_. Should be %_ bits, not %_.", i+1, arg_width, conn_nets[i].size()));

        if (is_input) inputs [arg_pos] = i;
        else          outputs[arg_pos] = i;
    }

    Vec<int> pi_nets;
    Vec<Nam> pi_names;
    for (uint i = 0; i < inputs.size(); i++){
        if (inputs[i] == UINT_MAX)
            throw String((FMT "Missing input(s) in instance '%_' of module '%_'.", inst_name, mod_name));
        Vec<int>& ns = conn_nets [inputs[i]];
        Vec<Nam>& ts = conn_names[inputs[i]];
        for (uint j = 0; j < ns.size(); j++){
            pi_nets .push(ns[j]);
            pi_names.push(ts[j]);
        }
    }

    Vec<int>  po_nets;
    Vec<Nam>  po_names;
    Vec<uint> po_num;
    uint      numC = 0;
    for (uint i = 0; i < outputs.size(); i++){
        if (outputs[i] == UINT_MAX){
            if (error_levels.unused_output == vel_Error)
                throw String((FMT "Unused output(s) in instance '%_' of module '%_'.", inst_name, mod_name));
            else if (error_levels.unused_output == vel_Warning)
                WriteLn "WARNING! Unused output(s) in instance '\a/%_\a/' of module '\a*%_\a*'.", inst_name, mod_name;

        }else{
            Vec<int>& ns = conn_nets [outputs[i]];
            Vec<Nam>& ts = conn_names[outputs[i]];
            for (uint j = 0; j < ns.size(); j++){
                po_nets .push(ns[j]);
                po_names.push(ts[j]);
                po_num  .push(numC + j);
            }
        }
        numC += subarg.id2type[subarg.onum2id[i]].range.fst + 1;
    }

    // Create UIF gate + Pin gates:
    Wire w_uif = N.add(Uif_(submod_id), pi_nets.size());
    assert(inst_name);
    if (store_names){
        scratch.clear();
        FWrite(scratch) "%_%_", verilog_instance_prefix, inst_name;
        N.names().add(w_uif, scratch.c_str());
    }

    for (uint i = 0; i < po_nets.size(); i++){
        Wire w_pin = N.add(Pin_(po_num[i]), w_uif);
        nets.push(BM_Net(~id(w_uif), w_pin.lit(), 0));

        if (po_nets[i] < 0)
            throw String((FMT "Cannot use a constant as an output signal."));
        if (gates[po_nets[i]])
            throw String((FMT "Signal defined twice: %_", po_names[i].fst));

        gates(po_nets[i]) = w_pin;
        if (store_names && po_names[i].fst)
            storeName(w_pin, po_names[i].fst, po_names[i].snd);
    }

    for (uint i = 0; i < pi_nets.size(); i++)
        nets.push(BM_Net(pi_nets[i], w_uif.lit(), i));
}


// The only assignments we allow is either binary ANDs or MUXes (whi may have pseudos):
//    assign x = ~y & z;
//    assign n1 = sel[0] ? data[7] : data[6];
//    assign n2 = n5 ? 1'b1 : 1'bx;
void BuildModules::assign(Str lhs_name, uint lhs_index, const VExpr& rhs)
{
    VerilogModule& M = mods[mod_id];
    NetlistRef     N = M.netlist;

    // Get left-hand side ID:
    uint lhs_id = lookup(lhs_name);
    if (width[lhs_id] != 1 && lhs_index == UINT_MAX) throw String((FMT "Left-hand side must be a single bit, not a vector: %_", lhs_name));
    if (width[lhs_id] == 1 && lhs_index != UINT_MAX) throw String((FMT "Left-hand side is not a vector: %_", lhs_name));
    if (lhs_index != UINT_MAX){
        if (lhs_index >= width[lhs_id]) throw String((FMT "Index out of range: %_[%_]", lhs_name, lhs_index));
        lhs_id += lhs_index;
    }

    if (gates[lhs_id])
        throw String((FMT "Signal defined twice: %_", lhs_name));

    if (type(rhs) == vx_And){
        uint id0   = getSimpleNet(elem(rhs, 0), false);
        bool sign0 = sign(elem(rhs, 0));
        uint id1   = getSimpleNet(elem(rhs, 1), false);
        bool sign1 = sign(elem(rhs, 1));

        Wire w_and = N.add(And_());
        nets.push(BM_Net(id0, w_and.lit() ^ sign0, 0));
        nets.push(BM_Net(id1, w_and.lit() ^ sign1, 1));

        //**/Dump(lhs_name, lhs_index, rhs, id0, sign0, id1, sign1, w_and);

        gates(lhs_id) = w_and ^ sign(rhs);

    }else if (type(rhs) == vx_Mux){
        uint sel = getSimpleNet(elem(rhs, 0));
        int  tt  = getExtendedNet(elem(rhs, 1));
        int  ff  = getExtendedNet(elem(rhs, 2));
        Wire w_mux = N.add(Mux_());
        nets.push(BM_Net(sel, w_mux.lit(), 0));
        nets.push(BM_Net(tt , w_mux.lit(), 1));
        nets.push(BM_Net(ff , w_mux.lit(), 2));

        gates(lhs_id) = w_mux ^ sign(rhs);

    }else{
        Wire w = N.add(Buf_());
        nets.push(BM_Net(getExtendedNet(rhs), w.lit(), 0));
        gates(lhs_id) = w ^ sign(rhs);
        //**/Dump(w, lhs_name, lhs_index, rhs);
        //**/Dump(nets.last().id, nets.last().fanout, nets.last().pin);
    }

    if (store_names)
        storeName(gates[lhs_id], lhs_name, lhs_index);
}


// NOTE! Gate names are lost. Only names of nets are stored.
void BuildModules::gate(VGateKind type_, Str/*optional inst. name*/, const Vec<VExpr>& args)
{
    VerilogModule& M = mods(mod_id);
    NetlistRef     N = M.netlist;

    uint id = getSimpleNet(args[0]);    // -- will make sure 'name' is declared
    //**/WriteLn "creating id=%_ of type %_ from %_", id, (uint)type_, args;

    switch (type_){
    case g_And:
    case g_Nand:{
        gates(id) = N.add(And_()) ^ (type_ == g_Nand);
        GLit acc = +gates[id];
        for (uint i = 1; i < args.size()-1; i++){
            nets.push(BM_Net(getExtendedNet(args[i]), acc, 0));
            if (i + 1 == args.size() - 1)
                nets.push(BM_Net(getExtendedNet(args[i+1]), acc, 1));
            else{
                Wire w = N.add(And_());
                nets.push(BM_Net(~w.id(), acc, 1));
                acc = w;
            }
        }
        break;}

    case g_Or:
    case g_Nor:{
        gates(id) = N.add(And_()) ^ (type_ == g_Or);
        GLit acc = +gates[id];
        for (uint i = 1; i < args.size()-1; i++){
            nets.push(BM_Net(getExtendedNet(args[i]), ~acc, 0));
            if (i + 1 == args.size() - 1)
                nets.push(BM_Net(getExtendedNet(args[i+1]), ~acc, 1));
            else{
                Wire w = N.add(And_());
                nets.push(BM_Net(~w.id(), acc, 1));
                acc = w;
            }
        }
        break;}

    case g_Xor:
    case g_Xnor:{
        gates(id) = N.add(Xor_()) ^ (type_ == g_Xnor);
        GLit acc = +gates[id];
        for (uint i = 1; i < args.size()-1; i++){
            nets.push(BM_Net(getExtendedNet(args[i]), acc, 0));
            if (i + 1 == args.size() - 1)
                nets.push(BM_Net(getExtendedNet(args[i+1]), acc, 1));
            else{
                Wire w = N.add(Xor_());
                nets.push(BM_Net(~w.id(), acc, 1));
                acc = w;
            }
        }
        break;}

    case g_Buf:
    case g_Not:
        if (args.size() != 2)
            throw String((FMT "Expected 1 argument, not %_ in gate declaration.", args.size()));

        gates(id) = N.add(Buf_()) ^ (type_ == g_Not);
        nets.push(BM_Net(getExtendedNet(args[1]), +gates[id], 0));
        break;

    case g_Unsupp:
    default: assert(false); }

    if (store_names){
        const VExpr& e = args[0];
        if (type(e) == vx_Var)
            storeName(gates[id], name(e), UINT_MAX);
        else assert(type(e) == vx_Bitsel),
            storeName(gates[id], name(e), index(e));
    }
}


void BuildModules::end()
{
    /*nothing yet*/
}


//=================================================================================================
// -- Parser: Main method


void VerilogParser::parse(Vec<VerilogModule>& result)
{
    ScanModules scan;
    parse(scan);

    BuildModules build(scan, store_names, error_levels);
    parse(build);
    build.mods.moveTo(result);
}


//=================================================================================================
// -- Parser: Recursive decent parser


// These macros assume variable 'p' is in scope and of type 'Tok*':
#define TOK  TokType(p[0].type)
#define TOK2 TokType(p[1].type)
#define STR  slice(text[p[0].offset], text[p[0].offset + p[0].len])
#define STR2 slice(text[p[1].offset], text[p[1].offset + p[1].len])
#define NEXT (p++)
#define LOC  getLocation(&text[p[0].offset])


// An empty range will be returned as '(0, 0)'.
Pair<uint,uint> VerilogParser::parseOptionalRange(Tok*& p)
{
    if (TOK == tok_LBrack){
        NEXT;
        uint rng[2];
        for (uint i = 0; i < 2; i++){
            if (TOK != tok_Num || !simpleNumber(STR)) throw Excp_ParseError((FMT "[%_] Expected constant number in range description, not: %_", LOC, STR));
            rng[i] = stringToUInt64((Str)STR);
            NEXT;

            if (i == 0){
                if (TOK != tok_Colon) throw Excp_ParseError((FMT "[%_] Expected ':' in range description, not: %_", LOC, STR));
                NEXT; }
        }
        if (TOK != tok_RBrack) throw Excp_ParseError((FMT "[%_] Expected ']' at end of range description, not: %_", LOC, STR));
        NEXT;

        return tuple(rng[0], rng[1]);

    }else
        return tuple(0u, 0u);
}


// 'vars' will be cleared, then populated. Terminating ';' is consumed.
void VerilogParser::parseListOfVariables(Tok*& p, /*out:*/Vec<Str>& vars)
{
    vars.clear();

    if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected variable name, not: %_", LOC, STR));
    vars.push(STR);
    NEXT;

    for(;;){
        if (TOK == tok_Semi){
            NEXT;
            break;
        }else if (TOK == tok_Comma){
            NEXT;
            if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected variable name, not: %_", LOC, STR));
            vars.push(STR);
            NEXT;
        }else
            throw Excp_ParseError((FMT "[%_] Expected ',' or ';', not: %_", LOC, STR));
    }
}


// This is intentionally a very restricted parser -- we want to make assumption on the input subset.
VExpr VerilogParser::parseExpr(Tok*& p)
{
    bool sign0 = false;
    if (TOK == tok_Neg){ NEXT; sign0 = true; }
    VExpr expr0 = parseSimpleExpr(p);
    if (sign0) VExpr_NegBang(expr0);

    if (TOK == tok_And || TOK == tok_Or){
        bool was_and = (TOK == tok_And);
        NEXT;
        bool sign1 = false;
        if (TOK == tok_Neg){ NEXT; sign1 = true; }
        VExpr expr1 = parseSimpleExpr(p);
        if (sign1) VExpr_NegBang(expr1);

        if (was_and)
            return VExpr_And(expr0, expr1);
        else{
            VExpr_NegBang(expr0);
            VExpr_NegBang(expr1);
            VExpr ret = VExpr_And(expr0, expr1);
            VExpr_NegBang(ret);
            return ret;
        }

    }else if (TOK == tok_Quest){
        NEXT;

        VExpr expr1;
        if (TOK == tok_Pseudo){
            NEXT;
            expr1 = VExpr_Pseudo();
        }else{
            bool sign1 = false;
            if (TOK == tok_Neg){ NEXT; sign1 = true; }
            expr1 = parseSimpleExpr(p);
            if (sign1) VExpr_NegBang(expr1);
        }

        if (TOK != tok_Colon) throw Excp_ParseError((FMT "[%_] Expected ':' after '?', not: %_", LOC, STR));
        NEXT;

        VExpr expr2;
        if (TOK == tok_Pseudo){
            NEXT;
            expr2 = VExpr_Pseudo();
        }else{
            bool sign2 = false;
            if (TOK == tok_Neg){ NEXT; sign2 = true; }
            expr2 = parseSimpleExpr(p);
            if (sign2) VExpr_NegBang(expr2);
        }

        return VExpr_Mux(expr0, expr1, expr2);

    }else
        return expr0;
}


VExpr VerilogParser::parseSimpleExpr(Tok*& p)
{
    if (TOK == tok_Ident){
        Str name = STR;
        NEXT;

        if (TOK == tok_LBrack){
            NEXT;
            if (TOK != tok_Num || !simpleNumber(STR)) throw Excp_ParseError((FMT "[%_] Expected constant number in array expression: %_", LOC, STR));
            uint idx = stringToUInt64((Str)STR);
            NEXT;

            if (TOK == tok_RBrack){
                NEXT;
                return VExpr_Bitsel(name, idx);

            }else{
                if (TOK != tok_Colon) throw Excp_ParseError((FMT "[%_] Expected ']' or ':' after array index, not: %_", LOC, STR));
                NEXT;

                if (TOK != tok_Num || !simpleNumber(STR)) throw Excp_ParseError((FMT "[%_] Expected constant number in range expression, not: %_", LOC, STR));
                uint idx2 = stringToUInt64((Str)STR);
                NEXT;

                if (TOK != tok_RBrack) throw Excp_ParseError((FMT "[%_] Expected ']' after range expression, not: %_", LOC, STR));
                NEXT;

                return VExpr_Slice(name, idx, idx2);
            }

        }else
            return VExpr_Var(name);

    }else if (TOK == tok_Num){
        uind sep = search(STR, '\'');
        if (sep == UIND_MAX) throw Excp_ParseError((FMT "[%_] Expected sized constant, not: %_", LOC, STR));

        uint w = stringToUInt64((Str)STR.slice(0, sep));

        if (STR[sep+1] != 'b') throw Excp_ParseError((FMT "[%_] Currently only supports binary constants: %_", LOC, STR));

        VExpr e = VExpr_Const(STR.slice(sep+2));
        if (nbits(e) != w) throw Excp_ParseError((FMT "[%_] Declared width does not match number of digits: %_", LOC, STR));
        NEXT;

        return e;

    }else if (TOK == tok_LBrace){
        NEXT;
        Vec<VExpr> es;
        for(;;){
            es.push(parseExpr(p));
            if (TOK == tok_RBrace){
                NEXT;
                break; }

            if (TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected '}' or ',' after concat expression, not: %_", LOC, STR));
            NEXT;
        }
        return VExpr_Concat(es);

    }else if (TOK == tok_Pseudo){
        NEXT;
        return VExpr_Pseudo();

    }else
        throw Excp_ParseError((FMT "[%_] Expected expression, not: %_", LOC, STR));
}


// Returns '(port_name, expr)' where 'port_name' may be a NULL-string.
Pair<Str,VExpr> VerilogParser::parseConnect(Tok*& p)
{
    if (TOK == tok_Period){
        NEXT;
        if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected port name, not: %_", LOC, STR));
        Str port_name = STR;
        NEXT;

        if (TOK != tok_LParen) throw Excp_ParseError((FMT "[%_] Expected '(' after port name, not: %_", LOC, STR));
        NEXT;

        if (TOK == tok_RParen){     // -- special case, unconnected (presumably output) signal
            NEXT;
            return tuple(port_name, VExpr_NULL()); }

        VExpr expr = parseExpr(p);
        if (TOK != tok_RParen) throw Excp_ParseError((FMT "[%_] Expected ')' after connection argument, not: %_", LOC, STR));
        NEXT;

        return tuple(port_name, expr);

    }else
        return tuple(Str(), parseExpr(p));
}


void VerilogParser::parse(ParserListener& pl)
{
    // Setup module item map:
    Map<Str,Pair<uint,uint> > imap;
    for (uind i = 0; i < elemsof(module_items); i++)
        imap.set(slize(module_items[i].sym), tuple((uint)module_items[i].item, (uint)module_items[i].sub));

    // Parse modules:
    Tok* p = toks.base();
    Tok* p0 = p;

    try{
        while (TOK != tok_NULL){
            // Read module header:
            bool verific_op = false;
            bool black_box  = false;
            if (TOK == tok_Ident && eq(STR, "__verific_operator__")){
                verific_op = true;
                NEXT; }
            if (TOK == tok_Ident && eq(STR, "__black_box__")){
                black_box = true;
                NEXT; }

            if (TOK != tok_Ident || !eq(STR, "module")) throw Excp_ParseError((FMT "[%_] Expected module definition, not: %_", LOC, STR));
            NEXT;

            if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected module name, not: %_", LOC, STR));
            Str module_name = STR;
            NEXT;

            if (TOK != tok_LParen) throw Excp_ParseError((FMT "[%_] Expected '(', not: %_", LOC, STR));
            NEXT;

            Vec<Str> formals;
            while (TOK != tok_RParen){
                if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected formal argument, not: %_", LOC, STR));
                formals.push(STR);
                NEXT;
                if (TOK == tok_Comma) NEXT;
            }
            NEXT;

            if (TOK != tok_Semi) throw Excp_ParseError((FMT "[%_] Expected ';' after module header, not: %_", LOC, STR));
            NEXT;

          #ifdef PARSER_DEBUG
            WriteLn "ModuleHead: name=%_  formals=%_%_", module_name, formals, (verific_op ? "  [verific-op]" : "");
          #endif

            ParserListener::Mode mode;
            mode = pl.newModule(module_name, formals, verific_op, black_box);

            // Skip module?
            if (mode == ParserListener::SKIP){      // <<== skip verific operators goes here?
                while (!(TOK == tok_Ident && eq(STR, "endmodule")))
                    NEXT;
            }

            // Read module items:
            for(;;){
                if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected keyword or identifier, not: %_", LOC, STR));

                Str sym = STR;
                NEXT;

                Pair<uint,uint>* m = NULL;
                bool match = imap.peek(sym, m);

                // Skip item?
                if (mode == ParserListener::ARGS_ONLY && (!match || (m->fst != i_Arg && m->fst != i_End))){
                    while (TOK != tok_Semi && TOK != tok_NULL) NEXT;
                    if (TOK == tok_Semi) NEXT;

                    continue;
                }

                // Parse item:
                if (!match){
                    //
                    // == MODULE INSTANTIATION ==
                    //
                    Str module_name = sym;

                    /*parse optional parameter value assignment here*/

                    for(;;){
                        if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected module instance name, not: %_", LOC, STR));
                        Str instance_name = STR;
                        NEXT;

                        if (TOK != tok_LParen) throw Excp_ParseError((FMT "[%_] Expected '(' not: %_", LOC, STR));
                        NEXT;

                        Vec<Pair<Str,VExpr> > conns;
                        for(;;){
                            conns.push(parseConnect(p));
                            if (conns.last().snd->type == vx_NULL)
                                conns.pop();

                            if (TOK == tok_RParen){
                                NEXT;
                                break; }
                            if (TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected ')' or ',' in connection list, not: %_", LOC, STR));
                            NEXT;
                        }

                      #ifdef PARSER_DEBUG
                        WriteLn "ModuleInst:  mod_name=%_  inst_name=%_  conns=%_", module_name, instance_name, conns;
                      #endif

                        if (TOK != tok_Semi && TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected ';' or ',' after module instance, not: %_", LOC, STR));

                        pl.instance(module_name, instance_name, conns);

                        if (TOK == tok_Semi){
                            NEXT;
                            break; }
                        NEXT;
                    }

                }else{
                    //
                    // == OTHER MODULE ITEM ==
                    //
                    switch (m->fst){
                    case i_Arg:{
                        Pair<uint,uint> rng = parseOptionalRange(p);
                        Vec<Str> vars;
                        parseListOfVariables(p, vars);  // -- swallows ';'

                      #ifdef PARSER_DEBUG
                        cchar* type_name[] = { "<null>", "Input", "Output", "Unsupp" };
                        WriteLn "ArgDecl %_: [%_:%_] %_", type_name[m->snd], rng.fst, rng.snd, vars;
                      #endif

                        pl.argDecl((VArgKind)m->snd, rng, vars);

                        break;}

                    case i_Net:{
                        // if next token is "trireg", then the net declarion has a different syntax (ignoring for now)

                        Pair<uint,uint> rng = parseOptionalRange(p);    // -- ignoring "scalared <range>" and "vectored <range>"
                        /*parse optional delay here*/
                        Vec<Str> vars;
                        parseListOfVariables(p, vars);  // -- swallows ';'

                      #ifdef PARSER_DEBUG
                        cchar* type_name[] = { "<null>", "Wire", "Unsupp" };
                        WriteLn "NetDecl %_: [%_:%_] %_", type_name[m->snd], rng.fst, rng.snd, vars;
                      #endif
                        pl.netDecl((VNetKind)m->snd, rng, vars);

                        break;}

                    case i_Assign:
                        // we only support continues assignments of type 'x = <expr>' or 'x[<num>] = <expr>'

                        /*may parse "drive strength" and "delay" here*/
                        for(;;){
                            if (TOK != tok_Ident) throw Excp_ParseError((FMT "[%_] Expected lvalue identifier in 'assign', not: %_", LOC, STR));
                            Str name = STR;
                            NEXT;

                            uint idx = UINT_MAX;
                            if (TOK == tok_LBrack){
                                NEXT;
                                if (TOK != tok_Num || !simpleNumber(STR)) throw Excp_ParseError((FMT "[%_] Expected constant number in array assignment: %_", LOC, STR));
                                idx = stringToUInt64((Str)STR);
                                NEXT;

                                if (TOK != tok_RBrack) throw Excp_ParseError((FMT "[%_] Expected ']' after array index, not: %_", LOC, STR));
                                NEXT;
                            }

                            if (TOK != tok_Assign) throw Excp_ParseError((FMT "[%_] Expected '=' in continuous assignment, not: %_", LOC, STR));
                            NEXT;

                            VExpr expr = parseExpr(p);

                          #ifdef PARSER_DEBUG
                            if (idx == UINT_MAX)
                                WriteLn "Assign %_ := %_", name, expr;
                            else
                                WriteLn "Assign %_[%_] := %_", name, idx, expr;
                          #endif

                            if (TOK != tok_Semi && TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected ';' or ',' in assignment list, not: %_", LOC, STR));

                            pl.assign(name, idx, expr);

                            if (TOK == tok_Semi){
                                NEXT;
                                break; }
                            NEXT;
                        }

                        break;

                    case i_Gate:{
                        uint gate_type ___unused = m->snd;
                        // Comma separated list of instances:
                        for(;;){
                            // (drive strength ("(supply0, weak1)") and delay ("#2+2") can come before name, but ignored for now)
                            Str gate_name;
                            if (TOK == tok_Ident){
                                gate_name = STR;
                                NEXT; }

                            if (TOK != tok_LParen) throw Excp_ParseError((FMT "[%_] Expected '(', not: %_", LOC, STR));
                            NEXT;

                            Vec<VExpr> args;      // -- must be non-empty
                            for(;;){
                                args.push(parseExpr(p));

                                if (TOK == tok_RParen){
                                    NEXT;
                                    break; }
                                if (TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected ')' or ',' in argument list, not: %_", LOC, STR));
                                NEXT;
                            }

                          #ifdef PARSER_DEBUG
                            cchar* type_name[] = { "NULL", "And", "Nand", "Or", "Nor", "Xor", "Xnor", "Not", "Buf", "Unsupp" };
                            WriteLn "Gate:  type=%_  gate_name=%_  args=%_", type_name[m->snd], gate_name, args;
                          #endif

                            pl.gate((VGateKind)m->snd, gate_name, args);

                            if (TOK == tok_Semi){
                                NEXT;
                                break; }

                            if (TOK != tok_Comma) throw Excp_ParseError((FMT "[%_] Expected ';' or ',' after module instance, not: %_", LOC, STR));
                            NEXT;
                        }
                        break;}

                    case i_Unsupp:
                        throw Excp_ParseError((FMT "[%_] Unsupported module item: %_", LOC, sym));

                    case i_End:
                        pl.endModule();
                        goto EndModule;
                    default: assert(false); }
                }
            }
          EndModule:;
        }
        pl.end();

    }catch (String msg){
        if (p != p0) p--;
        throw Excp_ParseError((FMT "[%_] %_", LOC, msg));
    }
}

#undef TOK
#undef TOK2
#undef STR
#undef STR2
#undef NEXT
#undef LOC


//=================================================================================================
// -- Debug:


void VerilogParser::dumpTokens()
{
    for (uind i = 0; i < toks.size(); i++){
        cchar* name = tokName(toks[i].type);
        cchar& start = text[toks[i].offset];
        cchar& end   = text[toks[i].offset + toks[i].len];
        WriteLn "%<7%_: `%_'", name, slice(start, end);
    }
}


void write_VerilogModule(Out& out, const VerilogModule& v)
{
    FWrite(out) "module{id=%_; name=%_; in=%_; out=%_; pseudos=%_; verif=%_; black=%_}",
        v.mod_id, v.mod_name, zipf("%_:%_", v.in_name, v.in_width), zipf("%_:%_", v.out_name, v.out_width),
        v.pseudos.size(), (uint)v.verific_op, (uint)v.black_box;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Main function:


void readVerilog(String file, bool store_names, VerilogErrors error_levels, /*out*/Vec<VerilogModule>& modules, /*in*/Array<char> prelude)
{
    VerilogParser parser;
    parser.store_names  = store_names;
    parser.error_levels = error_levels;
    parser.read(file, modules, prelude);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
