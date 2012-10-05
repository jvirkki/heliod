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

#ifndef _SHTMLPAGE_H_
#define _SHTMLPAGE_H_

#include <nspr.h>
#include <netsite.h>

#include "ShtmlElementList.h"
#include "ShtmlPageFnList.h"
#include "shtml_public.h"
#include "support/NSString.h"



class ShtmlPage {
  public : 

    static void Init();
    static ShtmlPage* GetCurrentPage();
    ShtmlPage(const char* path, Request* rq, Session* sn);
    ~ShtmlPage();
    PRBool Execute(pblock* pb, Session* sn, Request* rq);
    PRBool IsValid();
    PRBool HasBeenModified();
    PRBool OpenFileAndRead(Request* rq, Session* sn);
    void Parse(Request* rq, Session* sn);
    PRBool InitializePageFnList();
    PRBool AddTagToPageFnLst(const char* tag,
                             ShtmlElement* shtmlElem,
                             ShtmlTagPageLoadFunc loadFn,
                             ShtmlTagPageUnLoadFunc unLoadFn);
    void SetContentType(const char* contentType);
    PRBool Write(Request* rq, Session* sn,
                 const char* buff, size_t len);
    const char* GetError() { return _error; }

    /**
     * call GetProtocolError() when IsValid() returns PR_FALSE
     * _protocol_error has value -1 when not set
     *
     */
    int GetProtocolError() { return _protocol_error; }

  private:
    ShtmlElementList* _listP;
    ShtmlPageFnList* _pageFnList;
    char*         _path;
    char*         _contentType;
    PRBool        _valid;
    int		  _protocol_error;
    char*         _buff;
    size_t        _numBytes;
    NSString      _error;
};

#endif /* _SHTMLPAGE_H_ */
