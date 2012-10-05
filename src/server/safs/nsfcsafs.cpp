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

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include "filecache/nsfc_pvt.h"
#include "netsite.h"
#include "generated/ServerXMLSchema/FileCache.h"
#include "httpdaemon/statsmanager.h"
#include "base/params.h"
#include "base/util.h"
#include "frame/conf.h"
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "frame/log.h"
#include "time/nstime.h"
#include "support/prime.h"
#include "support/stringvalue.h"
#include "safs/nsfcsafs.h"
#include "safs/dbtsafs.h"

#ifdef XP_WIN32
#include <process.h>    /* getpid() */
#endif /* XP_WIN32 */

#ifdef SOLARIS
#include <dlfcn.h>
#endif

/* Default DirmonEnable */
const PRBool NSFC_DEFAULT_DIRMONENABLE = PR_TRUE;

/* Default dirmon polling interval, in seconds (DirmonPollInterval) */
const PRUint32 NSFC_DEFAULT_DIRMONPOLLINTERVAL = 20;

/* Minimum file replacement interval, in milliseconds */
const PRInt32 NSFC_DEFAULT_REPLACEINTERVAL = 100;

/* Default TempDir suffix */
const char *NSFC_DEFAULT_TEMPDIRSUFFIX = "-file-cache";

/* Handle for NSAPI NSFC cache instance */
NSFCCache nsapi_nsfc_cache = NULL;
NSFCCacheConfig nsfc_cache_config;

static void fill_nsfc_cache_config(NSFCCacheConfig& ccfg, ServerXMLSchema::FileCache& fileCache, PRInt32 maxOpenFiles);

void * PR_CALLBACK nsfc_local_alloc(int size)
{
    return MALLOC(size);
}

void PR_CALLBACK nsfc_local_free(void *ptr)
{
    if (ptr)
        FREE(ptr);
}

NSFCCache 
GetServerFileCache()
{
  return  nsapi_nsfc_cache;
}

int
GetServerFileCacheMaxAge()
{
    if (nsfc_cache_config.maxAge == PR_INTERVAL_NO_TIMEOUT)
        return -1;

    return PR_IntervalToSeconds(nsfc_cache_config.maxAge);
}

/*
 * nsfc_cache_init - initialize NSFC cache
 */

PRStatus
nsfc_cache_init(ServerXMLSchema::FileCache& fileCache, PRInt32 maxOpenFiles)
{
    NSFCGlobalConfig gcfg;
    PRIntn vmin, vmax;
    NSFCLocalMemAllocFns *local_memfns;
    PRStatus rv;

    /* Initialize the NSFC module */
    gcfg.version = NSFC_API_VERSION;
    vmin = NSFC_API_VERSION;
    vmax = NSFC_API_VERSION;

    rv = NSFC_Initialize(&gcfg, &vmin, &vmax);
    if (rv == PR_FAILURE) {

        /* Failed to initialize NSFC module */
        log_ereport(LOG_FAILURE, XP_GetAdminStr(DBT_nsfcsafEreport3),
                    vmin, vmax);
        return rv;
    }

    if (vmax > NSFC_API_VERSION) {
        /* NSFC module is newer than what we were built with */
        log_ereport(LOG_WARN, XP_GetAdminStr(DBT_nsfcsafEreport4),
                    vmax, NSFC_API_VERSION);
    }

    /* Report minimum and maximum versions supported */
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport5),
                vmin, vmax);

    fill_nsfc_cache_config(nsfc_cache_config, fileCache, maxOpenFiles);
    local_memfns = (NSFCLocalMemAllocFns *)PERM_MALLOC(sizeof(NSFCLocalMemAllocFns));
    local_memfns->alloc = &nsfc_local_alloc;
    local_memfns->free = &nsfc_local_free;
    rv = NSFC_CreateCache(&nsfc_cache_config, NULL, NULL, 
                          local_memfns,
                          &nsapi_nsfc_cache);
    if (rv == PR_SUCCESS) {
        PR_ASSERT(nsapi_nsfc_cache);
        if (nsapi_nsfc_cache->state == NSFCCache_Uninitialized) {
            if (nsfc_cache_config.cacheEnable == PR_FALSE) {
                log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport17));
            }
            else {
                log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport18));
            }
        }
        else if (nsapi_nsfc_cache->state == NSFCCache_Active) {
            log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport19));
        }
    }
    else {
        PR_ASSERT(!nsapi_nsfc_cache);
        log_ereport(LOG_FAILURE, XP_GetAdminStr(DBT_nsfcsafEreport20));

    }
    return rv;
}

