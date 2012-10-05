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

// cgi.cpp
//
// Unified NSAPI CGI implementation for Unix and WIN32.
//
// Derived from cgi.cpp, cgieng.cpp, and ntcgi.cpp as they existed circa 07/00.

#include <signal.h>

#include "frame/log.h"
#include "frame/httpact.h"  /* for servact_translate_uri */
#include "frame/http_ext.h" /* for servact_translate_uri */
#include "base/daemon.h"    /* for daemon_atrestart */
#include "base/util.h"      /* environment functions, can_exec */
#include "base/net.h"       /* SYS_NETFD */
#include "base/buffer.h"    /* netbuf */
#include "frame/protocol.h" /* protocol_status */
#include "frame/http.h"     /* http_hdrs2env */
#include "frame/conf.h"     /* server action globals */
#include "frame/conf_api.h" /* server action globals */
#include "frame/httpfilter.h"
#include "private/pprio.h"
#include "ssl.h"
#include "libaccess/nsauth.h"
#include "ldaputil/certmap.h"
#include "safs/child.h"
#include "safs/cgi.h"
#include "safs/dbtsafs.h"

//-----------------------------------------------------------------------------
// Macros
//-----------------------------------------------------------------------------

#define CGI_VERSION "CGI/1.1"

// CGI env sizing
#define MAX_CGI_COMMON_VARS      23
#define MAX_CGI_SPECIFIC_VARS    10
#define MAX_CGI_CLIENT_AUTH_VARS 64

#define NUM_RANDOM_BYTES 32

#if defined(XP_WIN32)
#define LIBRARY_PATH "SystemRoot"
#endif // XP_WIN32

#if defined(XP_UNIX)
#if defined(HPUX)
#define LIBRARY_PATH "SHLIB_PATH" // HP-UX
#elif defined(AIX)
#define LIBRARY_PATH "LIBPATH" // AIX
#else
#define LIBRARY_PATH "LD_LIBRARY_PATH" // Everyone else
#endif
#endif // XP_UNIX

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

static PRLock*     _initLock = 0;       // serializes initialization
static pblock*     _initenv = 0;        // initial environment
static char**      _initenv_vars = 0;   // all server's environment variables
static PRBool      _env_initialized = PR_FALSE; // called cgi_env_init?
static char*       _env_path = 0;       // PATH envvar
static char*       _env_tz = 0;         // TZ envvar
static char*       _env_lang = 0;       // LANG envvar
static char*       _env_ldpath = 0;     // LD_LIBRARY_PATH envvar
static PRIntervalTime _timeout = PR_INTERVAL_NO_TIMEOUT; // timeout for program
static PRIntervalTime _iotimeout = PR_SecondsToInterval(300); // timeout for IO

//-----------------------------------------------------------------------------
// cgi_parse_query
//-----------------------------------------------------------------------------

static int cgi_parse_query(char **argv, int n, char *q)
{
    int i = 0;

    for (;;) {
        // Parse an arg from the query string
        char *arg = q;
        while (*q != ' ' && *q != '\0')
            q++;

        // Caller must ensure argv[] is appropriately sized
        PR_ASSERT(i < n);
        if (i >= n)
            return -1;

        // Escape any shell characters (does a MALLOC on our behalf)
        char c = *q;
        *q = '\0';
        argv[i] = util_sh_escape(arg);
        *q = c;

        // Unescape this arg, bailing on error
        if (!argv[i] || !util_uri_unescape_strict(argv[i]))
            return -1;

        // We successfully parsed another arg
        i++;

        // We're done when we hit the end of the query string
        if (*q == '\0')
            break;

        q++;
    }

    return i;
}

//-----------------------------------------------------------------------------
// cgi_create_argv
//-----------------------------------------------------------------------------

static char **cgi_create_argv(const char *program, const char *script, const char *query)
{
    int nargs = 1;

    // The script name, if any, will be argv[1]
    if (script)
        nargs++;

    // Turn '+' into ' ' in query string, counting the number of arg as we go
    char *qargs = NULL;
    if (query && !strchr(query, '=')) {
        qargs = STRDUP(query);
        if (!qargs)
            return NULL;
        nargs++;
        for (char *t = qargs; *t; t++) {
            if (*t == '+')
                *t = ' ';
            if (*t == ' ')
                nargs++;
        }
    }

    // Allocate the argv[] array, leaving room for the trailing NULL
    char **argv = (char **) MALLOC((nargs + 1) * sizeof(char *));
    if (!argv)
        return NULL;

    int i = 0;

    // Set argv[0] to the program name
    argv[i] = STRDUP(program);
    i++;

    // Set argv[1] to the script name
    if (script) {
        argv[i] = STRDUP(script);
        i++;
    }

    // Parse the query string into argv[]
    if (qargs) {
        int n = cgi_parse_query(&argv[i], nargs - i, qargs);
        if (n > 0)
            i += n;
    }

    // Mark end of argv[]
    argv[i] = NULL;

    return argv;
}

//----------------------------------------------------------------------------
// get_request_uri
//----------------------------------------------------------------------------

char* get_request_uri(Request *req)
{
    // Extract the encoded URI from the request line if possible
    char *clf_request = pblock_findkeyval(pb_key_clf_request, req->reqpb);
    if (clf_request) {
        // Find the beginning of the method
        char *method = clf_request;
        while (*method && isspace(*method))
            method++;

        // Skip over the method
        char *uri = method;
        while (*uri && !isspace(*uri))
            uri++;

        // Find the beginning of the URI
        while (*uri && isspace(*uri))
            uri++;

        // Find the end of the URI
        char *end = uri;
        while (*end && !isspace(*end)) 
            end++;

        // Make a copy of the uri
        int len = end - uri;
        char* request_uri = (char*) MALLOC(len+1);
        memcpy(request_uri, uri, len);
        request_uri[len] = '\0';

        return request_uri;
    }

    // No request line.
    return NULL;
}

