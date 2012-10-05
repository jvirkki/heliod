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

#ifndef BASE_ARCFOUR_H
#define BASE_ARCFOUR_H

/*
 * arcfour.h: Simple software arcfour implementation
 *
 * Chris Elving
 */

#include "netsite.h"

NSPR_BEGIN_EXTERN_C

#define ARCFOUR_S_SIZE 256

typedef struct {
    unsigned char S[ARCFOUR_S_SIZE];
    unsigned char i;
    unsigned char j;
} Arcfour;

/*
 * Initialize arcfour state using sz bytes from buf as the key.
 */
void INTarcfour_init(Arcfour *arcfour, const void *buf, size_t sz);

/*
 * Discard n bytes of arcfour output.
 */
void INTarcfour_discard(Arcfour *arcfour, int n);

/*
 * Retrieve sz bytes of arcfour output.
 */
void INTarcfour_output(Arcfour *arcfour, void *buf, int sz);

NSPR_END_EXTERN_C

#define arcfour_init INTarcfour_init
#define arcfour_discard INTarcfour_discard
#define arcfour_output INTarcfour_output

#endif /* BASE_ARCFOUR_H */
