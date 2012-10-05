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

#include "frame/otype_helper.h"

static const pb_key* names[] = { pb_key_type,
                                 pb_key_enc,
                                 pb_key_lang,
                                 pb_key_charset };

static const char* headerNames[] = { "content-type",
                                     "content-encoding",
                                     "content-language",
                                     "magnus-charset" };

static const char* internalNames[] = { "NSInt_content-type",
                                       "NSInt_content-encoding",
                                       "NSInt_content-language",
                                       "NSInt_magnus-charset" };

static int numNames = sizeof(names) / sizeof(names[0]);

NSAPI_PUBLIC void
OtypeHelerSetDefaults(Request* rq, pblock* pb)
{
    PR_ASSERT(rq);
    PR_ASSERT(pb);

    for (int i = 0; i < numNames; i++) {
        char* val = pblock_findkeyval(names[i], pb);
        if (val) {
             /*********************************************************
             Note that since otypes functions are applied from the most
             specific object to the least specific, we let the most specific
             one set the default type in rq->vars
             *********************************************************/
            if (!pblock_findval(internalNames[i], rq->vars)) {
                pblock_nvinsert(internalNames[i], val, rq->vars);
                INTrequest_has_default_type(rq);
            }
        }
    }
}

NSAPI_PUBLIC void
OtypeHelperApplyDefaults(Request* rq)
{
    PR_ASSERT(rq);

    if (INTrequest_is_default_type_set(rq) == PR_TRUE) {
        for (int i = 0; i < numNames; i++) {
            if (pblock_findval(headerNames[i], rq->srvhdrs) == NULL) {
                char* defVal = pblock_findval(internalNames[i], rq->vars);
                if (defVal) {
                    pblock_nvinsert(headerNames[i], defVal, rq->srvhdrs);
                }
            }
        }
    }
}