//-----------------------------------------------------------------------------
// cgi_specific_vars
//-----------------------------------------------------------------------------

char** cgi_specific_vars(Session *sn, Request *rq, const char *args,
                         char** env, int scriptVars)
{
    int y;
    register pblock *pb;
    char c, *t, *u;

    pb = rq->reqpb;

    int x;
    env = util_env_create(env, MAX_CGI_SPECIFIC_VARS, &x);
    env[x++] = util_env_str("GATEWAY_INTERFACE", CGI_VERSION);

    // Added error check for missing request information.
    t = pblock_findval("protocol", pb);
    if (t == NULL) {
       log_error(LOG_FAILURE, "cgi_specific_vars", sn, rq, 
                 XP_GetAdminStr(DBT_cgiError21), "protocol");
       return NULL;
    }
    env[x++] = util_env_str("SERVER_PROTOCOL", t);

    t = pblock_findval("method", pb);
    if (t == NULL) {
       log_error(LOG_FAILURE, "cgi_specific_vars", sn, rq, 
                 XP_GetAdminStr(DBT_cgiError21), "request method");
       return NULL;
    }
    env[x++] = util_env_str("REQUEST_METHOD", t);

    if (args)
        env[x++] = util_env_str("QUERY_STRING", args);

    // set REQUEST_URI
    if (rq->orig_rq) {
        t = get_request_uri(rq->orig_rq);
    } else {
        t = get_request_uri(rq);
    }
    if (t) {
        env[x++] = util_env_str("REQUEST_URI", t);
        FREE(t);
    }

    if (scriptVars) {
        t = pblock_findval("uri", pb);

        /* Simulate CGI URIs by truncating path info */
        if ((u = pblock_findval("path-info", rq->vars))) {
            y = strlen(t) - strlen(u);
            if (y >= 0) {
                c = t[y];
                t[y] = '\0';
            }
            env[x++] = util_env_str("SCRIPT_NAME", t);
            if (y >= 0) {
                t[y] = c;
            }

            env[x++] = util_env_str("PATH_INFO", u);
            if((t = INTservact_translate_uri2(u, sn, rq))) {
                env[x++] = util_env_str("PATH_TRANSLATED", t);
                // keep path-translated in rq->vars since we may need it
                // during fastcgi processing
                pblock_nvinsert("path-translated", t, rq->vars);
                FREE(t);
            }
        } else {
            env[x++] = util_env_str("SCRIPT_NAME", t);
        }

        if (t = pblock_findval("path", rq->vars))
            env[x++] = util_env_str("SCRIPT_FILENAME", t);
    }

    env[x] = NULL;
    return env;
}

//-----------------------------------------------------------------------------
// cgi_ssi_vars
//
// passing SSI variables as environment vars for SSI CGIs -- gdong 
//-----------------------------------------------------------------------------

static char** cgi_ssi_vars(Session *sn, Request *rq, const char *args,
                           char** env)
{
    pblock *param = ((NSAPIRequest *) rq)->param;

    if (param) {
        int hi;

        // Count the number of parameters in the param pblock
        int nparam = 0;
        for (hi = 0; hi < param->hsize; hi++) {
            for (pb_entry *pe = param->ht[hi]; pe; pe = pe->next)
                nparam++;
        }

        // Make room for the parameters in env[]
        int x;
        env = util_env_create(env, nparam, &x);

        // Add the parameters to env[]
        for (hi = 0; hi < param->hsize; hi++) {
            for (pb_entry *pe = param->ht[hi]; pe; pe = pe->next)
                env[x++] = util_env_str(pe->param->name, pe->param->value);
        }

        env[x] = NULL;
    }

    return env;
}

//-----------------------------------------------------------------------------
// cgi_env_str_index
//-----------------------------------------------------------------------------

static char* cgi_env_str_index(char* name, int index, char* value) 
{
    char* t;

    /* allocate enough extra to accomodate the index */
    t = (char*)MALLOC(strlen(name) + strlen(value) + 20);
    if (t) sprintf(t, "%s_%d=%s", name, index, value);
    return t;
}

//-----------------------------------------------------------------------------
// cgi_cert_ava_to_env
//-----------------------------------------------------------------------------

static void cgi_cert_ava_to_env(void *cert, int which_dn, const char *attr,
                                const char *envvar, int add_index,
                                char **env, int *x, int xmax)
{
    /* NOTE:  This function is also duplicated in ntcgi.cpp */
    char **val = 0;

    if (ldapu_get_cert_ava_val(cert, which_dn, attr, &val) == LDAPU_SUCCESS) {
        if (add_index) {
            int i;
            for(i=0; val[i]; i++) {
                if ((*x) > xmax) break;
                env[(*x)++] = cgi_env_str_index((char *)envvar, i, val[i]);
            }
        } else {
            if ((*x) <= xmax) 
                env[(*x)++] = util_env_str((char *)envvar, val[0]);
        }
        ldapu_free_cert_ava_val(val);
    }
}
 
//-----------------------------------------------------------------------------
// cgi_client_auth_vars
//-----------------------------------------------------------------------------

