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


/* #define DBG_PRINT */

#include <netsite.h>
#include <base/rwlock.h>
#include <base/ereport.h>
#include <base/util.h>
#include <base/shexp.h>

#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclglobal.h>
#include <libaccess/aclerror.h>
#include <libaccess/digest.h>
#include "usrcache.h"

#include <libaccess/ldapacl.h>
#include <libaccess/LdapRealm.h>

#include <ldaputil/errors.h>
#include <ldaputil/certmap.h>
#include <ldaputil/ldaputil.h>
#include <ldaputil/dbconf.h>
#include "ldaputil/LdapSessionPool.h"

#include <base/vs.h>
#include <safs/init.h>                  // ACL_MethodBasic & ACL_MethodDigest
#include <safs/digest.h>                  // ACL_MethodBasic & ACL_MethodDigest

#include <plstr.h>

#include <frame/conf.h>

#include "definesEnterprise.h"

#define BIG_LINE 1024

#define DEFAULT_SESSION_POOL_SIZE 8
#define DEFAULT_TIMEOUT 10
#define MAX_SESSION_POOL_SIZE 512

/*
#define LDAP_URL_PREFIX      "ldap:"
#define LDAP_URL_PREFIX_LEN  5
#define LDAPS_URL_PREFIX     "ldaps:"
#define LDAPS_URL_PREFIX_LEN 6
*/
static int need_ldap_over_ssl = 0;

//
// ldapacl.cpp - LDAP specific functions for the ACL subsystem
//
// this file contains:
//
// get_is_valid_password_ldap       - attribute getter for ACL_ATTR_IS_VALID_PASSWORD
// get_user_ismember_ldap           - attribute getter for ACL_ATTR_USER_ISMEMBER
// get_cert2group_ldap              - attribute getter for ACL_ATTR_CERT2GROUP
// get_userdn_ldap                  - attribute getter for ACL_ATTR_USERDN
// get_user_exists_ldap             - attribute getter for ACL_ATTR_USER_EXISTS
// get_user_isinrole_ldap           - attribute getter for ACL_ATTR_USER_ISINROLE
//
// parse_ldap_url                   - parser for dbswitch.conf ldap database urls
// acl_map_cert_to_user_ldap        - find the user corresponding to a cert
// ACL_LDAPDatabaseHandle           - obsolete function to access LDAP handle
// ACL_LDAPSessionAllocate          - new API to access LDAP handle
// ACL_LDAPSessionFree              - new API to access LDAP handle
// ACL_NeedLDAPOverSSL              - hmm...
// acl_get_default_ldap_db          - gets default LDAP db handle
// 

//
// parse_ldap_url - parser for ldap database urls
//
// parses a dbswitch.conf entry and generates the database handle
//
NSAPI_PUBLIC int
parse_ldap_url (NSErr_t *errp, ACLDbType_t, const char *dbname,
                const char *urls, PList_t plist, void **db)
{
    char *authMethod = 0;
    char *certName   = 0;
    char *binddn = 0;
    char *bindpw = 0;
    char *dcsuffix = 0;
    char *nsesstr = 0;
#if defined(FEAT_DYNAMIC_GROUPS)
    char *dyngroups = 0;
    dyngroupmode_t dyngrmode = DYNGROUPS_ON;
#else
    dyngroupmode_t dyngrmode = DYNGROUPS_OFF;
#endif
    char *digestauthstr = 0;
    int digestauthstate = 0;
    int nsessions;
    char* userSearchFilter=0;
    char* groupSearchFilter=0;
    char* groupTargetAttr=0;
    int timeout=DEFAULT_TIMEOUT;

    int rv;

    *db = 0;

    if (!urls || !*urls) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5800, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclDatabaseUrlIsMissing));
	return -1;
    }

    if (!dbname || !*dbname) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR5810, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclDatabaseNameIsMissing));
	return -1;
    }

    /* look for binddn and bindpw in the plist */
    if (plist) {
	PListFindValue(plist, "binddn", (void **)&binddn, NULL);
	PListFindValue(plist, "bindpw", (void **)&bindpw, NULL);
	PListFindValue(plist, "dcsuffix", (void **)&dcsuffix, NULL);
	PListFindValue(plist, "authmethod", (void **)&authMethod, NULL);
	PListFindValue(plist, "client-cert-nickname",   (void **)&certName, NULL);
	PListFindValue(plist, "search-filter",   (void **)&userSearchFilter, NULL);
	PListFindValue(plist, "group-search-filter",  (void **)&groupSearchFilter, NULL);
	PListFindValue(plist, "group-target-attr",    (void **)&groupTargetAttr, NULL);

	char *timeoutAttr = 0;
	if (PListFindValue(plist, "timeout", (void **)&timeoutAttr, NULL) >= 0) {
	    timeout = atoi(timeoutAttr);
	}

    if (PListFindValue(plist, "sessions", (void **)&nsesstr, NULL) < 0) {
	    nsessions = DEFAULT_SESSION_POOL_SIZE;
	} else {
	    nsessions = atoi(nsesstr);
	}
	if (nsessions < 1 || nsessions > MAX_SESSION_POOL_SIZE) {
	    nserrGenerate(errp, ACLERRINVAL, ACLERR5815, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclInvalidNumberOfSessions));
	    return -1;
	}

#if defined(FEAT_DYNAMIC_GROUPS)
	// undocumented option to change the behaviour of dynamic groups with membership checks
	// off       - no consideration of dynamic groups
	// on        - dynamic groups are evaluated, but may not have group members
	// recursive - dynamic groups are evaluated and may have group members
	if (PListFindValue(plist, "dyngroups", (void **)&dyngroups, NULL) >= 0) {
	    if (PL_strcasecmp(dyngroups, "on") == 0) {
		dyngrmode = DYNGROUPS_ON;
	    } else if (PL_strcasecmp(dyngroups, "off") == 0) {
		dyngrmode = DYNGROUPS_OFF;
	    } else if (PL_strcasecmp(dyngroups, "recursive") == 0) {
		dyngrmode = DYNGROUPS_RECURSIVE;
        } else if (PL_strcasecmp(dyngroups, "fast") == 0) {
          dyngrmode = DYNGROUPS_FAST;
	    } else {
		nserrGenerate(errp, ACLERRINVAL, ACLERR5815, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclInvalidDynGroupsMode));
		return -1;
	    }
	}
