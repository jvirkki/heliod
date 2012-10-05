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

#include "netsite.h"

#include "drnsapi.h"
#include "drcache.h"

#include "base/pblock.h"    /* pblock_findval */
#include "base/file.h"      /* system_errmsg */
#include "base/util.h"      /* util_getline, util_is_url */
#include "frame/log.h"      /* log_error */
#include "frame/protocol.h" /* protocol_start_response */
#include "frame/httpact.h"	/* servact_handle_processed */
#include "frame/http.h"
#include "safs/nsfcsafs.h"
#include "filecache/nsfc_pvt.h"

PR_BEGIN_EXTERN_C
const char *nscperror_lookup(int error);
PR_END_EXTERN_C

NSAPI_PUBLIC PRInt32 dr_cache_init(DrHdl *hdl, RefreshFunc_t ref,
	FreeFunc_t fre, CompareFunc_t cmp,
	PRUint32 maxEntries, PRIntervalTime maxAge)
{
	*hdl = NULL;
	DrHashTable *obj = new DrHashTable(maxEntries, maxAge, cmp, ref, fre);
	*hdl = (DrHdl)obj;
	return((*hdl == (DrHdl)NULL) ? 0 : 1);
}

NSAPI_PUBLIC void dr_cache_destroy(DrHdl *hdl)
{
	if(*hdl != (DrHdl)NULL)
	{
		delete (DrHashTable *)*hdl;
		*hdl = (DrHdl)NULL;
	}
}

NSAPI_PUBLIC PRInt32 dr_net_write(DrHdl hdl, const char *key, PRUint32 klen,
	const char *hdr, const char *ftr, PRUint32 hlen, PRUint32 flen,
	PRIntervalTime timeout, PRUint32 flags, Request *rq, Session *sn)
{
	NSAPIIOVec   iov[3];
	DrHashTable *obj;
	DrHashNode  *node = 0;
	PRInt32 iovLen    = 0;
	PRInt32 cLen      = hlen + flen;

	char *pszUri = pblock_findval("uri", rq->reqpb);
	if(flags & DR_NONE)
	{
		iov[0].iov_base = (char *)hdr;
		iov[0].iov_len  = hlen;
		iov[1].iov_base = (char *)ftr;
		iov[1].iov_len  = flen;
		iovLen          = 2;
	}
	else
	if(hdl == (DrHdl)NULL)
	{
		const char *errmsg = "error code unavailable";
		log_error(LOG_WARN, "dr-net-write", sn, rq, "cache not initialized %s "
		"(%s)", pszUri, errmsg);
		return(DR_ERROR);
	}
	else
	{
		obj = (DrHashTable *)hdl;	

		if(flags & DR_FORCE)
		{
			node = obj->forceRefresh(key, klen, rq, sn);
		}
		else
		if(flags & DR_CHECK)
		{
/*The Caller does not want the Refresh To be called - if entry expired*/
/*The Caller does not want the Refresh To be called - if entry not present*/
			if(!(node = obj->tryGetEntry(key, klen, rq, sn)))
			{
				return(DR_EXPIR);
			}
		}
		else
		if(flags & DR_IGNORE)
		{
			node = obj->getEntry(key, klen, rq, sn);
		}
		else
		{
			node = obj->getUnexpiredEntry(key, klen, rq, sn);
		}
		if(!node)
		{
			const char *errmsg = "error code unavailable";
			log_error(LOG_WARN, "dr-net-write", sn, rq, "cache get error %s "
			"(%s)", pszUri, errmsg);
			return(DR_ERROR);
		}
/*We Need to lock the Node, otherwise someone may steal our Entry*/
		Entry *theEntry = node->nodeLockRead();
		if(!theEntry)
		{
			const char *errmsg = "error code unavailable";
			log_error(LOG_WARN, "dr-net-write", sn, rq, "cache entry null %s "
			"(%s)", pszUri, errmsg);

			node->nodeUnlock();

			return(DR_ERROR);
		}
		iov[0].iov_base = (char *)hdr;
		iov[0].iov_len  = hlen;
		iov[1].iov_base = theEntry->data;
		iov[1].iov_len  = theEntry->dataLen;
		iov[2].iov_base = (char *)ftr;
		iov[2].iov_len  = flen;
		iovLen          = 3;
		cLen           += theEntry->dataLen;
	}
	if(flags & (DR_CNTLEN - DR_PROTO))
	{
		param_free(pblock_remove("content-length", rq->srvhdrs));
		pblock_nninsert("content-length", cLen, rq->srvhdrs);
	}
	if(flags & DR_PROTO)
	{
		protocol_status(sn, rq, PROTOCOL_OK, NULL);
		protocol_start_response(sn, rq);
	}
	PRInt32 ret = net_writev(sn->csd, iov, iovLen);
/*If we have got the data from cache we need to unlock the node*/
	if(node)
	{
		node->nodeUnlock();
	}
	return(ret);
}
/*dr_cache_refresh
 *NSAPI Function to refresh cache in case 'ref' was passed NULL in
 *dr_cache_init, caller must call it on dr_net_write faliures with DR_EXPIR
 *INPUT   : hdl       - Persistent 'DrHdl' created through dr_cache_init
 *          key       - Key to Cache/Search/Refresh
 *         klen       - Length of Key
 *      timeout       - Timeout for expiry of this entry
 *                    - if a value of 0 is passed then it uses the expiry
 *                    - time provided as 'maxAge' in dr_cache_init
 *        entry       - the not NULL entry to be cached
 *           rq       - Pointer To the Actual 'Request'
 *           sn       - Pointer to the Actual 'Session'
 *OUTPUT  :
 *      Returns       - 0 if FAILED or 1 on SUCCESS
 */

