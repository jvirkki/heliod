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

#include <stdio.h>
#include <string.h>

#include "plhash.h"

#include <netsite.h>
#include <base/ereport.h>
#include <base/nsassert.h>
#include <base/vs.h>
#include <frame/conf.h>
#include <ldaputil/errors.h>
#include <ldaputil/certmap.h>
#include <ldaputil/dbconf.h>

#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include <libaccess/aclerror.h>
#include <httpdaemon/vsconf.h>

#ifndef STRINGIFY
#define STRINGIFY(literal) #literal
#endif

static int acl_url_to_dbtype(const char *url, ACLDbType_t *dbtype_out);

//
// acl_dbnamehash - global database name to AuthdbInfo_t * hash table
//

typedef struct {
    char *dbname;
    ACLDbType_t dbtype;
    void *dbparsed;
} AuthdbInfo_t;

static unsigned acl_dbnamehash_count;

static PRHashTable *acl_dbnamehash;

NSPR_BEGIN_EXTERN_C

static void *
acl_dbnamehash_alloc_table(void *pool, PRSize size)
{
    return pool_malloc((pool_handle_t *)pool, size);
}

static void
acl_dbnamehash_free_table(void *pool, void *item)
{
    // The acl_dbnamehash hash table is never destroyed
    pool_free((pool_handle_t *)pool, item);
}

static PRHashEntry *
acl_dbnamehash_alloc_entry(void *pool, const void *unused)
{
    // An entry is allocated when a new database is registered
    acl_dbnamehash_count++;

    return (PRHashEntry *)pool_malloc((pool_handle_t *)pool, sizeof(PRHashEntry));
}

static void
acl_dbnamehash_free_entry(void *pool, PRHashEntry *he, uintn flag)
{
    // An entry is freed when a new database is registered with the same name
    acl_dbnamehash_count--;

    // Note that we free the PRHashEntry but never the contained AuthdbInfo_t
    pool_free((pool_handle_t *)pool, he);
}

static PRHashAllocOps acl_dbnamehash_alloc_ops = {
    acl_dbnamehash_alloc_table,
    acl_dbnamehash_free_table,
    acl_dbnamehash_alloc_entry,
    acl_dbnamehash_free_entry
};

NSPR_END_EXTERN_C

//
// acl_virtualizabledb_list - linked list of virtualizable databases
//

typedef struct Property {
    char *name;
    char *value;
    struct Property *next;
} Property_t;

typedef struct ACLVirtualizableDbInfo {
    AuthdbInfo_t *authdb_info;
    char *vsid;
    char *virtdbname;
    char *url;
    Property_t *properties;
    struct ACLVirtualizableDbInfo *next;
} ACLVirtualizableDbInfo_t;

static ACLVirtualizableDbInfo *acl_virtualizabledb_list;

//
// ACLVirtualDb_t - virtual database name to AuthdbInfo_t * mapping
//

struct ACLVirtualDb {
    AuthdbInfo_t *authdb_info; // Information about the underlying database
    struct {
        pool_handle_t *pool;
        PList_t plist;
        PRRWLock *rwlock;
#ifdef DEBUG
        PRInt32 locked;
#endif
    } attrs;
};

//
// ACL_DbNameHashInit - create the global database name to AuthdbInfo_t * hash table
//
void
ACL_DbNameHashInit()
{
    acl_dbnamehash = PR_NewHashTable(0,
                                     ACLPR_HashCaseString,
                                     ACLPR_CompareCaseStrings,
                                     PR_CompareValues,
                                     &acl_dbnamehash_alloc_ops,
                                     ACL_DATABASE_POOL);
    NS_ASSERT(acl_dbnamehash);
}

//
// acl_enumerate_property - PListEnumerate callback for acl_enumerate_properties
//
static void
acl_enumerate_property(char *lval, const void *rval, void *user_data)
{
    Property_t *property = (Property_t *)PERM_MALLOC(sizeof(Property_t));
    if (property) {
        property->name = PERM_STRDUP(lval);
        property->value = PERM_STRDUP((const char *)lval);
        property->next = NULL;
    }

    // XXX let's hope the number of properties is small...
    Property_t **pnext = (Property_t **)user_data;
    while (*pnext)
        pnext = &(*pnext)->next;
    *pnext = property;
}