#endif
	if (PListFindValue(plist, "digestauthstate", (void **)&digestauthstr, NULL) >= 0 ||
            PListFindValue(plist, "digestauth", (void **)&digestauthstr, NULL) >= 0)
        {
	    if (PL_strcasecmp(digestauthstr, "on") == 0) {
		digestauthstate = 1;
            } else {
                // screw it - leave it at off
            }
        }
    }

    LdapRealm *ldapRealm = new LdapRealm(urls,binddn,bindpw,dcsuffix,dyngrmode,nsessions,digestauthstate,
                     timeout,
                     authMethod,certName,userSearchFilter,groupSearchFilter,groupTargetAttr);
    if (ldapRealm->init(errp)<0) {
        delete ldapRealm;
        return -1;
    }

    *db = (void *)ldapRealm;
    return 0;
}




//
// get_ldap_basedn - get a basedn for a particular virtual server and database
//
// once a value is found it's cached in the virtdb object.
// since the virtdb object is part of the configuration, once a new configuration
// is put in place, the process needs to be done again.
//
static const char *
get_ldap_basedn(NSErr_t *errp, LdapSession *ld, ACLVirtualDb_t *virtdb, const char *vsid, const char *servername)
{
    const char *vsbasedn;
    int rv;

    ACL_VirtualDbReadLock(virtdb);
    // we need to find the basedn for the search first
    rv = ACL_VirtualDbGetAttr(NULL, virtdb, "vsbasedn", &vsbasedn);
    ACL_VirtualDbUnlock(virtdb);
    if (rv >= 0)
        return vsbasedn;       // found a cached value (might be NULL, though)...

    // not found, get the write lock
    // from that point, we're alone in this code
    ACL_VirtualDbWriteLock(virtdb);

    // check again (someone might just have done the same thing...)
    rv = ACL_VirtualDbGetAttr(NULL, virtdb, "vsbasedn", &vsbasedn);
    if (rv >= 0) {
        ACL_VirtualDbUnlock(virtdb);
        return vsbasedn;       // found a cached value...
    }

    // find out via the directory
    char buffer[1024];
    rv = ld->find_vsbasedn(servername, buffer, 1024);
    buffer[1023] = '\0'; // just in case

    switch (rv) {
    case LDAPU_SUCCESS:
        // oh good.
        break;
    case LDAPU_ERR_NO_SERVERNAME:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5500, ACL_Program, 2,
                XP_GetAdminStr(DBT_getLdapBasednMustHaveName), vsid);
        break;
    default:
    case LDAPU_ERR_INVALID_STRING:      // buffer overflow
    case LDAPU_ERR_INTERNAL:            // internal foobar
    case LDAPU_ERR_INVALID_ARGUMENT:    // buffer not there
	nserrGenerate(errp, ACLERRFAIL, ACLERR5510, ACL_Program, 1, XP_GetAdminStr(DBT_getLdapBasednInternalError));
        break;
    case LDAPU_ERR_OUT_OF_MEMORY:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5520, ACL_Program, 1, XP_GetAdminStr(DBT_getLdapBasednOutOfMemory));
        break;
    case LDAPU_ERR_WRONG_ARGS:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5530, ACL_Program, 3, XP_GetAdminStr(DBT_getLdapBasednInvalidServername), vsid, servername);
        break;
    case LDAPU_ERR_MISSING_RES_ENTRY:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5540, ACL_Program, 3, XP_GetAdminStr(DBT_getLdapBasednDomainNotFound), vsid, servername);
        break;
    case LDAPU_ERR_DOMAIN_NOT_ACTIVE:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5550, ACL_Program, 3, XP_GetAdminStr(DBT_getLdapBasednDomainNotActive), vsid, servername);
        break;
    case LDAPU_ERR_MISSING_ATTR_VAL:
	nserrGenerate(errp, ACLERRFAIL, ACLERR5560, ACL_Program, 3, XP_GetAdminStr(DBT_getLdapBasednNoInetbasedomainAttr), vsid, servername);
        break;
    }

    if (rv == LDAPU_SUCCESS) {
        // cache the value
        ACL_VirtualDbSetAttr(NULL, virtdb, "vsbasedn", buffer);
        //
        // get the attr again to get a pointer that is allocated in the virtdb
        // (which is memory managed properly)
        ACL_VirtualDbGetAttr(NULL, virtdb, "vsbasedn", &vsbasedn);
    } else {
        // in case we had an error, cache the fact that it did not work so that we don't try again over and over.
        // a configuration reload will cause the server to try again
        ACL_VirtualDbSetAttr(NULL, virtdb, "vsbasedn", NULL);
        vsbasedn = NULL;
    }

    // now unlock
    ACL_VirtualDbUnlock(virtdb);

    return vsbasedn;
}


// 
// utility function to populate rq with the request from resource plist.
// if request not available, generate error message and return LAS_EVAIL_FAIL
//
static int get_request(void **rq, PList_t resource, NSErr_t *errp)
{
    int rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, 
                           (void **)rq, NULL);
    if (rv < 0) {
        char rv_str[16];
        sprintf(rv_str, "%d", rv);
        nserrGenerate(errp, ACLERRINVAL, ACLERR5320, ACL_Program, 2, 
                      XP_GetAdminStr(DBT_lasProgramUnableToGetRequest_), 
                      rv_str);
        return LAS_EVAL_FAIL;
    }

    return rv;
}


