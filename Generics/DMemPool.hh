//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : DMemPool.hh
//| Author(s)   : Niklas Een
//| Module      : Generics
//| Description : Dynamic memory pool for objects of size 2^k words.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Generics__DMemPool_hh
#define ZZ__Generics__DMemPool_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


typedef uint d_off;     // Offset type for dynamic memory pool.


// Dynamic memory pool for objects of size 2^k words, starting at 2^2 (i.e. 4 words = 16 bytes is
// the smallest allocatable size). To save memory (for the user), 32-bit offsets are returned
// rather than pointers. This limits the total addressable memory to 64 GB. Each individual block
// is limited to 2 GB. The implementation is optimized for many small objects. Don't use it for 
// singleton objects in the GB range.
//
class DMemPool {
    uint*   alloc_ptr;       // -- actual pointer to free ('mem_base' may start a few words inside this block for alignment reasons)
    uint*   mem_base;        // -- base memory (portions of this is what is returned to the user)
    uint64* freemask;        // -- one bit per four 'uint's (smallest allocatable unit); only edges of a block is marked '1' if free
    uint    n_freelists;     // -- how many freelists are currently in use ('idxToSize()' of this equals amount of memory currently reserved)
    uint    align_mask;      // -- defined maximum alignment required (eg. 0xFFF means that if 12 last bits are zero, then that is good enough for ANY block size)

  //________________________________________
  //  Minor helpers:

    static const uint max_freelists = 30;
    static const uint freelists_header_size = max_freelists * 4;

    static uint idxToSize(uint freelist_idx) { return 1 << (freelist_idx + 2); }
    static uint sizeToIdx(uint n_words)      { return bitsNeeded((n_words-1) >> 2); }

    uint*  toPtr(d_off offset) const { return mem_base + (4 * offset); }
    d_off  toOff(uint* ptr)    const { return (ptr - mem_base) >> 2; }

    uint*  freelistHead(uint freelist_idx) { return mem_base + 4 * freelist_idx; }

    void   markFree  (d_off off, uint sz);
    void   unmarkFree(d_off off, uint sz);
    bool   isFree    (d_off off);

    uint   wordsReserved() const { return idxToSize(n_freelists-1); }

  //________________________________________
  //  Major helpers:

    bool  mergeWithNeighbor(d_off& offset0, uint& idx);
    void  growPool();
    void  trimPool();
    d_off allocHelper(uint freelist_idx);
    void  freeHelper(d_off offset, uint size);

public:
  //________________________________________
  //  Public interface:

    DMemPool();
   ~DMemPool();

    void clear() { reconstruct(*this); }

    // NOTE! All sizes are in terms of 'uint's.

    d_off alloc(uint size);
        // -- return offset to newly allocated block (a word = 4 bytes). 'size == 0' is not allowed.
    d_off realloc(d_off offset, uint old_size, uint new_size);
        // -- change block size (data may be relocated, returns new offset).
    void  free(d_off offset, uint size);
        // -- free block pointed to by offset. 'size' must match that of the 'alloc()' or 'realloc()' that generated the offset
    uint* deref(d_off offset) const { return toPtr(offset); }
        // -- get a pointer to the block (only valid until next 'alloc()', 'free()' or 'realloc()' call).

    bool auto_trim;     // -- TRUE by default. Set to FALSE to disallow shrinking the internal memory pool

  //________________________________________
  //  Statistics:

    // Treat as read only!
    size_t mem_reserved;    // -- how much memory have we internally reserved through 'malloc()'?
    size_t mem_accessed;    // -- how much of that have we touched? (remaining tail is just a MMU affair; no physical RAM is used)
    size_t mem_useralloc;   // -- how much memory has the user requested through 'alloc()' and 'realloc()'?
    size_t mem_pow2alloc;   // -- each allocation is round up to the nearest power of 2 -- how much memory has the user requested counting that extra memory too?
        // -- all values are as of *now* (not "peak" or "accumulated")
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helper:


inline void DMemPool::markFree(d_off off, uint sz)
{
    off -= freelists_header_size >> 2;
    //**/WriteLn "  \a/(marking free: %_..%_ of size %_)\a/", off*4, off*4+sz-1, sz;
    freemask[off >> 6] |= uint64(1) << (off & 63);
    off += (sz >> 2) - 1;
    freemask[off >> 6] |= uint64(1) << (off & 63);
}


inline void DMemPool::unmarkFree(d_off off, uint sz)
{
    off -= freelists_header_size >> 2;
    //**/WriteLn "  \a/(UN-marking free: %_..%_ of size %_)\a/", off*4, off*4+sz-1, sz;
    freemask[off >> 6] &= ~(uint64(1) << (off & 63));
    off += (sz >> 2) - 1;
    freemask[off >> 6] &= ~(uint64(1) << (off & 63));
}


inline bool DMemPool::isFree(d_off off)
{
    off -= freelists_header_size >> 2;
    return freemask[off >> 6] & (uint64(1) << (off & 63));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
