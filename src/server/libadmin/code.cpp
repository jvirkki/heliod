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
 * SHA-1 implementation
 *
 * Copyright © 1995 Netscape Communications Corporation, all rights reserved.
 *
 * $Id: code.cpp,v 1.1.2.1.72.1 2005/05/16 06:17:14 ik116642 Exp $
 *
 * Reference implementation of NIST FIPS PUB 180-1.
 *   (Secure Hash Standard, revised version).
 *   Copyright 1994 by Paul C. Kocher.  All rights reserved.
 *
 * Comments:  This implementation is written to be as close as
 *   possible to the NIST specification.  No performance or size
 *   optimization has been attempted.
 *
 * Disclaimer:  This code is provided without warranty of any kind.
 */

#include "stdlib.h"
#include "assert.h"
#include "string.h"
#include "httpcode.h"

struct SHA1ContextStr
{
   unsigned int h[5];
   unsigned int w[80];
   int lenW;
   unsigned int sizeHi, sizeLo;
};

typedef struct SHA1ContextStr SHA1Context;

#define SHS_ROTL(X,n) (((X) << (n)) | ((X) >> (32-(n))))


static void
shsHashBlock(SHA1Context * ctx)
{
   int t;
   unsigned int a, b, c, d, e, temp;

   for (t = 16; t <= 79; t++)
      ctx->w[t] = SHS_ROTL(
			   ctx->w[t - 3] ^ ctx->w[t - 8] ^ ctx->w[t - 14] ^ ctx->w[t - 16], 1);

   a = ctx->h[0];
   b = ctx->h[1];
   c = ctx->h[2];
   d = ctx->h[3];
   e = ctx->h[4];

   for (t = 0; t <= 19; t++)
   {
      temp = SHS_ROTL(a, 5) + (((c ^ d) & b) ^ d) + e + ctx->w[t] + 0x5a827999l;
      e = d;
      d = c;
      c = SHS_ROTL(b, 30);
      b = a;
      a = temp;
   }
   for (t = 20; t <= 39; t++)
   {
      temp = SHS_ROTL(a, 5) + (b ^ c ^ d) + e + ctx->w[t] + 0x6ed9eba1l;
      e = d;
      d = c;
      c = SHS_ROTL(b, 30);
      b = a;
      a = temp;
   }
   for (t = 40; t <= 59; t++)
   {
      temp = SHS_ROTL(a, 5) + ((b & c) | (d & (b | c))) + e + ctx->w[t] + 0x8f1bbcdcl;
      e = d;
      d = c;
      c = SHS_ROTL(b, 30);
      b = a;
      a = temp;
   }
   for (t = 60; t <= 79; t++)
   {
      temp = SHS_ROTL(a, 5) + (b ^ c ^ d) + e + ctx->w[t] + 0xca62c1d6l;
      e = d;
      d = c;
      c = SHS_ROTL(b, 30);
      b = a;
      a = temp;
   }

   ctx->h[0] += a;
   ctx->h[1] += b;
   ctx->h[2] += c;
   ctx->h[3] += d;
   ctx->h[4] += e;
}

static void
https_SHA1_Begin(SHA1Context * ctx)
{
   ctx->lenW = 0;
   ctx->sizeHi = ctx->sizeLo = 0;

   /*
    * initialize h with the magic constants (see fips180 for constants)
    */
   ctx->h[0] = 0x67452301l;
   ctx->h[1] = 0xefcdab89l;
   ctx->h[2] = 0x98badcfel;
   ctx->h[3] = 0x10325476l;
   ctx->h[4] = 0xc3d2e1f0l;
}


static SHA1Context *
https_SHA1_NewContext(void)
{
   SHA1Context *cx;

   cx = (SHA1Context *) malloc(sizeof(SHA1Context));
   return cx;
}

static SHA1Context *
https_SHA1_CloneContext(SHA1Context * cx)
{
   SHA1Context *clone = https_SHA1_NewContext();
   if (clone)
      *clone = *cx;
   return clone;
}

static void
https_SHA1_DestroyContext(SHA1Context * cx, int freeit)
{
   if (freeit)
   {
      free(cx);
   }
}

static void
https_SHA1_Update(SHA1Context * ctx, unsigned char *dataIn, unsigned len)
{
   int i;

   /*
    * Read the data into W and process blocks as they get full
    */
   for (i = 0; i < len; i++)
   {
      ctx->w[ctx->lenW / 4] <<= 8;
      ctx->w[ctx->lenW / 4] |= (unsigned int) dataIn[i];
      if ((++ctx->lenW) % 64 == 0)
      {
	 shsHashBlock(ctx);
	 ctx->lenW = 0;
      }
      ctx->sizeLo += 8;
      ctx->sizeHi += (ctx->sizeLo < 8);
   }
}


static void
https_SHA1_End(SHA1Context * ctx, unsigned char *hashout,
	 unsigned int *pDigestLen, unsigned int maxDigestLen)
{
   unsigned char pad0x80 = 0x80;
   unsigned char pad0x00 = 0x00;
   unsigned char padlen[8];
   int i;

   assert(maxDigestLen >= SHA1_LENGTH);

   /*
    * Pad with a binary 1 (e.g. 0x80), then zeroes, then length
    */
   padlen[0] = (unsigned char) ((ctx->sizeHi >> 24) & 255);
   padlen[1] = (unsigned char) ((ctx->sizeHi >> 16) & 255);
   padlen[2] = (unsigned char) ((ctx->sizeHi >> 8) & 255);
   padlen[3] = (unsigned char) ((ctx->sizeHi >> 0) & 255);
   padlen[4] = (unsigned char) ((ctx->sizeLo >> 24) & 255);
   padlen[5] = (unsigned char) ((ctx->sizeLo >> 16) & 255);
   padlen[6] = (unsigned char) ((ctx->sizeLo >> 8) & 255);
   padlen[7] = (unsigned char) ((ctx->sizeLo >> 0) & 255);

   https_SHA1_Update(ctx, &pad0x80, 1);
   while (ctx->lenW != 56)
      https_SHA1_Update(ctx, &pad0x00, 1);
   https_SHA1_Update(ctx, padlen, 8);

   /*
    * Output hash
    */
   for (i = 0; i < SHA1_LENGTH; i++)
   {
      hashout[i] = (unsigned char) (ctx->h[i / 4] >> 24);
      ctx->h[i / 4] <<= 8;
   }
   *pDigestLen = SHA1_LENGTH;
   https_SHA1_Begin(ctx);
}


static int
https_SHA1_HashBuf(unsigned char *dataOut, unsigned char *dataIn, unsigned int src_len)
{
   SHA1Context ctx;
   unsigned int outLen;

   if (dataOut == NULL || dataIn == NULL || src_len == 0)
   {
      return 1;
   }

   https_SHA1_Begin(&ctx);
   https_SHA1_Update(&ctx, dataIn, src_len);
   https_SHA1_End(&ctx, dataOut, &outLen, SHA1_LENGTH);

   return 0;
}

int
https_SHA1_Hash(unsigned char *dest, char *src)
{
   if (dest == NULL || src == NULL)
   {
      return 1;
   }

   return https_SHA1_HashBuf(dest, (unsigned char *) src, strlen(src));
}
