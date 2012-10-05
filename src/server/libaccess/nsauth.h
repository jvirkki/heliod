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

#ifndef __nsauth_h
#define __nsauth_h

/*
 * Description (nsauth.h)
 *
 *	This file defines types and interfaces which pertain to client
 *	authentication.  The key types are Realm_t, which describes a
 *	context for authentication, and ClAuth_t, which is used to
 *	pass authentication information about a particular client
 *	into and out of authentication interface functions.
 *
 * All the v2 ACL stuff was removed for iWS5.0. We still need ClAuth_t, though,
 * otherwise, the whole file would be gone.
 */

#include "ssl.h"
#include "cert.h"               /* CERTCertificate */

/* Define a scalar IP address value */
#ifndef __IPADDR_T_
#define __IPADDR_T_
typedef unsigned long IPAddr_t;
#endif /* __IPADDR_T_ */

/*
 * Description (ClAuth_t)
 *
 *	This type describes a structure containing information about a
 *	particular client.  It is used to pass information into and out
 *	of authentication support functions, as well as to other functions
 *	needing access to client authentication information.
 * FUTURE:
 *	- add client certificate pointer
 */

typedef struct ClAuth_s ClAuth_t;
struct ClAuth_s {
    void * cla_realm;   	/* authentication realm pointer ; OBSOLETE */
    IPAddr_t cla_ipaddr;	/* IP address */
    char * cla_dns;		/* DNS name string pointer; OBSOLETE */
    void * cla_uoptr;   	/* authenticated user object pointer; OBSOLETE */
    void * cla_goptr;   	/* pointer to list of group objects; OBSOLETE */
    CERTCertificate * cla_cert; /* certificate from SSL client auth */
};

#endif /* __nsauth_h */
