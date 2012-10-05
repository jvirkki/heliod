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

/**************************************************************************/
/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE                               */
/* OF NETSCAPE COMMUNICATIONS CORPORATION                                 */
/*                                                                        */
/* Copyright © 1996,1997 Netscape Communications Corporation.  All Rights */
/* Reserved.  Use of this Source Code is subject to the terms of the      */
/* applicable license agreement from Netscape Communications Corporation. */
/*                                                                        */
/* The copyright notice(s) in this Source Code does not indicate actual   */
/* or intended publication of this Source Code.                           */
/**************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "netsite.h"
#include "base/crit.h"

#include "libdbm/mcom_db.h"
#include "libdbm/nsres.h"
#include "i18n.h"

#define MAX_LIBNAME_LEN   32
#define NUM_DATA_BUCKETS 128 /* must be a power of 2 */
#define NUM_LANG_BUCKETS  16 /* must be a power of 2 */
#define NUM_LIB_BUCKETS   16 /* must be a power of 2 */

#define DATA_BUCKET_MASK NUM_DATA_BUCKETS-1
#define LANG_BUCKET_MASK NUM_LANG_BUCKETS-1
#define LIB_BUCKET_MASK  NUM_LIB_BUCKETS-1

/* End Of language List character and string; */
/* can not be first character in any filename */
#define EOL_CHAR   '\1'
#define EOL_STRING "\1"

static char emptyString[] = "";

#define COMPILE_STRINGS_IN_MEMORY
/*  Use #define COMPILE_STRINGS_IN_MEMORY to put message string tables in memory.
 *  This is temporary solution for product which doesn't know where
 *  to load message table from.
 */

NSPR_BEGIN_EXTERN_C
NSAPI_PUBLIC const char *XP_GetStringFromDatabase(const char *strLibraryName, const char *strLanguage, int iToken);
NSPR_END_EXTERN_C

char*
XP_GetStringFromMemory(const char* strLibraryName, int iToken);


/*******************************************************************************/

/*  Global data structure and variable used in the file */

/*
 * pLangList -> LANGLIST       one for each language (link list)
 *                 |
 *                 V
 *   pDBList -> DBFILE         one for each database file,
 *                 |           may support several languages
 *                 V
 *  pNextLib -> LIBLIST        one for each library within database (link list)
 *                 |           (has hash table for resource strings)
 *                 V
 *    pCache -> RESCACHEDATA   one for each resource string
 *                             (link list from hash table)
 */

typedef struct RESCACHEDATA {
  int id;
  char* str;
  struct RESCACHEDATA* nextData;
} RESCACHEDATA;

typedef struct LIBLIST {
  char library[MAX_LIBNAME_LEN];
  struct LIBLIST* nextLib;
  struct RESCACHEDATA* firstData[NUM_DATA_BUCKETS];
} LIBLIST;

typedef struct DBFILE {
  char* filename;
  NSRESHANDLE hresdb;
  struct DBFILE* nextFile;
  struct LIBLIST* firstLib;
} DBFILE;

typedef struct LANGLIST {
  char language[XP_MAX_LANGUAGE_TAG_LEN];
  struct LANGLIST* nextLang;
  struct DBFILE* fileForLang;
} LANGLIST;

static LANGLIST LangListBase = { EOL_STRING,NULL,NULL }; /* first in link list */
static DBFILE DBListBase = { NULL,0,NULL,NULL };    /* first file in link list */

static CriticalSection *reslock = NULL;    /* Mutex variable used by APIs in this file */

static char* pathDB = NULL;        /* keep path passed at initialization */
static char* existDB = NULL;       /* path for database found to exist */
static int existDBSize = 0;

static DBFILE* rootDB = NULL;      /* RootDB reference */

static DBFILE* GetDatabaseList(const char *strLanguage);

/*******************************************************************************/

NSAPI_PUBLIC
void
XP_InitStringDatabase(const char* pathCWD, const char* databaseName)
{
  pathDB = (char*)malloc(strlen(pathCWD)
                   +1
                   +strlen(databaseName)
                   +1);
  if (pathDB)
    {
      sprintf(pathDB,"%s/%s",pathCWD,databaseName);
    }
  else
    {
      fprintf(stderr,"malloc failed for %s/%s\n",
              pathCWD,databaseName);
      pathDB = NULL;
    }

  existDBSize = strlen(pathDB)+XP_EXTRA_LANGUAGE_PATH_LEN+1;
  existDB = (char*)malloc(existDBSize);
  if (!existDB)
    {
      fprintf(stderr,"malloc failed %s +%d\n",pathDB,XP_EXTRA_LANGUAGE_PATH_LEN+1);
    }

  if (!reslock)
    reslock = new CriticalSection;

  /* RootDB initialization with empty string as language*/
  if (!rootDB)
    rootDB = GetDatabaseList(emptyString);
}

