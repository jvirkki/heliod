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
 * Implements the PAM auth-db.
 *
 *
 * Initialization
 * ==============
 *  Done by pamacl_parse_url. See function comments for details.
 *
 *
 * Attribute Getters:
 * ==================
 *  PAM authdb defines several ACLAttrGetterFn_t functions, which are
 *  registered in safs/init.cpp during startup:
 *
 *     - ACL_ATTR_IS_VALID_PASSWORD : pam_authenticate_user
 *     - ACL_ATTR_USER_ISMEMBER     : pam_user_ismember_get
 *     - ACL_ATTR_USER_EXISTS       : pam_userexists_get
 *
 *  Parameters to attribute getters are given below. acldef.h has list
 *  of valid plist attr names, but haven't found documentation on which
 *  can be expected to be where based on what state.
 *
 *     - errp: Error frames
 *     - subject: Property list for subject (p.22)
 *     - resource: Property list for resource (p.22)
 *     - auth_info: Property list containing auth info for attribute
 *          currently being evaluated (p.22)
 *     - global_auth: Property list containing auth info for all attrs (p.22)
 *     - arg: Pointer to additional information. This is the same pointer
 *          that was passed to ACL_AttrGetterRegister() when this getter
 *          was registered. For PAM authdb that's always NULL.
 *
 *  Return values:
 *
 *     As a side effect, the getter may add or remove values from the
 *     various plists. These changes don't seem to be fully documented
 *     in general and some of the existing documentation is wrong. See
 *     p.26 for some (not all) types. Also see each functions comments
 *     for observed behavior.
 * 
 *     - LAS_EVAL_TRUE : Success.
 *     - LAS_EVAL_FALSE : Cannot get the attribute.
 *     - LAS_EVAL_INVALID : Some input was invalid, can't process.
 *     - LAS_EVAL_FAIL : Error, resulting in failure.
 *     - LAS_EVAL_DECLINE : Pass. Additional getters for the same attribute
 *          (if any) are called. PAM authdb doesn't use this.
 *
 *  See pp. 55, 100 of ACPG for more info.
 *
 *
 * Error codes are in libaccess/aclerror.h.
 * Error strings in libaccess/dbtlibaccess.h
 *
 * See also Access Control Programmer's Guide (ACPG)
 * http://docs-pdf.sun.com/816-5643-10/816-5643-10.pdf
 *
 * */

#include <base/nsassert.h>
#include <base/shexp.h>
#include <base/ereport.h>
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/pamacl.h>
#include <libaccess/pamauth.h>
#include "usrcache.h"
#include "aclpriv.h"




/*--------------------------------------------------------------------------
 * pamacl_parse_url is registered as the parsing function for the PAM auth-db
 * in aclinit.cpp.  This will be called during startup when the auth-dbs
 * are initialized, for each auth-db of type pam (or more precisely, for
 * those which the url is "pam").
 *
 * This function is of type DbParseFn_t - see p.181 of ACPG
 *
 * PAM auth-db doesn't currently support any parameters so there isn't much
 * for this function to do.
 *
 * PARAMS:
 *  - errp: error frames
 *  - dbtype: The dbtype identifier that was assigned to PAM auth-db during
 *       ACL_DbTypeRegister.
 *  - name: Name of auth-db. This contains the name of this auth-db from
 *       the server config and appended to it ":N" where N is an int.
 *  - url: URL from server config (should be "pam")
 *  - plist: Property list of parameters to this auth-db, from server config.
 *       PAM auth-db doesn't support any properties currently.
 *  - db: OUT: Set to point to an arbitrary data structure which this
 *       auth-db needs to store data. This pointer can be obtained later
 *       in getter functions from the 'resource' PList using the key
 *       ACL_ATTR_DATABASE_INDEX. PAM authdb doesn't need to store any data
 *       currently so just sets this to NULL.
 *
 * RETURN:
 *  - 0 if successful, or a negative value if unsuccessful.
 *
 */
int pam_parse_url(NSErr_t *errp, ACLDbType_t dbtype, const char *name,
                  const char *url, PList_t plist, void **db)
{
    // This shouldn't be getting called for an auth-db other than PAM
    NS_ASSERT(dbtype == ACL_DbTypePAM);

    // A name and url should've been sent
    NS_ASSERT(name != NULL);
    NS_ASSERT(*name != 0);
    NS_ASSERT(url != NULL);
    NS_ASSERT(*url != 0);

    // PAM authdb URL is always just "pam"
    if (strcmp(PAM_AUTHDB_URL, url)) {
        nserrGenerate(errp, ACLERRSYNTAX, ACLERR6600, ACL_Program, 1,
                      XP_GetAdminStr(DBT_pamDbUrlIsWrong));
      return(-1);
    }

    // If server isn't root, this authdb probably won't work
    if (geteuid() != 0) {
        ereport(LOG_WARN, XP_GetAdminStr(DBT_pamNotRoot));
    }
    
    // See db comments above. PAM authdb doesn't need this currently.
    *db = NULL;

    return(0);                   // Success.
}


