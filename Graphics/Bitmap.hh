//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Bitmap.hh
//| Author(s)   : Niklas Een
//| Module      : Graphics
//| Description : Simple in-memory image (or bitmap) abstraction.
//|
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//|
//|________________________________________________________________________________________________

#ifndef ZZ__Graphics__Bitmap_hh
#define ZZ__Graphics__Bitmap_hh

#include "Color.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Bitmap:


struct Bitmap {
    bool    owner;      // -- Am I the owner of 'data'?
    bool    flipped;    // -- Is the y-axis flipped? If so 'data' does not point to the address to be freed.
    Color*  data;
    uint    width;
    uint    height;
    int     line_sz;    // -- May be negative, but absolute value is always >= width. Add this to get to the next line.

    Color  operator()(uint x, uint y) const    { return data[line_sz*y+x]; }
    Color& operator()(uint x, uint y)          { return data[line_sz*y+x]; }
    void   set       (uint x, uint y, Color v) { if (x < width && y < height) data[line_sz*y+x] = v; }
    void   setQ      (uint x, uint y, Color v) { data[line_sz*y+x] = v; }

    Bitmap()                                              { zero(); }
    Bitmap(uint w, uint h)                                { zero(); alloc(w, h); }
    Bitmap(uint w, uint h, Color cr)                      { zero(); alloc(w, h, cr); }
    Bitmap(uint w, uint h, Color* d)                      { zero(); init(w, h, d); }
        // -- NOTE! Default is to NOT take ownership!
    Bitmap(uint w, uint h, Color* d, bool own)            { zero(); init(w, h, d, own); }
    Bitmap(uint w, uint h, Color* d, bool own, int lsz)   { zero(); init(w, h, d, own, lsz); }
    Bitmap(Bitmap& src, uint x0, uint y0, uint w, uint h) { zero(); slice(src, x0, y0, w, h); }
   ~Bitmap()                                              { clear(); }


    void clear()
        // -- Restore bitmap to same state as default constructor gives:
        // (1) Free the current bitmap (if should)
        // (2) Set all member variables to their default zero values.
    {
        if (flipped) flip();
        if (owner && data != NULL) xfree(data);
        zero();
    }

    void init(uint w, uint h, Color* d, bool own = false, int lsz = INT_MAX)
        // -- Free the current bitmap (if should), then initialize member variables to given values.
    {
        clear();
        owner  = own;
        width  = w;
        height = h;
        data   = d;
        line_sz =  (lsz == INT_MAX) ? width : lsz;
    }

    void alloc(uint w, uint h, Color cr = Color())
        // -- Free the current bitmap (if should), then allocate (and clear) memory for new bitmap.
        // Ownership is taken for that memory.
    {
        clear();
        width  = w;
        height = h;
        data = xmalloc<Color>(width * height);
        line_sz = width;
        for (uint i = 0; i < width*height; i++) data[i] = cr;
        owner = true;
    }

    void slice(Bitmap& src, uint x0, uint y0, uint w, uint h)
        // -- Free the current bitmap (if should), then make this bitmap a slice of a larger bitmap.
        // No ownership is taken (naturally).
    {
        clear();
        width   = w;
        height  = h;
        data    = src.data + src.line_sz * y0 + x0;
        line_sz = src.line_sz;
        assert(x0 >= 0);
        assert(y0 >= 0);
        assert(x0 + w <= src.width);
        assert(y0 + h <= src.height);
    }

    void flip()
        // -- Turn the bitmap upside down by making 'line_sz' negative. Apply again to undo.
    {
        data += (height-1) * line_sz;
        line_sz = -line_sz;
        flipped = !flipped;
    }

protected:
    void zero() {
        owner   = false;
        flipped = false;
        data    = NULL;
        width   = 0;
        height  = 0;
        line_sz = 0;
    }
};


// Returns x-coordinate (in 'outer') of top-left corner.
// PRE-CONDITION: 'inner' is a slice of 'outer'.
macro uint sliceX0(Bitmap& outer, Bitmap& inner) {
    int diff = inner.data - outer.data;
    return diff % outer.line_sz; }

// Returns y-coordinate (in 'outer') of top-left corner.
// PRE-CONDITION: 'inner' is a slice of 'outer'.
macro uint sliceY0(Bitmap& outer, Bitmap& inner) {
    int diff = inner.data - outer.data;
    return diff / outer.line_sz; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Bitmap -- Basic functions:


macro void fill(Bitmap& bm, Color c)
{
    for (uint y = 0; y < bm.height; y++)
        for (uint x = 0; x < bm.width; x++)
            bm.setQ(x, y, c);
}


macro void fill(Bitmap& bm, uint x0, uint y0, uint x1, uint y1, Color c)
{
    if (x0 >= bm.width || x1 < 0 || y0 >= bm.height || y1 < 0) return;
    newMin(x1, bm.width); newMin(y1, bm.height);
    for (uint y = y0; y < y1; y++)
        for (uint x = x0; x < x1; x++)
            bm.setQ(x, y, c);
}


macro void hline(Bitmap& bm, uint x0, uint y0, uint x1, Color c)
{
    if (y0 < 0 || y0 >= bm.height) return;
    newMin(x1, bm.width);
    for (uint x = x0; x < x1; x++)
        bm.setQ(x, y0, c);
}

macro void vline(Bitmap& bm, uint x0, uint y0, uint y1, Color c)
{
    if (x0 < 0 || x0 >= bm.width) return;
    newMin(y1, bm.height);
    for (uint y = y0; y < y1; y++)
        bm.setQ(x0, y, c);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