/*******************************************************************************/

static char*
LoadStringFromCache(struct LIBLIST* pNextLib, const char* library, int id)
{
    RESCACHEDATA* pCache;

    /* find the library */
    while (pNextLib->nextLib) {
      if (strcmp(pNextLib->library,library)) {
        /* look further */
        pNextLib = pNextLib->nextLib;
      } else {
        /* found the library; use hash table to find entry */
        pCache = pNextLib->firstData[id&DATA_BUCKET_MASK];
        while (pCache->id != 0) {
          if (pCache->id == id) {
            return pCache->str;
          }
          pCache = pCache->nextData;
        }
        return NULL;
      }
    }
    
    /* library was not found; caller will handle this */
    return NULL;
}

/*******************************************************************************/

static char*
SaveStringToCache(struct LIBLIST* pNextLib, const char* library, int id, char* str)
{
    RESCACHEDATA* pCache;
    RESCACHEDATA* newItem;
    int i;
    
    /* find the library */
    while (pNextLib->nextLib) {
      if (strcmp(pNextLib->library,library)) {
        /* look further */
        pNextLib = pNextLib->nextLib;
      } else {
        /* found the library */
        break;
      }
    }
    
    if (pNextLib->nextLib==NULL) {
      /* library was not found; create new library entry */
      pNextLib->nextLib = (LIBLIST*)malloc(sizeof(LIBLIST));
      pNextLib->nextLib->nextLib = NULL;
      strcpy(pNextLib->library,library);
      pNextLib->firstData[0] = 
               (RESCACHEDATA*)malloc(NUM_DATA_BUCKETS*sizeof(RESCACHEDATA));
      for ( i=0 ; i<NUM_DATA_BUCKETS ; i++ ) {
        newItem = pNextLib->firstData[i] = pNextLib->firstData[0]+i;
        newItem->id = 0;
        newItem->str = NULL;
        newItem->nextData = NULL;
      }
    }
    
    /* use hash table to find first entry */
    pCache = pNextLib->firstData[id&DATA_BUCKET_MASK];
    while (pCache->id != 0) {
      pCache = pCache->nextData;
    }

    pCache->id = id;
    pCache->str = str;

    newItem = (RESCACHEDATA*)malloc(sizeof(RESCACHEDATA));
    newItem->id = 0;
    newItem->str = NULL;
    newItem->nextData = NULL;

    pCache->nextData = newItem;

    return str;
}

/*******************************************************************************/

static DBFILE* GetDatabaseList(const char* strLanguage) {

  LANGLIST* pLangList = &LangListBase;
  int compResult;
  DBFILE* pDBList;
  int status;

   /* first find the language */
  while ((compResult=(*(pLangList->language)!=EOL_CHAR))
          && (strcmp(pLangList->language,strLanguage))) {
    pLangList = pLangList->nextLang;
  }
  if (compResult==0) {
    if (*strLanguage) {
      status = XP_FormatLanguageFile(pathDB, strlen(pathDB), strLanguage,
                                     existDB, existDBSize);
      if (status == -1)
        return NULL;
    } else {
      strcpy(existDB, pathDB);
    }

    /* create entry for language if not found */
    pLangList->nextLang = (LANGLIST*)malloc(sizeof(LANGLIST));
    strcpy(pLangList->nextLang->language,EOL_STRING);
    strcpy(pLangList->language,strLanguage);
    /* see if file is already open */
    pDBList = &DBListBase;
    while ((pDBList->nextFile) && (strcmp(pDBList->filename,existDB))) {
      pDBList = pDBList->nextFile;
    }
    pLangList->fileForLang = pDBList;
    /* if not open, open database file, if possible */
    if (pDBList->nextFile==NULL) {
      pDBList->nextFile = (DBFILE*)malloc(sizeof(DBFILE));
      pDBList->nextFile->nextFile = NULL;
      pDBList->filename = strdup(existDB);
      pDBList->firstLib = (LIBLIST*)malloc(sizeof(LIBLIST));
      pDBList->firstLib->nextLib = NULL;
      pDBList->hresdb = NSResOpenTable(existDB, NULL);
    }
  } else {
    /* use previously created entry for language */
    pDBList = pLangList->fileForLang;
  }
  return pDBList;
}

