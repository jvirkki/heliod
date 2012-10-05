/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * url64.cpp: URL-friendly Base64-like encoding
 *
 * Chris Elving
 */


#include "base/url64.h"


static const char _url64_encode_table[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','-'
};

static const int _url64_decode_table[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,63,64,64,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};


/* ----------------------------- url64_encode ----------------------------- */

int url64_encode(const void *b, int blen, char *c, int csize)
{
    int clen = blen * 8 / 6;

    // XXX current implementation doesn't do padding
    if (clen * 6 != blen * 8) {
        PR_ASSERT(0);
        return -1;
    }

    if (clen > csize) {
        PR_ASSERT(0);
        return -1;
    }

    const unsigned char *bytes = (const unsigned char *)b;
    for (int i = 0; i < clen; bytes += 3) {
        c[i++] = _url64_encode_table[(bytes[0] & 0xfc) >> 2];
        c[i++] = _url64_encode_table[((bytes[0] & 0x03) << 4) | ((bytes[1] & 0xf0) >> 4)];
        c[i++] = _url64_encode_table[((bytes[1] & 0x0f) << 2) | ((bytes[2] & 0xc0) >> 6)];
        c[i++] = _url64_encode_table[bytes[2] & 0x3f];
    }

    return clen;
}


/* ----------------------------- url64_decode ----------------------------- */

int url64_decode(const char *c, int clen, void *b, int bsize)
{
    int blen = clen * 6 / 8;

    // XXX current implementation doesn't do padding
    if (blen * 8 != clen * 6) {
        PR_ASSERT(0);
        return -1;
    }

    if (blen > bsize) {
        PR_ASSERT(0);
        return -1;
    }

    unsigned char *bytes = (unsigned char *)b;
    for (int i = 0; i < blen; c += 4) {
        register unsigned c1 = _url64_decode_table[c[1]];
        register unsigned c2 = _url64_decode_table[c[2]];
        bytes[i++] = (_url64_decode_table[c[0]] << 2) | (c1 >> 4);
        bytes[i++] = (c1 << 4) | (c2 >> 2);
        bytes[i++] = (c2 << 6) | _url64_decode_table[c[3]];
    }

    return blen;
}