PRStatus
nsfc_cache_update_maxopen(PRInt32 maxOpenFiles)
{
    PRStatus rv;
    PRInt32 maxfiles = nsfc_cache_config.maxOpen;

    if (!nsapi_nsfc_cache)
        return PR_FAILURE;

    nsfc_cache_config.maxOpen = maxOpenFiles;
    rv = NSFC_UpdateCacheMaxOpen(&nsfc_cache_config, nsapi_nsfc_cache);
    if (rv == PR_FAILURE) {
        nsfc_cache_config.maxOpen = maxfiles;
        log_ereport(LOG_FAILURE, XP_GetAdminStr(DBT_nsfcsafEreport23));
        return rv;
    }

    return PR_SUCCESS;
}

static void 
initialize_nsapi_nsfc_cache()
{
    PRStatus rv;

    if (nsapi_nsfc_cache && 
        nsapi_nsfc_cache->state == NSFCCache_Uninitialized) {
        if (nsfc_cache_config.cacheEnable == PR_TRUE) {
            rv = NSFC_InitializeCache(&nsfc_cache_config, nsapi_nsfc_cache);
            if (rv == PR_SUCCESS) {
                log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport19));
            }
            else {
                log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_nsfcsafEreport18));
            }
        }
    }
}

static void
fill_nsfc_cache_config(NSFCCacheConfig& ccfg, ServerXMLSchema::FileCache& fileCache, PRInt32 maxOpenFiles)
{
    memset((void *)&ccfg, 0, sizeof(ccfg));
    ccfg.version = NSFC_API_VERSION;
    ccfg.cacheEnable = fileCache.enabled;
    ccfg.dirmonEnable = NSFC_DEFAULT_DIRMONENABLE;
    ccfg.contentCache = fileCache.cacheContent;
    ccfg.useSendFile = fileCache.sendfile;
    ccfg.sendfileSize = fileCache.sendfileSize;
    ccfg.maxAge = fileCache.maxAge.getPRIntervalTimeValue();
    ccfg.dirmonPoll = PR_SecondsToInterval(NSFC_DEFAULT_DIRMONPOLLINTERVAL);
    ccfg.maxFiles = fileCache.maxEntries;
    ccfg.maxHeap = fileCache.maxHeapSpace;
    ccfg.maxMmap = fileCache.maxMmapSpace;
    ccfg.maxOpen = maxOpenFiles;
    ccfg.limSmall = fileCache.maxHeapFileSize;
    ccfg.limMedium = fileCache.maxMmapFileSize;
    ccfg.replaceFiles = (fileCache.replacement != ServerXMLSchema::Replacement::REPLACEMENT_FALSE);
    ccfg.minReplace = NSFC_DEFAULT_REPLACEINTERVAL;
    ccfg.hitOrder = (fileCache.replacement == ServerXMLSchema::Replacement::REPLACEMENT_LFU);
    ccfg.bufferSize = fileCache.bufferSize;
    ccfg.copyFiles = fileCache.copyFiles;
    ccfg.tempDir = NULL;
    ccfg.instanceId = conf_get_true_globals()->Vserver_id;

    /*
     * Default temp dir is a subdirectory off of the system temp dir.
     */
    if (fileCache.getCopyPath()) {
        ccfg.tempDir = strdup(*fileCache.getCopyPath());
    } else {
        const char *system_temp_dir = system_get_temp_dir();
        ccfg.tempDir = (char *)malloc(strlen(system_temp_dir) + 1 +
                                      strlen(server_id) +
                                      strlen(NSFC_DEFAULT_TEMPDIRSUFFIX) + 1);
        strcpy(ccfg.tempDir, system_temp_dir);
        strcat(ccfg.tempDir, "/");
        strcat(ccfg.tempDir, server_id);
        strcat(ccfg.tempDir, NSFC_DEFAULT_TEMPDIRSUFFIX);
    }

    if (ccfg.hashInit == 0) {
        ccfg.hashInit = findPrime(2 * ccfg.maxFiles);
    }

    /*
     * maxHeap is only the maximum amount of heap space that can be
     * used for small files at this point.  Increase it by the size of
     * a cache entry times the maximum number of files, and also by the
     * size of the hash table.
     *
     * XXX We assume that filename string(s) per entry amount to an
     * average of 128 bytes.
     */
    ccfg.maxHeap += (ccfg.maxFiles * (128 + sizeof(NSFCEntryImpl)))
                           + (ccfg.hashInit * sizeof(NSFCEntryImpl *));

}

