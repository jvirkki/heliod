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
#include "base/params.h"
#include "base/pool_pvt.h"
#include "frame/http.h"
#include "safs/poolsafs.h"

int
service_pool_dump(pblock *param, Session *sn, Request *rq)
{
    char *query;
    char *refresh_val = NULL;
    PList_t qlist;
    ParamList *lparm;
    pool_config_t *pconfig = pool_getConfig();
    pool_global_stats_t *pstats = pool_getGlobalStats();
    pool_t *pool;
    int listLimit = 0;
    int npools;

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
        }
    }

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    if (refresh_val)
        pblock_nvinsert("refresh", refresh_val, rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);

    if (protocol_start_response(sn, rq) == REQ_NOACTION) {
        return REQ_PROCEED;
    }

    PR_fprintf(sn->csd, "<HTML>\n<HEAD>"
               "<TITLE>Memory Pool Status</TITLE></HEAD>\n<BODY>\n");
    PR_fprintf(sn->csd,
               "<CENTER><H2> "
               "Memory Pool Status</H2></CENTER>\n<PRE>\n");

    PR_fprintf(sn->csd, "<H3>Configuration Parameters</H3>\n");
    PR_fprintf(sn->csd, "Minimum allocation increment: %d bytes\n",
               pconfig->block_size);
    PR_fprintf(sn->csd,
               "Maximum size of per-pool free memory reserve: %d bytes\n",
               pconfig->retain_size);
    PR_fprintf(sn->csd,
               "Maximum blocks in per-pool free memory reserve: %d\n",
               pconfig->retain_num);

    PR_fprintf(sn->csd, "\n<H3>Global Pool Statistics</H3>\n");

    for (npools = 0, pool = pstats->poolList; pool; pool = pool->next) {
        ++npools;
    }

    PR_fprintf(sn->csd,
               "Number of pools "
               "(current/created/destroyed): %d/%d/%d\n",
               npools, pstats->createCnt, pstats->destroyCnt);
#ifdef POOL_GLOBAL_STATISTICS
    PR_fprintf(sn->csd, "Number of heap allocate/free operations: %d/%d\n",
               pstats->blkAlloc, pstats->blkFree);
#endif /* POOL_GLOBAL_STATISTICS */

#ifdef PER_POOL_STATISTICS
    if (listLimit > 0) {
        PR_fprintf(sn->csd, "\n<H3>Per-Pool Statistics</H3>\n");
    }

    pool_t *pnext;
    PRUint32 lpid = 0;
    PRUint32 npid;

    for (npools = 0; npools < listLimit; ++npools) {

        pnext = NULL;
        npid = 9999999;

        for (pool = pstats->poolList; pool; pool = pool->next) {
            if ((pool->stats.poolId < npid) &&
                (pool->stats.poolId > lpid)) {
                pnext = pool;
                npid = pool->stats.poolId;
            }
        }

        if (!pnext) {
            break;
        }

        pool = pnext;

        block_t *bptr;
        PRUint32 memUsed, memWaste;
        int nused;

        PR_fprintf(sn->csd, "Pool %d (0x%x)\n\n", npid, pool);

        if ((bptr = pool->curr_block) != NULL) {
            PR_fprintf(sn->csd,
                       "    Current block used/free memory: %d/%d bytes\n",
                       bptr->start - bptr->data,
                       bptr->end - bptr->start);
        }

        PR_fprintf(sn->csd,
                   "    Current/maximum memory allocated: %d/%d bytes\n",
                   pool->size, pool->stats.maxAlloc);

        for (bptr = pool->used_blocks, nused = 0, memUsed = 0, memWaste = 0;
             bptr; bptr = bptr->next) {
            memUsed += (bptr->start - bptr->data);
            memWaste += (bptr->end - bptr->start);
            ++nused;
        }
        PR_fprintf(sn->csd,
                   "    Used blocks/used space/fragmentation loss: "
                   "%d blocks/%d bytes/%d bytes\n",
                   nused, memUsed, memWaste);

        PR_fprintf(sn->csd,
                   "    Per pool free memory reserve: %d blocks/%d bytes\n",
                   pool->free_num, pool->free_size);
        PR_fprintf(sn->csd,
                   "    Number of local allocate/free operations: %d/%d\n",
                   pool->stats.allocCnt, pool->stats.freeCnt);
        PR_fprintf(sn->csd,
                   "    Number of global allocate/free operations: %d/%d\n",
                   pool->stats.blkAlloc, pool->stats.blkFree);
        PR_fprintf(sn->csd,
                   "    Last thread to use pool: 0x%x\n\n",
                   pool->stats.thread);

        lpid = npid;
    }
#endif /* PER_POOL_STATISTICS */
    PR_fprintf(sn->csd, "</PRE>-------\n</HTML>\n");

    return REQ_PROCEED;
}
