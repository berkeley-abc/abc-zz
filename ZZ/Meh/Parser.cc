//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.cc
//| Author(s)   : Niklas Een
//| Module      : Meh
//| Description : Model-language for the Engine Hacker
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Tokens:
//   - keywords: def const prop if elseif else for
//   - parens: ( ) { } [ ] ; ,
//   - operators: = := @= + - * / % # < > <= >= != == .
//   - var_names   -- all lower-case (or _)
//   - Fun_Names   -- first upper-case but some lower-case
//   - Temp_Names<args>
//   - CONST_NAMES -- all upper-case (or _)
//   - integers: 12 -42
//


enum Meh_TokType {
    tok_NULL,

    tok_Var,    // -- variable_name
    tok_Fun,    // -- Function_Name
    tok_Sym,    // -- SYMBOLIC_CONSTANT
    tok_Int,    // -- positive integer: 42, 4711
    tok_Str,    // -- "double-quote enclosed string"

    tok_LParen,
    tok_RParen,
    tok_LBrack,
    tok_RBrack,
    tok_LBrace,
    tok_RBrace,
    tok_Comma,
    tok_Semi,

    tok_Let,    // '='
    tok_Next,   // ':='
    tok_Init,   // '@='

    tok_Add,
    tok_Sub,
    tok_Mul,
    tok_Div,
    tok_Mod,

    tok_Size,   // '#'
    tok_Dot,    // '.'

    tok_LT,
    tok_LE,
    tok_EQ,     // '=='
    tok_NE,
    tok_GE,
    tok_GT,

    tok_DEF,
    tok_CONST,
    tok_PROP,
    tok_IF,
    tok_ELSEIF,
    tok_FOR,

    TokType_size
};


cchar* Meh_TokType_name[TokType_size] = {
    "<null>",
    "Var",
    "Fun",
    "Sym",
    "Int",
    "Str",
    "LParen",
    "RParen",
    "LBrack",
    "RBrack",
    "LBrace",
    "RBrace",
    "Comma",
    "Semi",
    "Let",
    "Next",
    "Init",
    "Add",
    "Sub",
    "Mul",
    "Div",
    "Mod",
    "Size",
    "Dot",
    "LT",
    "LE",
    "EQ",
    "NE",
    "GE",
    "GT",
    "DEF",
    "CONST",
    "PROP",
    "IF",
    "ELSEIF",
    "FOR",
};


macro cchar* tokName(Meh_TokType t) { return Meh_TokType_name[t]; }
macro cchar* tokName(uint        t) { return Meh_TokType_name[t]; }


struct Meh_Tok {
    uchar   type;
    ushort  len;
    uint    offset;
    Meh_Tok(uchar type_, ushort len_, uint offset_) : type(type_), len(len_), offset(offset_) {}
};


struct Meh_Loc {
    String file;
    uint   line;
    Meh_Loc(String file_, uint line_) : file(file_), line(line_) {}
};


template<> fts_macro void write_(Out& out, const Meh_Loc& v)
{
    FWrite(out) "%_:%_", v.file, v.line;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Replace comments with spaces. Returns FALSE if 'text' contains an unterminated block comment.
static bool removeComments(Array<char> text)
{
    char* p   = &text[0];
    char* end = &text.end();

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


// support #include "..." ? If so, also support #line so that all includes can be preprocessed into a single text chunk


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class MehParser {
  //________________________________________
  //  Local types:

    typedef MehParser_TokType TokType;
    typedef MehParser_Tok     Tok;
    typedef MehParser_Loc     Loc;

  //________________________________________
  //  Internal state:

    Vec<String> pp_files;   // A list of files currently being read (to avoid cyclic includes)
    Vec<char>   text;       // Contains text after preprocessing
    Vec<Tok>    toks;       // Tokenized input; contain offsets into 'text'

  //________________________________________
  //  Major internal methods:

    void preprocess(String filename, String from_file = "", uint from_line = 0);
    void preprocessText(String filename, Array<char> raw);  // -- parse 'raw' (using 'filename' only for error messages)
    void tokenize();

public:
  //________________________________________
  //  Public interface:

    MehParser() {}

//    void read(String filename, /*out*/Vec<MehModule>& result, /*in*/Array<char> prelude);
};


//=================================================================================================
// -- Public methods:


#if 0
void VerilogParser::read(String filename, /*out*/Vec<VerilogModule>& result, /*in*/Array<char> prelude)
{
    if (prelude)
        preprocessText("<prelude>", prelude);
    preprocess(filename);
    tokenize();
    parse(result);
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
