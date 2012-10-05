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

// Define this because for some stupid reason the client
// defines don't properly determine this is C++
#ifndef XP_CPLUSPLUS
#define XP_CPLUSPLUS
#endif
#include "secitem.h"
#include "cert.h"

#include <netsite.h>
#include "time/nstime.h"
#include <base/crit.h>
#include <base/ereport.h>
#include <frame/conf.h>
#include <frame/conf_api.h>
#include <ldaputil/errors.h>
#include <libaccess/acl.h>
#include <libaccess/aclproto.h>
#include "usrcache.h"
#include "aclpriv.h"
#include <libaccess/dbtlibaccess.h>
#include "base/util.h"       /* util_sprintf */

/* uid and userdn combination is unique within a database. cert is also unique
 * within a database. Use either cert or (userdn and uid) as keys to store 
 * entries in user cache tables. The user cache tables are stored per
 * database.  The following table maps a database name to the corresponding
 * user cache table.  The user cache table is another hash table which stores
 * the UserCacheObj instances.
 */
static PRHashTable *databaseUserCacheTable = 0;
static time_t acl_usr_cache_lifetime = 0;
static PRCList *usrobj_list = 0;
static int num_usrobj = DEFAULT_MAX_USER_CACHE;
static CRITICAL usr_hash_crit = NULL;	/* Controls user cache hash tables & */
					/* usrobj link list */
static pool_handle_t *usrcache_pool = NULL;

static int usrcache_ngroups = DEFAULT_MAX_GROUP_CACHE;
static int usrcache_usrcacheobj_sz = sizeof(UserCacheObj);

static PRBool usrcache_invalidate = PR_TRUE;

#ifdef DEBUG
static int usrcache_loguse = 0;
#endif /* DEBUG */

#define USEROBJ_PTR(l) \
    ((UserCacheObj*) ((char*) (l) - offsetof(UserCacheObj, list)))

static void user_hash_crit_enter (void)
{
    /* Caching may be disabled (usr_hash_crit will be NULL) */
    if (usr_hash_crit) crit_enter(usr_hash_crit);
}

static void user_hash_crit_exit (void)
{
    /* Caching may be disabled (usr_hash_crit will be NULL) */
    if (usr_hash_crit) crit_exit(usr_hash_crit);
}

static void user_hash_crit_init (void)
{
    usr_hash_crit = crit_init();
}

