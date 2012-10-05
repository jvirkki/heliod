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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include <nspr.h>
#include <private/pprio.h>

#include "netsite.h"

#include "NsprWrap/NsprError.h"
#include "support/EreportableException.h"
#include "base/dbtbase.h"
#include "base/ereport.h"
#include "base/nsassert.h"
#include "base/util.h"
#include "base/date.h"
#include "base/systhr.h"
#include "base/vs.h"
#include "frame/conf.h"
#include "frame/conf_api.h"
#include "httpdaemon/vsmanager.h"
#include "httpdaemon/vsconf.h"

#ifdef XP_WIN32
#include <nt/regparms.h>
#include <nt/messages.h>
#include <process.h>
#include "eventlog.h"
#else
#include <syslog.h>
#endif

struct ErrorLog {
    struct ErrorLog* next;
    char* filename;
    PRLock* lock;
    PRInt32 locked;
    PRFileDesc* fd;
    PRInt32 refcount;
};

struct ErrorCallback {
    struct ErrorCallback* next;
    EreportFunc* fn;
    void* data;
};

static struct passwd *_pwuser = 0;
static ErrorLog* _logServer = 0;
static PRBool _flagInitialized = PR_FALSE;
static PRBool _flagLogAll = PR_FALSE;
static PRBool _flagLogVsId = PR_FALSE;
static PRBool _flagAlwaysReopen = PR_FALSE;
static PRLock* _lockDisaster;
static const char* _msgOutOfMemory = "out of memory";
static char* volatile _fmtTime = NULL;
static const char* _servername = NULL;
static ErrorLog* _listLogs;
static ErrorCallback* _listCallbacks;
static PRLock* _lockLogs;
static int _slotErrorLog = -1;
static PRUintn _keyErrorLog = (PRUintn)-1;
static void (*_fnRotateCallback)(const char* filenameNew, const char* filenameOld);
static date_format_t *_fmtDate = NULL;

static PRBool _canLog[] = {
    PR_TRUE,  // LOG_WARN
    PR_TRUE,  // LOG_MISCONFIG
    PR_TRUE,  // LOG_SECURITY
    PR_TRUE,  // LOG_FAILURE
    PR_TRUE,  // LOG_CATASTROPHE
    PR_TRUE,  // LOG_INFORM
    PR_FALSE, // LOG_VERBOSE
    PR_FALSE, // LOG_FINER
    PR_FALSE, // LOG_FINEST
};

struct {
    int dbt;
    const char *msg;
} _msgDegree[] = {
    DBT_warning_, "warning",         // LOG_WARN
    DBT_config_, "config",           // LOG_MISCONFIG
    DBT_security_, "security",       // LOG_SECURITY
    DBT_failure_, "failure",         // LOG_FAILURE
    DBT_catastrophe_, "catastrophe", // LOG_CATASTROPHE
    DBT_info_, "info",               // LOG_INFORM
    DBT_verbose_, "fine",            // LOG_VERBOSE
    DBT_finer_, "finer",             // LOG_FINER
    DBT_finest_, "finest"            // LOG_FINEST
};

#define FD_SYSLOG ((PRFileDesc*)-1)
#define MAX_TIMEFMT_LEN 256

#ifdef XP_WIN32
#define EVENTLOG_INVALID 0xffff
static WORD _eventDegree[] = {
    EVENTLOG_WARNING_TYPE,     // LOG_WARN
    EVENTLOG_ERROR_TYPE,       // LOG_MISCONFIG
    EVENTLOG_ERROR_TYPE,       // LOG_SECURITY
    EVENTLOG_ERROR_TYPE,       // LOG_FAILURE
    EVENTLOG_ERROR_TYPE,       // LOG_CATASTROPHE
    EVENTLOG_INFORMATION_TYPE, // LOG_INFORM
    EVENTLOG_INVALID           // LOG_VERBOSE
};
#else
static int _syslogDegree[] = {
    LOG_WARNING, // LOG_WARN
    LOG_ERR,     // LOG_MISCONFIG
    LOG_NOTICE,  // LOG_SECURITY
    LOG_ALERT,   // LOG_FAILURE
    LOG_CRIT,    // LOG_CATASTROPHE
    LOG_INFO,    // LOG_INFORM
    LOG_INFO     // LOG_VERBOSE
};
#endif // XP_UNIX

static int _ereport(const VirtualServer* vs, int degree, const char *fmt, ...);
static int _ereport_date_formatter(struct tm *local, char *buffer, size_t size, void *context);
static PRStatus _ereport_open_syslog(void);
static VSInitFunc _ereport_init_vs;
static VSDestroyFunc _ereport_destroy_vs;

