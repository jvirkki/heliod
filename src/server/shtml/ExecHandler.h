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

#ifndef  _EXEC_HADNLER_H_ 
#define  _EXEC_HADNLER_H_

#include "NSBaseTagHandler.h"

class ExecHandler : public NSBaseTagHandler {
  public : 
    ExecHandler(pblock* pb);
    ~ExecHandler();
    PRBool Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg);
    PRBool IsValid() { return _valid;};

  private :
    PRBool ExecuteCgi(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg);
    PRBool ExecuteCmd(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg);

    enum ExecType {CGI, CMD};
    ExecType _type;
    char* _cgi;
    PRBool      _valid;
};

inline ExecHandler::ExecHandler(pblock* pb)
: _valid(PR_TRUE), _cgi(0)
{
  if (pb) {
    char* url = pblock_findval("cgi", pb);
    
    if (url) {
      _type = CGI;
      _cgi = PERM_STRDUP(url);
    }
    else {
      char* cmd = pblock_findval("cmd", pb);
      if (cmd) {
        _type = CMD;
      _cgi = PERM_STRDUP(cmd);
      }
      else {
        _valid = PR_FALSE;
      }
    }
  }
  else {
    _valid = PR_FALSE;
  }
}

inline ExecHandler::~ExecHandler()
{
  if (_cgi)
    PERM_FREE(_cgi);
}

#endif /* _EXEC_HADNLER_H_ */
    
