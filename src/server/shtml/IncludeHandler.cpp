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

#include "frame/log.h"
#include "frame/httpact.h"
#include "base/systhr.h"
#include "IncludeHandler.h"
#include "ShtmlHelperFuncs.h"
#include "ShtmlConfig.h"
#include "dbtShtml.h"

IncludeHandler::IncludeHandler(pblock* pb) 
: _path(0), _type(FILE), _valid(PR_TRUE)
{
  const char* path = pblock_findval("virtual", pb);
  if (path) {
    _type = VIRTUAL;
  }
  else {
    path = pblock_findval("file", pb);
    if (path) {
      _type = FILE;
    }
  }

  if (path && *path) 
    _path = PERM_STRDUP(path);
  else {
    _valid = PR_FALSE;
  }
}


IncludeHandler::~IncludeHandler()
{
  if (_path)
    PERM_FREE(_path);
}

PRBool
IncludeHandler::Execute(pblock* pb, Session* sn, Request* rq, 
												PageStateHandler& pageState)
{
    ShtmlConfig cfg(pb);
    PRBool res = PR_TRUE; 
    
    if ((res = IsValid()) == PR_FALSE)
        goto done;

    /* Send the headers before dispatching the internal request */
    res = pageState.SendHeaders();

    if (ISMHEAD(rq)) {
            goto done;
    }

    if (servact_include_virtual(sn, rq, _path, NULL) != REQ_PROCEED) {
        res = PR_FALSE;
    }

done:
    if (res == PR_FALSE) {
        pageState.WriteErrMsg();    
    }
    return res;
}

