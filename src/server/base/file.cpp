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
 * file.c: system specific functions for reading/writing files
 * 
 * See file.h for formal definitions of what these functions do
 *
 * Rob McCool
 */


#include "base/file.h"
#include "base/util.h"
#include "base/dbtbase.h"
#include "nscperror.h"
#include "ereport.h"
#ifdef BSD_RLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#else
#include <stdlib.h>
#include <signal.h>
#endif
#ifdef XP_WIN32
#include <windows.h>
#include <time.h>  /* time */
#include <sys/types.h> /* stat */
#include <sys/stat.h> /* stat */
#include <errno.h>
#include <direct.h>
#endif
#include <string.h>
#include <prerror.h>
#include "base/nsassert.h"
#include "NsprWrap/NsprError.h"
#include "systhr.h"

#include "private/pprio.h"
#include "prlock.h"

/* --- globals -------------------------------------------------------------*/
/* PRFileDesc * SYS_ERROR_FD = NULL; */

static const int errbuf_size = 256;
static const unsigned int LOCKFILERANGE = 0x7FFFFFFF;
static int errmsg_key = -1;
static const char *errmsg_outofmemory = "out of memory";
static const char *errmsg_unknownerror = "unknown error %d";
static PRLock *_atomic_write_lock = NULL;

/* ---------------------------- file_is_slash ----------------------------- */

static inline int file_is_slash(const char c)
{
#ifdef XP_WIN32
    return (c == '/' || c == '\\');
#else
    return (c == '/');
#endif
}

/* --------------------------------- stat --------------------------------- */


   /* XXXMB - Can't convert to PR_GetFileInfo because we directly exported
    * the stat interface... Damn.
    */
NSAPI_PUBLIC int system_stat(const char *path, struct stat *finfo)
{
#ifdef XP_UNIX
    if(stat(path, finfo) == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }
#else /* XP_WIN32 */
    const char *statpath = path;
    char *temppath = NULL;
    int l;

/* The NT stat is very peculiar about directory names. */
/* XXX aruna - there is a bug here, maybe in the C runtime.
 * Stating the same path in a separate program succeeds. From
 * jblack's profiling, this needs to be replaced by the Win32
 * calls anyway.*/
/* XXX elving I bet this problem has gone away, but I don't have a testcase */

    l = strlen(path);
    if((path[l - 1] == '/') && 
       (!(isalpha(path[0]) && (!strcmp(&path[1], ":/")))))
    {
        temppath = STRDUP(path);
        if (!temppath)
            return -1;
        temppath[--l] = '\0';
        statpath = temppath;
    }

    int rv = _stat(statpath, (struct _stat *)finfo);
    
    if(temppath)
        FREE(temppath);

    if(rv == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }

    /* NT sets the time fields to -1 if it thinks that the file
     * is a device ( like com1.html, lpt1.html etc)     In this case
     * simply set last modified time to the current time....
     */

    if (finfo->st_mtime == -1) {
        finfo->st_mtime = time(NULL);
    }
    if (finfo->st_atime == -1) {
        finfo->st_atime = 0;
    }
    if (finfo->st_ctime == -1) {
        finfo->st_ctime = 0;
    }

#endif /* XP_WIN32 */

    if(S_ISREG(finfo->st_mode) && (path[strlen(path) - 1] == '/')) {
        /* File with trailing slash */
        errno = ENOENT;
        NsprError::mapUnixErrno();
        return -1;
    }
    return 0;
}
/* --------------------------------- system_stat64 --------------------------------- */
#ifdef XP_WIN32
NSAPI_PUBLIC int system_stat64(const char *path, struct _stati64 *finfo)
{
    const char *statpath = path;
    char *temppath = NULL;
    int l = strlen(path);

    if((path[l - 1] == '/') &&
       (!(isalpha(path[0]) && (!strcmp(&path[1], ":/")))))
    {
        temppath = STRDUP(path);
        if (!temppath)
            return -1;
        temppath[--l] = '\0';
        statpath = temppath;
    }

    int rv = _stati64(statpath, finfo);
    
    if(temppath)
        FREE(temppath);

    if(rv == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }

    if (finfo->st_mtime == -1) {
        finfo->st_mtime = time(NULL);
    }
    if (finfo->st_atime == -1) {
        finfo->st_atime = 0;
    }
    if (finfo->st_ctime == -1) {
        finfo->st_ctime = 0;
    }

    if(S_ISREG(finfo->st_mode) && (path[strlen(path) - 1] == '/')) {
        errno = ENOENT;
        NsprError::mapUnixErrno();
        return -1;
    }
    return 0;
}
#else
NSAPI_PUBLIC int system_stat64(const char *path, struct stat64 *finfo)
{
    if(stat64(path, finfo) == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }

    if(S_ISREG(finfo->st_mode) && (path[strlen(path) - 1] == '/')) {
        /* File with trailing slash */
        errno = ENOENT;
        NsprError::mapUnixErrno();
        return -1;
    }
    return 0;
}
#endif

