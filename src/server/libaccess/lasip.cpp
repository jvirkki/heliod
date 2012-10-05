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


/*    aclip.c
 *    This file contains the IP LAS code.
 */

#include <stdio.h>
#include <string.h>
#ifndef UTEST
#include <netsite.h>
#include <base/plist.h>
#include <base/ereport.h>
#include <libaccess/nserror.h>
#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclerror.h>
#include <libaccess/nsauth.h>
#include <frame/http_ext.h> // GetHrq
#include <httpdaemon/daemonsession.h> // DaemonSession
#include <httpdaemon/httprequest.h> // DaemonSession
#include <base/util.h>
#else
#include "utest.h"
#endif

#define DOUBLE_COLON_FFFF_COLON     "::FFFF:"
#define DOUBLE_COLON_FFFF_COLON_LEN 7
#define DOUBLE_COLON      "::"
#define DOUBLE_COLON_LEN  2
#define ALL_255_IPV4      "255.255.255.255"
#define ALL_F_IPV6        "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"

#define        LAS_IP_IS_CONSTANT(x)    (((x) == (LASIpTree_t *)LAS_EVAL_TRUE) || ((x) == (LASIpTree_t *)LAS_EVAL_FALSE))

#ifdef	UTEST
extern PRNetAddr *LASIpGetIpv6();
#endif

typedef struct LASIpTree {
	struct LASIpTree	*action[2];
} LASIpTree_t;

typedef	struct LASIpContext {
	LASIpTree_t	*treetop; /* Top of the pattern tree	*/
	int	        ipVersion; /* PR_AF_INET6 or PR_AF_INET */
} LASIpContext_t;

#define VALID_CHARS_IN_PATTERN "1234567890abcdefABCDEF:.*+"
#define VALID_CHARS_IN_IP      "1234567890abcdefABCDEF:.*"
#define SUM_PR_AF_INET_AND_INET6 PR_AF_INET6+PR_AF_INET
static int
LASIpAddPattern(NSErr_t *errp, PRNetAddr netmask, PRNetAddr pattern, LASIpTree_t **treetop, int ipVersion);
static PRUint8 getBit(PRNetAddr addr, int pos, int max_bit);
static void setBit(PRNetAddr *addr, int pos, int max_bit);
static int traverseTreeAndCompareIPs(NSErr_t *errp, PRNetAddr *ip_addr,
                                     LASIpContext_t *context, char *ip,
                                     int ipVersion, bool comparator_is_equal);
int parseIp(char *iIP, char* iNetmask, PRNetAddr *pIP, PRNetAddr *pNetmask);

/*    dotdecimal
 *    Takes netmask and ip strings and returns the numeric values,
 *    accounting for wildards in the ip specification.  Wildcards in the
 *    ip override the netmask where they conflict.
 *    INPUT
 *    ipstr        e.g. "123.45.67.89"
 *    netmaskstr    e.g. "255.255.255.0"
 *    RETURNS
 *    *ip        
 *    *netmask    e.g. 0xffffff00
 *    result        NULL on success or else one of the LAS_EVAL_* codes.
 */
int
dotdecimal(char *ipstr, char *netmaskstr, int *ip, int *netmask)
{
    int     i;
    char    token[64];
    char    *dotptr;    /* location of the "."        */
    int     dotidx;     /* index of the period char    */

    /* Sanity check the patterns */

    /* Netmask can only have digits and periods. */
    if (strcspn(netmaskstr, "0123456789."))
        return LAS_EVAL_INVALID;

    /* IP can only have digits, periods and "*" */
    if (strcspn(ipstr, "0123456789.*"))
        return LAS_EVAL_INVALID;

    *netmask = *ip = 0;    /* Start with "don't care"    */

    for (i=0; i<4; i++) {
        dotptr    = strchr(netmaskstr, '.');

        /* copy out the token, then point beyond it */
        if (dotptr == NULL)
            strcpy(token, netmaskstr);
        else {
            dotidx    = dotptr-netmaskstr;
            strncpy(token, netmaskstr, dotidx);
            token[dotidx] = '\0';
            netmaskstr = ++dotptr;    /* skip the period */
        }

        /* Turn into a number and shift left as appropriate */
        *netmask    += (atoi(token))<<(8*(4-i-1));

        if (dotptr == NULL)
            break;
    }

    for (i=0; i<4; i++) {
        dotptr    = strchr(ipstr, '.');

        /* copy out the token, then point beyond it */
        if (dotptr == NULL)
            strcpy(token, ipstr);
        else {
            dotidx    = dotptr-ipstr;
            strncpy(token, ipstr, dotidx);
            token[dotidx] = '\0';
            ipstr    = ++dotptr;
        }

        /* check for wildcard    */
        if (strcmp(token, "*") == 0) {
            switch(i) {
            case 0:
                if (dotptr == NULL)
                    *netmask &= 0x00000000;
                else
                    *netmask &= 0x00ffffff;
                break;
            case 1:
                if (dotptr == NULL)
                    *netmask &= 0xff000000;
                else
                    *netmask &= 0xff00ffff;
                break;
            case 2:
                if (dotptr == NULL)
                    *netmask &= 0xffff0000;
                else
                    *netmask &= 0xffff00ff;
                break;
            case 3:
                *netmask &= 0xffffff00;
                break;
            }
            continue;
        } else {
            /* Turn into a number and shift left as appropriate */
            *ip    += (atoi(token))<<(8*(4-i-1));
        }

        /* check for end of string    */
        if (dotptr == NULL) {
            switch(i) {
            case 0:
                *netmask &= 0xff000000;
                break;
            case 1:
                *netmask &= 0xffff0000;
                break;
            case 2:
                *netmask &= 0xffffff00;
                break;
            }
            break;
        }
    }

    return (int)NULL;
}


/*    LASIpTreeAlloc
 *    Malloc a node and set the actions to LAS_EVAL_FALSE
 */
