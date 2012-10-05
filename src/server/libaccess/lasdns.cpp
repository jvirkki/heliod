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

CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF NETSCAPE COMMUNICATIONS
CORPORATION

Copyright (c) 1996 Netscape Communications Corporation.  All Rights Reserved.

Use of this Source Code is subject to the terms of the applicable
license agreement from Netscape Communications Corporation.

The copyright notice(s) in this Source Code does not indicate actual or
intended publication of this Source Code.

*/


/*	lasdns.c
 *	This file contains the DNS LAS code.
 */

#include <stdio.h>
#include <string.h>

#ifdef	XP_WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <netsite.h>
#include <base/plist.h>
#include <base/pool.h>
#include <base/ereport.h>
#include <libaccess/nserror.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include <libaccess/nsauth.h>   // for ClAuth_t
#include "ares/arapi.h"

#ifdef	UTEST
extern int LASDnsGetDns(char **dnsv);
#endif

typedef	struct LASDnsContext {
	PRHashTable	*Table;	
	pool_handle_t   *pool;
} LASDnsContext_t;

/*    LASDnsMatch
 *    Given an array of fully-qualified dns names, tries to match them 
 *    against a given hash table.
 *    INPUT
 *    dns	DNS string	
 *    context	pointer to an LAS DNS context structure
 */
int
LASDnsMatch(char *token, LASDnsContext_t *context)
{

    /* Test for the unusual case where "*" is allowed */
    if (PR_HashTableLookup(context->Table, "*"))
        return LAS_EVAL_TRUE;

    /*  Start with the full name.  Then strip off each component
     *  leaving the remainder starting with a period.  E.g.
     *    splash.mcom.com
     *    .mcom.com
     *    .com
     *  Search the hash table for each remaining portion.  Remember that
     *  wildcards were put in with the leading period intact.
     */
    do {
	if (PR_HashTableLookup(context->Table, token))
            return LAS_EVAL_TRUE;

        token = strchr(&token[1], '.');
    } while (token != NULL);

    return LAS_EVAL_FALSE;

}

/*  LASDNSBuild
 *  Builds a hash table of all the hostnames provided (plus their aliases
 *  if aliasflg is true).  Wildcards are only permitted in the leftmost
 *  field.  They're represented in the hash table by a leading period.
 *  E.g. ".mcom.com".
 *
 *  RETURNS	Zero on success, else LAS_EVAL_INVALID
 */