static char** cgi_client_auth_vars(Session *sn, Request *rq, char** env)
{
    char *t;

    int x;
    env = util_env_create(env, MAX_CGI_CLIENT_AUTH_VARS, &x);
    int xmax = x + MAX_CGI_CLIENT_AUTH_VARS - 1;

    if ((t = pblock_findval("auth-cert", rq->vars))) {
        /* Alan made up this environment variable name */
        env[x++] = util_env_str("CLIENT_CERT", t);
    }

    void *cert;
    if (sn && sn->clauth && (cert=((ClAuth_t*)sn->clauth)->cla_cert)) {
        /* Get the subject DN */
        if (ldapu_get_cert_subject_dn(cert, &t) == LDAPU_SUCCESS) {
            env[x++] = util_env_str("CLIENT_CERT_SUBJECT_DN", t);
            ldapu_free(t);
        }

        /* Get the issuer DN */
        if (ldapu_get_cert_issuer_dn(cert, &t) == LDAPU_SUCCESS) {
            env[x++] = util_env_str("CLIENT_CERT_ISSUER_DN", t);
            ldapu_free(t);
        }

        if (ldapu_get_cert_start_date(cert, &t) == LDAPU_SUCCESS) {
            env[x++] = util_env_str("CLIENT_CERT_VALIDITY_START", t);
            ldapu_free(t);
        }
        if (ldapu_get_cert_end_date(cert, &t) == LDAPU_SUCCESS) {
            env[x++] = util_env_str("CLIENT_CERT_VALIDITY_EXIRES", t);
            ldapu_free(t);
        }
        if (ldapu_get_cert_algorithm(cert, &t) == LDAPU_SUCCESS) {
            env[x++] = util_env_str("CLIENT_CERT_ALGORITHM", t);
            ldapu_free(t);
        }

        /* reflect the revocation status and methode if they exist */
        if (t = pblock_findval("revocation_method", sn->client)) {
            env[x++] = util_env_str("REVOCATION_METHOD", t);
        }
        
        if (t = pblock_findval("revocation_status", sn->client)) {
            env[x++] = util_env_str("REVOCATION_STATUS", t);
        }

        /* Get vars from the subject DN */
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "CN", 
                            "CLIENT_CERT_SUBJECT_CN", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "OU",
                            "CLIENT_CERT_SUBJECT_OU_", 1, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "O",
                            "CLIENT_CERT_SUBJECT_O", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "C",
                            "CLIENT_CERT_SUBJECT_C", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "L",
                            "CLIENT_CERT_SUBJECT_L", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "ST",
                            "CLIENT_CERT_SUBJECT_ST", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "E",
                            "CLIENT_CERT_SUBJECT_E", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_SUBJECT_DN, "UID",
                            "CLIENT_CERT_SUBJECT_UID", 0, env, &x, xmax);

        /* Get vars from the issuer DN */
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "CN",
                            "CLIENT_CERT_ISSUER_CN", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "OU",
                            "CLIENT_CERT_ISSUER_OU_", 1, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "O",
                            "CLIENT_CERT_ISSUER_O", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "C",
                            "CLIENT_CERT_ISSUER_C", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "L",
                            "CLIENT_CERT_ISSUER_L", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "ST",
                            "CLIENT_CERT_ISSUER_ST", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "E",
                            "CLIENT_CERT_ISSUER_E", 0, env, &x, xmax);
        cgi_cert_ava_to_env(cert, LDAPU_ISSUER_DN, "UID",
                            "CLIENT_CERT_ISSUER_UID", 0, env, &x, xmax);

        /*
         * load all the cert extensions.
         */
        void* handle;
        char* ext;
        char* value;
        if (ldapu_get_first_cert_extension(cert, &handle, &ext, &value) 
            == LDAPU_SUCCESS)
        {
            do {
                // If there's still room in env...
                if (x <= xmax) {
                    static const char certExt[] = "CLIENT_CERT_EXTENSION";

                    /* allocate enough extra to accomodate the index */
                    t = (char*)MALLOC(sizeof(certExt) + strlen(ext) 
                                      + strlen(value) + 2);
                    if (t) sprintf(t, "%s%s=%s", certExt, ext, value);
                    env[x++] = t;
                }

                ldapu_free(ext);
                ldapu_free(value);

            } while (ldapu_get_next_cert_extension(handle,&ext,&value) 
                     == LDAPU_SUCCESS);
            ldapu_cert_extension_done(handle);
        }
    }

    env[x] = NULL;

    return env;
}

//-----------------------------------------------------------------------------
// cgi_env_init_name
//-----------------------------------------------------------------------------

static char* cgi_env_init_name(const char *name)
{
    /* Check if the user specified a value for name on the init-cgi line */
    if (_initenv) {
        pb_param *pp = pblock_remove(name, _initenv);
        if (pp) {
            /*
             * Remove the value from _initenv; it will now be stored as
             * _env_path, _env_tz, etc.
             */
            char *v = PERM_STRDUP(pp->value);
            param_free(pp);
            return v;
        }
    }

    /* If the user didn't specify a value, check our environment */
    return getenv(name);
}

//-----------------------------------------------------------------------------
// cgi_env_init
//
// Grab the envvars the server needs to pass on to child CGI processes.  Saves
// us the getenv() overhead later on.
//-----------------------------------------------------------------------------

static void cgi_env_init()
{
    _env_path = cgi_env_init_name("PATH");
    _env_tz = cgi_env_init_name("TZ");
    _env_lang = cgi_env_init_name("LANG");
    _env_ldpath = cgi_env_init_name(LIBRARY_PATH);

    _env_initialized = PR_TRUE;
}

//-----------------------------------------------------------------------------
// cgi_common_vars
//-----------------------------------------------------------------------------

