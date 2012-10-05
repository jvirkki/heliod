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
 * control.cpp: Control functions for use in NSAPI expressions
 * 
 * Chris Elving
 */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef XP_WIN32
#include <io.h>
#ifndef R_OK
#define R_OK 4
#include <aclapi.h>
#endif
#else
#include <unistd.h>
#endif

#include "netsite.h"
#include "support/GenericVector.h"
#include "threadpool/nstp.h"
#include "NsprWrap/NsprError.h"
#include "time/nstime.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/uuid.h"
#include "base/daemon.h"
#include "frame/log.h"
#include "frame/req.h"
#include "frame/expr.h"
#include "frame/func.h"
#include "frame/httpact.h"
#include "safs/cgi.h"
#include "safs/child.h"
#include "safs/control.h"
#include "safs/dbtsafs.h"


/*
 * LOOKUP is the name of the lookup control function.
 */
static const char LOOKUP[] = "lookup";

/*
 * LOOKUP_MAX_LINE is the maximum length of a line in a lookup file.
 */
static const int LOOKUP_MAX_LINE = 4096;

/*
 * LookupStruct records the contents of a lookup file.
 */
struct LookupStruct {
    PRLock *lock;
    unsigned hval;
    char *filename;
    time_t mtime;
    time_t checked;
    pblock *pb;
};

/*
 * lookup_filename_* defines a hash table that contains LookupStruct pointers
 * hashed by filename.
 */
static const int lookup_filename_hmask = 0x1f;
static const int lookup_filename_hsize = lookup_filename_hmask + 1;
static struct LookupBucket {
    PRLock *lock;
    PtrVector<LookupStruct> list;
} lookup_filename_ht[lookup_filename_hsize];

/*
 * PIPE_READ_INDEX is the index of the read end of a pipe's PRFileDesc * in a
 * PRFileDesc * array.
 */
static const int PIPE_READ_INDEX = 0;

/*
 * PIPE_WRITE_INDEX is the index of the write end of a pipe's PRFileDesc * in a
 * PRFileDesc * array.
 */
static const int PIPE_WRITE_INDEX = 1;

/*
 * EXTERNAL is the name of the external control function.
 */
static const char EXTERNAL[] = "external";

/*
 * EXTERNAL_MAX_LINE is the maximum length of a response line from an external
 * program.
 */
static const int EXTERNAL_MAX_LINE = 4096;

/*
 * ExternalStruct records information about an external program.
 */
struct ExternalStruct {
    PRLock *lock;
    unsigned hval;
    char *path;
    PRFileDesc *in;
    PRFileDesc *out;
    Child *child;
};

/*
 * ExternalWork is used to enqueue work for external_work_fn on a native
 * thread.
 */
typedef struct NSTPWorkArg_s {
    ExternalStruct *external;
    Session *sn;
    Request *rq;

    struct {
        const char *p;
        int len;
    } input;

    struct {
        char buf[EXTERNAL_MAX_LINE];
    } output;

    PRStatus rv;
} ExternalWork;

/*
 * external_path_* defines a hash table that contains ExternalStruct pointers
 * hashed by filename.
 */
static const int external_path_hmask = 0x1f;
static const int external_path_hsize = external_path_hmask + 1;
static struct ExternalBucket {
    PRLock *lock;
    PtrVector<ExternalStruct> list;
} external_path_ht[external_path_hsize];

/*
 * OPERAND_RESULT evaluates an ExpressionFunc's operand.  The result, which may
 * be an error result, is assigned to const Result *result.  If the operand is
 * missing or there are multiple arguments, an error is returned.
 */
#define OPERAND_RESULT(args, sn, rq, result)       \
    const Result *result;                          \
    {                                              \
        int _n = args_length(args);                \
        if (_n < 1)                                \
            return result_not_enough_args(sn, rq); \
        if (_n > 1)                                \
            return result_too_many_args(sn, rq);   \
                                                   \
        const Expression *_e = args_get(args, 0);  \
                                                   \
        result = result_expr(sn, rq, _e);          \
    }

/*
 * OPERAND_INTEGER attempts to evaluate an ExpressionFunc's operand as an
 * integer.  If the operand is successfully evaluated, the result is assigned
 * to PRInt64 integer.  If the operand is missing, there are multiple
 * arguments, or an error is encountered evaluating the operand, an error is
 * returned.
 */
#define OPERAND_INTEGER(args, sn, rq, integer)   \
    PRInt64 integer;                             \
    {                                            \
        OPERAND_RESULT(args, sn, rq, _r);        \
                                                 \
        if (result_is_error(_r))                 \
            return _r;                           \
                                                 \
        integer = result_as_integer(sn, rq, _r); \
    }

