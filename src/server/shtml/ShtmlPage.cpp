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

#include "prio.h"
#include "base/session.h"
#include "frame/log.h"


#include "ShtmlPage.h"
#include "ShtmlElement.h"
#include "ShtmlElementList.h"
#include "ShtmlTagParser.h"


static int _gShtmlPageSlot = -1;


void ShtmlPage::Init()
{
  _gShtmlPageSlot = session_alloc_thread_slot(NULL);
}


ShtmlPage* ShtmlPage::GetCurrentPage()
{
  return (ShtmlPage*)session_get_thread_data(NULL, _gShtmlPageSlot);
}


ShtmlPage::ShtmlPage(const char* path, Request* rq, Session* sn)
: _path(0), _contentType(0), _listP(0), _valid(PR_TRUE),
  _protocol_error(-1),
  _pageFnList(0), _buff(0)
{
  PR_ASSERT(path);
  _path = PERM_STRDUP(path);
  _contentType = PERM_STRDUP("text/html");
  _listP = new ShtmlElementList();
  if (!_listP)
    _valid = PR_FALSE;
  if (OpenFileAndRead(rq, sn)) {
    Parse(rq, sn);
  }
  else {
    _valid = PR_FALSE;
  }
  if (_buff) 
    FREE(_buff);
}


ShtmlPage::~ShtmlPage()
{
  if(_path)
    PERM_FREE(_path);
  if (_contentType)
    PERM_FREE(_contentType);
  if (_listP)
    delete _listP;
  if (_pageFnList)
    delete _pageFnList;
}

  
void
ShtmlPage::SetContentType(const char* contentType)
{
  if (_contentType) {
    PERM_FREE(_contentType);
    _contentType = 0;
  }
  if (contentType)
    _contentType = PERM_STRDUP(contentType);
}


PRBool 
ShtmlPage::Execute(pblock* pb, Session* sn, Request* rq)
{
  PRBool res = IsValid();
  if (res == PR_TRUE) {
    if (_contentType) {
      param_free(pblock_remove("content-type", rq->srvhdrs));
      pblock_nvinsert("content-type", _contentType, rq->srvhdrs);
    }

    int numTagUserData = 0;
    if (_pageFnList)
      numTagUserData = _pageFnList->NumPageDataSlotsRequired();
    TagUserData* userDataP = 0;
    if (numTagUserData > 0) {
      userDataP = new TagUserData[numTagUserData];
      for (int i=0;i < _pageFnList->Length(); i++) {
        const ShtmlPageFnData& d = _pageFnList->GetElement(i);
        if (d.loadFn) {
          PR_ASSERT(d.pageLoadDataIndex >= 0);
          PR_ASSERT(d.pageLoadDataIndex < numTagUserData);
          userDataP[d.pageLoadDataIndex] = d.loadFn(pb, sn, rq);
        }
      }
    }

    ShtmlElement* p = _listP->GetFirst();
    PRBool res = PR_TRUE;
    while (p) {
      int pageLoadDataIndex = p->GetPageDataindex();
      TagUserData pageData = 0;
      if (pageLoadDataIndex >= 0)
        pageData = userDataP[pageLoadDataIndex];
      
      res = p->Execute(pb, sn, rq, pageData);
      p = p->next;
    }

    for (int i=0; _pageFnList && (i < _pageFnList->Length()); i++) {
      const ShtmlPageFnData& d = _pageFnList->GetElement(i);
      if (d.unLoadFn) {
        TagUserData  pageData = 0; 
        if (d.pageLoadDataIndex >= 0)
          pageData = userDataP[d.pageLoadDataIndex];
        d.unLoadFn(pageData);
      }
    }

    if (userDataP) {
      delete []userDataP;
      userDataP = 0;
    }
  }

  return res;
}


PRBool ShtmlPage::IsValid()
{
  return _valid;
}


PRBool 
ShtmlPage::HasBeenModified()
{
  // Use file cache to validate that the file has not changed
  return PR_FALSE;
}


