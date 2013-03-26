//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_punysat.cc
//| Author(s)   : Niklas Een
//| Module      : PunySat
//| Description :
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
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

    punySatTest(argc, argv);

    return 0;
}