char** cgi_common_vars(Session *sn, Request *rq, char **env) 
{
    char *t;
    int x;

    env = util_env_create(env, MAX_CGI_COMMON_VARS, &x);

    if (!_env_initialized) cgi_env_init();
    if (_env_path) env[x++] = util_env_str("PATH", _env_path);
    if (_env_tz) env[x++] = util_env_str("TZ", _env_tz);
    if (_env_lang) env[x++] = util_env_str("LANG", _env_lang);
    if (_env_ldpath) env[x++] = util_env_str(LIBRARY_PATH, _env_ldpath);
    env[x++] = util_env_str("SERVER_SOFTWARE", PRODUCT_HEADER_ID"/"PRODUCT_VERSION_ID);

    NSString srvName, portStr;
    char buf1[256], buf2[64];
    srvName.useStatic(buf1, sizeof(buf1), 0); 
    portStr.useStatic(buf2, sizeof(buf2), 0); 
    GetServerHostnameAndPort(*rq, *sn, srvName, portStr);
    env[x++] = util_env_str("SERVER_PORT", (char*)(const char*)portStr);
    env[x++] = util_env_str("SERVER_NAME", (char*)(const char*)srvName);

    t = http_uri2url_dynamic("","",sn,rq);
    env[x++] = util_env_str("SERVER_URL", t);
    FREE(t);

    char *ip;
    ip = pblock_findval("ip", sn->client);
    t = session_dns(sn);
    env[x++] = util_env_str("REMOTE_HOST", (t ? t : ip));
    env[x++] = util_env_str("REMOTE_ADDR", ip);

    if((t = pblock_findval("auth-user", rq->vars))) {
        env[x++] = util_env_str("REMOTE_USER", t);
         if((t = pblock_findval("auth-type", rq->vars))) {
             env[x++] = util_env_str("AUTH_TYPE", t);
        }
        if((t = pblock_findval("auth-userdn", rq->vars))) {
            env[x++] = util_env_str("REMOTE_USERDN", t);
        }
    }
    if((t = pblock_findval("password-policy", rq->vars))) {
        /* chrisk made up this variable name */
        env[x++] = util_env_str("PASSWORD_POLICY", t);
    }

    // Handle Apache ErrorDocument-style variables from the send-error SAF
    if (rq->orig_rq != rq) {
        if (t = pblock_findval("uri", rq->orig_rq->reqpb)) {
            env[x++] = util_env_str("REDIRECT_URL", t);
        }
        if (t = pblock_findval("status", rq->orig_rq->srvhdrs)) {
            env[x++] = util_env_str("REDIRECT_STATUS", t);
        }
    }

#if defined(NET_SSL)
    if (GetSecurity(sn)) {
        env[x++] = util_env_str("HTTPS", "ON");

        if (t = pblock_findval("keysize", sn->client))
            env[x++] = util_env_str("HTTPS_KEYSIZE", t);

        if (t = pblock_findval("secret-keysize", sn->client))
            env[x++] = util_env_str("HTTPS_SECRETKEYSIZE", t);

        t = pblock_findval("ssl-id", sn->client);
        env[x++] = util_env_str("HTTPS_SESSIONID", t ? t : (char *)"");

        unsigned char random_bytes[NUM_RANDOM_BYTES + 2];
        char random_string[NUM_RANDOM_BYTES*2 + 2];
        PK11_GenerateRandom(random_bytes, NUM_RANDOM_BYTES);

        int i;
        for(i = 0; i < NUM_RANDOM_BYTES; i++)  {
            sprintf(&random_string[i*2], "%02x", 
                    (unsigned int)(random_bytes[i] & 0xff));
        }
        random_string[NUM_RANDOM_BYTES*2] = '\0';
        env[x++] = util_env_str("HTTPS_RANDOM", random_string);

    } else {
        env[x++] = util_env_str("HTTPS", "OFF");
    }
#else
    env[x++] = util_env_str("HTTPS", "OFF");
#endif // NET_SSL

    env[x] = NULL;

    env = cgi_client_auth_vars(sn, rq, env);

    return env;
}

//-----------------------------------------------------------------------------
// cgi_scan_headers_failure
//-----------------------------------------------------------------------------

static int cgi_scan_headers_failure(Session* sn, Request* rq, const char *fmt, ...)
{
    char err[REQ_MAX_LINE];
    va_list args;

    va_start(args, fmt);

    util_vsnprintf(err, sizeof(err), fmt, args);
    log_error(LOG_FAILURE, "cgi_scan_headers", sn, rq, 
              XP_GetAdminStr(DBT_cgiError1),
              pblock_findval("path", rq->vars), err);

    va_end(args);

    return REQ_ABORTED;
}

//-----------------------------------------------------------------------------
// cgi_scan_headers
//-----------------------------------------------------------------------------

