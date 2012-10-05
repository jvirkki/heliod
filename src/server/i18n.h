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

#ifndef I18N_H
#define I18N_H

/* Make NSAPI_PUBLIC available */
#include "netsite.h"

/*******************************************************************************/

/*
 * XP_MAX_LANGUAGE_PRIMARY_TAG_LEN is the maximum length of a primary tag in a
 * language-tag (e.g. the "en" in "en-us").
 */
#define XP_MAX_LANGUAGE_PRIMARY_TAG_LEN 8

/*
 * XP_MAX_LANGUAGE_SUB_TAG_LEN is the maximum length of a subtag in a
 * language-tag (e.g. the "us" in "en-us").
 */
#define XP_MAX_LANGUAGE_SUB_TAG_LEN 8

/*
 * XP_MAX_LANGUAGE_TAG_LEN is the maximum length of a language-tag as defined
 * by RFC 1776:
 *
 *     Language-Tag = Primary-tag *( "-" Subtag )
 *     Primary-tag = 1*8ALPHA
 *     Subtag = 1*8ALPHA
 */
#define XP_MAX_LANGUAGE_TAG_LEN (XP_MAX_LANGUAGE_PRIMARY_TAG_LEN + 1 + XP_MAX_LANGUAGE_SUB_TAG_LEN)

/*
 * XP_EXTRA_LANGUAGE_PATH_LEN is the maximum number of additional bytes that
 * XP_FormatLanguageFile() and XP_FormatLanguageDir() may require to store the
 * language-specific path corresponding to an arbitrary non-language-specific
 * path.
 */
#define XP_EXTRA_LANGUAGE_PATH_LEN (XP_MAX_LANGUAGE_TAG_LEN + 1)

/*
 * XPLanguageScope defines the scope of XP_RegisterGetLanguageRangessCallback()
 * and XP_SetLanguageRanges() operations.
 *
 * Note that it is not safe to perform XP_LANGUAGE_SCOPE_PROCESS operations
 * when multiple threads are active.
 */
typedef enum {
    XP_LANGUAGE_SCOPE_PROCESS = 0,
    XP_LANGUAGE_SCOPE_THREAD = 1
} XPLanguageScope;

/*
 * XPLanguageAudience defines the intended audience for a localized string.
 */
typedef enum {
    XP_LANGUAGE_AUDIENCE_DEFAULT = 0,
    XP_LANGUAGE_AUDIENCE_CLIENT = 1,
    XP_LANGUAGE_AUDIENCE_ADMIN = 2
} XPLanguageAudience;

/*
 * XPGetLanguageRangesFn is the prototype for user-defined callback functions
 * passed to XP_RegisterGetLanguageRangesCallback().
 */
typedef const char *(XPGetLanguageRangesFn)(void *data);

/*
 * XPLanguageEnumState preserves language enumeration state across calls to
 * XP_EnumAudienceLanguageTags().
 */
typedef struct XPLanguageEnumState XPLanguageEnumState;

/*
 * In accordance with the recommendations in the 
 * "Netscape Coding Standard for Server Internationalization",
 * the following aliases are defined for fprintf, et al., and
 * these aliases should be used to clearly indicate the intended
 * destination for output.
 */

#define AdminFprintf  fprintf
#define DebugFprintf  fprintf

#define ClientSprintf sprintf
#define AdminSprintf  sprintf
#define DebugSprintf  sprintf

#define ClientFputs   fputs
#define AdminFputs    fputs
#define DebugFputs    fputs

/*******************************************************************************/

/*
 * Function prototypes for application and libraries
 */