NSAPI_PUBLIC PRInt32 dr_cache_refresh(DrHdl hdl, const char *key, PRUint32 klen,
					PRIntervalTime timeout, Entry *entry, Request *rq, Session *sn)
{
	DrHashNode  *node;
	char *pszUri = pblock_findval("uri", rq->reqpb);

	if(hdl == (DrHdl)NULL)
	{
		const char *errmsg = "error code unavailable";
		log_error(LOG_WARN, "dr-cache-refresh", sn, rq, "cache not initialized %s "
		"(%s)", pszUri, errmsg);
		return(0);
	}
	node = ((DrHashTable *)hdl)->putEntry(key, klen, timeout, entry);
	if(!node)
	{
		const char *errmsg = "error code unavailable";
		log_error(LOG_WARN, "dr-cache-refresh", sn, rq, "cache refresh failure %s"
		"(%s)", pszUri, errmsg);
		return(0);
	}
	return(1);
}

/* fc_net_write: 
 * NSAPI Function to send data to requester after constructing it with
 * Header (hdr), fileToBeInserted (fileName), Footer(ftr)
 * This function takes advantage of NSFC
 * INPUT   : fileName- desired file name 
 *           hdr     - prefix headers 
 *           ftr     - suffix trailers
 *           hlen    - length of the headers 
 *           flen    - length of the trailer 
 *           flags   - See WishList Below
 *           timeout - Timeout before this function aborts
 *           sn      - Session structure 
 *           rq      - Request structure 
 * OUTPUT  :
 *           Returns -  On Success  IO_OKAY
 *                      On Failure  IO_ERROR, FC_ERROR
 * - Get a handle to the server's file cache 
 * - Compute the total content length; 
 * - Send HTTP Header and hdr
 * - invoke NSFC_TransmitEntryFile to send the response. 
 * - Send ftr
 * If the file doesn't exist already in the cache, add it to the cache. If the 
 * file is not cacheable (for size considerations, for example),
 * then this function calls NSFC_TransmitFileNonCached
 */ 

