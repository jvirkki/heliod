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
 *
 * Access primitives to server configuration paramerters.
 * Created by: Antoni Lapinski, Oct 1996
 *
 */

#include "frame/conf.h"
#include "frame/conf_api.h"
#include "frame/dbtframe.h"
#include "base/pblock.h"
#include "base/vs.h"
#include "base/util.h"
#include "frame/log.h"
#include "support/stringvalue.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/configuration.h"

static PRBool conf_api_initialized = PR_FALSE;

static pblock *globalsUnaccessed;
static pblock *globalsMultiplyDefined;

static char *_lowercase(char *name)
{
    char *d = STRDUP(name);
    if (d) {
        char* t = d;
        while (*t) {
            *t = tolower(*t);
            t++;
        }
    }
    return d;
}

NSAPI_PUBLIC PRBool conf_initGlobal()
{
    conf_global_vars_s *globals = conf_get_true_globals();

    globals->genericGlobals = pblock_create(131);
    globalsUnaccessed = pblock_create(131);
    globalsMultiplyDefined = pblock_create(5);

    if (!globals->genericGlobals || !globalsUnaccessed || !globalsMultiplyDefined)
        return PR_FALSE;

    conf_api_initialized = PR_TRUE;

    return PR_TRUE;
}

NSAPI_PUBLIC char *conf_findGlobal(char *name)
{
    if (!conf_api_initialized) {
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_confApiCallBeforeInit));
        return NULL;
    }

    char *rv = NULL;
    char *d = _lowercase(name);
    
    if (d) {
        // Mark directive as accessed
        if (globalsUnaccessed)
            param_free(pblock_remove(d, globalsUnaccessed));

        // Lookup directive
        rv = pblock_findval(d, conf_get_true_globals()->genericGlobals);

        FREE(d);
    }

    return rv;
}

NSAPI_PUBLIC PRBool conf_setGlobal(char *name, char *value)
{
    pblock *globals = conf_get_true_globals()->genericGlobals;

    char *d = _lowercase(name);

    if (pblock_find(d, globals)) {
        // Mark directive as multiply defined
        pblock_nvinsert(d, name, globalsMultiplyDefined);
        param_free(pblock_remove(d, globals));
    } else {
        // Mark directive as unaccessed
        pblock_nvinsert(d, name, globalsUnaccessed);
    }

    PRBool rv = (pblock_nvinsert(d, value, globals) != NULL);

    FREE(d);

    return rv;
}

NSAPI_PUBLIC PRBool conf_deleteGlobal(char *name)
{
    pb_param *oldParam;
    pblock *globals = conf_get_true_globals()->genericGlobals;

    char *d = _lowercase(name);

    oldParam = pblock_remove(d, globals);

    FREE(d);

    return oldParam==NULL?PR_FALSE:PR_TRUE;
}

/* Hostname of the Mail Transport Agent :
 * typically "localhost" for the Unix boxes,
 * or the hostname of a POP server otherwise.
 * Needed by the Agents subsystem.
 * Current implementation relies on access to the global config. structure.
 */

NSAPI_PUBLIC const char *GetMtaHost(){
    return (conf_get_true_globals()->mtahost ? conf_get_true_globals()->mtahost : "");
}

NSAPI_PUBLIC const char *SetMtaHost(const char *HostName){

    if(conf_get_true_globals() -> mtahost)
	FREE(conf_get_true_globals() -> mtahost);

    conf_get_true_globals() -> mtahost = STRDUP(HostName);

    return conf_get_true_globals() -> mtahost;
}

NSAPI_PUBLIC const char *GetNntpHost(){
    return (conf_get_true_globals()->nntphost ? conf_get_true_globals()->nntphost : "");
}

NSAPI_PUBLIC const char *SetNntpHost(const char *HostName){

    if(conf_get_true_globals() -> nntphost)
        FREE(conf_get_true_globals() -> nntphost);

    conf_get_true_globals() -> nntphost = STRDUP(HostName);

    return conf_get_true_globals() -> nntphost;
}

NSAPI_PUBLIC void conf_warnunaccessed(void)
{
    // Generate a warning for every unused (misspelled, deprecated) directive
    struct pb_entry *p;
    int i;
    for (i = 0; i < globalsUnaccessed->hsize; i++) {
        p = globalsUnaccessed->ht[i];
        while (p) {
            ereport(LOG_WARN, XP_GetAdminStr(DBT_confUnaccessed), p->param->value);
            p = p->next;
        }
    }

    pblock_free(globalsUnaccessed);
    globalsUnaccessed = NULL;
}

NSAPI_PUBLIC void conf_warnduplicate(void)
{
    // Generate a warning for every multiply defined directive
    struct pb_entry *p;
    int i;
    for (i = 0; i < globalsMultiplyDefined->hsize; i++) {
        p = globalsMultiplyDefined->ht[i];
        while (p) {
            ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_confMultiplyDefined), p->param->value);
            p = p->next;
        }
    }

    pblock_free(globalsMultiplyDefined);
    globalsMultiplyDefined = NULL;
}
