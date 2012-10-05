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

/*NSAPI Function That provides Dynamic Response functionality, */
/*Pallab*/

#ifndef __DRNSAPI_H__
#define __DRNSAPI_H__


#if defined(WIN32)
#include <windows.h>
#endif

#include "nspr.h"
#include "nsapi.h"

PR_BEGIN_EXTERN_C

/*NSAPI Response Cache Object Opaque*/
typedef struct tagDrHdl
{
	void *reserved;
} *DrHdl;

/*NSAPI Response Entry (each) in Response Cache*/
typedef struct tagEntry
{
	char     *data;
	PRUint32 dataLen;
} Entry;

/*NSAPI Function That provides Dynamic Response Refresh functionality,
 *Return NULL on faliure,
 *Implemented by Application, called by cache engine
 *Implementor must fill out all the elements of the Entry
 */
typedef Entry *(*RefreshFunc_t)(const char *key, PRUint32 len,
				PRIntervalTime timeout, Request *rq, Session *sn);

/*NSAPI Function That deals with cleaning up the Entry that it creates
 *Implemented by Application, make sure it is not null
 *INPUT   : entry     - An existing entry created by RefreshFunc_t
 *OUTPUT  :
 */
typedef void (*FreeFunc_t)(Entry *entry);

/*NSAPI Key Comparator Function
 *Implemented by Application, called by cache engine
 *INPUT   : k1        - Key to Cache/Search/Refresh
 *          k2        - Second Key to compare with
 *         len1       - Length of Key k1
 *         len2       - Length of Key k2
 *OUTPUT  :
 *         Returns    - 1 if 'k1' matches 'k2' 0 on Failure
 */
typedef PRIntn (*CompareFunc_t)(const char *k1, const char *k2,
								PRUint32 len1, PRUint32 len2); 

/*NSAPI Function Called by the Application at Init*/
/*Modifies/Allocates Persistent Handle To Cache or NULL on Failure */
/*INPUT  : hdl        - pointer to unallocated "DrHdl"
 *         ref        - Function Pointer for Cache Refresh
 *                    - can be null, pl. see DR_CHECK, DR_EXPIR below
 *         fre        - Function Pointer to Free an Entry
 *         cmp        - Key Comparator Function
 *         maxEntries - Maximum Number of Entries
 *         maxAge     - Maximum age an Entry is Valid
 *                    - If 0 - then cache never expires
 *OUTPUT : hdl        - Allocated 'DrHdl' on Success NULL on Failure
 *         Returns    - 1 on Success 0 on Failure
 */

NSAPI_PUBLIC PRInt32 dr_cache_init(DrHdl *hdl, RefreshFunc_t ref,
				FreeFunc_t fre, CompareFunc_t cmp,
				PRUint32 maxEntries, PRIntervalTime maxAge);

/*NSAPI Function To Destroy Cache Entry Completly
 *This Function takes the handle to a previously initialized cache object
 *and destroys it and renders it unusable
 *INPUT   : hdl       - Persistent 'DrHdl' created through dr_cache_init
 *OUTPUT  :
 */

NSAPI_PUBLIC void dr_cache_destroy(DrHdl *hdl);

/*NSAPI Function to refresh cache in case 'ref' was passed NULL in
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
					PRIntervalTime timeout, Entry *entry, Request *rq, Session *sn);

/*NSAPI Function to send data to requester after constructing it with
 *Header (hdr), Cached Data (hdl, key), Footer(ftr)
 *By default this function will refresh the cache if expired or create a
 *cache Entry if none found with the key - unless DR_CHECK is passed in 'flags'
 *in which case the refresh must be done separately using dr_cache_refresh
 *INPUT   : hdl       - Persistent 'DrHdl' created through dr_cache_init
 *          key       - Key to Cache/Search/Refresh
 *         klen       - Length of Key
 *          hdr       - Any Header Data (can be NULL)
 *          ftr       - Any Footer Data (can be NULL)
 *         hlen       - Length of Header Data (can be 0)
 *         flen       - Length of Footer Data (can be 0)
 *      timeout       - Timeout before this function aborts
 *        flags       - Directives for this function (wishlist ORed)
 *                      See WishList Below for flags
 *           rq       - Pointer To the Actual 'Request'
 *           sn       - Pointer to the Actual 'Session'
 *OUTPUT  :
 *      Returns       - IO_OKAY or IO_ERROR
 *                    - DR_ERROR on any Cache related Error
 *                    - DR_EXPIR on Cache expiry/invalid
 */
NSAPI_PUBLIC PRInt32 dr_net_write(DrHdl hdl, const char *key, PRUint32 klen,
					const char *hdr, const char *ftr, PRUint32 hlen,
					PRUint32 flen, PRIntervalTime timeout, PRUint32 flags,
					Request *rq, Session *sn);

/*WishList that can be ORed and passed to dr_net_write*/
#define DR_NONE   (0x000001)/*No Cache to be used, works as net_write*/
                            /*'DrHdl' Can be NULL                    */
#define DR_FORCE  (0x800000)/*Force The Cache to Refresh, even if not expired*/
#define DR_CHECK  (0x080000)/*Return DR_EXPIR if expired else send it out*/
                            /*If caller has not provided a refresh function*/
                            /*and this flag is not used then returns DR_ERROR*/
#define DR_IGNORE (0x008000)/*Ignore Expiry of Cache, just use it*/
#define DR_CNTLEN (0x000880)/*Supply Content-length Header*/
                            /*Request for DR_CNTLEN will also include DR_PROTO*/
#define DR_PROTO  (0x000080)/*Do PROTOCOL_START_RESPONSE*/

#define DR_ERROR  (0x0 - 0xFFFFFF) /*Error in Handling Cache*/
#define DR_EXPIR  (0x0 - 0x080000) /*Cache Has Expired*/

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
 */ 

NSAPI_PUBLIC PRInt32 fc_net_write(
                            const char *fileName,
                            const char *hdr,
                            const char *ftr,
                            PRUint32    hlen,
                            PRUint32    flen,
                            PRUint32    flags,
                            PRIntervalTime timeout,
                            Session *sn,
                            Request *rq);
/*WishList that can be ORed and passed to fc_net_write*/
#define FC_CNTLEN (0x0880) /*Supply Content-length Header*/
                           /*Request for FC_CNTLEN will also include FC_PROTO*/
#define FC_PROTO  (0x0080) /*Do PROTOCOL_START_RESPONSE*/

#define FC_ERROR  (0x0 - 0xFFFF)

typedef struct tagFcHdl
{
	void   *reserved;
	PRSize fileSize;
} FcHdl;

/* fc_open:
 * NSAPI Function to return the cached File Descriptor
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
 * The file is only open for reading!
 */

NSAPI_PUBLIC PRFileDesc *fc_open(
							const char *fileName,
							FcHdl *hDl,
							PRUint32 flags,
							Session *sn,
							Request *rq);

#define DUP_FILE_DESC 0x0001 /*The File Descriptor Returned will be a   */
                             /*duplicate of the one stored in cache     */
							 /*This helps multiple threads to read from */
							 /*the same file without affecting currency */
/* fc_close:
 * NSAPI Function to close the cached File Descriptor
 * INPUT   : fd      - Pointer to Previously fc_open'ed PRFileDesc
 *           hdl     - Pointer to Initialized handle by fc_open
 *           sn      - Session structure
 *           rq      - Request structure
 * OUTPUT  :
 *           Returns -  NONE
 */
		  
NSAPI_PUBLIC void fc_close(PRFileDesc *fd, FcHdl *hDl);

PR_END_EXTERN_C
#endif
