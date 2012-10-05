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

// NSAPI functions related to virtual server support.

#include <string.h>

#include "netsite.h"
#include "NsprWrap/NsprSink.h"
#include "NsprWrap/NsprError.h"
#include "base/vs.h"
#include "base/ereport.h"
#include "base/session.h"
#include "base/systhr.h"
#include "base/dbtbase.h"
#include "frame/nsapi_accessors.h"
#include "frame/req.h"
#include "frame/httpact.h"
#include "frame/http_ext.h"
#include "frame/httpdir.h"
#include "frame/model.h"
#include "frame/conf.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/vsmanager.h"
#include "httpdaemon/vsconf.h"

//-----------------------------------------------------------------------------
// Private global variable definitions
//-----------------------------------------------------------------------------

static const int MAX_RESOLVE_DEPTH = 100;

//-----------------------------------------------------------------------------
// Prototypes for private functions
//-----------------------------------------------------------------------------

static PRStatus string_substitute(const VirtualServer* vs, const char* s, NSString& buffer, int depth, const char** ancestors);

//-----------------------------------------------------------------------------
// NSAPIVSListener (VSListener interface)
//-----------------------------------------------------------------------------

class NSAPIVSListener : public VSListener {
public:
    NSAPIVSListener(VSInitFunc* vs_init_func, VSDestroyFunc* vs_destroy_func)
    : fnInit(vs_init_func), fnDestroy(vs_destroy_func)
    {
    }

    PRStatus initVS(VirtualServer* incoming, const VirtualServer* current)
    {
        PRStatus rv = PR_SUCCESS;

        if (fnInit) {
            if ((*fnInit)(incoming, current) == REQ_PROCEED) {
                if (incoming) {
                    if (objset_interpolative(vs_get_httpd_objset(incoming)) != PR_SUCCESS)
                        ereport(LOG_VERBOSE, "Unable to interpolate objset for virtual server %s (%s)", vs_get_id(incoming), system_errmsg());
                }
            } else {
                rv = PR_FAILURE;
            }
        }

        return rv;
    }

    void destroyVS(VirtualServer* outgoing)
    {
        if (fnDestroy)
            (*fnDestroy)(outgoing);
    }

private:
    VSInitFunc* fnInit;
    VSDestroyFunc* fnDestroy;
};

//-----------------------------------------------------------------------------
// NSAPIDirectiveVSListener (VSListener interface)
//-----------------------------------------------------------------------------

class NSAPIDirectiveVSListener : public VSListener {
public:
    NSAPIDirectiveVSListener(FuncPtr func, VSDirectiveInitFunc* vs_init_func, VSDirectiveDestroyFunc* vs_destroy_func)
    : fn(func), fnInit(vs_init_func), fnDestroy(vs_destroy_func)
    {
    }

    PRStatus initVS(VirtualServer* incoming, const VirtualServer* current)
    {
        if (!fnInit) return PR_SUCCESS;

        // Call (*fnInit) for every matching directive in this VS
        PRStatus rv = PR_SUCCESS;
        httpd_objset* objset = vs_get_httpd_objset(incoming);
        int countObjects = objset_get_number_objects(objset);
        int iObject;
        for (iObject = 0; iObject < countObjects; iObject++) {
            const httpd_object* object = objset_get_object(objset, iObject);
            int countDirectiveTables = object_get_num_directives(object);
            int iDirectiveTable;
            for (iDirectiveTable = 0; iDirectiveTable < countDirectiveTables; iDirectiveTable++) {
                const dtable* dir_table = object_get_directive_table(object, iDirectiveTable);
                int countDirectives = directive_table_get_num_directives(dir_table);
                int iDirective;
                for (iDirective = 0; iDirective < countDirectives; iDirective++) {
                    const directive* dir = directive_table_get_directive(dir_table, iDirective);
                    const FuncStruct* fs = directive_get_funcstruct(dir);
                    if (fs && fs->func == fn) { 
                        if ((*fnInit)(dir, incoming, current) != REQ_PROCEED)
                            rv = PR_FAILURE;
                    }
                }
            }
        }

        if (rv == PR_SUCCESS) {
            if (objset_interpolative(objset) != PR_SUCCESS)
                ereport(LOG_VERBOSE, "Unable to interpolate objset for virtual server %s (%s)", vs_get_id(incoming), system_errmsg());
        }

        return rv;
    }

