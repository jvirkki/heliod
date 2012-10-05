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

#include "base/session.h"
#include "frame/req.h"
#include "frame/log.h"
#include "frame/func.h"
#include "frame/protocol.h"
#include "frame/httpfilter.h"
#include "httpdaemon/httprequest.h"

#include "shtml_public.h"
#include "ShtmlSaf.h"
#include "ShtmlTagRegistry.h"
#include "NSPageState.h"
#include "NSTagHandlers.h"
#include "ShtmlPage.h"
#include "ShtmlCache.h"
#include "dbtShtml.h"

struct TagRegEntry {
  const char* tag;
  ShtmlTagInstanceLoad ctor;
  ShtmlTagInstanceUnload dtor;
  ShtmlTagExecuteFunc exFn;
  ShtmlTagPageLoadFunc pageLoadFn;
  ShtmlTagPageUnLoadFunc pageUnLoadFn;
};

static TagRegEntry NSTagHandlers[] = {
 {"echo", NSEchoCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"text", NSTextCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"include", NSIncludeCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"flastmod", NSFileInfoCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"fsize", NSFileInfoCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"header", NSHeaderCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"redirect", NSRedirectCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"exec", NSExecCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"config", NSConfigCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"define", NSDefineCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler},
 {"printenv", NSPrintenvCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler}
};

static TagRegEntry NSHTMLTagHandlers[] = {
 {"servlet", NSServletCtor, NSDtor, NSHandler, NSPageLoadHandler, NSPageUnLoadHandler}
};

static PRUint16 maxSHTMLDepth = 10;

static PRBool _gUseCache = PR_TRUE;


PRStatus
shtml_init_early(void)
{
  ShtmlPage::Init();

  ShtmlTagRegistry::Init();
  ShtmlTagRegistry::GetRegistry();

  // Add our default tags
  int numHandlers = sizeof(NSTagHandlers)/sizeof(TagRegEntry);
  int i = 0;
  for (i = 0; i < numHandlers; i++) {
    shtml_add_tag(NSTagHandlers[i].tag,
                  NSTagHandlers[i].ctor,
                  NSTagHandlers[i].dtor,
                  NSTagHandlers[i].exFn,
                  NSTagHandlers[i].pageLoadFn,
                  NSTagHandlers[i].pageUnLoadFn);
  }

  // Add our default HTML tags
  numHandlers = sizeof(NSHTMLTagHandlers)/sizeof(TagRegEntry);
  for (i = 0; i < numHandlers; i++) {
    shtml_add_html_tag(NSHTMLTagHandlers[i].tag,
                       NSHTMLTagHandlers[i].ctor,
                       NSHTMLTagHandlers[i].dtor,
                       NSHTMLTagHandlers[i].exFn,
                       NSHTMLTagHandlers[i].pageLoadFn,
                       NSHTMLTagHandlers[i].pageUnLoadFn);
  }

  return PR_SUCCESS;
}


PRStatus
shtml_init_late(void)
{
  InitializeCache();

  return PR_SUCCESS;
}


NSAPI_PUBLIC int 
shtml_init(pblock* pb, Session* sn, Request* rq)
{
  int res = REQ_PROCEED;

  char* maxdepth = pblock_findval("shtmlMaxDepth", pb);
  if ((maxdepth) && (atoi(maxdepth)>0))
    maxSHTMLDepth = atoi(maxdepth);

  // shtml page caching can be disabled by adding useCache=no to shtml-init
  const char* useCache = pblock_findval("useCache", pb);
  _gUseCache = util_getboolean(useCache, _gUseCache);

  const char* cgiInitEnv = pblock_findval("addCgiInitVars", pb);
  if (util_getboolean(cgiInitEnv, PR_FALSE)) {
    PageStateHandler::NeedToAddCgiInitEnv(PR_TRUE);
  }

  return res;
}