//
// acl_enumerate_properties - decompose a string value PList_t into a linked list of properties
//
static Property_t *
acl_enumerate_properties(PList_t plist)
{
    Property_t *properties = NULL;
    PListEnumerate(plist, acl_enumerate_property, &properties);
    return properties;
}

//
// acl_dup_properties - copy a linked list of properties
//
static Property_t *
acl_dup_properties(Property_t *properties)
{
    Property_t *head = NULL;
    Property_t *tail = NULL;

    while (properties) {
        Property_t *new_property = (Property_t *)PERM_MALLOC(sizeof(Property_t));
        if (new_property) {
            new_property->name = PERM_STRDUP(properties->name);
            new_property->value = PERM_STRDUP(properties->value);
            new_property->next = NULL;
            if (tail) {
                tail->next = new_property;
            } else {
                head = new_property;
            }
            tail = new_property;
        }
        properties = properties->next;
    }

    return head;
}

//
// acl_free_properties - free a linked list of properties
//
static void
acl_free_properties(Property_t *properties)
{
    while (properties) {
        Property_t *next = properties->next;
        PERM_FREE(properties->name);
        PERM_FREE(properties->value);
        PERM_FREE(properties);
        properties = next;
    }
}

//
// acl_cmp_properties - compare two linked lists of properties
//
static int
acl_cmp_properties(const Property_t *a, const Property_t *b)
{
    for (;;) {
        int rv;

        if (a == b)
            return 0;
        if (!a)
            return -1;
        if (!b)
            return 1;

        rv = strcmp(a->name, b->name);
        if (rv)
            return rv;

        rv = strcmp(a->value, b->value);
        if (rv)
            return rv;

        a = a->next;
        b = b->next;
    }
}

//
// acl_virtualizabledb_find - check for an existing compatible virtualizable database
//
static ACLVirtualizableDbInfo_t *
acl_virtualizabledb_find(ACLDbType_t dbtype, const VirtualServer *vs, const char *virtdbname, const char *url, Property_t *properties)
{
    const char *vsid = vs ? vs_get_id(vs) : "";

    ACL_CritEnter();

    ACLVirtualizableDbInfo_t *virtdb_info = acl_virtualizabledb_list;
    while (virtdb_info) {
        if (virtdb_info->authdb_info->dbtype == dbtype &&
            !strcmp(virtdb_info->vsid, vsid) &&
            !strcasecmp(virtdb_info->virtdbname, virtdbname) &&
            !strcmp(virtdb_info->url, url) &&
            !acl_cmp_properties(virtdb_info->properties, properties))
        {
            // Found an existing compatible ACLVirtualizableDbInfo_t
            break;
        }
        virtdb_info = virtdb_info->next;
    }

    ACL_CritExit();

    return virtdb_info;
}

//
// acl_virtualizabledb_add - add a newly registered virtualizable database to the list
//
static void
acl_virtualizabledb_add(const VirtualServer *vs, const char *virtdbname, const char *url, Property_t *properties, AuthdbInfo_t *authdb_info)
{
    const char *vsid = vs ? vs_get_id(vs) : "";

    // Create a new ACLVirtualizableDbInfo_t
    ACLVirtualizableDbInfo_t *virtdb_info = (ACLVirtualizableDbInfo_t *)pool_malloc(ACL_DATABASE_POOL, sizeof(ACLVirtualizableDbInfo_t));
    if (virtdb_info) {
        virtdb_info->authdb_info = authdb_info;
        virtdb_info->vsid = pool_strdup(ACL_DATABASE_POOL, vsid);
        virtdb_info->virtdbname = pool_strdup(ACL_DATABASE_POOL, virtdbname);
        virtdb_info->url = pool_strdup(ACL_DATABASE_POOL, url);
        virtdb_info->properties = acl_dup_properties(properties);

        if (virtdb_info->vsid &&
            virtdb_info->virtdbname &&
            virtdb_info->url &&
            (virtdb_info->properties == NULL) == (properties == NULL))
        {
            // Add it to the list
            ACL_CritEnter();
            virtdb_info->next = acl_virtualizabledb_list;
            acl_virtualizabledb_list = virtdb_info;
            ACL_CritExit();
        } else {
            // Error allocating one or more members, discard it
            pool_free(ACL_DATABASE_POOL, virtdb_info->vsid);
            pool_free(ACL_DATABASE_POOL, virtdb_info->virtdbname);
            pool_free(ACL_DATABASE_POOL, virtdb_info->url);
            acl_free_properties(virtdb_info->properties);
            pool_free(ACL_DATABASE_POOL, virtdb_info);
            virtdb_info = NULL;
        }
    }
}

