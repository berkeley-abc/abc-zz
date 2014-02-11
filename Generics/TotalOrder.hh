//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : TotalOrder.hh
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

#ifndef ZZ__Generics__TotalOrder_h
#define ZZ__Generics__TotalOrder_h
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


#define MAX_PRIO (uint64(1) << 63)      // (#undef:ed below)


// Scales up to 2^30 elements. We use 64-bit priorities internally, which must be increased to 
// scale beyond this. The elements should be chosen as small, contiguous integers (0, 1, 2, 3...). 
//
class TotalOrder {
protected:
    struct Cell {
        uint64  prio;
        uint    prev;
        uint    next;

        Cell() : prev(UINT_MAX) {}
        bool null() const { return prev == UINT_MAX; }
    };

    Vec<Cell> cells;
        // -- Linked list using indices as apointers. NOTE! "index" == "elem# + 1".
        // 'cells[0]' is the sentinel pointing to beginning/end of the linked list. 

    uint n_elems;

    void add(uint x, uint y);
    void renumber(uint x);

public:
    TotalOrder()                       { clear(); }
    TotalOrder(const Vec<uint>& elems) { init(elems); }

    void clear(bool dealloc = false);
    void init (const Vec<uint>& elems, bool dealloc = false);

    // Manipulating the order:
    void addFirst (uint new_elem);                      // -- it is illegal to add the same element twice; you must 'remove()' it first.
    void addLast  (uint new_elem);
    void addBefore(uint existing_elem, uint new_elem);
    void addAfter (uint existing_elem, uint new_elem);
    void remove   (uint existing_elem);
    bool lessThan (uint elem1, uint elem2) const;       // -- constant time
    bool has      (uint elem) const;                    // -- constant time

    // Accessing the underlying linked list:
    uint count() const { return n_elems; }              // -- 'count' is the number of element in the order.
    uint size () const { return cells.size() - 1; }     // -- 'size' is the highest ID of an element in the order plus one.
    uint first() const { return cells[0].next - 1; }
    uint last () const { return cells[0].prev - 1; }
    uint next (uint elem) const { return cells[elem + 1].next - 1; }   // -- returns UINT_MAX if none.
    uint prev (uint elem) const { return cells[elem + 1].prev - 1; }   // -- returns UINT_MAX if none.
        // -- if you don't care about traversing elements in order, just use 'has()'
        // and loop from 0 to size - 1.

    // Debug:
    void dump();
};


//=================================================================================================
// -- Macros:


// Example usages:
// 
//     For_TotalOrder(t, x)            -- in the order of 't'
//         printf("%d\n", x);
// 
//     For_TotalOrder_Elems(t, x)      -- in order of growing IDs
//         printf("%d\n", x);

#define For_TotalOrder(t_order, var) \
    for (uint var = (t_order).first(); var != UINT_MAX; var = (t_order).next(x))

#define For_TotalOrder_Elems(t_order, var) \
    for (uint var = 0; var < (t_order).size(); var++) \
        if (!(t_order).has(var)); else


//=================================================================================================
// -- Implementation:


inline void TotalOrder::clear(bool dealloc)
{
    cells.clear(dealloc);       // -- 'dealloc' means free the memory; otherwise hold on to if for efficient reuse

    n_elems = 0;
    cells.push();
    cells[0].prio = 0;
    cells[0].prev = 0;
    cells[0].next = 0;
}


// Internal helper. Adds 'y' before 'z', counting elements from 1 not 0.
//
inline void TotalOrder::add(/*existing*/uint z, /*new*/uint y)
{
    n_elems++;

    Cell& cell_y = cells(y);
    assert(cell_y.null());      // -- if failed, you tried to insert the same element twice.

    uint x = cells[z].prev;     // -- 'x' current predecessor of 'z', so the final order will be 'x, y, z'.
    uint64 z_prio = (z == 0) ? MAX_PRIO : cells[z].prio;
    if (z_prio - cells[x].prio == 1){
        renumber(x);
        z_prio = (z == 0) ? MAX_PRIO : cells[z].prio;
    }
    assert(z_prio - cells[x].prio > 1);
    cells[y].prio = (z_prio + cells[x].prio) >> 1;

    cells[x].next = y;
    cells[y].prev = x;
    cells[y].next = z;
    cells[z].prev = y;
}


inline void TotalOrder::remove(uint elem)
{
    assert(has(elem));
    n_elems--;

    uint y = elem + 1;
    uint x = cells[y].prev;
    uint z = cells[y].next;
    cells[x].next = z;
    cells[z].prev = x;
    cells[y].prev = UINT_MAX;    // -- indicates empty cell.
}


inline bool TotalOrder::has(uint elem) const {
    uint x = elem + 1;
    return x < (uint)cells.size() && !cells[x].null(); }

inline bool TotalOrder::lessThan(uint elem1, uint elem2) const {
    uint x = elem1 + 1, y = elem2 + 1;
    return cells[x].prio < cells[y].prio; }

inline void TotalOrder::addBefore(uint existing_elem, uint new_elem) {
    assert(has(existing_elem));
    add(existing_elem + 1, new_elem + 1); }

inline void TotalOrder::addAfter(uint existing_elem, uint new_elem) {
    assert(has(existing_elem));
    uint z = cells[existing_elem + 1].next;
    add(z, new_elem + 1); }

inline void TotalOrder::addLast(uint new_elem) {
    add(0, new_elem + 1); }

inline void TotalOrder::addFirst(uint new_elem) {
    uint z = cells[0].next;
    add(z, new_elem + 1); }


#undef MAX_PRIO


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