/*
 * nsfc_cache_list - display contents of file cache
 *
 * This corresponds to the NSAPI service-nsfc-dump Service SAF.
 */
int
nsfc_cache_list(pblock *param, Session *sn, Request *rq)
{
    PList_t qlist;
    ParamList *lparm;
    NSFCCache cip = nsapi_nsfc_cache;
    NSFCCacheConfig *ccfg;
    NSFCEntryImpl *nep;
    PRIntervalTime now;
    PRInt32 secs;
    int n;
    char *query;
    char *refresh_val = NULL;
    int listLimit = 0;
    int stop = 0;
    int start = 0;
    char *cp;
    char flags[16];

    ccfg = &nsfc_cache_config;

    /* Check for query string */
    if ((query = pblock_findval("query", rq->reqpb)) != NULL) {

        /* Parse query string */
        qlist = param_Parse(sn->pool, query, NULL);
        if (qlist) {

            /* See if client asked for automatic refresh in query string */
            lparm = param_GetElem(qlist, "refresh");
            if (lparm) {
                refresh_val = lparm->value;
            }

            /* Check for list option */
            lparm = param_GetElem(qlist, "list");
            if (lparm) {

                listLimit = lparm->value[0] ? atoi(lparm->value) : 9999999;
            }

            /* Check for cache control commands */
            start = param_GetElem(qlist, "start") ? 1 : 0;
            stop = param_GetElem(qlist, "stop") ? 1 : 0;
            if (param_GetElem(qlist, "restart")) {
                start = 1;
                stop = 1;
            }
        }
    }

    httpfilter_buffer_output(sn, rq, PR_TRUE);

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    if (refresh_val)
        pblock_nvinsert("refresh", refresh_val, rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);

    if (protocol_start_response(sn, rq) != REQ_NOACTION) {
        PR_fprintf(sn->csd, "<HTML>\n<HEAD>"
                   "<TITLE>File Cache Status</TITLE></HEAD>\n<BODY>\n");
        PR_fprintf(sn->csd,
                   "<CENTER><H2> "
                   "File Cache Status (pid %d)</H2></CENTER>\n<P>\n",
                   getpid());

        if (query) {

            if (stop) {
                if (!cip) {
                    PR_fprintf(sn->csd,
                        "<H3>The file cache has not been created.</H3>\n");
                }
                else if (cip->state == NSFCCache_Uninitialized ||
                         cip->state == NSFCCache_Initializing) {
                    PR_fprintf(sn->csd,
                        "<H3>The file cache has not been initialized.</H3>\n");
                }
                else if (cip->state == NSFCCache_Active) {
                    PR_fprintf(sn->csd,
                               "<H3>Shutting down the file cache...</H3>\n");

                    NSFC_ShutdownCache(cip, PR_TRUE);
                    log_ereport(LOG_INFORM, 
                                XP_GetAdminStr(DBT_nsfcsafEreport15));
                }
                else if (cip->state == NSFCCache_Shutdown ||
                         cip->state == NSFCCache_Terminating ||
                         cip->state == NSFCCache_TerminatingWait) {
                    PR_fprintf(sn->csd,
                               "<H3>The cache is already stopped.</H3>\n");
                }
                else {
                    PR_fprintf(sn->csd,
                               "<H3>Unexpected cache state - %d</H3>\n",
                               cip->state);
                }
            }

            if (start) {
                if (!cip) {
                    PR_fprintf(sn->csd,
                        "<H3>The file cache has not been created.</H3>\n");
                }
                else if (cip->state == NSFCCache_Uninitialized) {
                    initialize_nsapi_nsfc_cache();
                    PR_ASSERT(cip && cip == nsapi_nsfc_cache);
                    if (cip->state == NSFCCache_Active) {
                        PR_fprintf(sn->csd,
                               "<H3>The cache has been initialized</H3>\n");
                    }
                    else if (cip->cfg.cacheEnable == PR_FALSE) {
                        PR_fprintf(sn->csd,
                               "<H3>The file cache is not enabled</H3>\n");
                    }
                    else {
                        PR_fprintf(sn->csd,
                               "<H3>Unable to initialize the cache</H3>\n");
                    }
                }
                else if (cip->state == NSFCCache_Active) {
                    PR_fprintf(sn->csd,
                               "<H3>The cache is already started.</H3>\n");
                }
                else if (cip->state == NSFCCache_Shutdown) {
                    PR_fprintf(sn->csd,
                               "<H3>Starting the file cache...</H3>\n");
                    NSFC_StartupCache(cip);
                    log_ereport(LOG_INFORM, 
                                XP_GetAdminStr(DBT_nsfcsafEreport16));
                }
            }
        }

        NSFC_EnterCacheMonitor(cip);
        now = ft_timeIntervalNow();
        if (!cip || !cip->cfg.cacheEnable || 
            cip->state == NSFCCache_Uninitialized) {
            PR_fprintf(sn->csd,
                       "<P>The file cache is <B>disabled</B>.  The "
                       "file cache parameter settings shown below are "
                       "the current settings, but are not in effect.\n");
        }
        else if (cip->state != NSFCCache_Active) {
            if (cip->state == NSFCCache_Uninitialized) {
                PR_fprintf(sn->csd,
                           "<H3>The cache has not been initialized.</H3>\n");
            }
            else {
                PR_fprintf(sn->csd,
                           "<P>The file cache is <B>enabled</B>, but is "
                           "currently <B>shut down</B>.  No cache entries "
                           "will be created until it is started.\n");
            }
        }
        else {
            ccfg = &cip->cfg;

            PR_fprintf(sn->csd,
                       "<P>The file cache is <B>enabled</B>.\n");
        }

        if (cip && cip->cfg.cacheEnable) {
            PR_fprintf(sn->csd, "<H3>Cache resource utilization</H3>\n"
                       "<PRE>\n");
            PR_fprintf(sn->csd, "Number of cached file entries = %d "
                       "(%d bytes each, %d total bytes)\n",
                       cip->curFiles, sizeof(*nep),
                       cip->curFiles * sizeof(*nep));
      /*
	    PR_fprintf(sn->csd, "Total number of cache entries created = %d\n",
		       cip->createCnt); */
            PR_fprintf(sn->csd,
               "Heap space used for cache = %lld/%lld bytes\n",
                       cip->curHeap, cip->cfg.maxHeap);
#ifdef XP_UNIX
            PR_fprintf(sn->csd,
               "Mapped memory used for medium file contents = %lld/%lld bytes\n",
                       cip->curMmap, cip->cfg.maxMmap);
#endif /* XP_UNIX */
            PR_fprintf(sn->csd,
                       "Number of cached open file descriptors = %d/%d\n",
                       cip->curOpen, cip->cfg.maxOpen);
            PRFloat64 percent = 0.0;
            if (cip->lookups > 0) {
                percent = ((PRFloat64)(cip->hitcnt) * 100.)
                          / (PRFloat64)(cip->lookups);
            }
            PR_fprintf(sn->csd,
                       "Number of cache lookup hits = %d/%d (%6.2f %%)\n",
                       cip->hitcnt, cip->lookups, percent);
            PR_fprintf(sn->csd,
                   "Number of hits/misses on cached file info = %d/%d\n",
                   cip->infoHits, cip->infoMiss);
            PR_fprintf(sn->csd,
                   "Number of hits/misses on cached file content = %d/%d\n",
                   cip->ctntHits, cip->ctntMiss);
            PR_fprintf(sn->csd,
                       "Number of outdated cache entries deleted = %d\n",
                       cip->outdCnt);
            PR_fprintf(sn->csd,
                       "Number of cache entry replacements = %d\n",
                       cip->rplcCnt);
            PR_fprintf(sn->csd,
                       "Total number of cache entries deleted = %d\n",
                       cip->delCnt);
            if (cip->busydCnt > 0) {
                PR_fprintf(sn->csd,
                           "Number of busy deleted cache entries = %d\n",
                           cip->busydCnt);
            }
            PR_fprintf(sn->csd, "</PRE>\n");
        }
        PR_fprintf(sn->csd, "<H3>Parameter settings</H3>\n<PRE>\n");
        PR_fprintf(sn->csd, "ReplaceFiles: %s\n",
                   ccfg->replaceFiles ? "true" : "false");
        PR_fprintf(sn->csd, "ReplaceInterval: %d milliseconds\n",
                   PR_IntervalToMilliseconds(ccfg->minReplace));
        PR_fprintf(sn->csd, "HitOrder: %s\n",
                   ccfg->hitOrder ? "true" : "false");
#if 0
        /* XXX Not implemented */
        PR_fprintf(sn->csd, "DirmonEnable: %s\n",
                   ccfg->dirmonEnable ? "true" : "false");
#endif
        PR_fprintf(sn->csd, "CacheFileContent: %s\n",
                   ccfg->contentCache ? "true" : "false");
        PR_fprintf(sn->csd, "TransmitFile: %s\n",
                   ccfg->useSendFile ? "true" : "false");
        PR_fprintf(sn->csd, "sendfileSize: %d bytes\n",
                   ccfg->sendfileSize);
        PR_fprintf(sn->csd, "MaxAge: %d seconds\n",
                   GetServerFileCacheMaxAge());
#if 0
        /* XXX Not implemented */
        PR_fprintf(sn->csd, "DirmonPollInterval: %d seconds\n",
                   PR_IntervalToSeconds(ccfg->dirmonPoll));
#endif
        PR_fprintf(sn->csd, "MaxFiles: %d files\n", ccfg->maxFiles);
#ifdef XP_UNIX
        PR_fprintf(sn->csd, "SmallFileSizeLimit: %d bytes\n",
                   ccfg->limSmall);
        PR_fprintf(sn->csd, "MediumFileSizeLimit: %d bytes\n",
                   ccfg->limMedium);
#endif /* XP_UNIX */
        PR_fprintf(sn->csd, "MaxOpenFiles: %d files\n",
                   ccfg->maxOpen);
        PR_fprintf(sn->csd, "BufferSize: %d bytes\n",
                   ccfg->bufferSize);
        PR_fprintf(sn->csd, "\n");
        PR_fprintf(sn->csd, "CopyFiles: %s\n",
                   ccfg->copyFiles ? "true" : "false");
        PR_fprintf(sn->csd, "Directory for temporary files: %s\n",
                   ccfg->tempDir ? ccfg->tempDir : "(none)");
        PR_fprintf(sn->csd, "Hash table size: %d buckets\n",
                   ccfg->hashInit);
        PR_fprintf(sn->csd, "</PRE>\n");

        if ((listLimit > 0) && cip && cip->hname) {
            PR_fprintf(sn->csd, "<H3>Listing of file cache entries</H3>\n"
                       "<PRE>\n");
            n = 0;
            {
                PR_Lock(cip->hitLock);

                PRCList *phit;
                for (phit = PR_LIST_HEAD(&cip->hit_list);
                     phit != &cip->hit_list; phit = PR_NEXT_LINK(phit)) {

                    nep = NSFCENTRYIMPL(phit);

                    if (++n > listLimit) {
                        goto end_list;
                    }

                    cp = flags;
                    if (nep->fDelete) {
                        *cp++ = 'D';
                    }
                    if (nep->flags & NSFCENTRY_ERRORINFO) {
                        *cp++ = 'E';
                    }
                    if (nep->flags & NSFCENTRY_HASINFO) {
                        *cp++ = 'I';
                    }
                    if (nep->flags & NSFCENTRY_HASCONTENT) {
                        *cp++ = 'C';
                    }
                    if (nep->flags & NSFCENTRY_CMAPPED) {
                        *cp++ = 'M';
                    }
                    if (nep->flags & NSFCENTRY_OPENFD) {
                        *cp++ = 'O';
                    }
                    if (nep->flags & NSFCENTRY_TMPFILE) {
                        *cp++ = 'T';
                    }
                    if (nep->flags & NSFCENTRY_CREATETMPFAIL) {
                        *cp++ = 'F';
                    }
                    if (nep->fWriting) {
                        *cp++ = 'W';
                    }
                    if (nep->pdlist) {
                        *cp++ = 'P';
                    }
                    if (nep->flags & NSFCENTRY_CREATETMPFAIL) {
                        *cp++ = 'F';
                    }
                    if (cp == flags) {
                        *cp++ = '-';
                    }
                    *cp = 0;

                    secs = PR_IntervalToSeconds(now - nep->finfo.lastUpdate);

                    PR_fprintf(sn->csd,
                               "%d. %s    hits %d    flags %s    refs %d    "
                               "age %d    ", n, nep->filename, nep->hitcnt,
                               flags, nep->refcnt, secs);

                    if (nep->flags & NSFCENTRY_HASINFO) {
                        if (nep->flags & NSFCENTRY_ERRORINFO) {
                            PR_fprintf(sn->csd, "prerr %d    ");
                        }
                        else {
                            PRExplodedTime mt;
                            PR_ExplodeTime(nep->finfo.pr.modifyTime, 
                                           PR_LocalTimeParameters, &mt);
                            PR_fprintf(sn->csd, 
                                       "modified %04d%02d%02d%02d%02d%02d    "
                                       "size %lld    ",
                                       mt.tm_year, mt.tm_month+1, mt.tm_mday, 
                                       mt.tm_hour, mt.tm_min, mt.tm_sec,
                                       nep->finfo.pr.size);
                        }
                    }

                    PR_fprintf(sn->csd, "fileid [%u, %u]\n",
                               nep->finfo.fileid[0], nep->finfo.fileid[1]);
                }

                PR_Unlock(cip->hitLock);
            }

  end_list:
            PR_fprintf(sn->csd, "</PRE>\n");
        }
        NSFC_ExitCacheMonitor(cip);

	PR_fprintf(sn->csd, "------\n");
        PR_fprintf(sn->csd, "</BODY>\n</HTML>\n");
    }

    return REQ_PROCEED;
}