//
// get_is_valid_password_ldap - attribute getter for ACL_ATTR_IS_VALID_PASSWORD
//
// works for both basic and digest auth.
//
int
get_is_valid_password_ldap (NSErr_t *errp, PList_t subject,
				      PList_t resource, PList_t auth_info,
				      PList_t global_auth, void *unused)
{
    /* If the raw-user-name and raw-user-password attributes are present then
     * verify the password against the LDAP database.
     * Otherwise call AttrGetter for raw-user-name.
     */
    char *raw_user;
    char *raw_pw;
    char *userdn = 0, *udn = 0;
    int rv;
    char *dbname;
    ACLMethod_t authtype, method, acl_auth_method;
    LdapSessionPool *sp;
    LdapSession *ld = NULL;
    pool_handle_t *subj_pool = PListGetPool(subject);
    char *authorization = NULL;
    Request *rq = NULL;

    //
    // the attribute getter for ACL_ATTR_RAW_USER should set ACL_ATTR_AUTHTYPE if it
    //  actually parses an "Authorization" header
    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_USER, (void **)&raw_user,
                          subject, resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
        // we return here if there's no Authorization header, or the Authorization header
        // cannot be parsed properly
        return rv;
    }

    ereport(LOG_VERBOSE, "ldap authdb: Authenticating user [%s]", raw_user);

    rv = ACL_GetAttribute(errp, ACL_ATTR_AUTHTYPE, (void **)&authtype,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE || authtype == ACL_MethodBasic) {
        // no authtype (which can happen if RAW_USER was set from outside)
        // or authtype is, in fact, BasicAuth ==> play BasicAuth.
        (void)ACL_AuthInfoGetMethod(errp, auth_info, &acl_auth_method);
        // but first, check if the ACL does not call for digest authentication or sth else
        // and the client is trying to step-down...
        if (acl_auth_method != ACL_MethodBasic) {
            nserrGenerate(errp, ACLERRINVAL, ACLERR5320, ACL_Program, 1, XP_GetAdminStr(DBT_getIsValidPasswordLdapStepDown));
            return LAS_EVAL_FALSE; // this calls for a 401 (with the correct challenge)
        }
        method = ACL_MethodBasic;
    } else if (authtype == ACL_MethodDigest) {
        // we'll take digest authentication even if the ACL says "basic"
        // however, the database must be capable of handling digestauth...
        // we'll check that later.
        //
        // XXX what do we do if a client sends both BasicAuth and DigestAuth Authorization?
        // this should not really happen because if you send BasicAuth authorization, it
        // doesn't make any sense to send DigestAuth as well (from a security point of view)...
        method = ACL_MethodDigest;
    } else {
        // this should never happen, so I want it to blow up in DEBUG builds
        // otherwise just try to fail as gracefully as possible
        PR_ASSERT(0);
        return LAS_EVAL_FAIL;
    }

    // ok, so now method is either ACL_MethodBasic or ACL_MethodDigest
    //  (that's all we're designed to handle)

    // if we have no ACL_ATTR_RAW_PASSWORD now, there's no reason to continue.
    // (ACL_ATTR_PASSWORD contains the AUTHORIZATION header for digest)
    if ((rv = PListGetValue(subject, ACL_ATTR_RAW_PASSWORD_INDEX, (void **)&raw_pw, NULL)) < 0)
        return rv;

    if (method == ACL_MethodDigest) {
        // Get request pblock so we can get the request method
        rv = get_request((void **)&rq, resource, errp);
        if (rv == LAS_EVAL_FAIL) {
            return rv;
        }

    } else {
        if (!raw_pw || !*raw_pw)
            // Null password is not allowed for BasicAuth/LDAP since most LDAP servers let
            // the bind call succeed as anonymous login (with limited privileges).
            return LAS_EVAL_FALSE;
    }

    //
    // Authenticate the raw_user against LDAP database.
    //

    // get the dbname out of the ACL's auth_info
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0) {
	char rv_str[16];
	sprintf(rv_str, "%d", rv);
	nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 2,
                XP_GetAdminStr(DBT_ldapaclUnableToGetDatabaseName), rv_str);
        return LAS_EVAL_FAIL;
    }

    // get the VS's idea of the authentication database
    ACLVirtualDb_t *virtdb;
    ACLDbType_t dbtype;
    void *dbparsed;
    const VirtualServer *vs;
    rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, &vs);
    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                      XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName),
                      dbname);
        return LAS_EVAL_FAIL;
    }

    // get hold of a db handle
    LdapRealm *pRealm = (LdapRealm *)dbparsed;
    sp = pRealm->getSessionPool();

    if (method == ACL_MethodDigest) {
        // we don't need to check on which type of database we are (we're guaranteed to be on LDAP)
        // but we need to fail of we're trying to do DigestAuth on an LDAP database that does
        // not have the plugin...
        if (!pRealm->allowDigestAuth()) {
            nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 3,
                    XP_GetAdminStr(DBT_getIsValidPasswordLdapDigestAuthNotSupported),
                    dbname, dbname);
            return LAS_EVAL_FAIL;
        }
    }

    if (method == ACL_MethodBasic && acl_usr_cache_enabled()) {
        // we do not support the user cache for digest auth - there's no "password"
        // XXX we could store the auth_user <-> userdn association, though...

	/* We have user name and password. */
	/* Check the cache to see if the password is valid */

	rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);
	if (rv >= 0 && userdn)
	if (acl_usr_cache_passwd_check(raw_user, dbname,
                                       raw_pw, userdn) == LAS_EVAL_TRUE)
        {
            ereport(LOG_VERBOSE,
                    "ldap authdb: Using cached authentication for [%s] belonging to userdn [%s]",
                    raw_user, userdn);
            goto done;
        }
    }

    // get ourselves a brand new spanking LDAP session
    ld = sp->get_session();
    const char *basedn;

    // find out our baseDN (this is VS and therefore request specific)
    if ((basedn = get_ldap_basedn(errp, ld, virtdb, vs_get_id(vs), acl_get_servername(resource))) == NULL) {
        // error message is in errp
        sp->free_session(ld);
        return LAS_EVAL_FAIL;
    }

    // delete RSPAUTH property
    PListDeleteProp(resource, ACL_ATTR_RSPAUTH_INDEX, ACL_ATTR_RSPAUTH);

    //
    // first find user DN, then check it against the password
    //
    if ((rv = ld->find_userdn(raw_user, basedn, &userdn)) == LDAPU_SUCCESS) {

        ereport(LOG_VERBOSE, "ldap authdb: Matched [%s] belonging to userdn [%s]",
                raw_user, userdn ? userdn :"null");
        
        if (method == ACL_MethodBasic) {
            /* We should ensure that the raw_user is exactly the same
             * as the uid returned in userdn since a filter has been
             * used while looking for the userdn and this will allow
             * wild card characters in the filter (in this case the
             * raw_user).
             */
            char *uid;
            char *lasts;
            uid = strdup(userdn);
            char *token = util_strtok(uid, "=,", &lasts);
            while (token) {
                if(!strcmp(token, "uid")) {
                    token = util_strtok(NULL, "=,", &lasts);
                    if(token == NULL) {
                        rv = LDAPU_FAILED;
                        break;
                    } else {
                     //ignore trailing spaces for uid value bug #4822462
                       int i;
                       i = strlen(token);
                       while ((--i)>0 && isspace(token[i]) )
                         token[i]=0;
                        if(strcasecmp(raw_user, token)) {
                            rv = LDAPU_FAILED;
                        }
                        break;
                    }
                }
                token = util_strtok(NULL, "=,", &lasts);
            }
            if(uid) free(uid);
            
            // Now that we have found out what the userdn is, we can 
            // use this userdn and uid both to check ACL user cache to find 
            // out if such an entry exists in ACL user cache. ACL user cache 
            // uniquely identifies a user with userdn and uid both.
            // If the user is found, and if password is already stored in
            // ACL user cache, we free all sessions and memory and return,
            // We don't have to make one more LDAP call. 
            // If password is not present or doesn't match in ACL user cache,
            //  we have to eventually check in LDAP server. 
            if (acl_usr_cache_enabled() && rv == LDAPU_SUCCESS) {
                if (acl_usr_cache_passwd_check(raw_user, dbname,
                                               raw_pw, userdn) == LAS_EVAL_TRUE)
                {
                    ereport(LOG_VERBOSE,
                            "ldap authdb: Using cached authentication for [%s] belonging to userdn [%s]",
                            raw_user, userdn ? userdn :"null");
                    sp->free_session(ld);
                    udn = pool_strdup(subj_pool, userdn);
                    ldap_memfree(userdn);
                    userdn = udn;
                    goto done;
                }
            } 

            if (rv == LDAPU_SUCCESS)
                rv = ld->userdn_password(userdn, raw_pw);

        } else {
            // Handle digest auth
            char *raw_realm = NULL;
            char *raw_nonce = NULL;
            char *raw_uri = NULL;
            char *raw_qop = NULL;
            char *raw_nc = NULL;
            char *raw_cnonce = NULL;
            char *raw_cresponse = NULL;
            char *raw_opaque = NULL;
            char *raw_algorithm = NULL;

            if ((rv = parse_digest_user_login(raw_pw, NULL, &raw_realm, &raw_nonce, &raw_uri,
                                               &raw_qop, &raw_nc, &raw_cnonce, &raw_cresponse,
                                               &raw_opaque, &raw_algorithm)) != LAS_EVAL_TRUE)
            {
                sp->free_session(ld);
                return rv;
            }

            /* The fields nonce & response are expected to be prersent in the digest request.
            Returns false, if any one of those fields result in to null value.*/
            if( (raw_nonce == NULL || strlen(raw_nonce)== 0) || (raw_cresponse == NULL || strlen(raw_cresponse)== 0))
            {
                nserrGenerate(errp, ACLERRINVAL, ACLERR5895, ACL_Program, 1, XP_GetAdminStr(DBT_invalidDigestRequest));
                sp->free_session(ld);
                return LAS_EVAL_FAIL;
            }

            char *http_method = pblock_findval("method", rq->reqpb);

            // we always use MD5 as algorithm.
            char *response = NULL;
            rv = ld->userdn_digest(userdn, raw_nonce, raw_cnonce, raw_user, raw_realm,
                                    "iplanetReversiblePassword", "MD5", raw_nc, http_method,
                                    raw_qop, raw_uri, raw_cresponse, &response);
            /* Check for a stale nonce value */
            if (rv == LDAPU_SUCCESS) {
                // check if nonce is stale after we checked whether the authentication would be
                // OK otherwise.
                if (Digest_check_nonce(GetDigestPrivatekey(), raw_nonce, "") == PR_FALSE) {
                    // come back with a 401 and a new WWW-Authenticate header indicating
                    // the nonce used by the client was stale...
                    PListInitProp(resource, ACL_ATTR_STALE_DIGEST_INDEX, ACL_ATTR_STALE_DIGEST, "true", 0);
                    rv = LDAPU_FAILED; /* so we'll free memory, etc */
                } else {
                    PListInitProp(resource, ACL_ATTR_STALE_DIGEST_INDEX, ACL_ATTR_STALE_DIGEST, "false", 0);
                    // this is put into the server headers if present...
                    PListInitProp(resource, ACL_ATTR_RSPAUTH_INDEX, ACL_ATTR_RSPAUTH,
                                  pool_strdup(PListGetPool(subject), response), 0);
                }
            }
            if (response) FREE(response);
        } // end handle digest auth
    }
    sp->free_session(ld);

    if (rv == LDAPU_FAILED || rv == LDAPU_ERR_USER_NOT_ACTIVE) {
        /* user entry not found, user inactive, or incorrect password  */
        ereport(LOG_VERBOSE, "ldap authdb: Authentication failed for [%s] belonging to userdn [%s]",
                raw_user, userdn ? userdn:"null");
        if (userdn) ldap_memfree(userdn);
        return LAS_EVAL_FALSE;
        
    } else if (rv == LDAPU_ERR_PASSWORD_EXPIRED) {

        ereport(LOG_VERBOSE, "ldap authdb: Authentication failed for [%s] "
                "belonging to userdn [%s] (expired)",
                raw_user, userdn ? userdn:"null");

        if (userdn) ldap_memfree(userdn);
        if (rq == NULL) {
            rv = get_request((void **)&rq, resource, errp);
        }

        // Set pb_key_auth_expiring to indicate the expired condition
        // (this flag is consumed in lib/frame/aclframe.cpp and may redirect
        // the request if this auth-db is configured to do so).

        if (rq) {
            pblock_kvinsert(pb_key_auth_expiring, "1", 1, rq->vars);
            if (dbname && pblock_findval("auth-db", rq->vars) == NULL) {
                pblock_nvinsert("auth-db", dbname, rq->vars);
            }
        }
        return LAS_EVAL_PWEXPIRED;

    } else if (rv == LDAPU_ERR_PASSWORD_EXPIRING) {

        ereport(LOG_VERBOSE,"ldap authdb: Password for [%s] belonging "
                "to userdn [%s] will expire soon.",
                raw_user, userdn ? userdn:"null");

        if (rq == NULL) {
            rv = get_request((void **)&rq, resource, errp);
        }

        // Set pb_key_auth_expiring to indicate the expiring condition
        // (this flag is consumed in lib/frame/aclframe.cpp and may redirect
        // the request if this auth-db is configured to do so).

        if (rq) {
            pblock_kvinsert(pb_key_auth_expiring, "1", 1, rq->vars);
        }

        // Note that we'll fall through to the success code path because
        // while the password is expiring, it is still valid. So other than
        // setting the expiring flag above this condition is equivalent to
        // successful authentication.

    } else if (rv != LDAPU_SUCCESS) {
        /* some unexpected LDAP error */
        nserrGenerate(errp, ACLERRFAIL, ACLERR5860, ACL_Program, 2, XP_GetAdminStr(DBT_ldapaclPassworkCheckLdapError), ldapu_err2string(rv));
        ereport(LOG_VERBOSE,
                "ldap authdb: Authentication failed for [%s] belonging to userdn [%s] (ldap error - %s)",
                raw_user,userdn ? userdn:"null", system_errmsg());
        if (userdn) ldap_memfree(userdn);
        return LAS_EVAL_FAIL;
    }

    if (method == ACL_MethodBasic && acl_usr_cache_enabled() && userdn != NULL)
        // make an entry in the cache
        acl_usr_cache_insert(raw_user, dbname, userdn, raw_pw, 0, 0);

    udn = pool_strdup(subj_pool, userdn);
    ldap_memfree(userdn);
    userdn = udn;