static LASIpTree_t *
LASIpTreeAllocNode(NSErr_t *errp)
{
    LASIpTree_t    *newnode;

    newnode = (LASIpTree_t *)PERM_MALLOC(sizeof(LASIpTree_t));
    if (newnode == NULL) {
	nserrGenerate(errp, ACLERRNOMEM, ACLERR5000, ACL_Program, 1, XP_GetAdminStr(DBT_lasiptreeallocNoMemoryN_));
        return NULL;
    }
    newnode->action[0] = (LASIpTree_t *)LAS_EVAL_FALSE;
    newnode->action[1] = (LASIpTree_t *)LAS_EVAL_FALSE;
    return newnode;
}


/*    LASIpTreeDealloc
 *    Deallocates a Tree starting from a given node down.
 *    INPUT
 *    startnode    Starting node to remove.  Could be a constant in 
 *            which case, just return success.
 *    OUTPUT
 *    N/A
 */
static void
LASIpTreeDealloc(LASIpTree_t *startnode)
{
    int    i;

    if (startnode == NULL)
        return;

    /* If this is just a constant then we're done             */
    if (LAS_IP_IS_CONSTANT(startnode))
        return;

    /* Else recursively call ourself for each branch down        */
    for (i=0; i<2; i++) {
        if (!(LAS_IP_IS_CONSTANT(startnode->action[i])))
            LASIpTreeDealloc(startnode->action[i]);
    }

    /* Now deallocate the local node                */
    PERM_FREE(startnode);
}

/*
 *    LASIpBuild
 *    INPUT
 *    attr_pattern    A comma-separated list of IP addresses and netmasks
 *            in dotted-decimal form.  Netmasks are optionally
 *            prepended to the IP address using a plus sign.  E.g.
 *            255.255.255.0+123.45.67.89.  Any byte in the IP address
 *            (but not the netmask) can be wildcarded using "*"
 *    context         A pointer to the IP LAS context structure.
 *    buildOnlyIpVersion       int will determine the size of the tree to build
 *    RETURNS
 *    ret code    The usual LAS return codes. In case of SUCCESS, IP address
 *                version (PR_AF_INET or PR_AF_INET6 or PR_AF_INET+PR_AF_INET6).
 */
static int
LASIpBuild(NSErr_t *errp, char *attr_pattern, LASIpTree_t **treetop, 
           int buildOnlyIpVersion)
{
    unsigned int delimiter;                /* length of valid token     */
    char        token[1024], token2[1024];    /* a single ip[+netmask]     */
    char        *curptr;                /* current place in attr_pattern */
    PRNetAddr   netmask;
    PRNetAddr   ip;
    char        *plusptr;
    int            retcode;
    int currIpVersion = -1;
    int count=0;

    /* ip address can be delimited by space, tab, comma, or carriage return
     * only.
     */
    curptr = attr_pattern;
    do {
        delimiter    = strcspn(curptr, ", \t");
        delimiter    = (delimiter <= strlen(curptr)) ? delimiter : strlen(curptr);
        strncpy(token, curptr, delimiter);
        token[delimiter] = '\0';
        /* skip all the white space after the token */
        curptr = strpbrk((curptr+delimiter), VALID_CHARS_IN_PATTERN);

        /* Is there a netmask?    */
        plusptr    = strchr(token, '+');
        if (plusptr == NULL) {
            if (curptr && (*curptr == '+')) {
                /* There was a space before (and possibly after) the plus sign*/
                curptr = strpbrk((++curptr), VALID_CHARS_IN_IP);
                delimiter    = strcspn(curptr, ", \t");
                delimiter    = (delimiter <= strlen(curptr)) ? delimiter : strlen(curptr);
                strncpy(token2, curptr, delimiter);
                token2[delimiter] = '\0';
                currIpVersion = parseIp(token, token2, &ip, &netmask);
                // When ip=* return PR_AF_INET6+PR_AF_INET, so that calling
                // function can skip some processing like comparing bits
                if (currIpVersion == SUM_PR_AF_INET_AND_INET6) {
                    return (currIpVersion);
                }
                curptr = strpbrk((++curptr), VALID_CHARS_IN_PATTERN);
            } else {
                currIpVersion = parseIp(token, NULL, &ip, &netmask);
                // When ip=* return PR_AF_INET6+PR_AF_INET, so that calling
                // function can skip some processing like comparing bits
                if (currIpVersion == SUM_PR_AF_INET_AND_INET6) {
                    return (currIpVersion);
                }
            }
        } else {
            /* token is the IP addr string in both cases */
            *plusptr ='\0';    /* truncate the string */
            currIpVersion = parseIp(token, ++plusptr, &ip, &netmask);
            // When ip=* return PR_AF_INET6+PR_AF_INET, so that calling
            // function can skip some processing like comparing bits
            if (currIpVersion == SUM_PR_AF_INET_AND_INET6) {
                return (currIpVersion);
            }
        }

        if ((currIpVersion == PR_AF_INET6) || (currIpVersion == PR_AF_INET))
           count++;
        if (currIpVersion == buildOnlyIpVersion) {
            retcode = LASIpAddPattern(errp, netmask, ip, treetop, currIpVersion);
            if (retcode) {
                return LAS_EVAL_INVALID;
            }
        }
    } while ((curptr != NULL) && (delimiter != (int)NULL));

    // Atleast one of the IP Addresses specified was a valid IP Address
    // Return the IP address version so that  32 bits or 128 bits are
    // compared in the tree.
    if (count)
        return buildOnlyIpVersion;
    else
        return LAS_EVAL_INVALID;
}

/*    LASIpAddPattern
 *    Takes a netmask and IP address and a pointer to an existing IP
 *    tree and adds nodes as appropriate to recognize the new pattern.
 *    INPUT
 *    netmask        netmask in PRNetAddr
 *    pattern        IP address in PRNetAddr
 *    *treetop    An existing IP tree or 0 if a new tree
 *    ipVersion   int PR_AF_INET or PR_AF_INET6
 *    RETURNS
 *    ret code    NULL on success, ACL_RES_ERROR on failure
 *    **treetop    If this is a new tree, the head of the tree.
 */
