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

#include <stdlib.h>
#ifdef XP_WIN32
#include "base/dll.h"
#else
#include <dlfcn.h>
#endif
#include "definesEnterprise.h"
#include "base/ereport.h"
#include "frame/conf.h"
#include "support/stringvalue.h"
#include "smartheapinit.h"

#ifdef USE_SMARTHEAP

//-----------------------------------------------------------------------------
// SmartHeapInit
//
// Look after the details of SmartHeap initialization.
//
// On Unix, the SmartHeap .so may be loaded via LD_PRELOAD or specified at link
// time.  On NT, the web server loads the SmartHeap DLL dynamically itself 
// (this will also work if the DLL was specified at link time).  In all cases,
// we will call MemRegisterTask() before creating any threads.
//-----------------------------------------------------------------------------

void SmartHeapInit(void)
{
    void (*pMemRegisterTask)(void) = 0;

#ifdef XP_WIN32
    // Dynamically loaded SmartHeap under Win32
    if (conf_getboolean("SmartHeap", PR_TRUE)) {
        const char* filename = conf_getstring("SmartHeapDLL", "../../" PRODUCT_LIB_SUBDIR "/libsh.dll");
        DLHANDLE dlhSmartHeap = dll_open(filename);
        if (dlhSmartHeap) {
            pMemRegisterTask = (void(*)(void))dll_findsym(dlhSmartHeap, "MemRegisterTask");
        }
        if (!pMemRegisterTask) {
            ereport(LOG_FAILURE, "Error opening SmartHeap DLL %s", filename);
        }
    }
#elif !defined(RTLD_DEFAULT) || defined(LINK_SMARTHEAP)
    // Statically linked SmartHeap under Unix
    void MemRegisterTask(void);
    pMemRegisterTask = &MemRegisterTask;
#else
    // LD_PRELOAD'd SmartHeap under Unix
    pMemRegisterTask = (void(*)(void))dlsym(RTLD_DEFAULT, "MemRegisterTask");
#endif

    // Did we find the MemRegisterTask entry point?
    PRBool flagHaveSmartHeap = PR_FALSE;
    if (pMemRegisterTask) {
        // SmartHeap (or a SmartHeap-like stub) is resident
        putenv("SMARTHEAP=ENABLED");
        (*pMemRegisterTask)();
        char* shstatus = getenv("SMARTHEAP");
        if (!shstatus || StringValue::getBoolean(shstatus)) {
            // This is SmartHeap, not a SmartHeap stub
            flagHaveSmartHeap = PR_TRUE;
        }
    }

    if (flagHaveSmartHeap) {
        ereport(LOG_VERBOSE, "SmartHeap memory management enabled");
    } else {
        ereport(LOG_VERBOSE, "SmartHeap memory management disabled");
    }
}

#endif
