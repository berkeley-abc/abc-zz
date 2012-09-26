#include "Prelude.hh"
#include "ZZ/MiniSat/SatTypes.hh"

//#define REQUIRE_SMALLER_OUTPUT

using namespace ZZ;


static
void parseLitList(In& in, Vec<Lit>& result)
{
    result.clear();

    skipWS(in);
    if (*in != '{') return;

    expect(in, "{ ");
    if (*in == '}'){ in++; return; }

    for(;;){
        bool sign;
        if (*in == '~'){ sign = true; in++; }
        else           { sign = false; }
        expect(in, " x");
        uint var = parseUInt(in);
        result.push(Lit(var, sign));
        skipWS(in);
        if (*in == '}'){ in++; break; }
        expect(in, "; ");
    }
}


bool run(Vec<Vec<char> >& lines, cchar* cmd, cchar* succ, uind& output_size)
{
    // Create CNF file:
    OutFile out("__shrink_tmp.mst");
    for (uind i = 0; i < lines.size(); i++)
        out %= "%_\n", lines[i];
    out.close();

    // Run command:
    String full_cmd = "ulimit -c 0; ";
    full_cmd += (FMT cmd, "__shrink_tmp.mst");
    full_cmd += " 2> __shrink_tmp.out > __shrink_tmp.out";

    int ignore ___unused = system(full_cmd.c_str());

    // Check input:
    Array<char> text = readFile("__shrink_tmp.out", true);
    output_size = text.size();
    return (strstr(text.base(), succ) != NULL);
}


// For now, the only shrink option we do is remove a random line.
void shrink(Vec<Vec<char> >& lines, uint64& seed)
{
    bool did_change = false;
    if (irand(seed, 5) == 0){
        // Shift variable downwards to unused index:
        Vec<char> used;
        Vec<Lit>  ps;

        for (uind i = 0; i < lines.size(); i++){
            Vec<char>& buf = lines[i];

            if (hasPrefix(buf, "clear(") || hasPrefix(buf, "simplifyDB(") || hasPrefix(buf, "addVar(") || hasPrefix(buf, "addVars("))
                continue;

            uind pos = find(buf, '(') + 1;
            In in2(&buf[pos], buf.size()-pos);
            parseLitList(in2, ps);
            expect(in2, " ) ");
            expectEof(in2);

            for (uind i = 0; i < ps.size(); i++)
                used(var(ps[i]), false) = true;
        }

        if (used.size() < 2) goto SkipShift;
        uind r = irand(seed, used.size() - 1) + 1;
        while (!used[r]){
            if (r == 1) goto SkipShift;
            r--;
        }
        if (r == 1) goto SkipShift;
        uind q = irand(seed, r - 1) + 1;
        while (used[q]){
            if (q == 1) goto SkipShift;
            q--;
        }

        // Move variable 'r' to 'q':
        //**/WriteLn "Changing x%_ to x%_", r, q;
        String tmp;
        for (uind i = 0; i < lines.size(); i++){
            Vec<char>& buf = lines[i];

            if (hasPrefix(buf, "clear(") || hasPrefix(buf, "simplifyDB(") || hasPrefix(buf, "addVar(") || hasPrefix(buf, "addVars("))
                continue;

            uind pos = find(buf, '(') + 1;
            In in2(&buf[pos], buf.size()-pos);
            parseLitList(in2, ps);
            expect(in2, " ) ");
            expectEof(in2);

            for (uind i = 0; i < ps.size(); i++){
                Var  x = var(ps[i]);
                bool s = sign(ps[i]);
                if (x == r){
                    ps[i] = Lit(q, s);
                    did_change = true;
                }
            }

            //**/WriteLn "Before: %_", buf;
            buf.shrinkTo(pos);
            tmp.clear();
            tmp %= "%_)", ps;
            append(buf, tmp);
            //**/WriteLn "After : %_", buf;
        }
    }
    if (did_change)
        return;
  SkipShift:;

    // Shrink by removing part of line or entire line:
    uind i = irand(seed, lines.size());
    Vec<char>& buf = lines[i];
    Out        out;
    Vec<Lit>   ps;

    if (hasPrefix(buf, "clear(")){
        goto RemoveLine;

    }else if (hasPrefix(buf, "simplifyDB(")){
        goto RemoveLine;

    }else if (hasPrefix(buf, "addVar(")){
        goto RemoveLine;

    }else if (hasPrefix(buf, "addVars(")){
        In in2(&buf[8], buf.size()-8);
        skipWS(in2);
        uint n = parseUInt(in2);
        expect(in2, " ) ");
        expectEof(in2);

        if (irand(seed,10) == 0 || n <= 1) goto RemoveLine;
        out %= "addVars(%_)", (irand(seed,2) == 0) ? n - 1 : (n+1) / 2;

    }else if (hasPrefix(buf, "addClause(")){
        In in2(&buf[10], buf.size()-10);
        parseLitList(in2, ps);
        expect(in2, " ) ");
        expectEof(in2);

        if (ps.size() == 0) goto RemoveLine;
        if (irand(seed, 4) == 0) goto RemoveLine;
        uint i = irand(seed, ps.size());
        ps[i] = ps.last();
        ps.pop();
        out %= "addClause(%_)", ps;

    }else if (hasPrefix(buf, "removeVars(")){
        In in2(&buf[11], buf.size()-11);
        parseLitList(in2, ps);
        expect(in2, " ) ");
        expectEof(in2);

        if (ps.size() == 0) goto RemoveLine;
        uint i = irand(seed, ps.size());
        ps[i] = ps.last();
        ps.pop();
        out %= "removeVars(%_)", ps;

    }else if (hasPrefix(buf, "solve(")){
        In in2(&buf[6], buf.size()-6);
        parseLitList(in2, ps);
        expect(in2, " ) ");
        expectEof(in2);

        if (ps.size() == 0) goto RemoveLine;
        uint i = irand(seed, ps.size());
        ps[i] = ps.last();
        ps.pop();
        out %= "solve(%_)", ps;

    }else
        Throw(Excp_ParseError) "Unknown API call: %_", buf;

    buf.clear();
    out.vec().copyTo(buf);
    return;

    // Just remove line:
  RemoveLine:;
    for (; i < lines.size()-1; i++)
        lines[i+1].moveTo(lines[i]);
    lines.pop();
}