NSAPI_PUBLIC int fc_net_write(const char *fileName, const char *hdr,
	const char *ftr, PRUint32 hlen, PRUint32 flen, PRUint32 flags,
	PRIntervalTime timeout, Session *sn, Request *rq) 
{
	int            res = IO_OKAY;
	PRStatus       statrv         = PR_SUCCESS;
	NSFCStatus     rfc;
	NSFCFileInfo   myfinfo;
	NSFCCache      nsfcCache      = GetServerFileCache();
	NSFCEntry      entry          = NSFCENTRY_INIT;
	NSFCFileInfo  *finfo          = NULL;
	PRInt32        xlen           = 0;
	int            cache_transmit = 0;
 

	char *pszUri = pblock_findval("uri", rq->reqpb);

	NSFCStatusInfo statusInfo;
	NSFCSTATUSINFO_INIT(&statusInfo);

	rfc = NSFC_AccessFilename(fileName, &entry, &myfinfo,
				nsfcCache, &statusInfo);
	if(rfc == NSFC_OK)
	{
		rfc = NSFC_GetEntryFileInfo(entry, &myfinfo, nsfcCache);
		if (rfc == NSFC_OK)
		{
			finfo = &myfinfo;
			cache_transmit = 1;
		}
		else
		{
			NSFC_ReleaseEntry(nsfcCache, &entry);
		}
	}
	if (rfc == NSFC_STATFAIL)
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		finfo = &myfinfo;
		statrv = PR_FAILURE;
	}

	if(finfo == NULL)
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		finfo = &myfinfo;
		statrv = NSFC_GetNonCacheFileInfo(fileName, finfo); 
	}
	if (statrv == PR_FAILURE)
	{
		const char *errmsg = "error code unavailable";
		char errbuf[32];
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		protocol_status(sn, rq, (rtfile_notfound() ? PROTOCOL_NOT_FOUND : 
									PROTOCOL_FORBIDDEN), NULL);
		if (finfo->prerr)
		{
			errmsg = nscperror_lookup(finfo->prerr);
		}
		if(!errmsg)
		{
			util_snprintf(errbuf, sizeof(errbuf), "NSPR %d OS %d",
											finfo->prerr, finfo->oserr);
			errmsg = errbuf;
		}
		log_error(LOG_WARN, "send-file", sn, rq, "can't find %s [%s] (%s)",
		pszUri, fileName, errmsg);
		return FC_ERROR;
	}

	if(flags & (FC_CNTLEN - FC_PROTO))
	{
		char cl[21];
		PR_snprintf(cl, sizeof(cl), "%lld", hlen + flen + finfo->pr.size);
		param_free(pblock_remove ("content-length", rq -> srvhdrs));
		pblock_nvinsert("content-length", cl, rq->srvhdrs);
	}
	if(flags & FC_PROTO)
	{
		protocol_status(sn, rq, PROTOCOL_OK, NULL);
		protocol_start_response(sn, rq);
	}

	NSFCSTATUSINFO_INIT(&statusInfo);
// Send the file along with hdr and ftr
	if(cache_transmit)
	{
		PR_ASSERT(NSFCENTRY_ISVALID(&entry));
		xlen = NSFC_TransmitEntryFile(sn->csd, entry,
								(const void *)hdr, (PRInt32)hlen,
								(const void *)ftr, (PRInt32)flen,
								PR_INTERVAL_NO_TIMEOUT,
								nsfcCache,
								&statusInfo);
	}
	else
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		xlen = NSFC_TransmitFileNonCached(sn->csd, fileName, finfo,
								(const void *)hdr, (PRInt32)hlen,
								(const void *)ftr, (PRInt32)flen,
								PR_INTERVAL_NO_TIMEOUT,
								nsfcCache,
								&statusInfo);
	}