//-----------------------------------------------------------------------------
// _ereport_load_strings
//-----------------------------------------------------------------------------

static inline void _ereport_load_strings(void)
{
    static PRBool flagLoadedStrings = PR_FALSE;

    if (!flagLoadedStrings) {
        const char* msg = XP_GetAdminStr(DBT_outofmemory);
        if (msg && *msg) {
            _msgOutOfMemory = msg;
        }

        int i = 0;
        for (i = 0; i < sizeof(_msgDegree) / sizeof(_msgDegree[0]); i++) {
            msg = XP_GetAdminStr(_msgDegree[i].dbt);
            if (msg && *msg) {
                _msgDegree[i].msg = msg;
            }
        }

        flagLoadedStrings = PR_TRUE;
    }
}

//-----------------------------------------------------------------------------
// _ereport_vsprintf
//-----------------------------------------------------------------------------

static inline int _ereport_vsprintf(const VirtualServer* vs, char* str, int strLen, int degree, const char *fmt, va_list args)
{
    int pos;

    // Load localized _msgDegree strings (in case we're being called to report
    // an error prior to ereport_init())
    _ereport_load_strings();

    // Don't log the pid when we're not logging the time (we're probably
    // running interactively)
    char pid[14] = "";
    if (_fmtTime) {
        util_snprintf(pid, sizeof(pid), " (%5u)", getpid());
    }

    if (!_flagLogVsId || !vs) {
        pos = util_snprintf(str, strLen - 1, "%s%s: ",
                            _msgDegree[degree].msg, pid);
    } else {
        pos = util_snprintf(str, strLen - 1, "%s%s %s: ",
                            _msgDegree[degree].msg, pid, vs_get_id(vs));
    }

    pos += util_vsnprintf(&str[pos], strLen - pos - 1, fmt, args);

    pos += util_snprintf(&str[pos], strLen - pos, ENDLINE);

    return pos;
}

//-----------------------------------------------------------------------------
// _ereport_serious
//-----------------------------------------------------------------------------