/*******************************************************************************/

static char* LoadString(const char* strLibraryName, DBFILE* pDBList, int iToken) {
  char* pRet = NULL;
  /* Load string from cache if it's there */
  pRet = LoadStringFromCache(pDBList->firstLib, strLibraryName, iToken);
  if (pRet == NULL) {
    /* Load string from DB if it's there */
    pRet = NSResLoadString(pDBList->hresdb, strLibraryName, iToken, 0, NULL);
    /* Store it in the cache */
    if (pRet)
      SaveStringToCache(pDBList->firstLib,strLibraryName, iToken, pRet);
  }
  return pRet;
}

/*******************************************************************************/

NSAPI_PUBLIC
const char *
XP_GetString(const char *strLibraryName,
             XPLanguageAudience audience,
             int iToken)
{
  DBFILE* pDBList;
  char* dbMsg = NULL;

  /* The following code is not thread safe, and therefore needs to be locked. */
  /* In this turns out to be a performance problem, a more sophisticated      */
  /* locking scheme will be needed. For now, just lock the whole function.    */
  if (reslock) reslock->acquire(); //crit_enter(reslock);

  /* Loop through Accept-Language one by one */
  XPLanguageEnumState *state = NULL;
  while (const char *lang = XP_EnumAudienceLanguageTags(audience, &state)) {
    /* Create the DB node if there is a db file for this language */
    pDBList = GetDatabaseList(lang);
    if (!pDBList)
      continue;
    /* get the message if it exist in the DB for this language only */
    dbMsg = LoadString(strLibraryName, pDBList, iToken);
    if (dbMsg && *dbMsg) {
      if (reslock) reslock->release(); // crit_exit(reslock);
      XP_EndLanguageEnum(&state);
      return dbMsg;
    }
  }
  XP_EndLanguageEnum(&state);

  /* Get the message from root DB if the accept-language is null or if lang DB doesn't have message  */
  if (rootDB) {
    dbMsg = LoadString(strLibraryName, rootDB, iToken);
    if (dbMsg && *dbMsg) {
      if (reslock) reslock->release(); // crit_exit(reslock);
      return dbMsg;
    }
  }

#ifdef COMPILE_STRINGS_IN_MEMORY
  /* Get the message from memory if DB doesn't exist or message doesn't exist in DB  */
  dbMsg = XP_GetStringFromMemory(strLibraryName,iToken) ;
#endif

  if (reslock) reslock->release(); // crit_exit(reslock);
  return dbMsg ? dbMsg : emptyString;
}

NSAPI_PUBLIC
const char *
XP_GetStringFromDatabase(const char *strLibraryName,
                         const char *strLanguage,
                         int iToken)
{
    return XP_GetString(strLibraryName, XP_LANGUAGE_AUDIENCE_DEFAULT, iToken);
}

/*******************************************************************************/

#ifdef XP_DEBUG

void
XP_DumpCache(void)  /* debug routine */
{
  LANGLIST*     pLangList = &LangListBase;
  DBFILE*       pDBList = &DBListBase;
  LIBLIST*      pNextLib;
  RESCACHEDATA* pCache;

  int i;
  
  printf("String Cache:  Number of Buckets %d %X   Mask %X\n",
         NUM_DATA_BUCKETS,NUM_DATA_BUCKETS,DATA_BUCKET_MASK);

  while (*(pLangList->language)!=EOL_CHAR) 
    {
      puts("");
      printf("Language: %s\n",pLangList->language);
      printf("DB File:  %s\n",pLangList->fileForLang->filename);
      printf("Handle    %X\n",pLangList->fileForLang->hresdb);
      pLangList = pLangList->nextLang;
    }
  
  while (pDBList->nextFile) 
    {
      puts("");
      printf("DB File:  %s\n",pDBList->filename);
      printf("Handle    %X\n",pDBList->hresdb);

      if (pDBList->hresdb) {
        pNextLib = pDBList->firstLib;
        while (pNextLib->nextLib) {
          printf("        Library: %s\n",pNextLib->library);
          for ( i=0 ; i<NUM_DATA_BUCKETS ; i++ ) {
            pCache = pNextLib->firstData[i];
            while (pCache->id!=0) {
              printf("                %4d %4d %4d %s\n",
                     i,pCache->id,strlen(pCache->str),pCache->str);
              pCache = pCache->nextData;
            }
          }
          pNextLib = pNextLib->nextLib;
        }
      }

      pDBList = pDBList->nextFile;
    }
  
}

#endif /* XP_DEBUG */

/*******************************************************************************/