/*
 * OPERAND_STRING attempts to evaluate an ExpressionFunc's operand as a
 * constant string.  If the operand is successfully evaluated, the result is
 * assigned to the string struct.  If the operand is missing, there are
 * multiple arguments, or an error is encountered evaluating the operand, an
 * error is returned.
 */
#define OPERAND_STRING(args, sn, rq, string)                        \
    struct {                                                        \
        const char *p;                                              \
        int len;                                                    \
    } string;                                                       \
    {                                                               \
        OPERAND_RESULT(args, sn, rq, _r);                           \
                                                                    \
        if (result_is_error(_r))                                    \
            return _r;                                              \
                                                                    \
        result_as_const_string(sn, rq, _r, &string.p, &string.len); \
    }

static ExpressionFunc control_dashd;
static ExpressionFunc control_dashe;
static ExpressionFunc control_dashf;
static ExpressionFunc control_dashl;
static ExpressionFunc control_dashr;
static ExpressionFunc control_dashs;
static ExpressionFunc control_dashu;
static ExpressionFunc control_defined;
static ExpressionFunc control_length;
static ExpressionFunc control_lc;
static ExpressionFunc control_uc;
static ExpressionFunc control_abs;
static ExpressionFunc control_httpdate;
static ExpressionFunc control_choose;
static ExpressionFunc control_escape;
static ExpressionFunc control_unescape;
static ExpressionFunc control_lookup;
static ExpressionFunc control_external;
static ExpressionFunc control_uuid;
static ExpressionFunc control_atime;
static ExpressionFunc control_ctime;
static ExpressionFunc control_mtime;
static ExpressionFunc control_owner;


/* ---------------------------- control_dashd ----------------------------- */

static const Result *control_dashd(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    NSFCFileInfo *finfo;
    PRStatus rv = request_info_path(operand.p, rq, &finfo);
    PRBool dir = (rv == PR_SUCCESS && finfo->pr.type == PR_FILE_DIRECTORY);

    log_error(LOG_FINEST, "-d", sn, rq, "-d \"%s\" = %d", operand.p, dir);

    return result_bool(sn, rq, dir);
}


/* ---------------------------- control_dashe ----------------------------- */

static const Result *control_dashe(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    PRStatus rv = request_info_path(operand.p, rq, NULL);
    PRBool exists = (rv == PR_SUCCESS);

    log_error(LOG_FINEST, "-e", sn, rq, "-e \"%s\" = %d", operand.p, exists);

    return result_bool(sn, rq, exists);
}


/* ---------------------------- control_dashf ----------------------------- */

static const Result *control_dashf(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    NSFCFileInfo *finfo;
    PRStatus rv = request_info_path(operand.p, rq, &finfo);
    PRBool file = (rv == PR_SUCCESS && finfo->pr.type == PR_FILE_FILE);

    log_error(LOG_FINEST, "-f", sn, rq, "-f \"%s\" = %d", operand.p, file);

    return result_bool(sn, rq, file);
}


/* ---------------------------- control_dashl ----------------------------- */

static const Result *control_dashl(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

#if !defined(_FILE_OFFSET_BITS) || (_FILE_OFFSET_BITS == 64)
    struct stat finfo;
    int rv = system_lstat(operand.p, &finfo);
#else
    struct stat64 finfo;
    int rv = lstat64(operand.p, &finfo);
#endif

    PRBool link = (rv == 0 && S_ISLNK(finfo.st_mode));

    log_error(LOG_FINEST, "-l", sn, rq, "-l \"%s\" = %d", operand.p, link);

    return result_bool(sn, rq, link);
}


/* ---------------------------- control_dashr ----------------------------- */

static const Result *control_dashr(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    int rv = access(operand.p, R_OK);
    PRBool readable = (rv == 0);

    log_error(LOG_FINEST, "-r", sn, rq, "-r \"%s\" = %d", operand.p, readable);

    return result_bool(sn, rq, readable);
}


/* ---------------------------- control_dashs ----------------------------- */

static const Result *control_dashs(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    NSFCFileInfo *finfo;
    PRStatus rv = request_info_path(operand.p, rq, &finfo);
    if (rv != PR_SUCCESS) {
        log_error(LOG_VERBOSE, "-s", sn, rq, "-s \"%s\": %s", operand.p, system_errmsg());
        return result_error(sn, rq, "%s", system_errmsg());
    }

    log_error(LOG_FINEST, "-s", sn, rq, "-s \"%s\" = %lld", operand.p, (PRInt64) finfo->pr.size);

    return result_integer(sn, rq, finfo->pr.size);
}


