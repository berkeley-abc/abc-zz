#include "Prelude.hh"
#include "ZZ_CmdLine.hh"
#include "Cluster.hh"
#include "Client.hh"
#include "ZZ_Unix.hh"
#include "TopMonitor.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm



/*
top - 14:58:12 up 119 days,  3:27,  7 users,  load average: 0.00, 0.01, 0.05
Tasks: 195 total,   1 running, 194 sleeping,   0 stopped,   0 zombie
Cpu(s):  0.4%us,  0.0%sy,  0.0%ni, 99.5%id,  0.1%wa,  0.0%hi,  0.0%si,  0.0%st
Mem:  24688248k total, 24453584k used,   234664k free,  2315980k buffers
Swap:  6385800k total,    17336k used,  6368464k free, 16999508k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND                                                                                                                                 
18424 een       20   0 17328 1340  944 R    2  0.0   0:00.01 top
    1 root      20   0 24592 2360 1296 S    0  0.0   0:43.41 init
    2 root      20   0     0    0    0 S    0  0.0   0:01.16 kthreadd
    3 root      20   0     0    0    0 S    0  0.0   1:26.29 ksoftirqd/0
    6 root      RT   0     0    0    0 S    0  0.0   0:00.00 migration/0
    7 root      RT   0     0    0    0 S    0  0.0   0:38.17 watchdog/0
    8 root      RT   0     0    0    0 S    0  0.0   0:00.00 migration/1
   10 root      20   0     0    0    0 S    0  0.0   0:54.15 ksoftirqd/1
   12 root      RT   0     0    0    0 S    0  0.0   0:22.08 watchdog/1
   13 root      RT   0     0    0    0 S    0  0.0   0:00.00 migration/2
   15 root      20   0     0    0    0 S    0  0.0   0:17.89 ksoftirqd/2
   16 root      RT   0     0    0    0 S    0  0.0   0:21.83 watchdog/2
   17 root      RT   0     0    0    0 S    0  0.0   0:00.00 migration/3
   19 root      20   0     0    0    0 S    0  0.0   0:19.37 ksoftirqd/3
   20 root      RT   0     0    0    0 S    0  0.0   0:20.70 watchdog/3
   21 root      RT   0     0    0    0 S    0  0.0   0:00.00 migration/4
   23 root      20   0     0    0    0 S    0  0.0   0:19.06 ksoftirqd/4

   
CPU > 100% för flertrådade program:

Swap:  9799608k total,        0k used,  9799608k free,  2745560k cached

  PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND                                                                                    
15294 een       20   0  769m 661m 1024 R  313  8.2   0:26.89 apart_mt                                                                                    
15291 root      30  10 84388  67m 4764 R  100  0.8   0:18.64 update-apt-xapi                                                                             
25139 een       20   0  618m 287m  32m S    2  3.6 369:38.55 firefox-bin                                                                                 
   83 root      20   0     0    0    0 S    0  0.0   0:16.07 kswapd0                                                                                     
 1484 root      20   0  133m 119m  11m S    0  1.5 134:00.86 Xorg                                                                                        
 7339 een       20   0 23880 3304 1992 S    0  0.0   6:22.52 synergys                                                                                    
15293 een       20   0  2668 1324  916 R    0  0.0   0:00.06 top                                                                                         
18041 een       20   0  2668 1364  916 S    0  0.0  35:58.21 top                                                                                         
27404 een       20   0 11248 6760 2384 S    0  0.1   0:28.86 xterm                                                                                       
    1 root      20   0  2792 1720 1232 S    0  0.0   0:05.06 init                                                                                        
    2 root      20   0     0    0    0 S    0  0.0   0:00.01 kthreadd                                                                                    
    3 root      RT   0     0    0    0 S    0  0.0   0:00.02 migration/0                                                                                 
    4 root      20   0     0    0    0 S    0  0.0   0:02.83 ksoftirqd/0                                                                                 
    5 root      RT   0     0    0    0 S    0  0.0   0:00.00 watchdog/0                                                                                  
   27 root      20   0     0    0    0 S    0  0.0   0:12.51 events/0                                                                                    
   35 root      20   0     0    0    0 S    0  0.0   0:00.00 cpuset                                                                                      
   36 root      20   0     0    0    0 S    0  0.0   0:00.00 khelper                                                                                     
   37 root      20   0     0    0    0 S    0  0.0   0:00.00 async/mgr                                                                                   
   38 root      20   0     0    0    0 S    0  0.0   0:00.00 pm                                                                                          
   40 root      20   0     0    0    0 S    0  0.0   0:00.20 sync_supers           

   
Kan vara värt att kolla PPID/PID struktur för multi-process program:
Total CPU tid, minneskonsumption etc. kan beräknas genom att lägga till barnprocesser
   
 PID USER      PR  NI  VIRT  RES  SHR S %CPU %MEM    TIME+  COMMAND                                                                                    
15430 een       20   0 29308  15m 2300 R   77  0.2   0:02.33 bip                                                                                         
15433 een       20   0 24484  12m 2600 R   73  0.2   0:02.21 abc                                                                                         
15428 een       20   0 53100  11m 2264 S    6  0.1   0:00.18 par                                                                                         
15427 root      30  10  2016  948  564 D    5  0.0   0:00.27 updatedb.mlocat                                                                             
 1484 root      20   0  133m 119m  11m S    1  1.5 134:00.99 Xorg                                                                                        
15293 een       20   0  2668 1324  916 R    1  0.0   0:00.14 top                                                                                         
18041 een  

Fält som bör läggas till: 
  b  (PPID)
  s  (data + stack size)
  u  (page fault count)
  v  (dirty pages count)
  j  (cpu used)
  l = CPU time (not counting time in kernel or what?)
  j = last used cpu  


  PID USER      PR  NI  VIRT  SHR S %CPU %MEM    TIME+   PPID P   TIME DATA nFLT nDRT COMMAND                                                           
25139 een       20   0  617m  32m S    1  3.6 370:07.68     1 0 370:07 445m  439    0 firefox-bin                                                        
  388 root      20   0     0    0 S    0  0.0   0:15.42     2 2   0:15    0    0    0 flush-8:0                                                          
15293 een       20   0  2668  920 R    0  0.0   0:00.56 27405 7   0:00  600    0    0 top                                                                
18041 een       20   0  2668  916 S    0  0.0  36:00.02 17801 2  36:00  600    0    0 top                                                                
    1 root      20   0  2792 1232 S    0  0.0   0:05.07     0 0   0:05  516   22    0 init                                                               
    2 root      20   0     0    0 S    0  0.0   0:00.01     0 1   0:00    0    0    0 kthreadd                                                           
    3 root      RT   0     0    0 S    0  0.0   0:00.02     2 0   0:00    0    0    0 migration/0                                                        
   
Kopiera 'top' till 'cluster-top' och skapa ~/.cluster-toprc

*/