static inline PRBool _ereport_serious(int degree)
{
    switch (degree) {
    case LOG_MISCONFIG:
    case LOG_SECURITY:
    case LOG_CATASTROPHE:
        return PR_TRUE;
    }

    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// _ereport_open_syslog
//-----------------------------------------------------------------------------

static PRStatus _ereport_open_syslog(void)
{
#ifndef XP_WIN32
    static PRBool flagOpenedSyslog = PR_FALSE;
    static PRBool flagSetIdent = PR_FALSE;
    static char ident[80] = PRODUCT_DAEMON_BIN;

    if (!flagSetIdent && _servername) {
        util_snprintf(ident, sizeof(ident), "%s %s", _servername, PRODUCT_DAEMON_BIN);
        flagSetIdent = PR_TRUE;
        flagOpenedSyslog = PR_FALSE;
    }

    if (!flagOpenedSyslog) {
        openlog(ident, LOG_PID, LOG_DAEMON);
        flagOpenedSyslog = PR_TRUE;
    }
#endif

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// _ereport_open
//-----------------------------------------------------------------------------

static const char* _ereport_open(ErrorLog* log)
{
    const char* errmsg = 0;

    if (!log->fd) {
        if (!strcmp(log->filename, EREPORT_SYSLOG)) {
            // Logging to syslog
            _ereport_open_syslog();
            log->fd = FD_SYSLOG;
        } else if (!strcmp(log->filename, "-")) {
            log->fd = PR_STDERR;
        } else {
            // Open the file
            PRFileDesc* fd;
            fd = system_fopenWA(log->filename);
            if (fd != SYS_ERROR_FD) {
                file_setinherit(fd, 0);
#ifdef XP_UNIX
                if (_pwuser)
                    chown(log->filename, _pwuser->pw_uid, _pwuser->pw_gid);
#endif
            } else {
                // Record an error
                errmsg = system_errmsg();
                fd = 0;
            }
            log->fd = fd;
        }
    }

    return errmsg;
}

//-----------------------------------------------------------------------------
// _ereport_reopen
//-----------------------------------------------------------------------------

static const char* _ereport_reopen(ErrorLog *log)
{
    const char* errmsg = 0;

    if (log->fd != FD_SYSLOG && log->fd != PR_STDERR) {
        if (log->fd) {
            system_fclose(log->fd);
            log->fd = 0;
        }
        errmsg = _ereport_open(log);
    }

    return errmsg;
}

//-----------------------------------------------------------------------------
// _ereport_lock
//-----------------------------------------------------------------------------

static inline ErrorLog* _ereport_lock(ErrorLog* log)
{
    if (log) {
        PR_AtomicIncrement(&log->locked);
        PR_Lock(log->lock);
    }
    return log;
}

//-----------------------------------------------------------------------------
// _ereport_unlock
//-----------------------------------------------------------------------------

static inline ErrorLog* _ereport_unlock(ErrorLog* log)
{
    if (log) {
        PR_Unlock(log->lock);
        PR_AtomicDecrement(&log->locked);
    }
    return log;
}

//-----------------------------------------------------------------------------
// _ereport_get_log
//-----------------------------------------------------------------------------

static ErrorLog* _ereport_get_log(const char* filename, ErrorLog* log, PRBool flagLogFailure)
{
    PR_ASSERT(filename);
    PR_ASSERT(strlen(filename));

    const char* errmsg = 0;
    char* path = 0;

    if (log && !strcmp(log->filename, filename)) {
        // Get exclusive access to the log
        _ereport_lock(log);

    } else {
        // Get canonical filename
        if (!strcmp(filename, EREPORT_SYSLOG)) {
            path = STRDUP(filename);
        } else {
            path = ereport_abs_filename(filename);
        }

        if (log && !strcmp(log->filename, path)) {
            // Get exclusive access to the log
            _ereport_lock(log);
        } else {
            PR_Lock(_lockLogs);

            // Look for VS's error log in our list
            log = _listLogs;
            while (log) {
                if (!strcmp(log->filename, path)) {
                    // Get exclusive access to the log
                    _ereport_lock(log);
                    break;
                }
                log = log->next;
            }

            // If we couldn't find the log...
            if (!log) {
                // Create a new ErrorLog entry for this VS's error log
                log = (ErrorLog*)PERM_MALLOC(sizeof(*log) + strlen(path) + 1);
                if (log) {
                    // Setup the ErrorLog entry
                    log->filename = (char*)(log + 1);
                    strcpy(log->filename, path);
                    log->fd = 0;
                    log->refcount = 0;
                    log->locked = 0;
                    log->lock = PR_NewLock();
                    if (log->lock) {
                        // Get exclusive access to the log
                        _ereport_lock(log);

                        // Add ErrorLog entry to our list
                        log->next = _listLogs;
                        _listLogs = log;
                    } else {
                        // Could not create mutex
                        PERM_FREE(log);
                        log = 0;
                        errmsg = system_errmsg();
                    }
                } else {
                    // Could not allocate ErrorLog
                    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                    errmsg = system_errmsg();
                }
            }

            PR_Unlock(_lockLogs);
        }
    }

    // Open the log file if it isn't open already
    if (log && !log->fd) {
        errmsg = _ereport_open(log);
    }

    // Log errors opening the VS error log to the server error log
    if (errmsg && flagLogFailure) {
        PR_ASSERT(!log || !log->fd);
        _ereport((const VirtualServer*)0, LOG_FAILURE, XP_GetAdminStr(DBT_ereportErrorOpeningErrorLog), filename, errmsg);
    }

    if (path) FREE(path);

    return log;
}

static ErrorLog* _ereport_get_log(const char* filename)
{
    return _ereport_get_log(filename, 0, PR_FALSE);
}

//-----------------------------------------------------------------------------
// _ereport_get_vs_log
//-----------------------------------------------------------------------------

static inline ErrorLog* _ereport_get_vs_log(const VirtualServer* vs, ErrorLog* logServer)
{
    if (_slotErrorLog == -1) return 0;

    // Check for an ErrorLog* entry
    ErrorLog* log = (ErrorLog*)vs_get_data(vs, _slotErrorLog);
    if (!log) return 0;

    // To prevent deadlock, don't return the server log as a VS log
    if (log == logServer) return 0;

    // Check for a valid fd
    if (log->fd) {
        _ereport_lock(log);
        if (log->fd) return log;
        _ereport_unlock(log);
    }

    PR_ASSERT(vs->getLogFile() != NULL);
    return _ereport_get_log(*vs->getLogFile(), log, PR_FALSE);
}

//-----------------------------------------------------------------------------
// _ereport_get_server_log
//-----------------------------------------------------------------------------

static inline ErrorLog* _ereport_get_server_log()
{
    ErrorLog* log = _logServer;
    if (log) {
        _ereport_lock(log);
        if (!log->fd) _ereport_open(log);
        if (log->fd) return log;
        _ereport_unlock(log);
    }
    return 0;
}

//-----------------------------------------------------------------------------
// _ereport_call_callbacks
//-----------------------------------------------------------------------------

static inline int _ereport_call_callbacks(ErrorCallback *listCallbacks, const VirtualServer* vs, int degree, const char *formatted, int formattedlen, const char *raw, int rawlen)
{
    int rv = 0;

    ErrorCallback *callback = listCallbacks;
    while (callback) {
        // Invoke this callback
        if (callback->fn)
            rv |= (*callback->fn)(vs, degree, formatted, formattedlen, raw, rawlen, callback->data);
        callback = callback->next;
    }

    return rv;
}

//-----------------------------------------------------------------------------
// _ereport_log
//-----------------------------------------------------------------------------

static PRStatus _ereport_log(PRFileDesc* fd, int degree, const char* errstr, int len, int posError)
{
#ifdef XP_WIN32
    WORD eventLog = _eventDegree[degree];
#endif
    PRStatus rv = PR_SUCCESS;

    if (fd == FD_SYSLOG) {
#ifdef XP_WIN32
        // Write to Win32 event viewer
        if (eventLog != EVENTLOG_INVALID) {
	    LogErrorEvent(NULL, eventLog, 0, MSG_EREPORT, server_id, &errstr[posError]);
        }
#else
        // Write to Unix syslog
        _ereport_open_syslog();
        syslog(_syslogDegree[degree], "%s", &errstr[posError]);
#endif
    } else if (fd) {
        // Write to file
        if (system_fwrite_atomic(fd, (char *)/* XXX */errstr, len) != IO_OKAY) {
            rv = PR_FAILURE;
        }

#ifdef XP_WIN32
        // Write "serious" errors to Win32 event viewer
        if (_ereport_serious(degree)) {
	    LogErrorEvent(NULL, eventLog, 0, MSG_EREPORT, server_id, &errstr[posError]);
        }
#endif
    }

    return rv;
}

static void _ereport_log(const VirtualServer* vs, int degree, const char* errstr, int len, int posError)
{
    if (!_flagInitialized) return;

    // Call thread-specific callbacks
    if (_keyErrorLog != (PRUintn)-1) {
        ErrorCallback *listCallbacks = (ErrorCallback*)PR_GetThreadPrivate(_keyErrorLog);
        if (_ereport_call_callbacks(listCallbacks, vs, degree, errstr, len, &errstr[posError], len - posError))
            return;
    }

    // Call global callbacks
    if (_ereport_call_callbacks(_listCallbacks, vs, degree, errstr, len, &errstr[posError], len - posError))
        return;

    // Get access to the server error log
    ErrorLog* logServer = _ereport_get_server_log();
    if (logServer && _flagAlwaysReopen) {
        _ereport_reopen(logServer);
    }
    PRFileDesc* fdServer = logServer ? logServer->fd : 0;
    PR_ASSERT(!logServer || logServer->refcount);

    // Get access to the VS error log
    ErrorLog* logVs = vs ? _ereport_get_vs_log(vs, logServer) : 0;
    if (logVs && _flagAlwaysReopen) {
        _ereport_reopen(logVs);
    }
    PRFileDesc* fdVs = logVs ? logVs->fd : 0;
    PR_ASSERT(!logVs || logVs->refcount);

    // Write to VS error log
    if (fdVs) {
        if (_ereport_log(fdVs, degree, errstr, len, posError) != PR_SUCCESS) {
            system_fclose(logVs->fd);
            logVs->fd = 0;
        }
    }

    // Write to system error log
    if (!fdVs || (_flagLogAll && (fdServer != fdVs))) {
        if (_ereport_log(fdServer, degree, errstr, len, posError) != PR_SUCCESS) {
            system_fclose(logServer->fd);
            logServer->fd = 0;
        }
    }

    // Release access to server and VS error logs
    if (logVs) _ereport_unlock(logVs);
    if (logServer) _ereport_unlock(logServer);
}

//-----------------------------------------------------------------------------
// _ereport_v
//-----------------------------------------------------------------------------

static int _ereport_v(const VirtualServer* vs, int degree, const char *fmt, va_list args)
{
    // Private interface: negative degrees force a message to be logged with
    // the corresponding positive degree number, even if that degree would
    // normally be suppressed
    if (degree < 0)
        degree = -degree;

    if (degree >= sizeof(_canLog) / sizeof(_canLog[0]))
        degree = LOG_CATASTROPHE;

    char errstr[MAX_ERROR_LEN];
    int len = 0;

    // Format timestamp
    if (_fmtDate) {
        len += date_current_formatted(_fmtDate, errstr, sizeof(errstr));
        if (len > 0)
            errstr[len++] = ' ';
    }

    // Format error message
    int posError = len;
    len += _ereport_vsprintf(vs, &errstr[len], sizeof(errstr) - len, degree, fmt, args);

    // Log message
    if (_flagInitialized) {
        _ereport_log(vs, degree, errstr, len, posError);
    } else {
        // Server log is not yet open, so log to the console and syslog
        AdminFprintf(stderr, "%s", errstr);
        _ereport_log(FD_SYSLOG, degree, errstr, len, posError);
    }

    return IO_OKAY;
}

//-----------------------------------------------------------------------------
// _ereport
//-----------------------------------------------------------------------------

static int _ereport(const VirtualServer* vs, int degree, const char *fmt, ...)
{
    va_list args;
    int rv;
    va_start(args, fmt);
    rv = _ereport_v(vs, degree, fmt, args);
    va_end(args);
    return rv;
}

//-----------------------------------------------------------------------------
// _ereport_date_formatter
//-----------------------------------------------------------------------------

static int _ereport_date_formatter(struct tm *local, char *buffer, size_t size, void *context)
{
    const char *fmt = _fmtTime;
    if (fmt)
        return util_strftime(buffer, fmt, local);
    else
        return 0;
}

//-----------------------------------------------------------------------------
// _ereport_rotate
//-----------------------------------------------------------------------------

static const char* _ereport_rotate(ErrorLog* log, const char* archive)
{
    const char* errmsg = 0;

#ifdef XP_WIN32
    // We can't MoveFileEx a file that's open
    system_fclose(log->fd);
    log->fd = 0;

    // Perform the rotation
    if (!MoveFileEx(log->filename, archive, MOVEFILE_REPLACE_EXISTING)) {
        NsprError::mapWin32Error();
        errmsg = system_errmsg();
    }
#else
    // We need to be concerned about multiple processes trying to rotate the
    // same error log at roughly the same time

    // Keep a private copy of the error log fd
    PRFileDesc* fd = log->fd;

    // Serialize processes
    PRBool flagFlocked = PR_TRUE;
    if (system_flock(fd) == IO_ERROR) {
        flagFlocked = PR_FALSE;
    }

    // Reopen the error log by name
    log->fd = 0;
    errmsg = _ereport_open(log);

    // If the new fd and old fd refer to the same file (that is, the file 
    // referenced by the old fd is still named log->filename)...
    if (!file_are_files_distinct(log->fd, fd)) {
        // Perform the rotation
        if (rename(log->filename, archive)) {
            NsprError::mapUnixErrno();
            errmsg = system_errmsg();
        }
    }

    // End cross-process serialization
    if (flagFlocked) {
        system_ulock(fd);
    }

    // Close these fds as they reference the archived error log
    system_fclose(fd);
    fd = 0;
    if (log->fd) {
        system_fclose(log->fd);
        log->fd = 0;
    }
#endif

    // Reopen the log file if all went well
    if (!errmsg) {
        errmsg = _ereport_open(log);
    }

    return errmsg;
}

//-----------------------------------------------------------------------------
// _ereport_init_vs
//-----------------------------------------------------------------------------

static int _ereport_init_vs(VirtualServer* incoming, const VirtualServer* current)
{
    // Does VS specify an error log filename?
    if (!incoming->getLogFile()) {
        // No error log for this VS
        vs_set_data(incoming, &_slotErrorLog, 0);
        return REQ_PROCEED;
    }

    // Update the cached ErrorLog*
    ErrorLog* log = current ? (ErrorLog*)vs_get_data(current, _slotErrorLog) : 0;
    log = _ereport_get_log(*incoming->getLogFile(), log, PR_TRUE);
    if (log) {
        log->refcount++;
        _ereport_unlock(log);
        vs_set_data(incoming, &_slotErrorLog, log);
        return REQ_PROCEED;
    }

    return REQ_ABORTED;
}

//-----------------------------------------------------------------------------
// _ereport_destroy_vs
//-----------------------------------------------------------------------------

static void _ereport_destroy_vs(VirtualServer* outgoing)
{
    ErrorLog* log = (ErrorLog*)vs_get_data(outgoing, _slotErrorLog);
    if (log) {
        _ereport_lock(log);
        log->refcount--;
        if (log->refcount < 1) {
            system_fclose(log->fd);
            log->fd = 0;
        }
        _ereport_unlock(log);
    }
}

//-----------------------------------------------------------------------------
// _ereport_register_cb
//-----------------------------------------------------------------------------

static int _ereport_register_cb(ErrorCallback** listCallbacks, EreportFunc* ereport_func, void *data)
{
    ErrorCallback *callback;
    callback = (ErrorCallback*)PERM_MALLOC(sizeof(*callback));
    if (!callback)
        return -1;

    callback->fn = ereport_func;
    callback->data = data;
    callback->next = *listCallbacks;
    *listCallbacks = callback;

    return 0;
}

//-----------------------------------------------------------------------------
// ereport_set_*
//-----------------------------------------------------------------------------

void ereport_set_servername(const char *name)
{
    if (name)
        _servername = PERM_STRDUP(name);
}

void ereport_set_logvsid(PRBool b)
{
    _flagLogVsId = b;
}

void ereport_set_logall(PRBool b)
{
    _flagLogAll = b;
}

void ereport_set_alwaysreopen(PRBool b)
{
    _flagAlwaysReopen = b;
}

void ereport_set_timefmt(const char* timeFmt)
{
    PERM_FREE(_fmtTime);
    if (timeFmt)
        _fmtTime = PERM_STRDUP(timeFmt);
    else
        _fmtTime = NULL;
}

void ereport_set_rotate_callback(void (*fn)(const char* filenameNew, const char* filenameOld))
{
    _fnRotateCallback = fn;
}

void ereport_set_degree(int degree)
{
    // Log levels in order of increasing severity (decreasing verbosity)
    int degrees[] = {
        LOG_FINEST,
        LOG_FINER,
        LOG_VERBOSE,
        LOG_INFORM,
        LOG_WARN,
        LOG_FAILURE,
        LOG_MISCONFIG,
        LOG_SECURITY,
        LOG_CATASTROPHE
    };

    for (int i = 0; i < sizeof(degrees) / sizeof(degrees[0]); i++) {
        // If this index corresponds to the specified degree...
        if (degrees[i] == degree) {
            // Enable/disable each NSAPI degree based on the specified degree
            for (int j = 0; j < sizeof(degrees) / sizeof(degrees[0]); j++)
                _canLog[degrees[j]] = (j >= i);
        }
    }
}

//-----------------------------------------------------------------------------
// ereport_level2degree
//-----------------------------------------------------------------------------

int ereport_level2degree(const char *level, int defdegree)
{
    if (level) {
        int i;

        for (i = 0; i < sizeof(_msgDegree) / sizeof(_msgDegree[0]); i++) {
            if (!strcasecmp(_msgDegree[i].msg, level))
                return i;
        }
    }

    return defdegree;
}

//-----------------------------------------------------------------------------
// ereport_can_log
//-----------------------------------------------------------------------------

PRBool ereport_can_log(int degree)
{
    // Private interface: negative degrees force a message to be logged with
    // the corresponding positive degree number, even if that degree would
    // normally be suppressed
    if (degree < 0)
        return PR_TRUE;

    if (degree >= sizeof(_canLog) / sizeof(_canLog[0]))
        degree = LOG_CATASTROPHE;

    return _canLog[degree];
}

//-----------------------------------------------------------------------------
// ereport_getfd
//-----------------------------------------------------------------------------

NSAPI_PUBLIC SYS_FILE ereport_getfd(void)
{
    // This interface is broken, as we reserve the right to open and close the
    // server error log at will.  Therefore, never tell the caller the fd.
    ereport(LOG_WARN, XP_GetAdminStr(DBT_ereportFunctionUnsupported), "ereport_getfd");
    return SYS_ERROR_FD;
}

//-----------------------------------------------------------------------------
// ereport_v
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int ereport_v(int degree, const char *fmt, va_list args)
{
    if (!ereport_can_log(degree))
        return IO_OKAY;

    NsprError error;
    error.save();

    int rv = _ereport_v(conf_get_vs(), degree, fmt, args);

    error.restore();

    return rv;
}

//-----------------------------------------------------------------------------
// ereport
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int ereport(int degree, const char *fmt, ...)
{
    if (!ereport_can_log(degree))
        return IO_OKAY;

    NsprError error;
    error.save();

    va_list args;
    va_start(args, fmt);
    int rv = _ereport_v(conf_get_vs(), degree, fmt, args);
    va_end(args);

    error.restore();

    return rv;
}

//-----------------------------------------------------------------------------
// ereport_request
//-----------------------------------------------------------------------------

int ereport_request(Request* rq, int degree, const char *fmt, ...)
{
    if (!ereport_can_log(degree))
        return IO_OKAY;

    NsprError error;
    error.save();

    va_list args;
    va_start(args, fmt);
    int rv = _ereport_v(request_get_vs(rq), degree, fmt, args);
    va_end(args);

    error.restore();

    return rv;
}

//-----------------------------------------------------------------------------
// ereport_exception
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int ereport_exception(const EreportableException& e)
{
    const char *messageID = e.getMessageID();
    const char *description = e.getDescription();
    if (messageID && description) {
        ereport(e.getDegree(), "%s: %s", messageID, description);
    } else {
        ereport(e.getDegree(), "%s", messageID ? messageID : description);
    }
    return IO_OKAY;
}

//-----------------------------------------------------------------------------
// ereport_abs_filename
//-----------------------------------------------------------------------------

char *ereport_abs_filename(const char *filename)
{
    // Special case for "-" (stdout/stderr)
    if (!strcmp(filename, "-"))
        return STRDUP(filename);

    return file_canonicalize_path(filename);
}

//-----------------------------------------------------------------------------
// ereport_init
//-----------------------------------------------------------------------------

NSAPI_PUBLIC char *ereport_init(const char *err_fn, const char *email, struct passwd *pwuser, const char *version, int restarted)
{
    if (_flagInitialized) return 0;

    _pwuser = pwuser;

    _lockLogs = PR_NewLock();
    _lockDisaster = PR_NewLock();
    if (!_lockLogs || !_lockDisaster) {
        return STRDUP("can't create lock");
    }

    // Preload localized strings
    _ereport_load_strings();

    // Setup per-VS error log handling
    _slotErrorLog = vs_alloc_slot();
    vs_register_cb(_ereport_init_vs, _ereport_destroy_vs);

    // Setup per-thread error log handling
    PR_NewThreadPrivateIndex(&_keyErrorLog, NULL);

    // Setup default time
    _fmtTime = PERM_STRDUP(ERR_TIMEFMT);

    // Register error log time stamp formatter
    _fmtDate = date_register_local_formatter(_ereport_date_formatter, MAX_TIMEFMT_LEN, NULL);

    // Open the server's error log
    ErrorLog* log = _ereport_get_log(err_fn);
    if (log) {
        log->refcount++;
        _ereport_unlock(log);
        _logServer = log;
    }
    if (!_logServer || !_logServer->fd) {
        char err[MAGNUS_ERROR_LEN];
        util_snprintf(err, MAGNUS_ERROR_LEN, "can't open error log %s", err_fn);
        return STRDUP(err);
    }

    setlocale(LC_ALL, "");

    _flagInitialized = PR_TRUE;

    char prodString[256];
    sprintf(prodString, "%s %s B%s", PRODUCT_ID, PRODUCT_FULL_VERSION_ID, BUILD_NUM);

    AdminFprintf(stderr, "%s\n", prodString);

    if (restarted == 0) {
        ereport(LOG_INFORM, XP_GetAdminStr(DBT_SBS_), prodString);
    } else {
        ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_SBS_), prodString);
    }
    if (strcasecmp(BUILD_NUM, version)) {
        ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_netscapeExecutableAndSharedLibra_));
        ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_executableVersionIsS_), version);
        ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_sharedLibraryVersionIsS_), BUILD_NUM);
    }

    // Initialize thread-specific error handling
    system_errmsg_init();

    return 0;
}

