//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Main_cls.cc
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Common.hh"
#include "Client.hh"
#include "Server.hh"
#include <syslog.h>

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


void myExit()
{
    syslog(LOG_INFO, "CL-server stopped.");
    closelog();
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("port", "uint", "61454", "Server port.");
    cli.add("restart", "bool", "no", "First kill existing server, if any.");
    cli.add("conf", "string", "/etc/cls.conf", "Location of configuration file.");
    cli.add("kill", "bool", "no", "Kill existing server without starting a new one.");
    cli.add("defaults", "bool", "no", "Show default job limits then exit.");
    cli.parseCmdLine(argc, argv);

    if (cli.get("defaults").bool_val){
        Job job;
        job.prettyPrint(std_out);
        exit(0);        // -- done;
    }

    if (cli.get("restart").bool_val || cli.get("kill").bool_val){
        while (pid_t pid = findProcess(argv[0]))
            kill(pid, SIGTERM);
        if (cli.get("kill").bool_val)
            exit(0);    // -- done
    }else if (findProcess(argv[0])){
        ShoutLn "CL-server already running, use \a*-restart\a* to kill old process.";
        exit(1);
    }

    openlog("cl-server", LOG_PID, LOG_USER);

    atExit(x_Always, myExit);
    silent_interrupt = true;

    Vec<int> drone_fds;
    connectToDrones(cli.get("conf").string_val, drone_fds);

    //int ret ___unused = daemon(1, 1);
    //serverLoop(cli.get("port").int_val);

    return 0;
}
