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

#ifndef __ipfilter_h
#define __ipfilter_h

/* Define error codes */
#define IPFERR_MAX	(-1)	/* maximum error code value */
#define IPFERR_MALLOC	(-1)	/* insufficient memory */
#define IPFERR_FOPEN	(-2)	/* file open error */
#define IPFERR_FILEIO	(-3)	/* file I/O error */
#define IPFERR_DUPSPEC	(-4)	/* duplicate filter specification */
#define IPFERR_INTERR	(-5)	/* internal error (bug) */
#define IPFERR_SYNTAX	(-6)	/* syntax error in filter file */
#define IPFERR_CNFLICT	(-7)	/* conflicting filter specification */
#define IPFERR_MIN	(-7)	/* minimum error code value */

/* Define a scalar IP address value */
#ifndef __IPADDR_T_
#define __IPADDR_T_
typedef unsigned long IPAddr_t;
#endif /* __IPADDR_T_ */


/* Define structure for returning error information */
typedef struct IPFilterErr_s IPFilterErr_t;
struct IPFilterErr_s {
    int errNo;			/* IPFERR_xxxx error code */
    int lineno;			/* file line number, if applicable */
    char * filename;		/* filename, if applicable */
    char * errstr;		/* error text, if any */
};

/* Data and functions in ipfilter.c */
extern void * ipf_objndx;
NSAPI_PUBLIC extern void ip_filter_destroy(void * ipfptr);
NSAPI_PUBLIC 
extern int ip_filter_setup(pblock * client, IPFilterErr_t * reterr);
NSAPI_PUBLIC extern int ip_filter_check(pblock * client, IPAddr_t cip);

#endif /* __ipfilter_h */