static PRHashNumber
usr_cache_hash_cert(const void *key)
{
    PRHashNumber h;
    const unsigned char *s;
    unsigned int i = 0;
    SECItem *derCert = (SECItem *)key;
    unsigned int len = derCert->len;
 
    h = 0;
    for (s = (const unsigned char *)derCert->data; i < len; s++, i++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}
 
static PRHashNumber
usr_cache_hash_fn (const void *key)
{
    UserCacheObj *usrObj = (UserCacheObj *)key;

    if (usrObj->derCert)
	return usr_cache_hash_cert(usrObj->derCert);
    else if (usrObj->userdn) {
        char *fulluid = NULL;
        int len= strlen(usrObj->uid) + strlen(usrObj->userdn) + 2;
        fulluid = (char *)MALLOC(len);
        util_sprintf(fulluid,"%s@%s", usrObj->uid, usrObj->userdn);
        fulluid[len-1] = '\0';
        PRHashNumber h = ACLPR_HashCaseString(fulluid);
        FREE(fulluid);
        return h;
    } else {
	PR_ASSERT(usrObj->userdn != NULL);
	PR_ASSERT(usrObj->uid != NULL);
        return LAS_EVAL_FALSE;
    }
}

static int
usr_cache_compare_certs(const void *v1, const void *v2)
{
    const SECItem *c1 = (const SECItem *)v1;
    const SECItem *c2 = (const SECItem *)v2;

    return (c1->len == c2 ->len && !memcmp(c1->data, c2->data, c1->len));
}
 
static int
usr_cache_compare_fn(const void *v1, const void *v2)
{
    UserCacheObj *usrObj1 = (UserCacheObj *)v1;
    UserCacheObj *usrObj2 = (UserCacheObj *)v2;

    if (usrObj1->derCert && usrObj2->derCert)
	return usr_cache_compare_certs(usrObj1->derCert, usrObj2->derCert);
    else if (!usrObj1->derCert && !usrObj2->derCert && 
             usrObj1->uid && usrObj2->uid && 
             usrObj1->userdn && usrObj2->userdn)
	return (ACLPR_CompareCaseStrings(usrObj1->uid, usrObj2->uid) && 
	        ACLPR_CompareCaseStrings(usrObj1->userdn, usrObj2->userdn));
    else {
	PR_ASSERT(usrObj1->uid && usrObj2->uid && 
	          usrObj1->userdn && usrObj2->userdn);
	return LAS_EVAL_FALSE;
	}
}

static PRHashTable *alloc_db2uid_table ()
{
    return PR_NewHashTable(0, 
			   usr_cache_hash_fn,
			   usr_cache_compare_fn,
			   PR_CompareValues,
			   &ACLPermAllocOps, 
			   usrcache_pool);
}


NSAPI_PUBLIC void ACL_SetUserCacheMaxAge(int timeout)
{
    acl_usr_cache_lifetime = timeout;
}


NSAPI_PUBLIC void ACL_SetUserCacheMaxUsers(int n)
{
    PR_ASSERT(n > 0);
    num_usrobj = n;
}


NSAPI_PUBLIC void ACL_SetUserCacheMaxGroupsPerUser(int n)
{
    PR_ASSERT(n > 0);
    usrcache_ngroups = n;
}


int acl_usr_cache_enabled ()
{
    return (acl_usr_cache_lifetime != 0);
}


int acl_usr_cache_init ()
{
    UserCacheObj *usrobj;
    int i;
    int ndatabases;

    if (acl_usr_cache_lifetime == 0) {
	/* Caching is disabled */
	DBG_PRINT1("usrcache is disabled");
        ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_usrcacheDisabled));
	return 0;
    }

#ifdef DEBUG
    {
        char *loguse = conf_findGlobal("ACLCacheUseLog");
        if (loguse && !PL_strcasecmp(loguse, "on")) {
            usrcache_loguse = 1;
        }
    }
#endif /* DEBUG */

    ereport(LOG_VERBOSE,
            XP_GetAdminStr(DBT_usrcacheExpiryTimeout),
            PR_IntervalToSeconds(acl_usr_cache_lifetime));

    ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_usrcacheSize), num_usrobj);

    usrcache_usrcacheobj_sz = sizeof(UserCacheObj);

    if (usrcache_ngroups > 1) {
        usrcache_usrcacheobj_sz += (usrcache_ngroups - 1)*sizeof(char *);
    }

    usrcache_invalidate = conf_getboolean("ACLCacheStrictPassword",
                                          usrcache_invalidate);

    ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_usrcacheGroupPerUsrSize),
            usrcache_ngroups);

    usrcache_pool = NULL;
    user_hash_crit_init();

    databaseUserCacheTable = PR_NewHashTable(0, 
                                             ACLPR_HashCaseString,
                                             ACLPR_CompareCaseStrings,
                                             PR_CompareValues,
                                             &ACLPermAllocOps, 
                                             usrcache_pool);

    /* Allocate first UserCacheObj and initialize the circular link list */
    usrobj = (UserCacheObj *)pool_malloc(usrcache_pool,
                                         usrcache_usrcacheobj_sz);
    if (!usrobj) return -1;
    memset((void *)usrobj, 0, usrcache_usrcacheobj_sz);
    usrobj_list = &usrobj->list;
    PR_INIT_CLIST(usrobj_list);

    /* Allocate rest of the UserCacheObj and put them in the link list */
    for(i = 0; i < num_usrobj; i++){
	usrobj = (UserCacheObj *)pool_malloc(usrcache_pool,
					     usrcache_usrcacheobj_sz);
					     
	if (!usrobj) return -1;
	memset((void *)usrobj, 0, usrcache_usrcacheobj_sz);
	PR_INSERT_AFTER(&usrobj->list, usrobj_list);
    }

    return databaseUserCacheTable ? 0 : -1;
}