NSAPI_PUBLIC int system_lstat(const char *path, struct stat *finfo) 
{
#ifdef XP_UNIX
    if(lstat(path, finfo) == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }    
    if(S_ISREG(finfo->st_mode) && (path[strlen(path) - 1] == '/')) {
        /* File with trailing slash */
        errno = ENOENT;
        NsprError::mapUnixErrno();
        return -1;
    }
    return 0;
#else
    return system_stat(path, finfo);
#endif
}

#ifdef XP_WIN32
NSAPI_PUBLIC int system_lstat64(const char *path, struct _stati64 *finfo)
{
    return system_stat64(path, finfo);
}
#else
NSAPI_PUBLIC int system_lstat64(const char *path, struct stat64 *finfo)
{
    if(lstat64(path, finfo) == -1) {
        NsprError::mapUnixErrno();
        return -1;
    }
    if(S_ISREG(finfo->st_mode) && (path[strlen(path) - 1] == '/')) {
        /* File with trailing slash */
        errno = ENOENT;
        NsprError::mapUnixErrno();
        return -1;
    }
    return 0;
}
#endif

NSAPI_PUBLIC int system_fread(SYS_FILE fd, void *buf, int sz) 
{
    /* XXXMB - this is the *one* function which does return a length
     * instead of the IO_ERROR/IO_OKAY.
     */
    return PR_Read(fd, buf, sz);
}

NSAPI_PUBLIC int system_fwrite(SYS_FILE fd, const void *buf, int sz) {
    int n,o,w;

    for(n=sz,o=0; n; n-=w,o+=w) {
        if((w = PR_Write(fd, (const char *)buf + o, n)) < 0)
            return IO_ERROR;
    }
    return IO_OKAY;
}

/* ---------------------------- Standard UNIX ----------------------------- */

#ifdef XP_UNIX

#include <sys/file.h>   /* flock */

NSAPI_PUBLIC int system_fwrite_atomic(SYS_FILE fd, const void *buf, int sz) 
{
    int ret;
#if 0
    if(flock(fd,LOCK_EX) == -1)
        return IO_ERROR;
#endif
    ret = system_fwrite(fd,buf,sz);
#if 0
    if(flock(fd,LOCK_UN) == -1)
        return IO_ERROR;  /* ??? */
#endif
    return ret;
}

/* -------------------------- system_nocoredumps -------------------------- */


NSAPI_PUBLIC int system_nocoredumps(void)
{
#ifdef BSD_RLIMIT
    struct rlimit rl;
    int rv;

    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    rv = setrlimit(RLIMIT_CORE, &rl);
    if (rv == -1)
        NsprError::mapUnixErrno();

    return rv;
#else
    signal(SIGQUIT, exit);
    signal(SIGILL, exit);
    signal(SIGTRAP, exit);
    signal(SIGABRT, exit);
    signal(SIGIOT, exit);
    signal(SIGEMT, exit);
    signal(SIGFPE, exit);
    signal(SIGBUS, exit);
    signal(SIGSEGV, exit);
    signal(SIGSYS, exit);

    return 0;
#endif
}
#endif /* XP_UNIX */

/* --------------------------- file_setinherit ---------------------------- */

NSAPI_PUBLIC int file_setinherit(SYS_FILE fd, int value)
{
    return PR_SetFDInheritable(fd, value);
}

/* --------------------------- file_setdirectio --------------------------- */

