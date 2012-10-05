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
* Description (acl.c)
*
*	This module contains routines involved in relating ACLs to
*	server objects.
*/

#include <ldap.h>
#include <ldap_ssl.h>

#include "netsite.h"
#include "safs/acl.h"
#include "safs/auth.h"
#include "base/shexp.h"
#include "frame/log.h"
#include "base/file.h"
#include "base/util.h"  /* util_uri_is_evil */
#include <base/pblock.h>
#include <base/daemon.h>
#include <base/vs.h>

#include "frame/protocol.h"
#include "frame/conf.h"
#include "frame/func.h"
#include "frame/http.h"
#include "frame/clauth.h"
#include <frame/aclinit.h>

#include <safs/init.h>
#include <safs/dbtsafs.h>
#include <safs/clauth.h>

#include <ldaputil/ldaputil.h>

#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include <libaccess/nserror.h>
#include <libaccess/nsauth.h>   /* ClAuth_t */
#include <libaccess/ldapacl.h>
#include <libaccess/aclerror.h>

#include <sys/stat.h>
#include <ctype.h>

#define BIG_LINE 1024

/*
 * Description (pcheck_check_acl30)
 *
 *	This function adds an ACL to an intermediate ACL list.
 *
 * Directive format:
 *
 *	PathCheck fn=check-acl acl=aclname path=shexp bong-file=filename
 *
 * Arguments:
 *
 *	pb		- PathCheck directive parameter block
 *	sn		- session pointer
 *	rq		- request pointer
 *
 * Returns:
 *
 *	Returns REQ_NOACTION or REQ_ABORTED.  REQ_ABORTED is
 *	returned only if an ACL denies access with an "always"
 *	directive.  The rq->aclstate value is updated if any
 *	any of the ACLs apply to the request.
 */

NSAPI_PUBLIC int pcheck_check_acl(pblock * pb, Session * sn, Request * rq)
{
    char * aclname;		/* ACL name */
    ACLHandle_t * acl;		/* named ACL */
    char *check_path;
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
    const VirtualServer *vs = request_get_vs(rq);
    ACLListHandle_t *acllist;
	
    /*
     * We add ACLs based only on the requested URI and obj.conf parameters,
     * so this directive is cacheable.  Whether the ACLs we add are compatible
     * with the accelerator cache is not our concern; that's decided later by
     * servact_pathchecks().
     */
    rq->directive_is_cacheable = 1;

    if (vs == 0 || (acllist = (ACLListHandle_t *)vs_get_acllist(vs)) == 0)
        return REQ_NOACTION;
	
    /* Is there a path shexp to match? */
    check_path = pblock_findval("path", pb);
	
    if (check_path != 0) {
        pb_param *path = pblock_find("path", rq->vars);
        
        /* Yes, check request path against it */
        if (WILDPAT_CMP(path->value, check_path)) {
                
            /* Request path does not match pattern, so do nothing */
            return REQ_NOACTION;
        }

        /* If magnus-internal/default-acls-only is set, caller wants only
         * those ACLs that apply to all paths
         */
        if (pblock_findkey(pb_key_magnus_internal_default_acls_only, rq->vars))
            return REQ_NOACTION;
    }
	
    /* Get the ACL name */
    aclname = pblock_findval("acl", pb);
    if (aclname == 0) {
        return REQ_NOACTION;
    }
	
    /* Else get it from the ACL database */
    acl = ACL_ListFind(errp, acllist, aclname, ACL_CASE_INSENSITIVE);
    if (!acl) {
        /* XXX server configuration error */
        log_error(LOG_MISCONFIG, "check-acl", sn, rq, XP_GetAdminStr(DBT_aclNameNotDef), aclname);
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
	
    /* Add it to the ACL List */
    if (!rq->acllist)
        rq->acllist = ACL_ListNew(errp);
    ACL_CritEnter();
    ACL_ListAppend(&err, rq->acllist, acl, 0);
    ACL_CritExit();

    return REQ_NOACTION;
}

/*
* Description (pcheck_acl_state)
*
*	This function processes the results of ACL evaluation, stored in
*	the aclstate field of the specified Request structure.  Most of
*	the work involves handling an ACL request for authentication.  The
*	authentication realm is specified by the clauth.cla_realm field
*	in the Session structure.  The aclstate field in the Request
*	structure indicates whether the request is immediate (ACD_ALWAYS)
*	or not.  The function first checks the request variables to see
*	if information is available from a previous call, and whether
*	the authentication was in the specified realm.  If not, it checks
*	for an "authorization" request header, and attempts to validate
*	the indicated user and password in the specified realm, creating
*	request variables to indicate a successful result.  If there is
*	no "authorization" request header, or if the result of processing
*	the user and password in the current realm was unsuccessful, then
*	the request is aborted with a request for authentication if
*	ACD_ALWAYS is set in aclstate.
*
*	This function is called by pcheck_check_acl() whenever an
*	individual ACL applies to a request.  It is also called from
*	servact_pathchecks() after all PathCheck directives have been
*	processed.  The ACD_ALWAYS flag is set prior to this call.
*
* Arguments:
*
*	pb		- PathCheck directive parameter block
*	sn		- session pointer
*	rq		- request pointer
*
* Returns:
*
*	Returns REQ_PROCEED, REQ_NOACTION, or REQ_ABORTED.  REQ_ABORTED
*	is returned to request immediate authentication, or to deny
*	access.
*/
int
pcheck_acl_state(pblock * pb, Session * sn, Request * rq)
{
    /* xxxx return error */
    log_error(LOG_SECURITY, "acl-state", sn, rq, 
            XP_GetAdminStr(DBT_aclpcheckACLStateErr1),
            rq->aclname, rq->acldirno);
    return REQ_NOACTION;
}

NSAPI_PUBLIC int
ACL_InitHttp(void)
{
    if (ACL_Init() || ACL_InitFrame() || ACL_InitSafs())
        return -1;
    else
        return 0;
}

NSAPI_PUBLIC int
ACL_InitHttpPostMagnus(void)
{
    if (ACL_InitPostMagnus() || ACL_InitFramePostMagnus() ||
		ACL_InitSafsPostMagnus() || ACL_LateInitPostMagnus())
        return -1;
    else
        return 0;
}

NSAPI_PUBLIC int
ACL_InitSafs(void)
{
    NSErr_t err = NSERRINIT;
	
    return init_acl_modules(&err) == REQ_PROCEED ? 0 : -1;
}

NSAPI_PUBLIC int
ACL_InitSafsPostMagnus(void)
{
    NSErr_t err = NSERRINIT;
    NSErr_t *errp = &err;
	
    if (!server_root) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_aclsafsEreport1));
        return -1;
    }
	
    char *sroot = server_root;
    int rv;
    char *dll_name;
    char *serv_type;
	
    dll_name = "ns-httpd40.dll";
    serv_type = "https";
	
    rv = ldaputil_init(security_active ? CERTMAP_CONF : 0,
		dll_name, sroot, serv_type, server_id);
	
    if (rv != LDAPU_SUCCESS) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_aclsafsEreport3),
                CERTMAP_CONF, ldapu_err2string(rv));
        return -1;
    }

    if (ACL_MethodIsEqual(errp, ACL_MethodGetDefault(errp), ACL_METHOD_INVALID)) {
        /*  default method is not set thro' magnus.conf */
        ACL_MethodSetDefault(errp, ACL_MethodBasic);
    }
    
    return 0;
}