/* ---------------------------- control_dashu ----------------------------- */

static const Result *control_dashu(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    if (*operand.p != '/')
        return result_error(sn, rq, XP_GetAdminStr(DBT_notLocalUriX), operand.p);

    // Bail out if a parent request is currently evaluating this expression
    for (Request *curr_rq = rq; curr_rq; curr_rq = curr_rq->orig_rq) {
        const char *p = pblock_findkeyval(pb_key_expr, curr_rq->vars);
        if (p && util_atoi64(p) == (PRInt64)args)
            return result_error(sn, rq, XP_GetAdminStr(DBT_recursiveDashUUriX), operand.p);
        if (curr_rq == curr_rq->orig_rq)
            break;
        curr_rq = curr_rq->orig_rq;
    }

    pblock_kllinsert(pb_key_expr, (PRInt64)args, rq->vars);

    Request *new_rq = request_create_child(sn, rq, NULL, operand.p, NULL);

    PRBool exists = PR_FALSE;
    if (servact_lookup(sn, new_rq) == REQ_PROCEED) {
        const char *path = pblock_findkeyval(pb_key_path, new_rq->vars);
        log_error(LOG_FINEST, "-U", sn, rq, "\"%s\" mapped to \"%s\"", operand.p, path);
        if (request_info_path(path, new_rq, NULL) == PR_SUCCESS)
            exists = PR_TRUE;
    } else {
        log_error(LOG_VERBOSE, "-U", sn, rq, "could not map \"%s\"", operand.p);
    }

    request_free(new_rq);

    pblock_removekey(pb_key_expr, rq->vars);

    log_error(LOG_FINEST, "-U", sn, rq, "-U \"%s\" = %d", operand.p, exists);

    return result_bool(sn, rq, exists);
}


/* ------------------------------- defined -------------------------------- */

static PRBool defined(const Arguments *args, Session *sn, Request *rq)
{
    int n = args_length(args);
    if (n < 1)
        return PR_FALSE;
    if (n > 1)
        return PR_FALSE;

    const Expression *e = args_get(args, 0);

    const Result *r = result_expr(sn, rq, e);

    return !result_is_error(r);
}


/* --------------------------- control_defined ---------------------------- */

static const Result *control_defined(const Arguments *args, Session *sn, Request *rq)
{
    return result_bool(sn, rq, defined(args, sn, rq));
}


/* ---------------------------- control_length ---------------------------- */

static const Result *control_length(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    return result_integer(sn, rq, operand.len);
}


/* ------------------------------ control_lc ------------------------------ */

static const Result *control_lc(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    char *s = (char *) pool_malloc(sn->pool, operand.len + 1);
    if (!s)
        return result_out_of_memory(sn, rq);

    for (int i = 0; i <= operand.len; i++)
        s[i] = tolower(operand.p[i]);

    return result_string(sn, rq, s, operand.len);
}


/* ------------------------------ control_uc ------------------------------ */

static const Result *control_uc(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    char *s = (char *) pool_malloc(sn->pool, operand.len + 1);
    if (!s)
        return result_out_of_memory(sn, rq);

    for (int i = 0; i <= operand.len; i++)
        s[i] = toupper(operand.p[i]);

    return result_string(sn, rq, s, operand.len);
}


/* ----------------------------- control_abs ------------------------------ */

static const Result *control_abs(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_INTEGER(args, sn, rq, i);

    if (i < 0)
        i = -i;

    return result_integer(sn, rq, i);
}


/* --------------------------- control_httpdate --------------------------- */

static const Result *control_httpdate(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_INTEGER(args, sn, rq, i);

    struct tm tm;
#if (defined(__GNUC__) && (__GNUC__ > 2))
    if (i >= 19000101000000LL)
#else
    if (i >= 19000101000000)
#endif
    {
        memset(&tm, 0, sizeof(tm));
#if (defined(__GNUC__) && (__GNUC__ > 2))
        tm.tm_year = (i / 10000000000LL) - 1900;
#else
        tm.tm_year = (i / 10000000000) - 1900;
#endif
        tm.tm_mon = (i / 100000000) % 100 - 1;
        tm.tm_mday = (i / 1000000) % 100;
        tm.tm_hour = (i / 10000) % 100;
        tm.tm_min = (i / 100) % 100;
        tm.tm_sec = i % 100;
        mktime(&tm);
    } else {
        time_t t = i;
        util_gmtime(&t, &tm);
    }

    char *p = (char *) pool_malloc(sn->pool, HTTP_DATE_LEN);
    if (!p)
        return result_out_of_memory(sn, rq);

    int len = util_strftime(p, HTTP_DATE_FMT, &tm);

    return result_string(sn, rq, p, len);
}