static int cgi_scan_headers(Session* sn, Request* rq, void* buf)
{
    register int x,y;
    register char c;
    int nh, i;
    char t[REQ_MAX_LINE];
    char* statusHeader = pblock_findval("status", rq->srvhdrs);

    nh = 0;
    x = 0; y = -1;

    for (;;) {
        if (x >= REQ_MAX_LINE) {
            return cgi_scan_headers_failure(sn, rq, "CGI header line too long (max %d)", REQ_MAX_LINE);
        }

        i = netbuf_getc((netbuf*)buf);

        if (i == IO_ERROR) {
            return cgi_scan_headers_failure(sn, rq, "read failed, error is %s", system_errmsg());
        } else if (i == IO_EOF) {
            return cgi_scan_headers_failure(sn, rq, "program terminated without a valid CGI header. Check for core dump or other abnormal termination");
        }

        c = (char) i;
        switch(c) {
        case CR:
            // Silently ignore CRs
            break;

        case LF:
            if (x == 0)
                return REQ_PROCEED;

            if (nh >= HTTP_MAX_HEADERS) {
                return cgi_scan_headers_failure(sn, rq, "too many headers from CGI script (max %d)", HTTP_MAX_HEADERS);
            }

            t[x] = '\0';
            if(y == -1) {
                return cgi_scan_headers_failure(sn, rq, "name without value: got line \"%s\"", t);
            }
            while(t[y] && isspace(t[y])) ++y;

            // Do not change the status header to 200 if it was already set
            // This would happen only if it were a cgi error handler
            // and so the status had been already set on the request
            // originally
            if (!statusHeader || // If we don't already have a Status: header
                PL_strcmp(t, "status") || // or this isn't a Status: header
                PL_strncmp(&t[y], "200", 3)) // or this isn't "Status: 200"
            {
                pblock_nvinsert(t, &t[y], rq->srvhdrs);
            }

            x = 0;
            y = -1;
            ++nh;
            break;

        case ':':
            if(y == -1) {
                y = x+1;
                c = '\0';
            }

        default:
            t[x++] = ((y == -1) && isupper(c) ? tolower(c) : c);
        }
    }
}

//-----------------------------------------------------------------------------
// cgi_send_content
//-----------------------------------------------------------------------------

static int cgi_send_content(Session *sn, Request *rq, SYS_NETFD s2c)
{
    int cl;

    const char *cls = pblock_findkeyval(pb_key_content_length, rq->headers);
    if (cls) {
        cl = atoi(cls);
        if (cl < 0) {
            log_error(LOG_WARN, "send-cgi", sn, rq, 
                      XP_GetAdminStr(DBT_cgiError2), cls);
            return REQ_ABORTED;
        }
    } else if (pblock_findkeyval(pb_key_transfer_encoding, rq->headers)) {
        cl = -1;
    } else {
        cl = 0;
    }

    int res;

    if (cl == 0) {
        // No request body
        res = REQ_NOACTION;
    } else if (netbuf_buf2sd(sn->inbuf, s2c, cl) == IO_ERROR) {
        if (sn->inbuf->pos == sn->inbuf->cursize) {
            // Error receiving request body
            log_error(LOG_VERBOSE, "send-cgi", sn, rq, 
                      "error receiving content from client (%s)",
                      system_errmsg());
            protocol_status(sn, rq, PROTOCOL_REQUEST_TIMEOUT, NULL);
            res = REQ_ABORTED;
        } else {
            int degree;
            if (PR_GetError() == PR_CONNECT_RESET_ERROR) {
                // CGI closed stdin before it read the entire body
                degree = LOG_VERBOSE;
                res = REQ_PROCEED;
            } else {
                // Unexpected error writing to CGI's stdin
                protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
                degree = LOG_FAILURE;
                res = REQ_ABORTED;
            }
            log_error(degree, "send-cgi", sn, rq,
                      XP_GetAdminStr(DBT_cgiError3),
                      system_errmsg());
        }
    } else {
        // Successfully sent request body to CGI's stdin
        res = REQ_PROCEED;
    }

    PR_Close(s2c);

    return res;
}

//-----------------------------------------------------------------------------
// cgi_terminate
//-----------------------------------------------------------------------------

static void cgi_terminate(void *unused)
{
    log_ereport(LOG_VERBOSE, "Entered cgi_terminate");

#if defined(XP_WIN32)
    if (_initenv_vars) {
        util_env_free(_initenv_vars);
        _initenv_vars = NULL;
    }
#endif // XP_WIN32

    if (_initenv) {
        pblock_free(_initenv);
        _initenv = NULL;
    }

    log_ereport(LOG_VERBOSE, "Exiting cgi_terminate");
}

//-----------------------------------------------------------------------------
// cgi_load_server_vars
//-----------------------------------------------------------------------------

#if defined(XP_WIN32)
static void cgi_load_server_vars()
{
    char **env;
    int env_count = 0;
    int index, pos;
    extern char **_environ;

    /* Count the number of env vars */

    if (_environ)
        for (env = _environ; *env; env++, env_count++);

    if (env_count == 0) return;

    _initenv_vars = util_env_create(NULL, env_count+1, &pos);

    for (index = 0; index < env_count; index++) 
        _initenv_vars[index] = STRDUP(_environ[index]);
    _initenv_vars[index] = NULL;
}
#endif // XP_WIN32

//-----------------------------------------------------------------------------
// cgi_add_var
//-----------------------------------------------------------------------------

void cgi_add_var(const char *name, const char *value)
{
    if (!_initenv)
        _initenv = pblock_create(7);

    param_free(pblock_remove(name, _initenv));

    if (value)
        pblock_nvinsert(name, value, _initenv);
}

//-----------------------------------------------------------------------------
// cgi_set_timeout
//-----------------------------------------------------------------------------

void cgi_set_timeout(PRIntervalTime timeout)
{
    _timeout = timeout;
}

//-----------------------------------------------------------------------------
// cgi_set_idle_timeout
//-----------------------------------------------------------------------------

void cgi_set_idle_timeout(PRIntervalTime timeout)
{
    _iotimeout = timeout;
}

//-----------------------------------------------------------------------------
// cgi_get_idle_timeout
//-----------------------------------------------------------------------------

PRIntervalTime cgi_get_idle_timeout(void)
{
    return _iotimeout;
}

//-----------------------------------------------------------------------------
// cgi_init
//-----------------------------------------------------------------------------

