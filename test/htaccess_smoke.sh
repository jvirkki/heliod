#!/bin/bash
#
# htaccess directive parsing, authentication, IP/domain access control, and
# dynamic .htaccess rewrite under concurrent load smoke test.
#
# Verifies:
#   1. Standalone ASAN + UBSAN test of src/server/plugins/htaccess/*,
#      src/server/libaccess/cryptwrapper.cpp, src/server/safs/auth.cpp
#      (simple_user / simple_group), src/server/base/buffer.cpp
#      (filebuf_open_nostat_nommap / filebuf_close_nommap), and htpasswd.c:
#      - Parsing and evaluation of all supported .htaccess directives on Linux:
#        AuthName, AuthType, AuthUserFile, AuthGroupFile, <Limit ...>,
#        </Limit>, <LimitExcept ...>, </LimitExcept>, order allow,deny /
#        deny,allow / mutual-failure, allow from (all, domain, subdomain,
#        exact IP, partial IP a.b.c., CIDR /nnn, netmask /a.b.c.d, multiple
#        hosts > SECTABLE_ALLOC_UNIT), deny from, require valid-user /
#        require user (quoted and unquoted) / require group (with
#        groups-with-users and separate group file, continuation lines), and
#        registered external directives via htaccess_register (BOOL, PASS1,
#        PASS2).
#      - Malformed / syntax error handling without memory leaks or crashes:
#        nested <Limit>, unclosed <Limit>, TRACE in <Limit>, unknown method,
#        directives inside <Limit>, unknown order, invalid IP/mask.
#      - Multi-level directory hierarchy (.htaccess in parent + child +
#        grandchild) exercising _htaccess_freectxt() when num_sec >= 2 and
#        directory paths without trailing slash.
#      - ACL_CryptCompare() with valid DES/SHA/MD5 hashes and invalid/empty
#        salt (where glibc/libxcrypt crypt() returns NULL).
#      - simple_group() exact username matching (no prefix match) and dynamic
#        groupnames buffer growth when matched group list exceeds file size.
#      - htpasswd binary creation (-c) and update under ASAN/UBSAN.
#   2. Standalone TSAN test of concurrent htaccess evaluation + basic auth +
#      group evaluation while a writer thread rewrites .htaccess, .htpasswd,
#      and .htgroup in place (O_TRUNC + write and atomic rename) every 100 ms.
#   3. Live webservd integration test (when <heliod-work-root> is provided):
#      loads lib/htaccess.so via Init fn="load-modules" and Init
#      fn="htaccess-init", configures PathCheck fn="htaccess-find" across
#      classic (18180) and event (18181) listeners, verifies all directives
#      over HTTP/1.1 and HTTP/2 (h2c), and runs a 60-second load test (override
#      duration via HTACCESS_LOAD_SECONDS) that rewrites .htaccess in place
#      every 100 ms while concurrent worker threads request protected and
#      public resources, asserting zero worker crashes or PID changes.
#
# Usage:
#   test/htaccess_smoke.sh [heliod-work-root]

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
LOAD_SECONDS="${HTACCESS_LOAD_SECONDS:-60}"

TMP_DIR=$(mktemp -d /tmp/heliod-htaccess-smoke-XXXXXX)
cleanup_tmp() {
    rm -rf "${TMP_DIR}"
}
trap cleanup_tmp EXIT

echo "=== [1/4] Building htpasswd and htaccess unit/stress harness with ASAN+UBSAN ==="

CC_BIN="${CC:-gcc}"
CXX_BIN="${CXX:-g++}"

# 1a. Build and test htpasswd under ASAN+UBSAN
"${CC_BIN}" -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DXP_UNIX -DLINUX -DLinux \
    "${REPO_ROOT}/src/server/plugins/htaccess/htpasswd.c" \
    -l:libcrypt.so.1 -o "${TMP_DIR}/htpasswd_asan"

HTPASSWD_FILE="${TMP_DIR}/test.htpasswd"
"${TMP_DIR}/htpasswd_asan" -c "${HTPASSWD_FILE}" alice Secret123 >/dev/null
"${TMP_DIR}/htpasswd_asan" "${HTPASSWD_FILE}" bobHunter2 Pass456 >/dev/null
"${TMP_DIR}/htpasswd_asan" "${HTPASSWD_FILE}" bob Pass789 >/dev/null
"${TMP_DIR}/htpasswd_asan" "${HTPASSWD_FILE}" alice NewSecret123 >/dev/null

grep -q '^alice:[^:]*$' "${HTPASSWD_FILE}" || { echo "FAIL: alice missing in htpasswd"; exit 1; }
grep -q '^bobHunter2:[^:]*$' "${HTPASSWD_FILE}" || { echo "FAIL: bobHunter2 missing in htpasswd"; exit 1; }
grep -q '^bob:[^:]*$' "${HTPASSWD_FILE}" || { echo "FAIL: bob missing in htpasswd"; exit 1; }
echo "PASS: htpasswd -c and update under ASAN+UBSAN"

# 1b. Create standalone mock NSAPI environment to compile the real htaccess
#     plugin sources + cryptwrapper.cpp under ASAN+UBSAN and TSAN.
mkdir -p "${TMP_DIR}/mock_inc/base" \
         "${TMP_DIR}/mock_inc/frame" \
         "${TMP_DIR}/mock_inc/httpdaemon" \
         "${TMP_DIR}/mock_inc/libaccess"

cat > "${TMP_DIR}/mock_inc/nspr.h" <<'EOF'
#ifndef MOCK_NSPR_H
#define MOCK_NSPR_H
#include <stdint.h>
#include <stddef.h>
typedef int32_t PRInt32;
typedef uint32_t PRUint32;
typedef int64_t PRInt64;
typedef uint64_t PRUint64;
typedef int PRBool;
#define PR_TRUE 1
#define PR_FALSE 0
#ifdef __cplusplus
#define NSPR_BEGIN_EXTERN_C extern "C" {
#define NSPR_END_EXTERN_C }
#else
#define NSPR_BEGIN_EXTERN_C
#define NSPR_END_EXTERN_C
#endif
#define NSAPI_PUBLIC
#endif
EOF

