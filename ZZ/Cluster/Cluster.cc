//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cluster.cc
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
#include "Cluster.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Marshalling:


static
void gets(In& in, String& ret)
{
    ret.clear();
    while (*in != 0)
        ret.push(in++);
    in++;
}

macro void getu(In& in, uint& x) {
    x = getu(in); }

macro void getu(In& in, uint64& x) {
    x = getu(in); }

macro void getF(In& in, float& x) {
    x = getF(in); }

macro void getSz(In& in, Vec<String>& xs) {
    xs.reset(getu(in)); }


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


void Job::serialize(Out& out) const
{
    putu(out, id);
    putu(out, prio);
    putu(out, conc);
    putz(out, batch);

    putz(out, exec);
    putu(out, args.size()); for (uint i = 0; i < args.size(); i++) putz(out, args[i]);
    putz(out, dir);
    putu(out, env .size()); for (uint i = 0; i < env .size(); i++) putz(out, env [i]);

    putF(out, real);
    putF(out, cpu);
    putu(out, mem);

    putz(out, stdin);
    putz(out, stdout);
    putz(out, stderr);
    putz(out, status);
    putz(out, topmon);

    putF(out, ztrig);
    putF(out, zpause);
    putF(out, zkill);
    putF(out, zrest);
    putF(out, zclear);
}


void Job::deserialize(In& in)
{
    getu(in, id);
    getu(in, prio);
    getu(in, conc);
    gets(in, batch);

    gets (in, exec);
    getSz(in, args); for (uint i = 0; i < args.size(); i++) gets(in, args[i]);
    gets (in, dir);
    getSz(in, env); for (uint i = 0; i < env .size(); i++) gets(in, env [i]);


    getF(in, real);
    getF(in, cpu);
    getu(in, mem);

    gets(in, stdin);
    gets(in, stdout);
    gets(in, stderr);
    gets(in, status);
    gets(in, topmon);

    getF(in, ztrig);
    getF(in, zpause);
    getF(in, zkill);
    getF(in, zrest);
    getF(in, zclear);
}


// <<== should be parsable by 'readConf()'.
void Job::prettyPrint(Out& out) const
{
    if (id != job_NULL)
        FWriteLn(out) "id     = %_", id;
    FWriteLn(out) "prio   = %_", prio;
    FWriteLn(out) "exec   = %_", exec;
    FWriteLn(out) "dir    = %_", dir;
    FWriteLn(out) "env    = %_", env;
    FWriteLn(out) "args   = %_", args;
    FWriteLn(out) "batch  = %_", batch;
    FWriteLn(out) "conc   = %_", conc;
    FWriteLn(out) "real   = %_", (real == no_timeout) ? String("-") : ((FMT "%t", real));
    FWriteLn(out) "cpu    = %_", (cpu  == no_timeout) ? String("-") : ((FMT "%t", cpu));
    FWriteLn(out) "mem    = %_", (mem  == no_memout) ? String("-") : ((FMT "%^DB", mem));
    FWriteLn(out) "stdin  = %_", stdin;
    FWriteLn(out) "stdout = %_", stdout;
    FWriteLn(out) "stderr = %_", stderr;
    FWriteLn(out) "status = %_", status;
    FWriteLn(out) "topmon = %_", topmon;
    FWriteLn(out) "ztrig  = %t", ztrig;
    FWriteLn(out) "zpause = %t", zpause;
    FWriteLn(out) "zkill  = %t", zkill;
    FWriteLn(out) "zrest  = %t", zrest;
    FWriteLn(out) "zclear = %t", zclear;
}


void Job::readConf(String filename)
{
    InFile in(filename); assert(in);

    Vec<char> buf;
    while (!in.eof()){
        // Read line:
        readLine(in, buf);
        uind p = search(buf, '#');
        if (p != UIND_MAX)
            buf.shrinkTo(p);
        trim(buf);
        if (buf.size() == 0) continue;

        // Split into key/value:
        p = search(buf, '=');
        if (p == UIND_MAX){
            WriteLn "Invalid line in '\a/%_\a/' (should be on form 'key=value'):", filename;
            WriteLn ">> \a/%_\a/", buf;
        }

        Str val = strip(buf.slice(p+1));
        bool append = (p > 0 && buf[p-1] == '+');
        if (append) p--;
        Str key = strip(buf.slice(0, p));

        try{
            if (eq(key, "prio")){
                prio = stringToUInt64(val);
            }else if (eq(key, "conc")){
                conc = stringToUInt64(val);
            }else if (eq(key, "batch")){
                batch = val;
            }else if (eq(key, "exec")){
                exec = val;
            }else if (eq(key, "args")){
                if (!append) args.clear();
                args += val;    // <<== break on space and interpret backslash
            }else if (eq(key, "dir")){
                dir = val;
            }else if (eq(key, "env")){
                if (!append) env.clear();
                env += val;     // <<== break on space and interpret backslash 
            }else if (eq(key, "real")){
                real = stringToDouble(val);
                // <<== mn/min/h/s/day/days etc... + '-' for no timeout
            }else if (eq(key, "cpu")){
                cpu = stringToDouble(val);
                // <<== mn/min/h/s/day/days etc... + '-' for no timeout
            }else if (eq(key, "mem")){
                mem = stringToUInt64(val);
                // <<== + suffixes MB, GB, kB + '-' for no memout
            }else if (eq(key, "stdin")){
                stdin = val;
            }else if (eq(key, "stdout")){
                stdout = val;
            }else if (eq(key, "stderr")){
                stderr = val;
            }else if (eq(key, "status")){
                status = val;
            }else if (eq(key, "topmon")){
                topmon = val;
            }else if (eq(key, "ztrig")){
                ztrig = stringToDouble(val);
            }else if (eq(key, "zpause")){
                zpause = stringToDouble(val);
            }else if (eq(key, "zkill")){
                zkill = stringToDouble(val);
            }else if (eq(key, "zrest")){
                zrest = stringToDouble(val);
            }else if (eq(key, "zclear")){
                zclear = stringToDouble(val);
            }else{
                WriteLn "Invalid key '\a/%_\a/' in '\a/%_\a/'.", key, filename;
            }

        }catch (...){
            WriteLn "Invalid value in '\a/%_\a/' for key '\a/%_\a/':", filename, key;
            WriteLn ">> \a/%_\a/", val;
        }
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
