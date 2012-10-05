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
 * Implements the "kerberos" auth-db and associated "gssapi" auth method
 *
 *
 * The kerberos auth-db supports the following parameters:
 *
 *    - servicename: Defaults to "HTTP"
 *    - hostname: Defaults to the value of rq->hostname
 *
 * Initialization
 * ==============
 *  kerberos_parse_url() initializes the kerberos auth-db.
 *  ws_gss_setup_env() sets up kerberos environment.
 *
 *
 * Attribute Getters:
 * ==================
 *  PAM authdb defines several ACLAttrGetterFn_t functions, which are
 *  registered in safs/init.cpp during startup:
 *
 *    - ACL_ATTR_IS_VALID_PASSWORD : gssapi_authenticate_user()
 *    - ACL_ATTR_USER              : get_auth_user_gssapi()
 *
 *
 * See also pamacl.cpp for additional documentation pointers.
 *
 */


#include <assert.h>
#include <base64.h>
#include <base/nsassert.h>
#include <base/shexp.h>
#include <base/ereport.h>
#include <base/plist.h>
#include <base/pblock.h>
#include <frame/protocol.h>
#include <frame/log.h>
#include <frame/conf_init.h>
#include <libaccess/aclerror.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/gssapi.h>
#include <safs/init.h>
#include <httpdaemon/configuration.h>
#include <httpdaemon/configurationmanager.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>

#define NEGOTIATE "Negotiate "
#define NEGOTIATE_LEN 10
#define KRB5_KTNAME "KRB5_KTNAME"

static char * env_keytab = NULL;     // store value of KRB5_KTHOME env var


// Internal convenience gssacl_info struct

typedef struct gssacl_info_t 
{
    const char * hostname;
    const char * servicename;
    gss_OID_set_desc desiredMechs;
    gss_ctx_id_t context;
    gss_cred_id_t server_creds;
} gssacl_info;


// Internal helper function prototypes

static void ws_gss_setup_env(const char * keytabfilepath);
static const char * gss_error2text(OM_uint32 maj, OM_uint32 min);
static void init_info(gssacl_info *info);
static void free_info(gssacl_info *info);
static int acquire_server_creds(gssacl_info * info, 
                                Session * sn, 
                                Request * rq);



/*-----------------------------------------------------------------------------
 * Reconfig preinit callback. This is registered (see gssapi_init) to be 
 * called during the initial phase of reconfig. 
 *
 */
int gssapi_conf_preinit(Configuration* incoming, 
                        const Configuration* outgoing)
{
    // The config framework also calls this function during server startup.
    // Detect and handle this as a special case because the postdestroy
    // function will not be called during startup. Therefore, we need to 
    // initialize the keytab value here during startup.

    if (env_keytab == NULL) {
        const char * keytabfilepath = (incoming->krb5KeytabFile).getStringValue();
        ws_gss_setup_env(keytabfilepath);
    }

    return PR_SUCCESS;
}


/*-----------------------------------------------------------------------------
 * Reconfig postdestroy callback. This is registered (see gssapi_init) to be
 * called during the final phase of reconfig. That means the new config has
 * been accepted and installed, so let's start using it.
 *
 */
void gssapi_conf_postdestroy(Configuration* outgoing)
{
    // Get the current (what was the incoming) config
    const Configuration * newconfig = ConfigurationManager::getConfiguration();
    assert(newconfig != NULL);
    const char * ktp = (newconfig->krb5KeytabFile).getStringValue();

    ws_gss_setup_env(ktp);

    // Release the config reference
    newconfig->unref();
}


/*-----------------------------------------------------------------------------
 * Initialize gssapi subsystem during startup.  Called from WebServer::Init
 *
 */
void gssapi_init()
{
    // Register our reconfig callback functions
    conf_register_cb(gssapi_conf_preinit, gssapi_conf_postdestroy);
}


/*-----------------------------------------------------------------------------
 * Set (or reset) the KRB5_KTNAME environment variable (see krb5envvar(5))
 * Called first during startup and also during reconfig.
 *
 */
