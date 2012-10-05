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
#include <ctype.h>

#include "netsite.h"
#include "libdbm/nsres.h"
#include "i18n.h"

#define NUM_LIBRARY_BUCKETS 32 /* must be power of 2 */

typedef struct LIBRARY_LIST {
  char library[32];
  int iLibrary;
  int maxStringId;
  struct LIBRARY_LIST* nextLibrary;
} LIBRARY_LIST;

static struct LIBRARY_LIST* LibListBase[NUM_LIBRARY_BUCKETS];
static countLibrary = 0;
                        
/*******************************************************************************/

void
PrintCopyright(FILE* hTarget)
{
  fprintf(hTarget,"/****************************************"
          "**********************************/\n");
  fprintf(hTarget,"/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE"
          "                               */\n");
  fprintf(hTarget,"/* OF NETSCAPE COMMUNICATIONS CORPORATION"
          "                                 */\n");
  fprintf(hTarget,"/*                                       "
          "                                 */\n");
  fprintf(hTarget,"/* Copyright © 1996,1997 Netscape Communications"
          " Corporation.  All Rights */\n");
  fprintf(hTarget,"/* Reserved.  Use of this Source Code is subject"
          " to the terms of the      */\n");
  fprintf(hTarget,"/* applicable license agreement from Netscape"
          " Communications Corporation. */\n");
  fprintf(hTarget,"/*                                        "
          "                                */\n");
  fprintf(hTarget,"/* The copyright notice(s) in this Source Code"
          " does not indicate actual   */\n");
  fprintf(hTarget,"/* or intended publication of this Source Code."
          "                           */\n");
  fprintf(hTarget,"/*****************************************"
          "*********************************/\n");
  fprintf(hTarget,"\n");
}

/*******************************************************************************/

#define LIBRARY_HASH_MASK NUM_LIBRARY_BUCKETS-1

int
hashLibraryName(char* libraryName)
{
  /* simple hash algorithm for library name */
  
  int sum = 0;

  while (*libraryName) {
    sum += *(libraryName++);
  }
  
  sum &= LIBRARY_HASH_MASK;

  return sum;
}

/*******************************************************************************/

int
strReplace(char* target,char* from,char* to)
{
  /* replace /from/to/ in target */
  
  char* pFrom;
  char* pOldTail;
  int   lenTo;
  
  pFrom = strstr(target,from);
  if (pFrom) {
    pOldTail = pFrom+strlen(from);
    lenTo = strlen(to);
    memmove(pFrom+lenTo,pOldTail,strlen(pOldTail)+1);
    memcpy(pFrom,to,lenTo);
    return 1;
  }
  
  return 0;
}

/*******************************************************************************/

void
addEscapeCharacters(char* tString)
{
        while (strReplace(tString,"\n","N\1N"));    /* back slash */
        while (strReplace(tString,"N\1N","\\n"));
}

/*******************************************************************************/

void
addAllEscapeCharacters(char* tString)
{
        while (strReplace(tString,"\\","B\1B"));    /* back slash */
        while (strReplace(tString,"\n","N\1N"));    /* newline */
        while (strReplace(tString,"\"","Q\1Q"));    /* double quote */
        while (strReplace(tString,"B\1B","\\\\"));
        while (strReplace(tString,"N\1N","\\n"));
        while (strReplace(tString,"Q\1Q","\\\""));
}

/*******************************************************************************/

void
rmEscapeCharacters(char* tString)
{
        while (strReplace(tString,"\\n","N\1N"));   /* back slash */
        while (strReplace(tString,"N\1N","\n"));
}

/*******************************************************************************/

void
ParseKey(char* key,char* library,int* id)
{
  char* cptr;

  strcpy(library,key);

  /*
  cptr = library+strlen(library);
  while ( (--cptr>=library) && isdigit(*cptr) );
  cptr++;
  *id = 0;
  sscanf(cptr,"%d",id);
  *cptr = '\0';
  */

  cptr = strrchr(library,'I');
  if (cptr==NULL) {
    printf("Error: String Resource Database is incompatible.\n");
    printf("key (%s) must have \"I\" between library name and id.\n",key);
    exit(1);
  }
  *cptr = '\0';
  *id = 0;
  sscanf(cptr+1,"%d",id);
}

/*******************************************************************************/

