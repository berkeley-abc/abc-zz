//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Client.hh
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
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
// Start function:


void clientLoop(int port);   // -- will listen for connection from server on given port


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Server->client communication:


bool cl_launch(int fd, const Job& job);
bool cl_pause (int fd, uint64 job_id);
bool cl_resume(int fd, uint64 job_id);
bool cl_kill  (int fd, uint64 job_id);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Client->server communication:


enum ClMsgType {
    clmsg_LaunchSucceeded,
    clmsg_LaunchFailed,
    clmsg_JobFinished,
    clmsg_JobDisturbed_Paused,
    clmsg_JobDisturbed_Killed,
};


struct ClMsg {
    ClMsgType   type;       // -- message type
    uint64      id;         // -- job ID
    uint64      data;       // -- extra data connected to this message (used by 'LaunchFailed')

    ClMsg() {}
    ClMsg(ClMsgType type_, uint64 id_, uint64 data_ = 0) : type(type_), id(id_), data(data_) {}
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