int cgi_init(pblock *pb, Session *sn, Request *rq)
{
    /* VB: cgi-init needs to be lateinit. So if we get called as 
     * an early-init function, just mark ourself for lateinit and
     * return REQ_PROCEED from the early-init
     */
    if (!conf_is_late_init(pb)) {
        pblock_nvinsert("LateInit", "yes", pb);
        return REQ_PROCEED;
    }
    param_free(pblock_remove("LateInit", pb));

    int retcode = 0;

    /* tell the server to clean us up */
    daemon_atrestart(cgi_terminate, NULL);

    /* JRP - remove server-version, otherwise the CGI will get it in the 
       environment, and the proper variable for this is SERVER_SOFTWARE */
    param_free(pblock_remove("server-version", pb));

#if defined(XP_UNIX)
    char* execPath = 0;
    int minChildren = 0;
    int maxChildren = 0;
    PRIntervalTime reapInterval = 0;
    pb_param* pservExec;
    pb_param* pservMinChild;
    pb_param* pservMaxChild;
    pb_param* pservReapInt;

    /*
     *  init the child stub subsystem
     */
    if (pservExec = pblock_remove("cgistub-path", pb)) {
        cgistub_set_path(pservExec->value);
    }
    if (pservMinChild = pblock_remove("minchildren", pb)) {
        cgistub_set_min_children(atoi(pservMinChild->value));
    }
    if (pservMaxChild = pblock_remove("maxchildren", pb)) {
        cgistub_set_max_children(atoi(pservMaxChild->value));
    }
    if (pservReapInt = pblock_remove("reapinterval", pb)) {
        PRIntervalTime timeout = util_getinterval(pservReapInt->value, 0);
        if (timeout != 0)
            cgistub_set_idle_timeout(timeout);
    }

    if (cgistub_init() != PR_SUCCESS)
        return REQ_ABORTED;

    param_free(pservExec);
    param_free(pservMinChild);
    param_free(pservMaxChild);
    param_free(pservReapInt);
#endif // XP_UNIX

    if (!_initenv) _initenv = pblock_create(7);
    pblock_copy(pb, _initenv);

    char* timeout = pblock_findval("timeout", _initenv);
    if (timeout) {
        PRIntervalTime interval = util_getinterval(timeout, 0);
        if (interval != 0)
            _iotimeout = interval;
        pblock_nvinsert("TIMEOUT", timeout, _initenv);
        param_free(pblock_remove("timeout", _initenv));
    }

    char* lingertimeout = pblock_findval("cgi_linger_timeout", _initenv);
    if (lingertimeout) {
        PRIntervalTime interval = util_getinterval(lingertimeout, 0);
        if (interval != 0)
            child_set_term_timeout(interval);
        param_free(pblock_remove("cgi_linger_timeout", _initenv));
    }

#if defined(XP_WIN32)
    if (pblock_findval("load-server-vars", _initenv)) cgi_load_server_vars();
#endif // XP_WIN32

    param_free(pblock_remove("fn", _initenv));

    cgi_env_init();

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// cgi_parse_output
//
// Returns -1 on error, 0 for proceed normally, and 1 for dump output and 
// restart request.
//-----------------------------------------------------------------------------

static int cgi_parse_output(void *buf, Session *sn, Request *rq)
{
    char *s;
    char *l;

    if(cgi_scan_headers(sn, rq, buf) == REQ_ABORTED) {
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return -1;
    }

    l = pblock_findval("location", rq->srvhdrs);

    if((s = pblock_findval("status", rq->srvhdrs))) {
        if((strlen(s) < 3) || 
           (!isdigit(s[0]) || (!isdigit(s[1])) || (!isdigit(s[2]))))
        {
            log_error(LOG_WARN, "cgi_parse_output", sn, rq, 
                      XP_GetAdminStr(DBT_cgiError17), s);
            s = NULL;
        }
        else {
          char ch = s[3];
          s[3] = '\0';
          int statusNum = atoi(s);
          s[3] = ch;

          rq->status_num = statusNum;
        }
    }
    if(l && (!util_is_url(l)))
        return 1;

    if(!s) {
        if (l)
            pblock_nvinsert("url", l, rq->vars);
        protocol_status(sn, rq, (l ? PROTOCOL_REDIRECT : PROTOCOL_OK), NULL);
    }

    return 0;
}

//-----------------------------------------------------------------------------
// _find_exe_path
//
// _find_exe_path takes a path and returns its file association
//-----------------------------------------------------------------------------

#if defined(XP_WIN32)
static char *_find_exe_path(char *path)
{
    HINSTANCE result;
    char *association;

    if (path == NULL)
        return NULL;

    association = (char *)MALLOC(MAX_PATH);

    result = FindExecutable(path, NULL, association);
    if (result <= (HINSTANCE)32) {
        FREE(association);
        if (result == (HINSTANCE)31) {
            /* No association is present */
            return NULL;
        } else {
            /* some other error returned by the call. record it. */
             ereport(LOG_WARN, XP_GetAdminStr(DBT_cgiError45), path, result);
             return NULL;
        }
    } else {
        return association;
    }
}
#endif // XP_WIN32

//-----------------------------------------------------------------------------
// _build_cwd
//-----------------------------------------------------------------------------

#if defined(XP_WIN32)
char* _build_cwd(char *path)
{
    char *new_path;
    char *ptr;

    if (!path)
        return NULL;

    if (*path == 0) {
        /* Make sure it can hold at least 2 chars -- for NT */
        new_path = (char *)MALLOC(2);
        *new_path = 0;
    }
    else {
        /* intentionally overallocate; we don't really care */
        new_path = (char *)STRDUP(path);
    }

    if (new_path) {
        if ( (ptr = strrchr(new_path, '/')) ) 
            *ptr = '\0';
        else if  ( (ptr = strrchr(new_path, '\\')) ) 
            *ptr = '\0';
        else
            *new_path = 0;

        if (*new_path == 0) {
            /* NT doesn't like the NULL path when used in CreateProcess */
            *new_path = '\\';
            *(new_path+1) = 0;
        }
    }

    return new_path;
}
#endif // XP_WIN32

//-----------------------------------------------------------------------------
// _cgi_is_dangerous
//
// Check for .BAT or .CMD scripts which might have bad args passed to them.
// Returns 1 if the request looks "dangerous", 0 otherwise.
//-----------------------------------------------------------------------------

#if defined(XP_WIN32)
static int _cgi_is_dangerous(char *path, const char * const *argv, pblock *pb)
{
    /* Fix .BAT/.CMD security hole
     * Note- everything has been unescaped at this point 
     * Note- path has all forward slashes at this point
     */
    char *ext, *last_slash;

    if (!(ext = strrchr(path, '.')))
        return 0;

    /* Do a quick check for ".[bB]" or ".[cC]" */
    if ( ext[1] != 'b' && ext[1] != 'B' &&
         ext[1] != 'c' && ext[1] != 'C')
        return 0;

    last_slash = strrchr(path, '/');

    if ( ext > last_slash ) {
        ext++;
        /* A ".bat" pattern will only be allowed if the character following the
         * ".bat" is alphanumeric or ".".  Paths which end with "." have 
         * already been weeded out in the clean-uri function.
         */
        if ( (!strncasecmp(ext, "bat", 3) ||
              !strncasecmp(ext, "cmd", 3)) &&
             (!ext[3] || (!isalpha(ext[3]) && !isdigit(ext[3])) && ext[3] != '.') ) {

            /* ok, this is a .BAT script.  Disallow &, |, <, >.
             * Allow these if 'dangerous' is enabled in the obj.conf.
             */
            if (!pblock_findval("allow-dangerous", pb)) {
                for (const char * const *a = argv; *a; a++) {
                    if (strpbrk(*a, "&|<>"))
                        return 1;
                }
            }
        }
    }
    return 0;
}
#endif // XP_WIN32

//-----------------------------------------------------------------------------
// cgi_get_param
//-----------------------------------------------------------------------------

static const char* cgi_get_param(const char *name, pblock *pb)
{
    // Return the value from the pblock if it exists and has non-zero length
    const char* value = pblock_findval(name, pb);
    if (value && *value) return value;
    return NULL;
}

//-----------------------------------------------------------------------------
// cgi_start_exec
//-----------------------------------------------------------------------------

static int cgi_start_exec(pblock *pb, Session *sn, Request *rq,
                          char *path, Child **pchild, PRFileDesc **pc2s)
{
    // Make sure CGI program exists
    NSFCFileInfo *finfo;
    if (request_info_path(path, rq, &finfo) != PR_SUCCESS) {
        log_error(LOG_WARN, "send-cgi", sn, rq,
                  XP_GetAdminStr(DBT_cgiError6), path, system_errmsg());
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }
    if (finfo->pr.type != PR_FILE_FILE) {
        log_error(LOG_WARN, "send-cgi", sn, rq,
                  XP_GetAdminStr(DBT_cgiError7), path);
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }

    // The query string is converted to command line arguments for some CGI 
    // programs
    const char *args = pblock_findval("query", rq->reqpb);

    // Chop the pathname from the file before putting it in argv[0]
    const char *argv0 = strrchr(path, FILE_PATHSEP);
    if (argv0) {
        argv0++;
    } else {
        argv0 = path;
    }

    // If we're executing a script, path points to the interpreter and
    // shellcgi-path points to the script
    const char *shellcgi_path = pblock_findval("shellcgi-path", rq->vars);

    // Create argv array from the program name, script name, and query string
    char **argv = cgi_create_argv(argv0, shellcgi_path, args);

#ifdef XP_WIN32
    if (argv && _cgi_is_dangerous(path, argv, pb)) {
        log_error(LOG_SECURITY, "send-cgi", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError8));	
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }
#endif

    // Setup the environment for the CGI program
    char **env;
    env = http_hdrs2env(rq->headers);
    env = cgi_common_vars(sn, rq, env);
    env = cgi_specific_vars(sn, rq, args, env, 1); // 1 implies add all variables
    if(env == NULL) {
        log_error(LOG_FAILURE, "send-cgi", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError21), "cgi specific variables");
        return REQ_ABORTED;
    }
    env = cgi_ssi_vars(sn, rq, args, env);
    if (_initenv)
        env = pblock_pb2env(_initenv, env);

    Child *child = child_create(sn, rq, path);
    if (!child) {
        log_error(LOG_FAILURE, "send-cgi", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError25), path, system_errmsg());
        return REQ_ABORTED;
    }

    PRFileDesc *s2c = child_pipe(child, PR_StandardInput, _iotimeout);
    PRFileDesc *c2s = child_pipe(child, PR_StandardOutput, _iotimeout);

    // Set up options
    ChildOptions opts;
    opts.dir = cgi_get_param("dir", pb);
    opts.root = cgi_get_param("chroot", pb);
    opts.user = cgi_get_param("user", pb);
    opts.group = cgi_get_param("group", pb);
    opts.nice = cgi_get_param("nice", pb);;
    opts.rlimit_as = cgi_get_param("rlimit_as", pb);
    opts.rlimit_core = cgi_get_param("rlimit_core", pb);
    opts.rlimit_cpu = cgi_get_param("rlimit_cpu", pb);
    opts.rlimit_nofile = cgi_get_param("rlimit_nofile", pb);

    // Start the child
    PRStatus rv = child_exec(child, argv, env, &opts, _timeout);
    if (rv != PR_SUCCESS) {
        log_error(LOG_FAILURE, "send-cgi", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError25), path, system_errmsg());
        return REQ_ABORTED;
    }

    // Send request body to child
    if (cgi_send_content(sn, rq, s2c) == REQ_ABORTED) {
        child_term(child);
        return REQ_ABORTED;
    }

    // Let caller know about the child
    *pchild = child;
    *pc2s = c2s;

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// cgi_send
//-----------------------------------------------------------------------------