done:
    PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX, ACL_ATTR_IS_VALID_PASSWORD, raw_user, 0);
    PListInitProp(subject, ACL_ATTR_USERDN_INDEX, ACL_ATTR_USERDN, userdn, 0);

    ereport(LOG_VERBOSE,
            "ldap authdb: Authentication succeeded for [%s] belonging to userdn [%s]", raw_user, userdn? userdn:"null");

    return LAS_EVAL_TRUE;
}

/* This function should be moved to lib/base/regexp.cpp */
static int regexp_casecmp_len (const char *str, const int slen,
			       const char *exp, const int elen)
{
    char tmp_str[1024];
    char tmp_exp[1024];
    char *cmp_str;
    char *cmp_exp;
    int rv;

    /* Copy str into tmp_str & setup cmp_str */
    if (slen < 1024) {
	cmp_str = tmp_str;
    }
    else {
	cmp_str = (char *)MALLOC(slen+1);
	if (!cmp_str) return -1;
    }

    strncpy(cmp_str, str, slen);
    cmp_str[slen] = 0;

    /* Copy exp into tmp_exp & setup cmp_exp */
    if (slen < 1024) {
	cmp_exp = tmp_exp;
    }
    else {
	cmp_exp = (char *)MALLOC(elen+1);
	if (!cmp_exp) return -1;
    }

    strncpy(cmp_exp, exp, elen);
    cmp_exp[elen] = 0;

    rv = WILDPAT_CASECMP(cmp_str, cmp_exp);

    if (cmp_str != tmp_str) FREE(cmp_str);
    if (cmp_exp != tmp_exp) FREE(cmp_exp);
    return rv;
}