/* ---------------------------- control_choose ---------------------------- */

static const Result *control_choose(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    int candidates = 1;
    const char *p = operand.p;
    while (*p) {
        if (*p == '|')
            candidates++;
        p++;
    }

    unsigned short r;
    util_random(&r, sizeof(r));
    int chosen = r % candidates;

    int losers = 0;
    const char *begin = operand.p;
    const char *end = operand.p;
    while (*end) {
        if (*end == '|') {
            losers++;
            if (losers > chosen)
                break;
            begin = end + 1;
        }
        end++;
    }

    int len = end - begin;

    log_error(LOG_FINEST, "choose", sn, rq, "selected \"%.*s\" from \"%s\"", len, begin, operand.p);

    return result_string(sn, rq, begin, len);
}


/* ---------------------------- control_escape ---------------------------- */

static const Result *control_escape(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    char *s = util_uri_escape(NULL, operand.p);
    if (!s)
        return result_out_of_memory(sn, rq);

    return result_string(sn, rq, s, strlen(s));
}


/* --------------------------- control_unescape --------------------------- */

static const Result *control_unescape(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    char *s = pool_strdup(sn->pool, operand.p);
    if (!s)
        return result_out_of_memory(sn, rq);

    util_uri_unescape(s);

    return result_string(sn, rq, s, strlen(s));
}


/* --------------------------------- hash --------------------------------- */

static inline unsigned hash(const char *p)
{
    unsigned hval = 0;

    while (*p) {
        hval = (hval << 5) ^ hval ^ *(const unsigned char *) p;
        p++;
    }

    return hval;
}


/* ---------------------------- lookup_create ----------------------------- */

static LookupStruct *lookup_create(unsigned hval, const char *filename)
{
    LookupStruct *lookup = (LookupStruct *) PERM_MALLOC(sizeof(LookupStruct));
    if (lookup) {
        lookup->lock = PR_NewLock();
        lookup->hval = hval;
        lookup->filename = PERM_STRDUP(filename);
        lookup->mtime = -1;
        lookup->checked = -1;
        lookup->pb = NULL;
        if (!lookup->lock || !lookup->filename) {
            if (lookup->lock)
                PR_DestroyLock(lookup->lock);
            PERM_FREE(lookup->filename);
            PERM_FREE(lookup);
            lookup = NULL;
        }
    }

    return lookup;
}
        

/* ----------------------------- lookup_parse ----------------------------- */

static pblock *lookup_parse(Session *sn, Request *rq, const char *filename)
{
    SYS_FILE fd = system_fopenRO(filename);
    if (fd == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, LOOKUP, sn, rq, XP_GetAdminStr(DBT_coreOpenFileXErrorY), filename, system_errmsg());
        return NULL;
    }

    filebuf_t *buf = filebuf_open(fd, FILE_BUFFERSIZE);
    if (!buf) {
        log_error(LOG_FAILURE, LOOKUP, sn, rq, XP_GetAdminStr(DBT_coreOpenFileXErrorY), filename, system_errmsg());
        system_fclose(fd);
        return NULL;
    }

    pblock *pb = pblock_create_pool(NULL, 1);
    if (!pb) {
        filebuf_close(buf);
        return NULL;
    }

    int ln = 0;

    for (;;)  {
        char line[LOOKUP_MAX_LINE];

        int eof = util_getline(buf, ++ln, sizeof(line), line);
        if (!line[0]) {
            if (eof) {
                if (eof == -1)
                    log_error(LOG_FAILURE, LOOKUP, sn, rq, XP_GetAdminStr(DBT_coreReadFileXErrorY), filename, system_errmsg());
                break;
            }
            continue;
        }

        if (line[0] == '#')
            continue;

        char *p = line;
        while (isspace(*p))
            p++;

        const char *name = p;
        if (!*name)
            continue;

        while (*p && !isspace(*p))
            p++;

        if (*p) {
            *p = '\0';
            p++;
            while (isspace(*p))
                p++;
        }

        const char *value = p;
        if (!*value) {
            log_error(LOG_MISCONFIG, LOOKUP, sn, rq, XP_GetAdminStr(DBT_coreFileXLineYNameZNoValue), filename, ln, name);
            continue;
        }

        pblock_nvinsert(name, value, pb);
    }

    filebuf_close(buf);

    return pb;
}


