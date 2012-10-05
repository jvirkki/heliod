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
 * conf.cpp: Legacy NSAPI globals, NSAPI Init, and magnus.conf processing
 * 
 * Rob McCool
 */

#include "netsite.h"
#include "support/stringvalue.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/vsconf.h"
#include "base/util.h"
#include "base/ereport.h"
#include "base/servnss.h"
#include "frame/dbtframe.h"
#include "frame/func.h"
#include "frame/error.h"
#include "frame/http_ext.h"
#include "frame/conf_api.h"
#include "frame/conf.h"


/**
 * ThreadGlobalsData records the NSAPI "globals" for a given request processing
 * thread.  (As a result of virtualization, what were once global variables now
 * vary by virtual server, so we maintain a separate set of globals for each
 * thread.)
 */
struct ThreadGlobalsData {
    HttpRequest *hrq;
    const VirtualServer *vs;
    PRBool initialized;
    conf_global_vars_s vars; // Valid only when initialized is set
};

static conf_global_vars_s _conf_globals;
static char *_conf_magnus_filename;
static PRUintn _conf_globals_key = (PRUintn)-1;
static PRBool _conf_initialized;


/* ----------------------- get_thread_globals_data ------------------------ */

static ThreadGlobalsData *get_thread_globals_data()
{
    ThreadGlobalsData *data = (ThreadGlobalsData *)PR_GetThreadPrivate(_conf_globals_key);
    if (data == NULL) {
        data = (ThreadGlobalsData *)PERM_CALLOC(sizeof(ThreadGlobalsData));
        PR_SetThreadPrivate(_conf_globals_key, data);
    }
    return data;
}


/* --------------------- destroy_thread_globals_data ---------------------- */

PR_BEGIN_EXTERN_C

static void destroy_thread_globals_data(void* ptr)
{
    ThreadGlobalsData *data = (ThreadGlobalsData *)ptr;
    PERM_FREE(data);
}

PR_END_EXTERN_C


/* ------------------------------ conf_init ------------------------------- */

static void conf_init()
{
    if (_conf_initialized)
        return;

    _conf_initialized = PR_TRUE;

    // Private index for thread-specific "globals"
    PRStatus rv = PR_NewThreadPrivateIndex(&_conf_globals_key, destroy_thread_globals_data);
    PR_ASSERT(rv == PR_SUCCESS);

    conf_global_vars_s& vars = _conf_globals;

    vars.vs_config_file = PERM_STRDUP(SERVERXML_FILENAME);

    // Various globals defaults
    vars.Vpool_max = 1;
    vars.digest_stale_timeout = 120;
    vars.wait_for_cgi = 1;

    // Track magnus.conf directives
    conf_initGlobal();
}


/* ------------------------ conf_init_true_globals ------------------------ */

NSAPI_PUBLIC void conf_init_true_globals(const ServerXMLSchema::Server& server)
{
    //
    // DO NOT ADD NEW MEMBERS TO conf_global_vars_s!
    //
    // The only stuff that belongs in conf_global_vars_s is the stuff that's
    // minimally required for NSAPI binary compatibility.  Server-internal
    // configuration belongs in Configuration.  To expose new configuration
    // information via NSAPI, bump the NSAPI version and introduce a new
    // accessor function.
    //

    conf_global_vars_s& vars = _conf_globals;

    if (!_conf_initialized)
        conf_init();

    // PKCS #11
    vars.Vsecurity_active = server.pkcs11.enabled;

    // Get the first listen socket
    const ServerXMLSchema::HttpListener *httpListener = NULL;
    if (server.getHttpListenerCount())
        httpListener = server.getHttpListener(0);

    // Get the first virtual server
    const ServerXMLSchema::VirtualServer *virtualServer = NULL;
    if (server.getVirtualServerCount())
        virtualServer = server.getVirtualServer(0);

    // Initialize LS-specific variables
    if (httpListener) {
        vars.Vport = httpListener->port;
        vars.Vaddr = PERM_STRDUP(httpListener->ip);
        vars.Vserver_hostname = PERM_STRDUP(httpListener->serverName);
    } else {
        vars.Vport = 80;
        vars.Vaddr = PERM_STRDUP("0.0.0.0");
        vars.Vserver_hostname = util_hostname();
        if (!vars.Vserver_hostname)
            vars.Vserver_hostname = PERM_STRDUP("localhost");
    }

    // Initialize SSL-specific variables
    if (httpListener && httpListener->ssl.enabled) {
        vars.Vssl3_active = httpListener->ssl.ssl2;
        vars.Vssl2_active = httpListener->ssl.ssl3;
        vars.Vsecure_auth = (httpListener->ssl.clientAuth != httpListener->ssl.clientAuth.CLIENTAUTH_FALSE);
    }

    // Initialize VS-specific variables
    if (virtualServer) {
        vars.Vroot_object = PERM_STRDUP(virtualServer->defaultObjectName);
        vars.accept_language = virtualServer->localization.negotiateClientLanguage;
    } else {
        vars.Vroot_object = PERM_STRDUP("default");
    }
}

