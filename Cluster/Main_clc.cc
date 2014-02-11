//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_clc.cc
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
#include "ZZ_CmdLine.hh"
#include "Common.hh"
#include "Client.hh"
#include <syslog.h>

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void myExit()
{
    syslog(LOG_INFO, "CL-client stopped.");
    closelog();
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("port", "uint", "61453", "Client port.");
    cli.add("restart", "bool", "no", "First kill existing client, if any.");
    cli.add("kill", "bool", "no", "Kill existing client without starting a new one.");
    cli.parseCmdLine(argc, argv);

    if (cli.get("restart").bool_val || cli.get("kill").bool_val){
        while (pid_t pid = findProcess(argv[0]))
            kill(pid, SIGTERM);
        if (cli.get("kill").bool_val)
            exit(0);    // -- done
    }else if (findProcess(argv[0])){
        ShoutLn "CL-client already running, use \a*-restart\a* to kill old process.";
        exit(1);
    }

    openlog("cl-client", LOG_PID, LOG_USER);

    atExit(x_Always, myExit);
    silent_interrupt = true;
    int ret ___unused = daemon(1, 1);
    clientLoop(cli.get("port").int_val);

    return 0;
}
