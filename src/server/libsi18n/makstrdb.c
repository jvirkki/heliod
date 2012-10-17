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

#include "libdbm/mcom_db.h"
#include "libdbm/nsres.h"

#define RESOURCE_STR

/********************************************/
/* Begin: Application dependent information */
/********************************************/

#include "gshttpd.h"
#define GSXXX_H_INCLUDED

/********************************************/
/*  End: Application dependent information  */
/********************************************/

/**********************************************/
/*  Begin: Check that BUILD_MODULE is handled */
/*         and a gs*.h file has been included */
/**********************************************/

#ifndef GSXXX_H_INCLUDED
#error Error in makstrdb.c: BUILD_MODULE not handled; gs*.h not included.
#endif

/********************************************/
/*  End: Check that BUILD_MODULE is handled */
/*       and a gs*.h file has been included */
/********************************************/

/*******************************************************************************/

#ifdef XP_DEBUG

void
XP_PrintStringDatabase(void)  /* debug routine */
{
  int i;
  int j;
  char* LibraryName;
  RESOURCE_TABLE* table;
  
  j = 0;
  while (table=allxpstr[j++].restable) {
    LibraryName = table->str;
    printf("Library %d: %s\n",j,LibraryName);
    i = 1;
    table++;
    while (table->str) {
      printf("%d: %s      %d      \"%s\"\n",i,LibraryName,table->id,table->str);
      i++;
      table++;
    }
  }
}

#endif /* XP_DEBUG */

/*******************************************************************************/

int
XP_MakeStringDatabase(void)
{
  int j;
  char* LibraryName;
  char* cptr;
  RESOURCE_TABLE* table;
  NSRESHANDLE hresdb;
  char DBTlibraryName[128];

  if (strlen(DATABASE_NAME) < 5) {
    printf("Error creating database %s, name not defined\n",DATABASE_NAME);
    return 1;
  }

  /* Creating database */
  hresdb = NSResCreateTable(DATABASE_NAME, NULL);
  if (hresdb==0) {
    printf("Error creating database %s\n",DATABASE_NAME);
    return 1;
  }

  j = 0;
  while (table=allxpstr[j++].restable) {
    LibraryName = table->str;
    printf("Add Library %d: %s\n",j,LibraryName);
    table++;
    while (table->str) {
      if (table->id==-1 && strstr(table->str,"$DBT: ")) {
        cptr = strstr(table->str,"referenced");
        if (cptr) {
          strncpy(cptr,"in DB file",10);
        }
      }
      NSResAddString(hresdb,LibraryName,table->id,table->str,0);
      table++;
    }
  }
  
  NSResCloseTable(hresdb);
  return 0;
}

/*******************************************************************************/

int main()
{
    return XP_MakeStringDatabase();
}

/*******************************************************************************/
