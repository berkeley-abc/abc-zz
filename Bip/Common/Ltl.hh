//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Ltl.hh
//| Author(s)   : Niklas Een
//| Module      : Common
//| Description : LTL expression parser (NuSMV style)
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Common__Ltl_hh
#define ZZ__Bip__Common__Ltl_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct LtlExpr {
    Str   atom;
    char  op;           // -- using '^  =  >' for 'xor  xnor/<->  ->' 
    LtlExpr* left;      // -- NULL for prefix op
    LtlExpr* right;     // -- NULL for postfix op

    LtlExpr(Str atom_) : atom(atom_), op(0), left(NULL), right(NULL) {}
    LtlExpr(char op_, LtlExpr* left_, LtlExpr* right_) : atom(Str_NULL), op(op_), left(left_), right(right_) {}
};


macro void dispose(LtlExpr* expr)
{
    if (expr){
        dispose(expr->left);
        dispose(expr->right);
        delete expr;
    }
}


template<> fts_macro void write_(Out& out, const LtlExpr& v)
{
    if (v.atom)
        out += v.atom;
    else if (v.left){
        if (v.right)
            Write "(%_ %_ %_)", *v.left, v.op, *v.right;
        else
            Write "(%_ %_)", *v.left, v.op;
    }else
        Write "(%_ %_)", v.op, *v.right;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


// Takes a zero-terminated string and returns an 'LtlExpr', or NULL on parse error 
// together with a formatted error string (through 'err_msg').
//
LtlExpr* parseLtl(cchar* text, String& err_msg);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