int
XP_ListStringDatabase(char* dbfile)
{
  NSRESHANDLE hresdb;

  char library[32];
  int id;
  
  int status;
  char keybuf[64];
  char buffer[4096];
  PRInt32 size;
  PRInt32 charset;
  
  /* Open input database */
  hresdb = NSResOpenTable(dbfile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbfile);
    return 1;
  }
 
  status = NSResFirstData(hresdb,keybuf,buffer,&size,&charset);
  
  while (status==0) {
    ParseKey(keybuf,library,&id);
    printf("%s %d : %d %d %s\n",library,id,size,charset,buffer);
    status = NSResNextData(hresdb,keybuf,buffer,&size,&charset);
  }

  NSResCloseTable(hresdb);
  return 0;
}

/*******************************************************************************/

int
XP_CollectLibraries(char* dbfile)
{
  int i;
  int hLibrary;
  
  NSRESHANDLE hresdb;
  LIBRARY_LIST* pNextLibrary; 
  char library[32];
  int id;
  
  int status;
  char keybuf[64];
  char buffer[4096];
  PRInt32 size;
  PRInt32 charset;
  
  /* Initialize the buckets */
  if (countLibrary==0) {
    for ( i=0 ; i<NUM_LIBRARY_BUCKETS ; i++ ) {
      LibListBase[i] = (LIBRARY_LIST*)malloc(sizeof(LIBRARY_LIST));
      *LibListBase[i]->library = '\0';
      LibListBase[i]->iLibrary = -1;
      LibListBase[i]->maxStringId = 0;
      LibListBase[i]->nextLibrary = NULL;
    }
  }
  
  /* Open input database */
  hresdb = NSResOpenTable(dbfile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbfile);
    return 1;
  }
 
  status = NSResFirstData(hresdb,keybuf,buffer,&size,&charset);
  
  while (status==0) {
    ParseKey(keybuf,library,&id);
    hLibrary = hashLibraryName(library);

    pNextLibrary=LibListBase[hLibrary];
    while (pNextLibrary->nextLibrary) {
      if (strcmp(pNextLibrary->library,library)) {
        /* look further */
        pNextLibrary = pNextLibrary->nextLibrary;
      } else {
        /* found the library */
        if (pNextLibrary->maxStringId < id) {
          pNextLibrary->maxStringId = id;
        }
        break;
      }
    }
    if (pNextLibrary->nextLibrary==NULL) {
      /* add the library to the link list */
      pNextLibrary->nextLibrary = (LIBRARY_LIST*)malloc(sizeof(LIBRARY_LIST));
      strcpy(pNextLibrary->library,library);
      pNextLibrary->iLibrary = countLibrary++;
      pNextLibrary->maxStringId = id;
      (pNextLibrary->nextLibrary)->nextLibrary = NULL;
    }
    
    status = NSResNextData(hresdb,keybuf,buffer,&size,&charset);
  }

  NSResCloseTable(hresdb);
  return 0;
}

/*******************************************************************************/