//=================================================================================================
// Connect to existing socket:


void send(const Job& job)
{
    int fd = connectToSocket((char*)"localhost", 0xF00D);
    if (fd < 0){
        WriteLn "Could not connect to client.";
        exit(1);
    }

    cl_launch(fd, job);
    /**/cl_launch(fd, job);
}


int main(int argc, char** argv)
{
    ZZ_Init;

    cli.addCommand("job", "Connect to client and send a job.");
    cli.addCommand("client", "Start a client process.");
    cli.parseCmdLine(argc, argv);

    if (cli.cmd == "client"){
        clientLoop(0xF00D);

    }else if (cli.cmd == "job"){
        Job job;
        job.readConf("tmp.conf");
        job.prettyPrint(std_out);

        job.prettyPrint(std_out);

        send(job);
    }

#if 0
    TopMonitor top(0.5);
#endif

#if 0
    Vec<String> args;
    Vec<String> env;
    String cmd = "/usr/bin/top";
    args += "-ibc", "-d0.5";
    env  += "COLUMNS=512";

    int child_pid, child_io[3];

    WriteLn "ret: %_", startProcess(cmd, args, child_pid, child_io, env);

    char buf[4096];
    for(;;){
        ssize_t n = read(child_io[1], buf, sizeof(buf));
        if (n == 0) break;
        ssize_t m ___unused = write(STDOUT_FILENO, buf, n);
    }
    return 0;
#endif

#if 0
    char** env = environ;
    while (*env){
        WriteLn "%_", *env;
        env++;
    }
    exit(0);
#endif

#if 0
    Job job;
    job.id = 42;
    job.exec = "bip";
    job.args.push(",pdr");
    job.env.push("PATH=/home/een/ZZ/bin/calc");
    job.batch = "pdr test";
    job.group = "pdr test";
    job.concurrency = 16;
    job.timeout_real = no_timeout;
    job.timeout_cpu = 600;
    job.memout = 8 * (1024.0f * 1024.0f * 1024.0f);

    job.stdout_file = "result.out";
    job.stderr_file = "result.err";

    Dump(job);

    Out data;
    put(data, job);

    In in(data.vec());
    Job job2;
    get(in, job2);

    Dump(job2);
    exit(0);

    clientLoop(0xF00D);
#endif

    return 0;
}


// c_run -cmd="bip ,pdr2 -pob=coi" -suite=ibm -timeout=600 -descr="Pdr2: Evaluating pseudo-justification"

/*
LAUNCH DATA:

executable, arguments, environment, cwd
timeout (cpu/real), mem-out
stdin/stdout/stderr redirection
status file, top-monitor file

group name
concurrency
*/

/*
BATCH:

binaries
source code
description
[monitor data template]
*/


/*
STATUS FILE:
cpu time
real time
mem used (reliable?)
exit code
signal thrown
start/stop date
which computer was used?
*/


/*
TIMELIMIT (seconds) : 3600
MEMLIMIT (mb) : 6000
ARGS : ['/work/een/bin/abc_pdr', '/work/een/benchmarks_2/suites/hwmcc11/single/6s9.aig']
OUTPUT : /work/een/benchmarks_2/output/hwmcc11/abc_2012.06.29-19.51.05.031369
PREFIX : 6s9
BENCHMARK_FILE : /work/een/benchmarks_2/suites/hwmcc11/single/6s9.aig
START TIME : Fri Jun 29 19:51:12 2012
END:  Fri Jun 29 20:51:12 2012
TOTAL (in seconds): 3600.23
*/




/*
start
restart-clients
pause
resume
top <drone>

default prio is 100; 0 means on hold (don't execute)

.cl_jobs/group/batch/prio/123.job

*/