int cgi_send(pblock *pb, Session *sn, Request *rq)
{
    char *      path;
    char        num[16];
    SYS_NETFD   cfd;
    SYS_NETFD   tfd = 0;
    netbuf *    buf;
    int         restart = 0;
    int         ns;
    int         is_nph = 0;

    path = pblock_findval("path", rq->vars); 

    param_free(pblock_remove("content-type", rq->srvhdrs));

    // Start the CGI
    Child *child;
    if (cgi_start_exec(pb, sn, rq, path, &child, &cfd) == REQ_ABORTED)
        return REQ_ABORTED;

    buf = netbuf_open(cfd, NET_BUFFERSIZE);

    // Check whether the program name begins with "nph-"
    char *slash;
#ifdef XP_WIN32
    char *program = pblock_findval("shellcgi-path", rq->vars);
    if (!program)
        program = path;
    slash = strrchr(program, FILE_PATHSEP);
    if (!slash)
        slash = strrchr(program, '\\');
#else
    slash = strrchr(path, FILE_PATHSEP);
#endif
    if (slash && !strncmp(slash + 1, "nph-", 4))
        is_nph = 1;

    if (is_nph) {
        tfd = sn->csd;
    } else {
        switch(cgi_parse_output(buf, sn, rq)) {
        case -1:
            netbuf_close(buf);
            child_term(child);
            return REQ_ABORTED;
        case 0:
            tfd = sn->csd;
            restart = 0;
            break;
        case 1:
            tfd = SYS_NET_ERRORFD;
            restart = 1;
        }
    }

    if(is_nph) {
        rq->senthdrs = 1;
        protocol_status(sn, rq, PROTOCOL_OK, NULL);
    }

    if(tfd == sn->csd)
        httpfilter_buffer_output(sn, rq, PR_TRUE);

    ns = netbuf_buf2sd(buf, tfd, -1);

    if( ns == IO_ERROR) {
        log_error(LOG_FAILURE, "send-cgi", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError19),
                  buf->errmsg);
        netbuf_close(buf);
        child_term(child);
        return REQ_ABORTED;
    }
    else {
        util_itoa(ns, num);
        pblock_nvinsert("content-length", num, rq->srvhdrs);
    }

    netbuf_close(buf);
    child_done(child);

    if(restart) {
        pb_param *l = pblock_remove("location", rq->srvhdrs);
        return request_restart_location(sn, rq, l->value);
    }

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// shellcgi_send
//-----------------------------------------------------------------------------