int
XP_MakeHeaderFile(char* dbSourceFile,char* target)
{
  int iBucket;
  int iString;
  int count;
  char* pString;
  char* cptr;
  char  tString[4096];
  
  LIBRARY_LIST* pNextLibrary; 
  NSRESHANDLE hresdb;
  FILE* hTarget;
  
  /* Open input database */
  hresdb = NSResOpenTable(dbSourceFile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbSourceFile);
    return 1;
  }
 
  /* Open output target file */
  hTarget = fopen(target,"w");
  if (hTarget==0) {
    printf("Error opening output header file %s\n",target);
    return 1;
  }

  /* write out boiler plate at the top */
  PrintCopyright(hTarget);
  fprintf(hTarget,"/* It is intended that this header file "
          "be generated by program dblink */\n\n");
  fprintf(hTarget,"#define NUM_BUCKETS %d /* must be a power of 2 */\n",
          NUM_LIBRARY_BUCKETS);

  /* go over all the buckets, and for each bucket, go over all the libraries */
  printf("Total:  %4d Buckets\n",NUM_LIBRARY_BUCKETS);
  for ( iBucket=0 ; iBucket<NUM_LIBRARY_BUCKETS ; iBucket++ ) {
    pNextLibrary = LibListBase[iBucket];
    count = 0;
    while (pNextLibrary->nextLibrary) {
#if 0
      printf("                    Library: %-12s   "
             "iLibrary: %4d   MaxStringId: %4d\n",
             pNextLibrary->library,
             pNextLibrary->iLibrary,pNextLibrary->maxStringId);
#endif
      count++;

      /* put all stings in the library into the header file */
      fprintf(hTarget,"\n/* strings in library %s */\n",pNextLibrary->library);

      pString = NSResLoadString(hresdb,pNextLibrary->library,-1,0,NULL);
      if (strstr(pString,"$DBT: ")) {
        cptr = strstr(pString,"in DB file");
        if (cptr) {
          strncpy(cptr,"in memory ",10);
        }
        fprintf(hTarget,"static const char* %sid[] = {\"%s\"};\n",
                pNextLibrary->library,pString);
      }

      fprintf(hTarget,"static const char* %s[] = {\n",pNextLibrary->library);
      for ( iString=0 ; iString<=pNextLibrary->maxStringId ; iString++ ) {
        pString = NSResLoadString(hresdb,pNextLibrary->library,iString,0,NULL);
        strcpy(tString,pString);
        /* deal with special cases needing escape character */
        addAllEscapeCharacters(tString);
        fprintf(hTarget,"  \"%s\",\n",tString);
      }
      fprintf(hTarget,"  emptyString };\n");

      pNextLibrary = pNextLibrary->nextLibrary;
    }

    /* put list of all files in this bucket into header file */
    fprintf(hTarget,"\n/* libraries in bucket for hashKey==%d */\n",iBucket);
    fprintf(hTarget,"static struct DATABIN bucket%d[] = {\n",iBucket);
    pNextLibrary = LibListBase[iBucket];
    while (pNextLibrary->nextLibrary) {
      fprintf(hTarget,"  {\"%s\",%s,%d},\n",
              pNextLibrary->library,
              pNextLibrary->library,
              pNextLibrary->maxStringId);
      pNextLibrary = pNextLibrary->nextLibrary;
    }
    fprintf(hTarget,"  {emptyString,NULL,0} };\n");

    if (count) {
      printf("Bucket: %4d   SubBuckets: %4d\n",iBucket,count);
    }
  }
  printf("Number of Libraries:       %4d\n",countLibrary);

  NSResCloseTable(hresdb);

  /* put list of all buckets into header file */
    fprintf(hTarget,"\n/* array of bucket pointers */\n");
    fprintf(hTarget,"static struct DATABIN* buckets[%d] = {\n",
            NUM_LIBRARY_BUCKETS);
    for ( iBucket=0 ; iBucket<NUM_LIBRARY_BUCKETS-1 ; iBucket++ ) {
      fprintf(hTarget,"  bucket%d,\n",iBucket);
    }
    fprintf(hTarget,"  bucket%d };\n",iBucket);

    fclose(hTarget);

    return 0;
}

/*******************************************************************************/

int
XP_MakeTextFiles(char* language,char* dbSourceFile)
{
  int iBucket;
  int iString;
  int rc;
  char* pString;
  char  tString[4096];
  char  target[256];

  LIBRARY_LIST* pNextLibrary; 
  NSRESHANDLE hresdb;
  FILE* hTarget;
  
  /* Open input database */
  hresdb = NSResOpenTable(dbSourceFile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbSourceFile);
    return 1;
  }
 
  /* go over all the buckets, and for each bucket, go over all the libraries */
  for ( iBucket=0 ; iBucket<NUM_LIBRARY_BUCKETS ; iBucket++ ) {
    pNextLibrary = LibListBase[iBucket];
    while (pNextLibrary->nextLibrary) {
      
      /* Open output target file */
      strcpy(target,pNextLibrary->library);
      strcat(target,".txt");
      hTarget = fopen(target,"w");
      if (hTarget==0) {
        printf("Error opening output header file %s\n",target);
        return 1;
      }
      
      /* write out boiler plate at the top */
      PrintCopyright(hTarget);

      fprintf(hTarget,"/* This text file was generated by program dblink */"
              "\n\n");
      fprintf(hTarget,"$StartStringData$\n\n");

      /* put all stings in the library into the text file */
      printf("strings in library %s to %s\n",pNextLibrary->library,target);
      
      for ( iString=-1 ; iString<=pNextLibrary->maxStringId ; iString++ ) {
        rc = NSResQueryString(hresdb,pNextLibrary->library,iString,0,NULL);
        if (rc) continue; /* skip non-existent strings */
        pString = NSResLoadString(hresdb,pNextLibrary->library,iString,0,NULL);
        strcpy(tString,pString);
        /* deal with special cases needing escape character */
        addEscapeCharacters(tString);
        fprintf(hTarget,"%s,%s,%d,[%s]\n",
                language,pNextLibrary->library,iString,tString);
      }
      
      fclose(hTarget);
      pNextLibrary = pNextLibrary->nextLibrary;
    }
  }
  printf("Number of Libraries:       %4d\n",countLibrary);

  NSResCloseTable(hresdb);

  return 0;
}

