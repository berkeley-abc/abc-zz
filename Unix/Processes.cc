//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Processes.cc
//| Author(s)   : Niklas Een
//| Module      : Unix
//| Description : Wrappers for process handling.
//| 
//| (C) Copyright 2013, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
void setCloseOnExec(int fd)
{
    int old_flags = fcntl(fd, F_GETFD, 0);
    if (old_flags != -1)
        fcntl(fd, F_SETFD, old_flags | FD_CLOEXEC);
}


// Close all file descriptors except 'fd' and: STDIN, STDOUT, STDERR
void closeAllBut(int fd)
{
    int n = sysconf(_SC_OPEN_MAX);
    for (int i = 3; i < n; i++)
        if (i != fd)
            close(i);
}


void closeChildIo(int out_std[3])
{
    close(out_std[0]);
    close(out_std[1]);
    close(out_std[2]);
}


// If 'cmd' starts with a '*' then 'execvp' is used (i.e. search the PATH). Output argument
// 'out_pid' will be set to the process ID of the child. 'out_std' will contain pipes connected to
// the child processes' stdin, stdout and stderr (in that order). NOTE! All three file descriptors
// need to be closed by parent.
//
bool startProcess(const String& cmd, const Vec<String>& args, int& out_pid, int out_std[3])
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];
    int signal_pipe[2];

    int res;
    res = pipe(stdin_pipe ); assert(res == 0);
    res = pipe(stdout_pipe); assert(res == 0);
    res = pipe(stderr_pipe); assert(res == 0);
    res = pipe(signal_pipe); assert(res == 0);

    setCloseOnExec(signal_pipe[0]);
    setCloseOnExec(signal_pipe[1]);

    pid_t child_pid = fork(); assert(child_pid != -1);

    if (child_pid == 0){
        // Child:
        dup2(stdin_pipe [0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        closeAllBut(signal_pipe[1]);

        bool use_path = (cmd.size() > 0 && cmd[0] == '*');
        char* exec_cmd = cmd.c_str() + use_path;
        char** exec_args = xmalloc<char*>(args.size() + 2);
        exec_args[0] = exec_cmd;
        for (uint i = 0; i < args.size(); i++)
            exec_args[i+1] = args[i].c_str();
        exec_args[args.size() + 1] = NULL;

        if (use_path)
            res = execvp(exec_cmd, exec_args);
        else
            res = execv(exec_cmd, exec_args);

        // Exec failed, let parent know:
        assert(res == -1);
        res = write(signal_pipe[1], "!", 1);
        assert(res != -1);
        _exit(255);
    }

    // Parent continues:
    close(stdin_pipe [0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    close(signal_pipe[1]);

    out_pid = child_pid;
    out_std[0] = stdin_pipe[1];
    out_std[1] = stdout_pipe[0];
    out_std[2] = stderr_pipe[0];

    // Did child execute 'exec' successfully?
    char buf[1];
    bool success = (read(signal_pipe[0], buf, sizeof(buf)) == 0);
    close(signal_pipe[0]);

    return success;
}


// Each element in 'env' should be on form "key=value" or just "key" (an entry "key" will unset that key).
//
bool startProcess(const String& cmd, const Vec<String>& args, int& out_pid, int out_std[3], const Vec<String>& env)
{
    Vec<String> clear;
    Vec<Pair<String,String> > restore;

    for (uint i = 0; i < env.size(); i++){
        char* key = const_cast<String&>(env[i]).c_str();
        char* p = strchr(key, '=');
        if (p == NULL){
            char* val = getenv(key);
            if (val != NULL){
                restore.push(tuple(String(key), String(val)));
                unsetenv(key);
            }
        }else{
            *p = '\0';
            char* val = getenv(key);
            if (val != NULL)
                restore.push(tuple(String(key), String(val)));
            else
                clear.push(String(key));

            setenv(key, p+1, 1);
            *p = '=';
        }
    }

    bool ret = startProcess(cmd, args, out_pid, out_std);

    for (uint i = 0; i < clear.size(); i++){
        unsetenv(clear[i].c_str()); }
    for (uint i = 0; i < restore.size(); i++){
        setenv(restore[i].fst.c_str(), restore[i].snd.c_str(), 1); }

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
