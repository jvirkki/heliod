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

#include <frame/log.h>

#include "ShtmlTagParser.h"
#include "ShtmlTagRegistry.h"
#include "ShtmlPage.h"



ShtmlTagParser::ShtmlTagParser()
{
}

ShtmlTagParser::~ShtmlTagParser()
{
}


PRBool
ShtmlTagParser::ParseSSI(const char* buff, ShtmlPage& shtmlPage,
                      ShtmlElement*& shtmlElemP, NSString& errStr)
{
  PRBool res = PR_TRUE;
  char* cur = (char*)buff;

  // Get to the first non-space character
  for (; *cur && isspace(*cur); cur++);

  const char* tag = cur;
  // check if there is a tag at all
  if (*tag) {
    for (; *cur && !isspace(*cur); ++cur); // Isolate the tag

    char replaceChar = *cur;
    char* replacedChar = cur;
    if (*cur)
      *cur++ = '\0'; // null terminate tag

    // Check if it is a registered SSI tag
    TagRegistryElement* e =
               (ShtmlTagRegistry::GetRegistry())->FindTagHandler(tag);

    if (e && (e->GetTagType() == TagRegistryElement::SHTML_SSI)) {
      const int SHTML_PARAMSIZE = 5;
      pblock *tagParams = pblock_create(SHTML_PARAMSIZE);

      // check for tag params
      for (; *cur && isspace(*cur); ++cur); // skip white space
      if (*cur) {
        // Parse tag params
        if (INTpblock_str2pblock_lowercasename(cur, tagParams) == -1) {
          errStr.append("Error parsing tag arguments for tag "); 
          errStr.append(tag); 
          res = PR_FALSE;
        }
      }

      // Find tag Handler
      shtmlElemP = GetElement(tag, shtmlPage, tagParams);
      if (!shtmlElemP)  {
        errStr.append("Could not create element for tag ");
        errStr.append(tag);
        res = PR_FALSE;
      }

      pblock_free(tagParams);
    }
    else {
      errStr.append("Not a valid SSI tag : ");
      errStr.append(tag);
      res = PR_FALSE;
    }

    *replacedChar = replaceChar;
  } 
  else { // no tag found
    errStr.append("Could not find a tag");
    res = PR_FALSE;
  }

  return res;
}


/* 
  The return values for ParseHtml are :
  0   : No processing done on data as in case of regular html
  -1  : An error occured. Error string in errStr
  +ve : No. of bytes processed. shtmlElemP is set.
*/
int 
ShtmlTagParser::ParseHtml(const char* buff, ShtmlPage& shtmlPage,
                      ShtmlElement*& shtmlElemP, NSString& errStr)
{
  int ret = 0;

  char* cur = (char*)buff;
  char* endOfOpen = strchr(cur, '>');

  // Tags cannot have spaces in them. If we see "</" it is an end tag
  // Return 0 to indicate that no processing has been done.
  const char* tag = cur;
  if (*tag == '/')
    return 0;

  // Isolate the tag
  for (; *cur && !isspace(*cur) && cur != endOfOpen; cur++);
  
  char replaceChar = *cur;
  char* replacedChar = cur;
  *cur++ = '\0';

  // We have the tag name. Find tag Handler
  TagRegistryElement* e = (ShtmlTagRegistry::GetRegistry())->FindHtmlTagHandler(tag);
  if (e && (e->GetTagType() == TagRegistryElement::SHTML_HTML)) {
    // this is a registered html tag. Check if we find the end tag 
    // Tags cannot have spaces within them
    char* closeTag = (char*) MALLOC(strlen(tag)+4);
    sprintf(closeTag, "</%s>", tag);

    // search for close tag
    char* startOfClose = strstr(endOfOpen + 1, closeTag);

    if (!startOfClose) {
      // No closeTag found. No more processing
      errStr.append("No closing tag found for tag "); 
      errStr.append(tag); 
      // flag error by returning -1
      ret = -1;
    }
    else {  // found the </close> tag

      // Get to the first non-space character
      for (; *cur && isspace(*cur); cur++);

      const int SHTML_PARAMSIZE = 5;
      pblock* tagParams = pblock_create(SHTML_PARAMSIZE);

      // check if there are any tag params
      if (cur < endOfOpen) {
        // null terminate str before passing to pblock_str2pblock
        char temp = *endOfOpen;
        *endOfOpen = '\0';

        // Parse the tag params
        if (pblock_str2pblock(cur, tagParams) == -1) {
          errStr.append("Error parsing tag arguments for tag "); 
          errStr.append(tag); 
          ret = -1; // return of -1 indicates an error in processing
        }
        *endOfOpen = temp; // restore replaced char
      }

      if (ret != -1) { // no error parsing tag params
        cur = endOfOpen + 1;
        for (; *cur && isspace(*cur); cur++); // skip leading white space

        // get the text between <tag> and </tag>
        int bodyLen = startOfClose - cur;

        // get tag Handler
        if (bodyLen > 0)
          shtmlElemP = GetElement(tag, shtmlPage, tagParams,
                                  cur, (size_t)bodyLen);
        else
          shtmlElemP = GetElement(tag, shtmlPage, tagParams);

        if (shtmlElemP) {
          // Make cur point to end of block, after closing tag
          cur = startOfClose + strlen(closeTag);
          ret = cur - buff; // return number of bytes processed
        }
        else {
          errStr.append("Error creating shtml element for tag "); 
          errStr.append(tag); 
          ret = -1; // return of -1 indicates an error in processing
        }
      }
      pblock_free(tagParams);
    }
    FREE(closeTag);
  }
  else {
    // this is not a registered tag. could be a regular HTML tag.
    // return 0 to indicate that no processing has been done.
    ret = 0;
  }

  *replacedChar = replaceChar; // restore char after tag
  return ret;
}


// VB: Are we ok returnin 0 if not found or should it be text ???
ShtmlElement* 
ShtmlTagParser::GetElement(const char* tag,
                           ShtmlPage& page,
                           pblock* pb,
                           const char* buff,
                           size_t len)
{
  ShtmlElement* res = 0;
  // Find tag Handler
  TagRegistryElement* e = (ShtmlTagRegistry::GetRegistry())->FindTagHandler(tag);
  if (e) {
    ShtmlTagInstanceLoad f = e->GetParseFunction();
    PR_ASSERT(f);
    TagUserData ptr = f(tag, pb, buff, len);
    res = new ShtmlElement(tag, page, e->GetCleanupFunction(),
                           e->GetExecutionFunction(),
                           ptr);
    page.AddTagToPageFnLst(tag, res, 
                           e->GetPageLoadFunction(),
                           e->GetPageUnLoadFunction());
  }
  
  return res;
}

ShtmlElement*
ShtmlTagParser::GetTextElement(ShtmlPage& page, pblock* pb,
                               const char* buff, size_t len)
{
  ShtmlElement* res = 0;

  TagRegistryElement* e = ShtmlTagRegistry::FindTextTagHandler();
  if (e) {
    ShtmlTagInstanceLoad f = e->GetParseFunction();
    PR_ASSERT(f);
    const char* tag = "text";
    TagUserData ptr = f(tag, pb, buff, len);
    res = new ShtmlElement(tag, page, e->GetCleanupFunction(),
                           e->GetExecutionFunction(),
                           ptr);
    page.AddTagToPageFnLst(tag, res,
                           e->GetPageLoadFunction(),
                           e->GetPageUnLoadFunction());
  }

  return res;
}

