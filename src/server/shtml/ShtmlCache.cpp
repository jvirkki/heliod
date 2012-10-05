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

#include "ShtmlCache.h"
#include "safs/nsfcsafs.h"

static NSFCCache _gShtmlCachePtr;
static NSFCPrivDataKey _prvDataKey;


static void 
CacheEvictorFunction(NSFCCache cache, const char *filename,
                     NSFCPrivDataKey key, void *privateData)
{
  PR_ASSERT(privateData);
  ShtmlPage* pg = (ShtmlPage*)privateData;
  delete pg;
}


PRBool
InitializeCache()
{
  PRBool res = PR_TRUE;
  _gShtmlCachePtr = GetServerFileCache();
  PR_ASSERT(_gShtmlCachePtr != NULL);
  _prvDataKey = NSFC_NewPrivateDataKey(_gShtmlCachePtr, CacheEvictorFunction);

  return res;
}


PRBool
GetShtmlPageFromCache(const char *path, ShtmlCachedPage& cachedPage)
{
  PRBool res = PR_FALSE;
  NSFCStatus rfc;

  cachedPage.shtmlPage = 0;
  cachedPage.cacheHandle = NSFCENTRY_INIT;

  rfc = NSFC_LookupFilename(path, &cachedPage.cacheHandle, _gShtmlCachePtr);
  if (rfc == NSFC_OK) {
    // We now have a read lock
    PR_ASSERT(cachedPage.cacheHandle);
    void** p = (void**)&cachedPage.shtmlPage;
    rfc = NSFC_GetEntryPrivateData(cachedPage.cacheHandle,
                                   _prvDataKey, p, _gShtmlCachePtr);

    if (rfc == NSFC_BUSY || rfc == NSFC_NOTFOUND) {
      NSFC_ReleaseEntry(_gShtmlCachePtr, &cachedPage.cacheHandle);
    }
    else {
      PR_ASSERT(cachedPage.shtmlPage || rfc == NSFC_NOTFOUND);
      res = PR_TRUE;
    }
  }

  return res;
}

void
ReleaseShtmlCachedEntry(ShtmlCachedPage& cachedPage)
{
  NSFC_ReleaseEntry(_gShtmlCachePtr, &cachedPage.cacheHandle);
}

PRBool 
InsertShtmlPageIntoCache(ShtmlCachedPage& cachedPage, ShtmlPage* shtmlPage)
{
  PRBool res = PR_TRUE;
  NSFCStatus rfc;

  PR_ASSERT(!cachedPage.shtmlPage);
  PR_ASSERT(shtmlPage);

  /* Nothing there, so store our page */
  rfc = NSFC_SetEntryPrivateData(cachedPage.cacheHandle, _prvDataKey,
                                 shtmlPage, _gShtmlCachePtr);
  if (rfc != NSFC_OK) {
    /* Failed, maybe because cache is reaching resource limits */
    res = PR_FALSE;
  }

  return res;
}

