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
 * uuid.cpp: RFC 4122 UUID processing
 *
 * Chris Elving
 */

#include "pk11func.h"
#include "netsite.h"
#include "base/uuid.h"

/*
 * uuid_s describes an RFC 4122 UUID in network byte order (most significant
 * bytes first)
 */
struct uuid_s {
    unsigned char time_low[4];
    unsigned char time_mid[2];
    unsigned char time_hi_and_version[2];
    unsigned char clock_seq_hi_and_reserved[1];
    unsigned char clock_seq_low[1];
    unsigned char node[6];
};

/*
 * The 2 most significant bits of clock_seq_hi_and_reserved specify variant
 */
#define UUID_VARIANT_MASK    0xc0
#define UUID_VARIANT_RFC4122 0x80

/*
 * The 4 most significant bits of time_hi_and_version specify version
 */
#define UUID_VERSION_MASK 0xf0
#define UUID_VERSION_1    0x10
#define UUID_VERSION_2    0x20
#define UUID_VERSION_3    0x30
#define UUID_VERSION_4    0x40
#define UUID_VERSION_5    0x50

/*
 * HEXDIGIT returns the ASCII hexadecimal digit ('0' through '9' or 'a'
 * through 'f') for the passed nibble n
 */
#define HEXDIGIT(n) ((n) < 10 ? (n) + '0' : (n) + 'a' - 10)

/*
 * UNPARSE writes the ASCII hexadecimal digits for the passed unsigned char[]
 * field to out
 */
#define UNPARSE(field, out)                        \
        {                                          \
            int i;                                 \
            for (i = 0; i < sizeof(field); i++) {  \
                unsigned char hi = field[i] >> 4;  \
                unsigned char lo = field[i] & 0xf; \
                *out++ = HEXDIGIT(hi);             \
                *out++ = HEXDIGIT(lo);             \
            }                                      \
        }


/* ---------------------------- uuid_generate ----------------------------- */

void uuid_generate(uuid_t out)
{
    struct uuid_s *uuid = (struct uuid_s *) out;

    /* Fill the UUID with pseudorandom data */
    PK11_GenerateRandom(out, UUID_SIZE);

    /* Indicate this is an RFC 4122 version 4 UUID */
    uuid->clock_seq_hi_and_reserved[0] &= ~UUID_VARIANT_MASK;
    uuid->clock_seq_hi_and_reserved[0] |= UUID_VARIANT_RFC4122;
    uuid->time_hi_and_version[0] &= ~UUID_VERSION_MASK;
    uuid->time_hi_and_version[0] |= UUID_VERSION_4;
}


/* ----------------------------- uuid_unparse ----------------------------- */

void uuid_unparse(uuid_t uu, char *out)
{
    struct uuid_s *uuid = (struct uuid_s *) uu;

    UNPARSE(uuid->time_low, out);
    *out++ = '-';
    UNPARSE(uuid->time_mid, out);
    *out++ = '-';
    UNPARSE(uuid->time_hi_and_version, out);
    *out++ = '-';
    UNPARSE(uuid->clock_seq_hi_and_reserved, out);
    UNPARSE(uuid->clock_seq_low, out);
    *out++ = '-';
    UNPARSE(uuid->node, out);
    *out = '\0';
}
