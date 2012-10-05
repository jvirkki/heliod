/*
 * FILE:        fileacl.cpp
 * DESCRIPTION:
 *      This is a module of Loadable Attribute Service (LAS).
 *
 * setup instruction for iWS6.1 for testing keyfile style databse
 *   0. magnus.conf(init.conf) in <iws61install>/<iws61 server instance>/config:
 *   #this step is not necessary any more. 
 *   Init fn="load-modules" shlib="pathto/liblibfileacl.so" funcs="las_file_init"
 *   Init fn="acl-register-module" module="fileacl_module" func="las_file_init"
 *   # liblibfileacl.so should be copied to <iwsinstall>/bin/https/lib
 *  
 *   1. server.xml in <iws61install>/<iws61 server instance>/config should read:
 *     <virtual-server id="server1" config-file="server1-obj.conf" 
 *       http-listeners="http-listener-1" hosts="vortexkai" 
 *       mime="mime1" state="on" accept-language="false" acls="acl1" >
 *          <auth-db id="default" database="default"/>
 *          <auth-db id="myfile" database="myfiledb"/>
 *       ....
 *      </virtual-server>
 *
 *   2. obj.conf (server1-obj.conf) in <iws61install>/<iws61 server instance>/config  
 *   nothing to do.
 *
 *   3. dbswitch.conf in <iws61install>/userdb directory:
 *   directory myfiledb file
 *   myfiledb:digestauth on
 *   myfiledb:syntax keyfile
 *   myfiledb:keyfile <installroot>/domains/domain1/server1/config/keyfile
 *   #keyfile is exactly same as S1AS
 *
 *   4. generated.<server instance>.acl in <iws61install>/httpacl
 *   version 3.0;
 *   acl "default";
 *   authenticate (user) {
 *     prompt = "File Realm";
 *     database = "myfile";
 *     method = "basic";
 *   };
 *   deny (all) user = "anyone";
 *   allow (all) user = "j2ee";
 *
 *   additional information on LASs is at http://docs-pdf.sun.com/816-5643-10/816-5643-10.pdf
 *   also, refer iws60/s1as admin document
 */

#include <base/pblock.h>
#include <base/ereport.h>
#include <safs/init.h>                  // ACL_MethodBasic & ACL_MethodDigest
#include <safs/digest.h>                // ACL_MethodBasic & ACL_MethodDigest
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/digest.h>
#include <libaccess/FileRealmUser.h>
#include <libaccess/FileRealm.h>
#include <libaccess/fileacl.h>
#include "aclpriv.h"

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC int parse_digest_user_login(const char *auth, char **user,
                                         char **realm, char **nonce,
                                         char **uri, char **qop, char **nc,
                                         char **cnonce, char **response,
                                         char **opaque, char **algorithm);
NSAPI_PUBLIC int fileacl_init(NSErr_t *errp);

NSPR_END_EXTERN_C




/* 
 * from the acl dbname, find out the fileacl database defined in dbswitch.conf
 * and registered by url parse function.
 * refer libaccess/digest.cpp
 *
 * @param errp  error stack
 * @param resource        - handle for resource property list
 * @param auth_info       - handle for "file" authentication properties
 * @return pointer to the filerealm database
 */
static FileRealm * findFileRealm(NSErr_t *errp,const PList_t resource, const PList_t auth_info)
{
    int  rv=0;
    char        *aclDBName;
    ACLDbType_t dbtype;
    void        *pAnyDB=NULL;

    ACL_AuthInfoGetDbname(auth_info, &aclDBName);
    rv = ACL_DatabaseFind(errp, aclDBName, &dbtype, &pAnyDB);
    if (rv != LAS_EVAL_TRUE || dbtype == ACL_DBTYPE_INVALID 
        || dbtype != ACL_DbTypeFile) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_fileaclDBHandleNotFound), aclDBName);
        return NULL;
    }
    return (FileRealm*)pAnyDB;
}



/*
 * @DESCRIPTION:
 *      This is authentication function for the "file".
 * @param  errp            - error stack
 * @param  subject         - handle for subject property list
 * @param  resource        - handle for resource property list
 * @param  auth_info       - handle for "file" authentication properties
 * @param  global_auth     - all attributes with authentication properties
 * @param  arg             - cookie specified to ACL_AttrGetterRegister()
 *
 * @return
 *      LAS_EVAL_TRUE  authentication success.
 *      LAS_EVAL_FALSE authentication failed.
 *      LAS_EVAL_INVALID invalid check
 *      LAS_EVAL_FAIL    otherwise
 */