NSAPI_PUBLIC void file_setdirectio(SYS_FILE fd, int value)
{
#if defined(DIRECTIO_ON)
    if (value) {
        directio(PR_FileDesc2NativeHandle(fd), DIRECTIO_ON);
    } else {
        directio(PR_FileDesc2NativeHandle(fd), DIRECTIO_OFF);
    }
#endif
}

NSAPI_PUBLIC SYS_FILE system_fopenRO(const char *p)
{
    SYS_FILE f = PR_Open(p, PR_RDONLY, 0);

    if (!f)
        return SYS_ERROR_FD;
    return f;
}

#ifdef XP_UNIX
static mode_t file_mode = 0644;
static mode_t dir_mode = 0755;

NSAPI_PUBLIC void file_mode_init (mode_t mode)
{
    file_mode = 0666 & mode;
    dir_mode = 0777 & mode;
}
#else
#define file_mode 0644
#define dir_mode 0755
#endif /* XP_UNIX */

NSAPI_PUBLIC SYS_FILE system_fopenWA(const char *p)
{
    SYS_FILE f = PR_Open(p, PR_RDWR|PR_CREATE_FILE|PR_APPEND, file_mode);

    if (!f)
        return SYS_ERROR_FD;
    return f;
}

NSAPI_PUBLIC SYS_FILE system_fopenRW(const char *p)
{
    SYS_FILE f = PR_Open(p, PR_RDWR|PR_CREATE_FILE, file_mode);

    if (!f)
        return SYS_ERROR_FD;
    return f;
}

NSAPI_PUBLIC SYS_FILE system_fopenWT(const char *p)
{
    SYS_FILE f = PR_Open(p, PR_RDWR|PR_CREATE_FILE|PR_TRUNCATE, file_mode);

    if (!f)
        return SYS_ERROR_FD;
    return f;
}

NSAPI_PUBLIC int system_fclose(SYS_FILE fd)
{
    return (PR_Close(fd));
}


#ifdef XP_WIN32

NSAPI_PUBLIC SYS_FILE system_fopen(const char *path, int access, int flags)
{
    char p2[MAX_PATH];
    HANDLE fd;

    if (strlen(path) >= MAX_PATH) {
        PR_SetError(PR_NAME_TOO_LONG_ERROR, 0);
        return SYS_ERROR_FD;
    }

    file_unix2local(path, p2);

    fd = CreateFile(p2, access, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                    NULL, flags, 0, NULL);
    if(fd == INVALID_HANDLE_VALUE) {
        NsprError::mapWin32Error();
        return SYS_ERROR_FD;
    }

    return PR_ImportFile((int32)fd);
}



NSAPI_PUBLIC int system_pread(SYS_FILE fd, void *buf, int BytesToRead) {
    unsigned long BytesRead = 0;
    int result = 0;
    BOOLEAN TimeoutSet = FALSE;

    /* XXXMB - nspr20 should be able to do this; but right now it doesn't
     * return proper error info.
     * fix it later...
     */
    if(ReadFile((HANDLE)PR_FileDesc2NativeHandle(fd), (LPVOID)buf, BytesToRead, &BytesRead, NULL) == FALSE) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            return IO_EOF;
        } else {
            NsprError::mapWin32Error();
            return IO_ERROR;
        }
    }
    return (BytesRead ? BytesRead : IO_EOF);
}

NSAPI_PUBLIC int system_pwrite(SYS_FILE fd, const void *buf, int BytesToWrite) 
{
    unsigned long BytesWritten;
    
    if (WriteFile((HANDLE)PR_FileDesc2NativeHandle(fd), (LPVOID)buf, 
                  BytesToWrite, &BytesWritten, NULL) == FALSE) {
        NsprError::mapWin32Error();
        return IO_ERROR;
    }
    return BytesWritten;
}


NSAPI_PUBLIC int system_fwrite_atomic(SYS_FILE fd, const void *buf, int sz) 
{
    int ret;

#if 0
    if(system_flock(fd) == IO_ERROR)
        return IO_ERROR;
#endif
    /* XXXMB - this is technically thread unsafe, but it catches any 
     * callers of fwrite_atomic when we're single threaded and just coming
     * to life.
     */
    if (!_atomic_write_lock) {
        _atomic_write_lock = PR_NewLock();
    }
    PR_Lock(_atomic_write_lock);
    ret = system_fwrite(fd,buf,sz);
    PR_Unlock(_atomic_write_lock);
#if 0
    if(system_ulock(fd) == IO_ERROR)
        return IO_ERROR;
#endif
    return ret;
}