int
LASDnsBuild(NSErr_t *errp, char *attr_pattern, LASDnsContext_t *context, int aliasflg)
{
    int		delimiter;    	/* length of valid token	*/
    char	token[256];    	/* max length dns name		*/
    int		i;
    int		ipcnt = 0;
    char	**p;
    unsigned long	*ipaddrs=0;
    pool_handle_t *pool;
    PRInt32	error=0;
    char	buffer[PR_AR_MAXHOSTENTBUF];
#ifdef	UTEST
    struct hostent *he, host;
#else
    PRHostEnt *he, host;
#endif

    context->Table = PR_NewHashTable(0,
				     ACLPR_HashCaseString,
				     ACLPR_CompareCaseStrings,
				     PR_CompareValues,
				     &ACLPermAllocOps,
				     NULL);
    pool = pool_create();
    context->pool = pool;
    if ((!context->Table) || (pool_enabled() && !context->pool)) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR4700, ACL_Program, 1, XP_GetAdminStr(DBT_lasdnsbuildUnableToAllocateHashT_));
        return LAS_EVAL_INVALID;
    }

    do {
	/*  Get a single hostname from the pattern string	*/
        delimiter    = strcspn(attr_pattern, ", \t");
        strncpy(token, attr_pattern, delimiter);
        token[delimiter] = '\0';

        /*  Skip any white space after the token 		*/
        attr_pattern     += delimiter;
        attr_pattern    += strspn(attr_pattern, ", \t");

        /*  If there's a wildcard, strip it off but leave the "."
	 *  Can't have aliases for a wildcard pattern.
	 *  Treat "*" as a special case.  If so, go ahead and hash it.
	 */
        if (token[0] == '*') {
	    if (token[1] != '\0') {
	        if (!PR_HashTableAdd(context->Table, pool_strdup(pool, &token[1]), (void *)-1)) {
		    nserrGenerate(errp, ACLERRFAIL, ACLERR4710, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), token);
	            return LAS_EVAL_INVALID;
	        }
	    } else {
	        if (!PR_HashTableAdd(context->Table, pool_strdup(pool, token), (void *)-1)) {
		    nserrGenerate(errp, ACLERRFAIL, ACLERR4720, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), token);
	            return LAS_EVAL_INVALID;
	        }
	    }
	} else  {
            /*  This is a single hostname. Add it to the hash table	*/
	    if (!PR_HashTableAdd(context->Table, pool_strdup(pool, &token[0]), (void *)-1)) {
		nserrGenerate(errp, ACLERRFAIL, ACLERR4730, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), token);
	        return LAS_EVAL_INVALID;
	    }

            if (aliasflg) {
#ifdef	UTEST
                he = gethostbyname(token);
#else
                he = (PR_AR_GetHostByName(token, buffer, PR_AR_MAXHOSTENTBUF,
                                    &host, PR_AR_DEFAULT_TIMEOUT, AF_INET) == PR_AR_OK) ? &host : NULL;
#endif
                if (he) {
                    /* Make a copy of the list of IP addresses if any */
                    if (he->h_addr_list && he->h_addr_list[0]) {

                        /* Count the IP addresses */
                        for (p = he->h_addr_list, ipcnt = 0; *p; ++p) {
                            ++ipcnt;
                        }
             
                        /* Allocate space */
                        ipaddrs = (unsigned long *)PERM_MALLOC(ipcnt * sizeof(unsigned long));

                        /* Copy IP addresses */
                        for (i = 0; i < ipcnt; ++i) {
                            ipaddrs[i] = 0;
                            memcpy((void *)&ipaddrs[i], he->h_addr_list[i], 4);
                        }
                    }
             
                    /* Add each of the aliases to the list */
                    if (he->h_aliases && he->h_aliases[0]) {
             
                        for (p = he->h_aliases; *p; ++p) {
                            /*  Add it to the hash table			*/
                            if (!PR_HashTableAdd(context->Table, pool_strdup(pool, *p), (void *)-1)) {
                                nserrGenerate(errp, ACLERRFAIL, ACLERR4740, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), *p);
                                PERM_FREE(ipaddrs);
                                return LAS_EVAL_INVALID;
                            }
                        }
                    }
                 
                    for (i = 0; i < ipcnt; ++i) {

#ifdef UTEST
                        he = gethostbyaddr((char *)&ipaddrs[i], 4, AF_INET);
#else
                        {
                            PRNetAddr naddr;
                            naddr.inet.family = AF_INET;
                            naddr.inet.port = 0;
                            memcpy((void *)&naddr.inet.ip, (void *)&ipaddrs[i], 4);
                            he = (PR_AR_GetHostByAddr(&naddr, buffer, PR_AR_MAXHOSTENTBUF,
                                                    &host, PR_AR_DEFAULT_TIMEOUT) == PR_AR_OK) ? &host : NULL;
                        }
#endif
                        if (he == NULL)
                            continue;

                        if (he->h_name) {
                            /*  Add it to the hash table			*/
                            if (!PR_HashTableAdd(context->Table, pool_strdup(pool, he->h_name), (void *)-1)) {
                                nserrGenerate(errp, ACLERRFAIL, ACLERR4750, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), he->h_name);
                                PERM_FREE(ipaddrs);
                                return LAS_EVAL_INVALID;
                            }
                        }
         
                        if (he->h_aliases && he->h_aliases[0]) {
                            for (p = he->h_aliases; *p; ++p) {
                                /*  Add it to the hash table			*/
                                if (!PR_HashTableAdd(context->Table, pool_strdup(pool, *p), (void *)-1)) {
                                    nserrGenerate(errp, ACLERRFAIL, ACLERR4760, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsbuildUnableToAddKeySN_), *p);
                                    PERM_FREE(ipaddrs);
                                    return LAS_EVAL_INVALID;
                                }
                            }
                        }
                    }
                    PERM_FREE(ipaddrs);
                }	/* if he */
	    }	/* if aliasflg */
	}	/* else - single hostname */
    } while ((attr_pattern != NULL) && (attr_pattern[0] != '\0') && (delimiter != (int)NULL));

    return 0;
}

/*  LASDnsFlush
 *  Given the address of a las_cookie for a DNS expression entry, frees up 
 *  all allocated memory for it.  This includes the hash table, plus the
 *  context structure.
 */
void
LASDnsFlush(void **las_cookie)
{
    if (*las_cookie == NULL)
        return;

    pool_destroy(((LASDnsContext_t *)*las_cookie)->pool);
    PR_HashTableDestroy(((LASDnsContext_t *)*las_cookie)->Table);
    PERM_FREE(*las_cookie);
    *las_cookie = NULL;
    return;
}