int
fileacl_user_get(NSErr_t *errp, PList_t subject, PList_t resource,
                PList_t auth_info, PList_t global_auth, void *arg)
{
    char * user=NULL;
    char * password=NULL;
    ACLMethod_t authtype, method, acl_auth_method;
    int  rv=0;
    FileRealm *pFileRealm = findFileRealm(errp,resource,auth_info);
    if (!pFileRealm)
        return LAS_EVAL_INVALID;

    /* Get the user */
    rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_USER, (void **)&user,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE)
        return rv;
    if (!user || !*user)
        return LAS_EVAL_INVALID;

    ereport(LOG_VERBOSE, "file authdb: Authenticating user [%s]", user);

    /* Get the supplied password */
    rv = ACL_GetAttribute(errp, ACL_ATTR_RAW_PASSWORD,
                          (void **)&password, subject, resource,
                          auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE)
        return rv;
    if (!password)
        return LAS_EVAL_INVALID;

    rv = ACL_GetAttribute(errp, ACL_ATTR_AUTHTYPE, (void **)&authtype,
                          subject, resource, auth_info, global_auth);
    //refer libaccess/ldapacl.cpp  plugins/nsacl/auth_dbm.c
    if (rv != LAS_EVAL_TRUE || authtype == ACL_MethodBasic) {
        (void)ACL_AuthInfoGetMethod(errp, auth_info, &acl_auth_method);
        if (acl_auth_method != ACL_MethodBasic) {
            return LAS_EVAL_FALSE; // this calls for a 401 (with the correct challenge)
        }
        method = ACL_MethodBasic;
    } else if (authtype == ACL_MethodDigest) {
        method = ACL_MethodDigest;
    } else {
        // this should never happen, so I want it to blow up in DEBUG builds
        // otherwise just try to fail as gracefully as possible
        PR_ASSERT(0);
        return LAS_EVAL_FAIL;
    }

    int idx = 0;
    FileRealmUser*filerealmUser = NULL;

    PRBool b = PR_FALSE;
    if (method == ACL_MethodBasic) {
        //ACL_CritEnter();
        if ( ! pFileRealm->isDigest()) {
            filerealmUser = pFileRealm->find(user);
        } else {
            char *prompt=NULL;
            int i = PListGetValue(auth_info,ACL_ATTR_PROMPT_INDEX, (void **)&prompt, NULL);
            //rv = ACL_GetAttribute(errp, ACL_ATTR_PROMPT, (void **)&prompt,
            //              subject, resource, auth_info, global_auth); //doesn't work. why?
            if (i>=0 && prompt!=NULL) {
                filerealmUser = pFileRealm->find(user,prompt);            
            }
        }
        //ACL_CritExit();

        if (idx<0 ||filerealmUser==NULL) {
            ereport(LOG_VERBOSE,
                    "file authdb: Authentication failed for [%s] (not found)",
                    user);
            return LAS_EVAL_FALSE;
        }
        
        if (pFileRealm->isKeyfile())
            b = filerealmUser->sshaVerify(password);
        else if (pFileRealm->isHTAccess())
            b = filerealmUser->htcryptVerify(password);
        else if (pFileRealm->isDigest()) {
            b = PR_FALSE;
            char *prompt=NULL;
            PListGetValue(auth_info,ACL_ATTR_PROMPT_INDEX, (void **)&prompt, NULL);
            NSString ha1;
            ha1Passwd(user,prompt,password,ha1);
            NSString pwd = filerealmUser->getHashedPwd();
            if ( pwd==ha1 )
                b = PR_TRUE;
        }

        if (b==PR_FALSE) {
            ereport(LOG_VERBOSE,
                    "file authdb: Authentication failed for [%s] (basic)",
                    user);
            //should not return LAS_EVAL_INVALID or LAS_EVAL_FAIL
            return LAS_EVAL_FALSE; 
        }
        
        /* Indicate the password is valid */
        PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX,
                      ACL_ATTR_IS_VALID_PASSWORD, user, 0);
        ereport(LOG_VERBOSE,
                "file authdb: Authentication succeeded for [%s] (basic)",
                user);
        return LAS_EVAL_TRUE;

    } else {
        // Handle digest auth
        
        if (pFileRealm->supportDigest()==PR_FALSE) {
            ereport(LOG_SECURITY, XP_GetAdminStr(DBT_fileaclDigestAuthNotSupport) );
            return LAS_EVAL_FAIL;
        }
        Request *rq=NULL;
        rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);
        if (rv < 0) {
            return LAS_EVAL_FAIL;
        }
        char *http_method = pblock_findval("method",rq->reqpb);
        char *raw_realm = NULL;
        char *raw_nonce = NULL;
        char *raw_uri = NULL;
        char *raw_qop = NULL;
        char *raw_nc = NULL;
        char *raw_cnonce = NULL;
        char *raw_cresponse = NULL;
        char *raw_opaque = NULL;
        char *raw_algorithm = NULL;

        if ((rv = parse_digest_user_login(password, NULL, &raw_realm, &raw_nonce, &raw_uri,
                                           &raw_qop, &raw_nc, &raw_cnonce, &raw_cresponse,
                                           &raw_opaque, &raw_algorithm)) == LAS_EVAL_TRUE)
        {
                //NSString userWithRealm;
                //userWithRealm.append(user);
                //userWithRealm.append("@");
                //userWithRealm.append(raw_realm);
            char *prompt=NULL;
            int i = PListGetValue(auth_info,ACL_ATTR_PROMPT_INDEX, (void **)&prompt, NULL);
            if (!prompt || !raw_realm)
                return LAS_EVAL_FALSE;
            if (strcmp(prompt,raw_realm)) {
                PListInitProp(resource, ACL_ATTR_STALE_DIGEST_INDEX, ACL_ATTR_STALE_DIGEST, "true", 0);
                return LAS_EVAL_FALSE;
            }

            filerealmUser = pFileRealm->find(user,raw_realm);
            if (idx<0 ||filerealmUser==NULL) {
                ereport(LOG_SECURITY, XP_GetAdminStr(DBT_fileaclUserRealmNotFound), user,raw_realm);
                return LAS_EVAL_FALSE;
            }
            // check if nonce is stale after we checked whether the authentication would be OK otherwise.
            if (Digest_check_nonce(GetDigestPrivatekey(), raw_nonce, (char*)"") == PR_FALSE) {
                // come back with a 401 and a new WWW-Authenticate header indicating
                // the nonce used by the client was stale...
                PListInitProp(resource, ACL_ATTR_STALE_DIGEST_INDEX, ACL_ATTR_STALE_DIGEST, "true", 0);
                return LAS_EVAL_FALSE;
            }
            //HASHHEX HA1;
            HASHHEX HA2 = "";
            HASHHEX response;
            //DigestCalcHA1( (char*)"MD5",user,raw_realm,password,raw_nonce,raw_cnonce,HA1);
            NSString pwd = filerealmUser->getHashedPwd();
            const char* HA1 = pwd.data();
            DigestCalcResponse( (char*)HA1,raw_nonce,raw_nc,raw_cnonce,raw_qop,http_method,raw_uri,HA2,response);

            if ( strcmp(response,raw_cresponse) ) {
                ereport(LOG_VERBOSE,
                "file authdb: Authentication failed for [%s] in [%s] (digest)",
                        user, raw_realm);
                return LAS_EVAL_FALSE;
            }
            
            PListInitProp(resource, ACL_ATTR_STALE_DIGEST_INDEX, ACL_ATTR_STALE_DIGEST, "false", 0);
            // this is put into the server headers if present...
            PListInitProp(resource, ACL_ATTR_RSPAUTH_INDEX, ACL_ATTR_RSPAUTH,strdup(response),0);
            PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX,ACL_ATTR_IS_VALID_PASSWORD, user,0);
            
            ereport(LOG_VERBOSE,
                    "file authdb: Authentication succeeded for [%s] (digest)",
                    user);
            return LAS_EVAL_TRUE;       
        }
    }
    return LAS_EVAL_FALSE;
}