static int
LASIpAddPattern(NSErr_t *errp, PRNetAddr netmask, PRNetAddr pattern, LASIpTree_t **treetop, int ipVersion)
{
    int        stopbit;    /* Don't care after this point    */
    int        curbit;        /* current bit we're working on    */
    int        curval;        /* value of pattern[curbit]    */
    LASIpTree_t    *curptr;    /* pointer to the current node    */
    LASIpTree_t    *newptr;

    int max_bit = 32;
    if (ipVersion == PR_AF_INET6)
        max_bit = 128;
    else if (ipVersion != PR_AF_INET)
       return LAS_EVAL_INVALID;

    /* stop at the first 1 in the netmask from low to high         */
    for (stopbit=0;  stopbit<max_bit; stopbit++) {
        if (getBit(netmask,stopbit,max_bit) != 0)
            break;
    }

    /* Special case if there's no tree.  Allocate the first node    */
    if (*treetop == (LASIpTree_t *)NULL) {    /* No tree at all */
        curptr = LASIpTreeAllocNode(errp);
        if (curptr == NULL) {
	    nserrGenerate(errp, ACLERRFAIL, ACLERR5100, ACL_Program, 1, XP_GetAdminStr(DBT_ipLasUnableToAllocateTreeNodeN_));
            return ACL_RES_ERROR;
        }
        *treetop = curptr;
    } else
	curptr = *treetop;

    /* Special case if the netmask is 0.
     */
    if (stopbit > (max_bit -1)) {
        curptr->action[0] = (LASIpTree_t *)LAS_EVAL_TRUE;
        curptr->action[1] = (LASIpTree_t *)LAS_EVAL_TRUE;
        return 0;
    }


    /* follow the tree down the pattern path bit by bit until the
     * end of the tree is reached (i.e. a constant).
     */
    for (curbit=max_bit-1,curptr=*treetop; curbit >= 0; curbit--) {

        /* Is the current bit ON?  If so set curval to 1 else 0    */
        curval = getBit(pattern, curbit, max_bit);

        /* Are we done, if so remove the rest of the tree     */
        if (curbit == stopbit) {
            LASIpTreeDealloc(curptr->action[curval]);
            curptr->action[curval] = 
                    (LASIpTree_t *)LAS_EVAL_TRUE;

            /* This is the normal exit point.  Most other 
             * exits must be due to errors.
             */
            return 0;
        }

        /* Oops reached the end - must allocate        */
        if (LAS_IP_IS_CONSTANT(curptr->action[curval])) {
            newptr = LASIpTreeAllocNode(errp);
            if (newptr == NULL) {
                LASIpTreeDealloc(*treetop);
	        nserrGenerate(errp, ACLERRFAIL, ACLERR5110, ACL_Program, 1, XP_GetAdminStr(DBT_ipLasUnableToAllocateTreeNodeN_1));
                return ACL_RES_ERROR;
            }
            curptr->action[curval] = newptr;
        }

        /* Keep going down the tree                */
        curptr = curptr->action[curval];
    }

    return ACL_RES_ERROR;
}

/*    LASIpFlush
 *    Deallocates any memory previously allocated by the LAS
 */
void
LASIpFlush(void **las_cookie)
{
    if (*las_cookie    == NULL)
        return;

    LASIpTreeDealloc(((LASIpContext_t *)*las_cookie)->treetop);
    PERM_FREE(*las_cookie);
    *las_cookie = NULL;
    return;
}

/*
 *    LASIpEval
 *    INPUT
 *    attr_name      The string "ip" - in lower case.
 *    comparator     CMP_OP_EQ or CMP_OP_NE only
 *    attr_pattern   A comma-separated list of IP addresses and netmasks
 *                   in dotted-decimal form.  Netmasks are optionally
 *                   prepended to the IP address using a plus sign.  E.g.
 *                   255.255.255.0+123.45.67.89.  Any byte in the IP address
 *                   (but not the netmask) can be wildcarded using "*"
 *    *cachable      Always set to ACL_INDEF_CACHABLE
 *    subject        Subject property list
 *    resource       Resource property list
 *    auth_info      The authentication info if any
 *    RETURNS
 *    ret code       The usual LAS return codes.
 */
int LASIpEval(NSErr_t *errp, char *attr_name, CmpOp_t comparator,
          char *attr_pattern, ACLCachable_t *cachable, void **LAS_cookie,
          PList_t subject, PList_t resource, PList_t auth_info,
          PList_t global_auth)
{
    void               *pip;
    int                retcode;
    LASIpContext_t     *context;
    int		       rv;

#ifndef UTEST
    *cachable = ACL_INDEF_CACHABLE;
#endif

    if (strcmp(attr_name, "ip") != 0) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5200, ACL_Program, 2, XP_GetAdminStr(DBT_lasIpBuildReceivedRequestForAttr_), attr_name);
        return LAS_EVAL_INVALID;
    }

    if ((comparator != CMP_OP_EQ) && (comparator != CMP_OP_NE)) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5210, ACL_Program, 2, XP_GetAdminStr(DBT_lasipevalIllegalComparatorDN_), comparator_string(comparator));
        return LAS_EVAL_INVALID;
    }

    /* GET THE IP ADDR FROM THE SESSION CONTEXT AND STORE IT IN THE
     * VARIABLE ip.
     */
#ifndef    UTEST
    rv = ACL_GetAttribute(errp, ACL_ATTR_IP, &pip,
			  subject, resource, auth_info, global_auth);

    PRNetAddr *ip_addr = (PRNetAddr *)pip;
    if (rv != LAS_EVAL_TRUE) {
        if (subject || resource) {
            /* Don't ereport if called from ACL_CachableAclList */
	    char rv_str[16];
	    sprintf(rv_str, "%d", rv);
	    nserrGenerate(errp, ACLERRINVAL, ACLERR5220, ACL_Program, 2, XP_GetAdminStr(DBT_lasipevalUnableToGetSessionAddre_), rv_str);
        }
        ereport(LOG_VERBOSE,
                "ERROR Attribute Getter for ACL_ATTR_IP returned error %d.",
                rv);
	return LAS_EVAL_FAIL;
    }