//
// acl_authdb_register - register a global database AuthdbInfo_t *
//
// This function is *NOT* exposed in the ACL API.
//
static int
acl_authdb_register(NSErr_t *errp, ACLDbType_t dbtype, const char *dbname, const char *url, PList_t plist, AuthdbInfo_t **pauthdb_info)
{
    DbParseFn_t parseFunc;
    void *db;
    int rv;
    AuthdbInfo_t *authdb_info;

    // find and call the URL parse function
    // of course, not only the URL is looked at, but also the properties
    // if all goes well, we'll have a db handle in "db"
    if ((parseFunc = ACL_DbTypeParseFn(errp, dbtype)) == NULL) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR4400, ACL_Program, 2, XP_GetAdminStr(DBT_DbtypeNotDefinedYet), dbname);
        return -1;
    }
    if ((rv = (*parseFunc)(errp, dbtype, dbname, url, plist, (void **)&db)) < 0) {
        /* plist contains error message/code */
        return rv;
   }

    // Store the db returned by the parse function in the hash table.
    if ((authdb_info = (AuthdbInfo_t *)pool_malloc(ACL_DATABASE_POOL, sizeof(AuthdbInfo_t))) == NULL) {
        nserrGenerate(errp, ACLERRNOMEM, ACLERR4420, ACL_Program, 0);
        return -1;
    }
    authdb_info->dbname = pool_strdup(ACL_DATABASE_POOL, dbname);
    authdb_info->dbtype = dbtype;
    authdb_info->dbparsed = db; /* value returned from parseFunc */

    // Register the database by adding it to the hash table.  If there was
    // already a database named dbname, its AuthdbInfo_t is silently leaked;
    // someone might still have a pointer to it, and in any event we don't
    // have a way to tell the parser to free its AuthdbInfo_t::dbparsed.
    ACL_CritEnter();
    PR_HashTableAdd(acl_dbnamehash, authdb_info->dbname, authdb_info);
    ACL_CritExit();

    // Give caller the AuthdbInfo_t *
    if (pauthdb_info)
        *pauthdb_info = authdb_info;

    return 0;
}

//
// acl_virtualdb_register - register a virtualizable database (as in a server.xml <auth-db> element)
//
static int
acl_virtualdb_register(NSErr_t *errp, const VirtualServer *vs, ACLDbType_t dbtype, const char *virtdbname, const char *url, PList_t plist, Property_t *properties, const char **dbname)
{
    // Look for an existing compatible virtualizable database
    ACLVirtualizableDbInfo_t *virtdb_info = acl_virtualizabledb_find(dbtype, vs, virtdbname, url, properties);
    if (virtdb_info) {
        // Give caller the global database's dbname
        *dbname = virtdb_info->authdb_info->dbname;
        return 0;
    }

    // Construct a canonical, globally unique database name by appending a
    // unique numeric ID (e.g. virtual server foo's virtual database name
    // "default" might become global database name "default:1")
    char *globaldbname = (char *)PERM_MALLOC(strlen(virtdbname) + 1 + sizeof(STRINGIFY(UINT_MAX)));
    if (!globaldbname) {
        nserrGenerate(errp, ACLERRNOMEM, ACLERR4420, ACL_Program, 0);
        return -1;
    }
    for (;;) {
        // Get a unique numeric ID
        ACL_CritEnter();
        static unsigned acl_virtualizabledb_count = 0;
        acl_virtualizabledb_count++;
        int id = acl_virtualizabledb_count;
        ACL_CritExit();

        // Format the global database name
        sprintf(globaldbname, "%s:%u", virtdbname, id);

        // Check to see if someone else already registered this name
        ACL_CritEnter();
        PRBool taken = (PR_HashTableLookup(acl_dbnamehash, globaldbname) != NULL);
        ACL_CritExit();

        // We're done if the name is unique
        if (!taken)
            break;
    }

    // Register the global database
    AuthdbInfo_t *authdb_info;
    int rv = acl_authdb_register(errp, dbtype, globaldbname, url, plist, &authdb_info);

    // We have the global database's AuthdbInfo_t *, so we no longer need our
    // copy of its global name
    PERM_FREE(globaldbname);
    globaldbname = NULL;

    // Bail if there was an error registering the global database
    if (rv < 0)
        return rv;

    // Remember that we created the virtualizable database; we'll probably want
    // it again when the next Configuration is set
    acl_virtualizabledb_add(vs, virtdbname, url, properties, authdb_info);

    ereport(LOG_VERBOSE, "registered database %s", authdb_info->dbname);

    // Give caller the global database's dbname
    *dbname = authdb_info->dbname;

    return 0;
}

