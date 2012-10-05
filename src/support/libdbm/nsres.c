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

#include "nsres.h"
#include <stdlib.h>

#ifdef BSDI
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <stdio.h>
#include <string.h>


enum {
    NS_DBM_VERSION_0 = 0,
    NS_DBM_VERSION_1 = 1    /* latest version */
};

#define NS_DBM_VERSION NS_DBM_VERSION_1

#define DBM_VERSION_KEY  "NS_DBM_VERSIONI2427"

struct RESDATABASE
{
    PRInt32 version;
	DB *hdb;
	NSRESTHREADINFO *threadinfo;
    PRInt32 dataOffset;
	char * pbuf[MAXBUFNUM];
} ;
typedef struct RESDATABASE *  RESHANDLE;

enum {
    DBM_SUCCESS = 0,
    DBM_FAIL
};

/*********************** Record Access begins ********************/

/**************************************************************
  Table Record Format:

      Data Type Field | Trans Field | Data Field
           32bit           32bit         Buffer

  Data Type:  UTF8, UNICODE, BINARY
  Trans Field:  Translatable or Not
  Data Field: String, Unicode or Binary data
 
***************************************************************/

/* record structure for first table format  */
typedef struct _DBRecord_0
{
    PRInt32 dataType;
    /*   data coming here  */
    char data_start;
} DBRecord_0;

/* record structure for current table format */
typedef struct _DBRecord
{
    PRInt32 dataType;
    PRInt32 trans;
    /*   data coming here  */
    char data_start;
} DBRecord;

/*  data for offset from record base */
int RecordDataOffset[2] = { 4, 8} ;


typedef	enum  {
    RESOURCE_TYPE_DEFAULT = 0,
	RESOURCE_TYPE_UTF8 = 1,
	RESOURCE_TYPE_UNICODE = 2,
	RESOURCE_TYPE_BINARY = 3
} DbDataType;

typedef	enum  {
    RESOURCE_TRANS_NO = 0,
    RESOURCE_TRANS_YES = 1
} DbTranslatable;

/***********************************************************************
 DbRecGetRecord: retrieve field data from table
  input: 
    pDbData: data block stored in table for each record
  output:
    dataType, trans, dataBuffer 
***********************************************************************/
int DbRecGetRecord(int version, DBT *pDbData, PRInt32 *dataType, 
                 PRInt32 *trans, unsigned char *dataBuffer, PRInt32 *size)
{

    if ((pDbData == NULL) || (pDbData->size < RecordDataOffset[version]))
        return DBM_SUCCESS;

    if (version == 0)
    {
        DBRecord_0 *pRec;

        pRec = (DBRecord_0 *)pDbData->data;

        if (dataType)
            memcpy(dataType, &(pRec->dataType), sizeof(PRInt32));
        if (trans)
            *trans = 0;
        if (size)
            *size = pDbData->size - RecordDataOffset[version];

        if (dataBuffer)
        {
            memcpy (dataBuffer, &(pRec->data_start), 
                pDbData->size - RecordDataOffset[version]);
        }
    }
    else if (version == 1)
    {
        DBRecord *pRec;

        pRec = (DBRecord *)pDbData->data;

        if (dataType)
            memcpy(dataType, &(pRec->dataType), sizeof(PRInt32));
        if (trans)
            memcpy(trans, &(pRec->trans), sizeof(PRInt32));
        if (size)
            *size = pDbData->size - RecordDataOffset[version];

        if (dataBuffer)
        {
            memcpy (dataBuffer, &(pRec->data_start), 
                pDbData->size - RecordDataOffset[version]);
        }
    }

    return DBM_SUCCESS;
}

/*********************************************************************
 DbRecGetDataSize: retrieve data size for current record
 Note: client should allocate memory based on the size
*********************************************************************/
int DbRecGetDataSize(int version, DBT *pDbData)
{
    if (pDbData == NULL)
        return DBM_FAIL;

    return pDbData->size - RecordDataOffset[version];
}