/* ----------------------- conf_get_thread_globals ------------------------ */

/* Take a backup of thread local data.
 *
 */

NSAPI_PUBLIC void conf_get_thread_globals(HttpRequest *&hrq,
                                          const VirtualServer *&vs)
{
    ThreadGlobalsData *data = get_thread_globals_data();
    PR_ASSERT(data);
    if (data) {
        hrq = data->hrq; 
        vs = data->vs; 
    }

}

/* ----------------------- conf_set_thread_globals ------------------------ */

/*
 * This must be called once for every new request to make sure that globals
 * will be handled properly
 */

NSAPI_PUBLIC void conf_set_thread_globals(class HttpRequest *hrq)
{
    ThreadGlobalsData *data = get_thread_globals_data();
    PR_ASSERT(data);
    if (data) {
        data->hrq = hrq;
        data->vs = hrq->getVS();
        data->initialized = PR_FALSE;
    }
}


 /* ---------------------- conf_set_thread_globals ---------------------- */

/* This is called to cover the scenario's when hrq could be null. The backup 
 * thread local variables are set back if hrq is null.
 */

NSAPI_PUBLIC void conf_set_thread_globals(class HttpRequest *hrq, 
                                          const VirtualServer *vs)
{
    ThreadGlobalsData *data = get_thread_globals_data();
    PR_ASSERT(data);
    if (data) {
        data->hrq = hrq;
        data->vs = vs;
    }
    
}



/* ---------------------- conf_set_thread_vs_globals ---------------------- */

/*
 * This is a lighter weight version of conf_set_thread_globals used during
 * VSInitFunc and VSDestroyFunc processing
 */

NSAPI_PUBLIC void conf_set_thread_vs_globals(const VirtualServer *vs)
{
    ThreadGlobalsData *data = get_thread_globals_data();
    PR_ASSERT(data);
    if (data) {
        data->hrq = NULL;
        data->vs = vs;
        data->initialized = PR_FALSE;
    }
}


/* ---------------------- conf_reset_thread_globals ----------------------- */

NSAPI_PUBLIC void conf_reset_thread_globals()
{
    ThreadGlobalsData *data = get_thread_globals_data();
    PR_ASSERT(data);
    if (data) {
        data->hrq = NULL;
        data->vs = NULL;
        data->initialized = PR_FALSE;
    }
}


/* ----------------------------- conf_get_vs ------------------------------ */

NSAPI_PUBLIC const VirtualServer *conf_get_vs()
{
    // Return the VirtualServer * used by conf_getglobals()
    ThreadGlobalsData *data = get_thread_globals_data();
    if (data)
        return data->vs;
    return NULL;
}


/* ------------------------ conf_get_true_globals ------------------------- */

NSAPI_PUBLIC conf_global_vars_s *conf_get_true_globals() 
{
    if (!_conf_initialized)
        conf_init();

    return &_conf_globals;
}


/* --------------------------- conf_getglobals ---------------------------- */

