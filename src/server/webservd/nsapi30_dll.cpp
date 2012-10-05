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
 *
 *
 */
#include <windows.h>
#define INTNSAPI
#include "netsite.h"
#include "frame/log.h"

#define UP_MODULE_NAME "ns-httpd40.dll"

NSAPI_PUBLIC nsapi_dispatch_t *__nsapi30_table = 0;
HINSTANCE hDll = 0;


int load_35_nsapi_table()
{
	hDll = GetModuleHandle(UP_MODULE_NAME);
	if (hDll){
		nsapi_dispatch_t **foreign_nsapi30_table;

		foreign_nsapi30_table = (nsapi_dispatch_t **)GetProcAddress(hDll, "__nsapi30_table");
		if (foreign_nsapi30_table)
			__nsapi30_table = *foreign_nsapi30_table;
		else{
			DWORD err;
			err = GetLastError();
			char buff[1024];
			sprintf(buff, "Error loading NSAPI function table for 3.0 compatibility.  Error=%d", err);
			log_ereport(LOG_FAILURE, buff);
			return 1;
		}
	}else{
		DWORD err;
		err = GetLastError();
		char buff[1024];
		sprintf(buff, "Error opening %s.  Error=%d", UP_MODULE_NAME, err);
		return 1;
	}
	return 0;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                        DWORD ul_reason_for_call, 
                        LPVOID lpReserved )
{
    switch( ul_reason_for_call ) {
    case DLL_PROCESS_ATTACH:
		if (load_35_nsapi_table())
			return FALSE;
    break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
	break;
    case DLL_PROCESS_DETACH:
		FreeLibrary(hDll);
		break;
	default:
	break;
    }
    return TRUE;
}