int DbRecSetRecord(DBT *pDbData, PRInt32 dataType, PRInt32 trans, 
                 unsigned char *dataBuffer, int dataBufferSize)
{
    DBRecord *pRec;

    if (pDbData == NULL)
        return DBM_FAIL;

    pRec = (DBRecord *) pDbData->data;

    memcpy( &(pRec->dataType), &dataType, sizeof(PRInt32));
    memcpy( &(pRec->trans), &trans, sizeof(PRInt32));

    if (dataBuffer && dataBufferSize)
    {
        char *p;
        p = &(pRec->data_start);
        memcpy (p, dataBuffer, dataBufferSize );
    }
    return DBM_SUCCESS;
}

DBT * DbRecGenRecord(int version, PRInt32 dataType, PRInt32 trans, 
                 unsigned char *dataBuffer, int dataBufferSize)
{
    DBT  *pDBT = NULL;

    if (version == 0)
    {
        DBRecord_0 *pRec;
        char *p;

        pDBT = (DBT *) malloc (sizeof(DBT));

        pDBT->size = dataBufferSize + RecordDataOffset[version];
        pDBT->data = (char *) malloc(pDBT->size);

        pRec = (DBRecord_0 *) pDBT->data;

        if (pDBT->data == NULL)
            return NULL;

        memcpy( &(pRec->dataType), &dataType, sizeof(PRInt32));

        p = &(pRec->data_start);

        memcpy(p, dataBuffer, dataBufferSize);
    }
    else if (version == 1)
    {
        DBRecord *pRec;
        char *p;

        pDBT = (DBT *) malloc (sizeof(DBT));

        pDBT->size = dataBufferSize + RecordDataOffset[version];
        pDBT->data = (char *) malloc(pDBT->size);

        pRec = (DBRecord *) pDBT->data;

        if (pDBT->data == NULL)
            return NULL;

        memcpy( &(pRec->dataType), &dataType, sizeof(PRInt32));
        memcpy( &(pRec->trans), &trans, sizeof(PRInt32));

        p = &(pRec->data_start);

        memcpy(p, dataBuffer, dataBufferSize);
    }

    return pDBT;
}

/*********************** Record Access ends  *************************/


typedef unsigned int CHARSETTYPE;
#define RES_LOCK if (hres->threadinfo) hres->threadinfo->fn_lock(hres->threadinfo->lock);
#define RES_UNLOCK if (hres->threadinfo) hres->threadinfo->fn_unlock(hres->threadinfo->lock);

#define MAX_KEY_LENGTH 48

/* 
  Right now, the page size used for resource is same as for Navigator cache
  database
 */
HASHINFO res_hash_info = {
        32*1024,
        0,
        0,
        0,
        0,   /* 64 * 1024U  */
        0};


int GenKeyData(const char *library, PRInt32 id, DBT *key)
{
    char idstr[12];
    static char strdata[MAX_KEY_LENGTH];
  
    if (id == 0) 
    {
        sprintf(idstr, "I");
    } 
    else 
    {
        sprintf(idstr, "I%d", id);
    }
  
    if (library == NULL) 
    {
        *strdata = '\0';
    } 
    else 
    {
        strcpy(strdata, library);
    }
  
    strcat(strdata, idstr);
  
    key->size = strlen(strdata)+1;
    key->data = strdata;
  
    return 1;
}

NSRESHANDLE NSResCreateTable(const char *filename, NSRESTHREADINFO *threadinfo)
{
	RESHANDLE hres;
	int flag;
    DBT key, data;

	flag = O_RDWR | O_CREAT;

	hres = (RESHANDLE) malloc ( sizeof(struct RESDATABASE) );
	memset(hres, 0, sizeof(struct RESDATABASE));

	if (threadinfo && threadinfo->lock && threadinfo->fn_lock 
	  && threadinfo->fn_unlock)
	{
		hres->threadinfo = (NSRESTHREADINFO *)
				malloc( sizeof(NSRESTHREADINFO) );
		hres->threadinfo->lock = threadinfo->lock;
		hres->threadinfo->fn_lock = threadinfo->fn_lock;
		hres->threadinfo->fn_unlock = threadinfo->fn_unlock;
	}


	RES_LOCK

	hres->hdb = dbopen(filename, flag, 0644, DB_HASH, &res_hash_info);

	RES_UNLOCK

	if(!hres->hdb)
		return NULL;

#if 1
    GenKeyData("NS_DBM_VERSION", 2427, &key);

    data.size = sizeof(PRInt32);
    data.data = (char *) malloc(data.size);
    hres->version = NS_DBM_VERSION;
    memcpy(data.data, &(hres->version), sizeof(PRInt32));
    if ( (*hres->hdb->put)(hres->hdb, &key, &data, 0) == 0)
        memcpy(&(hres->version), data.data, sizeof(PRInt32));;
#endif

	return (NSRESHANDLE) hres;
}

