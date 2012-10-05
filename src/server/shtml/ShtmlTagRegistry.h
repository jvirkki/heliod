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

#ifndef _SHTMLTAGREGISTRY_H_
#define _SHTMLTAGREGISTRY_H_

#include <support/SimpleHash.h>
#include <support/NSString.h>

#include "shtml_public.h"

class TagRegistryElement {
  public :
    enum ShtmlTagType {SHTML_SSI, SHTML_HTML};

    TagRegistryElement(const char* tag,
                    ShtmlTagInstanceLoad parseFn,
                    ShtmlTagInstanceUnload cleanupFn,
                    ShtmlTagExecuteFunc execFn,
                    ShtmlTagPageLoadFunc pageLoadFn,
                    ShtmlTagPageUnLoadFunc pageUnLoadFn,
                    ShtmlTagType tagType=SHTML_SSI);

    ~TagRegistryElement();
    ShtmlTagInstanceLoad GetParseFunction() { return _ctor; }
    ShtmlTagInstanceUnload GetCleanupFunction() { return _dtor; }
    ShtmlTagExecuteFunc    GetExecutionFunction() {return _execFn; }
    ShtmlTagPageLoadFunc   GetPageLoadFunction() {return _pageLoadFn;}
    ShtmlTagPageUnLoadFunc GetPageUnLoadFunction() {return _pageUnLoadFn;}
    ShtmlTagType GetTagType() {return _tagType;}

  private:
    NSString               _tag;
    ShtmlTagInstanceLoad   _ctor;
    ShtmlTagInstanceUnload _dtor;
    ShtmlTagExecuteFunc _execFn;
    ShtmlTagPageLoadFunc _pageLoadFn;
    ShtmlTagPageUnLoadFunc _pageUnLoadFn;
    ShtmlTagType           _tagType;
};


const int SHTMLTAGREG_SIZE_DEFAULT = 25;

class ShtmlTagRegistry {
  public :
    static void Init(int tagRegSize=SHTMLTAGREG_SIZE_DEFAULT) 
    { 
      if (!_gShtmlTagReg)
        _gShtmlTagReg = new ShtmlTagRegistry(tagRegSize);
    }

    static ShtmlTagRegistry* GetRegistry() 
    {
      Init();
      return _gShtmlTagReg;
    };

    PRBool AddTagHandler(const char* tag, ShtmlTagInstanceLoad ctor,
                         ShtmlTagInstanceUnload dtor,
                         ShtmlTagExecuteFunc exeFn,
                         ShtmlTagPageLoadFunc pageLoadFn,
                         ShtmlTagPageUnLoadFunc pageUnLoadFn,
                         TagRegistryElement::ShtmlTagType tagType);
    TagRegistryElement* FindTagHandler(const char* tag);
    TagRegistryElement* FindHtmlTagHandler(const char* tag);
    static TagRegistryElement* FindTextTagHandler();

  private :
    ShtmlTagRegistry(int size);
    ~ShtmlTagRegistry();

    static ShtmlTagRegistry* _gShtmlTagReg;
    static TagRegistryElement* _cachedTextHandler;
    static TagRegistryElement* _cachedIncludeHandler;
    SimpleStringHash* _table;
};

NSAPI_PUBLIC int shtml_register_tag(const char* tag,
                                    ShtmlTagInstanceLoad ctor,
                                    ShtmlTagInstanceUnload dtor,
                                    ShtmlTagExecuteFunc execFn,
                                    ShtmlTagPageLoadFunc pageLoadFn,
                                    ShtmlTagPageUnLoadFunc pageUnLoadFn,
                                    TagRegistryElement::ShtmlTagType tagType);

#endif /* _SHTMLTAGREGISTRY_H_ */