/*
 * @DESCRIPTION:
 *      check user existence. Support "BASIC" and "Digest" methods
 * @param  errp            - error stack
 * @param  subject         - handle for subject property list
 * @param  resource        - handle for resource property list
 * @param  auth_info       - handle for "file" authentication properties
 * @param  global_auth     - all attributes with authentication properties
 * @param  arg             - cookie specified to ACL_AttrGetterRegister()
 *
 * @return
 *      LAS_EVAL_TRUE   yes. user exists in file-db.
 *      LAS_EVAL_FALSE  no. doesn't exist that user.
 *      LAS_EVAL_INVALID invalid check
 *      LAS_EVAL_FAIL    otherwise
 */
int fileacl_userexists_get(NSErr_t *errp, PList_t subject,
    		  PList_t resource, PList_t auth_info,
    		  PList_t global_auth, void *unused)
{
    int   rv;
    char *user;
    char *password;

    rv = PListGetValue(subject, ACL_ATTR_USER_INDEX, (void **)&user, NULL);
    if (rv < 0) {  // We don't even have a user name
        return LAS_EVAL_FAIL;
    }
    password = (char*)"anything";

    ereport(LOG_VERBOSE,
            "file authdb: verifying whether user [%s] exists", user);

    FileRealm *pFileRealm = findFileRealm(errp,resource,auth_info);
    if (!pFileRealm)
        return LAS_EVAL_INVALID;

    FileRealmUser*filerealmUser = NULL;
    if (!pFileRealm->isDigest()) {
        filerealmUser = pFileRealm->find(user);
    } else {
        char *prompt=NULL;
        PListGetValue(auth_info,ACL_ATTR_PROMPT_INDEX, (void **)&prompt, NULL);
        filerealmUser = pFileRealm->find(user,prompt);
    }

    if (filerealmUser) {
        PListInitProp(subject, ACL_ATTR_USER_EXISTS_INDEX, 
                      ACL_ATTR_USER_EXISTS, user, 0);
        ereport(LOG_VERBOSE, "file authdb: user [%s] exists", user);
        return LAS_EVAL_TRUE;
    }

    ereport(LOG_VERBOSE, "file authdb: user [%s] does not exist", user);
    return LAS_EVAL_FALSE;
}