NSAPI_PUBLIC int 
shtml_send(pblock* pb, Session* sn, Request* rq)
{
  int res = REQ_PROCEED;

  if (INTrequest_is_internal(rq)) {
    PRUint16 depth = GetCurrentRecursionDepth(); // depth level of SSI
    if (depth > maxSHTMLDepth) {
      log_error(LOG_FAILURE, "parse-html", sn, rq,
                XP_GetAdminStr(DBT_shtmlsafError2), depth, maxSHTMLDepth);
      return REQ_ABORTED;        /* prevent stack overflow */
    }
    IncrementRecursionDepth();
  }

#if 0
  if ( CM_get_status() )  {       /* if cm is on */
    char *queryStr = pblock_findval("query", rq->reqpb);
    if ( queryStr && CM_is_cm_query( queryStr ) )
      return REQ_NOACTION;        /* let CM handle this */
  }
#endif

  PRBool foundInCache = PR_FALSE;
  ShtmlPage* page = 0;
  ShtmlCachedPage cachedPage;
    
  char* path = pblock_findval("path", rq->vars);

  PRBool useCache = _gUseCache;

  // shtml page caching can be also be disabled at the service
  // function level by adding useCache=no to shtml-send
  if (useCache == PR_TRUE) {
    char* val = pblock_findval("useCache", pb);
    if (val && (toupper(*val) == 'N'))
      useCache = PR_FALSE;
  }

  if (useCache == PR_TRUE) {
    // Get cache entry (which may or may not contain an ShtmlPage)
    foundInCache = GetShtmlPageFromCache(path, cachedPage);
    if (foundInCache == PR_TRUE)
      page = cachedPage.shtmlPage;
  }

  if (!page) {
    page = new ShtmlPage(path, rq, sn);
    log_error(LOG_VERBOSE, "parse-html", sn, rq, 
              XP_GetAdminStr(DBT_shtmlsafError4));
  }

  if (!page) {
    res = REQ_ABORTED;
    log_error(LOG_FAILURE, "parse-html", sn, rq,
              XP_GetAdminStr(DBT_shtmlsafError5));    
  }
  else {
    if (foundInCache == PR_TRUE && !cachedPage.shtmlPage) {
      // Cache entry didn't have an ShtmlPage, so insert the one we created
      if (InsertShtmlPageIntoCache(cachedPage, page) != PR_TRUE) {
        ReleaseShtmlCachedEntry(cachedPage);
        foundInCache = PR_FALSE;
      }
    }

    if (page->IsValid() == PR_FALSE) {
      const char* err = page->GetError();
      log_error(LOG_FAILURE, "parse-html", sn, rq, 
                err ? (char*)err : (char *)"Invalid ShtmlPage");
      int protocol_error;
      if ((protocol_error = page->GetProtocolError()) != -1)
        protocol_status(sn, rq, protocol_error, NULL);
      res = REQ_ABORTED;
    }
    else {
      protocol_status(sn, rq, PROTOCOL_OK, NULL);

      httpfilter_buffer_output(sn, rq, PR_TRUE);

      if (page->Execute(pb, sn, rq) == PR_FALSE) {
        log_error(LOG_FAILURE, "parse-html", sn, rq,
                  XP_GetAdminStr(DBT_shtmlsafError6));    
      }
    }
    if (foundInCache == PR_TRUE) {
      // Give up cache entry read lock
      ReleaseShtmlCachedEntry(cachedPage);
    }
    else {
      delete page;
    }
  }
#if 0
  util_itoa(si.bytes_sent, bytes);
  pblock_nvinsert("content-length", bytes, rq->srvhdrs);
#endif

#if 0
  if ( CM_get_status() ) {         /* if cm is on */
    if ( cmVtbl.CMTrigger )
      cmVtbl.CMTrigger( pb, sn, rq, NS_ACCESS_MSG );
  }
#endif

  if (INTrequest_is_internal(rq)) {
    DecrementRecursionDepth();
  }
         
  return res;
}