static void ws_gss_setup_env(const char * keytabfilepath)
{
    if (keytabfilepath == NULL) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_gssNoKeytab));
        return;
    }

    ereport(LOG_VERBOSE, "gssapi: using keytab: %s", keytabfilepath);

    char * newenv = (char *)
        PERM_MALLOC(strlen(KRB5_KTNAME) + strlen(keytabfilepath) + 2);

    if (newenv == NULL) {
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_AclerrfmtAclerrnomem));
        PR_ASSERT(0);
        return;
    }

    sprintf(newenv, "%s=%s", KRB5_KTNAME, keytabfilepath);
    putenv(newenv);

    if (env_keytab != NULL) {
        PERM_FREE(env_keytab);
    }

    env_keytab = newenv;

    return;
}


/*-----------------------------------------------------------------------------
 * kerberos_parse_url is registered as the parsing function for the
 * kerberos auth-db in aclinit.cpp.  This will be called during
 * startup when the auth-dbs are initialized, for each auth-db of type
 * kerberos (or more precisely, for those which the url is"kerberos").
 *
 * This function is of type DbParseFn_t - see p.181 of ACPG
 *
 *
 * PARAMS:
 *  - errp: error frames
 *  - dbtype: The dbtype identifier that was assigned to kerberos auth-db 
 *       during ACL_DbTypeRegister.
 *  - name: Name of auth-db. This contains the name of this auth-db from
 *       the server config and appended to it ":N" where N is an int.
 *  - url: URL from server config (should be "kerberos")
 *  - plist: Property list of parameters to this auth-db, from server config.
 *  - db: OUT: Set to point to an arbitrary data structure which this
 *       auth-db needs to store data. This pointer can be obtained later
 *       with ACL_DatabaseFind(). This is initialized to point to a struct
 *       of type kerberos_params.
 *
 * RETURN:
 *  - 0 if successful, or a negative value if unsuccessful.
 *
 */
int kerberos_parse_url(NSErr_t *errp, ACLDbType_t dbtype, const char *name,
                       const char *url, PList_t plist, void **db)
{
    // This shouldn't be getting called for an auth-db other than kerberos
    NS_ASSERT(dbtype == ACL_DbTypeKerberos);

    // A name and url should've been sent
    NS_ASSERT(name != NULL);
    NS_ASSERT(*name != 0);
    NS_ASSERT(url != NULL);
    NS_ASSERT(*url != 0);

    // kerberos authdb URL is always just "kerberos"
    if (strcmp(KERBEROS_AUTHDB_URL, url)) {
        nserrGenerate(errp, ACLERRSYNTAX, ACLERR6600, ACL_Program, 1,
                      XP_GetAdminStr(DBT_kerberosDbUrlIsWrong));
        return(-1);
    }

    // need to retrieve and store configuration parameters

    const char * sname = NULL;
    const char * hname = NULL;

    kerberos_params * kp = (kerberos_params *)
        PERM_MALLOC(sizeof(kerberos_params));

    if (kp == NULL) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR5520, ACL_Program, 1,
                      XP_GetAdminStr(DBT_kerberosOM));
        return(-1);
    }

    // servicename (probably defaults to 'HTTP')
    PListFindValue(plist, KERBEROS_PARAM_SERVICENAME, (void **)&sname, NULL);
    if (sname != NULL) {
        kp->servicename = PERM_STRDUP(sname);
    } else {
        kp->servicename = PERM_STRDUP(KERBEROS_PARAM_SERVICENAME_DEFAULT);
    }

    // hostname (typically NULL here, will use value from request)
    PListFindValue(plist, KERBEROS_PARAM_HOSTNAME, (void **)&hname, NULL);
    if (hname != NULL) {
        kp->hostname = PERM_STRDUP(hname);
    } else {
        kp->hostname = NULL;
    }

    *db = (void *)kp;

    return 0;
}


/*-----------------------------------------------------------------------------
 * Attribute getter for ACL_ATTR_USER.
 *
 */