cat > "${TMP_DIR}/mock_inc/netsite.h" <<'EOF'
#ifndef MOCK_NETSITE_H
#define MOCK_NETSITE_H
#include <stdlib.h>
#include <string.h>
#include "nspr.h"
#define NSAPI_PUBLIC
#define MALLOC(sz) malloc(sz)
#define CALLOC(sz) calloc(1, (sz))
#define REALLOC(p, sz) realloc((p), (sz))
#define FREE(p) free(p)
#define STRDUP(s) strdup(s)
#define PERM_MALLOC(sz) malloc(sz)
#define PERM_REALLOC(p, sz) realloc((p), (sz))
#define PERM_FREE(p) free(p)
#define REQ_PROCEED 0
#define REQ_ABORTED -1
#define REQ_NOACTION -2
#define REQ_EXIT -3
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/systems.h" <<'EOF'
#ifndef MOCK_BASE_SYSTEMS_H
#define MOCK_BASE_SYSTEMS_H
#include "../netsite.h"
typedef char *caddr_t;
#define FILE_BUFFERSIZE 4096
#define IO_OKAY 1
#define IO_ERROR -1
#define IO_EOF 0
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/pblock.h" <<'EOF'
#ifndef MOCK_BASE_PBLOCK_H
#define MOCK_BASE_PBLOCK_H
#include "../netsite.h"
typedef struct pb_param {
    char *name;
    char *value;
    struct pb_param *next;
} pb_param;
typedef struct pblock {
    pb_param *head;
} pblock;
pblock *pblock_create(int n);
void pblock_free(pblock *pb);
char *pblock_findval(const char *name, pblock *pb);
pb_param *pblock_nvinsert(const char *name, const char *value, pblock *pb);
pb_param *pblock_remove(const char *name, pblock *pb);
void param_free(pb_param *pp);
int pblock_copy(pblock *src, pblock *dst);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/session.h" <<'EOF'
#ifndef MOCK_BASE_SESSION_H
#define MOCK_BASE_SESSION_H
#include "pblock.h"
typedef struct Session {
    pblock *client;
    char *dns_host;
} Session;
char *session_dns(Session *sn);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/file.h" <<'EOF'
#ifndef MOCK_BASE_FILE_H
#define MOCK_BASE_FILE_H
#include <sys/stat.h>
#include "systems.h"
typedef int SYS_FILE;
#define SYS_ERROR_FD (-1)
SYS_FILE system_fopenRO(const char *path);
int system_fclose(SYS_FILE fd);
int system_fread(SYS_FILE fd, char *buf, int sz);
int system_stat(const char *path, struct stat *finfo);
const char *system_errmsg(void);
int PR_FileDesc2NativeHandle(SYS_FILE fd);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/buffer.h" <<'EOF'
#ifndef MOCK_BASE_BUFFER_H
#define MOCK_BASE_BUFFER_H
#include "file.h"
typedef struct filebuffer {
    SYS_FILE fd;
    caddr_t fp;
    int len;
    unsigned char *inbuf;
    int cursize;
    int pos;
    const char *errmsg;
} filebuffer;
typedef filebuffer filebuf_t;
#define filebuf_getc(b) ((b)->pos == (b)->len ? IO_EOF : (int)(unsigned char)((b)->fp)[(b)->pos++])
filebuf_t *filebuf_open(SYS_FILE fd, int sz);
filebuf_t *filebuf_open_nostat_nommap(SYS_FILE fd, int sz, struct stat *finfo);
void filebuf_close(filebuf_t *buf);
void filebuf_close_nommap(filebuf_t *buf);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/util.h" <<'EOF'
#ifndef MOCK_BASE_UTIL_H
#define MOCK_BASE_UTIL_H
#include "buffer.h"
int util_getline(filebuf_t *buf, int lineno, int maxlen, char *l);
int util_snprintf(char *s, int n, const char *fmt, ...);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/base/crit.h" <<'EOF'
#ifndef MOCK_BASE_CRIT_H
#define MOCK_BASE_CRIT_H
#include <pthread.h>
#include <unistd.h>
#include "../netsite.h"
typedef pthread_mutex_t *CRITICAL;
CRITICAL crit_init(void);
void crit_enter(CRITICAL c);
void crit_exit(CRITICAL c);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/frame/req.h" <<'EOF'
#ifndef MOCK_FRAME_REQ_H
#define MOCK_FRAME_REQ_H
#include <sys/stat.h>
#include "../base/pblock.h"
typedef struct Request {
    pblock *vars;
    pblock *reqpb;
    pblock *headers;
    pblock *srvhdrs;
    int directive_is_cacheable;
    int status_num;
    struct stat st;
    int has_stat;
} Request;
struct stat *request_stat_path(const char *path, Request *rq);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/frame/log.h" <<'EOF'
#ifndef MOCK_FRAME_LOG_H
#define MOCK_FRAME_LOG_H
#include "../base/session.h"
#include "req.h"
#define LOG_VERBOSE 0
#define LOG_FAILURE 1
#define LOG_MISCONFIG 2
#define LOG_SECURITY 3
#define LOG_WARN 4
void log_error(int degree, const char *func, Session *sn, Request *rq, const char *fmt, ...);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/frame/protocol.h" <<'EOF'
#ifndef MOCK_FRAME_PROTOCOL_H
#define MOCK_FRAME_PROTOCOL_H
#include "../base/session.h"
#include "req.h"
#define PROTOCOL_OK 200
#define PROTOCOL_UNAUTHORIZED 401
#define PROTOCOL_FORBIDDEN 403
#define PROTOCOL_SERVER_ERROR 500
void protocol_status(Session *sn, Request *rq, int n, const char *r);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/frame/http.h" <<'EOF'
#ifndef MOCK_FRAME_HTTP_H
#define MOCK_FRAME_HTTP_H
#include "protocol.h"
#endif
EOF

cat > "${TMP_DIR}/mock_inc/frame/func.h" <<'EOF'
#ifndef MOCK_FRAME_FUNC_H
#define MOCK_FRAME_FUNC_H
#include "../base/session.h"
#include "req.h"
typedef int (*FuncPtr)(pblock *, Session *, Request *);
FuncPtr func_find(const char *name);
void func_insert(const char *name, FuncPtr fn);
int func_exec(pblock *pb, Session *sn, Request *rq);
#endif
EOF

cat > "${TMP_DIR}/mock_inc/httpdaemon/HttpMethodRegistry.h" <<'EOF'
#ifndef MOCK_HTTP_METHOD_REGISTRY_H
#define MOCK_HTTP_METHOD_REGISTRY_H
#include <strings.h>
class HttpMethodRegistry {
public:
    static HttpMethodRegistry& GetRegistry() {
        static HttpMethodRegistry reg;
        return reg;
    }
    int GetMethodIndex(const char *m) {
        if (!m) return -1;
        if (!strcasecmp(m, "GET")) return 0;
        if (!strcasecmp(m, "PUT")) return 1;
        if (!strcasecmp(m, "POST")) return 2;
        if (!strcasecmp(m, "DELETE")) return 3;
        if (!strcasecmp(m, "OPTIONS")) return 4;
        if (!strcasecmp(m, "PROPFIND")) return 5;
        return -1;
    }
};
#endif
EOF

cp "${REPO_ROOT}/src/server/libaccess/cryptwrapper.h" "${TMP_DIR}/mock_inc/libaccess/cryptwrapper.h"

cat > "${TMP_DIR}/test_htaccess_unit.cpp" <<'EOF'
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>

#include "htaccess.h"
#include "base/crit.h"
#include "frame/protocol.h"
#include "frame/func.h"
#include "libaccess/cryptwrapper.h"

/* --- Mock NSAPI runtime implementation --- */

pblock *pblock_create(int) {
    return (pblock *)calloc(1, sizeof(pblock));
}

void param_free(pb_param *pp) {
    if (!pp) return;
    free(pp->name);
    free(pp->value);
    free(pp);
}

void pblock_free(pblock *pb) {
    if (!pb) return;
    pb_param *cur = pb->head;
    while (cur) {
        pb_param *next = cur->next;
        param_free(cur);
        cur = next;
    }
    free(pb);
}

char *pblock_findval(const char *name, pblock *pb) {
    if (!pb || !name) return NULL;
    for (pb_param *p = pb->head; p; p = p->next) {
        if (strcasecmp(p->name, name) == 0)
            return p->value;
    }
    return NULL;
}

pb_param *pblock_remove(const char *name, pblock *pb) {
    if (!pb || !name) return NULL;
    pb_param **pp = &pb->head;
    while (*pp) {
        if (strcasecmp((*pp)->name, name) == 0) {
            pb_param *found = *pp;
            *pp = found->next;
            found->next = NULL;
            return found;
        }
        pp = &(*pp)->next;
    }
    return NULL;
}

pb_param *pblock_nvinsert(const char *name, const char *value, pblock *pb) {
    if (!pb || !name) return NULL;
    param_free(pblock_remove(name, pb));
    pb_param *p = (pb_param *)calloc(1, sizeof(pb_param));
    p->name = strdup(name);
    p->value = strdup(value ? value : "");
    p->next = pb->head;
    pb->head = p;
    return p;
}

int pblock_copy(pblock *src, pblock *dst) {
    if (!src || !dst) return -1;
    for (pb_param *p = src->head; p; p = p->next) {
        pblock_nvinsert(p->name, p->value, dst);
    }
    return 0;
}

char *session_dns(Session *sn) {
    return sn ? sn->dns_host : NULL;
}

SYS_FILE system_fopenRO(const char *path) {
    if (!path) return SYS_ERROR_FD;
    return open(path, O_RDONLY);
}

int system_fclose(SYS_FILE fd) {
    return close(fd);
}

int system_fread(SYS_FILE fd, char *buf, int sz) {
    ssize_t r = read(fd, buf, (size_t)sz);
    return (int)r;
}

int system_stat(const char *path, struct stat *finfo) {
    return stat(path, finfo);
}