NSRESHANDLE NSResOpenTable(const char *filename, NSRESTHREADINFO *threadinfo)
{
	RESHANDLE hres;
	int flag;
    DBT key, data;

	flag = O_RDONLY;  /* only open database for reading */

	hres = (RESHANDLE) malloc ( sizeof(struct RESDATABASE) );
	memset(hres, 0, sizeof(struct RESDATABASE));

	if (threadinfo && threadinfo->lock && threadinfo->fn_lock 
	  && threadinfo->fn_unlock)
	{
		hres->threadinfo = (NSRESTHREADINFO *)
				malloc( sizeof(NSRESTHREADINFO) );
		hres->threadinfo->lock = threadinfo->lock;
		hres->threadinfo->fn_lock = threadinfo->fn_lock;
		hres->threadinfo->fn_unlock = threadinfo->fn_unlock;
	}


	RES_LOCK

	hres->hdb = dbopen(filename, flag, 0644, DB_HASH, &res_hash_info);

	RES_UNLOCK

	if(!hres->hdb)
		return NULL;

    GenKeyData("NS_DBM_VERSION", 2427, &key);

    hres->version = 0;
    if ( (*hres->hdb->get)(hres->hdb, &key, &data, 0) == 0)
        memcpy(&(hres->version), data.data, sizeof(PRInt32));;

    switch (hres->version) {
    case NS_DBM_VERSION_0:
    case NS_DBM_VERSION_1:
        /* Supported NS resource database */
        return (NSRESHANDLE) hres;

    default:
        /* Unknown database version or type */
        NSResCloseTable((NSRESHANDLE) hres);
        return NULL;
    }
}



void NSResCloseTable(NSRESHANDLE handle)
{
	RESHANDLE hres;
	int i;

	if (handle == NULL)
		return;
	hres = (RESHANDLE) handle;

	RES_LOCK

	(*hres->hdb->sync)(hres->hdb, 0);
	(*hres->hdb->close)(hres->hdb);

	RES_UNLOCK

	for (i = 0; i < MAXBUFNUM; i++)
	{
		if (hres->pbuf[i])
			free (hres->pbuf[i]);
	}

	if (hres->threadinfo)
		free (hres->threadinfo);

	free (hres);
}

/*********************************************************************
  Function: NSResLoadStringWithRoundMemory
  Note:  If retbuf is NULL, NSResLoadStringWithRoundMemory returns
		 temporary buffer to caller. The function maintains several 
		 temporary buffers in memory. Caller doesn't need to free the
		 return buffer. It's not thread safe!!!
 **********************************************************************/