PRBool 
ShtmlPage::OpenFileAndRead(Request* rq, Session* sn)
{
  _numBytes = 0;
  _buff = NULL;

  // Use file cache here
  PRBool res = PR_TRUE;
  PRFileDesc* fd = PR_Open(_path, PR_RDONLY, 0);
  if (fd) {
    PRFileInfo buf;
    PRStatus statRes = PR_GetOpenFileInfo(fd, &buf);
    if (statRes != PR_SUCCESS) {
      _error.append("Uanble to stat file ");
      _error.append(_path);
      _protocol_error = PROTOCOL_NOT_FOUND;
      res = PR_FALSE;
    }
    else {
      PRUint32 sz = buf.size;
      _buff = (char*)MALLOC(sz+1);
      if (_buff) {
        _buff[sz] = '\0';
        _numBytes = 0;
        PRUint32 bytesToRead = sz+1;
        PRInt32 bytesRead = PR_Read(fd, _buff, bytesToRead); 
        if (bytesRead == sz) {
          // done - file was sz bytes and we got it in one chunk
          _numBytes = bytesRead;
          res = PR_TRUE;
        }
        else {
          /* bytesRead > or <  sz */
          while (1) {
            if (bytesRead == 0) {
              res = PR_TRUE;
              break;
            }
            else if (bytesRead < 0) {
              res = PR_FALSE;
              char temp[1024];
              _numBytes = 0;
              FREE(_buff);
              _buff = 0;
              break;
            }

            _numBytes += bytesRead;
            if (_numBytes >= sz) {
              // We need to realloc
              _buff = (char*) REALLOC(_buff, sz+4096+2);
              sz = sz+4096;
              if (_buff == 0) {
                // REALLOC failed; bail
                _numBytes = 0;
                res = PR_FALSE;
                char temp[24];
                sprintf(temp, "%d", sz); 
                _error.append("Unable to grow buffer to ");
                _error.append(temp);
                _error.append(" for reading ");
                _error.append(_path);
                break;
              }
              else {
                bytesToRead = 4096;
              }
            }
            else {
              // Still have space to read; calculate bytesToRead
              bytesToRead = bytesToRead - bytesRead;
            }
            // Let us read for more
            bytesRead = PR_Read(fd, _buff+_numBytes, bytesToRead);
          } /* end of while */
        }    
      }
      else {
        char temp[24];
        sprintf(temp, "%d", sz); 
        _error.append("Unable to allocate ");
        _error.append(temp);
        _error.append(" for reading ");
        _error.append(_path);
        res = PR_FALSE;
      }
   }  
   PR_Close(fd);
  }
  else {
    _error.append("Unable to open ");
    _error.append(_path);
    _error.append(" for reading ");
    _protocol_error = PROTOCOL_NOT_FOUND;
    res = PR_FALSE;
  }

  if (_buff) {
    _buff[_numBytes] = '\0';
  }

  return res;
}
 