/* If the user hash table exists in the databaseUserCacheTable then return it.
 * Otherwise, create a new hash table, insert it in the databaseUserCacheTable
 * and then return it.
 */
static int usr_cache_table_get (const char *dbname, PRHashTable **usrTable)
{
    PRHashTable *table;

    /* To avoid polluting the usrTable for <virtual-server>-specific <auth-db>s
     * that happen to share a common virtual name, we need to make sure we
     * lookup the usrTable using the canonical global database name.
     */
    ACLVirtualDb_t *virtdb;
    if (ACL_VirtualDbLookup(NULL, conf_get_vs(), dbname, &virtdb) == LAS_EVAL_TRUE)
        ACL_VirtualDbGetCanonicalDbName(NULL, virtdb, &dbname);

    user_hash_crit_enter();

    table = (PRHashTable *)PR_HashTableLookup(databaseUserCacheTable,
					      dbname);

    if (!table) {
	/* create a new table and insert it in the databaseUserCacheTable */
	table = alloc_db2uid_table();

	if (table) {
	    PR_HashTableAdd(databaseUserCacheTable,
			    pool_strdup(usrcache_pool, dbname),
			    table);
	}
    }

    *usrTable = table;

    user_hash_crit_exit();

    return table ? LAS_EVAL_TRUE : LAS_EVAL_FAIL;
}

static void usr_cache_recycle_usrobj (UserCacheObj *usrobj)
{
    int i;

    /* If the removed usrobj is in the hashtable, remove it from there */
    if (usrobj->hashtable) {
        PR_HashTableRemove(usrobj->hashtable, usrobj);
        usrobj->hashtable = 0;
    }

    /* Free the memory associated with the usrobj */
    if (usrobj->userdn) {
        pool_free(usrcache_pool, usrobj->userdn);
        usrobj->userdn = 0;
    }
    if (usrobj->passwd) {
        pool_free(usrcache_pool, usrobj->passwd);
        usrobj->passwd = 0;
    }
    for (i = 0; i < usrcache_ngroups; ++i) {
        if (usrobj->groups[i]) {
            pool_free(usrcache_pool, usrobj->groups[i]);
            usrobj->groups[i] = 0;
        }
        else break;
    }
    if (usrobj->derCert) {
        SECITEM_FreeItem(usrobj->derCert, PR_TRUE);
        usrobj->derCert = 0;
    }
    if (usrobj->uid) {
        pool_free(usrcache_pool, usrobj->uid);
        usrobj->uid = 0;
    }
}

static void usr_cache_invalidate_usrobj (UserCacheObj *usrobj)
{
    usr_cache_recycle_usrobj(usrobj);

    PR_REMOVE_LINK(&usrobj->list);
    PR_APPEND_LINK(&usrobj->list, usrobj_list);
}


/*-----------------------------------------------------------------------------
 * Adds or updates the cache entry for this user with the info provided.
 * This is the generic add/update function, so not all parameters need to
 * be non-null, it will use the info given.
 *
 * uid: Name of the user.
 * dbname: Name of authdb handling this user.
 * userdn: DN of user.
 * password: Password of user.
 * group: A group in which 'uid' is a member. This needs to be a single
 *    group name, not a list of groups.
 * derCert: cert
 *
 * Return values
 *     LAS_EVAL_TRUE : on success or if cahing is disabled
 *     LAS_EVAL_FALSE : on failure
 *         user cache table is not found
 *         derCert is NULL AND either of userdn and uid are NULL
 */
