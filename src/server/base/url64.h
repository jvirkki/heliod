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

#ifndef BASE_URL64_H
#define BASE_URL64_H

/*
 * url64.cpp: URL-friendly Base64-like encoding
 *
 * Chris Elving
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

/*
 * Encode the binary data, b, to an array of characters, c.  csize, the size
 * of the buffer pointed to by c, must be at least ceil[blen * 8 / 6].  The
 * return value is the number of characters written to c.  The character array
 * is not nul-terminated.
 *
 * XXX current implementation doesn't pad, so blen * 8 must equal clen * 6
 */
NSAPI_PUBLIC int INTurl64_encode(const void *b, int blen, char *c, int csize);

/*
 * Decode the array of characters, c, to a binary data buffer, b.  bsize, the
 * size of the buffer pointed to by b, must be at least ceil[clen * 6 / 8].
 * The return value is the number of bytes written to b.
 *
 * XXX current implementation doesn't pad, so blen * 8 must equal clen * 6
 */
NSAPI_PUBLIC int INTurl64_decode(const char *c, int clen, void *b, int bsize);

NSPR_END_EXTERN_C

#define url64_encode INTurl64_encode
#define url64_decode INTurl64_decode

#endif /* BASE_URL64_H */