NSAPI_PUBLIC conf_global_vars_s *INTconf_getglobals(void)
{
    if (!_conf_initialized)
        conf_init();

    // use the true globals until the server completes init and goes threaded
    if (!request_is_server_active())
        return &_conf_globals;

    ThreadGlobalsData *data = get_thread_globals_data();

    // fallback to the real globals if we couldn't allocate thread-specific
    // globals
    if (!data)
        return &_conf_globals;

    // we're done if the thread-specific globals were already initialized
    if (data->initialized)
        return &data->vars;

    // we need to do some magic to fill the right values into the structure
    // unfortunately, since this function returns a pointer to the entire
    // structure as opposed to a specific field value, we have to do the magic
    // for all the fields at once

    // first, copy all the true globals
    conf_global_vars_s& vars = data->vars;
    vars = _conf_globals; // structure copy

    // update fields that require the request
    HttpRequest *hrq = data->hrq;
    if (hrq) {
        const DaemonSession& ds = hrq->GetDaemonSession();

        vars.Vport = ds.GetServerPort();
        vars.Vaddr = (char*)ds.GetServerIP();

        // get Vserver_hostname
        // we usually get this from the host header in the request
        const NSAPIRequest *nrq = hrq->GetNSAPIRequest();
        vars.Vserver_hostname = nrq->rq.hostname;

        // we first need to check if SSL was enabled on the connection
        const NSAPISession *nsn = hrq->GetNSAPISession();
        vars.Vsecurity_active = GetSecurity(&nsn->sn);

        const SSLSocketConfiguration* sslc = ds.conn->sslconfig;
        if (sslc) {
            vars.Vssl3_active = sslc->ssl2;
            vars.Vssl2_active = sslc->ssl3;
            vars.Vsecure_auth = (sslc->clientAuth != sslc->clientAuth.CLIENTAUTH_FALSE);
        }
    }

    // update fields that require the VS
    const VirtualServer *vs = data->vs;
    if (vs) {
        // we get Vroot_object, Vstd_os and Vacl_root_30 from the VS object
        vars.Vroot_object = (char *)(const char *)vs->defaultObjectName;
        vars.Vstd_os = vs->getObjset();
        vars.Vacl_root_30 = vs->getACLList(); // The root of ACL data structures
        // vars.Vacl_root is about 2.0 ACLs, obsolete and stays NULL

        // also get acceptlanguage from the VS object
        vars.accept_language = vs->localization.negotiateClientLanguage;
    }

    data->initialized = PR_TRUE; // to prevent the magic from being done again for this request

    return &data->vars;
}


/* ------------------------------ parse_line ------------------------------ */

/*
 * Parses a directive and value and takes action depending on the value.
 * Returns PR_SUCCESS on success and PR_FAILURE upon error (after having logged
 * an error).
 */