void 
ShtmlPage::Parse(Request* rq, Session* sn)
{
  static const char* SHTML_PREFIX = "<!--#"; 
  static const char* SHTML_SUFFIX = "-->" ;
  static const int SHTML_PREFIX_LEN = 5;
  static const int SHTML_SUFFIX_LEN = 3;

  int done  = 0;
  int bytesRemaining = _numBytes;
  char* curr = _buff;
  char* base = _buff;
  pblock* tagParams = pblock_create(5);

  session_set_thread_data(sn, _gShtmlPageSlot, this);

  // go through the buffer looking for "<"'s
  while (!done && (bytesRemaining > 0)) {
    char* nextOpenTag = strchr(base, '<');

    if (!nextOpenTag) {
      // no more "<"'s found, so the rest is one entire chunk of text
      // make an element out of it and store it
      ShtmlElement* p = ShtmlTagParser::GetTextElement(*this, 0,
                                                    curr, bytesRemaining);
      PR_ASSERT(p);
      if (p)
        _listP->Add(p);
      bytesRemaining = 0;
    }
    else {
      // Are we dealing with a standard SSI type tag or with a HTML type tag
      if (strncmp(nextOpenTag, SHTML_PREFIX, SHTML_PREFIX_LEN) == 0) {

        // We got a start SSI tag. Find the End tag.
        char* endTag   = strstr(nextOpenTag+SHTML_PREFIX_LEN, SHTML_SUFFIX); 
        if (!endTag) {
          // We got a start tag but not corresponding end tag;
          // Treat this as a chunk of text
          ShtmlElement* p = ShtmlTagParser::GetTextElement(*this, 0,
                                                     curr, bytesRemaining);
          PR_ASSERT(p);
          if (p)
            _listP->Add(p);
          bytesRemaining = 0;
        }
        else {
          // We got a start tag and the corresponding end tag;
          // Now find the element for which we have a start and end tag

          if (nextOpenTag != curr) {
            // but before we do that, we check if
            // there was "text" between current and start
            // if so, make a text element and add it to the list
            size_t textlen = nextOpenTag-curr;
            ShtmlElement* p = ShtmlTagParser::GetTextElement(*this, 0,
                                                         curr, textlen);
            PR_ASSERT(p);
            if (p)
              _listP->Add(p);
            bytesRemaining -= textlen;
            curr = base = nextOpenTag;
          }

          // null terminate the buffer before passing it to ParseSSI
          char temp = *endTag;
          *endTag = '\0';

          ShtmlElement* p = 0;
          NSString errStr;
          if (ShtmlTagParser::ParseSSI(nextOpenTag+SHTML_PREFIX_LEN, *this,
                                       p, errStr) == PR_FALSE)
            log_error(LOG_WARN, "ShtmlPage::Parse", sn, rq, "%s",
                     (char*)(const char*)errStr);
          if (p) 
            _listP->Add(p);

          *endTag = temp; // restore replaced char
          bytesRemaining -= (endTag-curr+SHTML_SUFFIX_LEN);
          curr = base = endTag+SHTML_SUFFIX_LEN;
        }
      }
      else {
        // We are dealing with HTML type tag
        // Find the ending ">"
        char* endOfOpenTag = strchr(nextOpenTag, '>');
        if (!endOfOpenTag) {
          // We got a "<"  but no ">"
          // Treat the whole thing including the preceding text as a chunk of text
          ShtmlElement* p = ShtmlTagParser::GetTextElement(*this, 0,
                                                     curr, bytesRemaining);
          PR_ASSERT(p);
          if (p)
            _listP->Add(p);
          bytesRemaining = 0;
        }
        else {
          // We got a start "<" and a ">"
          char* buf = nextOpenTag + 1; // jump over '<'
          ShtmlElement* p = 0;
          NSString errStr;
          int len = ShtmlTagParser::ParseHtml(buf, *this, p, errStr);
          
          if (len > 0) {
            // Registered html tag which was processed without errors
            if (nextOpenTag != curr) {
              // but before we add it, we check if
              // there was "text" between current and start of the tag
              // if so, make a text element and add it to the list
              size_t textlen = nextOpenTag-curr;
              ShtmlElement* p = ShtmlTagParser::GetTextElement(*this, 0,
                                                           curr, textlen);
              PR_ASSERT(p);
              if (p)
                _listP->Add(p);
              bytesRemaining -= textlen;
              curr = base = nextOpenTag;
            }
            PR_ASSERT(p);
            if (p) 
              _listP->Add(p);
            len += 1; // for the '<' we jumped over earlier
            bytesRemaining -= len;
            curr += len;
            base += len;
          }
          else {
            if (len < 0) // error occured. Log error
              log_error(LOG_WARN, "ShtmlPage::Parse", sn, rq,
                     (char*)(const char*)errStr);

            // leave curr as it is, we just basically got some more text
            // the next search for a tag starts at the next character after 
            // the begin (<) of the  of the last tag, though.
            // We cannot start the next search from the end of the last tag
            // since we would then miss embeded SSI tags in HTML tags
            // Ex: <text name="me" value="<!--# ECHO var="DATE_LOCAL"-->
            base = nextOpenTag + 1;
          }
        }
      }
    }
  } // end of while   

  session_set_thread_data(sn, _gShtmlPageSlot, NULL);
}


PRBool
ShtmlPage::InitializePageFnList()
{
  _pageFnList = new ShtmlPageFnList();
  return (_pageFnList == 0 ? PR_FALSE : PR_TRUE);
}

PRBool
ShtmlPage::AddTagToPageFnLst(const char* tag, 
                             ShtmlElement* shtmlElem,
                             ShtmlTagPageLoadFunc loadFn,
                             ShtmlTagPageUnLoadFunc unLoadFn)
{
  PRBool res = PR_TRUE;
  if (loadFn || unLoadFn) {
    if (!_pageFnList) 
      res = InitializePageFnList();
    if (res == PR_TRUE) { 
      int index = _pageFnList->Find(loadFn);
      if (index == -1) {
        index = _pageFnList->Insert(tag, loadFn, unLoadFn);
        if (loadFn) {
          if (index >= 0) {
            shtmlElem->SetPageDataindex(index);
          }
          else {
            res = PR_FALSE;
          }
        }
      }
      else {
        shtmlElem->SetPageDataindex(index);
      }
    }
  }

  return res;
}