/*******************************************************************************/

int
XP_MakeOneTextFile(char* language,char* dbSourceFile,char* target)
{
  int iBucket;
  int iString;
  int rc;
  int count;
  int count2;
  char* pString;
  char  tString[4096];

  LIBRARY_LIST* pNextLibrary; 
  NSRESHANDLE hresdb;
  FILE* hTarget;
  
  struct LIBRARY_LIST** LibListPtr;
  struct LIBRARY_LIST* LibListTmp;

  LibListPtr = (struct LIBRARY_LIST**)malloc(countLibrary*sizeof(struct LIBRARY_LIST*));

  /* Open input database */
  hresdb = NSResOpenTable(dbSourceFile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbSourceFile);
    return 1;
  }
 
  /* Open output target file */
  hTarget = fopen(target,"w");
  if (hTarget==0) {
    printf("Error opening output header file %s\n",target);
    return 1;
  }
  
  /* write out boiler plate at the top */
  PrintCopyright(hTarget);
  
  fprintf(hTarget,"/* This text file was generated by program dblink */"
          "\n\n");
  fprintf(hTarget,"$StartStringData$\n\n");

  /* go over all the buckets, and for each bucket, get all library names */
  count = 0;
  for ( iBucket=0 ; iBucket<NUM_LIBRARY_BUCKETS ; iBucket++ ) {
    pNextLibrary = LibListBase[iBucket];
    while (pNextLibrary->nextLibrary) {
      LibListPtr[count++] = pNextLibrary;
      pNextLibrary = pNextLibrary->nextLibrary;
    }
  }

  /* put libraries in order */
  for ( count = 0 ; count<countLibrary ; count++ ) {
    for ( count2 = count+1 ; count2<countLibrary ;count2++ ) {
      if (strcmp(LibListPtr[count]->library,LibListPtr[count2]->library)>0) {
        LibListTmp = LibListPtr[count];
        LibListPtr[count] = LibListPtr[count2];
        LibListPtr[count2] = LibListTmp;
      }
    }
  }

  /* go over all the libraries */
  for ( count = 0 ; count<countLibrary ; count++ ) {
    /* put all stings in the library into the text file */
    printf("strings in library %s to %s\n",
           LibListPtr[count]->library,target);
    
    for ( iString=-1 ;
          iString<=LibListPtr[count]->maxStringId ;
          iString++ ) {
      rc = NSResQueryString(hresdb,LibListPtr[count]->library,
                            iString,0,NULL);
      if (rc) continue; /* skip non-existent strings */
      pString = NSResLoadString(hresdb,LibListPtr[count]->library,
                                iString,0,NULL);
      strcpy(tString,pString);
      /* deal with special cases needing escape character */
      addEscapeCharacters(tString);
      fprintf(hTarget,"%s,%s,%d,[%s]\n",
              language,LibListPtr[count]->library,iString,tString);
    }
  }
  
  fclose(hTarget);
  printf("Number of Libraries:       %4d\n",countLibrary);

  NSResCloseTable(hresdb);

  free(LibListPtr);
  return 0;
}

/*******************************************************************************/

