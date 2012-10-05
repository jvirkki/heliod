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

#include <netsite.h>
#include <base/nsassert.h>
#include <base/ereport.h>
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include "plhash.h"

/*  ACL_AddAclName
 *  Adds the ACLs for just the terminal object specified in a pathname.
 *  INPUT
 *	path	The filesystem pathname of the terminal object.
 *	acllistp The address of the list of ACLs found thus far.  
 *		Could be NULL.  If so, a new acllist will be allocated (if any
 *		acls are found).  Otherwise the existing list will be added to.
 *      masterlist	Usually the ACL list of the virtual server.
 */
void
ACL_AddAclName(char *path, ACLListHandle_t **acllistp, ACLListHandle_t *masterlist)
{
    ACLHandle_t *acl;
    NSErr_t *errp = 0;

#ifdef XP_WIN32
    acl = ACL_ListFind(errp, masterlist, path, ACL_CASE_INSENSITIVE);
#else
    acl = ACL_ListFind(errp, masterlist, path, ACL_CASE_SENSITIVE);
#endif
    if (!acl)
	return;

    NS_ASSERT(ACL_AssertAcl(acl));

    ereport(LOG_VERBOSE, "acl: matched an acl for [%s]", path);

    if (!*acllistp)
	*acllistp = ACL_ListNew(errp);
    ACL_ListAppend(NULL, *acllistp, acl, 0);

    NS_ASSERT(ACL_AssertAcllist(*acllistp));
    return;
}


/*  ACL_GetAcls
 */
static PRBool
ACL_GetAcls(char *path, ACLListHandle_t **acllistp, char *prefix, ACLListHandle_t *masterlist, PRBool flagExtensions, PRBool flagParents)
{
    PRBool res = PR_TRUE;
    char *slashp=path;
    int  slashidx;
    // ppath needs to be ACL_PATH_MAX+maxppath+3
    // maxppath is assumed to be 24
    char ppath[ACL_PATH_MAX+24+3];
    int  prefixlen;
    int ppathlen;

    NS_ASSERT(path);
    NS_ASSERT(prefix);

    if ((!path) || (!ppath)) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclcacheNullPath));
        res = PR_FALSE;
        return res;
    }

    strncpy(ppath, prefix, ACL_PATH_MAX);
    int lenPath = strlen(path);
    prefixlen = strlen(ppath);

    /* Find the extension */
    char *suffix = NULL;
    int suffixlen = 0;
    if (flagExtensions) {
        /* The extension begins with the last '.' in the last path segment */
        suffix = strrchr(path, '.');
        if (suffix && !strchr(suffix, '/'))
            suffixlen = strlen(suffix);
        else
            suffix = NULL;
    }

    /* Do we have enough room in ppath? */
    if (lenPath+prefixlen+3+suffixlen >= sizeof(ppath)) {
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_aclcachePath2Long));
        res = PR_FALSE;
        return res;
    }

    ACL_CritEnter();

    /* Handle the extension */
    if (suffix) {
        /* Handle "*.jsp" */
        ppath[prefixlen] = '*';
        memcpy(&ppath[prefixlen + 1], suffix, suffixlen + 1);
        ACL_AddAclName(ppath, acllistp, masterlist);
    }

    /* Handle the first "/". i.e. the root directory */
    if (*path == '/') {
        /* Handle "/" */
	ppath[prefixlen]='/';
	ppath[prefixlen+1]='\0';
        if (flagParents)
            ACL_AddAclName(ppath, acllistp, masterlist);

        /* Handle "/*" */
        ppath[prefixlen + 1] = '*';
        ppath[prefixlen + 2] = 0;
        ACL_AddAclName(ppath, acllistp, masterlist);

        if (suffix) {
            /* Handle "/*.jsp" */
            memcpy(&ppath[prefixlen + 2], suffix, suffixlen + 1);
            ACL_AddAclName(ppath, acllistp, masterlist);
        }

    	slashp = path;		
    }

    do {
	slashp = strchr(++slashp, '/');
	if (slashp) {
            slashidx = slashp - path;
            ppathlen = prefixlen + slashidx;

            /* Handle "/a/b" */
            memcpy(&ppath[prefixlen], path, slashidx);
            ppath[ppathlen] = '\0';
            if (flagParents || !slashp[1])
                ACL_AddAclName(ppath, acllistp, masterlist);

            /* Handle "/a/b/" */
            ppath[ppathlen] = '/';
            ppath[ppathlen + 1] = 0;
            if (flagParents)
                ACL_AddAclName(ppath, acllistp, masterlist);

            /* Handle "/a/b/*" */
            ppath[ppathlen + 1] = '*';
            ppath[ppathlen + 2] = 0;
            ACL_AddAclName(ppath, acllistp, masterlist);

            if (suffix) {
                /* Handle "/a/b/*.jsp" */
                memcpy(&ppath[ppathlen + 2], suffix, suffixlen + 1);
                ACL_AddAclName(ppath, acllistp, masterlist);
            }

	    continue;
	}

        /* Handle "/a/b/c.jsp" */
        memcpy(&ppath[prefixlen], path, lenPath + 1);
        ACL_AddAclName(ppath, acllistp, masterlist);

        /* Handle "/a/b/c.jsp/" */
        ppathlen = prefixlen + lenPath;
        ppath[ppathlen] = '/';
        ppath[ppathlen + 1] = 0;
        ACL_AddAclName(ppath, acllistp, masterlist);

        /* Handle "/a/b/c.jsp/*" */
        ppath[ppathlen + 1] = '*';
        ppath[ppathlen + 2] = 0;
        ACL_AddAclName(ppath, acllistp, masterlist);

	break;
    } while (slashp);

    ACL_CritExit();

    return res;
}


/*  ACL_GetPathAcls
 *  Adds the ACLs for all directories plus the terminal object along a given
 *  filesystem pathname. For each pathname component, look for the name, the
 *  name + "/", and the name + "/*".  The last one is because the resource
 *  picker likes to postpend "/*" for directories.
 *  INPUT
 *	path	The filesystem pathname of the terminal object.
 *	acllistp The address of the list of ACLs found thus far.  
 *		Could be NULL.  If so, a new acllist will be allocated (if any
 *		acls are found).  Otherwise the existing list will be added to.
 *	prefix  A string to be prepended to the path component when looking
 *		for a matching ACL tag.
 *      masterlist	Usually the ACL list of the virtual server.
 *
 * XXX this stuff needs to be made MUCH more efficient. I think it's one of the main reasons why
 * uncached ACLs are horribly slow. It will call ACL_AddAclName->ACL_ListFind a mindboggling
 * number of times, like 2 + (n * 3) + 3, where n is the number of "/"'s in "path".
 * A possible way to optimize this would be to build a sparse tree that mirrors the file system
 * directory structure, with ACLs that apply to a path associated to the corresponding node.
 * ACL_GetPathAcls could walk the tree according to "path" and pick up any ACLs that are on the way.
 * The necessary data structures could maybe be piggybacked onto the ACLList.
 * The tree would need to be constructed for every VS acllist, though.
 * There are various optimizations: 
 *  - If no path or uri ACLs are present, do not construct the tree.
 *
 * With the introduction of extension mapping for Servlet spec compliance,
 * the situation has gotten even worse.
 *
 * ACL_GetPathAcls is currently used with the "uri=" and "path=" prefixes.
 */
PRBool
ACL_GetPathAcls(char *path, ACLListHandle_t **acllistp, char *prefix, ACLListHandle_t *masterlist)
{
    return ACL_GetAcls(path, acllistp, prefix, masterlist, PR_FALSE, PR_TRUE);
}