static PRStatus parse_line(const char *cfn, char *l, int ln, pblock *& param,
                           PRBool jvm_enabled) 
{
    char *d = l;
    char *v = NULL;
    
    if((*d) == '#')
	return PR_SUCCESS;
    
    // Check for Init line continuations
    if (isspace(*d) && param && conf_str2pblock(d, param) > 0)
        return PR_SUCCESS;
    param = NULL;

    while((*d) && (isspace(*d))) ++d;
    if(!(*d))
	return PR_SUCCESS;
    
    for(v = d; (*v) && (*v != ' '); ++v);
    if(!(*v))
	goto noval;
    
    *v++ = '\0';
    while((*v) && isspace(*v)) ++v;
    
    if(!(*v)) {
noval:
        ereport(LOG_MISCONFIG,
                XP_GetAdminStr(DBT_confFileXLineYNeedValue),
                cfn, ln);
        return PR_FAILURE;
    }

    PRBool flagAddToGlobals = PR_TRUE;
    PRBool flagRecognized = PR_TRUE;
    
    if(!strcasecmp(d, "AdminUsers")) {
	if (_conf_globals.Vadmin_users)
	    FREE(_conf_globals.Vadmin_users);
	_conf_globals.Vadmin_users = STRDUP(v);
    }
    
#ifdef XP_UNIX
    else if(!strcasecmp(d,"Umask")) {
	/* Assume it to be an octal integer */
	mode_t mode = (mode_t)strtol(v, (char **)0, 8);
	file_mode_init(mode);
    }
#endif /* XP_UNIX */
    
    else if (!strcasecmp(d, "ServerName")) {
	if(_conf_globals.Vserver_hostname)
		FREE(_conf_globals.Vserver_hostname);
	_conf_globals.Vserver_hostname = STRDUP(v);
    }

    else if (!strcasecmp(d, "MtaHost")) {
	SetMtaHost(v);
    }
    
#if defined(XP_WIN32)
    else if(!strcasecmp(d, "GetFullPathName"))
    {
        if(!strcasecmp(v, "on"))
        {
            set_fullpathname(PR_TRUE);
        }
        else if(!strcasecmp(v, "off"))
        {
            set_fullpathname(PR_FALSE);
        }
        else
        {
            ereport(LOG_MISCONFIG,
                    XP_GetAdminStr(DBT_confFileXLineYInvalidZValue),
                    cfn, ln, d);
            return PR_FAILURE;
        }
    }
#endif
 
    else if(!strcasecmp(d, "Init")) {
	param = pblock_create(PARAMETER_HASH_SIZE);
	conf_str2pblock(v, param);
        if (!pblock_findkey(pb_key_fn, param)) {
            pblock_free(param);
            ereport(LOG_MISCONFIG,
                    XP_GetAdminStr(DBT_confFileXLineYInvalidZValue),
                    cfn, ln, d);
            return PR_FAILURE;
        }
        conf_add_init(param, &_conf_globals);
        flagAddToGlobals = PR_FALSE;
    }
    
    else if(!strcasecmp(d, "MaxProcs"))
    {
        int maxprocs = atoi(v);
#if defined(XP_WIN32) || !defined(FEAT_MULTIPROCESS)
        if (maxprocs > 1)
        {
            ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confUnaccessed), d);
            flagAddToGlobals = PR_FALSE;
            maxprocs = 1;
        }
#endif
        if (maxprocs < 1)
            maxprocs = 1;
        if (maxprocs != 1 && jvm_enabled) {
            ereport(LOG_MISCONFIG,
                    XP_GetAdminStr(DBT_confFileXLineYDirectiveZJavaDeprecated),
                    cfn, ln, d);
        }
        _conf_globals.Vpool_max = maxprocs;
    }
    else if (!strcasecmp(d, "LogAllVs"))
	ereport_set_logall(StringValue::getBoolean(v));
    else if(!strcasecmp(d, "Chroot")) {
	if(_conf_globals.Vchr)
		FREE(_conf_globals.Vchr);
	_conf_globals.Vchr = STRDUP(v);
    }
    
    /* UI language for administrator */
    else if(!strcasecmp(d, "AdminLanguage")) {
        XP_SetLanguageRanges(XP_LANGUAGE_SCOPE_PROCESS,
                             XP_LANGUAGE_AUDIENCE_ADMIN,
                             v);
    }
    /* UI language for client user */
    else if(!strcasecmp(d, "ClientLanguage")) {
        XP_SetLanguageRanges(XP_LANGUAGE_SCOPE_PROCESS,
                             XP_LANGUAGE_AUDIENCE_CLIENT,
                             v);
    }
    /* Digest Authentication nonce timeout */
    else if(!strcasecmp(d, "DigestStaleTimeout")) {
            _conf_globals.digest_stale_timeout = atoi(v);
    }
    else if (!strcasecmp(d, "ContentHeaders")) {
        error_set_content_headers(v);
    }

    else {
        // We didn't recognize this directive
        flagRecognized = PR_FALSE;
    }

    if (flagAddToGlobals) {
        // Add this directive to the global variables pblock
        conf_setGlobal(d, v);

        if (flagRecognized) {
            // Indicate this directive is used
            conf_findGlobal(d);
        }
    }
    
    return PR_SUCCESS;
}


/* ------------------------------ conf_parse ------------------------------ */