/* ---------------------------- control_lookup ---------------------------- */

static const Result *control_lookup(const Arguments *args, Session *sn, Request *rq)
{
    int n = args_length(args);
    if (n < 2)
        return result_not_enough_args(sn, rq);
    if (n > 3)
        return result_too_many_args(sn, rq);

    const Expression *efilename = args_get(args, 0);
    const Result *rfilename = result_expr(sn, rq, efilename);
    if (result_is_error(rfilename))
        return rfilename;

    const char *filename;
    result_as_const_string(sn, rq, rfilename, &filename, NULL);

    const Expression *ename = args_get(args, 1);
    const Result *rname = result_expr(sn, rq, ename);
    if (result_is_error(rname))
        return rname;

    const char *name;
    result_as_const_string(sn, rq, rname, &name, NULL);

    const char *def = "";
    if (n > 2) {
        const Expression *edef = args_get(args, 2);
        const Result *rdef = result_expr(sn, rq, edef);
        if (result_is_error(rdef))
            return rdef;

        result_as_const_string(sn, rq, rdef, &def, NULL);
    }

    unsigned hval = hash(filename);

    int hi = hval & lookup_filename_hmask;

    PR_Lock(lookup_filename_ht[hi].lock);

    LookupStruct *lookup = NULL;
    for (int i = 0; i < lookup_filename_ht[hi].list.length(); i++) {
        lookup = lookup_filename_ht[hi].list[i];
        if (lookup->hval == hval && !strcmp(lookup->filename, filename))
            break;
    }

    if (!lookup) {
        lookup = lookup_create(hval, filename);
        if (lookup)
            lookup_filename_ht[hi].list.append(lookup);
    }

    PR_Unlock(lookup_filename_ht[hi].lock);

    if (!lookup)
        return result_out_of_memory(sn, rq);

    if (lookup->checked != ft_time()) {
        lookup->checked = ft_time();

        struct stat finfo;
        int rv = system_stat(filename, &finfo);
        if (rv == -1)
            finfo.st_mtime = -1;

        if (lookup->mtime != finfo.st_mtime) {
            PR_Lock(lookup->lock);

            lookup->mtime = finfo.st_mtime;

            pblock_free(lookup->pb);
            lookup->pb = lookup_parse(sn, rq, filename);

            PR_Unlock(lookup->lock);        
        }
    }

    PRBool valid;
    const char *value;

    PR_Lock(lookup->lock);

    if (lookup->pb) {
        valid = PR_TRUE;
        value = pblock_findval(name, lookup->pb);
    } else {
        valid = PR_FALSE;
    }

    PR_Unlock(lookup->lock);

    if (!valid)
        return result_error(sn, rq, XP_GetAdminStr(DBT_badLookupFileX), filename);

    if (value) {
        log_error(LOG_FINEST, LOOKUP, sn, rq, "found value \"%s\" for name \"%s\" in %s", value, name, filename);
    } else {
        log_error(LOG_FINEST, LOOKUP, sn, rq, "returning defult value \"%s\" (No value for name \"%s\" in %s)", def, name, filename);
        value = def;
    }

    return result_string(sn, rq, value, strlen(value));
}


/* --------------------------- external_create ---------------------------- */

static ExternalStruct *external_create(unsigned hval, const char *path)
{
    ExternalStruct *external = (ExternalStruct *) PERM_MALLOC(sizeof(ExternalStruct));
    if (external) {
        external->lock = PR_NewLock();
        external->hval = hval;
        external->path = PERM_STRDUP(path);
        external->in = NULL;
        external->out = NULL;
        external->child = NULL;
        if (!external->lock || !external->path) {
            if (external->lock)
                PR_DestroyLock(external->lock);
            PERM_FREE(external->path);
            external = NULL;
        }
    }

    return external;
}
        

/* ---------------------------- external_exec ----------------------------- */

static PRStatus external_exec(ExternalStruct *external)
{
    PRStatus rv = PR_FAILURE;

    Child *child = child_create(NULL, NULL, external->path);
    if (child) {
        // external() programs are subject to the same IO timeout as CGI
        // programs
        PRIntervalTime timeout = cgi_get_idle_timeout();

        PRFileDesc *in = child_pipe(child, PR_StandardInput, timeout);
        PRFileDesc *out = child_pipe(child, PR_StandardOutput, timeout);

        rv = child_shell(child, NULL, NULL, PR_INTERVAL_NO_TIMEOUT);
        if (rv == PR_SUCCESS) {
            external->in = in;
            external->out = out;
            external->child = child;
        }
    }

    return rv;
}