//
// ACL_DatabaseRegister - register a global database (as in a dbswitch.conf entry)
//
// The dbtype must have been registered before using ACL_DbTypeRegister().
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DatabaseRegister(NSErr_t *errp, ACLDbType_t dbtype, const char *dbname, const char *url, PList_t plist)
{
    int rv;

    if (!dbname || !*dbname) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR4500, ACL_Program, 1, XP_GetAdminStr(DBT_DatabaseRegisterDatabaseNameMissing));
        return -1;
    }

    if (!ACL_DbTypeIsRegistered(errp, dbtype)) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR4400, ACL_Program, 2, XP_GetAdminStr(DBT_DbtypeNotDefinedYet), dbname);
        return -1;
    }

    return acl_authdb_register(errp, dbtype, dbname, url, plist, NULL);
}

//
// ACL_DatabaseNamesGet - get a list of all the database names
//
// This function is **NOT** exposed in the ACL API
//
NSAPI_PUBLIC int
ACL_DatabaseNamesGet(NSErr_t *errp, char ***names, int *count)
{
    int rv;

    ACL_CritEnter();
    *count = acl_dbnamehash_count;
    rv = acl_registered_names(acl_dbnamehash, *count, names);
    ACL_CritExit();

    return rv;
}

//
// ACL_DatabaseNamesFree - free a list of database names acquired by ACL_DatabaseNamesGet
//
// This function is **NOT** exposed in the ACL API
//
NSAPI_PUBLIC int
ACL_DatabaseNamesFree(NSErr_t *errp, char **names, int count)
{
    int i;

    for (i = count-1; i>=0; i--) FREE(names[i]);

    FREE(names);
    return 0;
}

//
// ACL_RegisterDbFromACL - regsiter a database from an ACL
//
// the ACL must have a complete database URL in the "database" property
//
// This function is **NOT** exposed in the ACL API.
// This function is **OBSOLETE** - see ACL_ListPostParseForAuth for explanation
//
NSAPI_PUBLIC int
ACL_RegisterDbFromACL (NSErr_t *errp, const char *url, ACLDbType_t *dbtype)
{
    /* If the database by name url is already registered, don't do anything.
     * If it is not registered, determine the dbtype from the url.
     * If the dbtype can be determined, register the database with dbname same
     * as the url.  Return the dbtype.
     */
    void *db;
    int rv;
    PList_t plist;

    if (ACL_DatabaseFind(errp, url, dbtype, &db) == LAS_EVAL_TRUE)
	return 0;

    /* The database is not registered yet.  Parse the url to find out its
     * type.  If parsing fails, return failure.
     */
    rv = acl_url_to_dbtype(url, dbtype);
    if (rv < 0) {
	return rv;
    }

    plist = PListNew(NULL);
    rv = ACL_DatabaseRegister(errp, *dbtype, url, url, plist);
    PListDestroy(plist);
    return rv;
}