static int acl_grpcmpfn (const void *groupids, const char *group,
			 const int len)
{
    const char *token = (const char *)groupids;
    int tlen;
    char delim = ',';

    while((token = acl_next_token_len(token, delim, &tlen)) != NULL) {
	if (tlen > 0 && !regexp_casecmp_len(group, len, token, tlen))
	{
	    return LDAPU_SUCCESS;
	}
	else if (tlen == 0 || 0 != (token = strchr(token+tlen, delim)))
	    token++;
	else
	    break;
    }

    return LDAPU_FAILED;
}

static int get_user_ismember_helper (NSErr_t *errp, PList_t subject,
				     PList_t resource, PList_t auth_info,
				     PList_t global_auth,
				     const char *userdn, void *cert,
				     char **member_of_out)
{
    int retval;
    int rv;
    char *groups;
    char *member_of = 0;
    char *dbname;

    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_GROUPS, (void **)&groups, subject,
			  resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
	return rv;
    }

    ereport(LOG_VERBOSE,
            "ldap authdb: Is user [%s] in group [%s]?", userdn, groups);

    // get me the database name from the ACL
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0) {
	char rv_str[16];
	sprintf(rv_str, "%d", rv);
	nserrGenerate(errp, ACLERRINVAL, ACLERR5900, ACL_Program, 2, XP_GetAdminStr(DBT_GetUserIsMemberLdapUnabelToGetDatabaseName), rv_str);
        return rv;
    }

    // get the VS's idea of the authentication database
    ACLVirtualDb_t *virtdb;
    ACLDbType_t dbtype;
    void *dbparsed;
    const VirtualServer *vs;
    rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, &vs);
    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR5910, ACL_Program, 2,
                      XP_GetAdminStr(DBT_GetUserIsMemberLdapUnableToGetParsedDatabaseName),
                      dbname);
        return LAS_EVAL_FAIL;
    }

    LdapRealm *pRealm = (LdapRealm *)dbparsed;
    LdapSessionPool *sp = pRealm->getSessionPool();
    LdapSession *ld = sp->get_session();
    const char *basedn;

    // get basedn for this request
    if ((basedn = get_ldap_basedn(errp, ld, virtdb, vs_get_id(vs), acl_get_servername(resource))) == NULL) {
        sp->free_session(ld);
        return LAS_EVAL_FAIL;
    }

    /* check if the user is member of any of the groups */
    rv = ld->usercert_groupids(userdn, cert, groups, acl_grpcmpfn, 
			  basedn, 16 /* max recursion */, &member_of);
    sp->free_session(ld);

    if (rv == LDAPU_SUCCESS) {
      /* User is a member of one of the groups */
      if (member_of) {
	*member_of_out = member_of;
        ereport(LOG_VERBOSE,
                "ldap authdb: User [%s] matched group [%s]",
                userdn, member_of);
        retval = LAS_EVAL_TRUE;
        
      } else {
	/* This shouldn't happen */
	retval = LAS_EVAL_FALSE;
      }
    } else if (rv == LDAPU_FAILED) {
      /* User is not a member of any of the groups */
        ereport(LOG_VERBOSE,
                "ldap authdb: User [%s] is not in group [%s]", userdn, groups);
        retval = LAS_EVAL_FALSE;
      
    } else {
      /* unexpected LDAP error */
      nserrGenerate(errp, ACLERRFAIL, ACLERR5950, ACL_Program, 2,
		    XP_GetAdminStr(DBT_GetUserIsMemberLdapError),
		    ldapu_err2string(rv));
      retval = LAS_EVAL_FAIL;
    }

    return retval;
}

//
// get_user_ismember_ldap - attribute getter for ACL_ATTR_USER_ISMEMBER for LDAP databases
//
int
get_user_ismember_ldap (NSErr_t *errp, PList_t subject,
			    PList_t resource, PList_t auth_info,
			    PList_t global_auth, void *unused)
{
    int rv;
    char *userdn = 0;
    char *member_of = 0;

    DBG_PRINT1("get_user_ismember_ldap\n");

    rv = ACL_GetAttribute(errp, ACL_ATTR_USERDN, (void **)&userdn, subject,
			  resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
	return LAS_EVAL_FAIL;
    }

    rv = get_user_ismember_helper(errp, subject, resource, auth_info,
				  global_auth, userdn, (void *)0 /* cert */, &member_of);
    if (rv == LAS_EVAL_TRUE) {
	PListInitProp(subject, ACL_ATTR_USER_ISMEMBER_INDEX,
		      ACL_ATTR_USER_ISMEMBER,
		      pool_strdup(PListGetPool(subject), member_of), 0);
	ldapu_free(member_of);
    }

    return rv;
}