#else
     PRNetAddr *ip_addr = LASIpGetIpv6();
    if (ip_addr == NULL) {
        ereport(LOG_VERBOSE,
                "ERROR IP Address returned from LASIpGetIpv6() is NULL.");
        return LAS_EVAL_FAIL;
    }
#endif
    int ipVersion = PR_AF_INET;
    if (ip_addr->ipv6.family == PR_AF_INET6) {
         ipVersion = PR_AF_INET6;
    } else if (ip_addr->inet.family != PR_AF_INET) {
         PR_ASSERT(0);
    }

    /* If this is the first time through, build the pattern tree first.
     */
    if (*LAS_cookie == NULL) {
        if (strcspn(attr_pattern, "0123456789.*ABCDEF:abcdef,+ \t")) {
            nserrGenerate(errp,ACLERRINVAL,ACLERR5120,ACL_Program,2,
                 XP_GetAdminStr(DBT_lasIpIncorrentIPPattern),attr_pattern);
            return LAS_EVAL_INVALID;
        }
        ACL_CritEnter();
        context = (LASIpContext *) *LAS_cookie;
        if (*LAS_cookie == NULL) {    /* must check again */
            *LAS_cookie = context = (LASIpContext_t *)PERM_MALLOC(sizeof(LASIpContext_t));
            if (context == NULL) {
		nserrGenerate(errp, ACLERRNOMEM, ACLERR5230, ACL_Program, 1, XP_GetAdminStr(DBT_lasipevalUnableToAllocateContext_));
                ACL_CritExit();
                return LAS_EVAL_FAIL;
            }
            context->treetop = NULL;
            retcode = LASIpBuild(errp, attr_pattern, &context->treetop, ipVersion);
            if ((retcode == PR_AF_INET6) ||
                (retcode == PR_AF_INET) ||
                (retcode == (PR_AF_INET6+PR_AF_INET)))
                 context->ipVersion = retcode;
            else {
                ACL_CritExit();
                return (retcode);
	    }
        }
	ACL_CritExit();
    } else
        context = (LASIpContext *) *LAS_cookie;

    return traverseTreeAndCompareIPs(errp, ip_addr, context, attr_pattern, 
                                     ipVersion, (comparator == CMP_OP_EQ)?1:0);
}

/* 
 * traverseTreeAndCompareIPs
 * This function compares bit by bit of IP address with the tree nodes
 * Input
 *   NSErr_t *errp
 *   PRNetAddr * ip_addr - IP address of the client
 *   LASIpContext *context
 *   char * attr_pattern - ip address/pattern passed into LASIpEval
 *   int ipVersion - ipVersion of clients IP Address
 *   bool comparator_is_equal - true if comparator is CMP_OP_EQ
 * Returns 
 * LAS_EVAL_TRUE or LAS_EVAL_FALSE, or  LAS_* error codes in case of error.
 */
int traverseTreeAndCompareIPs(NSErr_t *errp, PRNetAddr *ip_addr, 
                              LASIpContext *context, char *attr_pattern,
                              int ipVersion, bool comparator_is_equal)
{
    // special case ip=*
    if (context->ipVersion == (PR_AF_INET6+PR_AF_INET)) {
        if (comparator_is_equal) {
            ereport(LOG_VERBOSE, "acl ip: match on ip = (%s)",
                    attr_pattern);
            return(LAS_EVAL_TRUE);
        } else {
            ereport(LOG_VERBOSE, "acl ip: no match on ip != (%s)",
                    attr_pattern);
            return(LAS_EVAL_FALSE);
        }
    }
     
    LASIpTree_t *node    = context->treetop;

    if (context->ipVersion != ipVersion || node == NULL) {
        ereport(LOG_VERBOSE,
                "ERROR IP Address returned by Attribute Getter for ACL_ATTR_IP did not match with IP Address Version set in ACL file.");
        if (comparator_is_equal) {
            return(LAS_EVAL_FALSE);
        } else {
            return(LAS_EVAL_TRUE);
        }
    }
    int max_bits=32;
    if (ipVersion == PR_AF_INET6)
        max_bits=128;
    else if (ipVersion != PR_AF_INET)
       return LAS_EVAL_INVALID;

    for (int bit=(max_bits-1); bit >=0; bit--) {
        int value = getBit(*ip_addr,bit,max_bits);
        if (LAS_IP_IS_CONSTANT(node->action[value])) {
            /* Reached a result, so return it */
            int r = (int)(size_t) node->action[value];
            if (comparator_is_equal) {
                ereport(LOG_VERBOSE, "acl ip: %s on ip = (%s)",
                        (r == LAS_EVAL_TRUE) ? "match" : "no match",
                        attr_pattern);
                return(r);
            }
            else {
                ereport(LOG_VERBOSE, "acl ip: %s on ip != (%s)",
                        (r == LAS_EVAL_TRUE) ? "no match" : "match",
                        attr_pattern);
                return((r == LAS_EVAL_TRUE) ? 
                    LAS_EVAL_FALSE : LAS_EVAL_TRUE);
            }
        }
        else {
            /* Move on to the next bit */
            node = node->action[value];
        }
    }

    /* Cannot reach here.  Even a 32 bit mismatch has a conclusion in 
     * the pattern tree.
     */
    char ip_str[124];
    memset(ip_str,0,124);
    PR_NetAddrToString(ip_addr,ip_str,124);
    nserrGenerate(errp, ACLERRINTERNAL, ACLERR5240, ACL_Program, 2, XP_GetAdminStr(DBT_lasipevalReach32BitsWithoutConcl_), max_bits, ip_str);
    return LAS_EVAL_INVALID;
}

#ifndef UTEST
int
LASIpGetter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
           auth_info, PList_t global_auth, void *arg)
{
    Session *sn=NULL;
    int rv;
    IPAddr_t ip;
    int retcode, tmpip, netmask;
    char * tmp;

    rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);
    if (rv < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameLASIpGetter1), rv);
        return LAS_EVAL_FAIL;
    }

    tmp = inet_ntoa(sn->iaddr);
    retcode =dotdecimal(tmp, "255.255.255.255", &tmpip, &netmask);
    if (retcode)
        return (retcode);
    ip = tmpip;

    rv = PListInitProp(subject, ACL_ATTR_IP_INDEX, ACL_ATTR_IP, (void *)ip, NULL);
    if (rv < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameLASIpGetter2), rv);
        return LAS_EVAL_FAIL;
    }

    return LAS_EVAL_TRUE;
}