//-----------------------------------------------------------------------------
// ereport_terminate
//-----------------------------------------------------------------------------

NSAPI_PUBLIC void ereport_terminate(void)
{
    if (!_flagInitialized) return;

    PR_Lock(_lockLogs);

    // Close all error logs
    PRBool flagCloseSyslog = PR_FALSE;
    ErrorLog* log = _listLogs;
    while (log) {
        _ereport_lock(log);
        if (log->fd) {
            if (log->fd == FD_SYSLOG) {
                flagCloseSyslog = PR_TRUE;
            } else if (log->fd != PR_STDERR) {
                system_fclose(log->fd);
                log->fd = 0;
            }
        }
        _ereport_unlock(log);
        log = log->next;
    }

    PR_Unlock(_lockLogs);

#ifdef XP_UNIX
    if (flagCloseSyslog) closelog();
#endif
}

//-----------------------------------------------------------------------------
// ereport_rotate
//-----------------------------------------------------------------------------

void ereport_rotate(const char* ext)
{
    if (!_flagInitialized) return;

    int lenExt = strlen(ext);

    PR_Lock(_lockLogs);

    // Rotate each error log
    ErrorLog* log = _listLogs;
    while (log) {
        const char* errmsg = 0;

        // Build the archive filename
        int lenFilename = strlen(log->filename);
        char* archive = (char*)MALLOC(lenFilename + lenExt + 1);
        if (archive) {
            memcpy(archive, log->filename, lenFilename);
            memcpy(archive + lenFilename, ext, lenExt + 1);

            _ereport((const VirtualServer*)0, LOG_VERBOSE, "Rotating error log %s (0x%08X)", log->filename, log->fd);

            // Rotate the error log
            _ereport_lock(log);
            if (log->fd && log->fd != FD_SYSLOG && log->fd != PR_STDERR) {
                errmsg = _ereport_rotate(log, archive);
            }
            _ereport_unlock(log);

            if (errmsg) {
                _ereport((const VirtualServer*)0, LOG_FAILURE, XP_GetAdminStr(DBT_ereportErrorRenamingErrorLog), log->filename, archive, errmsg);
            } else if (_fnRotateCallback) {
                (*_fnRotateCallback)(log->filename, archive);
            }

            FREE(archive);
        }

        log = log->next;
    }

    PR_Unlock(_lockLogs);
}

