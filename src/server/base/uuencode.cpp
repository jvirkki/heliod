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

#include "base/util.h"


/* The magic set of 64 chars in the uuencoded data */
static const unsigned char uuset[] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
    'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

static const unsigned char pr2six[256] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63, 
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64, 
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64, 
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};


/* ---------------------------- util_uuencode ----------------------------- */

NSAPI_PUBLIC char *INTutil_uuencode(const char *src, int len)
{
    int i;

    /*
     * To uuencode, we snip 8 bits from 3 bytes and store them as 6 bits in 4
     * bytes.  6*4 == 8*3 (get it?) and 6 bits per byte yields nice clean
     * bytes.
     *
     * It goes like this:
     *
     *     AAAAAAAA BBBBBBBB CCCCCCCC
     *
     * turns into the standard set of uuencode ASCII chars indexed by numbers:
     *
     *     00AAAAAA 00AABBBB 00BBBBCC 00CCCCCC
     *
     * Snip-n-shift, snip-n-shift, etc....
     */
 
    char *dst = (char *)MALLOC(len * 4 / 3 + 4);
    if (!dst)
        return NULL;

    const unsigned char *in = (const unsigned char *)src;
    unsigned char *out = (unsigned char *)dst;

    for (i = 0; i < len; i += 3) {
        /* Do 3 bytes of input */
        unsigned char b0, b1, b2;
 
        b0 = in[0];
        if (i == len - 1) {
            b1 = b2 = '\0';
        } else if (i == len - 2) {
            b1 = in[1];
            b2 = '\0';
        } else {
            b1 = in[1];
            b2 = in[2];
        }
 
        /* Do 4 bytes of output */
        *out++ = uuset[b0 >> 2];
        *out++ = uuset[(((b0 & 0x03) << 4) | ((b1 & 0xf0) >> 4))];
        *out++ = uuset[(((b1 & 0x0f) << 2) | ((b2 & 0xc0) >> 6))];
        *out++ = uuset[b2 & 0x3f];

       in += 3;
    }

    *out = '\0'; /* terminate the string */
 
    /* Clean up the trailing bytes when len wasn't a multiple 3 */
    for (; i != len; i--)
        *--out = '=';
 
    return dst;
}

 
/* ---------------------------- util_uudecode ----------------------------- */

NSAPI_PUBLIC char *INTutil_uudecode(const char *src)
{
    /* Find the length */
    const unsigned char *end = (const unsigned char *)src;
    while (pr2six[*end++] <= 63);
    int nprbytes = (const char *)end - src - 1;
    int nbytesdecoded = ((nprbytes + 3) / 4) * 3;

    char *dst = (char *)MALLOC(nbytesdecoded + 1);
    if (!dst)
        return NULL;

    const unsigned char *in = (const unsigned char *)src;
    unsigned char *out = (unsigned char *)dst;
    while (nprbytes > 0) {
        *out++ = (pr2six[in[0]] << 2 | pr2six[in[1]] >> 4);
        *out++ = (pr2six[in[1]] << 4 | pr2six[in[2]] >> 2);
        *out++ = (pr2six[in[2]] << 6 | pr2six[in[3]]);
        in += 4;
        nprbytes -= 4;
    }
    
    if (nprbytes & 0x3) {
        if (pr2six[in[-2]] > 63) {
            nbytesdecoded -= 2;
        } else {
            nbytesdecoded -= 1;
        }
    }

    dst[nbytesdecoded] = '\0';

    return dst;
}
