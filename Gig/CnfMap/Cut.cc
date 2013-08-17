//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Cut.cc
//| Author(s)   : Niklas Een
//| Module      : CnfMap
//| Description : Represents small cuts (size <= 4). 
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "ZZ_Npn4.hh"
#include "Cut.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// 'Cut' class, implementation:


void Cut::trim()
{
    if (sz == 0) return;
    while ((ftb & 0x5555) == ((ftb & 0xAAAA) >> 1)){
        ftb = apply_perm4[PERM4_3012][ftb];
        inputs[0] = inputs[1];
        inputs[1] = inputs[2];
        inputs[2] = inputs[3];
        sz--;
        if (sz == 0) return;
    }

    if (sz == 1) return;
    while ((ftb & 0x3333) == ((ftb & 0xCCCC) >> 2)){
        ftb = apply_perm4[PERM4_0312][ftb];
        inputs[1] = inputs[2];
        inputs[2] = inputs[3];
        sz--;
        if (sz == 1) return;
    }

    if (sz == 2) return;
    while ((ftb & 0x0F0F) == ((ftb & 0xF0F0) >> 4)){
        ftb = apply_perm4[PERM4_0132][ftb];
        inputs[2] = inputs[3];
        sz--;
        if (sz == 2) return;
    }

    if (sz == 3) return;
    if ((ftb & 0x00FF) == ((ftb & 0xFF00) >> 8))
        sz--;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Cut enumeration:


// PRE-CONDITION: Inputs of 'cut1' and 'cut2' are sorted.
// Output: A cut representing AND of 'cut1' and 'cut2' with signs 'inv1' and 'inv2' respectively; 
// or 'Cut_NULL' if more than four inputs would be required.
Cut combineCuts_And(const Cut& cut1, const Cut& cut2, bool inv1, bool inv2, bool use_xor)
{
    if (moreThanFourBits(cut1.abstr | cut2.abstr))
        return Cut_NULL;

    Cut   result(cut_empty_, 0x0000);
    uchar perm1[4] = {0, 1, 2, 3};
    uchar perm2[4] = {0, 1, 2, 3};
    uint  i = 0;
    uint  j = 0;
    if (cut1.size() == 0) goto FlushCut2;
    if (cut2.size() == 0) goto FlushCut1;
    for(;;){
        if (result.size() == 4) return Cut_NULL;
        if (cut1[i] < cut2[j]){
            swp(perm1[i], perm1[result.size()]);
            result.push(cut1[i]), i++;
            if (i >= cut1.size()) goto FlushCut2;
        }else if (cut1[i] > cut2[j]){
            swp(perm2[j], perm2[result.size()]);
            result.push(cut2[j]), j++;
            if (j >= cut2.size()) goto FlushCut1;
        }else{
            swp(perm1[i], perm1[result.size()]);
            swp(perm2[j], perm2[result.size()]);
            result.push(cut1[i]), i++, j++;
            if (i >= cut1.size()) goto FlushCut2;
            if (j >= cut2.size()) goto FlushCut1;
        }
    }

  FlushCut1:
    if (result.size() + cut1.size() - i > 4) return Cut_NULL;
    while (i < cut1.size()){
        swp(perm1[i], perm1[result.size()]);
        result.push(cut1[i]), i++; }
    goto Done;

  FlushCut2:
    if (result.size() + cut2.size() - j > 4) return Cut_NULL;
    while (j < cut2.size()){
        swp(perm2[j], perm2[result.size()]);
        result.push(cut2[j]), j++; }
    goto Done;

  Done:;

    // Compute new FTB:
    ushort ftb1 = cut1.ftb ^ (inv1 ? (ushort)0xFFFF : (ushort)0x0000);
    ushort ftb2 = cut2.ftb ^ (inv2 ? (ushort)0xFFFF : (ushort)0x0000);
    ftb1 = apply_perm4[pseq4_to_perm4[pseq4Make(perm1[0], perm1[1], perm1[2], perm1[3])]][ftb1];
    ftb2 = apply_perm4[pseq4_to_perm4[pseq4Make(perm2[0], perm2[1], perm2[2], perm2[3])]][ftb2];

    result.ftb = (!use_xor) ? (ftb1 & ftb2) : (ftb1 ^ ftb2);
    result.trim();

    return result;
}


Cut combineCuts_Mux(const Cut& cut1, const Cut& cut2, const Cut& cut3, bool inv1, bool inv2, bool inv3)
{
    if (moreThanFourBits(cut1.abstr | cut2.abstr | cut3.abstr))
        return Cut_NULL;

    Cut   result(cut_empty_, 0x0000);
    uchar perm1[4] = {0, 1, 2, 3};
    uchar perm2[4] = {0, 1, 2, 3};
    uchar perm3[4] = {0, 1, 2, 3};
    uint  i = 0;
    uint  j = 0;
    uint  k = 0;

    uint  done = 0;
    if (cut1.size() == 0) done |= 1;
    if (cut2.size() == 0) done |= 2;
    if (cut3.size() == 0) done |= 4;

    for(;;){
        if (result.size() == 4) return Cut_NULL;

        GLit m = GLit_MAX;
        if ((done & 1) == 0) newMin(m, cut1[i]);
        if ((done & 2) == 0) newMin(m, cut2[j]);
        if ((done & 4) == 0) newMin(m, cut3[k]);

        if ((done & 1) == 0 && cut1[i] == m){
            swp(perm1[i], perm1[result.size()]);
            i++;
            if (i == cut1.size()) done |= 1; }

        if ((done & 2) == 0 && cut2[j] == m){
            swp(perm2[j], perm2[result.size()]);
            j++;
            if (j == cut2.size()) done |= 2; }

        if ((done & 4) == 0 && cut3[k] == m){
            swp(perm3[k], perm3[result.size()]);
            k++;
            if (k == cut3.size()) done |= 4; }

        result.push(m);

        if (done == 7)
            break;
    }

    // Compute new FTB:
    ushort ftb1 = cut1.ftb ^ (inv1 ? (ushort)0xFFFF : (ushort)0x0000);
    ushort ftb2 = cut2.ftb ^ (inv2 ? (ushort)0xFFFF : (ushort)0x0000);
    ushort ftb3 = cut3.ftb ^ (inv3 ? (ushort)0xFFFF : (ushort)0x0000);
    ftb1 = apply_perm4[pseq4_to_perm4[pseq4Make(perm1[0], perm1[1], perm1[2], perm1[3])]][ftb1];
    ftb2 = apply_perm4[pseq4_to_perm4[pseq4Make(perm2[0], perm2[1], perm2[2], perm2[3])]][ftb2];
    ftb3 = apply_perm4[pseq4_to_perm4[pseq4Make(perm3[0], perm3[1], perm3[2], perm3[3])]][ftb3];

    result.ftb = (ftb1 & ftb2) | (~ftb1 & ftb3);
    result.trim();

    return result;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