/* 
 * LASIpv6Getter
 * This is the Attribute Getter function for  IPv6 Addresses.
 * LAS_EVAL_FAIL on failure of LAS_EVAL_TRUE on success.
 */
int
LASIpv6Getter(NSErr_t *errp, PList_t subject, PList_t resource, PList_t
              auth_info, PList_t global_auth, void *arg)
{
    Request *rq = 0;
    int rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX,
                           (void **)&rq, NULL);
    if (rv < 0) {
        ereport(LOG_VERBOSE, "Unable to get request object", rv);
        return LAS_EVAL_FAIL;
    }
    HttpRequest *hrq =  GetHrq(rq);
    DaemonSession &dsn = hrq->GetDaemonSession();
    PRNetAddr *ip = dsn.GetRemoteAddress();
    
    rv = PListInitProp(subject, ACL_ATTR_IP_INDEX, ACL_ATTR_IP,
                       (void *)ip, NULL);
    if (rv < 0) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclFrameLASIpGetter2), rv);
        return LAS_EVAL_FAIL;
    }

    return LAS_EVAL_TRUE;
}
#endif

/* separateIpAndNetmaskPrefixLength
 * This function extracts the netmask prefix-length from the IP Address string.
 * Removes the netmask from the IP Address string and returns that new
 * IP Address String without the netmask.
 * Input-Output:
 *     char * : pointer to combined IP Address and netmask string
 *     int base : 32 or 128
 * Output :
 *     int netmask or LAS_EVAL_INVALID in case of error. -1 if / is not found
 */
static int separateIpAndNetmaskPrefixLength(char *ip, int base) {
    PR_ASSERT(ip != NULL);
    if (ip == NULL)
        return LAS_EVAL_INVALID;

    char *pPrefix = strchr(ip, '/');
    if (pPrefix == NULL) {
        return -1;
    }
    // ip address passed is x::x/
    if (pPrefix == ip+strlen(ip)-1) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address prefix-length %s.",
                ip);
        return LAS_EVAL_INVALID;
    }
    // Remove trailing / and netmask and reflect back this new IP Address
    *pPrefix='\0';
    pPrefix++;

    int prefixLength = atoi(pPrefix);
    PR_ASSERT(base == 32 || base == 128);
    if (prefixLength <= 0 || prefixLength > base) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address prefix-length %s.",
                "Prefix length should be greater than %d or less than %d",
                pPrefix, 0, base);
        return LAS_EVAL_INVALID;
    }
    return prefixLength;
}

/* getDelimCount
 * This function calculates the number of times a particular character occurs
 * in a given input string.
 * Input :
 *   const char * str : input string
 *   char  delim : character whose occurance has to be counted.
 * Output :
 *   int the number of times the character occurs in the string specified.
 *   0 in case it is not found.
 */
static int getDelimCount(const char *str, char c)
{
    PR_ASSERT(str != NULL);
    if (str == NULL)
        return 0;

    int count=0;
    for (int i=0; i<strlen(str); i++) {
        if (str[i] == c) 
            count++;
    }
    return count;
}

/* setNetmaskBits
 * This functions fills PRNetAddr netmask from the input integer.
 * Set to 0, least Significant bits from 0 to ((128 or 32)-n) in netmask.
 * Rest (most significant) n bits are set to one.
 * Input :
 *     int n : the number of bits to be set to zero.
 *     int base : 32 or 128
 * Ouput :
 *     PRNetAddr *addr : pointer to the netmask address
 */
static void setNetmaskBits(PRNetAddr *addr, int n, int base) 
{
    PR_ASSERT(base == 32 || base == 128);
    PR_ASSERT(n >= 0 && n <= base);
    if (n < 0 || n > base)
        n = base;

    if (base == 128) {
        addr->raw.family = PR_AF_INET6;
        addr->ipv6.family = PR_AF_INET6;
        memset(&(addr->ipv6.ip.pr_s6_addr),0,16);
    } else if (base == 32) {
        addr->raw.family = PR_AF_INET;
        addr->inet.family = PR_AF_INET;
        addr->inet.ip = 0;
    } else
        return;
    for (PRUint32 i=base-n; i<base; i++) {
        setBit(addr,i,base);
    }
    return;
}

/*
 * handleIPv6Wildcard
 * This function takes a valid IPv6 address containing a "*" and
 * returns the position of that wildcard.
 * If ipAddress passed is abcd:* wildcard position returned is 16.
 * DO NOT pass ip=* into this function.
 * Call it only when wildcard is present in the string.
 * Input-Output: char * input ip address pattern
 * Output : 
 *     int - the position where wildcard is found. 
 *     LAS_EVAL_INVALID in case of error or if wildcard is not found.
 */
