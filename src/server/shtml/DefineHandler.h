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

#ifndef  _DEFINEHANDLER_H_ 
#define  _DEFINEHANDLER_H_

#include "NSBaseTagHandler.h"
#include "ShtmlDefines.h"
#include "base/systhr.h"

class DefineHandler : public NSBaseTagHandler {
  public : 
    DefineHandler(pblock* pb);
    ~DefineHandler();
    int Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg);

  private :
    pblock* _pb;
};

inline
DefineHandler::DefineHandler(pblock* pb)
: _pb(0)
{
  if (pb) {
    // Set thread mallok key to NULL to ensure that
    // pbloks get PERM_MALLOCED
    int malloc_key = getThreadMallocKey(); 
    void *oldpool = INTsysthread_getdata(malloc_key); 
    INTsysthread_setdata(malloc_key, NULL); 
    _pb = pblock_create(SHTML_PARAMSIZE);
    if (_pb)
      pblock_copy(pb, _pb);
    // Reset original thread malloc key
    INTsysthread_setdata(malloc_key, oldpool); 
  }
}

inline
DefineHandler::~DefineHandler()
{
  if (_pb) {
    int malloc_key = getThreadMallocKey(); 
    void *oldpool = INTsysthread_getdata(malloc_key); 
    INTsysthread_setdata(malloc_key, NULL); 
    pblock_free(_pb);
    // Reset original thread malloc key
    INTsysthread_setdata(malloc_key, oldpool);
  }
}
    

inline PRBool
DefineHandler::Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg)
{
  PRBool res = PR_TRUE;
  if (_pb) {
    pg.AddVars(_pb);
  }

  return res;
}

#endif /* _DEFINEHANDLER_H_ */
    