int
XP_MergeStringDatabase(char* dbSourceFile,
                       NSRESHANDLE htarget,
                       char* pFrom,
                       char *pTo)
{
  NSRESHANDLE hresdb;

  char library[32];
  int id;
  int rc;

  int status;
  char keybuf[64];
  char buffer[4096];
  PRInt32 size;
  PRInt32 charset;
  
  /* Open input database */
  hresdb = NSResOpenTable(dbSourceFile, NULL);
  if (hresdb==0) {
    printf("Error opening database %s\n",dbSourceFile);
    return 1;
  }
 
  status = NSResFirstData(hresdb,keybuf,buffer,&size,&charset);
  
  while (status==0) {
    ParseKey(keybuf,library,&id);
    rc = NSResQueryString(htarget,library,id,0,NULL);

    switch (rc) {
    case 0:
      /* library/id key already exists in target database */
      /* do not add */
      break;
    case 1:
      /* library/id key does not exist in target database */
      /* add to database*/
      if (pFrom) {
        strReplace(buffer,pFrom,pTo);
      }
      NSResAddString(htarget,library,id,buffer,0);
      break;
    default:
      printf("Error: unexpected return code (status) from NSResQueryString():"
             " %d\n",rc);
      printf("Library: %s\n",library);
      printf("ID:      %d\n",id);
      printf("Size:    %d\n",size);
      printf("CharSet: %d\n",charset);
      printf("String:  %s\n",buffer);
      return 1;
      break;
    }

    status = NSResNextData(hresdb,keybuf,buffer,&size,&charset);
  }

  NSResCloseTable(hresdb);
  return 0;
}

/*******************************************************************************/

int
XP_MergeTextFiles(char* txtSourceFile,
                  NSRESHANDLE htarget,
                  char* pFrom,
                  char *pTo)
{
  static char languageCheck[32] = "";

  FILE* hText;

  char library[32];
  int id;
  int rc;

  char buffer[4096];
  char language[32];
  char* cptr;
  char* cptrEnd;
  char* cRC;
  
  /* Open input database */
  hText = fopen(txtSourceFile, "r");
  if (hText==NULL) {
    printf("Error opening text file %s\n",txtSourceFile);
    return 1;
  }
  
  while (cRC = fgets(buffer,4096,hText)) {
    /* skip all text before $StartStringData$ */
    if (strncmp(buffer,"$StartStringData$",17)==0) break;
  }
  
  if (cRC==NULL) {
    printf("No string data found in text file %s\n",txtSourceFile);
    exit(1);
  }

  while (cRC = fgets(buffer,4096,hText)) {
    if ((cptr=strchr(buffer,'['))==NULL)  continue;     /* skip blank lines */
    cptr++;                                             /* remove ] */
    if ((cptrEnd=strrchr(buffer,']'))==NULL) {
      printf("Error in text file: No ] found at end of string.\n");
      printf("%s\n",buffer);
      exit(1);
    }
    *cptrEnd = '\0';                                    /* remove ] */

    strcpy(language,strtok(buffer,","));
    strcpy(library,strtok(NULL,","));
    sscanf(strtok(NULL,","),"%d",&id);

    /* check language consistency */ 
    if (languageCheck[0]=='\0') {
      strcpy(languageCheck,language);
      printf("Text file(s) for language: %s\n",language);
    } else if (strcmp(languageCheck,language)) {
        printf("Error: inconsistent languages in text file(s)\n");
        printf("Was %s, now changed to %s\n",languageCheck,language);
        printf("%s,%s,%d,%s\n",language,library,id,buffer);
        exit(1);
      }
    rc = NSResQueryString(htarget,library,id,0,NULL);
    switch (rc) {
    case 0:
      /* library/id key already exists in target database */
      /* do not add */
      break;
    case 1:
      /* library/id key does not exist in target database */
      /* add to database*/
      /* deal with special cases needing removal of escape character */
      rmEscapeCharacters(cptr);
      if (pFrom) {
        strReplace(cptr,pFrom,pTo);
      }
      NSResAddString(htarget,library,id,cptr,0);
      break;
    default:
      printf("Error: unexpected return code (status) from NSResQueryString():"
             " %d\n",rc);
      printf("Library: %s\n",library);
      printf("ID:      %d\n",id);
      printf("String:  %s\n",cptr);
      return 1;
      break;
    }
  }

  fclose(hText);
  return 0;
}

/*******************************************************************************/

