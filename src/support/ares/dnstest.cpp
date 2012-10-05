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

#include <nspr.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include "arapi.h"
#include <memory.h>

char *net_find_fqdn(PRHostEnt *p)
{
    int x;

    if((!p->h_name) || (!p->h_aliases))
        return NULL;

    if(!strchr(p->h_name, '.'))
    {
        for(x = 0; p->h_aliases[x]; ++x)
        {
            if((strchr(p->h_aliases[x], '.')) && 
               (!strncmp(p->h_aliases[x], p->h_name, strlen(p->h_name))))
            {
                return strdup(p->h_aliases[x]);
            }
        }
	    return NULL;
    } 
    else 
    {
        return strdup(p->h_name);
    }
}

char *dns_ip2host(char *ip, int verify)
{
    PRNetAddr iaddr;
    PRHostEnt* hptr = NULL;
    char* hn = NULL;
    unsigned long laddr = 0;
    char myhostname[256];
    PRHostEnt   hent;
    char        buf[PR_AR_MAXHOSTENTBUF];
    PRInt32     err = 0;

    if (PR_StringToNetAddr(ip, &iaddr) == PR_FAILURE)
        goto bong;

    /*
     * See if it happens to be the localhost IP address, and try
     * the local host name if so.
     */
    if (laddr == 0)
    {
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
                                &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK)
        {
            hptr = &hent;
        }
        else
        {
            hptr = 0;
        }
        /* Don't verify if we get a fully-qualified name this way */
        verify = 0;
    }
    else {
        if (PR_AR_GetHostByAddr(&iaddr, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT) == PR_AR_OK)
        {
            hptr = &hent;
        }
        else
        {
            hptr = 0;
        }
    }

    if ((!hptr) || !(hn = net_find_fqdn(hptr)))
        goto bong;

    if(verify)
    {
        char **haddr = 0;
        if (PR_AR_GetHostByName(hn, buf, PR_AR_MAXHOSTENTBUF,
                                &hent, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK)
        {
            hptr = &hent;
        }
        else
        {
            hptr = 0;
        }
        if(hptr)
        {
            for(haddr = hptr->h_addr_list; *haddr; haddr++)
            {
                if(((struct in_addr *)(*haddr))->s_addr == (unsigned long)iaddr.inet.ip)
                    break;
            }
        }
        if((!hptr) || (!(*haddr)))
            goto bong;
    }

    return hn;
  bong:
    return NULL;
}

const PRInt32 DNS_REPEATS = 512;

void test_reverse(void* arg)
{
    PRInt32 i;
    PRInt32 ok = 0;
    for (i=0;i<DNS_REPEATS;i++)
    {
        char* astring = dns_ip2host("205.217.237.53", 1);
        if (astring)
        {
            free(astring);
            ok++;
        }
    }
    printf("DNS test : %d out of %d\n", ok, i);
}

const PRInt32 THREAD_MAX = 50;

int main(int argc, char** argv)
{
    PR_AR_Init(PR_SecondsToInterval(12));

    //test_reverse(NULL);

    PRThread* threads[THREAD_MAX];
    PRInt32 i;
    for (i=0;i<THREAD_MAX;i++)
    {
        threads[i] = PR_CreateThread(PR_SYSTEM_THREAD,
            test_reverse,
            0,
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_JOINABLE_THREAD,
            0);

    }
    
    for (i=0;i<THREAD_MAX;i++)
    {
        PR_JoinThread(threads[i]);
    }
     return 0;
}