// Check for error on the transmit
	if(xlen < 0)
	{
		PRErrorCode prerr = PR_GetError();
		if(prerr != PR_CONNECT_RESET_ERROR)
		{
			log_error(LOG_WARN,
			"send-file", sn, rq, "error sending %s [%s] (%s) status=%d:%d",
			pszUri, fileName, system_errmsg(),
			(cache_transmit ? 1:0), statusInfo);
			if(prerr == PR_FILE_IS_BUSY_ERROR ||
					statusInfo == NSFC_STATUSINFO_FILESIZE)
			{
				protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
			}
			else
			{
				protocol_status(sn, rq,
					(file_notfound() ? PROTOCOL_NOT_FOUND
					: PROTOCOL_FORBIDDEN), NULL);
			}
			res =  FC_ERROR;
		}
		else
		{
			res = IO_ERROR;
		}
// If we failed transmitting the file, we need to give up our reference
		goto donE;
	}
	if(statusInfo == NSFC_STATUSINFO_FILESIZE)
	{
		log_error(LOG_VERBOSE, "send-file", sn, rq, 
		"sending %s [%s] (File size changed on transmit). end session.",
		pszUri, fileName);
		res = FC_ERROR;
		goto donE;
	}

donE:
	if(NSFCENTRY_ISVALID(&entry))
	{
		NSFC_ReleaseEntry(nsfcCache, &entry);
	}
	return res;
}

/* fc_open: 
 * NSAPI Function to return the cached File Descriptor
 * This function takes advantage of NSFC
 * INPUT   : fileName- desired file name 
 *           hdl     - Pointer to Un-initialized Handle
 *           flags   - 0 or DUP_FILE_DESC
 *           sn      - Session structure 
 *           rq      - Request structure 
 * OUTPUT  :
 *           Returns -  On Success  Pointer to PRFileDesc
 *                      On Failure  NULL
 * - Get a handle to the server's file cache 
 * If the file Descriptor or file content doesn't exist already in the cache,
 * add it to the cache. If CopyFile is TRUE, i.e if the Descriptor in not
 * present - then open the file locally and set hdl to NULL
 */ 