/*
 * @DESCRIPTION:
 *      This is authentication function for USER_ISMEMBER attribute
 * @param  errp            - error stack
 * @param  subject         - handle for subject property list
 * @param  resource        - handle for resource property list
 * @param  auth_info       - handle for "file" authentication properties
 * @param  global_auth     - all attributes with authentication properties
 * @param  arg             - cookie specified to ACL_AttrGetterRegister()
 *
 * @return
 *      LAS_EVAL_TRUE  authentication success.
 *      LAS_EVAL_FALSE authentication failed.
 *      LAS_EVAL_INVALID invalid check
 *      LAS_EVAL_FAIL    otherwise
 */
int
fileacl_user_ismember_get(NSErr_t *errp, PList_t subject, PList_t resource,
                PList_t auth_info, PList_t global_auth, void *arg)
{
    char * user=NULL;
    char * groups=NULL;
    int rv=0;
    FileRealm *pFileRealm = findFileRealm(errp,resource,auth_info);
    if (!pFileRealm)
        return LAS_EVAL_INVALID;

    rv = ACL_GetAttribute(errp, ACL_ATTR_USER, (void **)&user,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE)
        return rv;
    if (!user || !*user)
        return LAS_EVAL_FALSE; //LAS_EVAL_INVALID;

    rv = ACL_GetAttribute(errp, ACL_ATTR_GROUPS, (void **)&groups,
                          subject, resource, auth_info, global_auth);
    if (rv != LAS_EVAL_TRUE)
        return rv;
    if (!groups || !*groups)
        return LAS_EVAL_FALSE; //LAS_EVAL_INVALID;

    ereport(LOG_VERBOSE,
            "file authdb: Is user [%s] in group [%s]?", user, groups);

    int idx = 0;
    FileRealmUser*filerealmUser = NULL;
    if (!pFileRealm->isDigest()) {
        filerealmUser = pFileRealm->find(user);
    } else {
        char *prompt=NULL;
        PListGetValue(auth_info,ACL_ATTR_PROMPT_INDEX, (void **)&prompt, NULL);
        filerealmUser = pFileRealm->find(user,prompt);
    }
    if (idx<0 ||filerealmUser==NULL)
        return LAS_EVAL_FALSE;

    // Find group matching target(s), if any
    
    char * match = acl_attrlist_match(filerealmUser->getGroups(), groups);

    if (match) {
        PListDeleteProp(subject, ACL_ATTR_GROUPS_INDEX, ACL_ATTR_GROUPS);
        PListInitProp(subject, ACL_ATTR_USER_ISMEMBER_INDEX,
                      ACL_ATTR_USER_ISMEMBER, STRDUP(match), 0);
        ereport(LOG_VERBOSE,
                "file authdb: User [%s] matched group [%s]", user, match);
        FREE(match);
        return LAS_EVAL_TRUE;
    }

    ereport(LOG_VERBOSE,
            "file authdb: User [%s] is not in group [%s]", user, groups);
    return LAS_EVAL_FALSE;
}


