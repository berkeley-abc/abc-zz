//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : LtlCheck.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : LTL checking based on circuit monitor synthesis.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "LtlCheck.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void ltlCheck(NetlistRef N, const LtlExpr* spec)
{
    WriteLn "SPEC: %_", *spec;
}


void ltlCheck(NetlistRef N, String spec_file, uint prop_no)
{
    // Parse property:
    Array<char> text = readFile(spec_file, true);
    bool comment = false;
    uint curr_prop = 0;
    for (uint i = 0; i < text.size(); i++){
        if (text[i] == '\n')
            comment = false;
        else if (comment)
            text[i] = ' ';
        else if (text[i] == '#')
            comment = true,
            text[i] = ' ';
        else if (text[i] == ';')
            curr_prop++,
            text[i] = ' ';
        else if (curr_prop != prop_no)
            text[i] = ' ';
    }

    String err_msg;
    LtlExpr* spec = parseLtl(text.base(), err_msg);

    if (!spec){
        ShoutLn "Error parsing LTL specification %_:\n  -- %_", prop_no, err_msg;
        exit(0);
    }

    ltlCheck(N, spec);
    dispose(spec);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