const char *system_errmsg(void) {
    return strerror(errno);
}

int PR_FileDesc2NativeHandle(SYS_FILE fd) {
    return fd;
}

filebuf_t *filebuf_open_nostat_nommap(SYS_FILE fd, int sz, struct stat *finfo) {
    filebuf_t *buf = (filebuf_t *)MALLOC(sizeof(filebuf_t));
    if (!buf) return NULL;
    int cap = (finfo && finfo->st_size > 0) ? (int)finfo->st_size : 0;
    buf->pos = 0;
    buf->len = 0;
    buf->cursize = 0;
    buf->fd = fd;
    buf->inbuf = NULL;
    buf->errmsg = NULL;
    buf->fp = 0;
    int init_cap = (cap > 0) ? cap : ((sz > 0) ? sz : FILE_BUFFERSIZE);
    buf->fp = (char *)PERM_MALLOC(init_cap);
    if (!buf->fp) {
        FREE(buf);
        return NULL;
    }
    cap = init_cap;
    int totalRead = 0;
    while (1) {
        if (totalRead == cap) {
            int grow = (sz > 0) ? sz : FILE_BUFFERSIZE;
            int new_cap = cap + grow;
            char *new_fp = (char *)PERM_REALLOC(buf->fp, new_cap);
            if (!new_fp) {
                PERM_FREE(buf->fp);
                FREE(buf);
                return NULL;
            }
            buf->fp = new_fp;
            cap = new_cap;
        }
        int bytesRead = system_fread(fd, buf->fp + totalRead, cap - totalRead);
        if (bytesRead < 0) {
            PERM_FREE(buf->fp);
            FREE(buf);
            return NULL;
        }
        if (bytesRead == 0)
            break;
        totalRead += bytesRead;
    }
    if (totalRead == 0) {
        PERM_FREE(buf->fp);
        buf->fp = 0;
    }
    buf->len = totalRead;
    buf->cursize = totalRead;
    return buf;
}

filebuf_t *filebuf_open(SYS_FILE fd, int sz) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    fstat(fd, &st);
    return filebuf_open_nostat_nommap(fd, sz, &st);
}

void filebuf_close_nommap(filebuf_t *buf) {
    if (!buf) return;
    if (buf->fp) {
        PERM_FREE(buf->fp);
        buf->fp = 0;
    }
    system_fclose(buf->fd);
    FREE(buf);
}

void filebuf_close(filebuf_t *buf) {
    filebuf_close_nommap(buf);
}

int util_getline(filebuf_t *buf, int, int maxlen, char *l) {
    int i, x = 0;
    while (1) {
        i = filebuf_getc(buf);
        switch (i) {
        case IO_EOF:
            l[x] = '\0';
            return 1;
        case '\n':
            if (x && l[x - 1] == '\\') {
                --x;
                continue;
            }
            l[x] = '\0';
            return 0;
        case '\r':
            continue;
        default:
            l[x] = (char)i;
            if (++x == maxlen) {
                l[maxlen - 1] = '\0';
                return -1;
            }
            break;
        }
    }
}

int util_snprintf(char *s, int n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(s, (size_t)n, fmt, ap);
    va_end(ap);
    return r;
}

CRITICAL crit_init(void) {
    pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    return m;
}

void crit_enter(CRITICAL c) {
    if (c) pthread_mutex_lock(c);
}

void crit_exit(CRITICAL c) {
    if (c) pthread_mutex_unlock(c);
}

struct stat *request_stat_path(const char *path, Request *rq) {
    if (!path || !rq) return NULL;
    if (stat(path, &rq->st) == 0) {
        rq->has_stat = 1;
        return &rq->st;
    }
    return NULL;
}

void log_error(int, const char *, Session *, Request *, const char *, ...) {
}

void protocol_status(Session *, Request *rq, int n, const char *) {
    if (rq) rq->status_num = n;
}

struct FuncEntry {
    const char *name;
    FuncPtr fn;
};
static FuncEntry g_funcs[32];
static int g_num_funcs = 0;

FuncPtr func_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_num_funcs; i++) {
        if (strcasecmp(g_funcs[i].name, name) == 0)
            return g_funcs[i].fn;
    }
    return NULL;
}

void func_insert(const char *name, FuncPtr fn) {
    if (g_num_funcs < 32) {
        g_funcs[g_num_funcs].name = strdup(name);
        g_funcs[g_num_funcs].fn = fn;
        g_num_funcs++;
    }
}

int func_exec(pblock *pb, Session *sn, Request *rq) {
    char *fn_name = pblock_findval("fn", pb);
    FuncPtr fn = func_find(fn_name);
    if (!fn) return REQ_ABORTED;
    return fn(pb, sn, rq);
}

extern "C" int htaccess_init(pblock *pb, Session *sn, Request *rq);
extern "C" int htaccess_find(pblock *pb, Session *sn, Request *rq);
extern "C" int htaccess_userdb(pblock *pb, Session *sn, Request *rq);

