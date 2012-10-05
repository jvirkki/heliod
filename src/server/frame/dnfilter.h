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

#ifndef __dnfilter_h
#define __dnfilter_h

/* Error codes */
#define DNFERR_MAX	(-1)	/* maximum error code */
#define DNFERR_MALLOC	(-1)	/* insufficient memory */
#define DNFERR_FOPEN	(-2)	/* file open error */
#define DNFERR_FILEIO	(-3)	/* file I/O error */
#define DNFERR_DUPSPEC	(-4)	/* duplicate filter specification */
#define DNFERR_INTERR	(-5)	/* internal error (bug) */
#define DNFERR_SYNTAX	(-6)	/* syntax error in filter file */
#define DNFERR_MIN	(-6)	/* minimum error code */

/* This is used to return error information from dns_filter_setup() */
typedef struct DNSFilterErr_s DNSFilterErr_t;
struct DNSFilterErr_s {
    int errNo;			/* DNFERR_xxxx error code */
    int lineno;			/* file line number, if applicable */
    char * filename;		/* filename, if applicable */
    char * errstr;		/* error text, if any */
};

NSPR_BEGIN_EXTERN_C

/* Data and functions in dnfilter.c */
NSAPI_PUBLIC extern void * dnf_objndx;
NSAPI_PUBLIC void dns_filter_destroy(void * dnfptr);
NSAPI_PUBLIC int dns_filter_setup(pblock * client, 
                                         DNSFilterErr_t * reterr);
NSAPI_PUBLIC int dns_filter_check(pblock * client, char * cdns);

NSPR_END_EXTERN_C

#endif /* __dnfilter_h */





