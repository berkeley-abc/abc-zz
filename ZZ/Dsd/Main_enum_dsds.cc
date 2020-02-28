#include "Prelude.hh"

using namespace ZZ;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Enumeration:


// Negative numbers corresponds to a set of direct PIs
void enumBox(uint n, uint hi, Vec<int>& acc, Vec<Vec<int> >& result)
{
    if (n == 0){
        result.push();
        acc.copyTo(result[LAST]);

    }else{
        for (uint i = 1; i <= hi && i <= n; i++){
            if (i == 1){
                acc.push(-n);
                result.push();
                acc.copyTo(result[LAST]);
                acc.pop();
            }else{
                acc.push(i);
                enumBox(n - i, i, acc, result);
                acc.pop();
            }
        }
    }
}
void enumBox(uint n, Vec<Vec<int> >& result)
{
    Vec<int> acc;
    enumBox(n, n, acc, result);
}


/*
Result from enumBox(6):
  -6 
  2 -4 
  2 2 -2 
  2 2 2 
  3 -3 
  3 2 -1 
  3 3 
  4 -2 
  4 2 
  5 -1 
  6 
*/


// In result: 0 = '[', 1 = ']'
void enumAll(uint n, Vec<Vec<int> >& result)
{
    Vec<Vec<int> > pat, tmp;
    enumBox(n, pat);

    assert(pat[LAST].size() == 1);
    pat.pop();

    for (uint i = 0; i < pat.size(); i++){
        if (pat[i][0] < 2) continue;

        enumAll(pat[i][0], tmp);
        for (uint j = 0; j < tmp.size(); j++){
            pat.push();
            append(pat[LAST], pat[i].slice(1));
            pat[LAST].push(0);
            append(pat[LAST], tmp[j]);
            pat[LAST].push(1);
        }
        tmp.clear();
        pat[i].clear();
    }

    for (uint i = 0; i < pat.size(); i++){
        if (pat[i].size() == 0) continue;
        result.push();
        pat[i].copyTo(result[LAST]);
    }
}


/*
Result from enumAll(4):
  -4
  -2 0 -2 1 
  -1 0 -3 1
  -1 0 -1 0 -2 1 1
  0 -2 1 0 -2 1
  
0 = open brace '['  
1 = close brace ']'  
negative number = #PIs
*/


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Tree representation:


struct DsdTree {
    uint inputs;            // -- number of inputs to this box
    uint atoms;             // -- how many of those are primary input?
    Array<DsdTree*> sub;    // -- subtrees ('inputs - atoms')
};


DsdTree* buildTree(int*& flat)
{
    DsdTree* t = new DsdTree;
    if (*flat < 0){
        t->atoms = -*flat;
        flat++;
    }else
        t->atoms = 0;

    Vec<DsdTree*> s;
    while (*flat == 0){
        flat++;
        s.push(buildTree(flat));
    }
    if (*flat == 1) flat++;

    t->inputs = s.size() + t->atoms;
    t->sub = Array_copy(s);

    return t;
}


DsdTree* buildTree(Vec<int>& enum_result)
{
    enum_result.push(2);    // -- end marker
    int* p = &enum_result[0];
    DsdTree* t = buildTree(p);
    assert(*p == 2);
    enum_result.pop();
    return t;

}


void dumpTree(DsdTree* tree)
{
    if (tree->atoms != 0)
        Write "%_", tree->atoms;
    for (uint i = 0; i < tree->sub.size(); i++){
        Write "[";
        dumpTree(tree->sub[i]);
        Write "]";
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Reduction:


bool reduceInputs(DsdTree* tree)
{
    DsdTree& t = *tree;

    bool did_reduce = false;
    uint j = 0;
    for (uint i = 0; i < t.sub.size(); i++){
        if (t.sub[i]->inputs == 2 && t.sub[i]->atoms == 2){
            // Replace 'atom * atom' with 'atom':
            t.atoms++;
            did_reduce = true;
#if 1
        }else if (t.sub[i]->inputs == 3 && t.sub[i]->atoms == 3){
            // Replace 'op(atom, atom, atom)' with 'atom':
            t.atoms++;
            did_reduce = true;
#endif
        }else{
            did_reduce |= reduceInputs(t.sub[i]);
            t.sub[j++] = t.sub[i];
        }
    }
    t.sub.shrinkTo(j);

    return did_reduce;
}


bool reduceTree(DsdTree* tree)
{
    DsdTree& t = *tree;

    // F(atom, comp) -> comp
    if (t.inputs == 2 && t.atoms == 1){
        t = *t.sub[0];
        return true;
    }

#if 1
    // F(atom, atom, comp) -> comp
    if (t.inputs == 3 && t.atoms == 2){
        t = *t.sub[0];
        return true;
    }
#endif

    return reduceInputs(tree);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


int main(int argc, char** argv)
{
    ZZ_Init;

    if (argc != 2){
        WriteLn "USAGE: enum_dsds <#inputs>";
        exit(1); }

    Vec<Vec<int> > result;
    enumAll(atoi(argv[1]), result);

#if 0
    // Print all DSD shape:
    for (uint i = 0; i < result.size(); i++){
      #if 0
        for (uint j = 0; j < result[i].size(); j++){
            int v = result[i][j];
            if      (v == 0) Write "[";
            else if (v == 1) Write "]";
            else             Write "%_", -v;
        }
        NewLine;
      #endif

        DsdTree* t = buildTree(result[i]);
        dumpTree(t);
        NewLine;
    }
#endif

#if 1
    Vec<DsdTree*> ts;
    for (uint i = 0; i < result.size(); i++)
        ts.push(buildTree(result[i]));

    for(;;){
        bool did_reduce = false;
        uint j = 0;
        for (uint i = 0; i < ts.size(); i++){
            if (ts[i]->inputs == ts[i]->atoms) continue;

            dumpTree(ts[i]);
            if (reduceTree(ts[i])){
                Write " -> ";
                dumpTree(ts[i]);
                did_reduce = true;
            }
            NewLine;

            ts[j++] = ts[i];
        }
        ts.shrinkTo(j);

        if (!did_reduce) break;
        WriteLn "----------------------------------------";
    }
#endif

    return 0;
}


/*
All 6-input DSDs:

    6
    4[2]
    3[3]
    3[1[2]]
    2[4]
    2[2[2]]
    2[1[3]]
    2[1[1[2]]]
    2[[2][2]]
    1[5]
    1[3[2]]
    1[2[3]]
    1[2[1[2]]]
    1[1[4]]
    1[1[2[2]]]
    1[1[1[3]]]
    1[1[1[1[2]]]]
    1[1[[2][2]]]
    1[1[2][2]]
    1[[3][2]]
    1[[1[2]][2]]
    2[2][2]
    1[3][2]
    1[1[2]][2]
    [3][3]
    [3][1[2]]
    [1[2]][3]
    [1[2]][1[2]]
    [4][2]
    [2[2]][2]
    [1[3]][2]
    [1[1[2]]][2]
    [[2][2]][2]
    [2][2][2]

*/