//-----------------------------------------------------------------------------
// ereport_reopen
//-----------------------------------------------------------------------------

void ereport_reopen(void)
{
    if (!_flagInitialized) return;

    PR_Lock(_lockLogs);

    // Close and reopen each error log
    ErrorLog* log = _listLogs;
    while (log) {
        const char* errmsg;

        _ereport_lock(log);
        errmsg = _ereport_reopen(log);
        _ereport_unlock(log);

        if (errmsg) {
            _ereport((const VirtualServer*)0, LOG_FAILURE, XP_GetAdminStr(DBT_ereportErrorReopeningErrorLog), log->filename, errmsg);
        }

        log = log->next;
    }

    PR_Unlock(_lockLogs);
}

//-----------------------------------------------------------------------------
// _ereport_hideous_disaster
//-----------------------------------------------------------------------------

static void _ereport_hideous_disaster(const char *fmt, va_list args)
{
    // Log a disaster that happened before ereport initialization or during
    // disaster handling.  Don't use PR_fprintf as it can call malloc.

    char errstr[MAX_ERROR_LEN];
    int len;
    len = util_snprintf(errstr, sizeof(errstr), "%s: ", _msgDegree[LOG_CATASTROPHE].msg);
    len += util_vsnprintf(errstr + len, sizeof(errstr) - len, fmt, args);

    PR_Write(PR_STDERR, errstr, len);
    PR_Write(PR_STDERR, ENDLINE, sizeof(ENDLINE) - 1);
}

