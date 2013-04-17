//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Server.cc
//| Author(s)   : Niklas Een
//| Module      : Cluster
//| Description : Daemon running on server machine, talking to the drones and to the user(s). 
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Unix.hh"
#include "Cluster.hh"
#include "Server.hh"
#include <errno.h>
#include <syslog.h>


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Connect to drones:


static
void readConfig(String config_file, Vec<Pair<String,uint> >& addrs)
{
    InFile in(config_file);
    if (!in){
        ShoutLn "Cannot read configuration file: %_", config_file;
        exit(1);
    }

    Vec<char> text;
    uint line = 0;
    while (!in.eof()){
        // Read line:
        readLine(in, text);
        line++;
        uind pos = search(text, '#');
        if (pos != UIND_MAX)
            text.shrinkTo(pos);
        Str t = strip(text.slice());

        if (t.size() == 0) continue;

        addrs.push();
        pos = search(t, ':');
        if (pos == UIND_MAX){
            addrs[LAST].snd = 61453;        // -- default port
            addrs[LAST].fst = t;
        }else{
            try{
                addrs[LAST].snd = stringToUInt64(t.slice(pos+1));
                addrs[LAST].fst = t.slice(0, pos);
            }catch (Excp_ParseNum){
                ShoutLn "[%_:%_] Malformed port number.", config_file, line;
                exit(1);
            }
        }
    }
}


void connectToDrones(String config_file, Vec<int>& out_fds)
{
    Vec<Pair<String,uint> > addrs;
    readConfig(config_file, addrs);

    //WriteLn "%\r_", addrs;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#if 0
void serverLoop(int port)
{
    int sock_fd = setupSocket(port);

    signal(SIGCHLD, SIGCHLD_signalHandler);     // -- setup empty signal handler to make 'select()' abort on child process termination.

    syslog(LOG_INFO, "CL-client started.");

    Vec<char> pkg;
    int server_fd = -1;
    for(;;){
        // Listen for event:
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock_fd, &fds);
        if (server_fd != -1)
            FD_SET(server_fd, &fds);

        int n;
        if (child_sz > 0){
            n = -1;
        }else{
            struct timeval timeout;
            timeout.tv_sec = (uint)PROBE_INTERVAL;
            timeout.tv_usec = (PROBE_INTERVAL - (uint)PROBE_INTERVAL) * 1000000;
            n = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
            // <<== replace by top process?
        }

        // React to event:
        if (n == -1){
            // Child process terminated:
            while (child_sz > 0){
                child_sz--;
                // <<== report to server
                //WriteLn "Child died:  pid=%_  status=%_", child_pid[child_sz], child_stat[child_sz];
            }

        }else if (n == 0){
            // Heartbeat:

        }else{
            if (FD_ISSET(sock_fd, &fds)){
                // New connection:
                int fd = acceptConnection(sock_fd);
                syslog(LOG_NOTICE, "New server connection: fd=%d", fd);
                pkg.clear();

                if (server_fd != -1){
                    syslog(LOG_NOTICE, "Closing old server connection: fd=%d", server_fd);
                    shutdown(server_fd, SHUT_RDWR);
                    close(server_fd);
                }

                server_fd = fd;
                fcntl(fd, F_SETFL, O_NONBLOCK);
            }

            if (FD_ISSET(server_fd, &fds)){
                // Incoming data on socket:
                uint64 tot_size = 0;
                char buf[4096];
                for(;;){
                    errno = 0;
                    ssize_t size = read(server_fd, buf, sizeof(buf));
                    if (size < 0){
                        if (errno != EAGAIN)
                            syslog(LOG_ERR, "Unexpected error while reading data from server: %s", strerror(errno));
                        break;
                    }else if (size == 0){
                        if (tot_size == 0){
                            // First read was empty; assume socket is closed:
                            syslog(LOG_NOTICE, "Server disconnected: fd=%d", server_fd);
                            shutdown(server_fd, SHUT_RDWR);
                            close(server_fd);
                            server_fd = -1;
                        }
                        break;
                    }else{
                        for (int i = 0; i < size; i++)
                            pkg.push(buf[i]);
                        tot_size += size;
                    }
                }

                receivePackage(pkg, server_fd);     // -- will clear 'pkg' if complete
            }
        }
    }
}
#endif


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
