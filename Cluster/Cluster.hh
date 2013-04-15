//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cluster.hh
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Cluster__Cluster_hh
#define ZZ__Cluster__Cluster_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Job:


const uint64 job_NULL = 0;
const float  no_timeout = FLT_MAX;
const uint64 no_memout  = UINT64_MAX;


struct Job {
    uint64      id;     // Job ID
    uint        prio;   // Job priority (positive): higher runs first, 0 means "put on hold".
    uint        conc;   // Concurrency: number of other jobs that can run in parallel.
    String      group;  // <<== remove
    String      batch;  // Only processes from the same batch may run concurrently

    String      exec;   // Name of executable (full path)
    Vec<String> args;   // Arguments to pass to executable
    String      cwd;    // Current working directory
    Vec<String> env;    // Environment: vector of strings "key=value"

    float       real;   // Real time limit
    float       cpu;    // CPU time limit
    uint64      mem;    // Memory limit

    String      stdin;  // File to read standard input data from
    String      stdout; // File to write standard output data to
    String      stderr; // File to write standard error data to
    String      status; // File to write status to
    String      topmon; // File to write "top" data to

    // + behavior when other processes are disturbing this one (after X seconds, pause until no activity for Y seconds (or restart))

    Job() :
        id(job_NULL),
        prio(0),
        conc(1),
        real(no_timeout),
        cpu(no_timeout),
        mem(no_memout)
    {}

    void serialize(Out& out) const;
    void deserialize(In& in);
    void prettyPrint(Out& out) const;
    void readConf(String filename);      // contains lines "key = value" or "key += value" (for 'env' and 'args')
};


template<> fts_macro void write_(Out& out, const Job& v) {
    v.prettyPrint(out); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