int get_auth_user_gssapi(NSErr_t *errp, PList_t subject, PList_t resource,
			 PList_t auth_info, PList_t global_auth, void *unused)
{
    int rv;
    char * authreq;
    char * auth;
    char * cached_user;
    char * user = NULL;
    Request *rq;


    // Are we just checking whether we need to authenticate?
    rv = PListGetValue(resource, 
                       ACL_ATTR_AUTHREQCHECK_INDEX, (void **)&authreq, NULL);
    if ((rv >= 0) && authreq && *authreq) {
        // Check the fact that we needed an authenticated user & fail 
        authreq[0] = '*';       
        return LAS_EVAL_FAIL;
    }

    // There needs to be an Authorization: header if we are to do
    // anything, so try retrieving one now.. this calls the generic
    // ACL_ATTR_AUTHORIZATION getter since grabbing the header isn't
    // specific to GSS auth.

    rv = ACL_GetAttribute(errp, ACL_ATTR_AUTHORIZATION, (void **)&auth, 
                          subject, resource, auth_info, global_auth);

    if (rv != LAS_EVAL_TRUE || !auth || !*auth) {

        // Turns out it is not there, so we need to respond with a
        // Authorization: Negotiate header.  The header is set by code
        // in aclframe.cpp. To trigger it, we need to set the auth
        // prompt. For Negotiate there is no prompt, so we'll just set
        // a placeholder.

        PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, 
                        ACL_ATTR_WWW_AUTH_PROMPT);
        PListInitProp(resource, ACL_ATTR_WWW_AUTH_PROMPT_INDEX, 
                      ACL_ATTR_WWW_AUTH_PROMPT, " ", 0);

        // Also need to set the method, so aclframe code can call the
        // right setter function

        PListDeleteProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, 
                        ACL_ATTR_WWW_AUTH_METHOD);
        PListInitProp(resource, ACL_ATTR_WWW_AUTH_METHOD_INDEX, 
                      ACL_ATTR_WWW_AUTH_METHOD, ACL_MethodGSSAPI, 0);

        return LAS_EVAL_NEED_MORE_INFO;
    }

    // By now we do have authorization header, so try to process it

    rv = ACL_GetAttribute(errp, ACL_ATTR_IS_VALID_PASSWORD, 
                          (void **)&user, subject, resource, 
                          auth_info, global_auth);

    if (rv == LAS_EVAL_TRUE) {
        if (user) {
            PListDeleteProp(resource, ACL_ATTR_USER_INDEX, ACL_ATTR_USER);
            PListInitProp(subject, 
                          ACL_ATTR_USER_INDEX, ACL_ATTR_USER, user, 0);
            return LAS_EVAL_TRUE;
        } else {
            // Previous getter should always return user if rv was TRUE
            PR_ASSERT(0);
            return LAS_EVAL_FAIL;
        }
    }
}


/*-----------------------------------------------------------------------------
 * Attribute getter for ACL_ATTR_IS_VALID_PASSWORD.
 *
 * The real work happens here. Parse Negotiate header, obtain server 
 * credentials and validate client credentials.
 *
 */