char *NSResLoadStringWithRoundMemory(NSRESHANDLE handle, const char * library,
                               PRInt32 id, unsigned int charsetid, char *retbuf)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
    if (handle == NULL)
        return NULL;
  
    hres = (RESHANDLE) handle;
    GenKeyData(library, id, &key);
  
    RES_LOCK

    status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

    RES_UNLOCK

    if (retbuf)
    {
        DbRecGetRecord(hres->version, &data, NULL, NULL, (unsigned char *)retbuf, NULL);    
        return retbuf;
    }
    else 
    {
        static int WhichString = 0;
        static int bFirstTime = 1;
        char *szLoadedString;
        int i;
      
        RES_LOCK
        
        if (bFirstTime) {
            for (i = 0; i < MAXBUFNUM; i++)
            {
                hres->pbuf[i] = (char *) malloc(MAXSTRINGLEN * sizeof(char));
            }
            bFirstTime = 0; 
        } 
      
        szLoadedString = hres->pbuf[WhichString];
        WhichString++;
      
        /* reset to 0, if WhichString reaches to the end */
        if (WhichString == MAXBUFNUM)  
            WhichString = 0;
      
        if (status == 0)
            DbRecGetRecord(hres->version, &data, NULL, NULL, (unsigned char *)szLoadedString, NULL);    
        else
            szLoadedString[0] = 0; 
      
        RES_UNLOCK
        
        return szLoadedString;
    }
}

/*********************************************************************
  Function: NSResLoadString
  Note:  Unlike NSResLoadStringWithRoundMemory, NSResLoadString always 
		 allocates buffer and return it to caller if retbuf is NULL.
		 It's up to caller to free it after done
 **********************************************************************/
char *NSResLoadString(NSRESHANDLE handle, const char * library, PRInt32 id, 
	unsigned int charsetid, char *retbuf)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
    char *pRet;
    if (handle == NULL)
        return NULL;
  
    hres = (RESHANDLE) handle;
    GenKeyData(library, id, &key);
  
    RES_LOCK

    status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

    RES_UNLOCK

    if (retbuf)
    {
        DbRecGetRecord(hres->version, &data, NULL, NULL, (unsigned char *)retbuf, NULL);    
        return retbuf;
    }
    else 
    {
        RES_LOCK
        
        if (status == 0)
        {
            int size;
            size = DbRecGetDataSize(hres->version, &data);
            pRet = (char *) malloc (size);
            DbRecGetRecord(hres->version, &data, NULL, NULL, (unsigned char *)pRet, NULL);
        }
        else
        {
            pRet = (char *) malloc (2);
            memset(pRet, 0, 2);
        }

        RES_UNLOCK

        return pRet;
    }
}


int NSResQueryString(NSRESHANDLE handle, const char * library, PRInt32 id, 
	unsigned int charsetid, char *retbuf)
{
    int status;
    RESHANDLE hres;
    DBT key, data;

    if (handle == NULL)  return 1;
  
    hres = (RESHANDLE) handle;
    GenKeyData(library, id, &key);
  
    RES_LOCK
    
    status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);
  
    RES_UNLOCK

    return status;
}


PRInt32 NSResGetSize(NSRESHANDLE handle, const char *library, PRInt32 id)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;
	GenKeyData(library, id, &key);

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	return DbRecGetDataSize(hres->version, &data);
}

PRInt32 NSResGetInfo_key(NSRESHANDLE handle, char *keyvalue, int *size, int *charset)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

    key.size = strlen(keyvalue)+1;
    key.data = keyvalue;

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

    DbRecGetRecord(hres->version, &data, (PRInt32 *)charset, NULL, NULL, (PRInt32 *)size);
    return DBM_SUCCESS;
}

PRInt32 NSResLoadResource(NSRESHANDLE handle, const char *library, PRInt32 id, char *retbuf)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
    PRInt32 size;

	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;
	GenKeyData(library, id, &key);

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	if (retbuf)
	{
        DbRecGetRecord(hres->version, &data, NULL, NULL, (unsigned char *)retbuf, &size);
        return size;
	}
	else
		return 0;
}

PRInt32 NSResLoadResourceWithCharset_key(NSRESHANDLE handle, char *keyvalue, unsigned char *retbuf, int *charset)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

    key.size = strlen(keyvalue)+1;
    key.data = keyvalue;


	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	if (retbuf)
	{
        PRInt32 size;
        DbRecGetRecord(hres->version, &data, (PRInt32 *)charset, NULL, (unsigned char *)retbuf, &size);
        return size;
	}
	else
		return 0;
}

