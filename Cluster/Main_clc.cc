#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Client.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


static
bool isNumber(const String& text)
{
    for (uint i = 0; i < text.size(); i++)
        if (!isDigit(text[i]))
            return false;
    return true;
}


// Returns process ID of 'exec', different from myself, or 0 if not found.
static
pid_t findProcess(cchar* exec)
{
    Vec<FileInfo> files, dirs;
    readDir("/proc", files, dirs);

    pid_t my_pid = getpid();
    for (uint i = 0; i < dirs.size(); i++){
        if (isNumber(dirs[i].name)){
            pid_t pid = stringToUInt64(dirs[i].name);
            if (pid == my_pid) continue;

            File in(dirs[i].full + "/cmdline", "r");
            Str text = readFile(in, true, 256);

            if (strcmp(exec, text.base()) == 0)
                return pid;
        }
    }

    return 0;
}


void myExit()
{
    WriteLn "CL client stopped.";
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.add("port", "uint", "61453", "Client port.");
    cli.add("restart", "bool", "no", "First kill existing client, if any.");
    cli.parseCmdLine(argc, argv);

    if (cli.get("restart").bool_val){
        while (pid_t pid = findProcess(argv[0]))
            kill(pid, SIGTERM);
    }

    atExit(x_Always, myExit);
    clientLoop(cli.get("port").int_val);

    return 0;
}
