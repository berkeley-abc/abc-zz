//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Client.hh
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Cluster__Client_hh
#define ZZ__Cluster__Client_hh

#include "Cluster.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


void clientLoop(int port);   // -- will listen for connection from server on given port

void cl_launch(int fd, const Job& job);
void cl_pause (int fd, uint64 job_id);
void cl_resume(int fd, uint64 job_id);
void cl_kill  (int fd, uint64 job_id);

/*
Information going in other direction:

  - failed to launch job
  - job finished
  - job disturbed (and paused/aborted): command line of disturbing process(es)
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