static int handleIPv6Wildcard(char *ip)
{
    PR_ASSERT(ip != NULL);

    // validations
    // Fe30::ffff:* is not allowed
    // but if hasdot case, ::FFFF::123.80.*  is allowed
    // There is a catch here, ::FFFF::123.80.* corresponding ::FFFF:fff*
    if (strstr(ip, DOUBLE_COLON) != NULL) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s. Can not have :: and * both.",
                ip);
        return LAS_EVAL_INVALID;
    }
    char *ptr = NULL;
    if ((ptr = strchr(ip, '*')) == NULL) {
        ereport(LOG_VERBOSE, 
                "ERROR Invalid IP Address %s. * MUST be the last character.",
                ip);
        return LAS_EVAL_INVALID;
    }
    int len = strlen(ip);
    if ((ptr != ip+len-1) &&
       (ip[len-1] != '*')) {
        ereport(LOG_VERBOSE, 
                "ERROR Invalid IP Address %s. * MUST be the last character.",
                ip);
        return LAS_EVAL_INVALID;
    }

    if (getDelimCount(ip, '*') > 1) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s. MUST not have more than one *s.",
                ip);
        return LAS_EVAL_INVALID;
    }

    int wildcardPos = LAS_EVAL_INVALID;
    int cnt=0;
    int coloncnt=0;
    char *pLastColon=NULL;
    for(int i=0; i<len; i++) {
         char c = ip[i];
         if (c == '*') {
             // 123* return 3*4, 123:567*  return 7*4
             PR_ASSERT(i == len -1);
             wildcardPos = coloncnt*4 + cnt;
             break;
         } else if (c == ':') {
             cnt=0;
             coloncnt++;
             pLastColon = ip+i;
         } else {
             cnt++;
         }
    } // for
    // IP Address MUST not have more than 7 :s.
    if (coloncnt > 7) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s.", 
                ip);
        return LAS_EVAL_INVALID;
    }

    // substitue * with ::
    // but if it is the last 8th section substitute * with appropriate zeros.
    int digits = (ip+len-1) - (pLastColon+1);
    int num=4-digits;
    if ((coloncnt != 7) && (ip[len-2] == ':')) {
        // replace trailing :* with ::
        // abcd:ef01:2345:6789:abcd:* to abcd:ef01:2345:6789:abcd::
        ip[len-1] = ':';
    } else if (coloncnt == 7 && digits == 3) {
        // replace x:x:x:x:x:x:x:abc* to x:x:x:x:x:x:x:abc0
        // abcd:ef01:2345:6789:abcd:ef01:2345:678* to abcd:ef01:2345:6789:abcd:ef01:2345:6780
        ip[len-1] = '0';
     } else if (num == 0 && coloncnt == 7) {
        // MUST not have 7 :s and 4 digits in the last section
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s.",
                ip);
        return LAS_EVAL_INVALID;
    } else if (num >= 0) {
        // num = 0 for
        // replace x:x:x:x:x:abcd* to x:x:x:x:x:abcd::
        // abcd:ef01:2345:6789:abcd* to abcd:ef01:2345:6789:abcd::

        // Trailing colons = zero for
        // modify trailing digit* with digit<appropriate-number-of-zeros>
        // abcd:ef01:2345:6789:abcd:ef01:2345:67*
        // abcd:ef01:2345:6789:abcd:ef01:2345:6*
        // abcd:ef01:2345:6789:abcd:ef01:2345:*

        // trailingColons=2 for
        // modify trailing digit* with digit<appropriate--number-of-zeros>::
        // abcd:ef01:2345:6789:abcd:23* to abcd:ef01:2345:6789:abcd:2300::
        // abcd:ef01:2345:6789:abcd:2* to abcd:ef01:2345:6789:abcd:2000::
        // abcd:ef01:2345:6789:abcd:* to abcd:ef01:2345:6789:abcd::

        int trailingColons=(coloncnt == 7)?0:2;
        char *p= ip+len-1;
        for (int j=0; j<num; j++, p++)
            *p = '0';
        for (int k=0; k<trailingColons; k++, p++)
            *p = ':';
        *p = '\0';
        PR_ASSERT(p == (ip+len+num+trailingColons-1));
    } else {
        PR_ASSERT(0);
        return LAS_EVAL_INVALID;
    }

    return wildcardPos*4;
}

/*
 * handleIPv4Wildcard
 * This function takes a valid IPv4 address containing a "*" and returns
 * the position of that wildcard. If IPAddress starts with :: or ::FFFF: 
 * it removes it, so that it becomes a valid IPv4 address.
 * Call it only when IP address pattern specified has dots (i.e. it is 
 * an IPv4 address).
 * Call it only when wildcard is present in the string.
 * Do not pass ip=* into this function.
 * Input-Output : char * input ip address pattern
 * Output : int the position where wildcard is found.
 * Returns:
 * LAS_EVAL_INVALID in case of error or if wildcard is not found.
 */
static int handleIPv4Wildcard(char *ip)
{
    PR_ASSERT(ip != NULL);

    int l = strlen(ip);
    // If IP Address starts with ::FFFF:
    // i.e. ::FFFF:d.d.d.d Make it d.d.d.d
    if ((l > DOUBLE_COLON_FFFF_COLON_LEN) && 
        (!strncasecmp(ip, DOUBLE_COLON_FFFF_COLON,
                      DOUBLE_COLON_FFFF_COLON_LEN))) {
        //reset the input buffer
        memcpy(ip,ip+DOUBLE_COLON_FFFF_COLON_LEN,l-DOUBLE_COLON_FFFF_COLON_LEN);
        ip[l-DOUBLE_COLON_FFFF_COLON_LEN] = '\0';
    } else if ((l > DOUBLE_COLON_LEN) && 
               (!strncmp(ip,DOUBLE_COLON,DOUBLE_COLON_LEN))) {
        // If IP Address starts with ::
        // i.e. ::d.d.d.d Make it d.d.d.d
        //reset the input buffer
        memcpy(ip,ip+DOUBLE_COLON_LEN,l-DOUBLE_COLON_LEN);
        ip[l-DOUBLE_COLON_LEN] = '\0';
    }
    // If it still has a : return error
    if (strchr(ip, ':')) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s. Can have only ::d.d.d.d or ::FFFF:d.d.d.d format.",
                ip);
        return LAS_EVAL_INVALID;
    }
    int dotcount = getDelimCount(ip, '.');
    if (dotcount < 1 || dotcount > 3) {
         ereport(LOG_VERBOSE,
                 "ERROR Invalid IP Address %s. Can have only 1,2 or 3 '.'s with a '*'. %d '.'s found.",
                 ip, dotcount);
         return LAS_EVAL_INVALID;
    }

    // Assuming its 123.456.789.012 then,
    // it can be 123.* or 123.456.* or 123.456.789.*
    // or 123.*.*.*  or  123.456.*.* or *.*.*.*
    // we can NOT have 12* or 123.4* keeping the traditions of 6.1
    // Now calculate length after :: has been removed
    int len = strlen(ip);
    int starcount = getDelimCount(ip, '*');
    PRBool validFormat = PR_TRUE;
    if (starcount > 1 && dotcount != 3)
        validFormat = PR_FALSE;
    char* ptr = strchr(ip, '*');
    if (ptr && validFormat) {
        if ((ptr != ip) && (*(ptr -1) != '.'))
            validFormat = PR_FALSE;
        else {
            // Now we can only expect ".*" combination after ptr
            ptr++;
            while (validFormat && *ptr) {
                // *ptr is not null so it is safe to read *(ptr+1)
                // we expect only .* from this point
                if (! ((*ptr == '.') && (*(ptr+1) == '*'))) {
                    validFormat = PR_FALSE;
                    break;
                }
                ptr += 2;
            }
        }
     }
     if (!validFormat) {
          ereport(LOG_VERBOSE,
                 "ERROR Invalid IP Address %s",
                 ip);
         return LAS_EVAL_INVALID;
    }
    // if dotcount == 3 need to substitute * with 0
    // 8*1 for 123.* or 8*2 for 123.45.* or 8*3 for 123.45.67.* 
    int wildcardPos = 8*dotcount;
    // 8*1 for 123.*.*.* or 8*2 for 123.456.*.*  or 8*0 for *.*.*.*
    if (starcount > 1)
        wildcardPos = 8*(4-starcount);
    if (dotcount == 3) {
        // 123.*.*.* or 123.456.*.* or 123.456.78.* or *.*.*.*
        for(int i=0; i<len;i++)
            if (ip[i] == '*')
                ip[i] = '0';
    } else {
       // if dotcount == 1 need to substitute * with 0.0.0(5)
       // if dotcount == 2 need to substitute * with 0.0(3)
       int extra = (4-dotcount)*2-1;
       if (dotcount == 1) {
           memcpy(ip+len-1,"0.0.0",extra);
       } else if (dotcount == 2) {
           memcpy(ip+len-1,"0.0",extra);
       }
       ip[len+extra-1]= '\0';
   }
    return wildcardPos;
}

