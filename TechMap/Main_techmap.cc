//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_techmap.cc
//| Author(s)   : Niklas Een
//| Module      : TechMap
//| Description : Stand alone binary for calling technology mapper.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


namespace ZZ {
void test(int argc, char** argv);
}


int main(int argc, char** argv)
{
    ZZ_Init;

    test(argc, argv);

    return 0;
}