int acl_usr_cache_insert (const char *uid, const char *dbname,
			  const char *userdn, const char *passwd,
			  const char *group,
			  const SECItem *derCert)
{
    PRHashTable *usrTable;
    UserCacheObj *usrobj;
    UserCacheObj key;
    int rv;
    int i, j;

    if (acl_usr_cache_lifetime <= 0) {
	/* Caching is disabled */
	return LAS_EVAL_TRUE;
    }

    rv = usr_cache_table_get (dbname, &usrTable);

    if (rv != LAS_EVAL_TRUE) return rv;

    // MUST specify either derCert or (userdn and uid both)
    if (derCert == NULL && (userdn == NULL || uid == NULL))
        return LAS_EVAL_FALSE;

    user_hash_crit_enter();

    key.uid = (char *)uid;
    key.userdn = (char *)userdn;
    key.derCert = (SECItem *)derCert;

    usrobj = (UserCacheObj *)PR_HashTableLookup(usrTable, &key);

    if (usrobj) {
	time_t elapsed = ft_time() - usrobj->time;
	int expired = (elapsed >= acl_usr_cache_lifetime);

	/* Free & reset the old values in usrobj if -- there is an old value
	 * and if the new value is given then it is different or the usrobj
	 * has expired */
	/* Set the field if the new value is given and the field is not set */
	/* If the usrobj has not expired then we only want to update the field
	 * whose new value is non-NULL and different */

	/* Work on the 'uid' field */
	if (usrobj->uid &&
	    (uid ? strcmp(usrobj->uid, uid) : expired))
	{
	    pool_free(usrcache_pool, usrobj->uid);
	    usrobj->uid = 0;
	}
	if (uid && !usrobj->uid) {
	    usrobj->uid = pool_strdup(usrcache_pool, uid);
	}

	/* Work on the 'userdn' field */
	if (usrobj->userdn &&
	    (userdn ? strcmp(usrobj->userdn, userdn) : expired))
	{
	    pool_free(usrcache_pool, usrobj->userdn);
	    usrobj->userdn = 0;
	}
	if (userdn && !usrobj->userdn) {
	    usrobj->userdn = pool_strdup(usrcache_pool, userdn);
	}

	/* Work on the 'passwd' field */
	if (usrobj->passwd &&
	    (passwd ? strcmp(usrobj->passwd, passwd) : expired))
	{
	    pool_free(usrcache_pool, usrobj->passwd);
	    usrobj->passwd = 0;
	}
	if (passwd && !usrobj->passwd) {
	    usrobj->passwd = pool_strdup(usrcache_pool, passwd);
	}

	/* Work on the 'group' field -- not replace a valid group */
        for (i = 0, j = -1; i < usrcache_ngroups; ++i) {
            if (usrobj->groups[i]) {
                if (group && (j < 0) && !strcmp(usrobj->groups[i], group)) {
                    /* Specified group is already cached */
                    j = i;
                }
            }
            else {
                /* Empty group slot found */
                break;
            }
        }

        if (expired) {
            /* Free all the groups except the one that matched, if any */
            for (i = 0; i < usrcache_ngroups; ++i) {
                if (!usrobj->groups[i]) {
                    break;
                }
                if (i != j) {
                    pool_free(usrcache_pool, usrobj->groups[i]);
                    usrobj->groups[i] = 0;
                }
            }

            /* Make the specified group, if any, the first and only one */
            if (j > 0) {
                usrobj->groups[0] = usrobj->groups[j];
                usrobj->groups[j] = 0;
            }
            else if (group && (j < 0)) {
                usrobj->groups[0] = pool_strdup(usrcache_pool, group);
            }
        }
        else if (group) {

            char *mgroup = NULL;

            if ((j < 0) && (i >= usrcache_ngroups)) {
                /* Delete the last group to make room for the new one */
                i = usrcache_ngroups - 1;
                pool_free(usrcache_pool, usrobj->groups[i]);
                usrobj->groups[i] = 0;
            }

            if (j < 0) {
                mgroup = pool_strdup(usrcache_pool, group);
            }
            else if (j > 0) {
                i = j;
                mgroup = usrobj->groups[i];
            }

            if (j != 0) {
                while (i > 0) {
                    usrobj->groups[i] = usrobj->groups[i-1];
                    --i;
                }
                usrobj->groups[0] = mgroup;
            }
        }

	/* Work on the 'derCert' field */
	if (usrobj->derCert &&
	    (derCert ? (derCert->len != usrobj->derCert->len ||
			memcmp(usrobj->derCert->data, derCert->data,
			       derCert->len))
	     : expired))
	{
	    SECITEM_FreeItem(usrobj->derCert, PR_TRUE);
	    usrobj->derCert = 0;
	}
	if (derCert && !usrobj->derCert) {
	    usrobj->derCert = SECITEM_DupItem((SECItem *)derCert);
	}

	/* Reset the time only if the usrobj has expired */
	if (expired) {
	    DBG_PRINT1("Replace ");
	    usrobj->time = ft_time();
	}
	else {
	    DBG_PRINT1("Update ");
	}
    }
    else {
	/* Get the last usrobj from the link list, erase it and use it */
	/* Maybe the last usrobj is not invalid yet but we don't want to grow
	 * the list of usrobjs.  The last obj is the best candidate for being
	 * not valid.  We don't want to compare the time -- just use it.
	 */
	PRCList *tail = PR_LIST_TAIL(usrobj_list);
	usrobj = USEROBJ_PTR(tail);

	/* Fill in the usrobj with the current data */
	usr_cache_recycle_usrobj(usrobj);
	usrobj->uid = uid ? pool_strdup(usrcache_pool, uid) : 0;
	usrobj->userdn = userdn ? pool_strdup(usrcache_pool, userdn) : 0;
	usrobj->passwd = passwd ? pool_strdup(usrcache_pool, passwd) : 0;
	usrobj->derCert = derCert ? SECITEM_DupItem((SECItem *)derCert) : 0;
	usrobj->groups[0] = group ? pool_strdup(usrcache_pool, group) : 0;
	usrobj->time = ft_time();

	/* Add the usrobj to the user hash table */
	PR_HashTableAdd(usrTable, usrobj, usrobj);
	usrobj->hashtable = usrTable;
	DBG_PRINT1("Insert ");
    }

    /* Move the usrobj to the head of the list */
    PR_REMOVE_LINK(&usrobj->list);
    PR_INSERT_AFTER(&usrobj->list, usrobj_list);

    /* Set the time in the UserCacheObj */
    if (usrobj) {
	rv = LAS_EVAL_TRUE;
    }
    else {
	rv = LAS_EVAL_FAIL;
    }

    DBG_PRINT4("acl_usr_cache_insert: derCert = \"%s\" uid = \"%s\" at time = %ld\n",
	       usrobj->derCert ? (char *)usrobj->derCert->data : "<NONE>",
	       uid, time);

    user_hash_crit_exit();
    return rv;
}