int gssapi_authenticate_user(NSErr_t *errp, PList_t subject, PList_t resource,
                             PList_t auth_info, PList_t global_auth, void *arg)
{
    int rv;

    const char * user = "UNKNOWN";
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;


    // Get the Request object  

    Request *rq;

    rv = PListGetValue(resource, ACL_ATTR_REQUEST_INDEX, (void **)&rq, NULL);

    if (rv < 0 || !rq) {
        nserrGenerate(errp, ACLERRINTERNAL, ACLERR6800, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoRq));
        return LAS_EVAL_FAIL;
    }

    // Get the auth-db name 

    char * dbname = NULL;

    rv = ACL_AuthInfoGetDbname(auth_info, &dbname);

    if (rv < 0 || !dbname) {
        nserrGenerate(errp, ACLERRINTERNAL, ACLERR6801, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoDb));
        return LAS_EVAL_FAIL;
    }

    // Get auth-db type and its parameters

    ACLDbType_t dbtype;
    kerberos_params * kp = NULL;

    rv = ACL_DatabaseFind(errp, dbname, &dbtype, (void **)&kp);

    if (rv != LAS_EVAL_TRUE || !kp) {
        nserrGenerate(errp, ACLERRINTERNAL, ACLERR6802, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoDbInfo));
        return LAS_EVAL_FAIL;
    }

    // GSS auth only works with kerberos user database

    if (dbtype != ACL_DbTypeKerberos) {
        nserrGenerate(errp, ACLERRCONFIG, ACLERR6803, ACL_Program, 2,
                      XP_GetAdminStr(DBT_gssapiNotKerberos), dbname);
        return LAS_EVAL_FAIL;
    }

    // Get the authorization header which should be there by now

    char * auth = NULL;

    rv = ACL_GetAttribute(errp, ACL_ATTR_AUTHORIZATION, (void **)&auth, 
                          subject, resource, auth_info, global_auth);

    if (rv != LAS_EVAL_TRUE || !auth) {
        nserrGenerate(errp, ACLERRINTERNAL, ACLERR6804, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoAuthz));
        return LAS_EVAL_FAIL;
    }

    // Get the Session object

    Session * sn = NULL;

    rv = PListGetValue(subject, ACL_ATTR_SESSION_INDEX, (void **)&sn, NULL);

    if (rv < 0 || !sn) {
        nserrGenerate(errp, ACLERRINTERNAL, ACLERR6805, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoSn));
        return LAS_EVAL_FAIL;
    }

    // Log some info..

    log_error(LOG_VERBOSE, "gssapi", sn, rq, 
              "processing authentication with auth-db %s", dbname);
    log_error(LOG_VERBOSE, "gssapi", sn, rq, 
              "client authorization header: [%s]", auth);

    // We should only be here if we have a Negotiate authorization header

    if (strncasecmp(NEGOTIATE, auth, NEGOTIATE_LEN)) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR6806, ACL_Program, 2,
                      XP_GetAdminStr(DBT_gssapiNoNego), auth);
        return LAS_EVAL_FAIL;
    }

    // If we're verbose, get and report KRB5_KTNAME value for diagnostics

    if (ereport_can_log(LOG_VERBOSE)) {
        const char * krb5_ktname = getenv(KRB5_KTNAME);
        if (krb5_ktname == NULL) {
            log_error(LOG_VERBOSE, "gssapi", sn, rq,
                      "No KRB5_KTNAME environment!");
        } else {
            log_error(LOG_VERBOSE, "gssapi", sn, rq,
                      "KRB5_KTNAME is [%s]", krb5_ktname);
        }
    }

    // Decode (base64) authorization header into input_token

    unsigned int itlen;
    input_token.value = ATOB_AsciiToData(auth + NEGOTIATE_LEN, &itlen);
    input_token.length = (size_t)itlen;

    if (!input_token.value || !*(char *)input_token.value ||
        !input_token.length) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR6807, ACL_Program, 2,
                      XP_GetAdminStr(DBT_gssapiCantDecode), auth);
        return LAS_EVAL_FAIL;
    }


    // Try to get desired mechanism (it should be "spnego")

    gss_OID_desc client_mech_desc;
    gss_OID client_mechoid = &client_mech_desc;
    const char *mechstr = NULL;

    if (!__gss_get_mech_type(client_mechoid, &input_token)) {
        mechstr = (char *)__gss_oid_to_mech(client_mechoid);
    }
	
    if (mechstr == NULL) {
        client_mechoid = GSS_C_NULL_OID;
        mechstr = "<unknown>";
    }

    log_error(LOG_VERBOSE, "gssapi", sn, rq,
              "client wants gss mechanism: %s", mechstr);

    // Allocate and initialize the info struct to process this request

    gssacl_info info;
    init_info(&info);

    info.desiredMechs.count = 1;
    info.desiredMechs.elements = client_mechoid;
    info.servicename = kp->servicename;
    if (kp->hostname != NULL) {
        info.hostname = kp->hostname;
    } else {
        // if no default, use value from this request
        if (rq->hostname) {
            info.hostname = rq->hostname;
        } else {
            info.hostname = "localhost";
        }
    }

    // Go acquire server credentials

    rv = acquire_server_creds(&info, sn, rq);

    if (rv != LAS_EVAL_TRUE) {
        nserrGenerate(errp, ACLERRFAIL, ACLERR6808, ACL_Program, 1,
                      XP_GetAdminStr(DBT_gssapiNoServerCreds));
        free_info(&info);
        return LAS_EVAL_FAIL;
    }
    
    // Try to display the server creds information if we're logging verbosely

    if (ereport_can_log(LOG_VERBOSE)) {

        OM_uint32 major_status, minor_status;
        gss_name_t sname;
        gss_buffer_desc dname;

        major_status = gss_inquire_cred(&minor_status,
                                        info.server_creds,
                                        &sname, NULL, NULL, NULL);
        if (major_status == GSS_S_COMPLETE) {
            major_status = gss_display_name(&minor_status,
                                            sname, &dname, NULL);
        }
        if (major_status == GSS_S_COMPLETE) {
            log_error(LOG_VERBOSE, "gssapi", sn, rq,
                      "obtained server credentials for [%s]",
                      (char *)dname.value);
            gss_release_name(&minor_status, &sname);
            gss_release_buffer(&minor_status, &dname);
        }
    }


    // Pass client data to gss

    OM_uint32 major_status, minor_status;
    gss_name_t client_name = GSS_C_NO_NAME;
    gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
    gss_cred_id_t delegated_cred = GSS_C_NO_CREDENTIAL;
    char * authhdr = NULL;

    major_status = gss_accept_sec_context(&minor_status,
                                          &(info.context),
                                          info.server_creds,
                                          &input_token,
                                          GSS_C_NO_CHANNEL_BINDINGS,
                                          &client_name,
                                          NULL,
                                          &output_token,
                                          NULL,
                                          NULL,
                                          &delegated_cred);

    if (GSS_ERROR(major_status)) {
        const char * msg = gss_error2text(major_status, minor_status);
        log_error(LOG_VERBOSE, "gssapi", sn, rq,
                  "gss_accept_sec_context() failed: %s", msg);
        FREE((void *)msg);
        rv = LAS_EVAL_FALSE;
        goto cleanup;
    }

    // Some GSSAPI mechanisms may require multiple iterations to
    // establish authentication. Most notably, when MUTUAL_AUTHENTICATION
    // flag is used, multiple round trips are needed. We don't support this.

    if (major_status == GSS_S_CONTINUE_NEEDED) {
        log_error(LOG_VERBOSE, "gssapi", sn, rq,
                  "gss continuation not supported");
        rv = LAS_EVAL_FALSE;
        goto cleanup;
    }

    // Encode return value

    if (output_token.length) {
        char * authz_return = BTOA_DataToAscii((const unsigned char*)
                                               output_token.value, 
                                               output_token.length);
        authhdr = (char *)MALLOC(NEGOTIATE_LEN + strlen(authz_return) + 1);
        sprintf(authhdr, "%s%s", NEGOTIATE, authz_return);
        PORT_Free(authz_return);
    } 

    // Obtain name of the user who authenticated

    if (client_name != GSS_C_NO_NAME) {
        gss_buffer_desc name_token = GSS_C_EMPTY_BUFFER;

        major_status = gss_display_name(&minor_status, client_name,
                                        &name_token, NULL);

        if (GSS_ERROR(major_status)) {
            const char * msg = gss_error2text(major_status, minor_status);
            log_error(LOG_VERBOSE, "gssapi", sn, rq,
                      "gss_export_name() failed: %s", msg);
            FREE((void *)msg);
            nserrGenerate(errp, ACLERRFAIL, ACLERR6809, ACL_Program, 1,
                          XP_GetAdminStr(DBT_gssapiNoUserName));
            rv = LAS_EVAL_FAIL;
            goto cleanup;
        }


        if (name_token.length) {
            user = STRDUP((const char *)name_token.value);
            gss_release_buffer(&minor_status, &name_token);
        }
    }

    log_error(LOG_VERBOSE, "gssapi", sn, rq,
              "setting user name to [%s]", user);

    PListDeleteProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX, 
                    ACL_ATTR_IS_VALID_PASSWORD);
    PListInitProp(subject, ACL_ATTR_IS_VALID_PASSWORD_INDEX,
                  ACL_ATTR_IS_VALID_PASSWORD, user, 0);

    // Set value of negotiate header to be returned

    if (authhdr) {
        pblock_nvinsert("www-authenticate", STRDUP(authhdr), rq->srvhdrs);
    }

    // Done!

    rv = LAS_EVAL_TRUE;

 cleanup:
    free_info(&info);
    if (authhdr) { FREE(authhdr); }
    if (input_token.value) { PORT_Free(input_token.value); }

    return rv;
}


