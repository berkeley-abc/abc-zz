//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_punysat.cc
//| Author(s)   : Niklas Een
//| Module      : PunySat
//| Description :
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "PunySat.hh"

#include <emmintrin.h>

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    struct rlimit res;
    int result;

    result = getrlimit(RLIMIT_STACK, &res);
    if (result == 0){
        res.rlim_cur = 64L * 1024L * 1024L;
        result = setrlimit(RLIMIT_STACK, &res);
        if (result != 0){
            ShoutLn "Stack size could not be increased. Error code from 'setrlimit()': %_", result;
            exit(1); }
    }

    punySatTest(argc, argv);

    return 0;
}
