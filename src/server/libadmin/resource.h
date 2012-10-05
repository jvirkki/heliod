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

#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include "netsite.h"
#include "base/pool.h"
#include "unicode/ures.h"
#include "unicode/umsg.h"
#include "unicode/ustring.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************ICU STUFF************************************/

NSAPI_PUBLIC const char* getServerLocale();

NSAPI_PUBLIC char* get_resource_file(const char* serverRoot, const char* propertiesFile);

NSAPI_PUBLIC UResourceBundle* open_resource_bundle(const char* resource_file, const char* lang);

NSAPI_PUBLIC void close_resource_bundle(UResourceBundle* resBundle);

NSAPI_PUBLIC char* get_message(UResourceBundle* myBundle,
                               const char* key);
NSAPI_PUBLIC char* pool_get_message(pool_handle_t* pool_handle,
                                    UResourceBundle* myBundle,
                                    const char* key);

/**************************************************************************/


#ifdef __cplusplus
}
#endif


/*******************************************************************************/
/* 
 * this table contains library name
 * (stored in the first string entry, with id=0),
 * and the id/string pairs which are used by library  
 */

typedef struct res_RESOURCE_TABLE
{
  int id;
  char *str;
} res_RESOURCE_TABLE;

/*******************************************************************************/

/* 
 * resource global contains resource table list which is used
 * to generate the database.
 * Also used for "in memory" version of XP_GetStringFromDatabase()
 */

typedef struct res_RESOURCE_GLOBAL
{
  res_RESOURCE_TABLE  *restable;
} res_RESOURCE_GLOBAL;

/*******************************************************************************/

/*
 * Define the ResDef macro to simplify the maintenance of strings which are to
 * be added to the library or application header file (dbtxxx.h). This enables
 * source code to refer to the strings by theit TokenNames, and allows the
 * strings to be stored in the database.
 *
 * Usage:   ResDef(TokenName,TokenValue,String)
 *
 * Example: ResDef(DBT_HelloWorld_, \
 *                 1,"Hello, World!")
 *          ResDef(DBT_TheCowJumpedOverTheMoon_, \
 *                 2,"The cow jumped over the moon.")
 *          ResDef(DBT_TheValueOfPiIsAbout31415926536_, \
 *                 3,"The value of PI is about 3.1415926536."
 *
 * RESOURCE_STR is used by makstrdb.c only.  It is not used by getstrdb.c or
 * in library or application source code.
 */
 
#ifdef  RESOURCE_STR
#define BEGIN_STR(argLibraryName) \
                          RESOURCE_TABLE argLibraryName[] = { 0, #argLibraryName,
#define ResDef(argToken,argID,argString) \
                          argID, argString,
#define END_STR(argLibraryName) \
                          0, 0 };
#else
#define BEGIN_STR(argLibraryName) \
                          enum {
#define ResDef(argToken,argID,argString) \
                          argToken = argID,
#define END_STR(argLibraryName) \
                          argLibraryName ## top };
#endif



#endif /* _RESOURCE_H_ */
