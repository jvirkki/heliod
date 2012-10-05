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

// New Async DNS routines
// Julien Pierre

#include <nspr.h>
#include "arapi.h"
#include <memory.h>

#ifdef XP_UNIX
#include "arlib.h"

// a few globals
Resolver* resolver = NULL;

#else

void* resolver = NULL;

#endif

/*
 * PR_AR_Init
 *
 * Initialize and start a dedicated thread for receiving and processing
 * DNS request results.  The coresponding thread(s) which initiated the
 * DNS request will be awakened by this thread.
 */
 
void PR_AR_Init(PRIntervalTime timeout)
{
#ifdef XP_UNIX
    resolver = new Resolver(timeout);
    if (PR_TRUE != resolver->hasStarted())
    {
        delete(resolver);
        resolver = NULL;
    }
#endif
}

/*
 * PR_AR_GetHostByAddr
 *
 *  Similary to gethostbyaddr(), except that it makes a direct query
 *  to the DNS server in a non-blocking fashion.  It then waits for
 *  the response from the AR_worker thread via CondVar..
 *
 *
 *  Assumption:  Caller has previously made a call to PR_AR_Init().  If
 *        not, then we will use the synchronous version of gethostbyname.
 */
 
PR_IMPLEMENT(PRStatus) PR_AR_GetHostByAddr( const PRNetAddr *addrp,
    char *buf, PRIntn bufsize, PRHostEnt *hentp,  PRIntervalTime timeout )
{
    if ( (NULL == buf) || (0 == bufsize) || (NULL == hentp) )
    {
        return PR_FAILURE;
    }

    if (NULL == resolver)
    {
        return(PR_GetHostByAddr(addrp, buf, bufsize, hentp));
    }

#ifdef XP_UNIX
    if (timeout == PR_AR_DEFAULT_TIMEOUT)
        timeout = resolver->getDefaultTimeout();

    DNSSession newsession(addrp, timeout, buf, bufsize, hentp);
    if (PR_TRUE == resolver->process(&newsession))
    {
            return PR_SUCCESS;
    }
#endif

    return PR_FAILURE;
}

/*
 * PR_AR_GetHostByName
 *
 *  Similary to gethostbyname(), except that it makes a direct query
 *  to the DNS server in a non-blocking fashion.  It then waits for
 *  the response from the AR_worker thread via CondVar..
 *
 *
 *  Assumption:  Caller has previously made a call to PR_AR_Init().  If 
 *        not, then we will use the synchronous version of gethostbyname.
 */

PR_IMPLEMENT(PRStatus) PR_AR_GetHostByName( const char *name, char *buf,
    PRIntn bufsize, PRHostEnt *hentp, PRIntervalTime timeout, PRUint16 af )
{
    if ( (NULL == name) || (NULL == buf) || (0 == bufsize) || (NULL == hentp) )
    {
        return PR_FAILURE;
    }

    if (NULL == resolver)
    {
        return(PR_GetIPNodeByName(name, af, PR_AI_DEFAULT, buf, bufsize, hentp));
    }

#ifdef XP_UNIX    
    if (timeout == PR_AR_DEFAULT_TIMEOUT)
        timeout = resolver->getDefaultTimeout();

    DNSSession newsession(name, af, timeout, buf, bufsize, hentp);
    if (PR_TRUE == resolver->process(&newsession))
    {
            return PR_SUCCESS;
    }
#endif

    return PR_FAILURE;
}

PRBool GetAsyncDNSInfo(asyncDNSInfo *info)
{
    memset(info, 0, sizeof(asyncDNSInfo));
 
#ifdef XP_UNIX	
    info->enabled = (resolver != NULL);
    if (resolver)
    {
        info->numNameLookups = resolver->getNameLookups();
        info->numAddrLookups = resolver->getAddrLookups();
        info->numLookupsInProgress = resolver->getCurrentLookups();
        info->re_errors = resolver->GetInfo()->re_errors;
        info->re_nu_look = resolver->GetInfo()->re_nu_look;
        info->re_na_look = resolver->GetInfo()->re_na_look;
        info->re_replies = resolver->GetInfo()->re_replies;
        info->re_requests = resolver->GetInfo()->re_requests;
        info->re_resends = resolver->GetInfo()->re_resends;
        info->re_sent = resolver->GetInfo()->re_sent;
        info->re_timeouts = resolver->GetInfo()->re_timeouts;
    }
#endif

    return PR_TRUE;
}

