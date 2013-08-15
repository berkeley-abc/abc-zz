//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cnf4.hh
//| Author(s)   : Niklas Een
//| Module      : Npn4
//| Description : Tables for LUT4->CNF conversion.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| "isop" contains a minimal CNF encoding (irredundant sum-of-product)
//| "prime" contains all prime implicants (BCP complete).
//|________________________________________________________________________________________________

#ifndef ZZ__Npn4__Cnf4_hh
#define ZZ__Npn4__Cnf4_hh

#include "ZZ/Generics/Lit.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Tables:


struct Cnf4Head {
    ushort offset;
    uchar  n_clauses;
    uchar  n_pos_clauses;
};


extern Cnf4Head cnf4_isop_header[223];
extern uchar    cnf4_isop_data[1858];

extern Cnf4Head cnf4_prime_header[223];
extern uchar    cnf4_prime_data[2305];


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Interface:


macro uint cnfIsop_size   (uint cl) { return cnf4_isop_header[cl].n_clauses; }
macro uint cnfIsop_sizePos(uint cl) { return cnf4_isop_header[cl].n_pos_clauses; }
macro uint cnfIsop_sizeNeg(uint cl) { return cnf4_isop_header[cl].n_clauses - cnf4_isop_header[cl].n_pos_clauses; }

macro uint cnfPrime_size   (uint cl) { return cnf4_prime_header[cl].n_clauses; }
macro uint cnfPrime_sizePos(uint cl) { return cnf4_prime_header[cl].n_pos_clauses; }
macro uint cnfPrime_sizeNeg(uint cl) { return cnf4_prime_header[cl].n_clauses - cnf4_prime_header[cl].n_pos_clauses; }


// Produce the i:th clause for NPN class 'cl' (in 0..221). Will clear 'clause' first.
// Output literal is always first.
macro void cnfIsop_clause(uint cl, uint i, Lit inputs[4], Lit output, /*out*/Vec<Lit>& clause)
{
    assert_debug(i < cnfIsop_size(cl));

    clause.clear();
    clause.push(output ^ (i >= cnfIsop_sizePos(cl)));

    uchar mask = cnf4_isop_data[cnf4_isop_header[cl].offset + i];
    for (uint i = 0; i < 4; i++){
        if (mask & 3)
            clause.push(inputs[i] ^ !(mask & 1));
        mask >>= 2;
    }
}


macro void cnfPrime_clause(uint cl, uint i, Lit inputs[4], Lit output, /*out*/Vec<Lit>& clause)
{
    assert_debug(i < cnfPrime_size(cl));

    clause.clear();
    clause.push(output ^ (i >= cnfPrime_sizePos(cl)));

    uchar mask = cnf4_prime_data[cnf4_prime_header[cl].offset + i];
    for (uint i = 0; i < 4; i++){
        if (mask & 3)
            clause.push(inputs[i] ^ !(mask & 1));
        mask >>= 2;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