#ifdef __cplusplus
extern "C" {
#endif

/***************************/
/* XP_InitStringDatabase() */
/***************************/

NSAPI_PUBLIC
void
XP_InitStringDatabase(const char* pathCWD, const char* databaseName);

/**************************/
/* XP_ParseLanguageTags() */
/**************************/

/*
 * XP_ParseLanguageTags() parses 0 or more RFC 2616 language-range elements
 * into a NULL-terminated array of language-tags arranged in order of
 * descending preference.
 */

NSAPI_PUBLIC
char **
XP_ParseLanguageTags(const char *ranges);

/*************************/
/* XP_FreeLanguageTags() */
/*************************/

/*
 * XP_FreeLanguageTags() frees a NULL-terminated array of language-tags that
 * was previously allocated by XP_ParseLanguageTags().
 */

NSAPI_PUBLIC
void
XP_FreeLanguageTags(char **tags);

/**************************/
/* XP_SetLanguageRanges() */
/**************************/

/*
 * XP_SetLanguageRanges() sets the languages preferred for localized strings
 * based on 0 or more RFC 2616 language-range elements.
 */

NSAPI_PUBLIC
void
XP_SetLanguageRanges(XPLanguageScope scope,
                     XPLanguageAudience audience,
                     const char *ranges);

/**************************/
/* XP_GetLanguageRanges() */
/**************************/

/*
 * XP_GetLanguageRanges() returns either NULL or a string containing 0 or more
 * RFC 2616 language-range elements that identify the languages preferred for
 * localized strings.
 *
 * audience is XP_LANGUAGE_AUDIENCE_ADMIN to retrieve the languages preferred
 * by the administrator, XP_LANGUAGE_AUDIENCE_CLIENT to retrieve the languages
 * preferred by the client, or XP_LANGUAGE_AUDIENCE_DEFAULT to retrieve the
 * default languages.
 *
 * Note that the XP_LANGUAGE_AUDIENCE_ADMIN and XP_LANGUAGE_AUDIENCE_CLIENT
 * strings do not necessarily contain the language-range elements from the
 * XP_LANGUAGE_AUDIENCE_DEFAULT string.  As a result, the caller may need to
 * separately retrieve the XP_LANGUAGE_AUDIENCE_DEFAULT string if the
 * audience-specific strings do not contain any acceptable languages.
 */

NSAPI_PUBLIC
const char *
XP_GetLanguageRanges(XPLanguageAudience audience);

/********************/
/* XP_GetLanguage() */
/********************/

/*
 * XP_GetLanguage() calls XP_GetLanguageRanges() and returns either NULL or
 * a string that identifies the single language most preferred for localized
 * strings.
 *
 * audience is XP_LANGUAGE_AUDIENCE_ADMIN to retrieve the language preferred
 * by the administrator, XP_LANGUAGE_AUDIENCE_CLIENT to retrieve the language
 * preferred by the client, or XP_LANGUAGE_AUDIENCE_DEFAULT to retrieve the
 * default language.
 *
 * If XP_LANGUAGE_AUDIENCE_ADMIN or XP_LANGUAGE_AUDIENCE_CLIENT is specified
 * and no preferred languages have been configured for that audience, the
 * language preferred by XP_LANGUAGE_AUDIENCE_DEFAULT is returned instead.
 */

NSAPI_PUBLIC
const char *
XP_GetLanguage(XPLanguageAudience audience);

/******************/
/* XP_GetString() */
/******************/

/*
 * XP_GetString() extracts the appropriate string from the database or
 * in-memory cache for the given library name, audience, and token.
 *
 * Note: Use the macros XP_GetClientStr() and XP_GetAdminStr() defined below to
 * simplify source code.
 */

NSAPI_PUBLIC
const char *
XP_GetString(const char *library, XPLanguageAudience audience, int token);

/******************************************/
/* XP_RegisterGetLanguageRangesCallback() */
/******************************************/

/*
 * XP_RegisterGetLanguageRangesCallback() registers a callback function that
 * will identify preferred languages in response to an XP_GetLanguageRanges()
 * call.
 *
 * fn specifies the callback function.  The callback function returns either
 * NULL or a string containing 0 or more RFC 2616 language-range elements that
 * identify the languages preferred for localized strings.
 *
 * data is opaque, user-defined data that is passed to the callback function.
 *
 * When XP_GetLanguageRanges() is called, it invokes XP_LANGUAGE_SCOPE_THREAD
 * callback functions followed by XP_LANGUAGE_SCOPE_PROCESS callback
 * functions.  Callback functions are invoked in reverse order of
 * registration.  If any callback function returns a non-NULL value,
 * XP_GetLanguageRanges() immediately returns that value.  Otherwise,
 * XP_GetLanguageRanges() returns any value set by XP_SetLanguageRanges().
 */

NSAPI_PUBLIC
void
XP_RegisterGetLanguageRangesCallback(XPLanguageScope scope,
                                     XPLanguageAudience audience,
                                     XPGetLanguageRangesFn *fn,
                                     void *data);

/*********************************/
/* XP_EnumAudienceLanguageTags() */
/*********************************/

/*
 * XP_EnumAudienceLanguageTags() enumerates the languages acceptable to the
 * specified audience.  The enumeration returns tags in the following order:
 *
 * 1. language-tags preferred by the audience
 * 2. any abbreviated language-tags (i.e. primary tags) for language-tags
 *    preferred by the audience
 * 3. the default language-tags
 * 4. any abbreviated language-tags (i.e. primary tags) for the default
 *    language-tags.
 *
 * For example, if the client requested en-us;q=0.2, ja;q=0.1 and the default
 * language is fr-ca;q=1.0, XP_EnumAudienceLanguageTags() returns the
 * following values in the following order:
 *
 * 1. "en-us"
 * 2. "ja"
 * 3. "en"
 * 4. "fr-ca"
 * 5. "fr"
 * 6. NULL
 *
 * state is the address of an XPLanguageEnumState * that was set to NULL
 * before beginning the enumeration.  The enumeration continues until a) the
 * caller aborts the enumeration or b) XP_EnumAudienceLanguageTags() returns
 * NULL.
 *
 * When the enumeration has finished, XP_EndLanguageEnum() must be called to
 * free state associated with the enumeration.
 */

NSAPI_PUBLIC
const char *
XP_EnumAudienceLanguageTags(XPLanguageAudience audience,
                            XPLanguageEnumState **state);

/************************/
/* XP_EndLanguageEnum() */
/************************/

/*
 * XP_EndLanguageEnum() frees state associated with an enumeration.
 */

NSAPI_PUBLIC
void
XP_EndLanguageEnum(XPLanguageEnumState **state);

/***************************/
/* XP_FormatLanguageFile() */
/***************************/

/*
 * XP_FormatLanguageFile() constructs a language-specific path by
 * incorporating language-specific filename suffix into the specified
 * non-language-specific path.
 *
 * path and len define the non-language-specific path.  If path and len
 * specify a directory, it must end with a '/'.
 *
 * buf and size define the buffer used to construct language-specific paths.
 * The buffer must be at least (len + XP_EXTRA_LANGUAGE_PATH_LEN + 1) bytes
 * in size.
 *
 * Returns the length of the language-specific path if one was constructed or
 * -1 if it was not possible to construct a language-specific path.
 */

NSAPI_PUBLIC
int
XP_FormatLanguageFile(const char *path,
                      int len,
                      const char *tag,
                      char *buf,
                      int size);

/**************************/
/* XP_FormatLanguageDir() */
/**************************/

/*
 * XP_FormatLanguageDir() constructs a language-specific path by incorporating
 * a language-specific subdirectory into the specified non-language-specific
 * path.
 *
 * path and len define the non-language-specific path.  If path and len
 * specify a directory, it must end with a '/'.
 *
 * buf and size define the buffer used to construct language-specific paths.
 * The buffer must be at least (len + XP_EXTRA_LANGUAGE_PATH_LEN + 1) bytes
 * in size.
 *
 * Returns the length of the language-specific path if one was constructed or
 * -1 if it was not possible to construct a language-specific path.
 */

NSAPI_PUBLIC
int
XP_FormatLanguageDir(const char *path,
                     int len,
                     const char *tag,
                     char *buf,
                     int size);

#ifdef __cplusplus
}
#endif

