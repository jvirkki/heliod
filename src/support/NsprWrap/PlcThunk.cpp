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

void *_PL_Base64Decode;
void *_PL_Base64Encode;
void *_PL_CreateOptState;
void *_PL_DestroyOptState;
void *_PL_FPrintError;
/*
 * Thunk layer to hook old nspr lib name to new name.
 * Written by Robin J. Maxwell 02-18-99
 */
#include <windows.h>

#define UP_MODULE_NAME "libplc4.dll"

HINSTANCE hDll = 0;

//
// Pointers to symbols in the good library

void *_PL_GetNextOpt;
void *_PL_PrintError;
void *_PL_strcasecmp;
void *_PL_strcaserstr;
void *_PL_strcasestr;
void *_PL_strcat;
void *_PL_strcatn;
void *_PL_strchr;
void *_PL_strcmp;
void *_PL_strcpy;
void *_PL_strdup;
void *_PL_strfree;
void *_PL_strlen;
void *_PL_strncasecmp;
void *_PL_strncaserstr;
void *_PL_strncasestr;
void *_PL_strncat;
void *_PL_strnchr;
void *_PL_strncmp;
void *_PL_strncpy;
void *_PL_strncpyz;
void *_PL_strndup;
void *_PL_strnlen;
void *_PL_strnpbrk;
void *_PL_strnprbrk;
void *_PL_strnrchr;
void *_PL_strnrstr;
void *_PL_strnstr;
void *_PL_strpbrk;
void *_PL_strprbrk;
void *_PL_strrchr;
void *_PL_strrstr;
void *_PL_strstr;

// functions
extern "C" {

__declspec(naked) __declspec(dllexport) void PL_FPrintError(void)
{
    _asm jmp DWORD PTR _PL_FPrintError
}

__declspec(naked) __declspec(dllexport) void PL_GetNextOpt(void)
{
    _asm jmp DWORD PTR _PL_GetNextOpt
}

__declspec(naked) __declspec(dllexport) void PL_PrintError(void)
{
    _asm jmp DWORD PTR _PL_PrintError
}

__declspec(naked) __declspec(dllexport) void PL_strcasecmp(void)
{
    _asm jmp DWORD PTR _PL_strcasecmp
}

__declspec(naked) __declspec(dllexport) void PL_strcaserstr(void)
{
    _asm jmp DWORD PTR _PL_strcaserstr
}

__declspec(naked) __declspec(dllexport) void PL_strcasestr(void)
{
    _asm jmp DWORD PTR _PL_strcasestr
}

__declspec(naked) __declspec(dllexport) void PL_strcat(void)
{
    _asm jmp DWORD PTR _PL_strcat
}

__declspec(naked) __declspec(dllexport) void PL_strcatn(void)
{
    _asm jmp DWORD PTR _PL_strcatn
}

__declspec(naked) __declspec(dllexport) void PL_strchr(void)
{
    _asm jmp DWORD PTR _PL_strchr
}

__declspec(naked) __declspec(dllexport) void PL_strcmp(void)
{
    _asm jmp DWORD PTR _PL_strcmp
}

__declspec(naked) __declspec(dllexport) void PL_strcpy(void)
{
    _asm jmp DWORD PTR _PL_strcpy
}

__declspec(naked) __declspec(dllexport) void PL_strdup(void)
{
    _asm jmp DWORD PTR _PL_strdup
}

__declspec(naked) __declspec(dllexport) void PL_strfree(void)
{
    _asm jmp DWORD PTR _PL_strfree
}

__declspec(naked) __declspec(dllexport) void PL_strlen(void)
{
    _asm jmp DWORD PTR _PL_strlen
}

__declspec(naked) __declspec(dllexport) void PL_strncasecmp(void)
{
    _asm jmp DWORD PTR _PL_strncasecmp
}

__declspec(naked) __declspec(dllexport) void PL_strncaserstr(void)
{
    _asm jmp DWORD PTR _PL_strncaserstr
}

__declspec(naked) __declspec(dllexport) void PL_strncasestr(void)
{
    _asm jmp DWORD PTR _PL_strncasestr
}

__declspec(naked) __declspec(dllexport) void PL_strncat(void)
{
    _asm jmp DWORD PTR _PL_strncat
}

__declspec(naked) __declspec(dllexport) void PL_strnchr(void)
{
    _asm jmp DWORD PTR _PL_strnchr
}

__declspec(naked) __declspec(dllexport) void PL_strncmp(void)
{
    _asm jmp DWORD PTR _PL_strncmp
}

__declspec(naked) __declspec(dllexport) void PL_strncpy(void)
{
    _asm jmp DWORD PTR _PL_strncpy
}

__declspec(naked) __declspec(dllexport) void PL_strncpyz(void)
{
    _asm jmp DWORD PTR _PL_strncpyz
}

__declspec(naked) __declspec(dllexport) void PL_strndup(void)
{
    _asm jmp DWORD PTR _PL_strndup
}

__declspec(naked) __declspec(dllexport) void PL_strnlen(void)
{
    _asm jmp DWORD PTR _PL_strnlen
}

__declspec(naked) __declspec(dllexport) void PL_strnpbrk(void)
{
    _asm jmp DWORD PTR _PL_strnpbrk
}

__declspec(naked) __declspec(dllexport) void PL_strnprbrk(void)
{
    _asm jmp DWORD PTR _PL_strnprbrk
}

__declspec(naked) __declspec(dllexport) void PL_strnrchr(void)
{
    _asm jmp DWORD PTR _PL_strnrchr
}

__declspec(naked) __declspec(dllexport) void PL_strnrstr(void)
{
    _asm jmp DWORD PTR _PL_strnrstr
}

__declspec(naked) __declspec(dllexport) void PL_strnstr(void)
{
    _asm jmp DWORD PTR _PL_strnstr
}

__declspec(naked) __declspec(dllexport) void PL_strpbrk(void)
{
    _asm jmp DWORD PTR _PL_strpbrk
}

__declspec(naked) __declspec(dllexport) void PL_strprbrk(void)
{
    _asm jmp DWORD PTR _PL_strprbrk
}

__declspec(naked) __declspec(dllexport) void PL_strrchr(void)
{
    _asm jmp DWORD PTR _PL_strrchr
}

__declspec(naked) __declspec(dllexport) void PL_strrstr(void)
{
    _asm jmp DWORD PTR _PL_strrstr
}

__declspec(naked) __declspec(dllexport) void PL_strstr(void)
{
    _asm jmp DWORD PTR _PL_strstr
}

} // Extern C

