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

#include "Prelude.hh"
#include "Md5.hh"

namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Helpers:


// Encodes input (UINT4) into output (uchar). Assumes len is a multiple of 4.
static
void encode(uchar *output, uint *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4){
        output[j]   = (uchar)  (input[i] & 0xff);
        output[j+1] = (uchar) ((input[i] >> 8) & 0xff);
        output[j+2] = (uchar) ((input[i] >> 16) & 0xff);
        output[j+3] = (uchar) ((input[i] >> 24) & 0xff);
    }
}


// Decodes input (uchar) into output (UINT4). Assumes len is a multiple of 4.
static
void decode(uint *output, uchar *input, uint len)
{
    uint i, j;

    for (i = 0, j = 0; j < len; i++, j += 4)
        output[i] = ((uint)input[j]) | (((uint)input[j+1]) << 8) |
                    (((uint)input[j+2]) << 16) | (((uint)input[j+3]) << 24);
}


// Note: Replace "for loop" with standard memcpy if possible.
static
void memcpy(uchar *output, uchar *input, uint len){

  uint i;

  for (i = 0; i < len; i++)
    output[i] = input[i];
}



// Note: Replace "for loop" with standard memset if possible.
static
void memset(uchar *output, uchar value, uint len)
{
    uint i;

    for (i = 0; i < len; i++)
        output[i] = value;
}



// ROTATE_LEFT rotates x left n bits.
macro uint rotate_left(uint x, uint n) {
    return (x << n) | (x >> (32-n)); }


// F, G, H and I are basic MD5 functions.
macro uint F(uint x, uint y, uint z) {
    return (x & y) | (~x & z); }

macro uint G(uint x, uint y, uint z) {
    return (x & z) | (y & ~z); }

macro uint H(uint x, uint y, uint z) {
    return x ^ y ^ z; }

macro uint I(uint x, uint y, uint z) {
    return y ^ (x | ~z); }


// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
macro void FF(uint& a, uint b, uint c, uint d, uint x, uint  s, uint ac) {
    a += F(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

macro void GG(uint& a, uint b, uint c, uint d, uint x, uint s, uint ac) {
    a += G(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

macro void HH(uint& a, uint b, uint c, uint d, uint x, uint s, uint ac) {
    a += H(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}

macro void II(uint& a, uint b, uint c, uint d, uint x, uint s, uint ac) {
    a += I(b, c, d) + x + ac;
    a = rotate_left (a, s) +b;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// MD5 -- Public methods:


// MD5 block update operation. Continues an MD5 message-result operation, processing another
// message block, and updating the context.
//
void MD5::update(uchar *input, uint input_length)
{
    uint input_index, buffer_index;
    uint buffer_space;                // how much space is left in buffer

    assert(!finalized);

    // Compute number of bytes mod 64
    buffer_index = (uint)((count[0] >> 3) & 0x3F);

    // Update number of bits
    if (  (count[0] += ((uint) input_length << 3))<((uint) input_length << 3) )
        count[1]++;

    count[1] += ((uint)input_length >> 29);


    buffer_space = 64 - buffer_index;  // how much space is left in buffer

    // Transform as many times as possible.
    if (input_length >= buffer_space) { // ie. we have enough to fill the buffer
        // fill the rest of the buffer and transform
        memcpy (buffer + buffer_index, input, buffer_space);
        transform (buffer);

        // now, transform each 64-byte piece of the input, bypassing the buffer
        for (input_index = buffer_space; input_index + 63 < input_length;
         input_index += 64)
          transform (input+input_index);

        buffer_index = 0;  // so we can buffer remaining

    }else
        input_index=0;     // so we can buffer the whole input

    // and here we do the buffering:
    memcpy(buffer+buffer_index, input+input_index, input_length-input_index);
}


static uchar PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


// MD5 finalization. Ends an MD5 message-result operation, writing the
// the message result and zeroizing the context.
//
md5_hash MD5::finalize()
{
    uchar bits[8];
    uint index, padLen;

    assert(!finalized);

    // Save number of bits
    encode (bits, count, 8);

    // Pad out to 56 mod 64.
    index = (uint) ((count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    update (PADDING, padLen);

    // Append length (before padding)
    update (bits, 8);

    // Store state in result
    encode (result, state, 16);

    // Zeroize sensitive information
    memset (buffer, 0, sizeof(*buffer));

    finalized=1;

    // Return hash
    md5_hash h;
    h.fst = (uint64)result[15]
          | ((uint64)result[14] << 8)
          | ((uint64)result[13] << 16)
          | ((uint64)result[12] << 24)
          | ((uint64)result[11] << 32)
          | ((uint64)result[10] << 40)
          | ((uint64)result[9] << 48)
          | ((uint64)result[8] << 56);
    h.snd = (uint64)result[7]
          | ((uint64)result[6] << 8)
          | ((uint64)result[5] << 16)
          | ((uint64)result[4] << 24)
          | ((uint64)result[3] << 32)
          | ((uint64)result[2] << 40)
          | ((uint64)result[1] << 48)
          | ((uint64)result[0] << 56);

    return h;
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// MD5 -- Private methods:


void MD5::init()
{
    finalized=0;  // we just started!

    // Nothing counted, so count=0
    count[0] = 0;
    count[1] = 0;

    // Load magic initialization constants.
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
}



// Constants for MD5Transform routine.
// Although we could use C++ style constants, defines are actually better,
// since they let us easily evade scope clashes.

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21


// MD5 basic transformation. Transforms state based on block.
void MD5::transform(uchar block[64])
{
    uint a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    decode (x, block, 64);

    assert(!finalized);  // not just a user error, since the method is private

    /* Round 1 */
    FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
    FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
    FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
    FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
    FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
    FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
    FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
    FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
    FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

   /* Round 2 */
    GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
    GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
    II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
    II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
    II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // Zeroize sensitive information.
    memset ( (uchar *) x, 0, sizeof(x));
}





//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
