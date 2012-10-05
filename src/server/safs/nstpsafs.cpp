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

/**
 * nstpinit.cpp - Netscape thread pool init
 *
 * @author Ruslan Belkin <ruslan@netscape.com>
 * @version 1.0
 */

#include "netsite.h"
#include "safs/nstpsafs.h"
#include "frame/func.h"
#include "frame/log.h"

NSAPI_PUBLIC int
nstp_init_saf (pblock *pb, Session *, Request *)
{
        char* lateInit = pblock_findval("LateInit", pb);
        if (lateInit && (toupper(*lateInit) == 'Y')) {
            log_ereport(LOG_WARN, "thread-pool-init", 
            "This function must be called as an Early Init only");
            return REQ_ABORTED;
        }
	char *poolName = pblock_findval ("name", pb);

#if defined (XP_PC)
	PRBool poolStart = PR_TRUE;
#else
	PRBool poolStart = PR_FALSE;
#endif

	if (poolName == NULL)
	{
		pblock_nvinsert ("error", "missing name parameter", pb);
		return REQ_ABORTED;
	}

	unsigned maxThreads = 0;
	unsigned minThreads = 0;
	unsigned queueSize  = 0;
	unsigned stackSize  = 0;

	const char *val = pblock_findval ("MaxThreads", pb);

	if (val != NULL)
		maxThreads = atoi (val);

	val = pblock_findval ("MinThreads", pb);

	if (val != NULL)
		minThreads = atoi (val);

	val = pblock_findval ("QueueSize" , pb);

	if (val != NULL)
		queueSize  = atoi (val);

	val = pblock_findval ("StackSize" , pb);

	if (val != NULL)
		stackSize  = atoi (val);

	return func_addPool (poolName, minThreads, maxThreads, queueSize, stackSize, poolStart);
}
