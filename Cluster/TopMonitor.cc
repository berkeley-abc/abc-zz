//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TopMonitor.cc
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : Launch a 'top' process and extract output continuously from it.
//|
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "TopMonitor.hh"
#include "ZZ_Unix.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static cchar* top_config =
    "RCfile for \"top with windows\"\t\t# shameless braggin\'\n"
    "Id:a, Mode_altscr=0, Mode_irixps=1, Delay_time=3.000, Curwin=0\n"
    "Def\tfieldscur=AEHIOQTWKNMBcdfgJPLRSUVyZX\n"
    "\twinflags=62777, sortindx=10, maxtasks=0\n"
    "\tsummclr=1, msgsclr=1, headclr=3, taskclr=1\n"
    "Job\tfieldscur=ABcefgjlrstuvyzMKNHIWOPQDX\n"
    "\twinflags=62777, sortindx=0, maxtasks=0\n"
    "\tsummclr=6, msgsclr=6, headclr=7, taskclr=6\n"
    "Mem\tfieldscur=ANOPQRSTUVbcdefgjlmyzWHIKX\n"
    "\twinflags=62777, sortindx=13, maxtasks=0\n"
    "\tsummclr=5, msgsclr=5, headclr=4, taskclr=5\n"
    "Usr\tfieldscur=ABDECGfhijlopqrstuvyzMKNWX\n"
    "\twinflags=62777, sortindx=4, maxtasks=0\n"
    "\tsummclr=3, msgsclr=3, headclr=2, taskclr=3\n"
    ;

struct TopHead {
};

struct TopProc {
    uint    pid;
    uint    ppid;
    String  user;
};


void parseTopInfo(Str text, TopHead& out_header, Vec<TopProc>& out_procs)
{

}


TopMonitor::TopMonitor(double freq)
{
    // Create config file:
    String top_rc = homeDir() + "/.cl-top-rc";
    OutFile out(top_rc);
    FWrite(out) "%_", top_config;
    out.close();

    // Run 'top':
    String top_bin = "/usr/bin/top";
    Vec<String> args;
    args += "-ib", ((FMT "-d%_", freq));
    Vec<String> env;
    env += ((FMT "TOPRC=%_", top_rc));//, "COLUMNS=512";

    if (!startProcess(top_bin, args, pid, io, env))
        pid = 0;

#if 1
    char buf[4096];
    for(;;){
        ssize_t n = read(io[1], buf, sizeof(buf));
        if (n == 0) break;
        ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
    }
#endif
}


Str TopMonitor::getLatest()
{
    char buf[4096];
    for(;;){
        ssize_t n = read(io[1], buf, sizeof(buf));
        if (n == 0) break;
        ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
    }
    return Str_NULL;    // <<== later
}

/*
top - 18:26:17 up 20 days, 22:28, 23 users,  load average: 0.13, 0.27, 0.32
Tasks: 293 total,   1 running, 289 sleeping,   0 stopped,   3 zombie
Cpu(s):  1.4%us,  0.3%sy,  0.0%ni, 98.2%id,  0.1%wa,  0.0%hi,  0.0%si,  0.0%st
Mem:   8253812k total,  4231740k used,  4022072k free,   470756k buffers
Swap:  9799608k total,        0k used,  9799608k free,  2144664k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+   PPID P SWAP   TIME CODE DATA nFLT nDRT Flags    COMMAND                                                                                                                                                                                                                                                                                                                                                                                                           
26865 een       20   0  2680 1204  820 R    2  0.0   0:00.01 26864 5 1476   0:00   64  612    0    0 ..4.2... top                                                                                                                                                                                                                                                                                                                                                                                                               
*/


/*
  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+   PPID P SWAP   TIME CODE DATA nFLT nDRT Flags    COMMAND                                   
    1 root      20   0  2792 1720 1232 S    0  0.0   0:05.10     0 0 1072   0:05  104  516   22    0 ..4.21.. init                                       
    2 root      20   0     0    0    0 S    0  0.0   0:00.01     0 1    0   0:00    0    0    0    0 8.2.a.4. kthreadd                                   
    3 root      RT   0     0    0    0 S    0  0.0   0:00.02     2 0    0   0:00    0    0    0    0 842.a.4. migration/0                                
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
