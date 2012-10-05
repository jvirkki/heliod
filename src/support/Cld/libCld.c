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
 * libCld.so
 *
 * Both libC (-compat=4) and libCrun (-compat=5) define and use the
 * _ex_register symbol to register exception handlers.  However, the two
 * libraries' _ex_register definitions are incompatible.
 *
 * libCld.so defines an _ex_register symbol that attempts to determine
 * whether the caller expects libC or libCrun semantics and invokes the
 * appropriate _ex_register.  This makes it possible to run code linked
 * against libC in a process linked against libCrun and vice versa.
 *
 * libCld.so recognizes the following environment variables:
 *
 *     LIBCLD_DEFAULT_LIBRARY - library containing the default _ex_register
 *     LIBCLD_LIBC_LIBRARY    - library containing the libC _ex_register
 *     LIBCLD_LIBCRUN_LIBRARY - library containing the libCrun _ex_register
 *     LIBCLD_LIBC_PATTERN    - shared objects that need libC
 *     LIBCLD_LIBCRUN_PATTERN - shared objects that need libCrun
 *     LIBCLD_NO_AUTODETECT   - set to disable automatic libC/libCrun detection
 *
 * libCld.so attempts to detect whether a shared object expects libC or
 * libCrun semantics by searching for the _ex_clean symbol.  This symbol is
 * referenced by libC code but not by libCrun code.  Conversely, the mangled
 * equivalent is referenced only by libCrun code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <regex.h>
#include <dlfcn.h>
#include <link.h>
#include <gelf.h>
#include <sys/machelf.h>

#define EX_REGISTER      "_ex_register"
#define LIBC_EX_CLEAN    "_ex_clean"
#define LIBCRUN_EX_CLEAN "__1cG__CrunIex_clean6F_v_"
#define LIBC_SO          "/usr/lib/libC.so.5"
#define LIBCRUN_SO       "/usr/lib/libCrun.so.1"
#define LIBCLD_SO        "libCld.so"
#define LIBTHREAD_SO     "libthread.so.1"
#define LIBPTHREAD_SO    "libpthread.so.1"

#ifndef DEFAULT_SO
#define DEFAULT_SO LIBCRUN_SO
#endif

typedef struct cache {
    GElf_Shdr c_shdr;
    Elf_Data *c_data;
} Cache;

typedef void *(*ex_register_t)(void *, void *, void *, void *, void *);

static ex_register_t _fnC;
static ex_register_t _fnCrun;
static ex_register_t _fnDefault;
static regex_t _reC;
static regex_t _reCrun;
static int _autodetect = 1;
static pthread_key_t _key;
static int _initialized = 0;

typedef struct ex_register_thread_s {
    int active;
    void *a;
    ex_register_t fn;
} ex_register_thread_t;

/*
 * Extract a regular expression from an environment variable
 */
static void getenvre(regex_t *re, const char *name)
{
    const char *value;

    value = getenv(name);
    if (value)
        regcomp(re, value, REG_EXTENDED|REG_NOSUB);
}

/*
 * Lookup _ex_register in a specific lib
 */
static ex_register_t getfn(const char *filename)
{
    ex_register_t fn = NULL;
    void *dlh;

    dlh = dlopen(filename, RTLD_LAZY|RTLD_LOCAL|RTLD_NOLOAD);
    if (dlh)
        fn = (ex_register_t)dlsym(dlh, EX_REGISTER);

    return fn;
}

/*
 * Lookup _ex_register in the lib specified by the environment variable 'name'
 */
static ex_register_t getenvfn(const char *name, const char *deflib)
{
    const char *lib;

    lib = getenv(name);
    if (!lib)
        lib = deflib;

    return getfn(lib);
}

/*
 * Destroy thread-specific data
 */
static void ex_register_thread_destructor(void *thread)
{
    free(thread);
    pthread_setspecific(_key, NULL);
}

/*
 * Initialize libCld.so
 */
static void init(void)
{
    if (_initialized)
        return;

    _initialized = -1;

    /* XXX /bin/sh defines its own malloc, making interposition difficult */
    if (dlsym(RTLD_DEFAULT, "malloc") < dlsym(RTLD_NEXT, "malloc"))
        return;

    pthread_key_create(&_key, &ex_register_thread_destructor);

    _fnC = getenvfn("LIBCLD_LIBC_LIBRARY", LIBC_SO);

    _fnCrun = getenvfn("LIBCLD_LIBCRUN_LIBRARY", LIBCRUN_SO);

    _fnDefault = getenvfn("LIBCLD_DEFAULT_LIBRARY", DEFAULT_SO);

    getenvre(&_reC, "LIBCLD_LIBC_PATTERN");

    getenvre(&_reCrun, "LIBCLD_LIBCRUN_PATTERN");

    if (getenv("LIBCLD_NO_AUTODETECT"))
        _autodetect = 0;

    _initialized = 1;
}

/*
 * Prepare to make an _ex_register call.  Returns an ex_register_thread_t *
 * if libCld has been successfully initialized and another _ex_register call
 * isn't already in progress.
 */
ex_register_thread_t *begin_ex_register()
{
    ex_register_thread_t *thread;

    if (!_initialized)
        init();

    if (_initialized != 1)
        return NULL;

    thread = pthread_getspecific(_key);
    if (!thread) {
        thread = calloc(1, sizeof(*thread));
        pthread_setspecific(_key, thread);
    }

    if (thread) {
        if (thread->active)
            return NULL;
        thread->active = 1;
    }

    return thread;
}

/*
 * Prepare to return from an _ex_register call
 */
void end_ex_register(ex_register_thread_t *thread)
{
    if (thread)
        thread->active = 0;
}

