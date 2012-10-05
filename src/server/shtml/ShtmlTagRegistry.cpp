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

#include "ShtmlTagRegistry.h"

ShtmlTagRegistry* ShtmlTagRegistry::_gShtmlTagReg = 0;
TagRegistryElement* ShtmlTagRegistry::_cachedTextHandler = 0;
TagRegistryElement* ShtmlTagRegistry::_cachedIncludeHandler = 0;

TagRegistryElement::TagRegistryElement(const char* tag,
                    ShtmlTagInstanceLoad parseFn,
                    ShtmlTagInstanceUnload cleanupFn,
                    ShtmlTagExecuteFunc execFn,
                    ShtmlTagPageLoadFunc pageLoadFn,
                    ShtmlTagPageUnLoadFunc pageUnLoadFn,
                    ShtmlTagType tagType)
: _ctor(parseFn), _dtor(cleanupFn), 
  _execFn(execFn), _pageLoadFn(pageLoadFn), _pageUnLoadFn(pageUnLoadFn),
  _tagType(tagType)
{
  _tag.append(tag);
}

TagRegistryElement::~TagRegistryElement()
{
}



ShtmlTagRegistry::ShtmlTagRegistry(int size)
{
  _table = new SimpleStringHash(size);
  // comm
  _table->setMixCase();
}


ShtmlTagRegistry::~ShtmlTagRegistry()
{
  delete _table;
}


PRBool 
ShtmlTagRegistry::AddTagHandler(const char* tag, ShtmlTagInstanceLoad ctor,
                                ShtmlTagInstanceUnload dtor,
                                ShtmlTagExecuteFunc exeFn,
                                ShtmlTagPageLoadFunc pageLoadFn,
                                ShtmlTagPageUnLoadFunc pageUnLoadFn,
                                TagRegistryElement::ShtmlTagType tagType)
{
  TagRegistryElement *e = new TagRegistryElement(tag, 
                                                ctor, dtor, exeFn,
                                                pageLoadFn, pageUnLoadFn,
                                                tagType); 
  if (strcasecmp(tag, "text") == 0) {
    _cachedTextHandler = e;
  }
  else if (strcasecmp(tag, "include") == 0) {
    _cachedIncludeHandler = e;
  }
    
  if (_table->insert((void*)tag, e) == 0)
    return PR_FALSE;
  else 
    return PR_TRUE;
}


TagRegistryElement*
ShtmlTagRegistry::FindTextTagHandler()
{
  return _cachedTextHandler;
}


TagRegistryElement*
ShtmlTagRegistry::FindTagHandler(const char* tag)
{
  if (tag && (strcasecmp(tag, "include") == 0)) {
    return _cachedIncludeHandler;
  } 
  return (TagRegistryElement*) _table->lookup((void*)tag);
}

TagRegistryElement*
ShtmlTagRegistry::FindHtmlTagHandler(const char* tag)
{
  return (TagRegistryElement*) _table->lookup((void*)tag);
}


NSAPI_PUBLIC int 
shtml_register_tag(const char* tag,
                   ShtmlTagInstanceLoad ctor,
                   ShtmlTagInstanceUnload dtor,
                   ShtmlTagExecuteFunc execFn,
                   ShtmlTagPageLoadFunc pageLoadFn,
                   ShtmlTagPageUnLoadFunc pageUnLoadFn,
                   TagRegistryElement::ShtmlTagType tagType)
{
  int res = REQ_PROCEED;
  ShtmlTagRegistry* registry = ShtmlTagRegistry::GetRegistry();
  if (!registry) {
    res = REQ_ABORTED;
  }
  else {
    if (registry->AddTagHandler(tag, ctor, dtor, execFn,
        pageLoadFn, pageUnLoadFn, tagType) != PR_TRUE)
    {
      res = REQ_ABORTED;
    }
  }
  return res;
}