/*
 * convertIPv6NetmaskToIPv4
 * This function converts an IPv6 netmask address to an IPv4 address.
 * Input : PRNetAddr * src_v6addr IPv6 address
 * Output : PRNetAddr * dst_v4addr IPv4 address
 */
static void convertIPv6NetmaskToIPv4(const PRNetAddr *src_v6addr,
                                     PRNetAddr *dst_v4addr)
{
    PR_ASSERT(PR_AF_INET6 == src_v6addr->ipv6.family);

    const PRUint8 *srcp = src_v6addr->ipv6.ip.pr_s6_addr;
    memset((char *)&dst_v4addr->inet.ip, 0, 4);
    dst_v4addr->inet.ip = src_v6addr->ipv6.ip.pr_s6_addr32[3];
    dst_v4addr->inet.family = PR_AF_INET;
    dst_v4addr->raw.family = PR_AF_INET;
}

/*
 * convertToIPv4NetAddr
 * This function converts an IPv4-mapped IPv6 address to an IPv4 address.
 * ::FFFF:X:X (32 are non zeros) or 
 * ::X:X (32 are non zeros and rest are 0's) but NOT loopback address(::1)
 * Input : PRNetAddr * src_v6addr IPv6 address
 * Output : PRNetAddr * dst_v4addr IPv4 address
 */
static void convertToIPv4NetAddr(const PRNetAddr *src_v6addr,
                                 PRNetAddr *dst_v4addr)
{
    PR_ASSERT(PR_AF_INET6 == src_v6addr->ipv6.family);

    // ::FFFF:X:X
    if (PR_IsNetAddrType(src_v6addr, PR_IpAddrV4Mapped)) {
        // IPv6 address is a union of 16*PRUint8 each of 1 byte (8 bits)
        // copy last 32 bits src_v6addr->ipv6.ip.pr_s6_addr32[3] or
        // copy last 4, 8 bits(1 byte).
        const PRUint8 *srcp = src_v6addr->ipv6.ip.pr_s6_addr;
        memcpy((char *)&dst_v4addr->inet.ip, srcp + 12, 4);
        dst_v4addr->inet.family = PR_AF_INET;
        dst_v4addr->raw.family = PR_AF_INET;
    } else if (!PR_IsNetAddrType(src_v6addr, PR_IpAddrLoopback) && 
               (src_v6addr->ipv6.ip.pr_s6_addr32[0] == 0 &&
               src_v6addr->ipv6.ip.pr_s6_addr32[1] == 0 &&
               src_v6addr->ipv6.ip.pr_s6_addr32[2] == 0 &&
               src_v6addr->ipv6.ip.pr_s6_addr32[3] != 0)) {
        // ::X:X (32 are non zeros and rest are 0's)
        dst_v4addr->inet.ip = src_v6addr->ipv6.ip.pr_s6_addr32[3];
        dst_v4addr->inet.family = PR_AF_INET;
        dst_v4addr->raw.family = PR_AF_INET;
   }
}

/*
 * parseIp
 * This functions takes the IP address and netmask as set in ACL file
 * and converts them into PRNetAddr. Returns the type of address found or
 * LAS_EVAL_INVALID on error.
 * Input :
 *   char *iIP : input ip address string
 *   char *iNetmask : input netmask string
 * Output :
 *   pointer to PRNetAddr IP Address
 *   pointer to PRNetAddr netmask
 * Returns int
 * if IP Address passed is  :
 *     "*" - returns (PR_AF_INET + PR_AF_INET6)
 *     a valid IPv4 address - returns PR_AF_INET
 *     a valid IPv6 address - returns PR_AF_INET6
 * if error returns LAS_EVAL_* codes
 */
