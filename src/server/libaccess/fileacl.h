/*
 * FILE:        fileacl.h
 *
 * general header file for this module
 */
#ifndef __FILEACL_H__
#define __FILEACL_H__

#include <netsite.h>
#include <libaccess/aclproto.h>

#ifdef XP_WIN32
#ifdef BUILD_DLL
#define FILEACL_PUBLIC __declspec(dllexport)
#else
#define FILEACL_PUBLIC __declspec(dllimport)
#endif
#else /* Unix */
#define FILEACL_PUBLIC
#endif


//#define ACL_DBTYPE_FILEACL       "file"

#define FILEACL_ATTR_SYNTAX      "syntax"
#define FILEACL_ATTRVAL_KEYFILE  "keyfile"
#define FILEACL_ATTRVAL_HTACCESS "htaccess"
#define FILEACL_ATTRVAL_DIGEST   "digest"

#define FILEACL_ATTR_KEYFILE     "keyfile"
#define FILEACL_ATTR_USERFILE    "userfile"
#define FILEACL_ATTR_GROUPFILE   "groupfile"
#define FILEACL_ATTR_DIGESTFILE  "digestfile"

#define FILEACL_COMMA_SEP        ','

#define DBFORMAT_KEYFILE    FILEACL_ATTRVAL_KEYFILE
#define DBFORMAT_HTACCESS   FILEACL_ATTRVAL_HTACCESS
#define DBFORMAT_DIGEST     FILEACL_ATTRVAL_DIGEST

#define MAX_LINE_LEN 256
#define MAX_PATH_LEN 256

#define ERR_LINE_TOO_LONG -1


NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC int  fileacl_user_get(NSErr_t *errp, PList_t subject, PList_t resource,
             PList_t auth_info, PList_t global_auth, void *arg);
NSAPI_PUBLIC int  fileacl_user_ismember_get(NSErr_t *errp, PList_t subject, PList_t resource,
             PList_t auth_info, PList_t global_auth, void *arg);
NSAPI_PUBLIC int  fileacl_userexists_get(NSErr_t *errp, PList_t subject, PList_t resource,
             PList_t auth_info, PList_t global_auth, void *arg);
NSAPI_PUBLIC int  fileacl_parse_url(NSErr_t *errp, ACLDbType_t dbtype,const char*dbname, 
             const char *url, PList_t plist, void **db);
NSAPI_PUBLIC void fileacl_flush(void **any);

NSPR_END_EXTERN_C



#endif