// Load
int load_plc_table()
{
    if (!(hDll = GetModuleHandle(UP_MODULE_NAME)))
        hDll = LoadLibrary(UP_MODULE_NAME);
	if (hDll){
        _PL_FPrintError = GetProcAddress(hDll, "PL_FPrintError");
        _PL_GetNextOpt = GetProcAddress(hDll, "PL_GetNextOpt");
        _PL_PrintError = GetProcAddress(hDll, "PL_PrintError");
        _PL_strcasecmp = GetProcAddress(hDll, "PL_strcasecmp");
        _PL_strcaserstr = GetProcAddress(hDll, "PL_strcaserstr");
        _PL_strcasestr = GetProcAddress(hDll, "PL_strcasestr");
        _PL_strcat = GetProcAddress(hDll, "PL_strcat");
        _PL_strcatn = GetProcAddress(hDll, "PL_strcatn");
        _PL_strchr = GetProcAddress(hDll, "PL_strchr");
        _PL_strcmp = GetProcAddress(hDll, "PL_strcmp");
        _PL_strcpy = GetProcAddress(hDll, "PL_strcpy");
        _PL_strdup = GetProcAddress(hDll, "PL_strdup");
        _PL_strfree = GetProcAddress(hDll, "PL_strfree");
        _PL_strlen = GetProcAddress(hDll, "PL_strlen");
        _PL_strncasecmp = GetProcAddress(hDll, "PL_strncasecmp");
        _PL_strncaserstr = GetProcAddress(hDll, "PL_strncaserstr");
        _PL_strncasestr = GetProcAddress(hDll, "PL_strncasestr");
        _PL_strncat = GetProcAddress(hDll, "PL_strncat");
        _PL_strnchr = GetProcAddress(hDll, "PL_strnchr");
        _PL_strncmp = GetProcAddress(hDll, "PL_strncmp");
        _PL_strncpy = GetProcAddress(hDll, "PL_strncpy");
        _PL_strncpyz = GetProcAddress(hDll, "PL_strncpyz");
        _PL_strndup = GetProcAddress(hDll, "PL_strndup");
        _PL_strnlen = GetProcAddress(hDll, "PL_strnlen");
        _PL_strnpbrk = GetProcAddress(hDll, "PL_strnpbrk");
        _PL_strnprbrk = GetProcAddress(hDll, "PL_strnprbrk");
        _PL_strnrchr = GetProcAddress(hDll, "PL_strnrchr");
        _PL_strnrstr = GetProcAddress(hDll, "PL_strnrstr");
        _PL_strnstr = GetProcAddress(hDll, "PL_strnstr");
        _PL_strpbrk = GetProcAddress(hDll, "PL_strpbrk");
        _PL_strprbrk = GetProcAddress(hDll, "PL_strprbrk");
        _PL_strrchr = GetProcAddress(hDll, "PL_strrchr");
        _PL_strrstr = GetProcAddress(hDll, "PL_strrstr");
        _PL_strstr = GetProcAddress(hDll, "PL_strstr");
	}else{
		DWORD err;
		err = GetLastError();
		//char buff[1024];
		//sprintf(buff, "Error opening %s.  Error=%d", UP_MODULE_NAME, err);
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
		    if (load_plc_table())
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