//-----------------------------------------------------------------------------
// ereport_disaster
//-----------------------------------------------------------------------------

void ereport_disaster(int degree, const char *fmt, ...)
{
    // We're walking on eggshells.
    //
    // We get called when a system allocator was unable to allocate memory or
    // something else went hideously wrong.  In order to be useful, we must
    // avoid allocating memory ourselves and be very careful about blocking on
    // locks.
    //
    // If we get called as the result of a signal such as SIGSEGV, there's a
    // reasonable chance we'll die a horrible death during our processing.
    // That's no big loss, though, as the process was about to die anyway.

    va_list args;
    ErrorLog* log = _logServer;
    static PRBool flagBusy = PR_FALSE;
    PRBool flagAlwaysReopen = _flagAlwaysReopen;
    int retries;

    // Did disaster happen before ereport initialization?
    if (!_flagInitialized || !_lockDisaster || !log || !log->fd) goto ereport_hideous_disaster;

    // Avoid recursive ereport() calls as deadlock would likely result
    for (retries = 500; retries && flagBusy; retries--) {
        systhread_sleep(10);
    }
    if (!retries) goto ereport_hideous_disaster;
    for (retries = 500; retries && log->locked; retries--) {
        systhread_sleep(10);
    }
    if (!retries) goto ereport_hideous_disaster;

    // One disaster at a time, please
    PR_Lock(_lockDisaster);
    flagBusy = PR_TRUE;

    // Reopening the log file would require memory allocation; don't do that!
    _flagAlwaysReopen = PR_FALSE;

    // Try to log the disaster.  We use a NULL VirtualServer * as looking up
    // the thread's VirtualServer * can trigger a malloc from libpthread or
    // NSPR.
    va_start(args, fmt);
    _ereport_v(NULL, degree, fmt, args);
    va_end(args);

    // Restore the old _flagAlwaysReopen value
    _flagAlwaysReopen = flagAlwaysReopen;

    flagBusy = PR_FALSE;
    PR_Unlock(_lockDisaster);

    return;

ereport_hideous_disaster:
    va_start(args, fmt);
    _ereport_hideous_disaster(fmt, args);
    va_end(args);
}

//-----------------------------------------------------------------------------
// ereport_outofmemory
//-----------------------------------------------------------------------------

void ereport_outofmemory(void)
{
    ereport_disaster(LOG_CATASTROPHE, "%s", _msgOutOfMemory);
}

//-----------------------------------------------------------------------------
// ereport_register_cb
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int ereport_register_cb(EreportFunc* ereport_func, void *data)
{
    // Do not call after creating threads that may call ereport()
    return _ereport_register_cb(&_listCallbacks, ereport_func, data);
}

//-----------------------------------------------------------------------------
// ereport_register_thread_cb
//-----------------------------------------------------------------------------

NSAPI_PUBLIC int ereport_register_thread_cb(EreportFunc* ereport_func, void *data)
{
    ErrorCallback *listCallbacks = (ErrorCallback*)PR_GetThreadPrivate(_keyErrorLog);

    // This callback will be called for all logging events on the current thread
    int rv = _ereport_register_cb(&listCallbacks, ereport_func, data);

    PR_SetThreadPrivate(_keyErrorLog, listCallbacks);

    return rv;
}
