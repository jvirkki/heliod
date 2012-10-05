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

#include "httpdaemon/HttpMethodRegistry.h"
#include "netsite.h"
#include "../base/keyword_pvt.h"

HttpMethodRegistry* HttpMethodRegistry::_gHttpRegistryPtr = NULL;

const HttpMethodRegistry*
HttpMethodRegistry::Init()
{
    if (_gHttpRegistryPtr == NULL) {
        _gHttpRegistryPtr = new HttpMethodRegistry();
        if (_gHttpRegistryPtr && (_gHttpRegistryPtr->IsValid() == PR_FALSE)) {
            delete _gHttpRegistryPtr;
            _gHttpRegistryPtr = NULL;
        }
    }
    return _gHttpRegistryPtr;
}

HttpMethodRegistry&
HttpMethodRegistry::GetRegistry()
{
    return *_gHttpRegistryPtr;
}

typedef struct method_entry method_entry;
struct method_entry {
    const char * method;
    int index;
};

/*
 * N.B. the set of methods in http_methods[] should be consistent with the set
 * of methods in the ACL subsystem's http_generic[].
 */
static const method_entry http_methods[] = {
    /* Methods defined in nsapi.h */
    { "HEAD", METHOD_HEAD+1 },
    { "GET", METHOD_GET+1 },
    { "PUT", METHOD_PUT+1 },
    { "POST", METHOD_POST+1 },
    { "DELETE", METHOD_DELETE+1 },
    { "TRACE", METHOD_TRACE+1 },
    { "OPTIONS", METHOD_OPTIONS+1 },
    { "MOVE", METHOD_MOVE+1 },
    { "INDEX", METHOD_INDEX+1 },
    { "MKDIR", METHOD_MKDIR+1 },
    { "RMDIR", METHOD_RMDIR+1 },
    { "COPY", METHOD_COPY+1 },
    /* Methods not defined in nsapi.h */
    { "CONNECT", METHOD_MAX+1 },
    { "PROPFIND", METHOD_MAX+2 },
    { "PROPPATCH", METHOD_MAX+3 },
    { "MKCOL", METHOD_MAX+4 },
    { "LOCK", METHOD_MAX+5 },
    { "UNLOCK", METHOD_MAX+6 },
    { "ACL", METHOD_MAX+7 },
    { "REPORT", METHOD_MAX+8 },
    { "VERSION-CONTROL", METHOD_MAX+9 },
    { "CHECKOUT", METHOD_MAX+10 },
    { "CHECKIN", METHOD_MAX+11 },
    { "UNCHECKOUT", METHOD_MAX+12 },
    { "MKWORKSPACE", METHOD_MAX+13 },
    { "UPDATE", METHOD_MAX+14 },
    { "LABEL", METHOD_MAX+15 },
    { "MERGE", METHOD_MAX+16 },
    { "BASELINE-CONTROL", METHOD_MAX+17 },
    { "MKACTIVITY", METHOD_MAX+18 },
    { "SEARCH", METHOD_MAX+19 },
    { "SUBSCRIBE", METHOD_MAX+20 },
    { "UNSUBSCRIBE", METHOD_MAX+21 },
    { "NOTIFY", METHOD_MAX+22 },
    { "POLL", METHOD_MAX+23 },
    { "BDELETE", METHOD_MAX+24 },
    { "BCOPY", METHOD_MAX+25 },
    { "BMOVE", METHOD_MAX+26 },
    { "BPROPPATCH", METHOD_MAX+27 },
    { "BPROPFIND", METHOD_MAX+28 }
};

HttpMethodRegistry::HttpMethodRegistry()
: _httpMethodsTable(NULL), _numRegisteredMethod(0), _valid(PR_TRUE)
{
    int sz = sizeof(http_methods)/sizeof(method_entry);
    _httpMethodsTable = NSKW_CreateNamespace(sz);
    if (!_httpMethodsTable) {
        _valid = PR_FALSE;
    }
    else {
        for (int i = 0; (i < sz) && (_valid == PR_TRUE); i++) {
            const method_entry& me = http_methods[i];
            int rv = NSKW_DefineKeyword(me.method, me.index, _httpMethodsTable);
            PR_ASSERT(rv == me.index);
            if (rv != me.index) {
                _valid = PR_FALSE;
                _numRegisteredMethod = 0;
            }
            else {
                if (_numRegisteredMethod != 0) {
                    _allMethodsStr.append(", ");
                }
                _allMethodsStr.append(me.method);
                _numRegisteredMethod++;
            }
        }
        NSKW_Optimize(_httpMethodsTable);
    }
}

HttpMethodRegistry::~HttpMethodRegistry()
{
    _valid = PR_FALSE;
    _numRegisteredMethod = 0;
    NSKW_DestroyNamespace(_httpMethodsTable);
    _httpMethodsTable = 0;
}

PRBool
HttpMethodRegistry::IsValid() const
{
    return _valid;
}

PRInt32
HttpMethodRegistry::GetNumMethods() const
{
  return _numRegisteredMethod;
}

const NSKWSpace*
HttpMethodRegistry::GetMethodsTable() const
{
    return _httpMethodsTable;
}


PRBool
HttpMethodRegistry::IsKnownHttpMethod(const char* method) const
{
    return (GetMethodIndex(method) != -1) ? PR_TRUE : PR_FALSE;
}


PRInt32
HttpMethodRegistry::RegisterMethod(const char* method)
{
    PR_ASSERT(method);

    PRInt32 index = GetMethodIndex(method);
    /* Register the method only it hasn't already been registered */
    if (index == -1) {
        /* VB: keyword.cpp does  not address memory maangement correctly.
           We need to dup and free there.
           However, not to start making global changes, i fix this here
           for the moment by an extra stringdup */
        index = NSKW_DefineKeyword(strdup(method), -1, _httpMethodsTable);
        if (index != -1) {
            index = index - 1;
            _allMethodsStr.append(", ");
            _allMethodsStr.append(method);
            _numRegisteredMethod++;
        }
        NSKW_Optimize(_httpMethodsTable);
    }

    return index;
}
    
const char*
HttpMethodRegistry::GetAllMethods()
{
   const char* res = (const char*)_allMethodsStr; 
   return res;
}

const char*
HttpMethodRegistry::GetMethod(PRInt32 i)
{
    PR_ASSERT(i >= 0);
    PR_ASSERT(i < _numRegisteredMethod);
    PR_ASSERT(i < _httpMethodsTable->nextkwi);

    NSKWEntry* entry = _httpMethodsTable->itable[i];
    PR_ASSERT(entry);
    const char* res = entry->keyword;
    PR_ASSERT(res);

    return res;
}


PRInt32
HttpMethodRegistry::GetMethodIndex(const char* method) const
{
    PR_ASSERT(method);
    unsigned long hashVal = NSKW_HashKeyword(method);
    PRInt32 index = NSKW_LookupKeyword(method, strlen(method), PR_TRUE, 
                                       hashVal, _httpMethodsTable);
    if (index != -1) {
        index = index - 1;
    }
    return index;
}

