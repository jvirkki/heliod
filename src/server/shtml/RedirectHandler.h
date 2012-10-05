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

#ifndef  _REDIRECT_HANDLER_H_ 
#define  _REDIRECT_HANDLER_H_

#include "NSBaseTagHandler.h"

class RedirectHandler : public NSBaseTagHandler {
  public : 
    RedirectHandler(pblock* pb);
    ~RedirectHandler();
    PRBool Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg);
    PRBool IsValid() { return _valid;};

  private :
    char*  _url;
    size_t _len;
    PRBool _valid;
};

RedirectHandler::RedirectHandler(pblock* pb)
: _valid(PR_TRUE), _url(0), _len(0) 
{
  if (pb) {
    char* url = pblock_findval("url", pb);
    if (url) {
      if (!pblock_findval("escape", pb)) {
        const char* msg = util_url_escape(NULL, url);
        _url = PERM_STRDUP(msg);
      }
      else {
        _url = PERM_STRDUP(url);
      }
      if (!_url) { 
        _valid = PR_FALSE;
      }
    }
    else {
      _valid = PR_FALSE;
    }
  }
  else {
    _valid = PR_FALSE;
  }
}

RedirectHandler::~RedirectHandler()
{
  if (_url)
    PERM_FREE(_url);
}

PRBool 
RedirectHandler::Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg)
{
  PRBool res = IsValid();
  if (res == PR_TRUE) {
    if (!rq->senthdrs) {
      pblock_nvinsert("location", _url, rq->srvhdrs);
      protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
    }
    else {
      res = PR_FALSE;
    }
  }

  return res;
}

#endif /* _REDIRECT_HANDLER_H_ */
    