/*-----------------------------------------------------------------------------
 * Attempt to acquire the server's credentials.
 *
 * Return:
 *    LAS_EVAL_TRUE on success, LAS_EVAL_FAIL otherwise.
 *
 */
static int acquire_server_creds(gssacl_info * info,
                                Session * sn,
                                Request * rq)
{
    gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
    gss_name_t server_name = GSS_C_NO_NAME;
    OM_uint32 major_status, minor_status;
    char buf[1024];

    snprintf(buf, sizeof(buf), "%s@%s", info->servicename, info->hostname);
    input_token.value = buf;
    input_token.length = strlen(buf) + 1;

    log_error(LOG_VERBOSE, "gssapi", sn, rq, 
              "attempting to acquire server credentials for %s", buf);

    major_status = gss_import_name(&minor_status, &input_token,
                                   GSS_C_NT_HOSTBASED_SERVICE, &server_name);

    if (GSS_ERROR(major_status)) {
        const char * gsstxt = gss_error2text(major_status, minor_status);
        log_error(LOG_VERBOSE, "gssapi", sn, rq, 
                  "gss_import_name() failed: %s", gsstxt);
        FREE((void *)gsstxt);
        return LAS_EVAL_FAIL;
    }


    major_status = gss_acquire_cred(&minor_status, server_name,
                                    GSS_C_INDEFINITE, &(info->desiredMechs),
                                    GSS_C_ACCEPT,
                                    &(info->server_creds), NULL, NULL);

    if (GSS_ERROR(major_status)) {
        const char * gsstxt = gss_error2text(major_status, minor_status);
        log_error(LOG_VERBOSE, "gssapi", sn, rq,
                  "gss_acquire_cred() failed: %s", gsstxt);
        FREE((void *)gsstxt);
        gss_release_name(&minor_status, &server_name);
        return LAS_EVAL_FAIL;
    }

    gss_release_name(&minor_status, &server_name);
    return LAS_EVAL_TRUE;
}


