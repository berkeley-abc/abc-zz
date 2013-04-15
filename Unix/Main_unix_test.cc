#include "Prelude.hh"
#include "Unix.hh"
#include "Processes.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    ProcMode mode;
//    mode.username = "een";
//    mode.dir = "/home/een/ZZ/Bip";
    mode.memout = 1024ull * 1024 * 1024 * 8;
    mode.stdout_file = "gurkan.txt";
    mode.stderr_file = "gurkan.txt";

//    String exec = "/home/een/ZZ/bin/turbantad/bip";
    String exec = "/bin/ls";
    Vec<String> args;
//    Vec<String> args; args += "n12.aig";

    pid_t pid;
    int io[3];
#if 0
    char ret = startProcess(exec, args, pid, io, mode);

    if (ret == 0)
        WriteLn "Successful.";
    else{
        WriteLn "Error code: %_", ret;
        return 1;
    }

    // Dump output from process:
    if (io[1] != -1){
        WriteLn "---STDOUT---";
        char buf[4096];
        for(;;){
            ssize_t n = read(io[1], buf, sizeof(buf));
            if (n == 0) break;
            ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
        }
    }

    if (io[2] != -1){
        WriteLn "---STDERR---";
        char buf[4096];
        for(;;){
            ssize_t n = read(io[2], buf, sizeof(buf));
            if (n == 0) break;
            ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
        }
    }

#else
    for(;;){
        char ret = startProcess(exec, args, pid, io, mode);
        if (ret != 0){
            WriteLn "Error code: %_", ret;
            return 1;
        }else{
            int ret_pid = waitpid(pid, NULL, 0); assert(ret_pid == pid);
            closeChildIo(io);
            putchar('.'), fflush(stdout);
        }
    }
#endif

    return 0;
}
