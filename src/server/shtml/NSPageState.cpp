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

#include "base/pblock.h"
#include "base/date.h"
#include "base/pool.h"

#include "NSPageState.h"

#define DEFAULT_LAST_MODIFIED_FMT "%A, %d-%b-%y %T"

#ifndef SHTML_PARAMSIZE
#define SHTML_PARAMSIZE 25
#endif


PRBool PageStateHandler::_addCgiInitVars = PR_FALSE;


void
PageStateHandler::InitRqVarScope()
{
    PR_ASSERT(_rqVarScope == NULL);

    _rqVarScope = _rq;

    if (!_localVarScope) {
        // Find the top-most Request* that actually mapped to a local file,
        // which will typically be the Request* for the parent .shtml file
        Request* rqParent = _rq;
        while (rqParent->orig_rq != rqParent) {
            rqParent = rqParent->orig_rq;
            NSFCFileInfo *finfo;
            if (request_info_path(NULL, rqParent, &finfo) == PR_SUCCESS) {
                if (finfo->pr.type == PR_FILE_FILE) 
                    _rqVarScope = rqParent;
            }
        }
    }
}


void
PageStateHandler::InitPaths()
{
    PR_ASSERT(_vpath == NULL && _vpathLen == 0);
    PR_ASSERT(_name == NULL && _nameLen == 0);
    PR_ASSERT(_pathInfo == NULL && _pathInfoLen == 0);

    if (!_rqVarScope)
        InitRqVarScope();

    const char* uri = pblock_findkeyval(pb_key_uri, _rqVarScope->reqpb);
    const char* pathInfo = pblock_findkeyval(pb_key_path_info, _rqVarScope->vars);

    int vpathLen = strlen(uri);
    if (pathInfo) {
        int pathInfoLen = strlen(pathInfo);
        if (vpathLen > pathInfoLen)
            vpathLen -= pathInfoLen;

        _pathInfo = (char*)pool_malloc(_sn->pool, pathInfoLen + 1);
        if (_pathInfo) {
            memcpy(_pathInfo, pathInfo, pathInfoLen);
            _pathInfo[pathInfoLen] = '\0';
            _pathInfoLen = pathInfoLen;
        }
    }

    _vpath = (char*)pool_malloc(_sn->pool, vpathLen + 1);
    if (_vpath) {
        memcpy(_vpath, uri, vpathLen);
        _vpath[vpathLen] = '\0';
        _vpathLen = vpathLen;

        _name = strrchr(_vpath, '/');
        if (_name) {
            _name++;
        } else {
            _name = _vpath;
        }
        _nameLen = _vpathLen - (_name - _vpath);
    }
}


const char*
PageStateHandler::GetVirtualPath()
{
    if (!_vpath)
        InitPaths();

    return _vpath;
}


void
PageStateHandler::InitTimeVars()
{
    char buf[256];
    int len;

    if (_timeFmt) {
        struct tm ts;

        date_current_localtime(&ts);
        len = util_strlftime(buf, sizeof(buf), _timeFmt, &ts);
        pblock_kvinsert(pb_key_DATE_LOCAL, buf, len, _vars);

        date_current_gmtime(&ts);
        len = util_strlftime(buf, sizeof(buf), _timeFmt, &ts);
        pblock_kvinsert(pb_key_DATE_GMT, buf, len, _vars);
    } else {
        len = date_current_formatted(date_format_shtml_gmt, buf, sizeof(buf));
        pblock_kvinsert(pb_key_DATE_GMT, buf, len, _vars);

        len = date_current_formatted(date_format_shtml_local, buf, sizeof(buf));
        pblock_kvinsert(pb_key_DATE_LOCAL, buf, len, _vars);
    }

    if (!_rqVarScope)
        InitRqVarScope();

    // XXX the formatted LAST_MODIFIED value should be cached in the ShtmlPage
    const char* lmTimeFmt = _timeFmt ? _timeFmt : DEFAULT_LAST_MODIFIED_FMT;
    struct stat* fi = request_stat_path(NULL, _rqVarScope);
    struct tm lmts;
    util_localtime(&fi->st_mtime, &lmts);
    len = util_strlftime(buf, sizeof(buf), lmTimeFmt, &lmts);
    pblock_kvinsert(pb_key_LAST_MODIFIED, buf, len, _vars);
}