//
// ACL_DatabaseFind - find a database by name
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_DatabaseFind(NSErr_t *errp, const char *dbname, ACLDbType_t *dbtype, void **db)
{
    AuthdbInfo_t *authdb_info = NULL;

    *dbtype = ACL_DBTYPE_INVALID;
    *db = 0;

    // First, check if we're being called with an active VirtualServer (e.g. if
    // we're being called during HTTP request processing)
    const VirtualServer *vs = conf_get_vs();
    if (vs) {
        // There's an active VirtualServer.  See if it knows about the named
        // database.
        ACLVirtualDb_t *virtdb = vs->getVirtualDb(dbname);
        if (virtdb)
            authdb_info = virtdb->authdb_info;
    }

    if (!dbname) {
        dbname = ACL_DatabaseGetDefault(NULL);
        if (!dbname)
            return LAS_EVAL_FAIL;
    }

    // If the VirtualServer didn't know about the database, see if a plugin
    // registered a VirtualServer-independent database via ACL_DatabaseRegister
    if (!authdb_info) {
        ACL_CritEnter();
        authdb_info = (AuthdbInfo_t *)PR_HashTableLookup(acl_dbnamehash, dbname);
        ACL_CritExit();
    }

    if (authdb_info) {
        *dbtype = authdb_info->dbtype;
        *db = authdb_info->dbparsed;

        return LAS_EVAL_TRUE;
    }

    return LAS_EVAL_FAIL;
}

//
// acl_virtualdb_destroy - free an ACLVirtualDb_t
//
static void
acl_virtualdb_destroy(ACLVirtualDb_t *virtdb)
{
    if (virtdb) {
        PR_ASSERT(virtdb->attrs.locked == 0);

        if (virtdb->attrs.rwlock)
            PR_DestroyRWLock(virtdb->attrs.rwlock);

        if (virtdb->attrs.plist)
            PListDestroy(virtdb->attrs.plist);

        if (virtdb->attrs.pool)
            pool_destroy(virtdb->attrs.pool);

        pool_free(ACL_DATABASE_POOL, virtdb);
    }
}

//
// acl_virtualdb_create - create an ACLVirtualDb_t to wrap the passed authdb_info
//
static ACLVirtualDb_t *
acl_virtualdb_create(AuthdbInfo_t *authdb_info)
{
    ACLVirtualDb_t *virtdb;

    // Our current pool_free implementation is a noop, but we want to discard
    // all the ACLVirtualDb_t's after a new Configuration (the ACLVirtualDb_t
    // attrs may have contained information that's no longer correct in the new
    // Configuration), so we allocate ACLVirtualDb_t's from the process heap.
    PR_ASSERT(ACL_DATABASE_POOL == NULL);

    virtdb = (ACLVirtualDb_t *)pool_malloc(ACL_DATABASE_POOL, sizeof(ACLVirtualDb_t));
    if (virtdb) {
        virtdb->authdb_info = authdb_info;
        virtdb->attrs.pool = NULL;
        virtdb->attrs.plist = NULL;
        virtdb->attrs.rwlock = PR_NewRWLock(0, "ACLVirtualDb_t");
#ifdef DEBUG
        virtdb->attrs.locked = 0;
#endif
        if (!virtdb->attrs.rwlock) {
            acl_virtualdb_destroy(virtdb);
            virtdb = NULL;
        }
    }

    return virtdb;
}

//
// ACL_VirtualDbRegister - register a virtualizable database (as in a server.xml <auth-db> element)
//
// A virtualizable database has a synthetic, globally unique name that differs
// from its virtual name.  For example, the server.xml <auth-db> element for a
// database named "default" might result in a call to ACL_VirtualDbRegister
// that creates a global database named "default:1".
//
// If a compatible (same VirtualServer ID, dbtype, url, and properties)
// virtualizable database with the given virtdbname already exists, it is
// reused.  Otherwise, a globally unique database name is generated and a new
// global database is registered.
//
// The returned dbname may be passed to ACL_VirtualDbRef to create the
// ACLVirtualDb_t that's used to connect a VirtualServer to a global database.
//
// This function is *NOT* exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_VirtualDbRegister(NSErr_t *errp, const VirtualServer *vs, const char *virtdbname, const char *url, PList_t plist, const char **dbname)
{
    int rv;

    ACLDbType_t dbtype;
    rv = acl_url_to_dbtype(url, &dbtype);
    if (rv < 0) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR4610, ACL_Program, 2,
                      XP_GetAdminStr(DBT_CouldntDetermineDbtype), url);
        return rv;
    }

    // Create a copy of the plist properties (the parse function might mess
    // with the plist)
    Property_t *properties = acl_enumerate_properties(plist);

    rv = acl_virtualdb_register(errp, vs, dbtype, virtdbname, url, plist, properties, dbname);

    acl_free_properties(properties);

    return rv;
}

