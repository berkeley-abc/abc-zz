//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Processes.hh
//| Author(s)   : Niklas Een
//| Module      : Unix
//| Description : Wrappers for process handling.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Unix__Processes_hh
#define ZZ__Unix__Processes_hh

#include <sys/types.h>
#include <sys/wait.h>

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


bool startProcess(const String& exec, const Vec<String>& args, int& out_pid, int out_std[3]);
bool startProcess(const String& exec, const Vec<String>& args, int& out_pid, int out_std[3], const Vec<String>& env);
void closeChildIo(int out_std[3]);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