NSAPI_PUBLIC PRFileDesc *fc_open(const char *fileName, FcHdl *hDl,
	PRUint32 flags, Session *sn, Request *rq)
{
	char           errbuf[32];
	PRStatus       statrv         = PR_SUCCESS;
	NSFCStatus     rfc;
    NSFCEntryImpl  *nep;
	NSFCFileInfo   myfinfo;
	NSFCFileInfo   *finfo         = NULL;
	NSFCCache      nsfcCache      = GetServerFileCache();
	NSFCEntry      entry          = NSFCENTRY_INIT;
	int            cache_transmit = 0;
	PRFileDesc     *fd            = NULL;
	const char     *errmsg        = "error code unavailable";
	char           *pszUri        = pblock_findval("uri", rq->reqpb);
	PRBool         needtocache    = PR_TRUE;
	
	hDl->reserved = NULL;
	hDl->fileSize = 0;

	NSFCStatusInfo statusInfo;
	NSFCSTATUSINFO_INIT(&statusInfo);

	rfc = NSFC_AccessFilename(fileName, &entry, &myfinfo,
				nsfcCache, &statusInfo);
    nep = entry;
	if(flags & DUP_FILE_DESC)
	{
		needtocache = PR_FALSE;
	}

	if(rfc == NSFC_OK)
	{
		rfc = NSFC_GetEntryFileInfo(entry, &myfinfo, nsfcCache);
		if (rfc == NSFC_OK)
		{
			finfo = &myfinfo;
			if(needtocache == PR_TRUE)
			{
				cache_transmit = 1;
			}
			else
			{
				NSFC_ReleaseEntry(nsfcCache, &entry);
			}
		}
		else
		{
			NSFC_ReleaseEntry(nsfcCache, &entry);
		}
	}
	if (rfc == NSFC_STATFAIL)
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		finfo = &myfinfo;
		statrv = PR_FAILURE;
	}

	if(finfo == NULL)
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		finfo = &myfinfo;
		statrv = NSFC_GetNonCacheFileInfo(fileName, finfo); 
	}
	if (statrv == PR_FAILURE)
	{
		if (finfo->prerr)
		{
			errmsg = nscperror_lookup(finfo->prerr);
		}
		if(!errmsg)
		{
			util_snprintf(errbuf, sizeof(errbuf), "NSPR %d OS %d",
											finfo->prerr, finfo->oserr);
			errmsg = errbuf;
		}
		log_error(LOG_WARN, "fc-open", sn, rq, "can't find %s [%s] (%s)",
		pszUri, fileName, errmsg);
		return NULL;
	}
	if(cache_transmit)
	{
		PR_ASSERT(NSFCENTRY_ISVALID(&entry));
		if(!(nep->flags & NSFCENTRY_OPENFD))
		{
/*The FD is not cached but either it is mmapped or its content is in heap*/
			if(nep->flags & NSFCENTRY_HASCONTENT)
			{
				;
			}
			(void)PR_AtomicIncrement((PRInt32 *)&nsfcCache->ctntMiss);
			if(nsfcCache->cfg.contentCache == PR_FALSE)
			{
				needtocache = PR_FALSE;
			}
			fd = PR_Open(nep->filename, PR_RDONLY, 0);
			if(!fd)
			{
				NSFCSTATUSINFO_SET(&statusInfo, NSFC_STATUSINFO_OPENFAIL);
				log_error(LOG_WARN,
				"fc-open", sn, rq, "error open %s [%s] (%s)",
				pszUri, fileName, system_errmsg());

				NSFC_ReleaseEntry(nsfcCache, &entry);
				return NULL;
			}
			if(needtocache == PR_TRUE)
			{
				needtocache = PR_FALSE;
/* Ensure that the limit on open fds is not exceeded */
				if(nsfcCache->cfg.maxOpen
				&& (PR_AtomicIncrement((PRInt32 *)&nsfcCache->curOpen)
				>   nsfcCache->cfg.maxOpen))
				{
/* Restore cache->curOpen */
					PR_AtomicDecrement((PRInt32 *)&nsfcCache->curOpen);
				}
				else
				{
/* Get write access to the cache entry */
					if(NSFC_AcquireEntryWriteLock(nsfcCache, nep) == NSFC_OK)
					{
/* See if open fd got cached while we acquired the lock */
						if(!(nep->flags & NSFCENTRY_OPENFD))
						{
							nep->md.fd = fd;
							nep->flags  |=
								(NSFCENTRY_HASCONTENT|NSFCENTRY_OPENFD);
						}
						hDl->reserved = (void *)nep;
						NSFC_ReleaseEntryWriteLock(nsfcCache, nep);
					}
					else
					{
/* Restore cache->curOpen */
						PR_AtomicDecrement((PRInt32 *)&nsfcCache->curOpen);
					}
				}
			}
		}
		else
		{
			fd = nep->md.fd;
			hDl->reserved = (void *)nep;
		}
		if(!hDl->reserved)
		{
			NSFC_ReleaseEntry(nsfcCache, &entry);
		}
	}
	else
	{
		PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
		fd = PR_Open(fileName, PR_RDONLY, 0);
		if(!fd)
		{
			NSFCSTATUSINFO_SET(&statusInfo, NSFC_STATUSINFO_OPENFAIL);
			log_error(LOG_WARN,
			"fc-open", sn, rq, "error open %s [%s] (%s)",
			pszUri, fileName, system_errmsg());
			return NULL;
		}
	}
	hDl->fileSize = finfo->pr.size;
	return fd;
}

/* fc_close: 
 * NSAPI Function to close the cached File Descriptor
 * This function takes advantage of NSFC
 * INPUT   : fd      - Pointer to Previously fc_open'ed PRFileDesc
 *           hdl     - Pointer to Initialized handle by fc_open
 *           sn      - Session structure 
 *           rq      - Request structure 
 * OUTPUT  :
 *           Returns -  NONE
 */ 

NSAPI_PUBLIC void fc_close(PRFileDesc *fd, FcHdl *hDl)
{
	NSFCCache      nsfcCache      = GetServerFileCache();
	NSFCEntry      entry          = NSFCENTRY_INIT;
	if(hDl->reserved)
	{
		entry = (NSFCEntry)hDl->reserved;
		if(NSFCENTRY_ISVALID(&entry))
		{
			NSFC_ReleaseEntry(nsfcCache, &entry);
		}
	}
	else
	{
		PR_Close(fd);
	}
	hDl->reserved = NULL;
}