/*-----------------------------------------------------------------------------
 * This is the PAM authdb getter for ACL_ATTR_IS_VALID_PASSWORD. See p.38.
 * See file comments for getter function parameter info.
 * This gets called from lib/safs/auth.cpp get_auth_user_basic() in order
 * to authenticate a (user,pwd) when authdb is PAM.
 *
 * GETS:
 * =====
 *    - ACL_ATTR_RAW_USER : Expected to contain user sent from client.
 *    - ACL_ATTR_RAW_PASSWORD : Expected to contain pwd sent from client.
 *    - ACL_ATTR_AUTHTYPE : Expected to contain auth method sent from client.
 *         It better be BASIC since PAM authdb can't handle others.
 *
 * SETS:
 * =====
 *    - subject.ACL_ATTR_IS_VALID_PASSWORD=uid : when returning LAS_EVAL_TRUE
 *      Note error in doc: It says to set this attr to "true" - that doesn't
 *      work. Must set it to user name instead, so we do that here.
 *
 */
int pam_authenticate_user(NSErr_t *errp, PList_t subject, PList_t resource,
                          PList_t auth_info, PList_t global_auth, void *arg)
{
    int rv = 0;
    int cache_auth = 0;
    int cache_enabled = 0;
    char* user = NULL;
    char* userdn = NULL;
    char* password = NULL;
    char* dbname = NULL;
    ACLMethod_t method;
    
    // Get the username sent by client

    rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_USER, (void **)&user,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) { return rv; }
    if (!user || !*user) { return LAS_EVAL_INVALID; }

    ereport(LOG_VERBOSE, "pam authdb: Authenticating user [%s]", user);

    // Get the supplied password

    rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_PASSWORD, (void **)&password,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE) { return rv; }
    if (!password) { return LAS_EVAL_INVALID; }

    // If cache enabled, try there first to avoid going to PAM

    cache_enabled = acl_usr_cache_enabled();
    if (cache_enabled) {
        ACL_AuthInfoGetDbname(auth_info, &dbname);
        pool_handle_t *subj_pool = PListGetPool(subject);
        // for pam auth db, userdn is same as userid
        if (acl_usr_cache_passwd_check(user, dbname, password, user)
                                        == LAS_EVAL_TRUE)
        {
            cache_auth = 1;
        }
    }

    if (cache_auth || !pamauth_authenticate(user, password)) {
        // Set this attribute which indicates to code elsewhere that
        // authentication succeeded (p.38).
        PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX,
                      ACL_ATTR_IS_VALID_PASSWORD, user, 0);

        if (!cache_auth) {
            ereport(LOG_VERBOSE,
                    "pam authdb: Authentication succeeded for [%s]", user);
        } else {
            ereport(LOG_VERBOSE,
                    "pam authdb: Using cached authentication for [%s]", user);
        }

        // if we didn't find it in cache (and cache enabled), save it there
        if (!cache_auth && cache_enabled) {
            acl_usr_cache_insert(user, dbname, user, password, 0, 0);
        }

        return LAS_EVAL_TRUE;
    }

    ereport(LOG_VERBOSE, "pam authdb: Authentication failed for [%s]", user);

    return LAS_EVAL_FALSE;
}


/*-----------------------------------------------------------------------------
 * This is the PAM authdb getter for ACL_ATTR_USER_ISMEMBER. See p.44.
 * See file comments for getter function parameter info.
 * This gets called from libaccess/lasgroup.cpp LASGroupEval() when
 * evaluating an ACL which checks 'group' membership for an authenticated
 * user.
 *
 * GETS:
 * =====
 *    - ACL_ATTR_USER: Expected to contain already-authenticated user name.
 *    - ACL_ATTR_GROUPS: Get the group(s) (might be comma-separated list)
 *         to check whether user is a member of any of them.
 *
 * SETS:
 * =====
 *    - subject.ACL_ATTR_USER_ISMEMBER: If membership matched, this is
 *      set to the group which just matched (if multiple groups might've
 *      matched it is set to the first match found). Note that this
 *      behavior is consistent with other existing authdb's but
 *      inconsistent with the documentation (p.45).
 *
 */
