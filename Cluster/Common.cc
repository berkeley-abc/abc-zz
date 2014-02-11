//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Common.cc
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "Common.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
bool isNumber(const String& text)
{
    for (uint i = 0; i < text.size(); i++)
        if (!isDigit(text[i]))
            return false;
    return true;
}


// Returns process ID of 'exec', different from myself, or 0 if not found.
pid_t findProcess(cchar* exec)
{
    Vec<FileInfo> files, dirs;
    readDir("/proc", files, dirs);

    pid_t my_pid = getpid();
    for (uint i = 0; i < dirs.size(); i++){
        if (isNumber(dirs[i].name)){
            pid_t pid = stringToUInt64(dirs[i].name);
            if (pid == my_pid) continue;

            File in(dirs[i].full + "/cmdline", "r");
            Str text = readFile(in, true, 256);

            if (strcmp(exec, text.base()) == 0)
                return pid;
        }
    }

    return 0;
}




//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
