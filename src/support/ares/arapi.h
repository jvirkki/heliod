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
 * arapi.h 
 *
 * public API for async DNS, called by the daemon
 *
 */

#ifndef _ARAPI_H_
#define _ARAPI_H_

#include <nspr.h>

PR_BEGIN_EXTERN_C

#define PR_AR_MAXHOSTENTBUF 1024
#define PR_AR_DEFAULT_TIMEOUT	0

/* Error return codes */
#define PR_AR_OK	0

/* ----------- Function Prototypes ----------------*/

PR_EXTERN(void) 	PR_AR_Init(PRIntervalTime);

PR_EXTERN(PRStatus) 	PR_AR_GetHostByName(
	const char *,
        char *,
        PRIntn,
	PRHostEnt *,
        PRIntervalTime,
        PRUint16
);

PR_EXTERN(PRStatus) 	PR_AR_GetHostByAddr(
	const PRNetAddr *,
        char *,
        PRIntn,
        PRHostEnt *,
        PRIntervalTime
);

typedef struct asyncDNSInfo {
    PRBool   enabled;
    PRUint32 numNameLookups;
    PRUint32 numAddrLookups;
    PRUint32 numLookupsInProgress;
    PRUint32 re_errors;
    PRUint32 re_nu_look;
    PRUint32 re_na_look;
    PRUint32 re_replies;
    PRUint32 re_requests;
    PRUint32 re_resends;
    PRUint32 re_sent;
    PRUint32 re_timeouts;
} asyncDNSInfo;

PRBool GetAsyncDNSInfo(asyncDNSInfo *info);

PR_END_EXTERN_C

#endif /* _ARAPI_H_ */
