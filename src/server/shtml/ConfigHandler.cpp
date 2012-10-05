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

#include "ShtmlPage.h"
#include "ConfigHandler.h"

ConfigHandler::ConfigHandler(pblock* pb) 
: _timeFmt(0), _sizeFmt(UNKNOWN), _errMsg(0), _needToSetSizeFmt(PR_FALSE),
  _needToSetVarScope(PR_FALSE), _localVarScope(PR_FALSE)
{
  char* t = 0;
  if( (t = pblock_findval("timefmt", pb)) ) {
    _timeFmt = PERM_STRDUP(t);
  }                
  if (t = pblock_findval("sizefmt", pb)) {
    _needToSetSizeFmt = PR_TRUE;
    if (strcmp(t, "abbrev") == 0)
      _sizeFmt = SIZE_KMG;
    else if (strcmp(t, "bytes") == 0)
      _sizeFmt = SIZE_BYTES;
    else {
      _needToSetSizeFmt = PR_FALSE;
      _sizeFmt = UNKNOWN;
    }
  }
  if ((t = pblock_findval("errmsg", pb))) {
    _errMsg = PERM_STRDUP(t);
  }
  if ((t = pblock_findval("content-type", pb))) {
    ShtmlPage* page = ShtmlPage::GetCurrentPage();
    page->SetContentType(t);
  }
  if ((t = pblock_findval("scope", pb))) {
    _needToSetVarScope = PR_TRUE;
    _localVarScope = (strcasecmp(t, "global") != 0);
  }
}



ConfigHandler::~ConfigHandler()
{
  if (_errMsg)
    PERM_FREE(_errMsg);
  if (_timeFmt)
    PERM_FREE(_timeFmt);
}

PRBool 
ConfigHandler::Execute(pblock* pb, Session* sn, Request* rq, 
                       PageStateHandler& pg)
{
  PRBool res = PR_TRUE;
  if (_timeFmt)
    pg.SetTimeFormat(_timeFmt);
  if (_errMsg)
    pg.SetErrorMsg(_errMsg);
  if (_needToSetSizeFmt && (_sizeFmt != UNKNOWN))
    pg.SetSizeFormat(_sizeFmt);
  if (_needToSetVarScope)
    pg.SetLocalVarScope(_localVarScope);

  return res;
}