    void destroyVS(VirtualServer* outgoing)
    {
        if (!fnDestroy) return;

        // Call (*fnDestroy) for every matching directive in this VS
        httpd_objset* objset = vs_get_httpd_objset(outgoing);
        int countObjects = objset_get_number_objects(objset);
        int iObject;
        for (iObject = 0; iObject < countObjects; iObject++) {
            const httpd_object* object = objset_get_object(objset, iObject);
            int countDirectiveTables = object_get_num_directives(object);
            int iDirectiveTable;
            for (iDirectiveTable = 0; iDirectiveTable < countDirectiveTables; iDirectiveTable++) {
                const dtable* dir_table = object_get_directive_table(object, iDirectiveTable);
                int countDirectives = directive_table_get_num_directives(dir_table);
                int iDirective;
                for (iDirective = 0; iDirective < countDirectives; iDirective++) {
                    const directive* dir = directive_table_get_directive(dir_table, iDirective);
                    const FuncStruct* fs = directive_get_funcstruct(dir);
                    if (fs && fs->func == fn) { 
                        (*fnDestroy)(dir, outgoing);
                    }
                }
            }
        }
    }

private:
    FuncPtr fn;
    VSDirectiveInitFunc* fnInit;
    VSDirectiveDestroyFunc* fnDestroy;
};

//-----------------------------------------------------------------------------
// vs_alloc_slot
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int vs_alloc_slot(void)
{
    return VSManager::allocSlot();
}

//-----------------------------------------------------------------------------
// vs_set_data
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void* vs_set_data(const VirtualServer* vs, int* slot, void* data)
{
    if (*slot == -1) *slot = vs_alloc_slot();
    return VSManager::setData((VirtualServer*)vs, *slot, data);
}

//-----------------------------------------------------------------------------
// vs_get_data
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void* vs_get_data(const VirtualServer* vs, int slot)
{
    if (slot == -1) return NULL;
    return VSManager::getData((VirtualServer*)vs, slot);
}