#if defined(XP_WIN32)
int shellcgi_send(pblock *pb, Session *sn, Request *rq)
{
    pb_param *ppath = pblock_find("path", rq->vars);
    pb_param *pargs = pblock_find("query", rq->reqpb);

    char *path = ppath->value;
    char *args = NULL;

    char *shell_exe_path = _find_exe_path(path);
    struct stat fi;

    if (shell_exe_path) {
        /* Check to see if the associated file actually exists
         * This check may have been covered by the FindExecutable call */

        if(system_stat(shell_exe_path, &fi) == -1) {
            protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
            log_error(LOG_FAILURE, "shellcgi-send", sn, rq, 
                      XP_GetAdminStr(DBT_cgiError21), shell_exe_path);
            return REQ_ABORTED;
        }

        if(pargs)
            args = pargs->value;
                    
        /* We are going to override the old path with the path for the
         * executable.  But, in order to avoid us changing the query variable
         * and in order to get the program arguments right, we'll need to
         * know what the shellcgi-path originally was.  Save it in 
         * shellcgi-path.
         */
        pblock_nvinsert("shellcgi-path", path, rq->vars);
        FREE(ppath->value);
        ppath->value = shell_exe_path;

        return(cgi_send(pb, sn, rq));
    } else {
 
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        log_error(LOG_FAILURE, "shellcgi-send", sn, rq, 
                  XP_GetAdminStr(DBT_cgiError22), path);
        return REQ_ABORTED;
    }
}
#endif // XP_WIN32

//-----------------------------------------------------------------------------
// cgi_query
//-----------------------------------------------------------------------------

int cgi_query(pblock *pb, Session *sn, Request *rq)
{
    pb_param *rqpath = pblock_find("path", rq->vars);
    char *path = pblock_findval("path", pb);

    if (!path) 
        return REQ_ABORTED;

    if (rqpath) {
        FREE(rqpath->value);
        rqpath->value = STRDUP(path);
    }
    else {
        pblock_nvinsert("path", path, rq->vars);
    }

    if(!(pblock_findval("path-info", rq->vars)))
        pblock_nvinsert("path-info", pblock_findval("uri", rq->reqpb), 
                        rq->vars);

    return cgi_send(NULL, sn, rq);
}

//-----------------------------------------------------------------------------
// GetCgiInitEnv
//-----------------------------------------------------------------------------

const pblock* GetCgiInitEnv()
{
    return _initenv;
}