/*
 * not clear what we can do here. maybe clear the filerealm
 */
void 
fileacl_flush(void **any)
{
    /* Nothing to do */
}


/*
 * parse the dbswitch.conf and notisfication to databse register.
 * refer libaccess/ldapacl.cpp   libaccess/acldb.cpp   base/plist.cpp
 *
 * @param errp   -- error stack
 * @param dbtype -- the dbtype (in this module it should be same as acl_fileacl_dbtype
 * @param url    -- url part of directory definition of dbswitch, in this module, it should be "file"
 * @param plist  -- property list of that database directory entry
 * @param(OUT) db-- the file realm object
 * @return 0
 */
int
fileacl_parse_url(NSErr_t *errp, ACLDbType_t dbtype,const char *dbname, const char *url, PList_t plist, void **db)
{
    char *path=NULL;
    int  rc=-1;
    if (plist==NULL)
        return 0;
    if (!url || !*url) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6400, ACL_Program, 1, XP_GetAdminStr(DBT_fileaclDatabaseUrlIsMissing));
        return -1;
    }
    if (!dbname || !*dbname) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR6410, ACL_Program, 1, XP_GetAdminStr(DBT_fileaclDatabaseNameIsMissing));
        return -1;
    }
    
    FileRealm *pDB=NULL;
    path=NULL;
    PListFindValue(plist, FILEACL_ATTR_SYNTAX, (void **)&path, NULL);
    if (path==NULL || strcmp(path,FILEACL_ATTRVAL_KEYFILE)==0) {
       pDB = new FileRealm(DBFORMAT_KEYFILE);
       path=NULL;
       PListFindValue(plist, FILEACL_ATTR_KEYFILE, (void **)&path, NULL);
       if (path) {
           rc=pDB->load(errp,path,FILEACL_ATTR_KEYFILE);
       }
    } else if (strcmp(path,FILEACL_ATTRVAL_HTACCESS)==0) {
       pDB = new FileRealm(DBFORMAT_HTACCESS);
       path=NULL;
       PListFindValue(plist, FILEACL_ATTR_USERFILE, (void **)&path, NULL);
       if (!path) {
           rc=-1;
       } else {
           rc=pDB->load(errp,path,FILEACL_ATTR_USERFILE);
           if (rc==0) {
               char *path2=NULL;
               PListFindValue(plist, FILEACL_ATTR_GROUPFILE, (void **)&path2, NULL);
               if (path2) {
                   rc=pDB->load(errp,path2,FILEACL_ATTR_GROUPFILE);
               }
           }
       }
    } else if (strcmp(path,FILEACL_ATTRVAL_DIGEST)==0) {
       pDB = new FileRealm(DBFORMAT_DIGEST); //db format
       path=NULL;
       PListFindValue(plist, FILEACL_ATTR_DIGESTFILE, (void **)&path, NULL);
       if (path) {
           rc=pDB->load(errp,path,FILEACL_ATTR_DIGESTFILE); //file format
       }
    }
    
    if (rc<0) {
      nserrGenerate(errp, ACLERRINVAL, ACLERR6420, ACL_Program, 1, XP_GetAdminStr(DBT_fileaclErrorParsingFileUrl));
      return -1;        
    }
    
    *db = pDB;   //see ldapacl.cpp
    return 0;
}