NSAPI_PUBLIC PRStatus conf_parse(const char *cfn, PRBool jvm_enabled)
{
	filebuf_t *buf;
	SYS_FILE fd;
	int ln = 0, t;
	PRStatus rv;
	char line[CONF_MAXLEN + 80];
	pblock *param = NULL;

        /* Record the path to the configuration file */
        if (_conf_magnus_filename) {
            FREE(_conf_magnus_filename);
        }
        _conf_magnus_filename = file_canonicalize_path(cfn);

        if((fd = system_fopenRO(cfn)) == SYS_ERROR_FD) {
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_confErrorOpeningFileXBecauseY),
                    cfn, system_errmsg());
            return PR_FAILURE;
        }
	
	buf = filebuf_open(fd, FILE_BUFFERSIZE);
	if (!buf) {
            ereport(LOG_FAILURE,
                    XP_GetAdminStr(DBT_confErrorReadingFileXBecauseY),
                    cfn, system_errmsg());
            return PR_FAILURE;
        }
	
	while(1) {
		++ln;
		switch((t = util_getline(buf, ln, CONF_MAXLEN, line))) {
		case -1:
			filebuf_close(buf);
			ereport(LOG_FAILURE,
				XP_GetAdminStr(DBT_confErrorReadingFileXBecauseY),
				cfn, system_errmsg());
			return PR_FAILURE;
			
		case 1:
			rv = parse_line(cfn, line, ln, param, jvm_enabled);
			filebuf_close(buf);
			return rv;
			
		default:
			if((rv = parse_line(cfn, line, ln, param, jvm_enabled))) {
				filebuf_close(buf);
				return rv;
			}
		}
	}
}


/* ---------------------------- conf_add_init ----------------------------- */

NSAPI_PUBLIC void conf_add_init(pblock *initfn, conf_global_vars_s *cg)
{
    register int x;

    if(!cg->initfns) {
        cg->initfns = (pblock **) MALLOC(2*sizeof(pblock *));
        x = 0;
    }
    else {
        for(x = 0; cg->initfns[x]; ++x); /* no action */
        cg->initfns = (pblock **) 
            REALLOC(cg->initfns, (x+2) * sizeof(pblock *));
    }
    cg->initfns[x] = initfn;
    cg->initfns[x+1] = NULL;
}


/* -------------------------- conf_is_late_init --------------------------- */

NSAPI_PUBLIC int conf_is_late_init(pblock *pb)
{
    char *lateInit = pblock_findval("LateInit", pb);

    if (lateInit && util_getboolean(lateInit, PR_FALSE))
        return 1;

    return 0;
}


/* -------------------------- run_init_functions -------------------------- */

static PRStatus run_init_functions(PRBool runEarlyInitFunctions)
{
    char line[CONF_MAXLEN];
    int x;
    pblock **initfns = _conf_globals.initfns;
    
    if(!initfns)
	return PR_SUCCESS;
    
    for(x = 0; initfns[x]; ++x) {
	char *fn = pblock_findkeyval(pb_key_fn, initfns[x]);
	PRBool isEarlyInitFunction = !conf_is_late_init(initfns[x]);
	
	if (isEarlyInitFunction == runEarlyInitFunctions) {
	    pblock_nvinsert("server-version", MAGNUS_VERSION_STRING, 
			initfns[x]);

	    // Remember the current FUNC_USE_NATIVE_THREAD setting
	    PRBool flag = func_get_default_flag(FUNC_USE_NATIVE_THREAD);

	    // If this is a NativeThread="yes" NSAPI module's Init function,
	    // any functions it creates will be FUNC_USE_NATIVE_THREAD, too
	    if (fn) {
		FuncStruct *f = func_find_str(fn);
		func_set_default_flag(FUNC_USE_NATIVE_THREAD,
			f && (f->flags & FUNC_USE_NATIVE_THREAD));
	    }

	    if(func_exec(initfns[x], NULL, NULL) == REQ_ABORTED) {
		const char *v = pblock_findval("error", initfns[x]);
		if (v) {
		    ereport(LOG_FAILURE,
			    XP_GetAdminStr(DBT_confErrorRunningInitXErrorY),
			    fn, v);
		} else {
		    ereport(LOG_FAILURE,
			    XP_GetAdminStr(DBT_confErrorRunningInitX),
			    fn, v);
		}
		return PR_FAILURE;
	    }

	    // Restore the previous FUNC_USE_NATIVE_THREAD setting
	    func_set_default_flag(FUNC_USE_NATIVE_THREAD, flag);
            
	    param_free(pblock_remove("server-version",initfns[x]));
	}
    }
    
    return PR_SUCCESS;
}