/*
 *	LASDnsEval
 *	INPUT
 *	attr_name	The string "dns" - in lower case.
 *	comparator	CMP_OP_EQ or CMP_OP_NE only
 *	attr_pattern	A comma-separated list of DNS names
 *			Any segment(s) in a DNS name can be wildcarded using
 *			"*".  Note that this is not a true Regular Expression
 *			form.
 *	*cachable	Always set to ACL_INDEF_CACHE
 *      subject		Subject property list
 *      resource 	Resource property list
 *      auth_info	Authentication info, if any
 *	RETURNS
 *	ret code	The usual LAS return codes.
 */
int LASDnsEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator,
           char *attr_pattern, ACLCachable_t *cachable, void **LAS_cookie,
           PList_t subject, PList_t resource,
           PList_t auth_info, PList_t global_auth)
{
    int			result;
    int			aliasflg;
    char		*my_dns;
    LASDnsContext_t 	*context;
    int			rv;

    *cachable = ACL_INDEF_CACHABLE;

    if (strcmp(attr_name, "dns") == 0) 
	aliasflg = 0;
    else if (strcmp(attr_name, "dnsalias") == 0)
	aliasflg = 1;
    else {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4800, ACL_Program, 2, XP_GetAdminStr(DBT_lasDnsBuildReceivedRequestForAtt_), attr_name);
    	return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR4810, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsevalIllegalComparatorDN_), comparator_string(comparator));
    	return LAS_EVAL_INVALID;
    }

    /* If this is the first time through, build the pattern tree first.  */
    if (*LAS_cookie == NULL) {
	ACL_CritEnter();
        context = (LASDnsContext *) *LAS_cookie;
        if (*LAS_cookie == NULL) {	/* Must check again */
            *LAS_cookie = context = 
                (LASDnsContext_t *)PERM_MALLOC(sizeof(LASDnsContext_t));
            if (context == NULL) {
		nserrGenerate(errp, ACLERRNOMEM, ACLERR4820, ACL_Program, 1, XP_GetAdminStr(DBT_lasdnsevalUnableToAllocateContex_));
		ACL_CritExit();
                return LAS_EVAL_FAIL;
            }
    	    context->Table = NULL;
    	    LASDnsBuild(errp, attr_pattern, context, aliasflg);
	}
	ACL_CritExit();
    } else
        context = (LASDnsContext *) *LAS_cookie;

    /* Call the DNS attribute getter */
#ifdef  UTEST
    LASDnsGetDns(&my_dns);      /* gets stuffed on return       */
#else
    rv = ACL_GetAttribute(errp, ACL_ATTR_DNS, (void **)&my_dns,
			  subject, resource, auth_info, global_auth);

    if (rv != LAS_EVAL_TRUE) {
        if (subject || resource) {	
	    char rv_str[16];
            /* Don't ereport if called from ACL_CachableAclList */
	    sprintf(rv_str, "%d", rv);
	    nserrGenerate(errp, ACLERRINVAL, ACLERR4830, ACL_Program, 2, XP_GetAdminStr(DBT_lasdnsevalUnableToGetDnsErrorDN_), rv_str);
        }
	return LAS_EVAL_FAIL;
    }
#endif

    result = LASDnsMatch(my_dns, context);

    if (comparator == CMP_OP_NE) {
    	if (result == LAS_EVAL_FALSE)
            result = LAS_EVAL_TRUE;
        else if (result == LAS_EVAL_TRUE)
            result = LAS_EVAL_FALSE;
    }

    ereport(LOG_VERBOSE, "acl dns: %s on %s %s (%s)",
            (result == LAS_EVAL_TRUE) ? "match" : "no match",
            aliasflg ? "dnsalias" : "dns",
            (comparator == CMP_OP_EQ) ? "=" : "!=",
            attr_pattern);

    return (result);
}

int 
LASDnsGetter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
           auth_info, PList_t global_auth, void *arg)
{
    Session *sn=NULL;
    char *my_dns=NULL;
    int rv;
    struct in_addr in;

    if (dns_enabled() == PR_FALSE) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameLASDnsGetter1));
        return LAS_EVAL_FAIL;
    }
      
    if (PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL) < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_dnsLasUnableToGetSessionAddr));
        return LAS_EVAL_FAIL;
    }
    my_dns = ((ClAuth_t *)sn->clauth)->cla_dns;
    if (!my_dns) {
        my_dns = session_dns_lookup(sn, 0);
        if (!my_dns || !*my_dns) {
            my_dns = "unknown";
        }
    }

    rv = PListInitProp(subject, ACL_ATTR_DNS_INDEX, ACL_ATTR_DNS, my_dns, NULL);
    if (rv < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameLASDnsGetter2), rv);
        return LAS_EVAL_FAIL;
    }

    return LAS_EVAL_TRUE;
}