int main(int argc,char* argv[])
{
  NSRESHANDLE htarget;

  int rc;
  int source;
  int target = argc-1;
  
  char* targetFilename;
  char* pDot;
  char* pSlash;

  char* pFrom;
  char* pTo;
  
  /*****************************************************************/
  /* No arguments: Help Syntax                                     */
  /*****************************************************************/
  if (argc==1) {

    puts("Create list of strings in databases.");
    puts("Syntax: dblink -l dbsource1 [dbsource2 ...]");
    puts("");
    puts("Create header for in-memory string model.");
    puts("Syntax: dblink -h dbsource1 [dbsource2 ...] target");
    puts("");
    puts("Create separate text file for each library in database.");
    puts("Syntax: dblink -septxt language dbsource");
    puts("");
    puts("Create one text file for database.");
    puts("Syntax: dblink -txt language dbsource target");
    puts("");
    puts("Merge text files on command line into target database");
    puts("Syntax: dblink -db txtsource1 [txtsource2 ...] target");
    puts("");
    puts("Merge databases on command line into target database");
    puts("Syntax: dblink dbsource1 [dbsource2 ...] target");
    puts("");
    puts("Merge databases on command line into target database"
         " with substitution");
    puts("Syntax: dblink"
         " [-s From To] dbsource1 [[-s From To] dbsource2 ...] target");

    return 0;
  }

  /*****************************************************************/
  /* -l : if request was for list, print out contents of databases */
  /*****************************************************************/
  if (argc>1 && strcmp(argv[1],"-l")==0) {
    for ( source=2 ; source<argc ; source++ ){
      printf("\nsource %d: %s\n",source-1,argv[source]);
      rc = XP_ListStringDatabase(argv[source]);
      if (rc) return rc;
    }
    return 0;
  }

  /*****************************************************************/
  /* -h : if request was for header file, create getstrmem.h       */
  /*****************************************************************/
  if (argc>1 && strcmp(argv[1],"-h")==0) {
    puts("Create header for in-memory string model.");
    puts("Get strings from String Resource Database Files (*.db)");
    printf("target %s\n",argv[target]);

    if (argc<4) {
      puts("Error:  dblink -h needs at least 2 filenames as arguments");
      puts("Syntax: dblink -h dbsource1 [dbsource2 ...] target");
      return 1;
    }

    for ( source=2 ; source<target ; source++ ){
      printf("source %d: %s\n",source-1,argv[source]);
      rc = XP_CollectLibraries(argv[source]);
      if (rc) return rc;
    }

    if (argc>4) {
      puts("Sorry, this implementation doesn't support more than 1 source.");
      puts("Use dblink in merge mode to combine source database files to 1.");
      return 1;
    }
    
    rc = XP_MakeHeaderFile(argv[2],argv[target]);
    return rc;
  }

  /*****************************************************************/
  /* -septxt : create separate text file for each library               */
  /*****************************************************************/
  if (argc>1 && strcmp(argv[1],"-septxt")==0) {
    puts("Create text file for each library.");
    puts("Get strings from String Resource Database Files (*.db)");

    if (argc<4) {
      puts("Error:  dblink -septxt needs at least 1 filename as argument");
      puts("Syntax: dblink -septxt language dbsource1 [dbsource2 ...]");
      return 1;
    }

    for ( source=3 ; source<argc ; source++ ){
      printf("source %d: %s\n",source-2,argv[source]);
      rc = XP_CollectLibraries(argv[source]);
      if (rc) return rc;
    }

    if (argc>4) {
      puts("Sorry, this implementation doesn't support more than 1 source.");
      puts("Use dblink in merge mode to combine source databas files to 1.");
      return 1;
    }
    
    rc = XP_MakeTextFiles(argv[2],argv[3]);
    return rc;
  }

  /*****************************************************************/
  /* -txt : create one text file for database                        */
  /*****************************************************************/
  if (argc>1 && strcmp(argv[1],"-txt")==0) {
    puts("Create one text file for database.");
    puts("Get strings from String Resource Database Files (*.db)");

    if (argc<5) {
      puts("Error:  dblink -txt needs language and "
           "at least 2 filenames as arguments");
      puts("Syntax: dblink -txt language dbsource1 [dbsource2 ...] target");
      return 1;
    }

    for ( source=3 ; source<target ; source++ ){
      printf("source %d: %s\n",source-2,argv[source]);
      rc = XP_CollectLibraries(argv[source]);
      if (rc) return rc;
    }

    if (argc>5) {
      puts("Sorry, this implementation doesn't support more than 1 source.");
      puts("Use dblink in merge mode to combine source databas files to 1.");
      return 1;
    }
    
    rc = XP_MakeOneTextFile(argv[2],argv[3],argv[target]);
    return rc;
  }

  /*****************************************************************/
  /* -db: Merge text files on command line into target database    */
  /*****************************************************************/
  if (argc>1 && strcmp(argv[1],"-db")==0) {
    if (argc<4) {
      printf("Error:  dblink -db needs at least 3 filenames as arguments\n");
      printf("Syntax: dblink -db txtsource1 [txtsource2 ...] target\n");
      printf("    or: dblink -db"
             "[-s From To] txtsource1 [[-s From To] txtsource2 ...] target\n");
      return 1;
    }
    
    printf("Merge text files on command line into target database\n");
    printf("target %s\n",argv[target]);
    
    /* Create target database */
    targetFilename = (char*)malloc(strlen(argv[target])+4); /* room for .RPL */
    /* change .db to .RPL or append .RPL */
    strcpy(targetFilename,argv[target]);
    pDot = strrchr(targetFilename,'.');
    pSlash = strrchr(targetFilename,'/');
    if (pDot>pSlash) {
      *pDot='\0';
    }
    strcat(targetFilename,".RPL");
    
    htarget = NSResCreateTable(targetFilename, NULL);
    if (htarget==0) {
      printf("Error creating target database %s\n",targetFilename);
      return 1;
    }
    
    pFrom = NULL;
    pTo = NULL;
    
    for ( source=2 ; source<target ; source++ ){
      if (strcmp(argv[source],"-s")==0)
        {
          pFrom = argv[++source];
          pTo = argv[++source];
          if (++source>=target){
            printf("Syntax: dblink "
                   "[-s From To] txtsource1 [txtsource2 ...] target\n");
            printf("Error: -s must be followed by From To txtsource\n");
            return 1;
          }
        }
      printf("source %d: %s\n",source-1,argv[source]);
      rc = XP_MergeTextFiles(argv[source],htarget,pFrom,pTo);
      if (rc) return rc;
    }
    
    NSResCloseTable(htarget);
    unlink(argv[target]);
    rc = rename(targetFilename,argv[target]);
    if (rc) {
      perror("Error renaming target file");
      return 1;
    }
    
    return 0;
  }

  /*****************************************************************/
  /* Default: merge function                                       */
  /*****************************************************************/
  if (argc<3) {
    printf("Error:  dblink needs at least 2 filenames as arguments\n");
    printf("Syntax: dblink dbsource1 [dbsource2 ...] target\n");
    printf("    or: dblink "
           "[-s From To] dbsource1 [[-s From To] dbsource2 ...] target\n");
    return 1;
  }

  printf("Merge databases on command line into target database\n");
  printf("target %s\n",argv[target]);

  /* Create target database */
  targetFilename = (char*)malloc(strlen(argv[target])+4); /* room for .RPL */
  /* change .db to .RPL or append .RPL */
  strcpy(targetFilename,argv[target]);
  pDot = strrchr(targetFilename,'.');
  pSlash = strrchr(targetFilename,'/');
  if (pDot>pSlash) {
    *pDot='\0';
  }
  strcat(targetFilename,".RPL");
  
  htarget = NSResCreateTable(targetFilename, NULL);
  if (htarget==0) {
    printf("Error creating target database %s\n",targetFilename);
    return 1;
  }
  
  pFrom = NULL;
  pTo = NULL;
  
  for ( source=1 ; source<target ; source++ ){
    if (strcmp(argv[source],"-s")==0)
        {
          pFrom = argv[++source];
          pTo = argv[++source];
          if (++source>=target){
            printf("Syntax: dblink "
                   "[-s From To] dbsource1 [dbsource2 ...] target\n");
            printf("Error: -s must be followed by From To dbsource\n");
            return 1;
          }
        }
    printf("source %d: %s\n",source,argv[source]);
    rc = XP_MergeStringDatabase(argv[source],htarget,pFrom,pTo);
    if (rc) return rc;
  }
  
  NSResCloseTable(htarget);
  rc = rename(targetFilename,argv[target]);
  if (rc) {
    perror("Error renaming target file");
    return 1;
  }

  return 0;
}

/*******************************************************************************/
