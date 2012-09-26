//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : BoolExpr.cc
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Parser for boolean expressions in Liberty files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_BFunc.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Parser class:


struct SC_ExprParser {
    Str p;         // -- current parsing point (a suffix-slice of the original string)
    const Vec<Str>& var_names;

    void    skipWS();
    BoolFun parse_Atom();
    BoolFun parse_InvExpr();
    BoolFun parse_XorExpr();
    BoolFun parse_AndExpr();
    BoolFun parse_Expr();

    SC_ExprParser(Str func_text, const Vec<Str>& var_names_) :
        p(func_text), var_names(var_names_) {}
};


BoolFun parseBoolExpr(Str text, const Vec<Str>& var_names)
{
    SC_ExprParser P(text, var_names);
    return P.parse_Expr();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Implementation:


inline void SC_ExprParser::skipWS() {
    while (p.size() > 0 && isWS(*p)) p++; }


BoolFun SC_ExprParser::parse_Atom()
{
    skipWS();
    if (p.size() == 0) Throw(Excp_ParseError) "Unexpected end of string while parsing boolean expression.";

    if (*p == '0'){
        p++;
        return BoolFun(var_names.size(), false);

    }else if (*p == '1'){
        p++;
        return BoolFun(var_names.size(), true);

    }else if (*p == '('){
        p++;
        BoolFun ret = parse_Expr();
        skipWS();
        if (p.size() == 0 || *p != ')') Throw(Excp_ParseError) "Expected ')' in boolean expression ending in: %_", p;
        p++;
        return ret;

    }else{
        if (!isIdentChar0(*p)) Throw(Excp_ParseError) "Expected identifer in boolean expression ending in: %_", p;
        const char& start = p[0];
        p++;

        while (p.size() > 0 && isIdentChar(*p))
            p++;

        // Lookup name:
        Str name = slice(start, p[0]);
        for (uint i = 0; i < var_names.size(); i++){
            if (eq(name, var_names[i]))
                return BoolFun(var_names.size(), Lit(i));
        }

        Throw(Excp_ParseError) "Invalid variable in boolean expression: %_", name;
        return BoolFun();   // -- get rid of compiler warning
    }
}


BoolFun SC_ExprParser::parse_InvExpr()
{
    bool sign = false;
    for(;;){
        skipWS();
        if (p.size() == 0 || *p != '!') break;

        p++;
        sign = !sign;
    }

    BoolFun expr = parse_Atom(); assert_debug(expr);

    for(;;){
        skipWS();
        if (p.size() == 0 || *p != '\'') break;

        p++;
        sign = !sign;
    }

    return sign ? ~expr : expr;
}


BoolFun SC_ExprParser::parse_XorExpr()
{
    BoolFun lft = parse_InvExpr(); assert_debug(lft);
    for(;;){
        skipWS();
        if (p.size() == 0 || *p != '^') break;

        p++;
        BoolFun rht = parse_InvExpr(); assert_debug(rht);
        lft ^= rht;
    }

    return lft;
}


BoolFun SC_ExprParser::parse_AndExpr()
{
    BoolFun lft = parse_XorExpr(); assert_debug(lft);
    for(;;){
        skipWS();
        if (p.size() == 0) break;
        if (*p == '&' || *p == '*')
            p++;
        else if (*p == '!' || *p == '(' || isIdentChar0(*p))
            ;   // -- juxtaposition == AND
        else
            break;

        BoolFun rht = parse_XorExpr(); assert_debug(rht);
        lft &= rht;
    }

    return lft;
}


BoolFun SC_ExprParser::parse_Expr()
{
    BoolFun lft = parse_AndExpr(); assert_debug(lft);
    for(;;){
        skipWS();
        if (p.size() == 0 || (*p != '|' && *p != '+')) break;

        p++;
        BoolFun rht = parse_AndExpr(); assert_debug(rht);
        lft |= rht;
    }

    assert_debug(lft);
    return lft;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