int NSResAddString(NSRESHANDLE handle, const char *library, PRInt32 id, 
                   const char *string, unsigned int charset)
{
    int status;
    RESHANDLE hres;
    DBT key;
    DBT *pRec;
  
    if (handle == NULL)
        return 0;
    hres = (RESHANDLE) handle;
  
    GenKeyData(library, id, &key);

    pRec = DbRecGenRecord(hres->version, charset, 0, (unsigned char *)string, strlen(string)+1);
  
    RES_LOCK

    status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free(pRec->data);
        free(pRec);
    }

    RES_UNLOCK

    return status;
}

int unistrlen(unsigned short *src)
{
    int n = 0;
    while (*src --)
        n ++ ;
    return n;
}

int NSResAppendString(NSRESHANDLE handle, NSRESRecordData *record)
{
    int status;
    RESHANDLE hres;
    DBT key;
    DBT *pRec;
    int len;
  
    if (handle == NULL || record == NULL)
        return DBM_FAIL;

    hres = (RESHANDLE) handle;
  
    GenKeyData(record->library, record->id, &key);

    if (record->dataType == RESOURCE_TYPE_UNICODE)
    {
        len = (unistrlen((unsigned short *)record->dataBuffer) + 1) * 2;

    }
    else
    {
        len = strlen(record->dataBuffer) + 1;
    }

    pRec = DbRecGenRecord(hres->version, record->dataType, record->trans, 
            (unsigned char *)(record->dataBuffer), len);
  
    RES_LOCK

    status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free(pRec->data);
        free(pRec);
    }

    RES_UNLOCK

    return status;

}

int NSResAppendRecord(NSRESHANDLE handle, NSRESRecordData *record)
{
    int status;
    RESHANDLE hres;
    DBT key;
    DBT *pRec;
    int len;
  
    if (handle == NULL || record == NULL)
        return DBM_FAIL;

    hres = (RESHANDLE) handle;
  
    GenKeyData(record->library, record->id, &key);

    if (record->dataType == RESOURCE_TYPE_UNICODE)
    {
        len = (unistrlen((unsigned short *)record->dataBuffer) + 1) * 2;

    }
    else if (record->dataType == RESOURCE_TYPE_UTF8)
    {
        len = strlen(record->dataBuffer) + 1;
    }
    else if (record->dataBufferSize > 0)
    {
        len = record->dataBufferSize;
    }
    else  /* null terminated string by default*/
    {
        len = strlen(record->dataBuffer) + 1;
    }

    pRec = DbRecGenRecord(hres->version, record->dataType, record->trans, 
            (unsigned char *)(record->dataBuffer), len);
  
    RES_LOCK

    status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free(pRec->data);
        free(pRec);
    }

    RES_UNLOCK

    return status;

}


int NSResAddResource(NSRESHANDLE handle, const char *library, PRInt32 id, 
  char *buffer, PRInt32 bufsize)
{
	int status;
	RESHANDLE hres;
	DBT key;
    DBT *pRec;

	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

	GenKeyData(library, id, &key);

    pRec = DbRecGenRecord(hres->version, RESOURCE_TYPE_BINARY, 0, (unsigned char *)buffer, bufsize);

    RES_LOCK

	status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free(pRec->data);
        free(pRec);
    }

	RES_UNLOCK

	return status;
}

int NSResAppendResource(NSRESHANDLE handle, NSRESRecordData *record)
{
	int status;
	RESHANDLE hres;
	DBT key;
    DBT *pRec;

	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

	GenKeyData(record->library, record->id, &key);

    pRec = DbRecGenRecord(hres->version, RESOURCE_TYPE_BINARY, record->trans, 
            (unsigned char *)(record->dataBuffer), record->dataBufferSize);

    RES_LOCK

	status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free(pRec->data);
        free(pRec);
    }

	RES_UNLOCK

	return status;
}