//
// get_cert2group_ldap - attribute getter for ACL_ATTR_CERT2GROUP
//
int
get_cert2group_ldap (NSErr_t *errp, PList_t subject,
			 PList_t resource, PList_t auth_info,
			 PList_t global_auth, void *unused)
{
    int rv;
    char *userdn = 0;
    char *member_of = 0;
    void *cert = 0;

    DBG_PRINT1("get_cert2group_ldap\n");

    // get hold of the certificate, possibly redoing the SSL handshake
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER_CERT, (void **)&cert, subject,
			  resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
	return rv;
    }

    // use the userdn also if it is there
    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);

    // find all the groups the user is member of
    rv = get_user_ismember_helper(errp, subject, resource, auth_info,
				  global_auth, userdn, cert, &member_of);
    if (rv == LAS_EVAL_TRUE) {
	PListInitProp(subject, ACL_ATTR_CERT2GROUP_INDEX,
		      ACL_ATTR_CERT2GROUP,
		      pool_strdup(PListGetPool(subject), member_of), 0);
	ldapu_free(member_of);
    }

    return rv;
}

//
// acl_map_cert_to_user_ldap - find the user corresponding to a cert
//
// works only for LDAP databases
// This function returns LDAPU error codes so that the caller can call
// ldapu_err2string to get the error string.
int
acl_map_cert_to_user_ldap (NSErr_t *errp, const char *dbname,
			  void *handle, void *cert,
			  PList_t resource, pool_handle_t *pool,
			  char **user, char **userdn, char *certmap)
{
    int rv = LAS_EVAL_FALSE;

    // if we have a certmap, do not use the user cache 
    // the certmap attribute is only for testing purposes
    // if we'd implement it fully, we'd need to index the user cache both by
    // dbname and certmap - a cert could map to a different user if the certmap
    // is different!
    if (acl_usr_cache_enabled() && !certmap) {
	rv = acl_cert_cache_get_uid(cert, dbname, user, userdn, pool);
    }
    if (rv == LAS_EVAL_TRUE)
        return LDAPU_SUCCESS;

    // not found in the cache. do it the hard way.

    // get the VS's idea of the authentication database
    ACLVirtualDb_t *virtdb;
    ACLDbType_t dbtype;
    void *dbparsed;
    const VirtualServer *vs;
    rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, &vs);
    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                      XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName), dbname);
        return LDAPU_ERR_INTERNAL;
    }

    // get session
    LdapRealm *pRealm = (LdapRealm *)dbparsed;
    LdapSessionPool *sp = pRealm->getSessionPool();
    LdapSession *ld = sp->get_session();
    const char *basedn;
    char *dn = 0;
    char *uid;

    // find out our baseDN (this is VS and therefore request specific)
    if ((basedn = get_ldap_basedn(errp, ld, virtdb, vs_get_id(vs), acl_get_servername(resource))) == NULL) {
        // XXX error message
        sp->free_session(ld);
        return LDAPU_ERR_INTERNAL;
    }
    rv = ld->cert_to_user(cert, basedn, &uid, &dn, certmap,0);

    if (rv == LDAPU_SUCCESS) {
        *user = uid ? pool_strdup(pool, uid) : 0;
        if (!*user) rv = LDAPU_ERR_OUT_OF_MEMORY;
        ldapu_free(uid);

        *userdn = dn ? pool_strdup(pool, dn) : 0;
        if (!*userdn) rv = LDAPU_ERR_OUT_OF_MEMORY;
        ldapu_free(dn);

        if (acl_usr_cache_enabled() && !certmap) {
            acl_cert_cache_insert (cert, dbname, *user, *userdn);
        }
    }
    sp->free_session(ld);

    return rv;
}

NSAPI_PUBLIC int
acl_get_default_ldap_db(NSErr_t *errp, void **db)
{
    const char *dbname;
    ACLDbType_t dbtype;
    void *ldb;
    int rv;

    dbname = ACL_DatabaseGetDefault(errp);

    // we use the low level function here.
    rv = ACL_DatabaseFind(errp, dbname, &dbtype, &ldb);
    if (rv != LAS_EVAL_TRUE) {
	nserrGenerate(errp, ACLERRINVAL, ACLERR6000, ACL_Program, 2, XP_GetAdminStr(DBT_LdapDatabaseHandleNotARegisteredDatabase), dbname);
	return LAS_EVAL_FAIL;
    }

    if (!ACL_DbTypeIsEqual(errp, dbtype, ACL_DbTypeLdap)) {
	/* Not an LDAP database -- error */
	nserrGenerate(errp, ACLERRINVAL, ACLERR6010, ACL_Program, 2, XP_GetAdminStr(DBT_LdapDatabaseHandleNotAnLdapDatabase), dbname);
	return LAS_EVAL_FAIL;
    }
    *db = ldb;
    return LAS_EVAL_TRUE;
}

/*
 * ACL_LDAPDatabaseHandle -
 *   Finds the internal structure representing the 'dbname'.  If it is an LDAP
 * database, returns the 'LDAP *ld' pointer.  Also, binds to the LDAP server.
 * The LDAP *ld handle can be used in calls to LDAP API.
 * Returns LAS_EVAL_TRUE if successful, otherwise logs an error in
 * LOG_SECURITY and returns LAS_EVAL_FAIL.
 *
 * This function is obsolete as of ES4.0
 */
int ACL_LDAPDatabaseHandle (NSErr_t *errp, const char *dbname, LDAP **ld, char **basedn)
{
    ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_ldapACLDBHandleNotSupported), dbname ? dbname : "(null)");
    return LAS_EVAL_FAIL;
}

/*
 * ACL_LDAPSessionAllocate
 *
 * This function should be used instead of ACL_LDAPDatabaseHandle, as of ES 4.0.
 * It returns an (LDAP *) handle for an LDAP session, and this handle can be
 * used by the caller to access LDAP SDK functions.  Note that the caller must
 * now call ACL_LDAPSessionFree() when the session is no longer needed, normally
 * the calling function returns.
 *
 * Parameters:
 * errp   - Stack of errors to be returned to caller
 * dbid   - The database name as in the <auth-db> element in server.xml.  If
 *          this is NULL, it defaults to the default database name.
 * ld     - the LDAP handle will be returned in this.
 * basedn - basedn to be used in LDAP API calls returned in this.  If this is
 *          UNLL, it means that the caller is not interested in the basedn
 *
 * Return Values - LAS_EVAL_TRUE and LAS_EVAL_FAIL
 */ 