//-----------------------------------------------------------------------------
// vs_register_cb
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int vs_register_cb(VSInitFunc* vs_init_func, VSDestroyFunc* vs_destroy_func)
{
    if (!vs_init_func && !vs_destroy_func) return REQ_ABORTED;

    // XXX The NSAPIVSListener* is never delete'd
    VSManager::addListener(new NSAPIVSListener(vs_init_func, vs_destroy_func));
    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// vs_directive_register_cb
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int vs_directive_register_cb(FuncPtr func, VSDirectiveInitFunc* vs_init_func, VSDirectiveDestroyFunc* vs_destroy_func)
{
    if (!vs_init_func && !vs_destroy_func) return REQ_ABORTED;

    // XXX The NSAPIDirectiveVSListener* is never delete'd
    VSManager::addListener(new NSAPIDirectiveVSListener(func, vs_init_func, vs_destroy_func));
    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// vs_get_id
//-----------------------------------------------------------------------------

NSAPI_PUBLIC const char* vs_get_id(const VirtualServer* vs)
{
    if (!vs) return NULL;
    return vs->name;
}

//-----------------------------------------------------------------------------
// vs_get_acllist
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void * vs_get_acllist(const VirtualServer* vs)
{
    if (!vs) return NULL;
    return (void *)(vs->getACLList());
}

//-----------------------------------------------------------------------------
// vs_set_acllist
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void vs_set_acllist(VirtualServer* vs, void *acllist)
{
    if (!vs) return;
    vs->setACLList((ACLListHandle_t *)acllist);
}

//-----------------------------------------------------------------------------
// vs_get_httpd_objset
//-----------------------------------------------------------------------------

NSAPI_PUBLIC httpd_objset* vs_get_httpd_objset(VirtualServer* vs)
{
    if (!VSManager::isInInitVS() && !VSManager::isInDestroyVS()) {
        // The returned httpd_objset is active, so it would be an error for the
        // caller to modify it
        ereport(LOG_WARN, XP_GetAdminStr(DBT_GetHttpdObjsetCalledOutsideVS));
    }

    if (!vs) return NULL;
    return vs->getObjset();
}

//-----------------------------------------------------------------------------
// vs_get_default_httpd_object
//-----------------------------------------------------------------------------

NSAPI_PUBLIC httpd_object* vs_get_default_httpd_object(VirtualServer* vs)
{
    if (!VSManager::isInInitVS() && !VSManager::isInDestroyVS()) {
        // The returned httpd_object is active, so it would be an error for the
        // caller to modify it
        ereport(LOG_WARN, XP_GetAdminStr(DBT_GetDefaultHttpdObjsetCalledOutsideVS));
    }

    if (!vs) return NULL;

    httpd_objset* objset = vs->getObjset();
    if (!objset) return NULL;

    return objset_findbyname(vs->defaultObjectName, 0, objset);
}

//-----------------------------------------------------------------------------
// NSAPIEnvironment
//-----------------------------------------------------------------------------

class NSAPIEnvironment {
public:
    inline NSAPIEnvironment(const VirtualServer* vs, const char* uri);
    inline ~NSAPIEnvironment();
    inline PRBool isValid() { return sn && rq && objset; }

    HttpRequest* hrq;
    const VirtualServer* threadVS;
    HttpRequest* threadHrq;
    pool_handle_t* poolCaller;
    Session* sn;
    Request* rq;
    httpd_objset* objset;
   
private:
    static int keyPool;
    sockaddr_in address;
};

int NSAPIEnvironment::keyPool = -1;

//-----------------------------------------------------------------------------
// NSAPIEnvironment::NSAPIEnvironment
//-----------------------------------------------------------------------------

inline NSAPIEnvironment::NSAPIEnvironment(const VirtualServer* vs, const char* uri)
{
    // Break the thread's association with any HttpRequest.  The existing
    // HttpRequest may refer to a different VS and will be using a different
    // Session (and therefore MALLOC pool).
    hrq = HttpRequest::CurrentRequest();
    if (hrq)
        HttpRequest::SetCurrentRequest(NULL);
    else
        conf_get_thread_globals(threadHrq, threadVS);

    // Remember the caller's MALLOC pool
    if (keyPool == -1)
        keyPool = getThreadMallocKey();
    poolCaller = (pool_handle_t*)systhread_getdata(keyPool);

    // Ensure session is allocated from the PERM_* pool so we can free it
    // correctly later
    if (poolCaller)
        systhread_setdata(keyPool, NULL);

    // Create a dummy Session.  session_create() will create a pool for us.
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    sn = session_create(PR_NewSink(), &address);
        
    // Create a dummy Request
    rq = request_restart_internal((char*)uri, NULL);

    // Get the VS's objset
    objset = vs ? vs->getObjset() : NULL;

    // Setup the thread-specific "globals" for this vs
    conf_set_thread_vs_globals(vs);
}

//-----------------------------------------------------------------------------
// NSAPIEnvironment::~NSAPIEnvironment
//-----------------------------------------------------------------------------

inline NSAPIEnvironment::~NSAPIEnvironment()
{
    // Tidy up the thread-specific "globals" for the next caller
    conf_reset_thread_globals();

    // Discard the request
    if (rq)
        request_free(rq);

    // Discard the session
    if (sn) {
        if (sn->csd && sn->csd_open)
            PR_Close(sn->csd);
        sn->csd_open = 0;
        session_free(sn);
    }

    // Restore the caller's pool.  session_free() set it to NULL
    if (poolCaller)
        systhread_setdata(keyPool, poolCaller);

    // This may have been a request thread.  If it was, we need to restore the
    // HttpRequest* for the thread-specific "globals".
    if (hrq) {
        HttpRequest::SetCurrentRequest(hrq);
        conf_set_thread_globals(hrq);
    }
    else {
        conf_set_thread_globals(threadHrq, threadVS);
    }
}

//-----------------------------------------------------------------------------
// find_document_root_directive
//-----------------------------------------------------------------------------

static char* find_document_root_directive(httpd_objset *objset)
{
    // XXX Attempt to guess the document root by looking for the NameTrans
    // fn=document-root directive in obj.conf.  Note that we use the last
    // document-root directive's root parameter; earlier directives may have
    // been in less-specific objects or enclosed in <Client> tags.

    int countObjects = objset_get_number_objects(objset);
    int iObject;
    for (iObject = countObjects - 1; iObject >= 0; iObject--) {
        const httpd_object* object = objset_get_object(objset, iObject);
        const dtable* dir_table = object_get_directive_table(object, NSAPINameTrans);
        int countDirectives = directive_table_get_num_directives(dir_table);
        int iDirective;
        for (iDirective = countDirectives - 1; iDirective >= 0; iDirective--) {
            const directive* dir = directive_table_get_directive(dir_table, iDirective);
            const FuncStruct* fs = directive_get_funcstruct(dir);
            if (fs && fs->name && !strcmp(fs->name, "document-root")) { 
                char* docroot = pblock_findval("root", dir->param.pb);
                if (docroot)
                    return docroot;
            }
        }
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// vs_get_doc_root
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char* vs_get_doc_root(const VirtualServer* vs)
{
    PR_ASSERT(vs);
    if (!vs)
        return NULL;

    // Establish an environment suitable for invoking SAFs
    NSAPIEnvironment nsapi(vs, "/");
    if (!nsapi.isValid())
        return NULL;

    // Call the NameTrans SAFs
    char* docroot = NULL;
    if (servact_objset_uri2path(nsapi.sn, nsapi.rq, nsapi.objset) == REQ_PROCEED) {
        docroot = pblock_findkeyval(pb_key_ntrans_base, nsapi.rq->vars);
        if (!docroot) {
            // Nobody set ntrans-base; try stripping the filename off the path
            // variable instead
            char* path = pblock_findkeyval(pb_key_path, nsapi.rq->vars);
            if (path && file_is_path_abs(path)) {
                int len = strlen(path);
                while (len > 1 && path[len - 1] != '/')
                    len--;
                docroot = (char*)MALLOC(len + 1);
                if (docroot) {
                    memcpy(docroot, path, len);
                    docroot[len] = '\0';
                }
            }
        }
    }

    // XXX If NameTrans failed to return a document root (e.g. there's a
    // NameTrans fn=redirect url-prefix=/ directive), look for a NameTrans
    // fn=document-root directive in obj.conf
    if (!docroot) {
        httpd_objset *objset = nsapi.rq->os;
        if (!objset)
            objset = nsapi.objset;
        docroot = find_document_root_directive(objset);
        if (docroot) {
            char* t = vs_substitute_vars(vs, docroot);
            if (t)
                docroot = t;
        }
    }

    // Make a copy of the document root in the caller's MALLOC pool, stripping
    // any trailing slash
    if (docroot) {
        docroot = pool_strdup(nsapi.poolCaller, docroot);
        int len = strlen(docroot);
        if (len > 1 && docroot[len - 1] == '/')
            docroot[len - 1] = '\0';

        return docroot;
    }

    // We couldn't find a document root using NSAPI, so use the <document-root>
    // value from server.xml.  We treat this as a last resort because it
    // defaults to ../docs.
    return pool_strdup(nsapi.poolCaller, vs->getNormalizedDocumentRoot().data());
}

//-----------------------------------------------------------------------------
// vs_translate_uri
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char* vs_translate_uri(const VirtualServer* vs, const char* uri)
{
    PR_ASSERT(vs);
    if (!vs)
        return NULL;

    // Establish an environment suitable for invoking SAFs
    NSAPIEnvironment nsapi(vs, uri);
    if (!nsapi.isValid())
        return NULL;

    // Call the NameTrans SAFs
    char* path = NULL;
    if (servact_objset_uri2path(nsapi.sn, nsapi.rq, nsapi.objset) == REQ_PROCEED) {
        path = pblock_findkeyval(pb_key_path, nsapi.rq->vars);
        if (path)
            path = pool_strdup(nsapi.poolCaller, path);
    }

    return path;
}

//-----------------------------------------------------------------------------
// vs_get_mime_type
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char* vs_get_mime_type(const VirtualServer* vs, const char* uri)
{
    PR_ASSERT(vs);
    if (!vs)
        return NULL;

    // Establish an environment suitable for invoking SAFs
    NSAPIEnvironment nsapi(vs, uri);
    if (!nsapi.isValid())
        return NULL;

    // Call the NameTrans SAFs
    char* type = NULL;
    if (servact_objset_uri2path(nsapi.sn, nsapi.rq, nsapi.objset) == REQ_PROCEED) {
        // Call the ObjectType SAFs
        if (servact_fileinfo(nsapi.sn, nsapi.rq) == REQ_PROCEED) {
            type = pblock_findkeyval(pb_key_content_type, nsapi.rq->srvhdrs);
            if (type)
                type = pool_strdup(nsapi.poolCaller, type);
        }
    }

    return type;
}

//-----------------------------------------------------------------------------
// vs_find_ext_type
//-----------------------------------------------------------------------------

NSAPI_PUBLIC const char* vs_find_ext_type(const VirtualServer* vs, const char* ext)
{
    if (!vs)
        return NULL;

    const cinfo* cinfo = vs->getMime().findExt(ext);
    if (!cinfo)
        return NULL;

    return cinfo->type;
}

//-----------------------------------------------------------------------------
// vs_is_default_vs
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int vs_is_default_vs(const VirtualServer* vs)
{
    PRBool isDefault = PR_FALSE;

    Configuration* configuration = ConfigurationManager::getConfiguration();
    if (!configuration) return 0;

    if (configuration->getVSCount()) {
        if (!strcmp(vs->name, configuration->getVS(0)->name))
            isDefault = PR_TRUE;
    }

    configuration->unref();

    return isDefault;
}

//-----------------------------------------------------------------------------
// resolve_config_var
//-----------------------------------------------------------------------------

static PRStatus resolve_config_var(const VirtualServer* vs, const char* name, NSString& buffer, int depth, const char** ancestors)
{
    for (int i = 0; i < depth; i++) {
        if (!strcmp(ancestors[i], name)) {
            NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_varLoopFromXToY), ancestors[depth - 1], name);
            return PR_FAILURE;
        }
    }

    const char* value = vs_lookup_config_var(vs, name);
    if (!value) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_undefinedVarX), name);
        return PR_FAILURE;
    }

    if (depth >= MAX_RESOLVE_DEPTH) {
        NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_maxVarDepthX), MAX_RESOLVE_DEPTH);
        return PR_FAILURE;
    }

    ancestors[depth] = name;

    depth++;

    return string_substitute(vs, value, buffer, depth, ancestors);
}

