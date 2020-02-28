//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Effort.hh
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Callback functor base class for the verification procedures.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Bip__Effort_hh
#define ZZ__Bip__Effort_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct EffortCB {
    uint64 virt_time;   // Will be increased by the method calling the callback function.
    void*  info;        // Exact type depends on method.   

    virtual bool operator()() { return true; }
        // -- Return TRUE to continue work, FALSE to abort.

    EffortCB() : virt_time(0), info(NULL) {}
    virtual ~EffortCB() {}
};


struct EffortCB_Timeout : EffortCB {
    uint64 vt_limit;
    double cpu_limit;
    EffortCB_Timeout(uint64 vt_limit_, double cpu_limit_ = DBL_MAX) : vt_limit(vt_limit_), cpu_limit(cpu_limit_) {}

//  bool operator()() { WriteLn "CB: virt_time=%_   cpu_time=%.3f   limit=%_", virt_time, cpuTime(), vt_limit; return virt_time < vt_limit; }
    bool operator()() { return virt_time < vt_limit && (cpu_limit == DBL_MAX || cpuTime() < cpu_limit); }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wrapper for SAT callbacks:


static bool satEffortCB(uint64 work, void* data) ___unused;
static bool satEffortCB(uint64 work, void* data)
{
    EffortCB* cb = static_cast<EffortCB*>(data);
    cb->virt_time += work;
    return (*cb)();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
