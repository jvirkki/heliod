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
 * dl.c: Handles loading application functions from dynamic libraries
 * 
 * Rob McCool
 */


#include "netsite.h"
#include "support/NSString.h"
#include "safs/dl.h"
#include "safs/dbtsafs.h"
#include "frame/func.h"
#include "frame/conf.h"
#include "base/dll.h"
#include "base/util.h"
#include "base/ereport.h"

#include <plstr.h>


#ifdef DLL_CAPABLE

/**
 * Ruslan: add dlopen init flags
 */

static	struct	{
		const char *flag_name;
		int	flag_mode;
	}	dlopen_modes[]	=	{
			{ "lazy"	,	RTLD_LAZY		},
			{ "now"		,	RTLD_NOW		},
			{ "global"	,	RTLD_GLOBAL		},
			{ "local"	,	RTLD_LOCAL		},
#ifdef RTLD_PARENT
			{ "parent"	,	RTLD_PARENT		},
#endif
#ifdef RTLD_GROUP
			{ "group" 	,	RTLD_GROUP 		},
#endif
#ifdef RTLD_WORLD
			{ "world"	,	RTLD_WORLD		},
#endif
#ifdef RTLD_FIRST
			{ "first"	,	RTLD_FIRST		},
#endif
			{ "default"	,	DLL_DLOPEN_FLAGS},
			{ NULL		,	0				}
		};

static	const char *dllopen_sep = " \t|,:()";

static int
parse_dlopen_flags(const char *flag)
{
    int mode = DLL_DLOPEN_FLAGS;

    if (flag != NULL) {
        char *flagcopy = STRDUP(flag);

        char *cp = strtok(flagcopy, dllopen_sep);
        if (cp != NULL) {
            mode = 0;
            do {
                for (int i = 0; dlopen_modes[i].flag_name != NULL; i++)
                    if (!PL_strcasecmp(cp, dlopen_modes[i].flag_name))
                        mode |= dlopen_modes[i].flag_mode;
                mode |= strtol(cp, NULL, 0);
            } while ((cp = strtok(NULL, dllopen_sep)) != NULL);
        }

        FREE(flagcopy);
    }

    return mode;
}


/* ------------------------------- dl_init -------------------------------- */


int 
mark_func_cacheable(pblock *pb, Session *, Request *)
{
  char *funcNames = pblock_findval ("funcs", pb);

  if (!funcNames) {
    pblock_nvinsert("error","missing parameter (need funcs)",pb);
    return REQ_ABORTED;
  }

  /* We're going to mess this string up a little... */
  funcNames = STRDUP(funcNames);
  char* t = funcNames;
  while(1) {
    char* u = t;
    for(u = t; *u && *u != ','; ++u);
      char c = *u;
      *u = '\0';

      char* function = STRDUP(t);
      //
      // Convert dash to underscore
      for (char* w = function; *w; ++w)
        if (*w == '-')
          *w = '_';
     
      struct FuncStruct * fn = func_find_str(t); 
      if (!fn) {
        char errStr[256];
        sprintf(errStr, "Cannot find function %s", t);
        pblock_nvinsert("error", errStr, pb);
        return REQ_ABORTED;
      }
        
      t = u;
      if (!(*t++ = c))
        break;
  }

  return REQ_PROCEED;
}

