//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : ParClient.cc
//| Author(s)   : Niklas Een
//| Module      : Bip
//| Description : Client interface for parallel framework.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ParClient.hh"
#include "ZZ_Netlist.hh"
#include "ZZ/Generics/Sort.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// PAR initilization:


bool par = false;    // -- TRUE if running Bip in PAR mode.


struct ParWriter : ConsoleWriter {
    Vec<uchar> text;
    void putChar(char c) { text.push(c); }
    void flush()         { sendMsg(999996/*=ev_Text*/, text.slice()); text.clear(); }
};
ParWriter par_writer;


void startPar()
{
    redirectConsole(true, par_writer, false/*use_ansi*/, false/*is_console*/);
    par = true;
}


static FILE* log_to = NULL;
static FILE* replay = NULL;

ZZ_Finalizer(log_files, 0) {
    if (log_to) fclose(log_to);
    if (replay) fclose(replay);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Low-level read functions:


static const uint read_Success = 0;
static const uint read_TimeOut = 1;
static const uint read_NoSink  = 2;
static const uint read_Error   = 3;


// Returns one of 'read_XXX' defined above. 'buf' must be of at least size 'bytes_wanted'.
static
uint readFd(int fd, double timeout, uint bytes_wanted, /*outputs:*/ uchar* buf, uint& bytes_read)
{
    assert(timeout >= 0);

    bytes_read = 0;
    if (bytes_wanted == 0)
        return read_Success;

    // Setup timeout:
    struct timeval t;
    t.tv_sec  = uint(timeout);
    t.tv_usec = uint((timeout - uint(timeout)) * 1000000.0);

    // Setup file descriptor set for 'select':
    fd_set fs;
    FD_ZERO(&fs);
    FD_SET(fd, &fs);

    // Do read with timeout:
    int result = select(FD_SETSIZE, &fs, 0, 0, &t);
    if (result == -1)
        return read_Error;
    if (result == 0)
        return read_TimeOut;

    ssize_t size = read(fd, buf, bytes_wanted);
    if (size == 0)
        return read_NoSink;

    // Set 'bytes_read':
    assert(size > 0);
    //**/fprintf(stderr, "##  fd=%d  timeout=%g  bytes_wanted=%u  size=%d\n", fd, timeout, bytes_wanted, (int)size);
    assert(size <= ssize_t(INT_MAX));
    bytes_read = size;

    return read_Success;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Package accumulation:


// From Johan Alfredsson's documentation:
//
// Each package has a header describing its payload. The following constants
// specify the format of the header. The fields (type and size) are string
// representations of numbers. These fields are separated by a space.
// Example (including data):
//
// 000001 0000000000000017 This is a message

static const uint HEADER_TypeBytes = 6;
static const uint HEADER_SizeBytes = 16;
static const uint HEADER_Length = HEADER_TypeBytes + 1 + HEADER_SizeBytes + 1;


uchar      msg_head[HEADER_Length];
Vec<uchar> msg_data;
uint       msg_type;
uint       head_sz;
uint       data_sz;

ZZ_Initializer(msg, 0) { msg_type = 0; head_sz = 0; data_sz = 0; }
ZZ_Finalizer  (msg, 0) { msg_data.clear(true); }


// Accumulate message (may not arrive in one piece). Will return a full message if got one,
// otherwise 'Msg_NULL'.
Msg accMsg(int fd, double timeout)
{
    uint n, st;
    if (head_sz < HEADER_Length){
        st = readFd(fd, timeout, HEADER_Length - head_sz, &msg_head[head_sz], n);
        if (st == read_Success){
            head_sz += n;

            if (head_sz == HEADER_Length){
                // Got header, setup data buffer:
                msg_head[HEADER_TypeBytes] = 0;
                msg_head[HEADER_Length - 1] = 0;
                msg_type = (uint)stringToUInt64((cchar*)&msg_head[0]);
                msg_data.setSize((uind)stringToUInt64((cchar*)&msg_head[HEADER_TypeBytes + 1]));
                //**/fprintf(stderr, "##  Got message header: %u %u\n", msg_type, msg_data.size());
            }else
                assert(head_sz < HEADER_Length);
        }else
            assert(st == read_TimeOut);     // -- "NoSink" or "Error" should not happen
    }

    if (head_sz == HEADER_Length){
        st = readFd(fd, timeout, msg_data.size() - data_sz, &msg_data[data_sz], n);
        if (st == read_Success){
            data_sz += n;

            if (data_sz == msg_data.size()){
                // Got full message, return it:
                Msg ret(msg_type, Pkg(msg_data));
                head_sz = data_sz = 0;
                //**/fprintf(stderr, "##  Got message.\n");
                return ret;
            }else
                assert(data_sz < msg_data.size());
        }else
            assert(st == read_TimeOut);     // -- "NoSink" or "Error" should not happen
    }

    return Msg_NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Public functions:


static const Array<uchar> empty_data(NULL, 0);


Msg receiveMsg(int fd)
{
    if (log_to)
        fputc('R', log_to), fflush(log_to);
    if (replay){
        char c = fgetc(replay);
        if (feof(replay)){
            ShoutLn "REPLAY ERROR! Call to 'receiveMsg()' after end of poll/receive count stream.";
            exit(255); }
        if (c != 'R'){
            ShoutLn "REPLAY ERROR! Engine called 'receiveMsg()' during replay but 'pollMsg()' during recording.";
            exit(255);
        }
    }

    for(;;){
        Msg msg = accMsg(fd, 1.0);          // -- could have used any time-out here
        if (msg)
            return msg;
    }
}


Msg pollMsg(int fd)
{
    if (replay){
        if (replay == (FILE*)1)
            return Msg_NULL;

        char c = fgetc(replay);
        if (feof(replay)){
            WriteLn "  --((replay stream ended))--";
            fclose(replay);
            replay = (FILE*)1;
            return Msg_NULL;

        }else{
            if (c == '!'){
                for(;;){
                    Msg msg = accMsg(fd, 1.0);  // -- could have used any time-out here
                    if (msg)
                        return msg;
                }
            }else if (c == '.'){
                return Msg_NULL;
            }else if (c == 'R'){
                ShoutLn "REPLAY ERROR! Engine called 'pollMsg()' during replay but 'receiveMsg()' during recording.";
                exit(255);
            }else{
                ShoutLn "REPLAY ERROR! Unexpected character in poll/receive count stream: %d", (uchar)c;
                assert(false);
            }
        }

    }else{
        Msg msg = accMsg(fd, 0.0);
        if (log_to)
            fputc(msg ? '!' : '.', log_to), fflush(log_to);
        return msg;
    }

    return Msg();       // (dummy to please Microsoft compiler)
}


void sendMsg(uint type, Array<const uchar> data, int fd)
{
    char buf[HEADER_Length + 1];
    sprintf(buf, "%.*u %.*u ", HEADER_TypeBytes, type, HEADER_SizeBytes, data.size());
    //**/fprintf(stderr, "##  Sending message with header [%s]\n", buf);

    ssize_t n = write(fd, buf, HEADER_Length);
    if (n != (ssize_t)HEADER_Length){
        fprintf(stderr, "ParClient: Pipe closed prematurely?\n"); fflush(stderr);
        fprintf(stderr, "pid: %u\n", (uint)getpid()); fflush(stderr);
        _exit(255);
    }
    assert(n == (ssize_t)HEADER_Length);

    if (data.size() > 0){
        uind bytes_written = 0;
        while (bytes_written < data.size()){
            n = write(fd, &data[bytes_written], data.size() - bytes_written);
            if (n < 0){
                fprintf(stderr, "ParClient: Not all data was sent (%u bytes out of %u)\nPipe closed prematurely?\n", (uint)n, (uint)data.size()); fflush(stderr);
                _exit(255);
            }
            bytes_written += n;
        }
    }
}


void sendMsg(Msg msg, int fd)
{
    if (msg.pkg)
        sendMsg(msg.type, msg.pkg.slice(), fd);
    else
        sendMsg(msg.type, empty_data, fd);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Marshaling -- Basic building blocks:


macro uint64 getu(const uchar*& in, const uchar* end)
    // throw(Excp_EOF)
{
    uint   shift = 0;
    uint64 value = 0;
    for(;;){
        if (in == end) throw Excp_EOF();
        uchar x = *in++;
        value |= uint64(x & 0x7F) << shift;
        if (x < 0x80) return value;
        shift += 7;
    }
}


macro void putu(Vec<uchar>& out, uint64 x)
{
    while (x >= 0x80){
        out.push(uchar(x) | 0x80);
        x >>= 7; }
    out.push(uchar(x));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Netlist marshalling: [internal]


static
void streamOut_Netlist(Vec<uchar>& data, NetlistRef N)
{
    WMap<uint> xlat(UINT_MAX);
    xlat(Wire_NULL) = 0;
    xlat(N.True ()) = 2;
    xlat(N.False()) = 3;

    uint n_const = 2;     // -- 0=NULL input, 1=constant TRUE
    uint n_pis = 0, n_ffs = 0;

    // Translate PIs:
    {
        // Collect:
        Vec<Pair<int,GLit> > nums;
        For_Gatetype(N, gate_PI, w){
            int num = attr_PI(w).number; assert(num >= 0);
            nums.push(tuple(num, w));
            newMax(n_pis, uint(num + 1));
        }

        // Check numbering:
        sort(nums);
        for (uint i = 1; i < nums.size(); i++) assert(nums[i-1].fst != nums[i].fst);

        // Store in map:
        for (uint i = 0; i < nums.size(); i++)
            xlat(N[nums[i].snd]) = (n_const + nums[i].fst) << 1;
    }
    putu(data, n_pis);  // <<== fel om inte kontinuerligt numrerade!!

    // Translate Flops:
    Vec<Pair<int,GLit> > ff_nums;
    {
        // Collect:
        Vec<Pair<int,GLit> >& nums = ff_nums;
        For_Gatetype(N, gate_Flop, w){
            int num = attr_Flop(w).number; assert(num >= 0);
            nums.push(tuple(num, w));
            newMax(n_ffs, uint(num + 1));
        }

        // Check numbering:
        sort(nums);
        for (uint i = 1; i < nums.size(); i++) assert(nums[i-1].fst != nums[i].fst);

        // Store in map:
        for (uint i = 0; i < nums.size(); i++)
            xlat(N[nums[i].snd]) = (n_const + n_pis + nums[i].fst) << 1;
    }
    putu(data, n_ffs);

    // Save ANDs/LUTs:
    Auto_Pob(N, up_order);
    uint next_lit = 2 * (n_const + n_pis + n_ffs);
    uint n_gates = 0;
    For_UpOrder(N, w){
        n_gates += uint(type(w) == gate_And || type(w) == gate_Lut4); }
    putu(data, n_gates);

    For_UpOrder(N, w){
        if (type(w) != gate_Flop){
            For_Inputs(w, v)
                assert(xlat[v] != UINT_MAX);
        }

        if (type(w) == gate_And){
            putu(data, (xlat[w[0]] ^ (int)sign(w[0])) << 1);    // TAG
            putu(data, xlat[w[1]] ^ (int)sign(w[1]));
            xlat(w) = next_lit;
            next_lit += 2;

        }else if (type(w) == gate_Lut4){
            putu(data, ((xlat[w[0]] ^ (int)sign(w[0])) << 1) | 1);    // TAG
            putu(data, xlat[w[1]] ^ (int)sign(w[1]));
            putu(data, xlat[w[2]] ^ (int)sign(w[2]));
            putu(data, xlat[w[3]] ^ (int)sign(w[3]));
            putu(data, attr_Lut4(w).ftb);
            xlat(w) = next_lit;
            next_lit += 2;
        }else{
            if (! (type(w) == gate_PI || type(w) == gate_PO || type(w) == gate_Flop)){
                ShoutLn "Unexpected gate type: %_", GateType_name[type(w)];
                assert(false); }
        }
    }

    // Save flop inputs and init value:
    if (Has_Pob(N, flop_init)){
        Get_Pob(N, flop_init);
        uint j = 0;
        for (uint i = 0; i < n_ffs; i++){
            if (ff_nums[j].fst > (int)i){
                putu(data, 0);
            }else{
                Wire w = N[ff_nums[j].snd];
                assert(flop_init[w].value < 4);
                j++;
                putu(data, ((xlat[w[0]] ^ (int)sign(w[0])) << 2) | flop_init[w].value);
            }
        }
        assert(j == ff_nums.size());
    }else{
        for (uint i = 0; i < n_ffs; i++)
            putu(data, 0);
    }

    // Save properties:
    if (Has_Pob(N, properties)){
        Get_Pob(N, properties);
        putu(data, properties.size());
        for (uint i = 0; i < properties.size(); i++){
            Wire w = properties[i]; assert(type(w) == gate_PO);
            putu(data, xlat[w[0]] ^ (int)sign(w[0]));
        }
    }else
        putu(data, 0);  // -- no properties
}


static
void streamIn_Netlist(const uchar* in, const uchar* end, NetlistRef N)
    // throw(Excp_EOF)
{
    assert(N.empty());

    Vec<Wire> xlat;
    xlat.push(Wire_NULL);
    xlat.push(N.True());

    // Read number of PIs and Flops:
    uind n_pis   = getu(in, end);
    uind n_ffs   = getu(in, end);
    uind n_gates = getu(in, end);

    for (uind i = 0; i < n_pis; i++)
        xlat.push(N.add(PI_(i)));
    uind ff_offset = xlat.size();
    for (uind i = 0; i < n_ffs; i++)
        xlat.push(N.add(Flop_(i)));

    // Read logic gates:
    for (uind i = 0; i < n_gates; i++){
        uint64 d0 = getu(in, end);
        uint64 d1 = getu(in, end);
        Wire w;
        if (!(d0 & 1)){
            // AND:
            d0 >>= 1;
            w = N.add(And_(), xlat[d0>>1] ^ bool(d0 & 1), xlat[d1>>1] ^ bool(d1 & 1));
        }else{
            // LUT:
            uint64 d2 = getu(in, end);
            uint64 d3 = getu(in, end);
            uint   ftb = getu(in, end);
            w = N.add(Lut4_());
            w.set(0, xlat[d0>>1] ^ bool(d0 & 1));
            w.set(1, xlat[d1>>1] ^ bool(d1 & 1));
            w.set(2, xlat[d2>>1] ^ bool(d2 & 1));
            w.set(3, xlat[d3>>1] ^ bool(d3 & 1));
            attr_Lut4(w).ftb = ftb;
        }
        xlat.push(w);
    }

    // Read flop inputs:
    Add_Pob(N, flop_init);
    flop_init.nil = l_Undef;
    for (uind i = 0; i < n_ffs; i++){
        uint64 d = getu(in, end);
        if (d != 0){
            Wire w = xlat[ff_offset + i];
            flop_init(w) = lbool_new(d & 3);    // -- here we are relying on the serialized format to have the same underlying representation of lifted booleans as 'lbool'.
            d >>= 2;
            w.set(0, xlat[d>>1] ^ bool(d & 1));
        }
    }

    // Read properties:
    Add_Pob(N, properties);
    uind n_props = getu(in, end);
    for (uind i = 0; i < n_props; i++){
        uint64 d = getu(in, end);
        properties.push(N.add(PO_(i), xlat[d>>1] ^ bool(d & 1)));
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Marshaling -- Generic composits:


static
void put_vec_uint(Vec<uchar>& data, const Vec<uint>& v)
{
    putu(data, v.size());
    for (uind i = 0; i < v.size(); i++)
        putu(data, v[i]);
}


static
void put_vec_lbool(Vec<uchar>& data, const Vec<lbool>& v)
{
    putu(data, v.size());
    for (uind i = 0; i < v.size(); i++)
        putu(data, v[i].value);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Marshaling -- Specific types:


static
void put_Cex(Vec<uchar>& data, const Cex& cex, NetlistRef N, bool concrete)
{
    Vec<lbool> v;

    putu(data, (uint)concrete);

    // Inputs:
    Vec<GLit> pi;
    For_Gatetype(N, gate_PI, w)
        pi(attr_PI(w).number, glit_NULL) = w;

    putu(data, cex.inputs.size());
    for (uint d = 0; d < cex.inputs.size(); d++){
        v.clear();
        for (uint i = 0; i < pi.size(); i++){
            if (!pi[i])
                v.push(l_Error);
            else
                v.push(cex.inputs[d][N[pi[i]]]);
        }
        put_vec_lbool(data, v);
    }

    // Flops:
    Vec<GLit>& ff = pi;
    ff.clear();
    For_Gatetype(N, gate_Flop, w)
        ff(attr_Flop(w).number, glit_NULL) = w;

    uint lim = cex.flops.size();
    if (concrete && lim > 1)
        lim = 1;
    putu(data, lim);
    for (uint d = 0; d < lim; d++){
        v.clear();
        for (uint i = 0; i < ff.size(); i++){
            if (!ff[i])
                v.push(l_Error);
            else
                v.push(cex.flops[d][N[ff[i]]]);
        }
        put_vec_lbool(data, v);
    }
}


static
void put_Abstr(Vec<uchar>& data, const WZetL& abstr, NetlistRef N)
{
    Vec<uint> v;
    For_Gatetype(N, gate_Flop, w){
        if (abstr.has(w))
            v.push(attr_Flop(w).number);
    }
    put_vec_uint(data, v);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Composite message functions:


// Will NOT add terminating '\0' to 'buf'.
void bsGet_string(In& in, Vec<char>& buf)
{
    skipWS(in);
    uint len = parseUInt(in);
    if (*in != ' ') throw Excp_ParseError((FMT "Expected space in boost protocol, not: %_", *in));
    in++;

    buf.clear();
    for (uint i = 0; i < len; i++)
        buf.push(in++);
}


uint bsGet_uint(In& in)
{
    skipWS(in);
    return parseUInt(in);
}


static
void parseStartPackage(const Pkg& pkg, uint& protocol_ver, String& log_path, String& replay_path, uint& verbosity, bool& capture_stderr)
{
    // Reset (in case of exception, have initialized data):
    protocol_ver = 0;
    log_path.clear();
    replay_path.clear();
    verbosity = 0;
    capture_stderr = false;

#if 0   // -- Debug
    Write "Package:";
    for (uind i = 0; i < pkg.size(); i++)
        Write " %.2X", pkg[i];
    NewLine;
    Write "ASCII: ";
    for (uind i = 0; i < pkg.size(); i++){
        char c = pkg[i];
        if (c >= 32)
            std_out += c;
        else
            Write "\a*[%_]\a*", (uchar)c;
    }
    NewLine;
#endif

    In in((cchar*)pkg.base(), pkg.size());

    /*
    string "serialization::archive"
    uint    version (5, 7 and 9 tested)
    uint    ?? (should be 0)
    uint    ?? (should be 0)

    uint  protocol_version
    string log_path
    string replay_path
    uint   verbosity
    uint   capture_stderr
    */

    // Check header:
    Vec<char> buf;
    bsGet_string(in, buf);
    buf.push(0);
    if (strcmp(buf.base(), "serialization::archive") != 0){
        WriteLn "Invalid start package: boost serialization version not supported (A)\n[%_]", buf; exit(255); }
    uint version = bsGet_uint(in);
    if (version < 3 || version > 9){
        WriteLn "Invalid start package: boost serialization version not supported (B)\n[%_]", version; exit(255); }
    uint unknown1 = bsGet_uint(in);
    if (unknown1 != 0){
        WriteLn "Invalid start package: boost serialization version not supported (C)\n[%_]", unknown1; exit(255); }
    uint unknown2 = bsGet_uint(in);
    if (unknown2 != 0){
        WriteLn "Invalid start package: boost serialization version not supported (D)\n[%_]", unknown2; exit(255); }

    // Get data:
    protocol_ver = bsGet_uint(in);
    bsGet_string(in, log_path);
    bsGet_string(in, replay_path);
    verbosity = bsGet_uint(in);
    capture_stderr = (bool)bsGet_uint(in);

    if (!in.eof())
        throw Excp_ParseError("Invalid start package: too many fields.");
}

// Receive initial task.
void receiveMsg_Task(NetlistRef N, String& params)
{
    Msg s = receiveMsg(); assert(s.type == 999999);     // -- 999999 = Start package

    uint   protocol_ver;
    String log_filename;
    String replay_filename;
    uint   verbosity ___unused;
    bool   capture_stderr ___unused;    // <<== support this!

    parseStartPackage(s.pkg, protocol_ver, log_filename, replay_filename, verbosity, capture_stderr);
    if (protocol_ver != 1)
        throw Excp_ParseError("PAR Protocol version must be 1.");

    if (log_filename != "")
        log_to = fopen(log_filename.c_str(), "wb");
    if (replay_filename != "")
        replay = fopen(replay_filename.c_str(), "rb");

    /**/WriteLn "DEBUG: Protocol version: %_", protocol_ver;
    /**/WriteLn "DEBUG: Log filename    : %_", log_filename;
    /**/WriteLn "DEBUG: Replay filename : %_", replay_filename;
    /**/WriteLn "DEBUG: Verbosity       : %_", verbosity;
    /**/WriteLn "DEBUG: Capture stderr  : %_", capture_stderr;

    Msg n = receiveMsg();
    if (n.type == 999999) n = receiveMsg();     // -- temporary fix for early bug in Johan's replay code
    assert(n.type == 1);      // Type 1 = Netlist
    Msg p = receiveMsg();
    assert(p.type == 2);      // Type 2 = Parameters


    assert(N.empty());
    streamIn_Netlist(&n.pkg[0], &n.pkg.end(), N);

    params.setSize(p.pkg.size());
    for (uind i = 0; i < p.pkg.size(); i++)
        params[i] = p.pkg[i];
}


// Send a string tagged with 'type'.
void sendMsg_Text(uint type, const String& text)
{
    sendMsg(type, slice((uchar&)text[0], (uchar&)text.end()));
}


// Send a cube which have been proved to be unreachble for the first 'k' time frames
// (where 'k' may be 'UINT_MAX'). NOTE! Literals of 's' are expressed in gate IDs!
// (that's why 'N' is needed; to convert them into 'number's)
void sendMsg_UnreachCube(NetlistRef N, TCube s)
{
    Vec<uchar> data;
    putu(data, s.frame);
    putu(data, s.cube.size());
    for (uint i = 0; i < s.cube.size(); i++){
        Wire w = N[s.cube[i]];           assert(type(w) == gate_Flop);
        int  num = attr_Flop(w).number;  assert(num >= 0);
        putu(data, (num << 1) | (uint)sign(w));
    }

    sendMsg(104/*UCube*/, data.slice());
}


// Same as above. NOTE! 's' here is expressed in 'number's.
void sendMsg_UnreachCube(const Vec<GLit>& s, uint frame)
{
    Vec<uchar> data;
    putu(data, frame);
    putu(data, s.size());
    for (uint i = 0; i < s.size(); i++)
        putu(data, (s[i].id << 1) | (uint)s[i].sign);

    sendMsg(104/*UCube*/, data.slice());
}


void unpack_UCube(Pkg pkg, /*outputs:*/ uint& frame, Vec<GLit>& state)
{
    const uchar* in  = &pkg[0];
    const uchar* end = &pkg.end();

    frame = getu(in, end);
    state.setSize(getu(in, end));
    for (uint i = 0; i < state.size(); i++){
        uint v = getu(in, end);
        state[i] = GLit(v >> 1, v & 1);
    }
}


void sendMsg_Result_unknown(const Vec<uint>& props)
{
    Vec<uchar> data;
    data.push(l_Undef.value);
    put_vec_uint(data, props);
    sendMsg(101/*Result*/, data.slice());
}


// 'concrete' means flops are not abstracted => only the initial state will be included in the counterexample
void sendMsg_Result_fails(const Vec<uint>& props, const Vec<uint>& depths, const Cex& cex, NetlistRef N, bool concrete_cex)
{
    Vec<uchar> data;
    data.push(l_False.value);
    put_vec_uint(data, props);
    put_vec_uint(data, depths);
    put_Cex(data, cex, N, concrete_cex);
    sendMsg(101/*Result*/, data.slice());
}


void sendMsg_Result_holds(const Vec<uint>& props, NetlistRef N_invar)
{
    Vec<uchar> data;
    data.push(l_True.value);
    put_vec_uint(data, props);
    putu(data, 0);  // -- boolean; 0 = no invariant follows
    sendMsg(101/*Result*/, data.slice());
}


void sendMsg_Abstr(const WZetL& abstr, NetlistRef N)
{
    Vec<uchar> data;
    put_Abstr(data, abstr, N);
    sendMsg(103/*Abstr*/, data.slice());
}


void sendMsg_AbstrBad()
{
    sendMsg(105/*AbstrBad*/, empty_data);
}


void sendMsg_Reparam(NetlistRef N, NetlistRef N_recons)
{
    Vec<uchar> data;
    streamOut_Netlist(data, N);
    streamOut_Netlist(data, N_recons);
    sendMsg(108/*Reparam*/, data.slice());
}


void sendMsg_Abort(String text)
{
    sendMsg(5/*Abort*/, slice((uchar&)text[0], (uchar&)text.end()));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}


using namespace ZZ;
void testMarshal(NetlistRef N)
{
    Netlist M;
    Vec<uchar> data;
    streamOut_Netlist(data, N);
    streamIn_Netlist(&data[0], &data.end(), M);

    nameByCurrentId(N);
    nameByCurrentId(M);
    N.write("N.gig");
    M.write("M.gig");

    //void streamOut_Netlist(Vec<uchar>& data, NetlistRef N)
    //void streamIn_Netlist(const uchar* in, const uchar* end, NetlistRef N)
}