/*
 * nsfc_get_stats - populate a StatsCacheBucket*
 */
int
nsfc_get_stats(StatsCacheBucket* cache)
{
    NSFCCache cip = nsapi_nsfc_cache;

    NSFC_EnterCacheMonitor(cip);

    if (!cip || !cip->cfg.cacheEnable || cip->state == NSFCCache_Uninitialized) {
        // Disabled
        cache->flagEnabled = PR_FALSE;
    } else if (cip->state != NSFCCache_Active) {
        if (cip->state == NSFCCache_Uninitialized) {
            // Uninitialized
            cache->flagEnabled = PR_FALSE;
        } else {
            // Shut down
            cache->flagEnabled = PR_FALSE;
        }
    } else {
        cache->flagEnabled = PR_TRUE;
        cache->countEntries = cip->curFiles;
        cache->maxEntries = cip->cfg.maxFiles;
        cache->countOpenEntries = cip->curOpen;
        cache->maxOpenEntries = cip->cfg.maxOpen;
        cache->sizeHeapCache = cip->curHeap;
        cache->maxHeapCacheSize = cip->cfg.maxHeap;
        cache->sizeMmapCache = cip->curMmap;
        cache->maxMmapCacheSize = cip->cfg.maxMmap;
        cache->countHits = cip->hitcnt;
        cache->countMisses = cip->lookups - cip->hitcnt;
        cache->countInfoHits = cip->infoHits;
        cache->countInfoMisses = cip->infoMiss;
        cache->countContentHits = cip->ctntHits;
        cache->countContentMisses = cip->ctntMiss;
        cache->secondsMaxAge = GetServerFileCacheMaxAge();
    }

    NSFC_ExitCacheMonitor(cip);

    return REQ_PROCEED;
}
