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
 * dnsdmain.c: DNS domain guessing stuff moved out of dns.c because of the
 * string ball problems
 */


#include "netsite.h"
#include "base/net.h"
#include <string.h>
#include <stdio.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <ctype.h>
#include "util.h"

/* Under NT, this is taken care of by net.h including winsock.h */
#ifdef XP_UNIX
#include <netdb.h>  /* struct hostent */
#endif

#ifdef NS_OLDES3X
extern "C" {
#include <libares/arapi.h>      /* For Asynchronous DNS lookup */
}
#else
#include "ares/arapi.h"
#endif /* NS_OLDES3X */

#include "frame/conf.h"



/* This is contained in resolv.h on most systems. */
#define _PATH_RESCONF "/etc/resolv.conf"

#ifdef XP_UNIX
NSPR_BEGIN_EXTERN_C
#ifdef HPUX
extern int gethostname (char *name, size_t namelen);
#else
#if !defined(AIX) && !defined(IRIX) && !defined(LINUX)
extern int gethostname (char *name, int namelen);
#endif
#endif
NSPR_END_EXTERN_C
#endif /* XP_UNIX */

#ifndef NO_DOMAINNAME

#if defined(SOLARIS)
#include <sys/systeminfo.h>
#else
#if !defined(LINUX)
NSPR_BEGIN_EXTERN_C
int getdomainname(char *name, int namelen);
NSPR_END_EXTERN_C
#endif
#endif /* SOLARIS */

int 
GetDomainName(char* name, int nameLen)
{
#ifdef SOLARIS
    long res = sysinfo(SI_SRPC_DOMAIN, name, nameLen);
    return (res < 0 ? res : 0);
#else
    return getdomainname(name, nameLen);
#endif
}
#endif /* NO_DOMAINNAME */



/* ---------------------------- dns_guess_domain -------------------------- */


extern "C" NSAPI_PUBLIC char *dns_guess_domain(char * hname)
{
    FILE *f;
    char * cp;
    int hnlen;
    char line[256];
    int dnlen = 0;
    char * domain = 0;
    PRHostEnt   hent;
    char        buf[PR_AR_MAXHOSTENTBUF];
    PRInt32     err;

    /* Sanity check */
    if (strchr(hname, '.')) {
	return STRDUP(hname);
    }

    if (dnlen == 0) {

	/* First try a little trick that seems to often work... */

	/*
	 * Get the local host name, even it doesn't come back
	 * fully qualified.
	 */
	line[0] = 0;
	gethostname(line, sizeof(line));
	if (line[0] != 0) {

	    /* Is it fully qualified? */
	    domain = strchr(line, '.');
	    if (domain == 0) {
#ifdef NS_OLDES3X
		struct hostent * hptr;
#else
                PRHostEnt * hptr;
#endif /* NS_OLDES3X */

		/* No, try gethostbyname() */
#ifdef NS_OLDES3X
       	hptr = (struct hostent*)PR_AR_GetHostByName(
                    line,
                    &hent,
                    buf,
                    PR_AR_MAXHOSTENTBUF,
                    &err,
                    PR_AR_DEFAULT_TIMEOUT
                    AF_INET);
#else
        if (PR_AR_GetHostByName(line, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK) {
            hptr = &hent;
        }
        else {
            hptr = 0;
        }
#endif /* NS_OLDES3X */
		if (hptr) {
		    /* See if h_name is fully-qualified */
		    if (hptr->h_name) {
			domain = strchr(hptr->h_name, '.');
		    }

		    /* Otherwise look for a fully qualified alias */
		    if ((domain == 0) &&
			(hptr->h_aliases && hptr->h_aliases[0])) {
			char **p;
			for (p = hptr->h_aliases; *p; ++p) {
			    domain = strchr(*p, '.');
			    if (domain) break;
			}
		    }
		}
	    }
	}

	/* Still no luck? */
	if (domain == 0) {

	    f = fopen(_PATH_RESCONF, "r");

	    /* See if there's a domain entry in their resolver configuration */
	    if(f) {
		while(fgets(line, sizeof(line), f)) {
		    if(!strncasecmp(line, "domain", 6) && isspace(line[6])) {
			for (cp = &line[7]; *cp && isspace(*cp); ++cp) ;
			if (*cp) {
			    domain = cp;
			    for (; *cp && !isspace(*cp); ++cp) ;
			    *cp = 0;
			}
			break;
		    }
		}
		fclose(f);
	    }
	}

#ifndef NO_DOMAINNAME
	if (domain == 0) {
	    /* No domain found. Try getdomainname. */
      line[0] = '\0';
	    GetDomainName(line, sizeof(line));
	    if (line[0] != 0) domain = &line[0];
	}
#endif

	if (domain != 0) {
	    if (domain[0] == '.') ++domain;
	    domain = STRDUP(domain);
	    dnlen = strlen(domain);
	}
	else dnlen = -1;
    }

    if (domain != 0) {
	hnlen = strlen(hname);
	if ((hnlen + dnlen + 2) <= sizeof(line)) {
	    strcpy(line, hname);
	    line[hnlen] = '.';
	    strcpy(&line[hnlen+1], domain);
	    return STRDUP(line);
	}
    }

    return 0;
}
