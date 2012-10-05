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
 * dns.c: DNS resolution routines
 * 
 * Rob McCool
 */
#define DNS_GUESSING

#include "netsite.h"
#ifdef XP_UNIX
#include "systems.h"
#else /* XP_WIN32 */
#include "base/systems.h"
#endif /* XP_WIN32 */

#include "net.h"    /* SYS_NETFD, various headers to do DNS */

/* Under NT, these are taken care of by net.h including winsock.h */
#ifdef XP_UNIX
#include <arpa/inet.h>  /* inet_ntoa */
#include <netdb.h>  /* struct hostent */
#ifdef NEED_GHN_PROTO
extern "C" int gethostname (char *name, size_t namelen);
#endif
#endif
#ifdef DNS_CACHE
#include "base/dns_cache.h"
#include "base/ereport.h"
#endif /* DNS_CACHE */
#include <stdio.h>

#include "ares/arapi.h"

#include "frame/conf.h"


/* ---------------------------- dns_find_fqdn ----------------------------- */


/* defined in dnsdmain.c */
extern "C"  NSAPI_PUBLIC char *dns_guess_domain(char * hname);

char *net_find_fqdn(PRHostEnt *p)
{
    int x;

    if((!p->h_name) || (!p->h_aliases))
        return NULL;

    if(!strchr(p->h_name, '.')) {
        for(x = 0; p->h_aliases[x]; ++x) {
            if((strchr(p->h_aliases[x], '.')) && 
               (!strncmp(p->h_aliases[x], p->h_name, strlen(p->h_name))))
            {
                return STRDUP(p->h_aliases[x]);
            }
        }
#ifdef DNS_GUESSING
	return dns_guess_domain(p->h_name);
#else
	return NULL;
#endif /* DNS_GUESSING */
    } 
    else 
        return STRDUP(p->h_name);
}


/* ----------------------------- dns_ip2host ------------------------------ */


char *dns_ip2host(const char *ip, int verify)
{
    PRNetAddr iaddr;
    PRHostEnt *hptr;
    char *hn;
#ifdef DNS_CACHE
    dns_cache_entry_t *dns_entry;
#endif
    unsigned long laddr = 0;
    char myhostname[256];
    PRHostEnt   hent;
    char        buf[PR_AR_MAXHOSTENTBUF];
    PRInt32     err;

    if (PR_StringToNetAddr(ip, &iaddr) == PR_FAILURE)
        goto bong;

#ifdef DNS_CACHE
    if ( (dns_entry = dns_cache_lookup_ip((unsigned int)iaddr.inet.ip)) )
    {
        hn = NULL;
        if ( dns_entry->host && 
             /* Only use entry if the cache entry has been verified or if 
              * verify is off...
              */
             (dns_entry->verified || !verify) ) {
            hn = STRDUP( dns_entry->host );
	    (void)dns_cache_use_decrement(dns_entry);
	    return hn;
        }
	dns_cache_delete(dns_entry);
	dns_entry = 0;
    }
#endif

    /*
     * See if it happens to be the localhost IP address, and try
     * the local host name if so.
     */
    if (laddr == 0) {
	laddr = inet_addr("127.0.0.1");
	myhostname[0] = 0;
	gethostname(myhostname, sizeof(myhostname));
    }

    /* Have to match the localhost IP address and have a hostname */

    if (((unsigned long)iaddr.inet.ip == laddr) && (myhostname[0] != 0))
    {

        /*
         * Now try for a fully-qualified domain name, starting with
         * the local hostname.
         */
        if (PR_AR_GetHostByName(myhostname, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK) {
            hptr = &hent;
        }
        else {
            hptr = 0;
        }
        /* Don't verify if we get a fully-qualified name this way */
        verify = 0;
    }
    else {
        if (PR_AR_GetHostByAddr(&iaddr, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT) == PR_AR_OK) {
            hptr = &hent;
        }
        else {
            hptr = 0;
        }
    }

    if ((!hptr) || !(hn = net_find_fqdn(hptr))) goto bong;

    if(verify) {
        char **haddr = 0;
        if (PR_AR_GetHostByName(hn, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK) {
            hptr = &hent;
        }
        else {
            hptr = 0;
        }
        if(hptr) {
            for(haddr = hptr->h_addr_list; *haddr; haddr++) {
                if(((struct in_addr *)(*haddr))->s_addr == (unsigned long)iaddr.inet.ip)
                    break;
            }
        }
        if((!hptr) || (!(*haddr)))
            goto bong;
    }

#ifdef DNS_CACHE
    if ( (dns_entry = dns_cache_insert(hn, (unsigned int)iaddr.inet.ip, verify)) )
    {
        (void) dns_cache_use_decrement(dns_entry);
    } 
#endif /* DNS_CACHE */
    return hn;
  bong:
#ifdef DNS_CACHE
    /* Insert the lookup failure */
    if ( (dns_entry = dns_cache_insert(NULL, (unsigned int)iaddr.inet.ip, verify)) )
    {
        (void) dns_cache_use_decrement(dns_entry);
    } 
#endif /* DNS_CACHE */
    return NULL;
}