/*-----------------------------------------------------------------------------
 * Gets the usr cache object from the hast table for this user.
 *
 * Input parameters 
 *     uid: Name of the user.
 *     derCert: cert
 *     dbname: Name of authdb handling this user.
 *     userdn: DN of user.
 *
 * Output parameters 
 *     usrobj_out  : Pointer to UserCacheObj
 * Return values
 *     LAS_EVAL_TRUE : on success
 *     LAS_EVAL_FALSE : on failure 
 *         if caching is disabled
 *         user cache table is not found
 *         user cache object is not found
 *         derCert is NULL AND either of userdn and uid are NULL
 */
static int acl_usr_cache_get_usrobj (const char *uid, const SECItem *derCert,
				     const char *dbname, const char *userdn,
				     UserCacheObj **usrobj_out)
{
    PRHashTable *usrtable;
    UserCacheObj *usrobj;
    UserCacheObj key;
    time_t elapsed;
    int rv;

    *usrobj_out = 0;

    if (acl_usr_cache_lifetime <= 0) {
	/* Caching is disabled */
	return LAS_EVAL_FALSE;
    }

    rv = usr_cache_table_get(dbname, &usrtable);
    if (!usrtable) return LAS_EVAL_FALSE;

    // MUST specify either cert or (userdn and uid both)
    if (derCert == NULL && (userdn == NULL || uid == NULL))
        return LAS_EVAL_FALSE;

    key.uid = (char *)uid;
    key.userdn = (char *)userdn;
    key.derCert = (SECItem *)derCert;

    usrobj = (UserCacheObj *)PR_HashTableLookup(usrtable, &key);

    if (!usrobj) return LAS_EVAL_FALSE;

    rv = LAS_EVAL_FALSE;

    elapsed = ft_time() - usrobj->time;

    /* If the cache is valid, return the usrobj */
    if (elapsed < acl_usr_cache_lifetime) {
	rv = LAS_EVAL_TRUE;
	*usrobj_out = usrobj;
	DBG_PRINT4("usr_cache found: derCert = \"%s\" uid = \"%s\" at time = %ld\n",
		   usrobj->derCert ? (char *)usrobj->derCert->data : "<NONE>",
		   usrobj->uid, time);
    }
    else {
	DBG_PRINT4("usr_cache expired: derCert = \"%s\" uid = \"%s\" at time = %ld\n",
		   usrobj->derCert ? (char *)usrobj->derCert->data : "<NONE>",
		   usrobj->uid, time);
    }

    return rv;
}

