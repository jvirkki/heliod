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

#include "nsfc_pvt.h"

PR_IMPLEMENT_DATA(PRMonitor *) NSFCMonitor = NULL;
PR_IMPLEMENT_DATA(PRBool) NSFCTerminating = PR_FALSE;

static PRCallOnceType once = { 0 };

static PRStatus InitializeCache(void)
{
    NSFCMonitor = PR_NewMonitor();
    if (!NSFCMonitor) return PR_FAILURE;

    return PR_SUCCESS;
}

/*
 * NSFC_EnterGlobalMonitor - enter the file cache module monitor
 *
 * This function enters the monitor associated with the file cache
 * module.  The same thread may enter the monitor more than once,
 * but only one thread may enter at a time.
 */
PR_IMPLEMENT(void)
NSFC_EnterGlobalMonitor(void)
{
    PRStatus rv = PR_CallOnce(&once, InitializeCache);
    if (rv == PR_SUCCESS) {
        PR_EnterMonitor(NSFCMonitor);
    }
}

/*
 * NSFC_ExitGlobalMonitor - exit the file cache module monitor
 *
 * This function exits the monitor associated with the file cache
 * module, previously entered via NSFC_EnterGlobalMonitor().  The
 * thread must call NSFC_ExitGlobalMonitor() the same number of times
 * as it has called NSFC_EnterGlobalMonitor() before another thread can
 * enter the monitor.
 */
PR_IMPLEMENT(PRStatus)
NSFC_ExitGlobalMonitor(void)
{
    return PR_ExitMonitor(NSFCMonitor);
}

PR_IMPLEMENT(PRStatus)
NSFC_Initialize(const NSFCGlobalConfig *config, PRIntn *vmin, PRIntn *vmax)
{
    PRStatus rv = PR_SUCCESS;

    /* Return the minimum and maximum version numbers if possible */
    if (vmin)
        *vmin = NSFC_API_VERSION_MIN;
    if (vmax)
        *vmax = NSFC_API_VERSION_MAX;

    /* If configuration is present, validate version number */
    if (config && (config->version < NSFC_API_VERSION_MIN ||
                   config->version > NSFC_API_VERSION_MAX)) {
        rv = PR_FAILURE;
    }
    else {
        rv = PR_CallOnce(&once, InitializeCache);
    }
            
    return rv;
}

PR_IMPLEMENT(void)
NSFC_Terminate(PRBool flushtmp, PRBool doitnow)
{
    if (NSFCMonitor) {
        PR_DestroyMonitor(NSFCMonitor);
        NSFCMonitor = NULL;
    }
}
