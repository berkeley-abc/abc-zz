//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Server.hh
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : Daemon running on server machine, talking to the drones and to the user(s). 
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Cluster__Server_hh
#define ZZ__Cluster__Server_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void connectToDrones(String config_file, Vec<int>& out_fds);


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