NSAPI_PUBLIC void file_unix2local(const char *path, char *p2)
{
#ifdef XP_WIN32
    /* Try to handle UNIX-style paths */
    if((strchr(path, FILE_PATHSEP))) {
        int x;

        for(x = 0; path[x]; x++)
            p2[x] = (path[x] == '/' ? '\\' : path[x]);
        p2[x] = '\0';
    }
    else
        strcpy(p2, path);
#else
    strcpy(p2, path);
#endif
}


NSAPI_PUBLIC int system_nocoredumps(void)
{
    return 0;
}


/* ------------------------- Dir related stuff ---------------------------- */


NSAPI_PUBLIC SYS_DIR dir_open(const char *pathp)
{
    dir_s *ret = (dir_s *) MALLOC(sizeof(dir_s));
    char path[MAX_PATH];
    int l;

    if (strlen(pathp) >= MAX_PATH) {
        return NULL;
    }

    l = util_sprintf(path, "%s", pathp) - 1;
    path[strlen(pathp)] = '\0';
    if(path[strlen(path) - 1] != FILE_PATHSEP)
        strcpy (path + strlen(path), "\\*.*");
    else
        util_sprintf(path, "%s*.*", path);

    ret->de.d_name = NULL;
    if( (ret->dp = FindFirstFile(path, &ret->fdata)) != INVALID_HANDLE_VALUE)
        return ret;
    FREE(ret);
    return NULL;
}

NSAPI_PUBLIC SYS_DIRENT *dir_read(SYS_DIR ds)
{
    if(FindNextFile(ds->dp, &ds->fdata) == FALSE)
        return NULL;
    if(ds->de.d_name)
        FREE(ds->de.d_name);
    ds->de.d_name = STRDUP(ds->fdata.cFileName);

    return &ds->de;
}

NSAPI_PUBLIC void dir_close(SYS_DIR ds)
{
    FindClose(ds->dp);
    if(ds->de.d_name)
        FREE(ds->de.d_name);
    FREE(ds);
}

#endif /* XP_WIN32 */

NSAPI_PUBLIC int file_notfound(void)
{
#ifdef XP_WIN32
    PRErrorCode errn = PR_GetError();
    return (errn == PR_FILE_NOT_FOUND_ERROR || errn == PR_NAME_TOO_LONG_ERROR );
#else
    return (errno == ENOENT);
#endif
}

NSAPI_PUBLIC int INTdir_create_mode(char *dir)
{
#ifdef XP_UNIX
    return mkdir(dir, dir_mode);
#else
    return dir_create(dir);
#endif
}


NSAPI_PUBLIC int dir_create_all(char *dir)
{
    struct stat fi;
    char *t;

#ifdef XP_WIN32
    t = dir + 3;
#else /* XP_UNIX */
    t = dir + 1;
#endif
    while(1) {
        t = strchr(t, FILE_PATHSEP);
        if(t) *t = '\0';
        if(stat(dir, &fi) == -1) {
            if(INTdir_create_mode(dir) == -1)
                return -1;
        }
        if(t) *t++ = FILE_PATHSEP;
        else break;
    }
    return 0;
}


NSAPI_PUBLIC void system_errmsg_init(void)
{
    if (errmsg_key == -1) {
        errmsg_key = systhread_newkey();

        const char* msg;
        msg = XP_GetAdminStr(DBT_system_errmsg_outofmemory);
        if (msg && *msg)
            errmsg_outofmemory = msg;
        msg = XP_GetAdminStr(DBT_system_errmsg_unknownerror);
        if (msg && *msg)
            errmsg_unknownerror = msg;

        if (!_atomic_write_lock)
            _atomic_write_lock = PR_NewLock();
    }
}