/*******************************************************************************/

/*
 * Function prototypes for building string database
 */

extern int XP_MakeStringDatabase(void);

/* Used to create the string database at build time; not used by the application
   itself.  Returns 0 is successful. */

extern void XP_PrintStringDatabase(void);

/* DEBUG: Prints out entire string database to standard output. */

/*******************************************************************************/

/*
 * Macros to simplify calls to XP_GetString()
 */

#define XP_GetClientStr(DBTTokenName)             \
        XP_GetString(LIBRARY_NAME,                \
                     XP_LANGUAGE_AUDIENCE_CLIENT, \
                     DBTTokenName)

#define XP_GetAdminStr(DBTTokenName)             \
        XP_GetString(LIBRARY_NAME,               \
                     XP_LANGUAGE_AUDIENCE_ADMIN, \
                     DBTTokenName)

/*******************************************************************************/

/* 
 * this table contains library name
 * (stored in the first string entry, with id=0),
 * and the id/string pairs which are used by library  
 */

typedef struct RESOURCE_TABLE
{
  int id;
  char *str;
} RESOURCE_TABLE;

/*******************************************************************************/

/* 
 * resource global contains resource table list which is used
 * to generate the database.
 * Also used for "in memory" version of XP_GetString()
 */

typedef struct RESOURCE_GLOBAL
{
  RESOURCE_TABLE  *restable;
} RESOURCE_GLOBAL;

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

/*******************************************************************************/

#endif