/* ----------------------------- external_io ------------------------------ */

static PRStatus external_io(ExternalStruct *external, Session *sn, Request *rq, const char *p, int len, char *buf, int size)
{
    PRInt32 rv;

    // p must end with '\n'
    PR_ASSERT(len > 0);

    log_error(LOG_VERBOSE, EXTERNAL, sn, rq, "sending \"%.*s\" to %s", len - 1, p, external->path);

    rv = PR_Write(external->in, p, len);
    if (rv != len) {
        log_error(LOG_FAILURE, EXTERNAL, sn, rq, XP_GetAdminStr(DBT_coreProgXWriteErrorY), external->path, system_errmsg());
        return PR_FAILURE;
    }

    int pos = 0;

    for (;;) {
        log_error(LOG_VERBOSE, EXTERNAL, sn, rq, "waiting for response from %s", external->path);

        rv = PR_Read(external->out, buf + pos, size - pos);
        if (rv == 0) {
            PR_SetError(PR_END_OF_FILE_ERROR, 0);
            rv = -1;
        }
        if (rv == -1) {
            log_error(LOG_FAILURE, EXTERNAL, sn, rq, XP_GetAdminStr(DBT_coreProgXReadErrorY), external->path, system_errmsg());
            return PR_FAILURE;
        }

        int used = pos + rv;

        for (; pos < used; pos++) {
            if (buf[pos] == '\0') {
                log_error(LOG_FAILURE, EXTERNAL, sn, rq, XP_GetAdminStr(DBT_coreProgXSentNul), external->path);
                return PR_FAILURE;
            }

            if (buf[pos] == '\n') {
                if (pos != used - 1) {
                    buf[pos] = '\0';
                    log_error(LOG_FAILURE, EXTERNAL, sn, rq, XP_GetAdminStr(DBT_coreBadLineXProgY), buf, external->path);
                    return PR_FAILURE;
                }

                if (pos > 0 && buf[pos - 1] == '\r')
                    pos--;

                log_error(LOG_VERBOSE, EXTERNAL, sn, rq, "received \"%.*s\" from %s", pos, buf, external->path);

                buf[pos] = '\0';

                return PR_SUCCESS;
            }
        }

        if (pos == size) {
            log_error(LOG_FAILURE, EXTERNAL, sn, rq, XP_GetAdminStr(DBT_coreXBytesProgYNoEol), size, external->path);
            return PR_FAILURE;
        }

        log_error(LOG_VERBOSE, EXTERNAL, sn, rq, "received \"%.*s\" from %s, still waiting for end of line...", pos, buf, external->path);
    }
}


/* ---------------------------- external_term ----------------------------- */

static void external_term(ExternalStruct *external)
{
    if (external->child) {
        ereport(LOG_VERBOSE, "terminating %s", external->path);

        child_term(external->child);

        external->in = NULL;
        external->out = NULL;
        external->child = NULL;
    }
}


/* --------------------------- external_work_fn --------------------------- */

PR_BEGIN_EXTERN_C

static PR_CALLBACK void external_work_fn(ExternalWork *work)
{
    PRStatus rv;

    PR_Lock(work->external->lock);

    PRBool retry = PR_TRUE;
    while (retry) {
        // If the external program isn't running...
        if (!work->external->child) {
            // Start the external program
            rv = external_exec(work->external);
            if (rv == PR_FAILURE) {
                log_error(LOG_FAILURE, EXTERNAL, work->sn, work->rq, XP_GetAdminStr(DBT_coreExecXErrorY), work->external->path, system_errmsg());
                break;
            }

            PR_ASSERT(work->external->child);

            // If the external program fails right off the bat, don't retry
            retry = PR_FALSE;
        }

        // Try the external transaction
        rv = external_io(work->external, work->sn, work->rq,
                         work->input.p, work->input.len,
                         work->output.buf, sizeof(work->output.buf));
        if (rv == PR_SUCCESS)
            break;

        // The external program is misbehaving.  Tell it to exit.
        external_term(work->external);

        PR_ASSERT(!work->external->child);
    }

    PR_Unlock(work->external->lock);

    work->rv = rv;
}

PR_END_EXTERN_C


/* --------------------------- control_external --------------------------- */

