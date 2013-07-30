//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : AlignedAlloc.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Allocate aligned memory blocks.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| This ADT will return blocks of size 'small_block', aligned to its size, (which MUST be a power
//| of 2) by allocating memory chunks of size 'big_block' and trimming away beginning and end of
//| that block. To not waste too much, 'big_block' should be 50-100 times larger than 'small_block'
//| or so. If 'skew' is specified, the block is shifted backwards, so that the returned pointer +
//| skew is aligned.
//|________________________________________________________________________________________________

#ifndef ZZ__Netlist__AlignedAlloc_h
#define ZZ__Netlist__AlignedAlloc_h
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Profiling:


extern size_t total_waste;
extern size_t total_alloc;
extern bool   profile_aligned_alloc;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Implementation:


template<size_t small_block_, size_t big_block_, size_t skew = 0>
class AlignedAlloc {
    Vec<char*> blocks;
    char* curr;
    char* end;

public:
    static const size_t small_block = small_block_;
    static const size_t big_block   = big_block_;

    char* alloc();  // -- Return pointer to newly allocated (and aligned) 'small_block' sized memory area.
    void  clear();  // -- Free all allocations.
    char* allocBig(size_t n_bytes);
        // -- Return pointer to newly allocated (and aligned) 'n_bytes' sized memory area. NOTE!
        // A lot less memory efficient than 'alloc()'; intended to be used for large blocks.
    void  moveTo(AlignedAlloc<small_block_, big_block_>& dst);

    AlignedAlloc() : curr(NULL), end(NULL) {}
   ~AlignedAlloc() { clear(); }
};


template<size_t sb, size_t bb, size_t skew>
char* AlignedAlloc<sb,bb,skew>::alloc()
{
    static_assert_(bb >= 2 * sb - 1);        // 'big_block' must be at least twice the size of 'small_block' (or else we cannot guarantee alignment)
    static_assert_((sb & (sb-1)) == 0);      // 'small_block' must be power of 2

    if (!curr || curr + sb > end){
        if (profile_aligned_alloc){
            if (curr) printf("  (final waste: %u bytes)\n", uint(end - curr));
            total_waste += end - curr; }

        char* start = xmalloc<char>(bb);
        curr = (char*)((uintp(start + skew) + (sb-1)) & ~uintp(sb-1)) - skew;
        end = start + bb;
        blocks.push(start);     // -- remember original pointer for 'xfree()'

        if (profile_aligned_alloc){
            printf("ALLOC (sb=%u  bb=%u): %p..%p   start rounded to: %p  (wasting %u bytes)\n", (uint)sb, (uint)bb, start, end, curr, uint(curr-start));
            total_waste += curr - start; }
    }

    char* ret = curr;
    curr += sb;
    if (profile_aligned_alloc){
        total_alloc += sb; }

    return ret;
}


template<size_t sb, size_t bb, size_t skew>
char* AlignedAlloc<sb,bb,skew>::allocBig(size_t n_bytes)
{
    char* start = xmalloc<char>(n_bytes + sb - 1);
    char* ret = (char*)((uintp(start + skew) + (sb-1)) & ~uintp(sb-1)) - skew;
    assert(ret >= start);
    assert(ret < start + n_bytes + sb - 1);

    blocks.push(start);         // -- remember original pointer for 'xfree()'
    return ret;
}


template<size_t sb, size_t bb, size_t skew>
inline void AlignedAlloc<sb,bb,skew>::clear()
{
    for (uind i = 0; i < blocks.size(); i++)
        xfree(blocks[i]);
    blocks.clear(true);
    curr = end = NULL;
}


template<size_t sb, size_t bb, size_t skew>
inline void AlignedAlloc<sb,bb,skew>::moveTo(AlignedAlloc<sb, bb>& dst)
{
    dst.clear();
    blocks.moveTo(dst.blocks);
    dst.curr = curr;
    dst.end  = end;
    curr = end = NULL;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
