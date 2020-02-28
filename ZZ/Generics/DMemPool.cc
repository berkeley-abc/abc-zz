//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DMemPool.cc
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Dynamic memory pool for objects of size 2^k words.
//| 
//| (C) Copyright 2010-2014, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#include "Prelude.hh"
#include "DMemPool.hh"

namespace ZZ {
using namespace std;


// Layout of a freed memory block (minimum size 4 'uint's):
//
//   [Next, Prev, Size, ...]
//
// All offsets are in terms of quadruples of 'uint's (smallest unit) to save bits.
// Sizes are in log-scale ('log2(size) - 2', as returned by 'sizeToIdx()').
//
// The bits corresponding to the first and last element of a block in 'freemask' is set
// to 1 if the block is freed.


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Create / Resize:


DMemPool::DMemPool()
{
  #if defined(_SC_PAGESIZE)
    uint pagesize = (uint)sysconf(_SC_PAGESIZE);
  #else
    uint pagesize = 4096;               // -- educated guess
  #endif
    align_mask = (pagesize - 1) >> 2;   // -- express in words not bytes

    size_t sz  = align_mask + 1;
    uint   idx = sizeToIdx(sz);
    alloc_ptr = xmalloc<uint>(sz + align_mask + freelists_header_size);

    uintp p = uintp(alloc_ptr + freelists_header_size) >> 2;
    p = (p + align_mask) & ~uintp(align_mask);
    mem_base = (uint*)(p << 2) - freelists_header_size;

    uint freemask_sz = (sz + 255) / 256;
    freemask = xmalloc<uint64>(freemask_sz);
    for (uint i = 0; i < freemask_sz; i++)
        freemask[i] = 0;

    n_freelists = idx + 1;
    for (uint i = 0; i < n_freelists; i++){
        freelistHead(i)[0] = freelistHead(i)[1] = toOff(freelistHead(i)); }

    uint* block = mem_base + freelists_header_size;
    uint* fl    = freelistHead(idx);
    fl[0] = fl[1] = toOff(block);

    block[0] = toOff(fl);
    block[1] = toOff(fl);
    block[2] = idx;
    markFree(toOff(block), sz);

    auto_trim = true;

    mem_reserved  = wordsReserved() * size_t(4);
    mem_accessed  = 0;
    mem_useralloc = 0;
    mem_pow2alloc = 0;
}


DMemPool::~DMemPool()
{
    xfree(alloc_ptr);
    xfree(freemask);
}


void DMemPool::growPool()
{
    uint   idx = n_freelists;
    size_t sz = size_t(idxToSize(idx) - 1) + 1;   // -- trick to handle 4GB case
    uint*  new_alloc_ptr = xrealloc(alloc_ptr, sz + align_mask + freelists_header_size);

    uintp p = uintp(new_alloc_ptr + freelists_header_size) >> 2;
    p = (p + align_mask) & ~uintp(align_mask);
    uint* new_mem_base = (uint*)(p << 2) - freelists_header_size;

    if (new_alloc_ptr != alloc_ptr && (mem_base - alloc_ptr) != (new_mem_base - new_alloc_ptr)){
        // Data were mis-aligned by realloc:
        int diff = (mem_base - alloc_ptr) - (new_mem_base - new_alloc_ptr);
        memmove(new_mem_base, new_mem_base + diff, 4 * (sz/2 + freelists_header_size));
    }
    alloc_ptr = new_alloc_ptr;
    mem_base  = new_mem_base;

    uint freemask_sz = (sz + 255) / 256;
    uint freemask0   = (wordsReserved() + 255) / 256;
    freemask = xrealloc(freemask, freemask_sz);
    for (uint i = freemask0; i < freemask_sz; i++)
        freemask[i] = 0;

    for (uint i = n_freelists; i <= idx; i++){
        freelistHead(i)[0] = freelistHead(i)[1] = toOff(freelistHead(i)); }
    n_freelists = idx + 1;

    uint* block = mem_base + freelists_header_size;
    if (isFree(toOff(block)) && block[2] == idx-1){
        // Empty left block -- merge with right block:
        unmarkFree(toOff(block), idxToSize(idx-1));
        uint* fl = freelistHead(idx-1);
        fl[0] = fl[1] = toOff(fl);
    }else{
        // Can only put right block into free list:
        sz >>= 1;
        block += sz;
        idx--;
    }

    uint* fl = freelistHead(idx);
    fl[0] = fl[1] = toOff(block);

    block[0] = toOff(fl);
    block[1] = toOff(fl);
    block[2] = idx;
    markFree(toOff(block), sz);

    mem_reserved = wordsReserved() * size_t(4);
}


void DMemPool::trimPool()
{
    // If all memory is freed, shrink back to default size.

    uint* fl = freelistHead(n_freelists - 1);
    if (fl[0] != toOff(fl)){
        // All memory has been freed, reset memory pool:
        this->~DMemPool();
        new (this) DMemPool();

    }else{
        uint min_freelists = sizeToIdx(align_mask + 1) + 1;
        while (n_freelists > min_freelists){
            assert(min_freelists >= 3);
            uint* fl1 = freelistHead(n_freelists - 2);
            uint* fl2 = freelistHead(n_freelists - 3);

            if (fl1[0] != toOff(fl1) && fl2[0] != toOff(fl2) && /*must be second half of memory*/fl1[0] != max_freelists){
                // Both top freelist have elements (must be just one in each) -- free the top one:
                assert(fl1[0] == fl1[1]);
                assert(fl2[0] == fl2[1]);
                fl1[0] = fl1[1] = toOff(fl1);   // -- remove element from biggest freelist

                uint   idx = n_freelists - 2;
                size_t sz = idxToSize(idx);
                uint*  new_alloc_ptr = xrealloc(alloc_ptr, sz + align_mask + freelists_header_size);

                uintp p = uintp(new_alloc_ptr + freelists_header_size) >> 2;
                p = (p + align_mask) & ~uintp(align_mask);
                uint* new_mem_base = (uint*)(p << 2) - freelists_header_size;

                if (new_alloc_ptr != alloc_ptr && (mem_base - alloc_ptr) != (new_mem_base - new_alloc_ptr)){
                    // Data were mis-aligned by realloc:
                    int diff = (mem_base - alloc_ptr) - (new_mem_base - new_alloc_ptr);
                    memmove(new_mem_base, new_mem_base + diff, 4 * (sz + freelists_header_size));
                }
                alloc_ptr = new_alloc_ptr;
                mem_base  = new_mem_base;

                uint freemask_sz = (sz + 255) / 256;
                freemask = xrealloc(freemask, freemask_sz);

                n_freelists--;

            }else
                break;
        }
        mem_reserved = wordsReserved() * size_t(4);
        newMin(mem_accessed, mem_reserved);
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Allocation / Free:


inline bool DMemPool::mergeWithNeighbor(d_off& offset0, uint& idx)
{
    if (idx >= n_freelists - 1)
        return false;

    uint offset = offset0 - max_freelists;    // -- here we are counting in units of 4 'uint's.
    uint sz = 1 << idx;
    assert(offset == (offset & ~(sz - 1)));

    uint  neigh = offset ^ sz;
    uint  neigh0 = neigh + max_freelists;
    uint* bl = toPtr(neigh0);
    bool  is_free = isFree(neigh0) && bl[2] == idx;
    if (is_free){
        // Merge:
        toPtr(bl[0])[1] = bl[1];    // -- unlink from freelist
        toPtr(bl[1])[0] = bl[0];

        unmarkFree(neigh0, idxToSize(idx));
        if (neigh < offset)
            offset0 = neigh0;
        idx++;
        return true;

    }else
        return false;
}


uint DMemPool::allocHelper(uint idx)
{
    assert(idx < max_freelists);

    while (idx >= n_freelists){
        growPool(); }
    if (idx == n_freelists - 1 && freelistHead(idx) == toPtr(freelistHead(idx)[0])){
        growPool(); }

    uint* fl = freelistHead(idx);
    if (toPtr(fl[0]) == fl){
        // Empty freelist, allocate bigger block and split it:
        assert(toPtr(fl[1]) == fl);
        d_off off = allocHelper(idx + 1);
        fl = freelistHead(idx); // -- may have been moved
        uint* block = toPtr(off);
        uint  sz = idxToSize(idx);
        block += sz;
        markFree(toOff(block), sz);
        fl[0] = fl[1] = toOff(block);
        block[0] = block[1] = toOff(fl);
        block[2] = idx;

        newMax(mem_accessed, 4*uint(toPtr(off) - mem_base - max_freelists) + sz);
        return off;

    }else{
        // Non-empty freelist, chop of one block:
        uint* block = toPtr(fl[0]);
        uint  sz    = idxToSize(idx);
        unmarkFree(fl[0], sz);
        assert(block[2] == idx);
        d_off off = fl[0];
        fl[0] = block[0];
        toPtr(fl[0])[1] = toOff(fl);

        newMax(mem_accessed, 4*uint(toPtr(off) - mem_base - max_freelists) + sz);
        return off;
    }

    return 0;
}


void DMemPool::freeHelper(d_off offset, uint n_words)
{
    n_words = (n_words + 3) & ~3u;
    uint idx  = sizeToIdx(n_words);
    uint size = idxToSize(idx);

    while (mergeWithNeighbor(offset, idx));
    size = idxToSize(idx);

    uint* fl = freelistHead(idx);
    uint* bl = toPtr(offset);

    bl[0] = fl[0];          // -- insert free block at head
    bl[1] = toOff(fl);
    toPtr(fl[0])[1] = offset;
    fl[0] = offset;
    bl[2] = idx;
    markFree(offset, size);
}


uint DMemPool::alloc(uint n_words)
{
    assert(n_words > 0);

    mem_useralloc += n_words * 4;
    n_words = (n_words + 3) & ~3u;
    mem_pow2alloc += idxToSize(sizeToIdx(n_words)) * 4;
    uint ret = allocHelper(sizeToIdx(n_words));
    return ret;
}


void DMemPool::free(d_off offset, uint n_words)
{
    mem_useralloc -= n_words * 4;
    mem_pow2alloc -= idxToSize(sizeToIdx((n_words + 3) & ~3u)) * 4;
    freeHelper(offset, n_words);
    if (auto_trim)
        trimPool();
}


d_off DMemPool::realloc(d_off offset, uint old_size, uint new_size)
{
    mem_useralloc += new_size * 4;
    mem_useralloc -= old_size * 4;
    uint old_alloc = sizeToIdx((old_size + 3) & ~3u);
    uint new_alloc = sizeToIdx((new_size + 3) & ~3u);
    if (old_alloc == new_alloc)
        return offset;

    mem_pow2alloc += idxToSize(new_alloc) * 4;
    mem_pow2alloc -= idxToSize(old_alloc) * 4;

    uint* bl = toPtr(offset);
    uint d0 = bl[0];
    uint d1 = bl[1];
    uint d2 = bl[2];

    freeHelper(offset, old_size);
    d_off ret = allocHelper(new_alloc);
    bl = toPtr(offset);
    uint* new_bl = toPtr(ret);
    if (offset != ret)
        memmove(new_bl, bl,  min_(old_size, new_size) * 4);
    new_bl[0] = d0;
    new_bl[1] = d1;
    new_bl[2] = d2;

    if (auto_trim)
        trimPool();

    return ret;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
