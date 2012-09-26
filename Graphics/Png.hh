//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Png.hh
//| Author(s)   : Niklas Een
//| Module      : Graphics
//| Description : Read and write PNG files.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Graphics__Png_hh
#define ZZ__Graphics__Png_hh

#include "Bitmap.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool readPng (String filename, Bitmap& bm);
bool writePng(String filename, const Bitmap& bm);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
