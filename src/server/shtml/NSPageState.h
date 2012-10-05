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

#ifndef _NSPAGE_STATE_H_
#define _NSPAGE_STATE_H_

#include <time.h>     /* time */

#include <base/pblock.h>
#include <base/session.h>
#include <frame/req.h>
#include <base/util.h>
#include <frame/http.h>
#include <safs/cgi.h>

#include "ShtmlDefines.h"


#define DEFAULT_ERRMSG  "[an error occurred while processing this directive]"

class PageStateHandler {
  public :
    static void NeedToAddCgiInitEnv(PRBool flag);

    PageStateHandler(pblock* pb, Session* sn, Request* rq) 
    : _pb(pb), _sn(sn), _rq(rq), _localVarScope(PR_FALSE), _rqVarScope(NULL),
      _vpath(NULL), _vpathLen(0), _name(NULL), _nameLen(0),
      _pathInfo(NULL), _pathInfoLen(0), _env(NULL), _vars(NULL),
      _errMsg(NULL), _timeFmt(NULL), _sizeFmt(SIZE_KMG),
      _varsAddedToEnv(PR_FALSE)
    { }

    ~PageStateHandler() 
    {
      if (!_rq->senthdrs && !INTERNAL_REQUEST(_rq))
        protocol_start_response(_sn, _rq);
    }

    void SetLocalVarScope(PRBool flag);
    void AddVars(const pblock* pb);
    const pblock* GetNonEnvVars();
    const char* GetVar(const char* name);
    const char* const* GetEnv();
    const char* GetVirtualPath();

    size_t Write(const char* buff, size_t len)
    {
      size_t res = 0;
      if ((SendHeaders() == PR_TRUE) && (!ISMHEAD(_rq))) {
        res = net_write(_sn->csd, (char*)buff, len);
      }
      
      return res;
    }

    void WriteErrMsg()
    {
      const char* str = GetErrMsg(); 
      Write(str, strlen(str));
    }

    PRBool SendHeaders()
    {
      PRBool  error = PR_TRUE;
      if (!_rq->senthdrs) {
        if (protocol_start_response(_sn, _rq) != REQ_PROCEED)
          error = PR_FALSE;
      }
      return error;
    }

    const char* GetErrMsg() 
    {
      return (_errMsg ? _errMsg : DEFAULT_ERRMSG);
    }

    void SetErrorMsg(const char* errMsg) 
    {
      if (_errMsg) {
        FREE(_errMsg);
        _errMsg = 0;
      }
      if (errMsg)
        _errMsg = STRDUP(errMsg);
    }

    void SetTimeFormat(const char* timeFmt) 
    {
      if (_timeFmt) {
        FREE(_timeFmt);
        _timeFmt = 0;
      }
      if (timeFmt)
        _timeFmt = STRDUP(timeFmt);
      ResetTimeVars();
    }

    const char* GetLastModifiedTimeFormat();

    SizeFmt GetSizeFormat()
    {
      return _sizeFmt;
    }

    void SetSizeFormat(SizeFmt fmt)
    {
      _sizeFmt = fmt;
    }

  private:
    void InitRqVarScope();
    void InitPaths();
    void InitVars();
    void InitTimeVars();
    void ResetTimeVars();
    void InitEnv();

    pblock*  _pb;
    Session* _sn;
    Request* _rq;
    PRBool   _localVarScope;
    Request* _rqVarScope;
    char*    _vpath;
    int      _vpathLen;
    const char *_name;
    int      _nameLen;
    char*    _pathInfo;
    int      _pathInfoLen;
    char**   _env;
    pblock*  _vars;
    char*    _errMsg;
    char*    _timeFmt;
    SizeFmt  _sizeFmt;
    PRBool   _varsAddedToEnv;

    static PRBool _addCgiInitVars;
};

#endif /* _NSPAGE_STATE_H_ */