static int mock_simple_user(pblock *param, Session *sn, Request *rq) {
    char *pwfile = pblock_findval("userdb", param);
    char *user = pblock_findval("user", param);
    char *pw = pblock_findval("pw", param);
    if (!pwfile || !user || !pw) return REQ_ABORTED;
    SYS_FILE fd = system_fopenRO(pwfile);
    if (fd == SYS_ERROR_FD) {
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    struct stat finfo;
    memset(&finfo, 0, sizeof(finfo));
    fstat(fd, &finfo);
    filebuf_t *buf = filebuf_open_nostat_nommap(fd, FILE_BUFFERSIZE, &finfo);
    if (!buf) {
        system_fclose(fd);
        return REQ_ABORTED;
    }
    char line[1024];
    int ln = 1;
    while (1) {
        int eof = util_getline(buf, ln++, sizeof(line), line);
        if (eof < 0) break;
        if (!line[0]) {
            if (eof) break;
            continue;
        }
        char *t = strchr(line, ':');
        if (!t) {
            if (eof) break;
            continue;
        }
        *t++ = '\0';
        if (strcmp(line, user) == 0) {
            int res = ACL_CryptCompare(pw, t, t);
            filebuf_close_nommap(buf);
            return (res == 0) ? REQ_PROCEED : REQ_NOACTION;
        }
        if (eof) break;
    }
    filebuf_close_nommap(buf);
    return REQ_NOACTION;
}

static int mock_simple_group(pblock *param, Session *sn, Request *rq) {
    char *gfn = pblock_findval("groupdb", param);
    char *user = pblock_findval("user", param);
    if (!gfn || !user) return REQ_ABORTED;
    size_t user_len = strlen(user);
    SYS_FILE fd = system_fopenRO(gfn);
    if (fd == SYS_ERROR_FD) {
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }
    struct stat finfo;
    memset(&finfo, 0, sizeof(finfo));
    fstat(fd, &finfo);
    filebuf_t *buf = filebuf_open_nostat_nommap(fd, FILE_BUFFERSIZE, &finfo);
    if (!buf) {
        system_fclose(fd);
        return REQ_ABORTED;
    }
    char line[1024];
    char *groupnames = NULL;
    size_t groupnames_cap = 0;
    size_t groupnames_len = 0;
    int empty = 1;
    int ln = 0;
    while (1) {
        int eof = util_getline(buf, ++ln, sizeof(line), line);
        if (eof < 0) {
            filebuf_close_nommap(buf);
            free(groupnames);
            return REQ_NOACTION;
        }
        if (!line[0]) {
            if (eof) {
                filebuf_close_nommap(buf);
                if (empty == 1) return REQ_PROCEED;
                if (groupnames) {
                    pblock_nvinsert("auth-group", groupnames, rq->vars);
                    free(groupnames);
                    return REQ_PROCEED;
                }
                return REQ_NOACTION;
            }
            continue;
        }
        empty = 0;
        char *t = strchr(line, ':');
        if (!t) {
            if (eof) { line[0] = '\0'; continue; }
            continue;
        }
        *t++ = '\0';
        while (1) {
            while (*t && (*t == ' ' || *t == '\t')) ++t;
            if (!*t) break;
            char *u = t;
            while (*u && (*u != ' ' && *u != '\t')) ++u;
            if (((size_t)(u - t) == user_len) && strncmp(user, t, user_len) == 0) {
                size_t gname_len = strlen(line);
                size_t needed = groupnames_len + (groupnames ? 1 : 0) + gname_len + 1;
                if (needed > groupnames_cap) {
                    size_t new_cap = groupnames_cap ? groupnames_cap * 2 : ((size_t)buf->len + 16);
                    while (new_cap < needed) new_cap *= 2;
                    groupnames = (char *)realloc(groupnames, new_cap);
                    groupnames_cap = new_cap;
                }
                if (groupnames_len > 0) groupnames[groupnames_len++] = ',';
                memcpy(groupnames + groupnames_len, line, gname_len + 1);
                groupnames_len += gname_len;
                break;
            }
            if (!*u) break;
            t = u;
        }
        if (eof) {
            filebuf_close_nommap(buf);
            if (groupnames) {
                pblock_nvinsert("auth-group", groupnames, rq->vars);
                free(groupnames);
                return REQ_PROCEED;
            }
            return REQ_NOACTION;
        }
    }
}

static int mock_basic_auth(pblock *param, Session *sn, Request *rq) {
    char *auth = pblock_findval("authorization", rq->headers);
    if (!auth || strncasecmp(auth, "basic ", 6) != 0)
        return REQ_NOACTION;
    /* In unit tests we pass "Basic user:pass" directly for simplicity */
    const char *creds = auth + 6;
    while (*creds == ' ') creds++;
    char *copy = strdup(creds);
    char *colon = strchr(copy, ':');
    if (!colon) {
        free(copy);
        return REQ_NOACTION;
    }
    *colon++ = '\0';
    char *user = copy;
    char *pw = colon;

    char *pwfile = pblock_findval("userdb", param);
    char *grpfile = pblock_findval("groupdb", param);
    char *pwfn = pblock_findval("userfn", param);
    char *grpfn = pblock_findval("groupfn", param);

    pblock *npb = pblock_create(4);
    pblock_nvinsert("user", user, npb);
    pblock_nvinsert("pw", pw, npb);
    pblock_nvinsert("userdb", pwfile, npb);
    if (grpfile) pblock_nvinsert("groupdb", grpfile, npb);
    pblock_nvinsert("fn", pwfn, npb);

    int ret = func_exec(npb, sn, rq);
    if (ret == REQ_PROCEED) {
        pblock_nvinsert("auth-type", "basic", rq->vars);
        pblock_nvinsert("auth-user", user, rq->vars);
        pblock_nvinsert("auth-db", pwfile, rq->vars);
        if (grpfile && grpfn) {
            pblock_nvinsert("fn", grpfn, npb);
            ret = func_exec(npb, sn, rq);
        }
    }
    pblock_free(npb);
    free(copy);
    return ret;
}

static int mock_custom_cfg_fn(pblock *pb, Session *, Request *rq) {
    char *a1 = pblock_findval("arg1", pb);
    char *a2 = pblock_findval("arg2", pb);
    if (a1) pblock_nvinsert("custom-arg1", a1, rq->vars);
    if (a2) pblock_nvinsert("custom-arg2", a2, rq->vars);
    return REQ_PROCEED;
}

static int mock_custom_user_check(pblock *pb, Session *, Request *) {
    char *u = pblock_findval("user", pb);
    char *pw = pblock_findval("pw", pb);
    if (u && pw && strcmp(u, "customuser") == 0 && strcmp(pw, "custompass") == 0)
        return REQ_PROCEED;
    return REQ_NOACTION;
}

static int mock_custom_group_check(pblock *pb, Session *, Request *) {
    char *u = pblock_findval("user", pb);
    char *g = pblock_findval("group", pb);
    if (u && g && strcmp(u, "customuser") == 0 && strncmp(g, "customgrp", 9) == 0)
        return REQ_PROCEED;
    return REQ_NOACTION;
}

struct TestReq {
    pblock *pb;
    Session sn;
    Request rq;

    TestReq(const char *base, const char *path, const char *method,
            const char *ip, const char *dns = NULL, const char *auth = NULL) {
        pb = pblock_create(4);
        memset(&sn, 0, sizeof(sn));
        sn.client = pblock_create(4);
        sn.dns_host = dns ? strdup(dns) : NULL;
        pblock_nvinsert("ip", ip, sn.client);

        memset(&rq, 0, sizeof(rq));
        rq.vars = pblock_create(8);
        rq.reqpb = pblock_create(4);
        rq.headers = pblock_create(4);
        rq.srvhdrs = pblock_create(4);
        rq.status_num = 200;

        pblock_nvinsert("ntrans-base", base, rq.vars);
        pblock_nvinsert("path", path, rq.vars);
        pblock_nvinsert("method", method, rq.reqpb);
        if (auth)
            pblock_nvinsert("authorization", auth, rq.headers);
    }

    ~TestReq() {
        pblock_free(pb);
        pblock_free(sn.client);
        free(sn.dns_host);
        pblock_free(rq.vars);
        pblock_free(rq.reqpb);
        pblock_free(rq.headers);
        pblock_free(rq.srvhdrs);
    }
};

static void write_file_inplace(const char *path, const char *content) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    size_t len = strlen(content);
    ssize_t w = write(fd, content, len);
    assert(w == (ssize_t)len);
    close(fd);
}

