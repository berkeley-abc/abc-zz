//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Color.hh
//| Author(s)   : Niklas Een
//| Module      : Graphics
//| Description : Simple 8-bit color abstraction (with alpha channel).
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//| The underlying representation is the same on all platforms viewed as bytes. But because
//| of endianness, casting to a 'uint' may give different results on different platform.
//| Use 'color.data()' to convert representation to a platform independent 'uint' and
//| 'toColor(data : uint)' to convert back.
//|________________________________________________________________________________________________

#ifndef ZZ__Graphics__Color_hh
#define ZZ__Graphics__Color_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


struct Color {
    uchar r;
    uchar g;
    uchar b;
    uchar a;        // -- 255=opaque, 0=transparent 

    Color() : r(0), g(0), b(0), a(0) {}
    explicit Color(uint gray) : r(gray), g(gray), b(gray), a(255) {}
    Color(uint r_, uint g_, uint b_)         : r(r_), g(g_), b(b_), a(255) {}
    Color(uint r_, uint g_, uint b_, uint a_) : r(r_), g(g_), b(b_), a(a_)  {}

    uchar  operator[](uint i) const { assert_debug(i < 4); return ((uchar*)&r)[i]; }
    uchar& operator[](uint i)       { assert_debug(i < 4); return ((uchar*)&r)[i]; }
        // -- access the RGBA tuple as an array.

  #if !defined(ZZ_BYTE_ORDER_BIG_ENDIAN)
    uint data() const { return *(uint*)this; }
  #else
    uint data() const { uint v = *(uint*)this; return (v>>24) | ((v>>8)&0xFF00) | ((v<<8)&0xFF0000) | (v<<24); }
  #endif

    uint hash() const { return this->data(); }
    bool operator< (const Color& other) const { return this->data() <  other.data(); }
    bool operator==(const Color& other)       { return this->data() == other.data(); }
};


#if !defined(ZZ_BYTE_ORDER_BIG_ENDIAN)
macro Color toColor(uint data) { return *(Color*)&data; }
#else
macro Color toColor(uint data) { data = (data>>24) | ((data>>8)&0xFF00) | ((data<<8)&0xFF0000) | (data<<24); return *(Color*)&data; }
#endif


template<> fts_macro void write_(Out& out, const Color& v)
{
    if (v.data() == 0)
        FWrite(out) "#<nil>";
    else{
        FWrite(out) "#%.2X%.2X%.2X", v.r, v.g, v.b;
        if (v.a != 255)
            FWrite(out) "(%.2X)", v.a;
    }
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
