//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TopMonitor.hh
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : Launch a 'top' process and extract output continuously from it.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Cluster__TopMonitor_hh
#define ZZ__Cluster__TopMonitor_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


class TopMonitor {
    int io[3];
    Vec<char> buf;

public:
    TopMonitor(double freq); // -- 'freq' is in seconds (the '-d' parameter of top)

    int pid;                 // -- process ID for "top" (or 0 if failed to start 'top')
    int fd;                  // -- use this 'fd' in your 'select()' loop
    Str getLatest();         // -- then call 'getLatest()' if input on 'fd' (may return Str_NULL if input is not yet complete)
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