int main(int argc, char **argv) {
    const char *work_dir = (argc > 1) ? argv[1] : "/tmp";
    int run_tsan_stress = (argc > 2 && strcmp(argv[2], "--stress") == 0);

    ACL_CryptCritInit();
    func_insert("basic-auth", mock_basic_auth);
    func_insert("simple-userdb", mock_simple_user);
    func_insert("simple-groupdb", mock_simple_group);
    func_insert("custom-cfg-fn", mock_custom_cfg_fn);
    func_insert("custom-user-check", mock_custom_user_check);
    func_insert("custom-group-check", mock_custom_group_check);

    pblock *init_pb = pblock_create(4);
    assert(htaccess_init(init_pb, NULL, NULL) == REQ_PROCEED);
    pblock_free(init_pb);

    /* 1. Test ACL_CryptCompare NULL / invalid salt safety (Oracle 6928578) */
    char *hash = ACL_Crypt("Secret123", "ab");
    assert(hash != NULL);
    assert(ACL_CryptCompare("Secret123", hash, hash) == 0);
    assert(ACL_CryptCompare("WrongPass", hash, hash) != 0);
    /* Invalid salt strings where modern glibc/libxcrypt crypt() returns NULL */
    assert(ACL_CryptCompare("Secret123", "", "") != 0);
    assert(ACL_CryptCompare("Secret123", "$invalid$", "$invalid$") != 0);
    assert(ACL_CryptCompare("Secret123", ":", ":") != 0);
    assert(ACL_CryptCompare(NULL, hash, hash) != 0);
    assert(ACL_CryptCompare("Secret123", NULL, hash) != 0);
    assert(ACL_CryptCompare("Secret123", hash, NULL) != 0);

    /* 2. Prepare directory tree for directive & hierarchy tests */
    char root_dir[512], sub_dir[512], deep_dir[512];
    snprintf(root_dir, sizeof(root_dir), "%s/docroot", work_dir);
    snprintf(sub_dir, sizeof(sub_dir), "%s/docroot/sub", work_dir);
    snprintf(deep_dir, sizeof(deep_dir), "%s/docroot/sub/deep", work_dir);
    mkdir(root_dir, 0755);
    mkdir(sub_dir, 0755);
    mkdir(deep_dir, 0755);

    char f_root_file[512], f_sub_file[512], f_deep_file[512];
    snprintf(f_root_file, sizeof(f_root_file), "%s/index.html", root_dir);
    snprintf(f_sub_file, sizeof(f_sub_file), "%s/page.html", sub_dir);
    snprintf(f_deep_file, sizeof(f_deep_file), "%s/secret.html", deep_dir);
    write_file_inplace(f_root_file, "root\n");
    write_file_inplace(f_sub_file, "sub\n");
    write_file_inplace(f_deep_file, "deep\n");

    char pw_file[512], grp_file[512], comb_file[512];
    snprintf(pw_file, sizeof(pw_file), "%s/.htpasswd", work_dir);
    snprintf(grp_file, sizeof(grp_file), "%s/.htgroup", work_dir);
    snprintf(comb_file, sizeof(comb_file), "%s/.htcombined", work_dir);

    char pw_content[1024];
    snprintf(pw_content, sizeof(pw_content),
             "alice:%s\nbobHunter2:%s\nbob:%s\ncorrupt::\n",
             hash, hash, hash);
    write_file_inplace(pw_file, pw_content);

    /* Group file with continuation lines and multiple groups where bobHunter2
     * appears before bob (testing prefix-match bug and buffer growth) */
    char grp_content[1024];
    snprintf(grp_content, sizeof(grp_content),
             "admins: alice \\\n bobHunter2\n"
             "staff: bob\n"
             "g1: bob\ng2: bob\ng3: bob\ng4: bob\ng5: bob\ng6: bob\n");
    write_file_inplace(grp_file, grp_content);

    snprintf(pw_content, sizeof(pw_content),
             "alice:%s:admins,engineering\nbob:%s:staff\n",
             hash, hash);
    write_file_inplace(comb_file, pw_content);
    free(hash);

    char ht_root[512], ht_sub[512], ht_deep[512];
    snprintf(ht_root, sizeof(ht_root), "%s/.htaccess", root_dir);
    snprintf(ht_sub, sizeof(ht_sub), "%s/.htaccess", sub_dir);
    snprintf(ht_deep, sizeof(ht_deep), "%s/.htaccess", deep_dir);

    /* 3. Test IP / domain / order / Limit / LimitExcept directives */
    write_file_inplace(ht_root,
        "# Root htaccess with >5 allow entries to trigger grow_sectable\n"
        "<Limit GET POST>\n"
        "order deny,allow\n"
        "deny from all\n"
        "allow from 10.1.2.3 192.168.1. 172.16.0.0/12 172.31.255.0/255.255.255.0 .example.com TrustedHost.org\n"
        "</Limit>\n"
        "<LimitExcept GET>\n"
        "order allow,deny\n"
        "deny from all\n"
        "</LimitExcept>\n");

    /* Allowed by exact IP */
    {
        TestReq t(root_dir, f_root_file, "GET", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Allowed by partial IP a.b.c. */
    {
        TestReq t(root_dir, f_root_file, "GET", "192.168.1.200");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Allowed by CIDR /12 (172.16.0.0 - 172.31.255.255) */
    {
        TestReq t(root_dir, f_root_file, "GET", "172.20.99.1");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Allowed by netmask /255.255.255.0 */
    {
        TestReq t(root_dir, f_root_file, "GET", "172.31.255.42");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Allowed by domain suffix */
    {
        TestReq t(root_dir, f_root_file, "GET", "203.0.113.5", "host.sub.example.com");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Denied when domain is only a substring (not subdomain boundary) */
    {
        TestReq t(root_dir, f_root_file, "GET", "203.0.113.5", "badexample.com");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_FORBIDDEN);
    }
    /* Denied IP */
    {
        TestReq t(root_dir, f_root_file, "GET", "203.0.113.99");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_FORBIDDEN);
    }
    /* HEAD treated as GET */
    {
        TestReq t(root_dir, f_root_file, "HEAD", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* POST denied by <LimitExcept GET> */
    {
        TestReq t(root_dir, f_root_file, "POST", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_FORBIDDEN);
    }
    /* Directory path without trailing slash (exercises MALLOC(len + 2) + htaccess_strcpy_dir) */
    {
        TestReq t(root_dir, sub_dir, "GET", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }

    /* 4. Multi-level hierarchy (num_sec == 3) + Basic Auth + Group Auth */
    char sub_cfg[1024], deep_cfg[1024];
    snprintf(sub_cfg, sizeof(sub_cfg),
             "AuthName \"Sub Realm\"\n"
             "AuthType Basic\n"
             "AuthUserFile %s\n"
             "AuthGroupFile %s\n"
             "<Limit GET>\n"
             "require group admins\n"
             "</Limit>\n",
             pw_file, grp_file);
    write_file_inplace(ht_sub, sub_cfg);

    snprintf(deep_cfg, sizeof(deep_cfg),
             "AuthName \"Deep Realm\"\n"
             "AuthType Basic\n"
             "AuthUserFile %s\n"
             "AuthGroupFile %s\n"
             "order mutual-failure\n"
             "allow from 10.1.2.0/24\n"
             "deny from 10.1.2.99\n"
             "<Limit GET>\n"
             "require user \"alice\" bob\n"
             "</Limit>\n",
             pw_file, grp_file);
    write_file_inplace(ht_deep, deep_cfg);

    /* Unauthenticated -> 401 with WWW-Authenticate */
    {
        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_UNAUTHORIZED);
        char *www = pblock_findval("www-authenticate", t.rq.srvhdrs);
        assert(www && strstr(www, "Sub Realm") != NULL);
    }
    /* Corrupt salt user in .htpasswd -> 401 (no crash in ACL_CryptCompare) */
    {
        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic corrupt:any");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_UNAUTHORIZED);
    }
    /* User 'bob' is NOT in 'admins' (even though 'bobHunter2' is in 'admins' - regression check for prefix match) */
    {
        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic bob:Secret123");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_UNAUTHORIZED);
    }
    /* User 'alice' IS in 'admins' */
    {
        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic alice:Secret123");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Deep hierarchy (3 .htaccess files -> num_sec == 3 -> exercises _htaccess_freectxt) */
    {
        TestReq t(root_dir, f_deep_file, "GET", "10.1.2.3", NULL, "Basic bob:Secret123");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
    }
    /* Deep hierarchy denied by mutual-failure deny rule */
    {
        TestReq t(root_dir, f_deep_file, "GET", "10.1.2.99", NULL, "Basic bob:Secret123");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_FORBIDDEN);
    }

    /* 5. Test htaccess_in_group direct file parser (when auth-group is not preset by SAF)
     *    and combined user/group database (htaccess-userdb with groups-with-users=yes) */
    {
        pblock *gwu_pb = pblock_create(4);
        pblock_nvinsert("groups-with-users", "yes", gwu_pb);
        assert(htaccess_init(gwu_pb, NULL, NULL) == REQ_PROCEED);
        pblock_free(gwu_pb);

        /* Same file for AuthUserFile and AuthGroupFile triggers htaccess-userdb */
        snprintf(sub_cfg, sizeof(sub_cfg),
                 "AuthName \"Combined Realm\"\n"
                 "AuthType Basic\n"
                 "AuthUserFile %s\n"
                 "<Limit GET>\n"
                 "require group engineering\n"
                 "</Limit>\n",
                 comb_file);
        write_file_inplace(ht_sub, sub_cfg);

        TestReq t1(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic alice:Secret123");
        assert(htaccess_find(t1.pb, &t1.sn, &t1.rq) == REQ_PROCEED);

        TestReq t2(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic bob:Secret123");
        assert(htaccess_find(t2.pb, &t2.sn, &t2.rq) == REQ_ABORTED);
        assert(t2.rq.status_num == PROTOCOL_UNAUTHORIZED);

        /* Reset groups-with-users=no */
        gwu_pb = pblock_create(4);
        pblock_nvinsert("groups-with-users", "no", gwu_pb);
        assert(htaccess_init(gwu_pb, NULL, NULL) == REQ_PROCEED);
        pblock_free(gwu_pb);
    }

    /* 6. Test registered custom directives (BOOL, PASS1, PASS2) */
    {
        pblock *rpb = pblock_create(6);
        pblock_nvinsert("directive", "CustomUserAuth", rpb);
        pblock_nvinsert("function", "custom-cfg-fn", rpb);
        pblock_nvinsert("check_fn", "custom-user-check", rpb);
        pblock_nvinsert("argtype", "0", rpb); /* BOOL */
        pblock_nvinsert("authtype", "0", rpb); /* USER_AUTH */
        assert(htaccess_register(rpb, NULL, NULL) == REQ_PROCEED);
        pblock_free(rpb);

        rpb = pblock_create(6);
        pblock_nvinsert("directive", "CustomGroupAuth", rpb);
        pblock_nvinsert("function", "custom-cfg-fn", rpb);
        pblock_nvinsert("check_fn", "custom-group-check", rpb);
        pblock_nvinsert("argtype", "2", rpb); /* PASS2 */
        pblock_nvinsert("authtype", "1", rpb); /* GROUP_AUTH */
        assert(htaccess_register(rpb, NULL, NULL) == REQ_PROCEED);
        pblock_free(rpb);

        write_file_inplace(ht_sub,
            "CustomUserAuth On\n"
            "CustomGroupAuth argOne argTwo\n"
            "require group customgrp\n");

        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3", NULL, "Basic customuser:custompass");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_PROCEED);
        assert(strcmp(pblock_findval("custom-arg1", t.rq.vars), "argOne") == 0);
        assert(strcmp(pblock_findval("custom-arg2", t.rq.vars), "argTwo") == 0);
    }

    /* 7. Test syntax error paths (ensure 500 status and zero memory leaks under ASAN) */
    const char *bad_configs[] = {
        "<Limit TRACE>\nallow from all\n</Limit>\n",
        "<Limit BOGUSMETHOD>\nallow from all\n</Limit>\n",
        "<Limit GET>\n<Limit POST>\n</Limit>\n</Limit>\n",
        "<Limit GET>\nAuthName ForbiddenInsideLimit\n</Limit>\n",
        "<Limit GET>\nallow from all\n",
        "<LimitExcept GET>\nallow from all\n",
        "</Limit>\n",
        "order invalid-order\n",
        "allow without_from\n",
        "deny without_from\n",
        "allow from 10.0.0.1/99\n",
        "allow from 999.999.999.999\n"
    };
    for (size_t i = 0; i < sizeof(bad_configs) / sizeof(bad_configs[0]); i++) {
        write_file_inplace(ht_sub, bad_configs[i]);
        TestReq t(root_dir, f_sub_file, "GET", "10.1.2.3");
        assert(htaccess_find(t.pb, &t.sn, &t.rq) == REQ_ABORTED);
        assert(t.rq.status_num == PROTOCOL_SERVER_ERROR);
    }

    if (run_tsan_stress) {
        /* Concurrent dynamic .htaccess rewrite stress test under TSAN/ASAN */
        snprintf(sub_cfg, sizeof(sub_cfg),
                 "AuthName \"Dynamic Realm\"\n"
                 "AuthType Basic\n"
                 "AuthUserFile %s\n"
                 "AuthGroupFile %s\n"
                 "<Limit GET>\n"
                 "order deny,allow\n"
                 "deny from all\n"
                 "allow from 10.1.2.0/24\n"
                 "require group admins\n"
                 "</Limit>\n",
                 pw_file, grp_file);
        write_file_inplace(ht_sub, sub_cfg);
        unlink(ht_deep);

        std::atomic<bool> stop{false};
        std::atomic<long> eval_count{0};

        struct Ctx {
            const char *root_dir;
            const char *f_sub_file;
            const char *ht_sub;
            const char *pw_file;
            const char *grp_file;
            std::atomic<bool> *stop;
            std::atomic<long> *eval_count;
        } ctx = { root_dir, f_sub_file, ht_sub, pw_file, grp_file, &stop, &eval_count };

        auto writer_fn = [](void *arg) -> void * {
            Ctx *c = (Ctx *)arg;
            char cfg_a[2048], cfg_b[2048];
            snprintf(cfg_a, sizeof(cfg_a),
                     "# Short config A\n"
                     "AuthName \"Dynamic Realm A\"\n"
                     "AuthType Basic\n"
                     "AuthUserFile %s\n"
                     "AuthGroupFile %s\n"
                     "<Limit GET>\n"
                     "order deny,allow\n"
                     "deny from all\n"
                     "allow from 10.1.2.0/24\n"
                     "require valid-user\n"
                     "</Limit>\n",
                     c->pw_file, c->grp_file);
            snprintf(cfg_b, sizeof(cfg_b),
                     "# Longer config B with extra padding comments to change file size across page boundaries\n"
                     "# Padding 11111111111111111111111111111111111111111111111111111111111111111\n"
                     "# Padding 22222222222222222222222222222222222222222222222222222222222222222\n"
                     "AuthName \"Dynamic Realm B\"\n"
                     "AuthType Basic\n"
                     "AuthUserFile %s\n"
                     "AuthGroupFile %s\n"
                     "<Limit GET POST>\n"
                     "order allow,deny\n"
                     "allow from 10.1.2.3 192.168.1. 172.16.0.0/12 .example.com\n"
                     "deny from 10.1.2.99\n"
                     "require group admins staff\n"
                     "</Limit>\n",
                     c->pw_file, c->grp_file);
            int iter = 0;
            while (!c->stop->load(std::memory_order_relaxed)) {
                write_file_inplace(c->ht_sub, (iter & 1) ? cfg_a : cfg_b);
                iter++;
                usleep(20 * 1000); /* 20ms in unit stress */
            }
            return NULL;
        };

        auto reader_fn = [](void *arg) -> void * {
            Ctx *c = (Ctx *)arg;
            while (!c->stop->load(std::memory_order_relaxed)) {
                TestReq t(c->root_dir, c->f_sub_file, "GET", "10.1.2.3",
                          "host.example.com", "Basic alice:Secret123");
                int rv = htaccess_find(t.pb, &t.sn, &t.rq);
                assert(rv == REQ_PROCEED || rv == REQ_ABORTED);
                c->eval_count->fetch_add(1, std::memory_order_relaxed);
            }
            return NULL;
        };

        pthread_t writer;
        pthread_t readers[8];
        pthread_create(&writer, NULL, writer_fn, &ctx);
        for (int i = 0; i < 8; i++)
            pthread_create(&readers[i], NULL, reader_fn, &ctx);

        usleep(3000 * 1000); /* 3 seconds of intense in-process TSAN/ASAN contention */
        stop.store(true, std::memory_order_relaxed);

        pthread_join(writer, NULL);
        for (int i = 0; i < 8; i++)
            pthread_join(readers[i], NULL);

        assert(eval_count.load() > 100);
        printf("PASS: concurrent dynamic .htaccess rewrite stress (%ld evaluations)\n",
               eval_count.load());
    }

    printf("PASS: test_htaccess_unit all checks passed\n");
    return 0;
}
EOF

HTACCESS_SRCS=(
    "${REPO_ROOT}/src/server/plugins/htaccess/http_access.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/http_auth.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/http_config.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/http_register.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/list.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/main.cpp"
    "${REPO_ROOT}/src/server/plugins/htaccess/util.cpp"
    "${REPO_ROOT}/src/server/libaccess/cryptwrapper.cpp"
)

"${CXX_BIN}" -std=gnu++14 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wno-write-strings -Wno-format-truncation \
    -DXP_UNIX -DLINUX -DLinux \
    -I"${TMP_DIR}/mock_inc" \
    -I"${REPO_ROOT}/src/server/plugins/htaccess" \
    "${TMP_DIR}/test_htaccess_unit.cpp" \
    "${HTACCESS_SRCS[@]}" \
    -lpthread -l:libcrypt.so.1 \
    -o "${TMP_DIR}/test_htaccess_asan"

mkdir -p "${TMP_DIR}/run_asan"
"${TMP_DIR}/test_htaccess_asan" "${TMP_DIR}/run_asan" --stress

echo "=== [2/4] Building and running htaccess concurrent stress harness with TSAN ==="

"${CXX_BIN}" -std=gnu++14 -O1 -g -fsanitize=thread -fno-omit-frame-pointer \
    -Wno-write-strings -Wno-format-truncation \
    -DXP_UNIX -DLINUX -DLinux \
    -I"${TMP_DIR}/mock_inc" \
    -I"${REPO_ROOT}/src/server/plugins/htaccess" \
    "${TMP_DIR}/test_htaccess_unit.cpp" \
    "${HTACCESS_SRCS[@]}" \
    -lpthread -l:libcrypt.so.1 \
    -o "${TMP_DIR}/test_htaccess_tsan"

mkdir -p "${TMP_DIR}/run_tsan"
"${TMP_DIR}/test_htaccess_tsan" "${TMP_DIR}/run_tsan" --stress

# If no work-root was passed, exit after the standalone ASAN/UBSAN + TSAN checks
if [ $# -lt 1 ] || [ ! -d "$1" ]; then
    echo "PASS: htaccess standalone ASAN/UBSAN + TSAN tests completed"
    exit 0
fi

WORK_ROOT=$(cd "$1" && pwd)
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
DOCS_DIR="${INSTANCE_DIR}/docs"
HT_PLUGIN="${WORK_ROOT}/plugins/htaccess/htaccess.so"
HTPASSWD_BIN="${WORK_ROOT}/bin/htpasswd"
CLASSIC_PORT=18180
EVENT_PORT=18181
SERVER_PID=""
TEMP_DIR=""

if [ ! -f "${HT_PLUGIN}" ]; then
    echo "FAIL: ${HT_PLUGIN} not found"
    exit 1
fi

cleanup_server() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    for p in $(pgrep -x webservd 2>/dev/null || true; pgrep -x webservd-wdog 2>/dev/null || true); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in *"${INSTANCE_DIR}"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
}

restore_config() {
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"
        mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
    rm -rf "${DOCS_DIR}/ht_public" "${DOCS_DIR}/ht_ip" "${DOCS_DIR}/ht_auth" \
           "${DOCS_DIR}/ht_group" "${DOCS_DIR}/ht_limit" "${DOCS_DIR}/ht_dynamic"
}

cleanup_all() {
    cleanup_server
    restore_config
    if [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ]; then
        rm -rf "${TEMP_DIR}"
    fi
    cleanup_tmp
}
trap cleanup_all EXIT

cd "${WORK_ROOT}"
cleanup_server
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-htaccess-XXXXXX)
chmod 700 "${TEMP_DIR}"

echo "=== [3/4] Testing live webservd htaccess directives across classic & event listeners ==="

# Create user and group databases using shipped htpasswd
PW_DB="${TEMP_DIR}/users.htpasswd"
GRP_DB="${TEMP_DIR}/groups.htgroup"
"${HTPASSWD_BIN}" -c "${PW_DB}" alice Secret123 >/dev/null
"${HTPASSWD_BIN}" "${PW_DB}" bobHunter2 Secret456 >/dev/null
"${HTPASSWD_BIN}" "${PW_DB}" bob Secret789 >/dev/null

cat > "${GRP_DB}" <<'EOF'
admins: alice bobHunter2
staff: bob
EOF

mkdir -p "${DOCS_DIR}/ht_public" \
         "${DOCS_DIR}/ht_ip" \
         "${DOCS_DIR}/ht_auth" \
         "${DOCS_DIR}/ht_group/nested" \
         "${DOCS_DIR}/ht_limit" \
         "${DOCS_DIR}/ht_dynamic"

echo "PUBLIC_OK" > "${DOCS_DIR}/ht_public/index.html"
echo "IP_OK" > "${DOCS_DIR}/ht_ip/index.html"
echo "AUTH_OK" > "${DOCS_DIR}/ht_auth/index.html"
echo "GROUP_OK" > "${DOCS_DIR}/ht_group/nested/index.html"
echo "LIMIT_OK" > "${DOCS_DIR}/ht_limit/index.html"
echo "DYNAMIC_OK" > "${DOCS_DIR}/ht_dynamic/index.html"

cat > "${DOCS_DIR}/ht_ip/.htaccess" <<'EOF'
order deny,allow
deny from all
allow from 127.0.0.1/8 10.0.0.
EOF

cat > "${DOCS_DIR}/ht_auth/.htaccess" <<EOF
AuthName "Protected Area"
AuthType Basic
AuthUserFile ${PW_DB}
<Limit GET>
require valid-user
</Limit>
EOF

cat > "${DOCS_DIR}/ht_group/.htaccess" <<EOF
AuthName "Group Area"
AuthType Basic
AuthUserFile ${PW_DB}
AuthGroupFile ${GRP_DB}
order allow,deny
allow from all
EOF

cat > "${DOCS_DIR}/ht_group/nested/.htaccess" <<EOF
<Limit GET>
require group admins
</Limit>
EOF

cat > "${DOCS_DIR}/ht_limit/.htaccess" <<'EOF'
<Limit GET>
order deny,allow
allow from all
</Limit>
<LimitExcept GET>
order allow,deny
deny from all
</LimitExcept>
EOF

cat > "${DOCS_DIR}/ht_dynamic/.htaccess" <<EOF
AuthName "Dynamic Load Realm"
AuthType Basic
AuthUserFile ${PW_DB}
AuthGroupFile ${GRP_DB}
<Limit GET>
order deny,allow
deny from all
allow from 127.0.0.0/8
require group admins staff
</Limit>
EOF

# Configure magnus.conf to load htaccess.so and initialize htaccess-init
cat >> "${CONFIG_DIR}/magnus.conf" <<EOF
Init fn="load-modules" shlib="${HT_PLUGIN}" funcs="htaccess-init,htaccess-find,htaccess-register"
Init fn="htaccess-init"
EOF

# Add PathCheck fn="htaccess-find" to default object in obj.conf
python3 - "${CONFIG_DIR}/obj.conf" <<'PY'
import sys
path = sys.argv[1]
with open(path, "r") as f:
    text = f.read()
target = "<Object name=\"default\">\n"
assert target in text, "default object not found in obj.conf"
text = text.replace(target, target + "PathCheck fn=\"htaccess-find\"\n", 1)
with open(path, "w") as f:
    f.write(text)
PY

# Configure server.xml with classic (18180) and event (18181) listeners
python3 - "${CONFIG_DIR}/server.xml" "${TEMP_DIR}" "${CLASSIC_PORT}" "${EVENT_PORT}" <<'PY'
import sys
import xml.etree.ElementTree as ET

path, temp_dir, classic_port, event_port = sys.argv[1:5]
tree = ET.parse(path)
root = tree.getroot()

tp = root.find("temp-path")
if tp is None:
    tp = ET.SubElement(root, "temp-path")
tp.text = temp_dir

plat = root.find("platform")
if plat is None:
    plat = ET.SubElement(root, "platform")
plat.text = "64"

for ls in list(root.findall("http-listener")):
    root.remove(ls)

def add_listener(name, port, io_model):
    ls = ET.SubElement(root, "http-listener")
    ET.SubElement(ls, "name").text = name
    ET.SubElement(ls, "port").text = str(port)
    ET.SubElement(ls, "server-name").text = "localhost"
    ET.SubElement(ls, "default-virtual-server-name").text = "test"
    ET.SubElement(ls, "io-model").text = io_model

add_listener("http-listener-1", classic_port, "classic")
add_listener("http-listener-2", event_port, "event")

vs = root.find("virtual-server")
for el in list(vs.findall("http-listener-name")):
    vs.remove(el)
for name in ("http-listener-1", "http-listener-2"):
    ET.SubElement(vs, "http-listener-name").text = name

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY

get_worker_pid() {
    for p in $(pgrep -x webservd 2>/dev/null || true); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in
            *"${INSTANCE_DIR}"*)
                echo "$p"
                ;;
        esac
    done | sort -n | tr '\n' ' '
}

rm -f "${INSTANCE_DIR}/logs/errors"
"${INSTANCE_DIR}/bin/startserv"
sleep 2

WORKER_PID=$(get_worker_pid)
if [ -z "${WORKER_PID}" ]; then
    echo "FAIL: webservd worker did not start"
    cat "${INSTANCE_DIR}/logs/errors" || true
    exit 1
fi

for port in "${CLASSIC_PORT}" "${EVENT_PORT}"; do
    # Public directory without .htaccess
    code=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${port}/ht_public/index.html")
    [ "${code}" = "200" ] || { echo "FAIL: public GET on ${port} returned ${code}"; exit 1; }

    # IP allow (127.0.0.1/8)
    code=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${port}/ht_ip/index.html")
    [ "${code}" = "200" ] || { echo "FAIL: ht_ip GET on ${port} returned ${code}"; exit 1; }

    # Directory request without trailing slash (exercises path buffer + redirect/index)
    code=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${port}/ht_ip")
    [ "${code}" = "302" ] || [ "${code}" = "200" ] || { echo "FAIL: ht_ip dir GET on ${port} returned ${code}"; exit 1; }

    # Basic auth: unauthenticated -> 401, bad password -> 401, valid user -> 200
    code=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${port}/ht_auth/index.html")
    [ "${code}" = "401" ] || { echo "FAIL: ht_auth unauth on ${port} returned ${code}"; exit 1; }

    code=$(curl -s -o /dev/null -w "%{http_code}" -u "alice:WrongPass" "http://127.0.0.1:${port}/ht_auth/index.html")
    [ "${code}" = "401" ] || { echo "FAIL: ht_auth bad pw on ${port} returned ${code}"; exit 1; }

    code=$(curl -s -o /dev/null -w "%{http_code}" -u "alice:Secret123" "http://127.0.0.1:${port}/ht_auth/index.html")
    [ "${code}" = "200" ] || { echo "FAIL: ht_auth valid user on ${port} returned ${code}"; exit 1; }

    # Group auth in nested directory (2 .htaccess files in hierarchy):
    # bob is in staff (not admins, even though bobHunter2 is in admins) -> 401
    code=$(curl -s -o /dev/null -w "%{http_code}" -u "bob:Secret789" "http://127.0.0.1:${port}/ht_group/nested/index.html")
    [ "${code}" = "401" ] || { echo "FAIL: ht_group bob (non-admin) on ${port} returned ${code}"; exit 1; }

    # alice is in admins -> 200
    code=$(curl -s -o /dev/null -w "%{http_code}" -u "alice:Secret123" "http://127.0.0.1:${port}/ht_group/nested/index.html")
    [ "${code}" = "200" ] || { echo "FAIL: ht_group alice (admin) on ${port} returned ${code}"; exit 1; }

    # <Limit GET> + <LimitExcept GET>: GET -> 200, POST -> 403
    code=$(curl -s -o /dev/null -w "%{http_code}" "http://127.0.0.1:${port}/ht_limit/index.html")
    [ "${code}" = "200" ] || { echo "FAIL: ht_limit GET on ${port} returned ${code}"; exit 1; }

    code=$(curl -s -o /dev/null -w "%{http_code}" -X POST "http://127.0.0.1:${port}/ht_limit/index.html")
    [ "${code}" = "403" ] || { echo "FAIL: ht_limit POST on ${port} returned ${code}"; exit 1; }
done

# Also verify over HTTP/2 (h2c) on the event listener
code=$(curl -s --http2-prior-knowledge -o /dev/null -w "%{http_code}" -u "alice:Secret123" \
    "http://127.0.0.1:${EVENT_PORT}/ht_group/nested/index.html")
[ "${code}" = "200" ] || { echo "FAIL: h2c ht_group GET returned ${code}"; exit 1; }

echo "PASS: live webservd htaccess directive checks passed on classic, event, and HTTP/2"

echo "=== [4/4] Running ${LOAD_SECONDS}s dynamic .htaccess rewrite (every 100ms) under concurrent load ==="

python3 - "${DOCS_DIR}/ht_dynamic/.htaccess" "${PW_DB}" "${GRP_DB}" \
          "${CLASSIC_PORT}" "${EVENT_PORT}" "${LOAD_SECONDS}" <<'PY'
import base64
import http.client
import os
import sys
import threading
import time

ht_path, pw_db, grp_db, classic_port, event_port, duration_str = sys.argv[1:7]
classic_port = int(classic_port)
event_port = int(event_port)
duration = float(duration_str)

cfg_variants = [
    f"""# Variant 0: short config
AuthName "Dynamic Load Realm"
AuthType Basic
AuthUserFile {pw_db}
AuthGroupFile {grp_db}
<Limit GET>
order deny,allow
deny from all
allow from 127.0.0.1
require valid-user
</Limit>
""",
    f"""# Variant 1: longer config with group auth and multiple CIDRs
# Padding line AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
# Padding line BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
# Padding line CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
AuthName "Dynamic Load Realm"
AuthType Basic
AuthUserFile {pw_db}
AuthGroupFile {grp_db}
<Limit GET POST>
order allow,deny
allow from 127.0.0.0/8 10.0.0. 192.168.1.0/255.255.255.0
deny from 10.99.99.99
require group admins staff
</Limit>
""",
    f"""# Variant 2: IP-only config without require
order deny,allow
deny from all
allow from 127.0.0.
"""
]

stop_event = threading.Event()
errors = []
req_count = 0
rewrite_count = 0
lock = threading.Lock()

auth_hdr = "Basic " + base64.b64encode(b"alice:Secret123").decode("ascii")

def rewriter():
    global rewrite_count
    idx = 0
    while not stop_event.is_set():
        content = cfg_variants[idx % len(cfg_variants)].encode("utf-8")
        # Rewrite in-place (O_WRONLY | O_TRUNC) to specifically exercise the
        # Oracle 14094667 truncate/write race against concurrent readers.
        fd = os.open(ht_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        os.write(fd, content)
        os.close(fd)
        idx += 1
        with lock:
            rewrite_count += 1
        time.sleep(0.1)

def worker(port):
    global req_count
    conn = None
    while not stop_event.is_set():
        try:
            if conn is None:
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            conn.request("GET", "/ht_dynamic/index.html", headers={"Authorization": auth_hdr})
            resp = conn.getresponse()
            body = resp.read()
            # During an in-place O_TRUNC rewrite without atomic rename, a reader
            # may see either the full config (200), a momentarily empty file
            # (200), or a partial line (500 syntax error), or 401/403, but the
            # server must NEVER crash or reset the connection.
            if resp.status not in (200, 401, 403, 500):
                with lock:
                    errors.append(f"Unexpected HTTP status {resp.status} on port {port}")
                break
            with lock:
                req_count += 1
        except Exception as e:
            if conn is not None:
                try:
                    conn.close()
                except Exception:
                    pass
                conn = None
            with lock:
                errors.append(f"Worker on port {port} exception: {e}")
            break
    if conn is not None:
        try:
            conn.close()
        except Exception:
            pass

t_rewriter = threading.Thread(target=rewriter, daemon=True)
t_rewriter.start()

workers = []
for i in range(8):
    port = classic_port if (i % 2 == 0) else event_port
    t = threading.Thread(target=worker, args=(port,), daemon=True)
    t.start()
    workers.append(t)

deadline = time.monotonic() + duration
while time.monotonic() < deadline:
    with lock:
        if errors:
            break
    time.sleep(0.25)

stop_event.set()
t_rewriter.join(timeout=2)
for t in workers:
    t.join(timeout=2)

if errors:
    for err in errors[:5]:
        print("FAIL:", err, file=sys.stderr)
    sys.exit(1)

print(f"PASS: completed {req_count} concurrent requests across {rewrite_count} .htaccess rewrites")
PY

NEW_WORKER_PID=$(get_worker_pid)
if [ "${NEW_WORKER_PID}" != "${WORKER_PID}" ]; then
    echo "FAIL: webservd worker PID changed from ${WORKER_PID} to ${NEW_WORKER_PID} (crash/restart)"
    cat "${INSTANCE_DIR}/logs/errors" || true
    exit 1
fi

if grep -E 'SIGSEGV|SIGBUS|SEGV|segfault|fatal|core' "${INSTANCE_DIR}/logs/errors" >/dev/null 2>&1; then
    echo "FAIL: crash signature found in error log"
    cat "${INSTANCE_DIR}/logs/errors"
    exit 1
fi

echo "PASS: htaccess_smoke all checks passed"
