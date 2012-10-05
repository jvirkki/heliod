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
 * logsafs.cpp: Logging SAFs
 *
 * Chris Elving
 */

#include "netsite.h"
#include "support/NSString.h"
#include "frame/log.h"
#include "safs/var.h"
#include "safs/dbtsafs.h"
#include "safs/logsafs.h"


/* ----------------------------- logsafs_log ------------------------------ */

int logsafs_log(pblock *pb, Session *sn, Request *rq)
{
    const char *message = pblock_findkeyval(pb_key_message, pb);
    if (!message) {
        log_error(LOG_MISCONFIG, "log", sn, rq, XP_GetAdminStr(DBT_NeedMessage));
        return REQ_ABORTED;
    }

    int degree = LOG_INFORM;
    const char *level = pblock_findkeyval(pb_key_level, pb);
    if (level)
        degree = ereport_level2degree(level, degree);

    if (ereport_can_log(degree)) {
        NSString pblocks;

        // Handle pblock="..." parameters
        for (int i = 0; i < pb->hsize; i++) {
            for (struct pb_entry *p = pb->ht[i]; p; p = p->next) {
                if (param_key(p->param) == pb_key_pblock) {
                    pblocks.append(" ");
                    pblocks.append(p->param->value);
                    pblocks.append(": ");
                    pblock *knownpb = var_get_pblock(sn, rq, p->param->value);
                    if (knownpb)
                        pblocks.append(pblock_pblock2str(knownpb, NULL));
                }
            }
        }

        ereport(degree, "%s%s%s", XP_GetAdminStr(DBT_LogPrefix),
                message, pblocks.data());
    } else {
        rq->directive_is_cacheable = 1;
    }

    return REQ_NOACTION;
}