/*-----------------------------------------------------------------------------
 * Construct gss error message string. 
 * Caller must FREE() returned buffer.
 *
 */
static const char * gss_error2text(OM_uint32 maj, OM_uint32 min)
{
    OM_uint32 maj_stat, min_stat;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc msg;
    char * out = (char *)MALLOC(1024);
    int bufsize = 1024;

    out[0]=0;

    do {
        maj_stat = gss_display_status (&min_stat,
                                       maj, GSS_C_GSS_CODE, GSS_C_NO_OID,
                                       &msg_ctx, &msg);
        if (GSS_ERROR(maj_stat)) { break; }

        strncat(out, (char *)msg.value, bufsize);
        bufsize -= strlen((char *)msg.value);
        (void)gss_release_buffer(&min_stat, &msg);

        maj_stat = gss_display_status (&min_stat,
                                       min, GSS_C_MECH_CODE, GSS_C_NULL_OID,
                                       &msg_ctx, &msg);
        if (!GSS_ERROR(maj_stat)) {

            strncat(out, " (", bufsize);
            bufsize -= 1;
            strncat(out, (char *)msg.value, bufsize);
            bufsize -= strlen((char *)msg.value);
            strncat(out, ") ", bufsize);
            bufsize -= 1;
            (void)gss_release_buffer(&min_stat, &msg);
        }

    } while (!GSS_ERROR(maj_stat) && msg_ctx != 0);

    return (const char *)out;
}


/*-----------------------------------------------------------------------------
 * Initialize the gssacl_info struct.
 *
 */
static void init_info(gssacl_info *info)
{
    info->hostname = NULL;
    info->servicename = NULL;
    info->server_creds == GSS_C_NO_CREDENTIAL;
    info->context = GSS_C_NO_CONTEXT;
}


/*-----------------------------------------------------------------------------
 * Free the gssacl_info struct as necessary.
 *
 */
static void free_info(gssacl_info *info)
{
    OM_uint32 minor_status;

    if (info->context != GSS_C_NO_CONTEXT) {
        gss_delete_sec_context(&minor_status,
                               &info->context, GSS_C_NO_BUFFER);
    }

    if (info->server_creds != GSS_C_NO_CREDENTIAL) {
        gss_release_cred(&minor_status, &info->server_creds);
    }

}