int
ACL_LDAPSessionAllocate(NSErr_t *errp, const char *dbname, LDAP **ld, const char **basedn)
{
    int rv; 

    *ld = 0;

    const VirtualServer *vs = conf_get_vs();
    if(vs == 0) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR6500, ACL_Program, 1, XP_GetAdminStr(DBT_LDAPSessionAllocateNoAssociatedVS));
        return LAS_EVAL_FAIL;
    } 

    if (!dbname || !*dbname)
        dbname = ACL_DatabaseGetDefault(errp);

    ACLVirtualDb_t *virtdb;
    rv = ACL_VirtualDbLookup(errp, vs, dbname, &virtdb);
    if(rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6510, ACL_Program, 3, XP_GetAdminStr(DBT_LDAPSessionAllocateNotARegisteredVirtDb), dbname, vs_get_id(vs));
        return LAS_EVAL_FAIL;
    }

    ACLDbType_t dbtype;
    ACL_VirtualDbGetDbType(NULL, virtdb, &dbtype);
    if (!ACL_DbTypeIsEqual(errp, dbtype, ACL_DbTypeLdap)) {
        /* Not an LDAP database -- error */
        nserrGenerate(errp, ACLERRINVAL, ACLERR6530, ACL_Program, 2, XP_GetAdminStr(DBT_LDAPSessionAllocateNotAnLdapDatabase), dbname);
        return LAS_EVAL_FAIL;
    }

    LdapRealm *realm;
    ACL_VirtualDbGetParsedDb(NULL, virtdb, (void **)&realm);

    LdapSessionPool *sp = realm->getSessionPool();
    LdapSession *session = sp->get_session();
    session->bindAsDefault();

    *ld = session->getSession();

    if(basedn) {
        const char *servername = 0;
        servername = conf_getglobals()->Vserver_hostname;
        *basedn = get_ldap_basedn(errp, session, virtdb, vs_get_id(vs), servername);
    }
   
    return LAS_EVAL_TRUE;
}

/*
 * ACL_LDAPSessionFree
 *
 * Frees the ldap session associated with ld
 *
 */
void
ACL_LDAPSessionFree(LDAP *ld)
{
    LdapSessionPool::free_ldap_session(ld);
}


int
ACL_NeedLDAPOverSSL ()
{
    return need_ldap_over_ssl;
}

//
// get_userdn_ldap - attribute getter for ACL_ATTR_USERDN for LDAP dbs
//
int
get_userdn_ldap (NSErr_t *errp, PList_t subject,
		     PList_t resource, PList_t auth_info,
		     PList_t global_auth, void *unused)
{
    char *uid;
    char *dbname;
    char *userdn;
    pool_handle_t *subj_pool = PListGetPool(subject);
    int rv;
    
    // get authenticated user
    rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&uid, subject,
			  resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) {
        // if we don't have it, there's no way to get to a user DN
	return LAS_EVAL_FAIL;
    }

    // the getter for ACL_ATTR_USER may have put the USERDN on subject already
    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);
    if (rv >= 0) {
	// in this case, we're done
	return LAS_EVAL_TRUE;
    }

    // ok, we have to do it on foot.

    // get hold of authentication database name from the ACL
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0) {
	char rv_str[16];
	sprintf(rv_str, "%d", rv);
	nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 2,
		      XP_GetAdminStr(DBT_ldapaclUnableToGetDatabaseName), rv_str);
	return LAS_EVAL_FAIL;
    }

    // get the VS's idea of the authentication database
    ACLVirtualDb_t *virtdb;
    ACLDbType_t dbtype;
    void *dbparsed;
    const VirtualServer *vs;
    rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, &vs);
    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                      XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName), dbname);
        return rv;
    }

    const char *basedn;

    // get session
    LdapRealm *pRealm = (LdapRealm *)dbparsed;
    LdapSessionPool *sp = pRealm->getSessionPool();
    LdapSession *ld = sp->get_session();

    // find out our baseDN (this is VS and therefore request specific)
    if ((basedn = get_ldap_basedn(errp, ld, virtdb, vs_get_id(vs), acl_get_servername(resource))) == NULL) {
        // XXX error message
        sp->free_session(ld);
        return LAS_EVAL_FAIL;
    }
    rv = ld->find_userdn(uid, basedn, &userdn);
    sp->free_session(ld);

    if (rv == LDAPU_SUCCESS) {
        // Found it.  Store it in the user cache as well
        if (acl_usr_cache_enabled() && userdn != NULL) {
            acl_usr_cache_insert(uid, dbname, userdn, 0, 0, 0);
        }
        PListInitProp(subject, ACL_ATTR_USERDN_INDEX, ACL_ATTR_USERDN, pool_strdup(subj_pool, userdn), 0);
        ldapu_free(userdn);
        rv = LAS_EVAL_TRUE;
    } else if (rv == LDAPU_FAILED) {
        // Not found but not an error
        rv = LAS_EVAL_FALSE;
    } else {
        // unexpected LDAP error
        nserrGenerate(errp, ACLERRFAIL, ACLERR5860, ACL_Program, 2, XP_GetAdminStr(DBT_ldapaclPassworkCheckLdapError), ldapu_err2string(rv));
        rv = LAS_EVAL_FAIL;
    }
    return rv;
}

