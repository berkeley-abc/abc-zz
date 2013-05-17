//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Ltl.cc
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : LTL expression parser (NuSMV style)
//|
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Ltl.hh"
#include "ZZ/Generics/ExprParser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
                                                                                                 /*

expr :: simple_expr     -- a simple boolean expression
    | ( expr )
    | ! expr            -- logical not
    | ~ expr            -- alt. syntax for "not" (converted to !)
    | expr & expr       -- logical and
    | expr | expr       -- logical or
    | expr xor expr     -- logical exclusive or
    | expr xnor expr    -- logical NOT exclusive or (same as <-> but different precedence)
    | expr <-> expr     -- logical equivalence
    | expr -> expr      -- logical implies (right associative)
    
    -- FUTURE
    | X expr            -- next state
    | G expr            -- globally
    | F expr            -- finally
    | expr U expr       -- until (left assoc.)
    | expr R expr       -- releases
    | expr V expr       -- alt. syntax for "releases" (converted to R)
    | expr W expr       -- weak until
    
    -- PAST
    | Y expr            -- previous state
    | Z expr            -- not previous state not
    | H expr            -- historically
    | P expr            -- once 
    | O expr            -- alt. syntax for "once" (converted to P)
    | expr S expr       -- since
    | expr T expr       -- triggered
    | expr M expr       -- weak since (extension)

Precedence:

    ! F G X Y Z H O P     (50)
    U V W R S T M         (40)
    &                     (30)
    | xor xnor            (20)   
    <->                   (10)
    ->                     (0)

Operators of equal precedence associate to the left, except -> that associates to the right.

Single charatcter operator substitutions:

   ->      >
   <->     =
   xnor    =
   xor     ^
   O       P

Also scope operator '$' is used elsewhere but not yet introduced in this parser.   
   
                                                                                                 */
//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct LtlTokenizer : XP_TokenStream {
    cchar* p0;
    cchar* p;
    NetlistRef N;
    LtlTokenizer(cchar* p_, NetlistRef N_) : p0(p_), p(p_), N(N_) {}

    void skipWS() { while (isWS(*p)) p++; }

    bool parseOp(uint& op_tag, uint& pos, XP_OpType& type, int& prio){
        skipWS();
        pos = uint(p - p0);
        switch (*p){
        case '!': case '~': case 'F': case 'G': case 'X': case 'Y': case 'Z': case 'H': case 'O': case 'P':
            op_tag = *p++; prio = 50; type = xop_PREFIX;
            if (op_tag == 'O') op_tag = 'P';
            if (op_tag == '~') op_tag = '!';
            return true;

        case 'U': case 'V': case 'W': case 'R': case 'S': case 'T': case 'M':
            op_tag = *p++; prio = 40; type = xop_INFIXL;
            if (op_tag == 'V') op_tag = 'R';
            return true;

        case '&':
            op_tag = *p++; prio = 30; type = xop_INFIXL;
            return true;

        case '|':
            op_tag = *p++; prio = 20; type = xop_INFIXL;
            return true;

        case 'x':
            if (p[1] == 'o' && p[2] == 'r' && !isIdentChar(p[3])){
                p += 3; op_tag = '^'; prio = 20; type = xop_INFIXL;
                return true;
            }else if (p[1] == 'n' && p[2] == 'o' && p[3] == 'r' && !isIdentChar(p[4])){
                p += 4; op_tag = '='; prio = 20; type = xop_INFIXL;
                return true;
            }else
                return false;

        case '<':
            if (p[1] == '-' && p[2] == '>' && !isIdentChar(p[3])){
                p += 3; op_tag = '='; prio = 10; type = xop_INFIXL;
                return true;
            }else
                return false;

        case '-':
            if (p[1] == '>' && !isIdentChar(p[2])){
                p += 2; op_tag = '>'; prio = 0; type = xop_INFIXR;
                return true;
            }else
                return false;


        default: return false; }
    }

    bool parseLParen(uint& paren_tag, uint& pos){
        skipWS();
        pos = uint(p - p0);
        if (*p == '('){
            p++;
            return true;
        }else
            return false;
    }

    bool parseRParen(uint& paren_tag, uint& pos){
        skipWS();
        pos = uint(p - p0);
        if (*p == ')'){
            p++;
            return true;
        }else
            return false;
    }

    void* toExpr(GLit p)     const { return (void*)(uintp)p.data(); }
    Wire  toWire(void* expr) const { return GLit(packed_, (uint)(uintp)expr) + N; }

    bool parseAtom(void*& atom_expr, uint& pos){
        skipWS();
        pos = uint(p - p0);
        cchar* start = p;
        while (isIdentChar(*p) || *p == '[' || *p == ']' || *p == '.' || *p == '=' || (*p == '!' && *(p+1) == '='))
            // -- allow for identifiers + array index + hierarchical name + enum comparison
            p++;
        if (p == start)
            return false;
        else{
            Str name = slice(*start, *p);
            GLit p = N.names().lookup(name);
            if (!p){
                p = N.add(Ltl_());
                N.names().add(p, name); }
            atom_expr = toExpr(p);
            return true;
        }
    }

    void* applyPrefix (uint op_tag, void* expr)               { return toExpr(N.add(Ltl_(op_tag), Wire_NULL, toWire(expr))); }
    void* applyPostfix(uint op_tag, void* expr)               { return toExpr(N.add(Ltl_(op_tag), toWire(expr), Wire_NULL)); }
    void* applyInfix  (uint op_tag, void* expr0, void* expr1) { return toExpr(N.add(Ltl_(op_tag), toWire(expr0), toWire(expr1))); }
    void  disposeExpr (void* expr)                            {}

    String nameLParen(uint paren_tag) { return String("("); }
    String nameRParen(uint paren_tag) { return String(")"); }
    String nameOp    (uint op_tag)    { String s; s += (char)op_tag; return s; }
    String namePos   (uint pos)       { String s; FWrite(s) "char %_", pos; return s; }
};


Wire parseLtl(cchar* text, NetlistRef N, String& err_msg)
{
    N.names().enableLookup();

    LtlTokenizer tok(text, N);
    void* expr = tok.parse(err_msg);

    if (expr == NULL){
        if (err_msg == "")
            err_msg = String("Illegal first character (or empty expression).");
        return Wire_NULL;
    }else if (*tok.p != '\0'){
        err_msg = (FMT "[char %_] Extra characters after expression.", tok.p - tok.p0);
        return Wire_NULL;
    }

    return tok.toWire(expr);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