/*
 * Check for a reference to the symbol 'symname' in module 'elf'.
 */
static int findelfsymref(Elf *elf, const char *symname)
{
    GElf_Ehdr ehdr;
    Elf_Scn *scn;
    Cache *cache;
    Cache *_cache;
    Cache *csymtab;
    const char *strs;
    int i;
    int rv = 0;

    if (gelf_getehdr(elf, &ehdr) == NULL)
        return 0;

    /*
     * If there are no sections (core files) we might as well return now.
     */
    if (ehdr.e_shnum == 0)
        return 0;
        
    /*
     * Fill in the cache descriptor with information for each section.
     */
    cache = (Cache *)calloc(ehdr.e_shnum, sizeof(Cache));
    if (!cache)
        return 0;

    _cache = cache;
    _cache++;
    csymtab = 0;

    for (scn = NULL; scn = elf_nextscn(elf, scn); _cache++) {
        if (gelf_getshdr(scn, &_cache->c_shdr) == NULL) {
            free(cache);
            return 0;
        }

        _cache->c_data = elf_getdata(scn, NULL);
    }

    for (i = 1; i < ehdr.e_shnum; i++) {
        if (cache[i].c_shdr.sh_type == SHT_DYNSYM) {
            csymtab = &cache[i];
            break;
        } else if (cache[i].c_shdr.sh_type == SHT_SYMTAB) {
            csymtab = &cache[i];
        }
    }
    if (!csymtab) {
        free(cache);
        return 0;
    }

    strs = cache[csymtab->c_shdr.sh_link].c_data->d_buf;
    for (i = 0; (Xword)i < csymtab->c_shdr.sh_size / csymtab->c_shdr.sh_entsize; i++) {
        GElf_Sym tsym;
        if (gelf_getsym(csymtab->c_data, i, &tsym) == NULL)
            break;
        if (!strcmp(symname, strs + tsym.st_name)) {
            rv = 1;
            break;
        }
    }

    free(cache);

    return rv;
}

/*
 * Check for a reference to the symbol 'symname' in shared object 'filename'.
 */
static int findsymref(const char *filename, const char *symname)
{
    Elf *elf;
    int fd;
    int rv = 0;

    fd = open(filename, O_RDONLY);
    if (fd == -1)
        return 0;

    elf_version(EV_CURRENT);

    elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) {
        close(fd);
        return 0;
    }

    if (elf_kind(elf) == ELF_K_ELF)
        rv = findelfsymref(elf, symname);

    elf_end(elf);

    close(fd);

    return rv;
}

/*
 * Return the filename portion of a path.
 */
static const char *findfname(const char *path)
{
    const char *fname = path;
    const char *p;

    p = fname;
    while (*p) {
        if (*p == '/')
            fname = p + 1;
        p++;
    }

    return fname;
}

/*
 * Return 0 if paths a and b end with the same filename.
 */
static int fnamecmp(const char *a, const char *b)
{
    a = findfname(a);
    b = findfname(b);

    /* reimplement strcmp() to avoid pulling in libc */
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

/*
 * Direct _ex_register calls to libC or libCrun as appropriate
 */
void *_ex_register(void *a, void *b, void *c, void *d, void *e)
{
    ex_register_thread_t *thread = NULL;
    ex_register_t fn = NULL;
    Dl_info dli;
    void *rv;

    /* If we're being called from libC, libCrun, or libCld... */
    if (_initialized != 1 && dladdr(a, &dli) != 0 &&
        (!fnamecmp(dli.dli_fname, LIBC_SO) ||
         !fnamecmp(dli.dli_fname, LIBCRUN_SO) ||
         !fnamecmp(dli.dli_fname, LIBCLD_SO) ||
         !fnamecmp(dli.dli_fname, LIBTHREAD_SO) ||
         !fnamecmp(dli.dli_fname, LIBPTHREAD_SO)))
    {
        fn = (ex_register_t)dlsym(RTLD_NEXT, EX_REGISTER);
        if (fn)
            return fn(a, b, c, d, e);
    }

    thread = begin_ex_register();

    if (thread && thread->a == a) {
        /* We saw this address before and know which library it expects */
        fn = thread->fn;

    } else if (dladdr(a, &dli) != 0) {
        /* Figure out which library the caller expects */
        if (regexec(&_reC, dli.dli_fname, 0, NULL, 0) == 0) {
            /* Shared object matched libC pattern */
            fn = _fnC;

        } else if (regexec(&_reCrun, dli.dli_fname, 0, NULL, 0) == 0) {
            /* Shared object matched libCrun pattern */
            fn = _fnCrun;

        } else if (_autodetect) {
            if (thread) {
                if (findsymref(dli.dli_fname, LIBC_EX_CLEAN)) {
                    /* Shared object references a libC-only pattern */
                    fn = _fnC;

                } else if (findsymref(dli.dli_fname, LIBCRUN_EX_CLEAN)) {
                    /* Shared object references a libCrun-only pattern */
                    fn = _fnCrun;
                }
            }
        }
    }

    if (!fn)
        fn = _fnDefault;

    if (!fn)
        fn = (ex_register_t)dlsym(RTLD_NEXT, EX_REGISTER);

    if (!fn) {
        /* Couldn't find _ex_register */
        const char *err = dlerror();
        write(2, err, strlen(err));
        write(2, "\n", 1);
        abort();
    }

    if (thread) {
        /* Remember this caller-to-function mapping */
        thread->a = a;
        thread->fn = fn;
    }

    /* Call the appropriate _ex_register */
    rv = fn(a, b, c, d, e);

    end_ex_register(thread);

    return rv;
}

