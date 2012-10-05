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

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#ifdef XP_UNIX
#include <strings.h>
#else
#include <string.h>
#endif

#include "nspr.h"
#include "base/pblock.h"
#include "EchoHandler.h"

EchoHandler::EchoHandler(pblock* pb)
: _var(0), _encoding(ENCODING_ENTITY)
{
  const char* t = pblock_findval("var", pb);
  if (t) {
    _var = PERM_STRDUP(t);
  }
  t = pblock_findval("encoding", pb);
  if (t) {
    if (!strcasecmp(t, "url")) {
      _encoding = ENCODING_URL;
    } else if (!strcasecmp(t, "none")) {
      _encoding = ENCODING_NONE;
    }
  } 
}

EchoHandler::~EchoHandler()
{
  if (_var)
    PERM_FREE(_var);
}

PRBool
EchoHandler::Execute(pblock* pb, Session* sn, Request* rq, 
                     PageStateHandler& pageState)
{
  PRBool res = PR_TRUE;
  if (_var) {
    // VB : This has to be in what was originally being passed as env
    // This consisted of the http_headers and cgi_common_vars 
    // This can be done by just doing pblock_findvals
    // We should not be forced to create the env for this
    // The env should be created only for cgi/exec requests

    const char* res = pageState.GetVar(_var);
    if (!res)
      res = "(none)";
    switch (_encoding) {
    case ENCODING_ENTITY:
        res = util_html_escape(res);
        break;
    case ENCODING_URL:
        res = util_url_escape(NULL, res);
        break;
    }
    size_t len = strlen(res);
    pageState.Write(res, len);
  }
  else {
    pageState.WriteErrMsg();
  }
  
  return res;
}