PRInt32 NSResAddResourceWithCharset_key(NSRESHANDLE handle, char *keyvalue, 
    unsigned char *buffer, PRInt32 bufsize, int charset)
{
	int status;
	RESHANDLE hres;
	DBT key;
    DBT *pRec;

	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

    key.size = strlen(keyvalue)+1;
    key.data = keyvalue;


    pRec = DbRecGenRecord(hres->version, RESOURCE_TYPE_BINARY, 0, (unsigned char *)buffer, bufsize);

	RES_LOCK

	status = (*hres->hdb->put)(hres->hdb, &key, pRec, 0);

    if (pRec && pRec->data)
    {
        free (pRec->data);
        free (pRec);
    }

	RES_UNLOCK

	return status;
}

/*
	Input:
	  handle: handle
	Output:  
	  ret:  0: sucess, other: false
      buffer:  buffer
      size:    buffer size
	  charset: charset

 */
int NSResFirstData(NSRESHANDLE handle,
                   char *keybuf,
                   char *buffer,
                   PRInt32 *size,
                   PRInt32 *charset)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
  
    if (handle == NULL)
        return 0;
    hres = (RESHANDLE) handle;
  
    if ((*hres->hdb->sync)(hres->hdb, 0) != 0)
        return 1;
  
    RES_LOCK

    status = (*hres->hdb->seq)(hres->hdb, &key, &data, R_FIRST);

    RES_UNLOCK
    if (status == 0)
    {
        /* skip version key  */
        if (memcmp(key.data, DBM_VERSION_KEY, strlen(DBM_VERSION_KEY)) == 0)  
        {
            status = (*hres->hdb->seq) (hres->hdb, &key, &data, R_NEXT);
        }

        if (status == 0)
        {
            memcpy(keybuf, (char *) key.data, key.size);
            DbRecGetRecord(hres->version, &data, charset, NULL, (unsigned char *)buffer, size);
        }
    }  
    return status;
}

/*
	Input:
	  handle: resource handle
      buffer: receive buffer
      size:   data size
	Output:  
	  ret:  0: sucess, other: false

 */
int NSResNextData(NSRESHANDLE handle,
                  char *keybuf,
                  char *buffer,
                  PRInt32 *size,
                  PRInt32 *charset)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
  
    if (handle == NULL)
        return 0;
    hres = (RESHANDLE) handle;
  
    RES_LOCK
    
    status = (*hres->hdb->seq) (hres->hdb, &key, &data, R_NEXT);
  
    RES_UNLOCK
    if (status == 0)
    {
        /* skip version key  */
        if (memcmp(key.data, DBM_VERSION_KEY, strlen(DBM_VERSION_KEY)) == 0)  
        {
            status = (*hres->hdb->seq) (hres->hdb, &key, &data, R_NEXT);
        }

        if (status == 0)
        {
            memcpy(keybuf, (char *) key.data, key.size);
	    DbRecGetRecord(hres->version, &data, charset, NULL, (unsigned char *)buffer, size);
        }
    }  

    return status;
}

int NSResLoadFirstData(NSRESHANDLE handle, NSRESRecordData *record)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
  
    if (handle == NULL)
        return 0;
    hres = (RESHANDLE) handle;
  
    if ((*hres->hdb->sync)(hres->hdb, 0) != 0)
        return 1;
  
    RES_LOCK

    status = (*hres->hdb->seq)(hres->hdb, &key, &data, R_FIRST);

    RES_UNLOCK

    DbRecGetRecord(hres->version, &data, &(record->dataType), &(record->trans), 
        (unsigned char *)(record->dataBuffer), &(record->dataBufferSize) );
  
    return status;
}

/*
	Input:
	  handle: resource handle
      buffer: receive buffer
      size:   data size
	Output:  
	  ret:  0: sucess, other: false

 */
int NSResLoadNextData(NSRESHANDLE handle, NSRESRecordData *record)
{
    int status;
    RESHANDLE hres;
    DBT key, data;
  
    if (handle == NULL)
        return 0;
    hres = (RESHANDLE) handle;
  
    RES_LOCK
    
    status = (*hres->hdb->seq) (hres->hdb, &key, &data, R_NEXT);
  
    RES_UNLOCK

    DbRecGetRecord(hres->version, &data, &(record->dataType), &(record->trans), 
        (unsigned char *)(record->dataBuffer), &(record->dataBufferSize));

    return status;
}