NSAPI_PUBLIC int system_errmsg_fn(char **buff, size_t maxlen)
{
    const char *lmsg = NULL; /* Local message pointer */
    size_t msglen = 0;
    int sys_error = 0;
    PRErrorCode nscp_error;
#ifdef XP_WIN32
    LPTSTR sysmsg = 0;
#endif

    /* Allocate a buffer if caller didn't provide one */
    if (*buff == NULL) {
        *buff = (char *) PERM_MALLOC(errbuf_size);
        if (*buff != NULL)
            maxlen = errbuf_size;
        else
            maxlen = 0;
    }

    if (maxlen < 1)
        return 0;

    /* Grab the NSPR and OS error numbers from NSPR */
    nscp_error = PR_GetError();
    sys_error = PR_GetOSError();

    /* If there's no NSPR error and NSPR didn't record an OS error, ask the OS
     * directly as a last resort.
     */
#ifdef XP_WIN32
    /* IO completion ports pollute GetLastError() with ERROR_IO_PENDINGs */
    if ((sys_error == 0 || sys_error == ERROR_IO_PENDING) && (nscp_error == 0 || nscp_error == PR_UNKNOWN_ERROR))
#else
    if (sys_error == 0 && (nscp_error == 0 || nscp_error == PR_UNKNOWN_ERROR))
#endif
    {
#ifdef XP_WIN32
        int tmp_err = GetLastError();
#else
        int tmp_err = errno;
#endif
        if (tmp_err < 0) {
            /* Not really an OS error -- must be one of ours */
            nscp_error = tmp_err;
        } else if (tmp_err > 0) {
            sys_error = tmp_err;
        }
    }

    msglen = PR_GetErrorTextLength();

    if (msglen > 0 && maxlen >= msglen + 1) {
        /* Someone set the error message explicitly; use that */
        PR_GetErrorText(*buff);
        lmsg = *buff;

    } else if (nscp_error != 0 && nscp_error != PR_UNKNOWN_ERROR) {
        /* Map an NSPR error to an error message */
        const char *nscp_error_msg;

        nscp_error_msg = nscperror_lookup(nscp_error);
        if(nscp_error_msg){
            lmsg = nscp_error_msg;
        } else {
            util_snprintf(*buff, maxlen, errmsg_unknownerror, nscp_error);
            lmsg = *buff;
        }

    } else {
        /* Map an OS error to an error message */
#if defined(XP_WIN32)
        msglen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS |
                               FORMAT_MESSAGE_ALLOCATE_BUFFER,
                               NULL, 
                               sys_error, 
                               LOCALE_SYSTEM_DEFAULT, 
                               (LPTSTR)&sysmsg, 
                               0, 
                               0);
        if (msglen > 0) {
            lmsg = sysmsg;
            char *eol;
            eol = strchr(lmsg, '\r');
            if (eol)
                *eol = '\0';
            eol = strchr(lmsg, '\n');
            if (eol)
                *eol = '\0';
        } else {
            util_snprintf(*buff, maxlen, errmsg_unknownerror, sys_error);
            lmsg = *buff;
        }
#else /* XP_UNIX */
        NS_ASSERT(sys_error);
        if (sys_error < 0) {
            util_snprintf(*buff, maxlen, errmsg_unknownerror, sys_error);
            lmsg = *buff;
        } else {
            /* Locale specific messages would be nice. At least Solaris and AIX
             * provide such a beast, so why not use it?
             */
            util_strerror(sys_error, *buff, maxlen);
            lmsg = *buff;
        }
#endif /* XP_WIN32 */
    }

    if (lmsg) {
        msglen = strlen(lmsg);
    } else {
        msglen = 0;
    }

    /* Copy error message to caller's buffer if it isn't there already */
    if (lmsg != *buff) {
        if (msglen >= maxlen)
            msglen = maxlen - 1;

        memcpy(*buff, lmsg, msglen);
        (*buff)[msglen] = '\0';
    }

    /* Restore the NSPR error state */
    PR_SetError(nscp_error, sys_error);
    if (lmsg > 0)
        PR_SetErrorText(msglen, lmsg);

#ifdef XP_WIN32
    /* NT's FormatMessage() dynamically allocated the msg; free it */
    if (sysmsg)
        LocalFree(sysmsg);
#endif

    return msglen;
}