//-----------------------------------------------------------------------------
// string_substitute
//-----------------------------------------------------------------------------

static PRStatus string_substitute(const VirtualServer* vs, const char* s, NSString& buffer, int depth, const char** ancestors)
{
    while (*s) {
        int len = model_fragment_scan(s);
        if (len == -1)
            return PR_FAILURE;

        PR_ASSERT(len > 0);

        const char* fp;
        int fl;
        if (model_fragment_is_var_ref(s, &fp, &fl)) {
            NSString name(fp, fl);
            if (resolve_config_var(vs, name, buffer, depth, ancestors) == PR_FAILURE)
                return PR_FAILURE;
        } else if (model_fragment_is_invariant(s, &fp, &fl)) {
            buffer.append(fp, fl);
        } else {
            NsprError::setErrorf(PR_INVALID_ARGUMENT_ERROR, XP_GetAdminStr(DBT_badFragmentLenXStrY), len, s);
            return PR_FAILURE;
        }

        s += len;
    }

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// vs_lookup_config_var
//-----------------------------------------------------------------------------

NSAPI_PUBLIC const char* vs_lookup_config_var(const VirtualServer* vs, const char* name)
{
    return vs->getVars().findValue(name);
}

//-----------------------------------------------------------------------------
// vs_resolve_config_var
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char* vs_resolve_config_var(const VirtualServer* vs, const char* name)
{
    if (!vs || !name)
        return NULL;

    NSString buffer;
    const char* ancestors[MAX_RESOLVE_DEPTH];
    if (resolve_config_var(vs, name, buffer, 0, ancestors) == PR_FAILURE)
        return NULL;

    return STRDUP(buffer);
}

//-----------------------------------------------------------------------------
// vs_substitute_vars
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char* vs_substitute_vars(const VirtualServer* vs, const char* string)
{
    if (!vs || !string)
        return NULL;

    NSString buffer;
    const char* ancestors[MAX_RESOLVE_DEPTH];
    if (string_substitute(vs, string, buffer, 0, ancestors) == PR_FAILURE)
        return NULL;

    return STRDUP(buffer);
}

//-----------------------------------------------------------------------------
// vs_get_conf
//-----------------------------------------------------------------------------

NSAPI_PUBLIC const Configuration* vs_get_conf(const VirtualServer* vs)
{
    if (!vs) return NULL;
    return vs->getConfiguration();
}
