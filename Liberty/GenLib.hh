//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : GenLib.hh
//| Author(s)   : Niklas Een
//| Module      : Liberty
//| Description : Convert Liberty files to .genlib files (losing most information).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Liberty__GenLib_hh
#define ZZ__Liberty__GenLib_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool writeGenlibFile(String filename, const SC_Lib& L, bool quiet = false);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