static const Result *control_external(const Arguments *args, Session *sn, Request *rq)
{
    int n = args_length(args);
    if (n < 2)
        return result_not_enough_args(sn, rq);
    if (n > 2)
        return result_too_many_args(sn, rq);

    const Expression *epath = args_get(args, 0);
    const Result *rpath = result_expr(sn, rq, epath);
    if (result_is_error(rpath))
        return rpath;

    const char *path;
    result_as_const_string(sn, rq, rpath, &path, NULL);

    const Expression *ename = args_get(args, 1);
    const Result *rname = result_expr(sn, rq, ename);
    if (result_is_error(rname))
        return rname;

    const char *name;
    result_as_const_string(sn, rq, rname, &name, NULL);

    // External programs are line-oriented, so line must end with '\n'
    int llen = 0;
    while (name[llen] != '\0' && name[llen] != '\n')
        llen++;
    char *line = (char *) pool_malloc(sn->pool, llen + 2);
    if (!line)
        return result_out_of_memory(sn, rq);
    memcpy(line, name, llen);
    line[llen++] = '\n';
    line[llen] = '\0';

    unsigned hval = hash(path);

    int hi = hval & external_path_hmask;

    PR_Lock(external_path_ht[hi].lock);

    ExternalStruct *external = NULL;
    for (int i = 0; i < external_path_ht[hi].list.length(); i++) {
        external = external_path_ht[hi].list[i];
        if (external->hval == hval && !strcmp(external->path, path))
            break;
    }

    if (!external) {
        external = external_create(hval, path);
        if (external)
            external_path_ht[hi].list.append(external);
    }

    PR_Unlock(external_path_ht[hi].lock);

    if (!external)
        return result_out_of_memory(sn, rq);

    ExternalWork work;
    work.external = external;
    work.sn = sn;
    work.rq = rq;
    work.input.p = line;
    work.input.len = llen;
    work.rv = PR_FAILURE;

    if (func_is_native_thread()) {
        external_work_fn(&work);
    } else {
        NSTPStatus status = NSTP_QueueWorkItem(func_get_native_pool(),
                                               &external_work_fn,
                                               &work,
                                               PR_INTERVAL_NO_TIMEOUT);
        if (status != NSTP_STATUS_WORK_DONE)
            work.rv = PR_FAILURE;
    }

    if (work.rv != PR_SUCCESS)
        return result_error(sn, rq, XP_GetAdminStr(DBT_programXFailed), path);

    return result_string(sn, rq, work.output.buf, strlen(work.output.buf));
}


/* ----------------------------- control_uuid ----------------------------- */

static const Result *control_uuid(const Arguments *args, Session *sn, Request *rq)
{
    const Result *result;

    if (args_length(args))
        return result_too_many_args(sn, rq);

    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_string[UUID_STRING_SIZE];
    uuid_unparse(uuid, uuid_string);
    PR_ASSERT(strlen(uuid_string) == UUID_STRING_SIZE - 1);
    result = result_string(sn, rq, uuid_string, UUID_STRING_SIZE - 1);
        
    return result;
}


/* ---------------------------- control_atime ----------------------------- */

static const Result *control_atime(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    struct stat *finfo = request_stat_path(operand.p, rq);
    if (!finfo) {
        log_error(LOG_VERBOSE, "atime", sn, rq, "atime(\"%s\"): %s", operand.p, system_errmsg());
        return result_error(sn, rq, "%s", system_errmsg());
    }

    return result_integer(sn, rq, finfo->st_atime);
}


/* ---------------------------- control_ctime ----------------------------- */

static const Result *control_ctime(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    NSFCFileInfo *finfo;
    PRStatus rv = request_info_path(operand.p, rq, &finfo);
    if (rv != PR_SUCCESS) {
        log_error(LOG_VERBOSE, "ctime", sn, rq, "ctime(\"%s\"): %s", operand.p, system_errmsg());
        return result_error(sn, rq, "%s", system_errmsg());
    }

    return result_integer(sn, rq, finfo->pr.creationTime / PR_USEC_PER_SEC);
}


/* ---------------------------- control_mtime ----------------------------- */

static const Result *control_mtime(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    NSFCFileInfo *finfo;
    PRStatus rv = request_info_path(operand.p, rq, &finfo);
    if (rv != PR_SUCCESS) {
        log_error(LOG_VERBOSE, "mtime", sn, rq, "mtime(\"%s\"): %s", operand.p, system_errmsg());
        return result_error(sn, rq, "%s", system_errmsg());
    }

    return result_integer(sn, rq, finfo->pr.modifyTime / PR_USEC_PER_SEC);
}


/* -------------------------------- owner --------------------------------- */