static DLHANDLE dl_init(pblock *pb, const char *libname, PRBool bNativeThread)
{
    /* Look for a cached DLHANDLE */
    DLHANDLE *pdlp = (DLHANDLE *)pblock_findkeyval(pb_key_magnus_internal, pb);
    if (pdlp)
        return *pdlp;

    char *funcNames = pblock_findval ("funcs", pb);
    const char *poolName  = pblock_findval ("pool", pb);

    int funcflags = FUNC_USE_NATIVE_THREAD;
    char c, err[MAGNUS_ERROR_LEN];
    register char *t, *u, *function, *w;
    DLHANDLE dlp;
    FuncPtr tfn;

    ereport(LOG_VERBOSE, "attempting to load %s", libname);

    if(!(dlp = dll_open2 (libname, parse_dlopen_flags (pblock_findval ("shlib_flags", pb)))))	{
        char* dllerr = dll_error(dlp);
        util_snprintf(err, MAGNUS_ERROR_LEN, "dlopen of %s failed (%s)", libname, dllerr);
        pblock_nvinsert("error", err, pb);
        return NULL;
    }

    if(funcNames) {
        /* We're going to mess this string up a little... */
        funcNames = STRDUP(funcNames);
        t = funcNames;

        if (bNativeThread) {
            if (poolName == NULL)
                poolName = "NativePool";
        } else {
            funcflags &= ~(FUNC_USE_NATIVE_THREAD);
        }

        while(1) {
            for(u = t; *u && *u != ','; ++u);
            c = *u;
            *u = '\0';

            function = STRDUP(t);
            //
            // Convert dash to underscore
            for(w = function; *w; ++w)
                if(*w == '-')
                    *w = '_';

            tfn = (FuncPtr) dll_findsym(dlp, function);
            FREE(function);

            if(!tfn) {
                util_sprintf(err, "dlsym for %s failed (%s)", t, dll_error(dlp));
                pblock_nvinsert("error", err, pb);
                FREE(funcNames);
                return NULL;
            }
            func_insert2 (STRDUP(t), tfn, funcflags, poolName);
            t = u;
            if(!(*t++ = c))
                break;
        }
        FREE(funcNames);
    }

    /* Cache the DLHANDLE */
    pblock_kvinsert(pb_key_magnus_internal, (const char *)&dlp, sizeof(dlp), pb);

    return dlp;
}

int load_modules(pblock *pb, Session *sn, Request *rq)
{
    const char *libname = pblock_findval("shlib", pb);
    const char *cNativeThread = pblock_findval("NativeThread", pb);
    PRBool bNativeThread = util_getboolean(cNativeThread, PR_TRUE);

    if(!libname) {
        pblock_nvinsert("error", XP_GetAdminStr(DBT_NeedShlib), pb);
        return NULL;
    }

    // Load the module
    DLHANDLE dlp = dl_init(pb, libname, bNativeThread);
#ifdef PRODUCT_PLUGINS_SUBDIR
    if(!dlp && !file_is_path_abs(libname)) {
        // The module wasn't found in the OS library path, so check the
        // server's private plugins directory
        NSString plugins_root;
        plugins_root.append(conf_get_true_globals()->Vnetsite_root);
        plugins_root.append("/"PRODUCT_PLUGINS_SUBDIR"/");

        // Look for the module in each subdirectory of the plugins directory
        if(SYS_DIR ds = dir_open(plugins_root)) {
            while(SYS_DIRENT *d = dir_read(ds)) {
                if(d->d_name[0] == '.' && d->d_name[1] != '\0')
                    continue;

                NSString path;
                path.append(plugins_root);
                path.append(d->d_name);
                if(*PRODUCT_PLATFORM_SUBDIR)
                    path.append("/"PRODUCT_PLATFORM_SUBDIR);
                path.append("/");
                path.append(libname);

                struct stat finfo;
                if(!system_stat(path, &finfo)) {
                    dlp = dl_init(pb, path, bNativeThread);
                    if(dlp) {
                        libname = path;
                        break;
                    }
                }
            }
            dir_close(ds);
        }
    }
#endif
    if(!dlp)
        return REQ_ABORTED;

    // Call the module's initialization entry point
    FuncPtr nsapi_module_init = (FuncPtr)dll_findsym(dlp, "nsapi_module_init");
    if(nsapi_module_init) {
        // If this is a NativeThread="yes" NSAPI module, any functions it
        // creates will be FUNC_USE_NATIVE_THREAD, too
        PRBool bOldNativeThread = func_set_default_flag(FUNC_USE_NATIVE_THREAD, bNativeThread);

        int rv = (*nsapi_module_init)(pb, sn, rq);

        func_set_default_flag(FUNC_USE_NATIVE_THREAD, bOldNativeThread);

        if(rv == REQ_ABORTED) {
            char err[MAGNUS_ERROR_LEN];
            util_snprintf(err, sizeof(err), "initialization of %s failed",
                          libname);
            pblock_nvinsert("error", err, pb);
            return rv;
        }
    }

    return REQ_PROCEED;
}
#endif
