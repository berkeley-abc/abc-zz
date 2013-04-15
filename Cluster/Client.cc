//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Client.cc
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
#include "ZZ_Unix.hh"
#include "ZZ_Md5.hh"
#include "Cluster.hh"
#include "Client.hh"


#include <errno.h>

#define PROBE_INTERVAL 0.5

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// ReqHeader:


struct ReqHeader {
    char  username[32];
    uchar ssh_hash[16];     // 'fst' then 'snd' part of 'md5_hash' uint64s, both in little endian order.
    uchar pkg_len[8];       // Little endian 64-bit number: length of package NOT including this header.
    uchar pkg_tag[4];       // Little endian 32-bit number encoding 'ReqType'.
};

enum ReqType {
    req_NULL,
    req_Launch,
    req_Pause,
    req_Resume,
    req_Kill,
};


static
String username(const ReqHeader& req)
{
    String text;
    for (uint i = 0; i < sizeof(req.username); i++){
        if (req.username[i] == 0) break;
        text.push(req.username[i]);
    }
    return text;
}


static
uint64 pkgLen(const ReqHeader& req)
{
    return (uint64)req.pkg_len[0] | ((uint64)req.pkg_len[1] << 8) | ((uint64)req.pkg_len[2] << 16) | ((uint64)req.pkg_len[3] << 24)
         | ((uint64)req.pkg_len[4] << 32) | ((uint64)req.pkg_len[5] << 40) | ((uint64)req.pkg_len[6] << 48) | ((uint64)req.pkg_len[7] << 56);
}


static
ReqType pkgTag(const ReqHeader& req)
{
    return ReqType((uint)req.pkg_tag[0] | ((uint)req.pkg_tag[1] << 8) | ((uint)req.pkg_tag[2] << 16) | ((uint)req.pkg_tag[3] << 24));
}


void makeRequest(ReqHeader& req, String user, uint64 len, uint tag)
{
    for (uint i = 0; i < sizeof(req.username); i++)
        req.username[i] = (i < user.size()) ? user[i] : 0;

    Str text = readFile(homeDir(user) + "/.ssh/id_rsa");
    if (!text)
        memset(req.ssh_hash, 0, sizeof(req.ssh_hash));
    else{
        md5_hash h = md5(text);
        for (uint i = 0; i < 8; i++)
            req.ssh_hash[i] = h.fst >> (8 * i);
        for (uint i = 0; i < 8; i++)
            req.ssh_hash[8+i] = h.snd >> (8 * i);
    }

    for (uint i = 0; i < 8; i++)
        req.pkg_len[i] = len >> (8 * i);
    for (uint i = 0; i < 4; i++)
        req.pkg_tag[i] = tag >> (8 * i);
}


static
bool validateRequest(const ReqHeader& req)
{
    ReqHeader tmp;
    makeRequest(tmp, username(req), 0, 0);
    return memcmp(req.ssh_hash, tmp.ssh_hash, sizeof(req.ssh_hash)) == 0;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Server->Client communication:


void cl_launch(int fd, const Job& job)
{
    String pkg;
    job.serialize(pkg);

    ReqHeader req;
    makeRequest(req, userName(), pkg.size(), req_Launch);

    ssize_t m = write(fd, &req, sizeof(req));
    ssize_t n = write(fd, pkg.base(), pkg.size());

    if (m != sizeof(req) || n != (ind)pkg.size()){
        ShoutLn "ERROR! 'cl_launch()' failed to send whole package.";
        exit(1); }
}


void cl_pause(int fd, uint64 job_id)
{

}


void cl_resume(int fd, uint64 job_id)
{

}


void cl_kill(int fd, uint64 job_id)
{

}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Launch job:


void launchJob(const String& username, const Job& job)
{
    ProcMode mode;
    if (username != "" && username != userName())
        mode.username = username;
    mode.dir = job.cwd;
    mode.own_group = true;
    mode.timeout = job.cpu;     // -- real-time is monitored in client loop
    mode.memout = job.mem;
    mode.stdin_file = job.stdin;
    mode.stdout_file = job.stdout;
    mode.stderr_file = job.stderr;

    pid_t child_pid;
    int child_io[3];
    char ret = startProcess(job.exec, job.args, child_pid, child_io, job.env, mode);

    /**/if (ret != 0) WriteLn "Failed to run '%_' with args '%_'. Error code: %_", job.exec, job.args, ret;

    char buf[4096];
    for(;;){
        ssize_t n = read(child_io[1], buf, sizeof(buf));
        if (n == 0) break;
        ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
void removePrefix(Vec<char>& v, uind len)
{
    for (uind i = len; i < v.size(); i++)
        v[i-len] = v[i];
    v.shrinkTo(v.size() - len);
}


void receivePackage(Vec<char>& pkg)
{
    for(;;){
        if (pkg.size() < sizeof(ReqHeader))
            return;
        const ReqHeader& req = *reinterpret_cast<const ReqHeader*>(pkg.base());

        if (!validateRequest(req)){
            WriteLn "WARNING! Spurious request.";
            pkg.clear();
            return; }

        // Get package size:
        uint64 len = pkgLen(req);

        if (pkg.size() >= sizeof(ReqHeader) + len){
            // Received a complete package:
            In in(pkg.slice(sizeof(ReqHeader)));

            switch (pkgTag(req)){
            case req_Launch: {
                Job job;
                job.deserialize(in);
                launchJob(username(req), job);
                removePrefix(pkg, sizeof(ReqHeader) + len);
                break;}

            case req_Pause:{

                break; }

            case req_Kill:{
                break; }

            default:
                WriteLn "INTERNAL ERROR! Invalid package tag in 'receivePackage()': %_", (uint)pkgTag(req);
                exit(1);
            }

        }else
            return;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static pid_t child_pid[1024];
static int   child_stat[1024];
static uint  child_sz;

ZZ_Initializer(child, 0){ child_sz = 0; }


static void SIGCHLD_signalHandler(int signum)
{
    child_pid[child_sz] = wait(&child_stat[child_sz]);
    child_sz++;
}


void clientLoop(int port)
{
    int sock_fd = setupSocket(port);

    signal(SIGCHLD, SIGCHLD_signalHandler);     // -- setup empty signal handler to make 'select()' abort on child process termination.

    WriteLn "Starting CL client.";

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
                WriteLn "Child died:  pid=%_  status=%_", child_pid[child_sz], child_stat[child_sz];
            }

        }else if (n == 0){
            // Heartbeat:

        }else{
            if (FD_ISSET(sock_fd, &fds)){
                // New connection:
                int fd = acceptConnection(sock_fd);
                WriteLn "New server connection: %_", fd;
                pkg.clear();

                if (server_fd != -1){
                    WriteLn "Closing old server fd: %_", server_fd;
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
                            WriteLn "Error: %_", strerror(errno);
                        break;
                    }else if (size == 0){
                        if (tot_size == 0){
                            // First read was empty; assume socket is closed:
                            WriteLn "Server disconnected, closing fd: %_", server_fd;
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

                receivePackage(pkg);        // -- will clear 'pkg' if complete
            }
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