/*-----------------------------------------------------------------------------
 * Get the usr cache object from the hast table for this user (uid and userdn).
 * If the user cache object has the password stored, compare the password specified in this function. 
 * Input parameters 
 *     uid: Name of the user.
 *     dbname: Name of authdb handling this user.
 *     userdn: DN of user.
 *     passwd: password supplied to be compared with the actual password if stored.
 *
 * Return values
 *     LAS_EVAL_TRUE : on success if object is found and passwords do match 
 *     LAS_EVAL_FALSE : on failure 
 *         if passwords don't match 
 *         corresponding user cache object is not found
 */
int acl_usr_cache_passwd_check (const char *uid, const char *dbname,
				const char *passwd, const char *userdn)
{
    UserCacheObj *usrobj;
    int rv;

    // userdn and uid MUST be specified
    if (userdn == NULL || uid == NULL)
        return LAS_EVAL_FALSE;

    user_hash_crit_enter();
    rv = acl_usr_cache_get_usrobj(uid, 0, dbname, userdn, &usrobj);

    if (rv == LAS_EVAL_TRUE) {
        if (usrobj->passwd && passwd && !strcmp(usrobj->passwd, passwd)) {
            rv = LAS_EVAL_TRUE;
            DBG_PRINT1("Success ");
#ifdef DEBUG
            if (usrcache_loguse) {
                ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_usrcacheAuthUsrViaCache),
                        uid ? uid : (usrobj->userdn ? usrobj->userdn : "(null)"));
            }
#endif /* DEBUG */
        }
        else {
            /* authentication failure */
            rv = LAS_EVAL_FALSE;
            DBG_PRINT1("Failed ");
            if (usrcache_invalidate) {
                usr_cache_invalidate_usrobj(usrobj);
                DBG_PRINT1("Invalidated ");
            }
        }
    }
    else {
	rv = LAS_EVAL_FALSE;
	DBG_PRINT1("Not Cached ");
    }

    DBG_PRINT3("acl_usr_cache_passwd_check: uid = \"%s\" at time = %ld\n",
	       uid, time);
    user_hash_crit_exit();

    return rv;
}


static int
group_check_helper (UserCacheObj *usrobj, const char *group, int len)
{
    int i;
    char *savegroup;

    if (!group) {
        return LAS_EVAL_FALSE;
    }

    /* Search cached groups */
    for (i = 0; i < usrcache_ngroups; ++i) {
        savegroup = usrobj->groups[i];
        if (savegroup) {
            if ((len < 0) ? !PL_strcasecmp(savegroup, group)
                          : (!PL_strncasecmp(savegroup, group, len) &&
                             (savegroup[len] == 0))) {

                /* Found group - move it to the beginning of the list */
                while (i > 0) {
                    usrobj->groups[i] = usrobj->groups[i-1];
                    --i;
                }
                usrobj->groups[0] = savegroup;
#ifdef DEBUG
                if (usrcache_loguse) {
                    ereport(LOG_VERBOSE,
                            XP_GetAdminStr(DBT_usrcacheGrpMembershipViaCache), 
                            usrobj->uid ? usrobj->uid
                                        : (usrobj->userdn ? usrobj->userdn : "(null)"),
                            group);
                }
#endif /* DEBUG */
                return LAS_EVAL_TRUE;
            }
        }
        else break;
    }

    return LAS_EVAL_FALSE;
}