NSAPI_PUBLIC char *
system_errmsg(void)
{
    char *buff = NULL;

    if (errmsg_key != -1)
        buff = (char *) systhread_getdata(errmsg_key);

    // rmaxwell - This is extremely lame.
    // Allocate a buffer in thread local storage to 
    // hold the last error message.
    if (buff == NULL) {
        buff = (char *) PERM_MALLOC(errbuf_size);

        // If errmsg_key hasn't been initialized yet, we'll leak memory. That's
        // better than saying "unknown early startup error", though.
        if (errmsg_key != -1)
            systhread_setdata(errmsg_key, (void *)buff);
    }

    system_errmsg_fn(&buff, errbuf_size);
    if (buff == 0)
        buff = (char *) errmsg_outofmemory;

    return buff;
}

NSAPI_PUBLIC int
system_rename(const char *oldpath, const char *newpath)
{
    PRStatus status = PR_Rename(oldpath, newpath);

    if (PR_SUCCESS == status)
        return 0;
    else
        return -1;
}

NSAPI_PUBLIC int
system_unlink(const char *path)
{
    return PR_Delete(path)==PR_FAILURE?-1:0;
}

NSAPI_PUBLIC int
system_lseek(SYS_FILE fd, int off, int wh)
{
    PRSeekWhence x;

    switch (wh) {
    case 2:
            x = PR_SEEK_END;
            break;
    case 1:
            x = PR_SEEK_CUR;
            break;
    case 0:
    default:
            x = PR_SEEK_SET;
    }

    return PR_Seek(fd, off, x);
}

NSAPI_PUBLIC int
system_tlock(SYS_FILE fd)
{
#if defined(XP_UNIX)
    // NSPR doesn't handle EDEADLK well
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;

    int rv = fcntl(PR_FileDesc2NativeHandle(fd), F_SETLK, &fl);
    if (rv == -1) {
        NsprError::mapUnixErrno();
        return IO_ERROR;
    }

    return IO_OKAY;
#else
    return PR_TLockFile(fd) == PR_FAILURE ? IO_ERROR : IO_OKAY;
#endif
}

NSAPI_PUBLIC int
system_flock(SYS_FILE fd)
{
#if defined(XP_UNIX)
    // NSPR doesn't handle EDEADLK well
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;

    int rv = fcntl(PR_FileDesc2NativeHandle(fd), F_SETLKW, &fl);
    if (rv == -1) {
        NsprError::mapUnixErrno();
        return IO_ERROR;
    }

    return IO_OKAY;
#else
    return PR_LockFile(fd) == PR_FAILURE ? IO_ERROR : IO_OKAY;
#endif
}

NSAPI_PUBLIC int 
system_ulock(SYS_FILE fd)
{
#if defined(XP_UNIX)
    // NSPR doesn't handle EDEADLK well
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;

    int rv = fcntl(PR_FileDesc2NativeHandle(fd), F_SETLK, &fl);
    if (rv == -1) {
        NsprError::mapUnixErrno();
        return IO_ERROR;
    }

    return IO_OKAY;
#else
    return PR_UnlockFile(fd) == PR_FAILURE ? IO_ERROR : IO_OKAY;
#endif
}

NSAPI_PUBLIC int
file_is_path_abs(const char *path)
{
    if (path) {
#ifdef XP_WIN32
        if (path[0] == '/' || path[0] == '\\') return 1;
        if (isalpha(path[0]) && path[1] == ':') return 1;
#else
        if (path[0] == '/') return 1;
#endif
    }
    return 0;
}

#ifdef XP_WIN32

char* file_canonicalize_path(const char* path)
{
    // There've been way too many security-related bugs with alternate data 
    // streams; refuse to deal with them.
    if (path[0] && path[1] && strchr(path+2, ':')) return 0;

    char* pszPath = (char*)MALLOC(MAX_PATH);
    if (!pszPath) return 0;

    char* pszFilename;
    if (!GetFullPathName(path, MAX_PATH, pszPath, &pszFilename)) {
        FREE(pszPath);
        return 0;
    }

    // Yuck.  Windows path names are usually (but not always) case insenitive.
    // Treat lower case as the canonical form.
    CharLower(pszPath);

    return pszPath;
}

#else

static inline int _matchpathsegment(const char* path, const char* segment)
{
    while (*path && *path == *segment) {
        path++;
        segment++;
    }
    if (*segment) return 0; // Mismatch
    if (*path && *path != '/') return 0; // Mismatch
    return 1; // Match
}