static const char *owner(const char *path, Session *sn, Request *rq)
{
#ifdef XP_WIN32
    char *pathcopy = pool_strdup(sn->pool, path);
    if (!pathcopy)
        return NULL;

    PSID psid;
    PSECURITY_DESCRIPTOR psd;
    DWORD error = GetNamedSecurityInfo(pathcopy, SE_FILE_OBJECT,
                                       OWNER_SECURITY_INFORMATION, &psid,
                                       NULL, NULL, NULL, &psd);
    if (error != ERROR_SUCCESS) {
        SetLastError(error);
        NsprError::mapWin32Error();
        return NULL;
    }

    char *name = NULL;
    DWORD nsize = 1;

    char *domain = NULL;
    DWORD dsize = 1;

    for (;;) {
        name = (char *) pool_realloc(sn->pool, name, nsize);
        if (!name)
            return NULL;

        domain = (char *) pool_realloc(sn->pool, domain, dsize);
        if (!domain)
            return NULL;

        SID_NAME_USE snu;
        if (LookupAccountSid(NULL, psid, name, &nsize, domain, &dsize, &snu))
            break;

        DWORD error = GetLastError();

        NsprError::mapWin32Error();

        if (error != ERROR_INSUFFICIENT_BUFFER) {
            name = NULL;
            domain = NULL;
            break;
        }
    }

    LocalFree(psd);

    return name;
#else
    struct stat *finfo = request_stat_path(path, rq);
    if (!finfo)
        return NULL;

    char *buf = (char *) pool_malloc(sn->pool, DEF_PWBUF);
    if (!buf)
        return NULL;

    // XXX cache uid lookups?

    struct passwd pw;
    struct passwd *ppw = util_getpwuid(finfo->st_uid, &pw, buf, DEF_PWBUF);
    if (!ppw)
        return NULL;

    return ppw->pw_name;
#endif
}


/* ---------------------------- control_owner ----------------------------- */

static const Result *control_owner(const Arguments *args, Session *sn, Request *rq)
{
    OPERAND_STRING(args, sn, rq, operand)

    const char *s = owner(operand.p, sn, rq);
    if (!s) {
        log_error(LOG_VERBOSE, "owner", sn, rq, "owner(\"%s\"): %s", operand.p, system_errmsg());
        return result_error(sn, rq, "%s", system_errmsg());
    }

    log_error(LOG_FINEST, "owner", sn, rq, "owner(\"%s\") = \"%s\"", operand.p, s);

    return result_string(sn, rq, s, strlen(s));
}


/* -------------------------- control_terminate --------------------------- */

PR_BEGIN_EXTERN_C

static void control_terminate(void *arg)
{
    for (int hi = 0; hi < external_path_hsize; hi++) {
        PR_Lock(external_path_ht[hi].lock);

        for (int i = 0; i < external_path_ht[hi].list.length(); i++) {
            ExternalStruct *external = external_path_ht[hi].list[i];

            PR_Lock(external->lock);

            external_term(external);

            PR_Unlock(external->lock);
        }

        PR_Unlock(external_path_ht[hi].lock);
    }
}

PR_END_EXTERN_C


/* ----------------------------- control_init ----------------------------- */

PRStatus control_init(void)
{
    int hi;

    daemon_atrestart(control_terminate, NULL);

    for (hi = 0; hi < lookup_filename_hsize; hi++)
        lookup_filename_ht[hi].lock = PR_NewLock();

    for (hi = 0; hi < external_path_hsize; hi++)
        external_path_ht[hi].lock = PR_NewLock();

    expr_control_func_insert("-d", &control_dashd);
    expr_control_func_insert("-e", &control_dashe);
    expr_control_func_insert("-f", &control_dashf);
    expr_control_func_insert("-l", &control_dashl);
    expr_control_func_insert("-r", &control_dashr);
    expr_control_func_insert("-s", &control_dashs);
    expr_control_func_insert("-U", &control_dashu);
    expr_control_func_insert("defined", &control_defined);
    expr_control_func_insert("length", &control_length);
    expr_control_func_insert("lc", &control_lc);
    expr_control_func_insert("uc", &control_uc);
    expr_control_func_insert("abs", &control_abs);
    expr_control_func_insert("httpdate", &control_httpdate);
    expr_control_func_insert("choose", &control_choose);
    expr_control_func_insert("escape", &control_escape);
    expr_control_func_insert("unescape", &control_unescape);
    expr_control_func_insert(LOOKUP, &control_lookup);
    expr_control_func_insert(EXTERNAL, &control_external);
    expr_control_func_insert("uuid", &control_uuid);
    expr_control_func_insert("atime", &control_atime);
    expr_control_func_insert("ctime", &control_ctime);
    expr_control_func_insert("mtime", &control_mtime);
    expr_control_func_insert("owner", &control_owner);

    return PR_SUCCESS;
}
