//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TotalOrder.cc
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Maintain a total order of elements (represented by a integer ID).
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This ADT efficiently maintains a dynamic total order of some elements e0, e1, e2,...e[size-1].
//| Elements are referenced by their ID which must be a small integer (for efficiency reasons,
//| it is preferable, but not required, that the IDs are contigous). Elements not yet in the order
//| can be placed anywhere in it. Elemens already in the order can be removed. A constant time
//| 'lessThan()' method can compare two elements. There are also mechanisms for iterating over all
//| elements, either in the order of the ADT or in ascending ID order.
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "TotalOrder.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#define MAX_PRIO (uint64(1) << 63)      // -- must be consistent with definition in header file!


// Clear current order (if any) and set it to 'elems'. This is faster than using 'addLast()'
// repeatedly on an empty order (but semantically equivalent).
void TotalOrder::init(const Vec<uint>& elems, bool dealloc)
{
    clear(dealloc);
    if (elems.size() == 0) return;

    n_elems = elems.size();
    cells.growTo(n_elems + 1);

    uint64 mult = MAX_PRIO / elems.size(); assert(mult > 1);
    uint64 base = mult >> 1;
    for (uint i = 0; i < n_elems; i++){
        cells[i+1].prio = base + i * mult;
        cells[i+1].prev = i;
        cells[i+1].next = i+2;
    }
    cells[n_elems].next = 0;
}


// An attempt was made to insert element after 'x', but its successor has a priority just 1 higher.
void TotalOrder::renumber(uint x)
{
    double density = 0.5;
    double T = 0.707107;

    uint b = x;  // -- backward tracing
    uint f = x;  // -- forward tracing
    uint count = 1;
    uint64 x_prio = cells[x].prio;
    uint64 mask   = 1;
    for(;;){
        uint64 lo = x_prio & ~mask;
        uint64 hi = x_prio |  mask;

        while (cells[b].prev != 0 && cells[cells[b].prev].prio >= lo) b = cells[b].prev, count++;
        while (cells[f].next != 0 && cells[cells[f].next].prio <= hi) f = cells[f].next, count++;

        if (double(count) / (mask + 1) < density){
            uint64 mult = (mask + 1) / count; assert(mult > 1);
            uint64 base = lo + (mult >> 1);
            assert(base + (count - 1) * mult < MAX_PRIO);
            uint y = b;
            for (uint i = 0; i < count; i++){
                cells[y].prio = base + i * mult;
                y = cells[y].next;
            }
            assert(cells[y].prev == f);
            assert(cells[b].prio > cells[cells[b].prev].prio + 1);
            assert(cells[f].next == 0 || cells[f].prio < cells[cells[f].next].prio - 1);
            break;
        }

        mask = (mask << 1) | 1;
        density *= T;
    }
}


// Debugging...
void TotalOrder::dump()
{
    printf("Cells:");
    uint x = 0;
    do{
        printf("  %u(%g)", x - 1, double(cells[x].prio) / MAX_PRIO);
        uint y = cells[x].next;
        assert(cells[y].prev == x);
        x = y;
    }while (x != 0);
    printf("\n");
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