/* -------------------- conf_run_early_init_functions --------------------- */

NSAPI_PUBLIC PRStatus conf_run_early_init_functions() 
{
    return run_init_functions(PR_TRUE);
}


/* --------------------- conf_run_late_init_functions --------------------- */

NSAPI_PUBLIC PRStatus conf_run_late_init_functions(Configuration *configuration)
{
    if (configuration->getVSCount())
        conf_set_thread_vs_globals(configuration->getVS(0));

    PRStatus rv = run_init_functions(PR_FALSE);

    conf_reset_thread_globals();

    return rv;
}


/* --------------------------- conf_str2pblock ---------------------------- */

NSAPI_PUBLIC int conf_str2pblock(const char *str, pblock *pb)
{
    pblock *pb_new = pblock_create(3);

    int np = pblock_str2pblock(str, pb_new);

    /* "magnus-internal" is reserved for runtime binary data */
    while (pb_param *pp = pblock_remove("magnus-internal", pb_new)) {
        param_free(pp);
        np--;
    }

    /* "refcount" is reserved for pblock reference counts */
    while (pb_param *pp = pblock_remove("refcount", pb_new)) {
        param_free(pp);
        np--;
    }

    /* "Directive" is reserved for NSAPI directive names */
    while (pb_param *pp = pblock_remove("Directive", pb_new)) {
        param_free(pp);
        np--;
    }

    pblock_copy(pb_new, pb);

    pblock_free(pb_new);

    return np;
}


/* ----------------------------- conf_get_id ------------------------------ */

NSAPI_PUBLIC int conf_get_id(const Configuration *configuration)
{
    if (!configuration)
        return 0;

    return configuration->getID();
}


/* ------------------------- conf_getServerString ------------------------- */

NSAPI_PUBLIC const char *conf_getServerString(void)
{
    return MAGNUS_VERSION_STRING;
}


/* --------------------------- conf_getfilename --------------------------- */

NSAPI_PUBLIC const char* conf_getfilename(void)
{
    return _conf_magnus_filename ? _conf_magnus_filename : PRODUCT_MAGNUS_CONF; // "magnus.conf"
}


/* ---------------------------- conf_getstring ---------------------------- */

NSAPI_PUBLIC const char* conf_getstring(const char* name, const char* def)
{
    const char* value = conf_findGlobal((char*)name);

    if (value) {
        if (*value) {
            return value;
        }
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confExpectedString), name);
    }

    return def;
}


/* --------------------------- conf_getboolean ---------------------------- */

NSAPI_PUBLIC int conf_getboolean(const char* name, int def)
{
    const char* value = conf_findGlobal((char*)name);

    if (value) {
        if (StringValue::hasBoolean(value)) {
            return StringValue::getBoolean(value);
        }
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confExpectedBoolean), name);
    }

    return def;
}


/* --------------------------- conf_getinteger ---------------------------- */

NSAPI_PUBLIC int conf_getinteger(const char* name, int def)
{
    const char* value = conf_findGlobal((char*)name);

    if (value) {
        if (StringValue::isInteger(value)) {
            return StringValue::getInteger(value);
        }
        if (StringValue::hasBoolean(value)) {
            return StringValue::getBoolean(value);
        }
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confExpectedInteger), name);
    }

    return def;
}


/* ------------------------ conf_getboundedinteger ------------------------ */

NSAPI_PUBLIC int conf_getboundedinteger(const char* name, int min, int max, int def)
{
    PR_ASSERT(def >= min && def <= max);

    int value = conf_getinteger(name, def);
    if (value < min || value > max) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confExpectedBoundedInteger), name, min, max);
        value = def;
    }

    return value;
}