int parseIp(char *iIP, char* iNetmask, PRNetAddr *pIP, PRNetAddr *pNetmask)
{
    // Must not be null and MUST only have 0123456789.*:abcdefABCDEF characters
    if (iIP == NULL || strcspn(iIP, "0123456789.*:abcdefABCDEF/")) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s.",
                iIP);
        return LAS_EVAL_INVALID;
    }

    // ip=*
    if (!strcmp(iIP,"*"))
        return (PR_AF_INET + PR_AF_INET6);

    int hasdot =0;
    if (strchr(iIP, '.'))
        hasdot=1;
    int hascolon=0;
    if (strchr(iIP, ':'))
        hascolon=1;
    if (!hasdot && !hascolon) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s. MUST have either ':' or a '.'.",
                iIP);
        return LAS_EVAL_INVALID;
    }

    // Do not modify the original input string
    char ip[1024];
    strcpy(ip,iIP);

    int prefix_length=-1;
    if (strchr(ip, '/')) {
        if (strchr(ip, '*')) {
            // ip=*/digits
            ereport(LOG_VERBOSE,
                    "ERROR Invalid IP Address %s. Can not have * and / both",
                    iIP);
            return LAS_EVAL_INVALID;
        }
        prefix_length = separateIpAndNetmaskPrefixLength(ip, hasdot?32:128);
        if (prefix_length == LAS_EVAL_INVALID) {
            return LAS_EVAL_INVALID;
        }
    }

    int wildcardPos=-1;
    if (strchr(ip, '*')) {
        if (hasdot)
            wildcardPos = handleIPv4Wildcard(ip);
        else
            wildcardPos = handleIPv6Wildcard(ip);
        if (wildcardPos == LAS_EVAL_INVALID) {
            return LAS_EVAL_INVALID;
        }
    }

    PRStatus status = PR_StringToNetAddr(ip, pIP);
    if (status != PR_SUCCESS) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid IP Address %s.",
                ip);
        return LAS_EVAL_INVALID;
    }

    if (pIP->raw.family == PR_AF_INET6)
        convertToIPv4NetAddr(pIP, pIP);

    // Netmask Routines
    // Wildcards in the ip override the netmask where they conflict.
    if (wildcardPos != -1) {
        setNetmaskBits(pNetmask, wildcardPos, 
                       (pIP->raw.family == PR_AF_INET6)?128:32);
    } else if (prefix_length != -1) {
        setNetmaskBits(pNetmask, prefix_length, hascolon?128:32);
    } else if (iNetmask) {
        if (PR_StringToNetAddr(iNetmask, pNetmask) !=  PR_SUCCESS) {
            int err1 = PR_GetError();
            /* NSPR issue PR_StringToNetAddr(255.255.255.255) is not a valid */
            if (err1 == PR_INVALID_ARGUMENT_ERROR &&
                !strcmp(iNetmask,ALL_255_IPV4)) {
                setNetmaskBits(pNetmask,32,32);
            } else if (err1 == PR_INVALID_ARGUMENT_ERROR &&
                       !strcasecmp(iNetmask,ALL_F_IPV6)) {
                setNetmaskBits(pNetmask,128,128);
            } else {
                ereport(LOG_VERBOSE,
                        "ERROR Invalid netmask %s.",
                        iNetmask);
                return LAS_EVAL_INVALID;
            }
        } // != PR_SUCCESS
    } else {
        // equivalent to setting "255.255.255.255"
        // or setting "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
        setNetmaskBits(pNetmask,
                       (pIP->raw.family == PR_AF_INET6)?128:32,
                       (pIP->raw.family == PR_AF_INET6)?128:32);
    }

    // if ip address passed is "::123.45.67.89/96" 
    // convert netmask into 32 bit netmask also.
    if (pIP->raw.family == PR_AF_INET &&
        pNetmask->raw.family == PR_AF_INET6) {
        convertIPv6NetmaskToIPv4(pNetmask, pNetmask);
    } else if (pIP->raw.family == PR_AF_INET6 &&
               pNetmask->raw.family == PR_AF_INET) {
        ereport(LOG_VERBOSE,
                "ERROR Invalid netmask in %s. IPv6 Address should NOT have IPv4 Netmask.", 
                iIP);
        return LAS_EVAL_INVALID;
    }
    PR_ASSERT(pIP->raw.family == PR_AF_INET || pIP->raw.family == PR_AF_INET6);
    return (pIP->raw.family);
}

/* setBit sets 1 in the bit of an 8 bit integer.
 * addr[0] contains the most significant byte
 * Input :
 *  addr : pointer to PRNetAddr structure containing the IP Adddress
 *  int pos : postition within the byte (range 0..127/31 for IPv6/IPv4 Address)
 *  int max_bit : type of ip address. 128 or 32 for IPv6/IPv4 Address
*/
static void setBit(PRNetAddr *addr, int pos, int max_bit)
{
    PR_ASSERT(pos >= 0 && pos<max_bit);
    int idx = (pos/8);
    int n = pos - (idx *8);
    PRUint8 *x=NULL;
    if (max_bit == 32) {
        unsigned char *c=(unsigned char *)&(addr->inet.ip);
        // byte number = 3-idx;
        x = c+3-idx;
    } else if (max_bit == 128) {
        // byte number = 15-idx;
        x = addr->ipv6.ip.pr_s6_addr+15-idx;
    } else {
        PR_ASSERT(0);
        return;
    }
    *(x) |= (1 << n);
    return;
}

/* getBit returns  the value set in the bit of an 8 bit integer.
 * Input :
 *  addr : pointer to PRNetAddr structure containing the IP Adddress
 *  int pos : postition within the byte (range 0..127/31 for IPv6/IPv4 Address)
 *  int max_bit : type of ip address. 128 or 32 for IPv6/IPv4 Address
 * returns:
 *     PRUInt8 : 0 or 1
*/
static PRUint8 getBit(PRNetAddr addr, int pos, int max_bit)
{
    PR_ASSERT(pos >= 0 && pos<max_bit);
    int idx = (pos/8);
    int n =  pos-(idx*8);
    PRUint8 x=0;
    if (max_bit == 32) {
        // byte number = 3-idx;
        unsigned char *c=(unsigned char *)&(addr.inet.ip);
        x = *(c+3-idx);
    } else if (max_bit == 128) {
        // byte number = 15-idx;
        x = addr.ipv6.ip.pr_s6_addr[15-idx];
    } else {
        PR_ASSERT(0);
        return 0;
    }
    return ((x & (1 << n)) ?1 : 0);
}
