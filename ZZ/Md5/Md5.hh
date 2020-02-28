//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Md5.cc
//| Author(s)   : Mordechai T. Abzug
//| Module      : Md5
//| Description : Computes md5-checksum for a binary block of data.
//| 
//| Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All rights reserved.
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| Based on:
//| 
//|    MD5.H - header file for MD5C.C
//|    MDDRIVER.C - test driver for MD2, MD4 and MD5
//| 
//| License to copy and use this software is granted provided that it is identified as the "RSA
//| Data Security, Inc. MD5 Message-Digest Algorithm" in all material mentioning or referencing
//| this software or this function.
//| 
//| License is also granted to make and use derivative works provided that such works are
//| identified as "derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm" in all
//| material mentioning or referencing the derived work.
//| 
//| RSA Data Security, Inc. makes no representations concerning either the merchantability of this
//| software or the suitability of this software for any particular purpose. It is provided "as is"
//| without express or implied warranty of any kind.
//| 
//| These notices must be retained in any copies of any part of this documentation and/or software.
//|________________________________________________________________________________________________

#ifndef ZZ__Md5__md5_hh
#define ZZ__Md5__md5_hh
namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm


typedef Pair<uint64,uint64> md5_hash;   // -- Pair (least-significant-bits, most-significant-bits).


class MD5 {
    uint  state[4];
    uint  count[2];
    uchar buffer[64];
    uchar finalized;
    uchar result[16];   // -- read this after call to 'finalize()'.

    void init();
    void transform(uchar *buffer);

public:
    MD5() { init(); }

    void     update(uchar* data, uint len);
    void     update(Str data) { update((uchar*)data.base(), data.size()); }
    md5_hash finalize();
};


macro md5_hash md5(Str data) {
    MD5 m;
    m.update(data);
    return m.finalize();
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