void
PageStateHandler::ResetTimeVars()
{
    if (_vars) {
        pblock_removekey(pb_key_DATE_LOCAL, _vars);
        pblock_removekey(pb_key_DATE_GMT, _vars);
        pblock_removekey(pb_key_LAST_MODIFIED, _vars);

        InitTimeVars();
    }
}


void
PageStateHandler::SetLocalVarScope(PRBool flag)
{
    if (flag != _localVarScope) {
        // Reset everything that may depend on the value of _localVarScope
        _rqVarScope = NULL;
        _vpath = NULL;
        _vpathLen = 0;
        _name = NULL;
        _nameLen = 0;
        _pathInfo = NULL;
        _pathInfoLen = 0;
        _vars = NULL;
        _env = NULL;

        _localVarScope = flag;
    }
}


void
PageStateHandler::AddVars(const pblock* pb)
{
    if (!_vars)
        InitVars();

    pblock_copy(pb, _vars);
}


const pblock*
PageStateHandler::GetNonEnvVars()
{
    if (!_vars)
        InitVars();

    return _vars;
}


const char*
PageStateHandler::GetVar(const char* name)
{
    if (!_vars)
        InitVars();

    const char* res = NULL;

    if (_vars)
        res = pblock_findval(name, _vars);

    if (!res) {
        if (!_env)
            InitEnv();
        res = util_env_find(_env, name);
    }

    return res;
}


void
PageStateHandler::InitVars()
{
    PR_ASSERT(_vars == NULL);

    _vars = pblock_create(SHTML_PARAMSIZE);

    if (!_vpath)
        InitPaths();

    if (_vpath)
        pblock_kvinsert(pb_key_DOCUMENT_URI, _vpath, _vpathLen, _vars);

    if (_name)
        pblock_kvinsert(pb_key_DOCUMENT_NAME, _name, _nameLen, _vars);

    // Inherit query string from parent if we don't have our own
    const char* qs;
    for (Request* rq_qs = _rq; ; rq_qs = rq_qs->orig_rq) {
        qs = pblock_findkeyval(pb_key_query, rq_qs->reqpb);
        if (qs || _localVarScope || rq_qs->orig_rq == rq_qs)
            break;
    }
    if (qs) {
        pblock_kvinsert(pb_key_QUERY_STRING, qs, strlen(qs), _vars);
        if (char* t = pool_strdup(_sn->pool, qs)) {
            util_uri_unescape(t);
            pblock_kvinsert(pb_key_QUERY_STRING_UNESCAPED, t, strlen(t), _vars);
        }
    }

    // Inherit path-info from parent if we don't have our own
    const char* pi;
    for (Request* rq_pi = _rq; ; rq_pi = rq_pi->orig_rq) {
        pi = pblock_findkeyval(pb_key_path_info, rq_pi->vars);
        if (pi || _localVarScope || rq_pi->orig_rq == rq_pi)
            break;
    }
    if (pi)
        pblock_kvinsert(pb_key_PATH_INFO, pi, strlen(pi), _vars);

    InitTimeVars();
}


const char* const*
PageStateHandler::GetEnv()
{
    if (!_vars)
        InitVars();

    if (!_env)
        InitEnv();

    if (_varsAddedToEnv == PR_FALSE) {
        _env = pblock_pb2env(_vars, _env);
        _varsAddedToEnv = PR_TRUE;
    }

    return _env;
}


void
PageStateHandler::InitEnv()
{
    if (!_env) {
        if (!_rqVarScope)
            InitRqVarScope();

        _env = http_hdrs2env(_rqVarScope->headers);
        _env = cgi_common_vars(_sn, _rqVarScope, _env);
        if (_addCgiInitVars) {
            const pblock* cgiInitEnv = GetCgiInitEnv();
            if (cgiInitEnv)
                pblock_pb2env(cgiInitEnv, _env);
        }
    }
}


const char*
PageStateHandler::GetLastModifiedTimeFormat()
{
    return _timeFmt ? _timeFmt : DEFAULT_LAST_MODIFIED_FMT;
}


void
PageStateHandler::NeedToAddCgiInitEnv(PRBool flag)
{
    _addCgiInitVars = flag;
}
