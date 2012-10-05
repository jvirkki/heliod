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
 * sslinit.c: Initialize SSL for a server
 * 
 * Rob McCool
 */


#include "pk11func.h"
#include "netsite.h"
#include "base/util.h"


NSAPI_PUBLIC void *nsapi_random_create(void)
{
    return (void*)0xDEADC0DE;
}

NSAPI_PUBLIC void nsapi_random_destroy(void *rctx)
{
}

NSAPI_PUBLIC
void nsapi_random_generate(void *rctx, unsigned char *outbuf, int length)
{
    INTutil_random(outbuf, length);
}

NSAPI_PUBLIC
void nsapi_random_update(void *rctx, unsigned char *inbuf, int length)
{
}

/* MD5 hash computation */

NSAPI_PUBLIC void *nsapi_md5hash_create(void)
{
    return (void *)PK11_CreateDigestContext(SEC_OID_MD5);
}

NSAPI_PUBLIC void *nsapi_md5hash_copy(void *hctx)
{
    return NULL;
}

NSAPI_PUBLIC void nsapi_md5hash_begin(void *hctx)
{
    PK11_DigestBegin((PK11Context*)hctx);
}

NSAPI_PUBLIC
void nsapi_md5hash_update(void *hctx, unsigned char *inbuf, int length)
{
    PK11Context* md5 = (PK11Context*)hctx;
    PK11_DigestOp(md5, inbuf, length);
}

NSAPI_PUBLIC void nsapi_md5hash_end(void *hctx, unsigned char *outbuf)
{
    unsigned int outlen;

    PK11Context* md5 = (PK11Context*)hctx;
    PK11_DigestFinal(md5, outbuf, &outlen, 16);
}

NSAPI_PUBLIC void nsapi_md5hash_destroy(void *hctx)
{
    PK11_DestroyContext((PK11Context*)hctx, PR_TRUE);
}

NSAPI_PUBLIC void
nsapi_md5hash_data(unsigned char *outbuf, unsigned char *inbuf, int length)
{
    PK11_HashBuf(SEC_OID_MD5, outbuf, inbuf, length);
}

/*
 * Function for 3.5 servers only Should return -1 on 4.0 servers.
 */
NSAPI_PUBLIC int
nsapi_rsa_set_priv_fn(void *func)
{
    PR_ASSERT(0);
    return (-1);
}