/*-----------------------------------------------------------------------------
 * Check the cache to see if user is known to be in any of the given groups.
 *
 * uidP: User to check.
 * cert_in: cert
 * dbname: Name of authdb handling this user.
 * groups: The group or groups to check against. Note that this may be a
 *    comma-separated list of groups, in which case each group in the list
 *    is checked separately.
 * delim: Delimiter for groups list (should be ',')
 * userdn: The LDAP DN for the user. 
 * pool: pool?
 *
 * Return value :
 * LAS_EVAL_TRUE : on success
 * LAS_EVAL_FALSE : 
 *    derCert AND (either of userdn and uid) are null
 *    on failure
 */
int acl_usr_cache_groups_check (char **uidP, void *cert_in,
				const char *dbname, const char *groups,
				char delim, const char *userdn,
				pool_handle_t *pool)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    SECItem *derCertp = 0;
    UserCacheObj *usrobj;
    int rv = LAS_EVAL_FALSE;
    const char *uid = (uidP) ? *uidP : 0;
    const char *group;
    int len;

    // MUST specify either cert_in or (userdn and uid both)
    if (cert_in == NULL && (userdn == NULL || uid == NULL))
        return LAS_EVAL_FALSE;

    if (cert) derCertp = &cert->derCert;

    user_hash_crit_enter();
    rv = acl_usr_cache_get_usrobj(uid, derCertp, dbname, userdn, &usrobj);

    if (rv == LAS_EVAL_TRUE && usrobj->groups[0] && groups) {

	/* Loop through all the groups and check if any is in the cache */
	group = groups;
	rv = LAS_EVAL_FALSE;
	while((group = acl_next_token_len(group, delim, &len)) != NULL) {
	    if (group_check_helper(usrobj, group, len) == LAS_EVAL_TRUE) {
		/* success */
		rv = LAS_EVAL_TRUE;
		DBG_PRINT1("Success ");
                /* Return the cached uid if possible */
                if (uidP && usrobj->uid) {
                    *uidP = pool_strdup(pool, usrobj->uid);
                }
		break;
	    }
	    if (0 != (group = strchr(group+len, delim)))
		group++;
	    else
		break;
	}
    }
    else {
	rv = LAS_EVAL_FALSE;
	DBG_PRINT1("Failed ");
    }

    DBG_PRINT3("acl_usr_cache_groups_check: uid/cert = \"%s\" groups = \"%s\"\n",
	       uid ? uid : (const char *)derCertp->data, groups);
    user_hash_crit_exit();

    return rv;
}

/*-----------------------------------------------------------------------------
 * Gets the usr cache object from the hast table for this user (uid and userdn).
 * Used to check if the user corresponding to this uid and userdn exists.
 *
 * Input parameters 
 *     uid: Name of the user.
 *     dbname: Name of authdb handling this user.
 *     userdn: DN of user.
 * Return values
 *     LAS_EVAL_TRUE : on success
 *     LAS_EVAL_FALSE : on failure 
 *         user cache object is not found
 *         uid or userdn are null
 */
int acl_usr_cache_user_check (const char *uid, const char *dbname,
				const char *userdn)
{
    UserCacheObj *usrobj;
    int rv;

    // userdn and uid must be non NULL
    if (userdn == NULL || uid == NULL)
        return LAS_EVAL_FALSE;

    user_hash_crit_enter();
    rv = acl_usr_cache_get_usrobj(uid, 0, dbname, userdn, &usrobj);

    if (rv == LAS_EVAL_TRUE && usrobj->userdn && userdn &&
	!strcmp(usrobj->userdn, userdn))
    {
	DBG_PRINT1("Success ");
    }
    else {
	rv = LAS_EVAL_FALSE;
	DBG_PRINT1("Failed ");
    }

    DBG_PRINT3("acl_usr_cache_user_check: uid = \"%s\" userdn = \"%s\"\n",
	       uid, userdn ? userdn : "<NONE>");
    user_hash_crit_exit();

    return rv;
}