//
// get_user_exists_ldap - attribute getter for ACL_ATTR_USER_EXISTS for LDAP dbs
//
int
get_user_exists_ldap (NSErr_t *errp, PList_t subject,
			  PList_t resource, PList_t auth_info,
			  PList_t global_auth, void *unused)
{
    int rv;
    char *user;
    char *userdn;

    // check if we have a cached userDN
    rv = PListGetValue(subject, ACL_ATTR_USERDN_INDEX, (void **)&userdn, NULL);

    if (rv >= 0) {
        // we found a cached user DN
        // however, it may be stale, so check with the LDAP db if it's still valid
	char *dbname;
	const char *some_attrs[] = { "c", 0 };

        // get hold of the authentication database name from the ACL
	rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
	if (rv < 0) {
	    char rv_str[16];
	    sprintf(rv_str, "%d", rv);
	    nserrGenerate(errp, ACLERRFAIL, ACLERR5830, ACL_Program, 2,
			  XP_GetAdminStr(DBT_ldapaclUnableToGetDatabaseName), rv_str);
	    return LAS_EVAL_FAIL;
	}

        // get the VS's idea of the authentication database
        ACLVirtualDb_t *virtdb;
        ACLDbType_t dbtype;
        void *dbparsed;
        rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, NULL);
        if (rv != LAS_EVAL_TRUE) {
            nserrGenerate(errp, ACLERRFAIL, ACLERR5840, ACL_Program, 2,
                          XP_GetAdminStr(DBT_ldapaclUnableToGetParsedDatabaseName), dbname);
	    return rv;
	}

        // now do an LDAP base search on the user DN to see if it's still around
        // we do not need to get the baseDN here as the userdn is absolute
        // XXX if it's not, we have to append the osisuffix here
	LdapSearchResult *res;
	LdapRealm *pRealm = (LdapRealm *)dbparsed;
	LdapSessionPool *sp = pRealm->getSessionPool();
	LdapSession *ld	 = sp->get_session();

        ereport(LOG_VERBOSE,
                "ldap authdb: verifying whether user [%s] exists", userdn);

	rv = ld->find(userdn, LDAP_SCOPE_BASE, NULL, some_attrs, 1, res);
	delete res;
	sp->free_session(ld);

	if (rv == LDAPU_SUCCESS) {
	    // bingo
	    rv = LAS_EVAL_TRUE;
	} else if (rv == LDAPU_FAILED) {
            // not found.
	    rv = LAS_EVAL_FALSE;
	} else {
	    // unexpected LDAP error - blow up
	    nserrGenerate(errp, ACLERRFAIL, ACLERR5860, ACL_Program, 2, XP_GetAdminStr(DBT_ldapaclPassworkCheckLdapError), ldapu_err2string(rv));
	    rv = LAS_EVAL_FAIL;
	}
    } else {
        // no cached user DN
        // try to get it from LDAP

	// for this, we must at least have an user
	rv = PListGetValue(subject, ACL_ATTR_USER_INDEX, (void **)&user, NULL);
	if (rv < 0) {
	    // We don't even have a user name
	    return LAS_EVAL_FAIL;
	}

        ereport(LOG_VERBOSE,
                "ldap authdb: verifying whether user [%s] exists", user);

        // associate user DN to user
	rv = ACL_GetAttribute(errp, ACL_ATTR_USERDN, (void **)&userdn, subject,
			      resource, auth_info, global_auth);

        // if we got the userDN freshly from LDAP, then the user exists
    }

    // if all went well, finally, do what we came in for - 
    // set the ACL_ATTR_USER_EXISTS attribute to the userDN
    if (rv == LAS_EVAL_TRUE) {
	PListInitProp(subject, ACL_ATTR_USER_EXISTS_INDEX, 
                      ACL_ATTR_USER_EXISTS, userdn, 0);
        ereport(LOG_VERBOSE, "ldap authdb: user [%s] exists", userdn);
    } else {
        ereport(LOG_VERBOSE, "ldap authdb: user [%s] does not exist", 
                (userdn != NULL) ? userdn : user);
    }

    return rv;
}

//
// get_user_isinrole_ldap - attribute getter for ACL_ATTR_USER_ISINROLE for LDAP databases
//
int
get_user_isinrole_ldap (NSErr_t *errp, PList_t subject,
			    PList_t resource, PList_t auth_info,
			    PList_t global_auth, void *unused)
{
    int rv;
    char *userdn = 0;
    char *in_role = 0;
    int retval;
    char *roles;
    char *dbname;

    // well, if USERDN isn't set yet, there's no way.
    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_USERDN, (void **)&userdn, subject,
			  resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
	return LAS_EVAL_FAIL;
    }

    if ((rv = ACL_GetAttribute(errp, ACL_ATTR_ROLES, (void **)&roles, subject,
			  resource, auth_info, global_auth)) != LAS_EVAL_TRUE)
    {
	return rv;
    }

    // get me the database name from the ACL
    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);
    if (rv < 0) {
	char rv_str[16];
	sprintf(rv_str, "%d", rv);
	nserrGenerate(errp, ACLERRINVAL, ACLERR5900, ACL_Program, 2, XP_GetAdminStr(DBT_GetUserIsInRoleLdapUnableToGetDatabaseName), rv_str);
        return rv;
    }

    // get the VS's idea of the authentication database
    ACLVirtualDb_t *virtdb;
    ACLDbType_t dbtype;
    void *dbparsed;
    const VirtualServer *vs;
    rv = ACL_VirtualDbFind(errp, resource, dbname, &virtdb, &dbtype, &dbparsed, &vs);
    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR5910, ACL_Program, 2,
                      XP_GetAdminStr(DBT_GetUserIsInRoleLdapUnableToGetParsedDatabaseName),
                      dbname);
        return LAS_EVAL_FAIL;
    }

    LdapRealm *pRealm = (LdapRealm *)dbparsed;
    LdapSessionPool *sp = pRealm->getSessionPool();
    LdapSession *ld = sp->get_session();
    const char *basedn;

    // get basedn for this request
    if ((basedn = get_ldap_basedn(errp, ld, virtdb, vs_get_id(vs), acl_get_servername(resource))) == NULL) {
        sp->free_session(ld);
        return LAS_EVAL_FAIL;
    }

    /* check if the user is in any of the roles */
    rv = ld->user_roleids(userdn, roles, basedn, &in_role);
    sp->free_session(ld);

    if (rv == LDAPU_SUCCESS) {
      /* User is in at least one of the roles */
      if (in_role) {
	PListInitProp(subject, ACL_ATTR_USER_ISINROLE_INDEX,
		      ACL_ATTR_USER_ISINROLE,
		      pool_strdup(PListGetPool(subject), in_role), 0);
	ldapu_free(in_role);
	retval = LAS_EVAL_TRUE;
      } else {
	/* This shouldn't happen */
	retval = LAS_EVAL_FALSE;
      }
    } else if (rv == LDAPU_FAILED) {
      /* User is not in any of the roles */
      retval = LAS_EVAL_FALSE;
    } else {
      /* unexpected LDAP error */
      nserrGenerate(errp, ACLERRFAIL, ACLERR5950, ACL_Program, 2,
		    XP_GetAdminStr(DBT_GetUserIsInRoleLdapError), ldapu_err2string(rv));
      retval = LAS_EVAL_FAIL;
    }

    return retval;
}
