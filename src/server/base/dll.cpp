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
 * dll.c: Abstracts loading of dynamic libraries
 * 
 * Rob McCool
 */


#include "netsite.h"
#include "systems.h"
#include "file.h"
#include "dll.h"
#include "prerror.h"
#include "NsprWrap/NsprError.h"
#include <limits.h>

#ifdef DLL_CAPABLE

#if defined(USE_NSPR_DLL)
#include <nspr/prlink.h>

#elif defined(DLL_DLOPEN)
#include <dlfcn.h>

#elif defined(DLL_HPSHL)
#include <dl.h>
#endif

#if defined(DLL_WIN32)
static char _dll_library[MAX_PATH];

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        GetModuleFileName(hinstDLL, _dll_library, sizeof(_dll_library));

    return TRUE;
}
#else
static char _dll_library[PATH_MAX];

class DllInitialize {
public:
    DllInitialize()
    {
#if defined(DLL_DLOPEN)
        char *name = PR_GetLibraryFilePathname(NULL, (PRFuncPtr)_dll_library);
        if (name) {
            int len = strlen(name);
            if (len >= PATH_MAX)
                len = PATH_MAX-1;
            memcpy(_dll_library, name, len);
            _dll_library[len] = '\0';
            PR_Free(name);
        }
#elif defined (DLL_HPSHL)
        const unsigned long addr = (unsigned long) _dll_library;
        struct shl_descriptor desc;
        for (int i = 0; shl_get_r(i, &desc) == 0; i++) {
            if (desc.dstart <= addr && desc.dend >= addr) {
                realpath(desc.filename, _dll_library);
                break;
            }
        }
#else
#error Missing DllInitialize implementation
#endif
    }
} _dll_initialize;
#endif

/*
 * NSPR is broken for shared libs.
 * It doesn't allow dynamic managment.
 * Once loaded, they are never unloaded.
 */
NSAPI_PUBLIC DLHANDLE dll_open(const char *fn)
{
	return dll_open2 (fn, DLL_DLOPEN_FLAGS);
}

NSAPI_PUBLIC DLHANDLE dll_open2 (const char *fn, int mode)
{
#if defined(USE_NSPR_DLL)
    if(!(PR_LoadLibrary(fn)))
        return NULL;
    return (void *) 1;

#elif defined(DLL_WIN32)
    struct stat finfo;
    char d2[MAX_PATH];

    file_unix2local(fn, d2);

    /* LoadLibrary returns a bad pointer instead of NULL on error */
    /* WS7(2005/05/25): The above statement is no longer true. */
    /* If the library path is a relative path, then no need for stat. */
    if(file_is_path_abs(d2)) {
        if(system_stat(d2, &finfo) == -1)
            return NULL;
    }
    void *dlp = LoadLibraryEx(d2, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dlp)
        dlp = LoadLibrary(d2);
    if (!dlp)
        NsprError::mapWin32Error();
    return dlp;

#else
    void *dlp;

#if defined(DLL_HPSHL)
    /* Added DYNAMIC_PATH, so as to pick the path from SHLIB_PATH */
    dlp = shl_load(fn, BIND_IMMEDIATE | BIND_VERBOSE | DYNAMIC_PATH, NULL);
#elif defined(AIX)
    dlp = dlopen((char *)fn, mode);
#else
    dlp = dlopen(fn, mode);
#endif
    if (!dlp)
        NsprError::mapUnixErrno();

    return dlp;
#endif
}


NSAPI_PUBLIC void *dll_findsym(DLHANDLE dlp, const char *name)
{
#if defined(USE_NSPR_DLL)
    return PR_FindSymbol(name);

#elif defined(DLL_WIN32)
    void *sym = GetProcAddress((HMODULE) dlp, name);
    if (!sym)
        NsprError::mapWin32Error();
    return sym;

#else
    void *sym;

#if defined(DLL_HPSHL)
    if (shl_findsym((shl_t *)&dlp, name, TYPE_PROCEDURE, &sym) == -1)
        sym = NULL;
#elif defined(AIX)
    sym = dlsym(dlp, (char *)name);
#else
    sym = dlsym(dlp, name);
#endif
    if (!sym)
        NsprError::mapUnixErrno();

    return sym;
#endif
}

NSAPI_PUBLIC char *dll_error(DLHANDLE)
{
#if defined(USE_NSPR_DLL)
    return PR_DLLError();

#elif defined(DLL_DLOPEN)
    return dlerror();

#else
    return system_errmsg();
#endif
}

NSAPI_PUBLIC void dll_close(DLHANDLE dlp)
{
#if defined(USE_NSPR_DLL)
    /* nothing */

#elif defined(DLL_DLOPEN)
    dlclose(dlp);

#elif defined (DLL_HPSHL)
    shl_unload((shl_t) dlp);

#elif defined(DLL_WIN32)
    FreeLibrary((HMODULE) dlp);
#endif
}

NSAPI_PUBLIC const char *dll_library(void)
{
    return _dll_library;
}

#endif /* DLL_CAPABLE */