//
// ACL_VirtualDbRef - connect a virtual server to an existing database
//
// This function is **NOT** exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_VirtualDbRef(NSErr_t *errp, const char *dbname, ACLVirtualDb_t **virtdb)
{
    // Get a pointer to the global database's AuthdbInfo_t
    ACL_CritEnter();
    AuthdbInfo_t *authdb_info = (AuthdbInfo_t *)PR_HashTableLookup(acl_dbnamehash, dbname);
    ACL_CritExit();

    NS_ASSERT(authdb_info);
    if (!authdb_info) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6000, ACL_Program, 2,
                      XP_GetAdminStr(DBT_notARegisteredDatabase), dbname);
        return -1;
    }

    // Create an ACLVirtualDb_t that wraps the global database's AuthdbInfo_t
    // and pass it to the caller
    *virtdb = acl_virtualdb_create(authdb_info);
    if (!*virtdb) {
        nserrGenerate(errp, ACLERRNOMEM, ACLERR4420, ACL_Program, 0);
        return -1;
    }

    return 0;
}

//
// ACL_VirtualDbUnref - disconnect a virtual server from a database
//
// This function is **NOT** exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbUnref(ACLVirtualDb_t *virtdb)
{
    acl_virtualdb_destroy(virtdb);
}

//
// ACL_VirtualDbFind - find virtual-server-specific database information
// 
// This function is **NOT** exposed in the ACL API (see ACL_VirtualDbLookup).
//
NSAPI_PUBLIC int
ACL_VirtualDbFind(NSErr_t *errp, PList_t resource, const char *virtdbname, ACLVirtualDb_t **virtdb, ACLDbType_t *dbtype, void **db, const VirtualServer **vs)
{
    VirtualServer *pvs;

    if (!resource)
        return LAS_EVAL_FAIL;

    if (PListGetValue(resource, ACL_ATTR_VS_INDEX, (void **)&pvs, NULL) < 0) {
        // oops, no VS
        return LAS_EVAL_FAIL;
    }

    // Delegate the lookup to the VirtualServer.  It will have previously
    // called ACL_VirtualDbRef for each database it knows abouts and will
    // now return the corresponding ACLVirtualDb_t *.
    ACLVirtualDb_t *pvirtdb = pvs->getVirtualDb(virtdbname);
    if (!pvirtdb) {
        // Unknown database
        return LAS_EVAL_FAIL;
    }

    if (virtdb)
        *virtdb = pvirtdb;
    if (dbtype)
        *dbtype = pvirtdb->authdb_info->dbtype;
    if (db)
        *db = pvirtdb->authdb_info->dbparsed;
    if (vs)
        *vs = pvs;

    return LAS_EVAL_TRUE;
}

//
// ACL_VirtualDbLookup - find virtual-server-specific database information
//
// Similar to ACL_VirtualDbFind, but this is exposed and based on the VS.
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_VirtualDbLookup(NSErr_t *errp, const VirtualServer *vs, const char *virtdbname, ACLVirtualDb_t **virtdb)
{
    if (!virtdb)
        return LAS_EVAL_TRUE;

    *virtdb = vs->getVirtualDb(virtdbname);
    
    return *virtdb ? LAS_EVAL_TRUE : LAS_EVAL_FAIL;
}

//
// ACL_VirtualDbGetAttr - get an attribute specific to a virtual server and database pair
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_VirtualDbGetAttr(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char *name, const char **value)
{
    PR_ASSERT(virtdb->attrs.locked > 0);

    if (virtdb->attrs.plist == NULL)
        return -1;

    if (PListFindValue(virtdb->attrs.plist, name, (void **)value, NULL) < 0)
        return -1;

    return (*value == NULL) ? -1 : 0;
}