int main(int argc, char** argv)
{
    ZZ_Init;

    // Parse input:
    if (argc != 4){
        WriteLn "USAGE: \a*shrink_mst\a* <mst-file> <command with %%_> <success-string>";
        exit(0); }

    InFile in(argv[1]);
    if (in.null()){
        ShoutLn "ERROR! Could not open %_", argv[1];
        exit(1); }
    Vec<Vec<char> > lines;
    while (!in.eof()){
        lines.push();
        readLine(in, lines.last());
        uind k = search(lines.last(), '#');
        if (k != UIND_MAX)
            lines.last().shrinkTo(k);
        trim(lines.last());
        if (lines.last().size() == 0)
            lines.pop();
    }

    // Valid start point?
    uind out_sz;
    if (!run(lines, argv[2], argv[3], out_sz)){
        ShoutLn "ERROR! Original file did not meet success string.";
        exit(1); }

    // Give user a way to stop shrinking:
    WriteLn "\n    \a*kill -1 %d\a*\n", getpid();
    OutFile out("kill_shrink.sh");
    out %= "kill -1 %d\n", getpid();
    out.close();
    int ignore ___unused = system("chmod +x kill_shrink.sh");

    // Shrink:
    uint   n_shrinks = 0;
    uint64 seed = generateSeed();
    uind   last_sz = UIND_MAX;
    Vec<Vec<char> > lines2;
    for(;;){
        if (lines.size() == 0) break;

        lines2.setSize(lines.size());
        for (uind i = 0; i < lines.size(); i++)
            lines[i].copyTo(lines2[i]);

            shrink(lines2, seed);
        if (run(lines2, argv[2], argv[3], out_sz)){
#if defined(REQUIRE_SMALLER_OUTPUT)
            if (out_sz <= last_sz)
#endif
            {
                lines2.moveTo(lines);
                n_shrinks++;
                WriteLn "#shrinks: %_   #lines: %_   output chars: %_", n_shrinks, lines.size(), out_sz;
                last_sz = out_sz;

                if (fileSize("best.mst") != UINT64_MAX){
                    int ignore2 ___unused = system("mv -f best.mst best1.mst"); }
                OutFile out("best.mst");
                for (uind i = 0; i < lines.size(); i++)
                    out += lines[i], '\n';
            }
        }
    }

    return 0;
}
