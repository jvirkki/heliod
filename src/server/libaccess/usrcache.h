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


#ifndef ACL_USER_CACHE_H
#define ACL_USER_CACHE_H

#include "plhash.h"
#include <sys/types.h>
#include <time.h>
#include "cert.h"
#include <prclist.h>

/*
 * User/group cache entry
 *
 * Note that this object is dynamically sized to contain the specified maximum
 * number of groups per user.  Accordingly, the 'groups' array must always be
 * the last element.
 */
typedef struct {
    PRCList list;		/* pointer to next & prev obj */
    char *uid;			/* uid and userdn combination is unique within a database */
    char *userdn;		/* LDAP DN if using LDAP db */
    char *passwd;		/* password */
    SECItem *derCert;		/* raw certificate data */
    time_t time;		/* last time when the cache was validated */
    PRHashTable *hashtable;	/* hash table where this obj is being used */
    /*** Keep this last! ***/
    char *groups[1];		/* groups recently checked for membership */
} UserCacheObj;

/* Default number of UserCacheObj in the user/group cache */
#define DEFAULT_MAX_USER_CACHE 200

/* Default number of groups cached in a single UserCacheObj */
#define DEFAULT_MAX_GROUP_CACHE 4

NSPR_BEGIN_EXTERN_C

/* Is the cache enabled? */
extern int acl_usr_cache_enabled();

/* initialize user cache */
extern int acl_usr_cache_init ();

/* Creates a new user obj entry */
extern int acl_usr_cache_insert (const char *uid, const char *dbname,
				 const char *dn, const char *passwd,
				 const char *group, const SECItem *derCert);

/* Add group to the user's cache obj. */
extern int acl_usr_cache_set_group (const char *uid, void *cert_in,
				    const char *dbname, const char *group, 
				    const char *userdn);

/* Returns LAS_EVAL_TRUE if the user's password matches */
extern int acl_usr_cache_passwd_check (const char *uid, const char *dbname,
				       const char *passwd, const char *dn);

/* Returns LAS_EVAL_TRUE if the user is a member of one of the groups */
extern int acl_usr_cache_groups_check (char **uidP, void *cert_in,
				       const char *dbname, const char *groups,
				       char delim, const char *userdn,
				       pool_handle_t *pool);

/* Returns LAS_EVAL_TRUE if the user exists in the cache */
extern int acl_usr_cache_user_check (const char *uid, const char *dbname,
				       const char *userdn);

/* Creates a new user obj entry for cert to user mapping */
extern int acl_cert_cache_insert (void *cert, const char *dbname,
				  const char *uid, const char *dn);

/* Returns LAS_EVAL_TRUE if the user's cache is valid and returns uid */
extern int acl_cert_cache_get_uid (void *cert, const char *dbname,
				   char **uid, char **dn, pool_handle_t *pool);

NSPR_END_EXTERN_C


#endif /* ACL_USER_CACHE_H */