//
// ACL_VirtualDbSetAttr - set an attribute specific to a virtual server and database pair
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC int
ACL_VirtualDbSetAttr(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char *name, const char *value)
{
    PR_ASSERT(virtdb->attrs.locked > 0);

    // Create the attrs PList on demand
    if (virtdb->attrs.plist == NULL) {
        if (virtdb->attrs.pool == NULL)
            virtdb->attrs.pool = pool_create();

        if (virtdb->attrs.pool != NULL)
            virtdb->attrs.plist = PListCreate(virtdb->attrs.pool, 0, 0, 0);

        if (virtdb->attrs.plist == NULL)
            return -1;
    }

    if (name) {
        char *v;

        // Remove the old value (if any).  Note that we don't free the old
        // value as an ACL_VirtualDbGetAttr caller may still have a reference.
        PListDeleteProp(virtdb->attrs.plist, 0, name);

        // Create a copy of the caller's value
        if (value) {
            v = pool_strdup(virtdb->attrs.pool, value);
            if (v == NULL)
                return - 1;
        } else {
            v = NULL;
        }

        // The name will be strdup'ed in PListInitProp -> PListDefProp -> PListNameProp
        if (PListInitProp(virtdb->attrs.plist, 0, name, v, NULL) < 0) {
            if (v)
                pool_free(virtdb->attrs.pool, v);
            return -1;
        }
    }

    return 0;
}

//
// ACL_VirtualDbReadLock - restrict access to virtual-server-specific attributes
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbReadLock(ACLVirtualDb_t *virtdb)
{
    PR_RWLock_Rlock(virtdb->attrs.rwlock);
#ifdef DEBUG
    PR_AtomicIncrement(&virtdb->attrs.locked);
    PR_ASSERT(virtdb->attrs.locked > 0);
#endif
}

//
// ACL_VirtualDbWriteLock - restrict access to virtual-server-specific attributes
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbWriteLock(ACLVirtualDb_t *virtdb)
{
    PR_RWLock_Wlock(virtdb->attrs.rwlock);
#ifdef DEBUG
    PR_AtomicIncrement(&virtdb->attrs.locked);
    PR_ASSERT(virtdb->attrs.locked > 0);
#endif
}

//
// ACL_VirtualDbUnlock - grant access to virtual-server-specific attributes
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbUnlock(ACLVirtualDb_t *virtdb)
{
#ifdef DEBUG
    PR_ASSERT(virtdb->attrs.locked > 0);
    PR_AtomicDecrement(&virtdb->attrs.locked);
#endif
    PR_RWLock_Unlock(virtdb->attrs.rwlock);
}

//
// ACL_VirtualDbGetParsedDb - get the data retrieved by the underlying database's parsing function
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbGetParsedDb(NSErr_t *errp, ACLVirtualDb_t *virtdb, void **db)
{
    if (virtdb) {
        *db = virtdb->authdb_info->dbparsed;
    } else {
        *db = NULL;
    }
}

//
// ACL_VirtualDbGetDbType - get the underlying database's type
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbGetDbType(NSErr_t *errp, ACLVirtualDb_t *virtdb, ACLDbType_t *dbtype)
{
    if (virtdb) {
        *dbtype = virtdb->authdb_info->dbtype;
    } else {
        *dbtype = ACL_DBTYPE_INVALID;
    }
}

//
// ACL_VirtualDbGetCanonicalDbName - get the underlying database's name
//
// The returned name is unique across all virtual servers' databases.
//
// This function is exposed in the ACL API.
//
NSAPI_PUBLIC void
ACL_VirtualDbGetCanonicalDbName(NSErr_t *errp, ACLVirtualDb_t *virtdb, const char **dbname)
{
    if (virtdb) {
        *dbname = virtdb->authdb_info->dbname;
    } else {
        *dbname = NULL;
    }
}

/* try to determine the dbtype from the database url */
static int acl_url_to_dbtype (const char *url, ACLDbType_t *dbtype_out)
{
    ACLDbType_t dbtype;
    NSErr_t *errp = 0;

    *dbtype_out = dbtype = ACL_DBTYPE_INVALID;
    if (!url || !*url) return -1;

    // urls with ldap:, ldaps: and ldapdb: are all of type ACL_DBTYPE_LDAP.
    if (!strncmp(url, "ldap:", strlen("ldap:"))) {
	dbtype = ACL_DbTypeLdap;
    } else if (!strncmp(url, "ldaps:", strlen("ldaps:"))) {
	dbtype = ACL_DbTypeLdap;
    } else {
	/* treat prefix in the url as dbtype if it has been registered.
	 * "null", for example, should be registered
	 */
	int prefix_len = strcspn(url, ":");
	char dbtypestr[64];

	if (prefix_len && prefix_len < sizeof(dbtypestr)) {
	    memcpy(dbtypestr, url, prefix_len);
	    dbtypestr[prefix_len] = 0;

	    if (ACL_DbTypeFind(errp, dbtypestr, &dbtype)) {
		/* prefix is not a registered dbtype */
		dbtype = ACL_DBTYPE_INVALID;
	    }
	}
    }

    if (ACL_DbTypeIsEqual(errp, dbtype, ACL_DBTYPE_INVALID)) return -1;

    *dbtype_out = dbtype;
    return 0;
}

