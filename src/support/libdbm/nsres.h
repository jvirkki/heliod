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

#ifndef NSRES_H
#define NSRES_H
#include "cdefs.h"
#include "mcom_db.h"

__BEGIN_DECLS

/* C version */
#define NSRESHANDLE void *

typedef void (*NSRESTHREADFUNC)(void *);

typedef struct NSRESTHREADINFO
{
	void *lock;
	NSRESTHREADFUNC fn_lock;
	NSRESTHREADFUNC fn_unlock;
} NSRESTHREADINFO;

#define MAXBUFNUM 10
#define MAXSTRINGLEN 300

#define MAX_MODULE_NAME 40

/**************************************************************
Database Record:
  Each record has its unique key
  Each record contains charset, translatable and data fields.

  Record key is generated from library and id
  Record data is from dataBuffer and dataBufferSize

  Note: when passing binary data, make sure set dataBufferSize
***************************************************************/

typedef struct _NSRESRecordData
{
    char library[MAX_MODULE_NAME];
    int id;
    PRInt32 dataType;
    char *dataBuffer;
    PRInt32 dataBufferSize;
    PRInt32 trans;
} NSRESRecordData;



/****************************************************************************
 Table Access:  Creation and Reading
*****************************************************************************/
DBMDLLEXPORT NSRESHANDLE NSResCreateTable(const char *filename, 
                             NSRESTHREADINFO *threadinfo);
DBMDLLEXPORT NSRESHANDLE NSResOpenTable(const char *filename, 
                           NSRESTHREADINFO *threadinfo);
DBMDLLEXPORT void NSResCloseTable(NSRESHANDLE handle);

/***************************************************************************
  Load Record data (String or Binary) to DBM table
***************************************************************************/
DBMDLLEXPORT PRInt32 NSResGetSize(NSRESHANDLE handle, 
                   const char *library, 
                   PRInt32 id);

DBMDLLEXPORT int NSResQueryString(NSRESHANDLE handle, 
                     const char * library, 
                     PRInt32 id,      
                     unsigned int charsetid, 
                     char *retbuf);


DBMDLLEXPORT char *NSResLoadString(NSRESHANDLE handle, 
                      const char * library, 
                      PRInt32 id,     
                      unsigned int charsetid, 
                      char *retbuf);

DBMDLLEXPORT PRInt32 NSResLoadResource(NSRESHANDLE handle, 
                        const char *library, 
                        PRInt32 id,        
                        char *retbuf);


/***************************************************************************
    Append Record data (String or Binary) to DBM table
****************************************************************************/
DBMDLLEXPORT int NSResAddString(NSRESHANDLE handle, 
                   const char *library, 
                   PRInt32 id,        
                   const char *string, 
                   unsigned int charset);
DBMDLLEXPORT int NSResAddResource(NSRESHANDLE handle, 
                     const char *library, 
                     PRInt32 id,        
                     char *buffer, 
                     PRInt32 bufsize);

/******************************************************************
  Enumeration all table records 
******************************************************************/
DBMDLLEXPORT int NSResFirstData(NSRESHANDLE handle, 
                   char *keybuf, 
                   char *buffer, 
                   PRInt32 *size,        
                   PRInt32 *charset);
DBMDLLEXPORT int NSResNextData(NSRESHANDLE handle, 
                  char *keybuf, 
                  char *buffer, 
                  PRInt32 *size,        
                  PRInt32 *charset);

/******************************************************************
  Load Data in round memory, so caller doesn't need to free it.
******************************************************************/
DBMDLLEXPORT char *NSResLoadStringWithRoundMemory(NSRESHANDLE handle,       
                                     const char * library, 
                                     PRInt32 id, 
                                     unsigned int charsetid, 
                                     char *retbuf);




/******************************************************************
  Client generates the record key value and pass to DBM
 *****************************************************************/
DBMDLLEXPORT PRInt32 NSResGetInfo_key(NSRESHANDLE handle, 
                       char *keyvalue, 
                       int *size, 
                       int *charset);
DBMDLLEXPORT PRInt32 NSResLoadResourceWithCharset_key(NSRESHANDLE handle, 
                                       char *key,
                                       unsigned char *retbuf, 
                                       int *charset);
DBMDLLEXPORT PRInt32 NSResAddResourceWithCharset_key(NSRESHANDLE handle,     
                                      char *key, 
                                      unsigned char *buffer, 
                                      PRInt32 bufsize, 
                                      int charset);


/***************************************************************************
  Record Interface. Client fills in Record data structure and pass to DBM
****************************************************************************/
DBMDLLEXPORT int NSResAppendString(NSRESHANDLE hNSRes,   
                      NSRESRecordData *record);
DBMDLLEXPORT int NSResAppendResource(NSRESHANDLE hNSRes, 
                        NSRESRecordData *record);
DBMDLLEXPORT int NSResLoadFirstData(NSRESHANDLE hNSRes,  
                       NSRESRecordData *record);
DBMDLLEXPORT int NSResLoadNextData(NSRESHANDLE hNSRes,   
                      NSRESRecordData *record);

DBMDLLEXPORT int NSResAppendRecord(NSRESHANDLE handle, 
                      NSRESRecordData *record);


__END_DECLS


#endif