int pam_user_ismember_get(NSErr_t *errp, PList_t subject, PList_t resource,
                          PList_t auth_info, PList_t global_auth, void *arg)
{
    int rv;
    char* user = NULL;
    char* groups = NULL;
    char* grouplist = NULL;
    
    // Get the previously authenticated user from ACL_ATTR_USER

    rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE || !user || !*user) {
        return LAS_EVAL_FAIL;
    }

    // Now get the group(s) to check. Note that this passes the string
    // from the ACL 'group = "*"' as-is (but see 6226967). So, if the
    // ACL entry has a comma-separated list of groups, we'll get
    // that. Also worth noticing, if the ACL has multiple lines for
    // groups, this function gets called once for each line.

    rv = ACL_GetAttribute(errp, ACL_ATTR_GROUPS, (void **)&groups,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE || !groups || !*groups) {
        return LAS_EVAL_FAIL;
    }

    ereport(LOG_VERBOSE,
            "pam authdb: Is user [%s] in group [%s]?", user, groups);

    // Retrieve the list of groups for 'user'

    if ((rv = pamauth_get_groups_for_user(user, &grouplist)) == -1) {
        return LAS_EVAL_FAIL;
    }
    ereport(LOG_VERBOSE,
            "pam authdb: All groups for user [%s]: [%s]", user, grouplist);

    // If user is not in any group, we're done here.
    
    if (!grouplist) {
        ereport(LOG_VERBOSE,
                "pam authdb: User [%s] is not in any group", user);
        return LAS_EVAL_FALSE;
    }
    
    // Find group matching target(s), if any
    
    char * match = acl_attrlist_match(groups, grouplist);

    // If membership matches, set ACL_ATTR_USER_ISMEMBER. See fn comments.

    if (match) {
        PListInitProp(subject, ACL_ATTR_USER_ISMEMBER_INDEX,
                      ACL_ATTR_USER_ISMEMBER, STRDUP(match), 0);
        rv = LAS_EVAL_TRUE;
        ereport(LOG_VERBOSE,
                "pam authdb: User [%s] matched group [%s]", user, match);
        FREE(match);
        
    } else {
        rv = LAS_EVAL_FALSE;
        ereport(LOG_VERBOSE,
                "pam authdb: User [%s] is not in group [%s]", user, groups);
    }

    
    FREE(grouplist);
    
    return rv;
}


/*----------------------------------------------------------------------------
 * This is the PAM authdb getter for ACL_ATTR_USER_EXISTS. Not documented
 * in ACPG. See file comments for getter function parameter info.
 *
 * This gets called (indirectly) from nativerealm_check_group() when 
 * NativeRealm is in use and a group membership check is being done for
 * a user.
 *
 * GETS:
 * =====
 *    - subject.ACL_ATTR_USER_INDEX: Expected to contain user to check.
 *
 * SETS:
 * =====
 *    - subject.ACL_ATTR_USER_EXISTS = user : when returning LAS_EVAL_TRUE
 *
 */
int pam_userexists_get(NSErr_t *errp, PList_t subject,
                       PList_t resource, PList_t auth_info,
                       PList_t global_auth, void *unused)
{
    int rv;
    char* user;
    char* dbname = NULL;

    rv = PListGetValue(subject, ACL_ATTR_USER_INDEX, (void **)&user, NULL);
    if (rv < 0) {  // We don't even have a user name
        return LAS_EVAL_FAIL;
    }    

    ereport(LOG_VERBOSE,
            "pam authdb: verifying whether user [%s] exists", user);

    // Try the cache if possible. In PAM auth-db userdn is same as uid
    // so using the user_check function. If an entry for this user
    // is found in cache, it'll match.

    if (acl_usr_cache_enabled()) {
        ACL_AuthInfoGetDbname(auth_info, &dbname);
        rv = acl_usr_cache_user_check(user, dbname, user);

        if (rv == LAS_EVAL_TRUE) {
            ereport(LOG_VERBOSE, "pam authdb: user [%s] exists in cache",user);
            goto success;
        }
    }

    // If no cache or cache didn't work, check with PAM

    rv = pamauth_userexists(user);

    switch(rv) {

    case LAS_EVAL_FAIL:
        return LAS_EVAL_FAIL;

    case LAS_EVAL_FALSE:
        ereport(LOG_VERBOSE, "pam authdb: user [%s] does not exist", user);
        return LAS_EVAL_FALSE;

    case LAS_EVAL_TRUE:
        ereport(LOG_VERBOSE, "pam authdb: user [%s] exists", user);
    success:
        PListInitProp(subject, ACL_ATTR_USER_EXISTS_INDEX,
                      ACL_ATTR_USER_EXISTS, user, 0);
        return LAS_EVAL_TRUE;
    }
}