static inline char* _prevpathsegment(char* buffer, char* segment)
{
    while (*segment == '/' && segment > buffer) segment--;
    while (*segment != '/' && segment > buffer) segment--;
    return segment;
}

static inline const char* _nextpathsegment(const char* path)
{
    while (*path && *path != '/') path++;
    while (*path && *path == '/') path++;
    return path;
}

static inline char* _canonicalize(char* buffer, char* out, const char* path)
{
    const char* in = path;

    while (*in == '/') in++;

    while (*in) {
        if (_matchpathsegment(in, "..")) {
            // Backup one path segment
            out = _prevpathsegment(buffer, out);
        } else if (_matchpathsegment(in, ".")) {
            // Do nothing
        } else {
            // Write out path segment
            *out++ = '/';
            while (*in && *in != '/') *out++ = *in++;
        }
        in = _nextpathsegment(in);
    }

    *out = 0;

    return out;
}

NSAPI_PUBLIC char*
file_canonicalize_path(const char* path)
{
    char cwd[1024] = "/";
    if (path[0] != '/') {
        getcwd(cwd, sizeof(cwd));
    }

    char* buffer = (char*)MALLOC(strlen(cwd) + strlen(path) + 2);
    if (buffer) {
        _canonicalize(buffer, _canonicalize(buffer, buffer, cwd), path);
        if (!*buffer) strcpy(buffer, "/");
    }

    return buffer;
}

#endif

NSAPI_PUBLIC char *
file_basename(const char *path)
{
    const char *basename = NULL;
    const char *p = path;
    while (*p) {
        if (file_is_slash(p[0]) && !file_is_slash(p[1]) && p[1] != '\0')
            basename = p + 1;
        p++;
    }

    int len;
    if (basename) {
        len = p - basename;
        while (file_is_slash(basename[len - 1]))
            len--;
    } else {
        basename = "";
        len = 0;
    }

    char *result = (char *) MALLOC(len + 1);
    memcpy(result, basename, len);
    result[len] = '\0';

    return result;
}

NSAPI_PUBLIC int
file_are_files_distinct(SYS_FILE fd1, SYS_FILE fd2)
{
    if (!fd1 || !fd2 || fd1 == SYS_ERROR_FD || fd2 == SYS_ERROR_FD) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1; // Error
    }

    if (fd1 == fd2) return 0; // Same

#ifdef XP_WIN32
    HANDLE hFile1 = (HANDLE)PR_FileDesc2NativeHandle(fd1);
    HANDLE hFile2 = (HANDLE)PR_FileDesc2NativeHandle(fd2);
    if (hFile1 == hFile2) return 0; // Same

    BY_HANDLE_FILE_INFORMATION FileInformation1;
    if (!GetFileInformationByHandle(hFile1, &FileInformation1)) {
        NsprError::mapWin32Error();
        return -1; // Error
    }

    BY_HANDLE_FILE_INFORMATION FileInformation2;
    if (!GetFileInformationByHandle(hFile2, &FileInformation2)) {
        NsprError::mapWin32Error();
        return -1; // Error
    }

    if (FileInformation1.nFileIndexLow != FileInformation2.nFileIndexLow) return 1; // Different
    if (FileInformation1.nFileIndexHigh != FileInformation2.nFileIndexHigh) return 1; // Different
    if (FileInformation1.dwVolumeSerialNumber != FileInformation2.dwVolumeSerialNumber) return 1; // Different
#else
    int osfd1 = (int)PR_FileDesc2NativeHandle(fd1);
    int osfd2 = (int)PR_FileDesc2NativeHandle(fd2);
    if (osfd1 == osfd2) return 0; // Same

    struct stat64 finfo1;
    if (fstat64(osfd1, &finfo1)) {
        NsprError::mapUnixErrno();
        return -1; // Error
    }

    struct stat64 finfo2;
    if (fstat64(osfd2, &finfo2)) {
        NsprError::mapUnixErrno();
        return -1; // Error
    }

    if (finfo1.st_ino != finfo2.st_ino) return 1; // Different
    if (finfo1.st_dev != finfo2.st_dev) return 1; // Different
#endif

    return 0; // Same
}