// -----------------------------------------------------------------------------
// ACL_DatabaseDefault functions - set & get the default database
// -----------------------------------------------------------------------------

static const char *ACLDatabaseDefault = "default";

NSAPI_PUBLIC const char *
ACL_DatabaseGetDefault(NSErr_t *errp)
{
    ACL_CritEnter();
    const char *dbname = ACLDatabaseDefault;
    ACL_CritExit();
    return dbname;
}

NSAPI_PUBLIC int
ACL_DatabaseSetDefault(NSErr_t *errp, const char *dbname)
{
    ACLDbType_t dbtype;
    int rv;
    void *db;

    if (!dbname || !*dbname) return LAS_EVAL_FAIL;

    ACL_CritEnter();

    rv = ACL_DatabaseFind(errp, dbname, &dbtype, &db);

    // Note that we need to leak the old ACLDatabaseDefault value as someone
    // may still have a pointer to it
    if (strcmp(ACLDatabaseDefault, dbname)) {
        char *p = pool_strdup(ACL_DATABASE_POOL, dbname);
        if (p) {
            ACLDatabaseDefault = p;
        } else {
            rv = LAS_EVAL_FAIL;
        }
    }

    if (rv == LAS_EVAL_TRUE) {
        ACL_DbTypeSetDefault(errp, dbtype);
    } else {
        ACL_DbTypeSetDefault(errp, ACL_DBTYPE_INVALID);
    }

    ACL_CritExit();

    return rv == LAS_EVAL_TRUE ? 0 : -1;
}

//
//  ACL_AuthInfoSetDbname - set ACL_ATTR_DATABASE in auth_info
//
//   this used to set ACL_ATTR_DBTYPE, but does no more - we cannot cache
//   the database type in the ACL anymore, because it might be different for
//   every virtual server.
//
//    INPUT
//	auth_info	A PList of the authentication name/value pairs as
//			provided by EvalTestRights to the LAS.
//	dbname		Name of the new auth_info database.
//    OUTPUT
//	retcode		0 on success.
//
NSAPI_PUBLIC int
ACL_AuthInfoSetDbname(NSErr_t *errp, PList_t auth_info, const char *dbname)
{
    char *copy;
    char *n2;
    int old;
    int rv;

    if (!auth_info)
	// out of memory or nothing to do
	return -1;

    if (!dbname)
        dbname = ACL_DatabaseGetDefault(errp);

    // Check the existing entry
    old = PListGetValue(auth_info, ACL_ATTR_DATABASE_INDEX, (void **)&n2, NULL);
    if (old >= 0) {
        PListDeleteProp(auth_info, ACL_ATTR_DATABASE_INDEX, ACL_ATTR_DATABASE);
        pool_free(PListGetPool(auth_info), n2);
    }

    // Create new entry
    if ((copy = (char *)pool_strdup(PListGetPool(auth_info), dbname)) == NULL)
        return -1;

    PListInitProp(auth_info, ACL_ATTR_DATABASE_INDEX, ACL_ATTR_DATABASE, copy, 0);

    return 0;
}

//
//  ACL_AuthInfoGetDbname
//	INPUT
//	auth_info	A PList of the authentication name/value pairs as
//			provided by EvalTestRights to the LAS.
//	OUTPUT
//	dbname		The database name.  This can be the default database
//			name if the auth_info PList doesn't explicitly
//			have a database entry.
//	retcode		0 on success.
//
NSAPI_PUBLIC int
ACL_AuthInfoGetDbname(PList_t auth_info, char **dbname)
{
    char *p;

    if (auth_info && PListGetValue(auth_info, ACL_ATTR_DATABASE_INDEX, (void **)&p, NULL) >= 0) {
        *dbname = p;
    } else {
        *dbname = (char *)ACL_DatabaseGetDefault(NULL);
    }

    return *dbname ? 0 : -1;
}