/*-----------------------------------------------------------------------------
 * Set group membership info in cache for the given user,dbname. If an entry
 * for this user exists in cache it is updated otherwise an entry is created.
 *
 * uid: Name of user entry to add/modify.
 * userdn: DN of the user to add
 * cert_in: cert?
 * dbname: Name of authdb handling this user.
 * group: This is the value of ACL_ATTR_USER_ISMEMBER set by the auth-db
 *    when it processed the ismember getter. That means, it can be a single
 *    group or a comma-separated list of groups.
 *
 * RETURNS:
 *  - LAS_EVAL_FAIL: On malloc failure.
 *  - LAS_EVAL_FAIL: 
 *     if derCert is null AND (either of userdn and uid) are null
 *  - Otherwise, return value passed as-is from acl_usr_cache_insert()
 *
 */
int acl_usr_cache_set_group (const char *uid, void *cert_in,
			     const char *dbname, const char *group,
			     const char *userdn)
{
    int rv;
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    SECItem *derCertp = 0;

    // MUST specify either cert_in or (userdn and uid both)
    if (cert_in == NULL && (userdn == NULL || uid == NULL))
        return LAS_EVAL_FALSE;

    if (cert) derCertp = &cert->derCert;

    rv = acl_usr_cache_insert(uid, dbname, userdn, 0, group, derCertp);

    return rv;
}

/*-----------------------------------------------------------------------------
 * Insert a cert, userdn and uid if specified into user cache.
 *
 * Input parameters 
 *     cert_in: pointer to cert
 *     dbname: Name of authdb handling this user.
 *     uid: Name of the user.
 *     dn: DN of user.
 *
 * Return values
 *     LAS_EVAL_TRUE : on success
 *     LAS_EVAL_FALSE : on failure 
 * return value passed as-is from acl_usr_cache_insert()
 *
 */
int acl_cert_cache_insert (void *cert_in, const char *dbname,
			   const char *uid, const char *dn)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    SECItem derCert = cert->derCert;
    int rv;

    rv = acl_usr_cache_insert(uid, dbname, dn, 0, 0, &derCert);

    return rv;
}

/*-----------------------------------------------------------------------------
 * For a given cert, if user's cache is valid, returns the uid and userdn.
 *
 * Input parameters 
 *     cert_in: pointer to cert
 *     dbname: Name of authdb handling this user.
 *
 * Output parameters 
 *     uid: Name of the user.
 *     dn: DN of user.
 *
 * Return values
 *     LAS_EVAL_TRUE : on success
 *     LAS_EVAL_FALSE : on failure 
 *         user cache object is not found
 *         if cert_in is null
 */
int acl_cert_cache_get_uid (void *cert_in, const char *dbname,
			    char **uid, char **dn, pool_handle_t *pool)
{
    CERTCertificate *cert = (CERTCertificate *)cert_in;
    SECItem derCert = cert->derCert;
    UserCacheObj *usrobj = 0;
    int rv;

    if (cert == NULL)
        return LAS_EVAL_FALSE;

    user_hash_crit_enter();
    rv = acl_usr_cache_get_usrobj(0, &derCert, dbname, 0, &usrobj);

    if (rv == LAS_EVAL_TRUE && usrobj && usrobj->uid) {
	*uid = pool_strdup(pool, usrobj->uid);
	*dn = usrobj->userdn ? pool_strdup(pool, usrobj->userdn) : 0;
    }
    else {
	*uid = 0;
	*dn = 0;
	rv = LAS_EVAL_FALSE;
    }
    user_hash_crit_exit();

    return rv;
}

