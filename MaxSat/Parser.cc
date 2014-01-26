//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Parser.cc
//| Author(s)   : Niklas Een
//| Module      : MaxSat
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Parser.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
/*


c
c comments Max-SAT
c
p cnf 3 4
1 -2 0
-1 2 -3 0
-3 2 0
1 3 0

c
c comments Weighted Max-SAT
c
p wcnf 3 4
10 1 -2 0
3 -1 2 -3 0
8 -3 2 0
5 1 3 0



c
c comments Weigthed Partial Max-SAT
c
p wcnf 4 5 16
16 1 -2 4 0
16 -1 -2 3 0
8 -2 -4 0
4 -3 2 0
3 1 3 0


*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// DIMACS Parser:


// Push literals onto 'lits' (no clearing).    
static
void readClause(In& in, Vec<Lit>& lits, uint64* weight)
{
    skipWS(in);
    if (weight){
        *weight = parseInt64(in);
        skipWS(in);
    }
    for (;;){
        int64 n = parseInt64(in);
        skipWS(in);
        if (n == 0) break;
        lits += (n > 0) ? Lit((uint)n) : ~Lit((uint)(-n));
    }
}


static
void parse_DIMACS_(In& in, MaxSatProb& P)
{
    bool p_line = false;
    uint n_clauses = 0;
    uint64 top_weight = 0;
    bool weighted = false;

    for(;;){
        skipWS(in);
        if (in.eof()){
            break;

        }else if (*in == 'c'){
             skipLine(in);

        }else if (*in == 'p'){
            #define P_Assert(cond) if (!(cond)) Throw(Excp_ParseError) "Incorrect p-line"
            Vec<char> text;
            readLine(in, text);
            Vec<Str> fs;
            splitArray(text.slice(), " ", fs);
            assert(fs.size() > 0);

            P_Assert(eq(fs[0], "p"));
            if (eq(fs[1], "cnf")){
                P_Assert(fs.size() == 4);
                P.n_vars  = stringToUInt64(fs[2], 0, UINT_MAX);
                n_clauses = stringToUInt64(fs[3], 0, UINT_MAX);
            }else{
                P_Assert(eq(fs[1], "wcnf"));
                P_Assert(fs.size() == 4 || fs.size() == 5);
                weighted = true;
                P.n_vars  = stringToUInt64(fs[2], 0, UINT_MAX);
                n_clauses = stringToUInt64(fs[3], 0, UINT_MAX);
                if (fs.size() == 5)
                    top_weight = stringToUInt64(fs[4]);
            }
            p_line = true;
            #undef P_Assert

        }else{
            if (!p_line)
                Throw(Excp_ParseError) "Missing 'p cnf <#vars> <#clauses>' line.";

            P.weight.push(1);
            P.off.push(P.lits.size());
            readClause(in, P.lits, weighted ? &P.weight.last() : (uint64*)NULL);
        }
    }
    P.off.push(P.lits.size());

    for (uint i = 0; i < P.size(); i++){
        if (P.weight[i] == top_weight)
            P.weight[i] = UINT64_MAX;
    }

//    if (P.size() != n_clauses)
//        Throw(Excp_ParseError) "Number of clauses does not match 'p cnf <#vars> <#clauses>' line.";
}


void parse_DIMACS(In& in, MaxSatProb& P)
{
    try{
        parse_DIMACS_(in, P);
    }catch (Excp_ParseNum){
        Throw(Excp_ParseError) "Input contains invalid number.";
    }
}


void parse_DIMACS(String filename, MaxSatProb& P)
{
    InFile in(filename);
    if (!in)
        Throw(Excp_ParseError) "Could not open file: %_", filename;
    parse_DIMACS(in, P);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
