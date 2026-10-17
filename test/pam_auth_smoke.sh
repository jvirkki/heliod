#!/bin/bash
#
# PAM authentication conversation and password-aging smoke test.
#
# Compiles src/server/libaccess/pamauth.c under AddressSanitizer +
# UndefinedBehaviorSanitizer against a mock PAM framework modeled on Solaris
# libpam's free_resp() and Linux-PAM's _pam_drop_reply(), and verifies:
#   1. Standard PAM authentication with preset PAM_AUTHTOK succeeds (0).
#   2. Password-aging warning during pam_acct_mgmt() (emitting PAM_TEXT_INFO
#      and PAM_ERROR_MSG messages through the conversation callback followed
#      by free_resp()) succeeds without freeing non-heap (.rodata) pointers.
#   3. Expired-password account where pam_acct_mgmt() emits PAM_ERROR_MSG +
#      PAM_TEXT_INFO and returns PAM_NEW_AUTHTOK_REQD / PAM_ACCT_EXPIRED fails
#      cleanly without crashing or leaking responses in free_resp().
#   4. PAM module that prompts via PAM_PROMPT_ECHO_OFF (alongside
#      PAM_PROMPT_ECHO_ON, PAM_TEXT_INFO, and PAM_ERROR_MSG in a multi-message
#      batch) receives a heap-allocated copy of the password for
#      PAM_PROMPT_ECHO_OFF and NULL for informational/echo-on messages, and
#      free_resp() zeroes and frees the response cleanly.
#   5. Conversation error/edge handling: num_msg <= 0, NULL message array or
#      response pointer, NULL message element mid-batch (cleaning up earlier
#      allocations in the batch), and NULL appdata_ptr on PAM_PROMPT_ECHO_OFF.
#   6. pamauth_userexists() and pamauth_get_groups_for_user() for existing and
#      nonexistent users (verifying nonexistent user lookup returns -1 cleanly
#      without reading uninitialized passwd fields).
#
# Usage:
#   test/pam_auth_smoke.sh [heliod-work-root]

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

TMP_DIR=$(mktemp -d /tmp/heliod-pam-smoke-XXXXXX)
cleanup() {
    rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

mkdir -p "${TMP_DIR}/include/security" \
         "${TMP_DIR}/include/libaccess" \
         "${TMP_DIR}/include/base"

cat > "${TMP_DIR}/include/security/pam_appl.h" <<'EOF'
#ifndef _SECURITY_PAM_APPL_H
#define _SECURITY_PAM_APPL_H

typedef struct pam_handle pam_handle_t;

struct pam_message {
    int msg_style;
    const char *msg;
};

struct pam_response {
    char *resp;
    int resp_retcode;
};

struct pam_conv {
#if defined(LINUX) || defined(__linux__)
    int (*conv)(int num_msg, const struct pam_message **msg,
                struct pam_response **resp, void *appdata_ptr);
#else
    int (*conv)(int num_msg, struct pam_message **msg,
                struct pam_response **resp, void *appdata_ptr);
#endif
    void *appdata_ptr;
};

#define PAM_SUCCESS             0
#define PAM_OPEN_ERR            1
#define PAM_SYMBOL_ERR          2
#define PAM_SERVICE_ERR         3
#define PAM_SYSTEM_ERR          4
#define PAM_BUF_ERR             5
#define PAM_CONV_ERR            6
#define PAM_PERM_DENIED         7
#define PAM_MAXTRIES            8
#define PAM_AUTH_ERR            9
#define PAM_NEW_AUTHTOK_REQD    10
#define PAM_CRED_INSUFFICIENT   11
#define PAM_AUTHINFO_UNAVAIL    12
#define PAM_USER_UNKNOWN        13
#define PAM_ACCT_EXPIRED        18

#define PAM_PROMPT_ECHO_OFF     1
#define PAM_PROMPT_ECHO_ON      2
#define PAM_ERROR_MSG           3
#define PAM_TEXT_INFO           4
#define PAM_MSG_NOCONF          5
#define PAM_CONV_INTERRUPT      6

#define PAM_AUTHTOK             6

int pam_start(const char *service_name, const char *user,
              const struct pam_conv *pam_conversation,
              pam_handle_t **pamh);
int pam_end(pam_handle_t *pamh, int pam_status);
int pam_set_item(pam_handle_t *pamh, int item_type, const void *item);
int pam_authenticate(pam_handle_t *pamh, int flags);
int pam_acct_mgmt(pam_handle_t *pamh, int flags);

#endif
EOF

cat > "${TMP_DIR}/include/netsite.h" <<'EOF'
#ifndef NETSITE_H
#define NETSITE_H
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifndef uint_t
typedef unsigned int uint_t;
#endif
#define NSPR_BEGIN_EXTERN_C
#define NSPR_END_EXTERN_C
#define MALLOC(sz) malloc(sz)
#define CALLOC(sz) calloc(1, (sz))
#define REALLOC(p, sz) realloc((p), (sz))
#define FREE(p) free(p)
#define STRDUP(s) strdup(s)
#endif
EOF

cat > "${TMP_DIR}/include/definesEnterprise.h" <<'EOF'
#ifndef DEFINES_ENTERPRISE_H
#define DEFINES_ENTERPRISE_H
#define PRODUCT_DAEMON_BIN "webservd"
#endif
EOF

cat > "${TMP_DIR}/include/libaccess/aclerror.h" <<'EOF'
#ifndef ACLERROR_H
#define ACLERROR_H
#define LAS_EVAL_TRUE   0
#define LAS_EVAL_FALSE  (-1)
#define LAS_EVAL_FAIL   (-2)
#endif
EOF

cat > "${TMP_DIR}/include/base/util.h" <<'EOF'
#ifndef BASE_UTIL_H
#define BASE_UTIL_H
#include <pwd.h>
struct passwd *util_getpwnam(const char *name, struct passwd *result,
                             char *buffer, int buflen);
#endif
EOF

cp "${REPO_ROOT}/src/server/libaccess/pamauth.h" "${TMP_DIR}/include/libaccess/pamauth.h"

cat > "${TMP_DIR}/test_pamauth.c" <<'EOF'
#define _GNU_SOURCE
#include <assert.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>

/* Include the real pamauth.c implementation directly so we can also test
 * static converse() edge cases alongside the public pamauth_* APIs. */
#include "pamauth.c"

struct passwd *util_getpwnam(const char *name, struct passwd *result,
                             char *buffer, int buflen)
{
    struct passwd *res = NULL;
    if (getpwnam_r(name, result, buffer, (size_t)buflen, &res) != 0)
        return NULL;
    return res;
}

/* Exact replica of Solaris libpam free_resp() / Linux-PAM _pam_drop_reply():
 * zeroes and frees each non-NULL r->resp, then frees resp array. */
static void free_resp(int num_msg, struct pam_response *resp)
{
    if (resp != NULL) {
        struct pam_response *r = resp;
        for (int i = 0; i < num_msg; i++, r++) {
            if (r->resp != NULL) {
                memset(r->resp, 0, strlen(r->resp));
                free(r->resp);
                r->resp = NULL;
            }
        }
        free(resp);
    }
}

struct pam_handle {
    char *service;
    char *user;
    char *authtok;
    struct pam_conv conv;
};

typedef enum {
    MODE_PRESET_OK = 0,
    MODE_AGING_WARNING_OK,
    MODE_PASSWORD_EXPIRED,
    MODE_ACCT_EXPIRED,
    MODE_PROMPT_ECHO_OFF
} MockMode;

static MockMode g_mode = MODE_PRESET_OK;
static const char *g_expected_pw = "S3cretP@ss!";
static int g_conv_calls = 0;

int pam_start(const char *service_name, const char *user,
              const struct pam_conv *pam_conversation,
              pam_handle_t **pamh)
{
    if (!service_name || !user || !pam_conversation || !pamh)
        return PAM_SYSTEM_ERR;
    pam_handle_t *h = (pam_handle_t *)calloc(1, sizeof(pam_handle_t));
    if (!h)
        return PAM_BUF_ERR;
    h->service = strdup(service_name);
    h->user = strdup(user);
    h->conv = *pam_conversation;
    *pamh = h;
    return PAM_SUCCESS;
}

int pam_end(pam_handle_t *pamh, int pam_status)
{
    (void)pam_status;
    if (!pamh)
        return PAM_SUCCESS;
    free(pamh->service);
    free(pamh->user);
    if (pamh->authtok) {
        memset(pamh->authtok, 0, strlen(pamh->authtok));
        free(pamh->authtok);
    }
    free(pamh);
    return PAM_SUCCESS;
}

int pam_set_item(pam_handle_t *pamh, int item_type, const void *item)
{
    if (!pamh)
        return PAM_SYSTEM_ERR;
    if (item_type == PAM_AUTHTOK) {
        free(pamh->authtok);
        pamh->authtok = item ? strdup((const char *)item) : NULL;
        return PAM_SUCCESS;
    }
    return PAM_SYMBOL_ERR;
}

static int run_conversation(pam_handle_t *pamh, int num_msg,
                            struct pam_message *msgs,
                            struct pam_response **out_resp)
{
    const struct pam_message *ptrs[8];
    assert(num_msg > 0 && num_msg <= 8);
    for (int i = 0; i < num_msg; i++)
        ptrs[i] = &msgs[i];
    g_conv_calls++;
#if defined(LINUX) || defined(__linux__)
    return pamh->conv.conv(num_msg, ptrs, out_resp, pamh->conv.appdata_ptr);
#else
    return pamh->conv.conv(num_msg, (struct pam_message **)ptrs, out_resp,
                           pamh->conv.appdata_ptr);
#endif
}

int pam_authenticate(pam_handle_t *pamh, int flags)
{
    (void)flags;
    if (g_mode == MODE_PROMPT_ECHO_OFF) {
        /* Simulate a PAM module that prompts for password via conversation
         * along with an echo-on prompt and info banner. */
        struct pam_message msgs[3] = {
            { PAM_TEXT_INFO, "Authenticating against PAM" },
            { PAM_PROMPT_ECHO_ON, "Username: " },
            { PAM_PROMPT_ECHO_OFF, "Password: " }
        };
        struct pam_response *resp = NULL;
        int rc = run_conversation(pamh, 3, msgs, &resp);
        if (rc != PAM_SUCCESS || !resp)
            return PAM_CONV_ERR;
        int info_null = (resp[0].resp == NULL && resp[1].resp == NULL);
        int pw_ok = (resp[2].resp != NULL &&
                     strcmp(resp[2].resp, g_expected_pw) == 0);
        free_resp(3, resp);
        assert(info_null);
        return pw_ok ? PAM_SUCCESS : PAM_AUTH_ERR;
    }

    if (!pamh->authtok || strcmp(pamh->authtok, g_expected_pw) != 0)
        return PAM_AUTH_ERR;
    return PAM_SUCCESS;
}

int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    (void)flags;
    if (g_mode == MODE_AGING_WARNING_OK) {
        /* Password aging warning: PAM emits PAM_TEXT_INFO + PAM_ERROR_MSG
         * and then calls free_resp() before returning PAM_SUCCESS. */
        struct pam_message msgs[4] = {
            { PAM_TEXT_INFO, "Warning: your password will expire in 2 days." },
            { PAM_ERROR_MSG, "Please change your password soon." },
            { PAM_MSG_NOCONF, "No confirmation required" },
            { PAM_CONV_INTERRUPT, "Interrupt notice" }
        };
        struct pam_response *resp = NULL;
        int rc = run_conversation(pamh, 4, msgs, &resp);
        if (rc != PAM_SUCCESS || !resp)
            return PAM_CONV_ERR;
        int all_null = 1;
        for (int i = 0; i < 4; i++) {
            if (resp[i].resp != NULL)
                all_null = 0;
        }
        free_resp(4, resp);
        assert(all_null);
        return PAM_SUCCESS;
    }

    if (g_mode == MODE_PASSWORD_EXPIRED) {
        /* Expired password: PAM emits PAM_ERROR_MSG + PAM_TEXT_INFO, frees
         * responses with free_resp(), and returns PAM_NEW_AUTHTOK_REQD. */
        struct pam_message msgs[2] = {
            { PAM_ERROR_MSG, "Your password has expired." },
            { PAM_TEXT_INFO, "Password change required; contact administrator." }
        };
        struct pam_response *resp = NULL;
        int rc = run_conversation(pamh, 2, msgs, &resp);
        if (rc != PAM_SUCCESS || !resp)
            return PAM_CONV_ERR;
        int all_null = (resp[0].resp == NULL && resp[1].resp == NULL);
        free_resp(2, resp);
        assert(all_null);
        return PAM_NEW_AUTHTOK_REQD;
    }

    if (g_mode == MODE_ACCT_EXPIRED) {
        struct pam_message msgs[1] = {
            { PAM_ERROR_MSG, "Account has expired." }
        };
        struct pam_response *resp = NULL;
        int rc = run_conversation(pamh, 1, msgs, &resp);
        if (rc != PAM_SUCCESS || !resp)
            return PAM_CONV_ERR;
        int all_null = (resp[0].resp == NULL);
        free_resp(1, resp);
        assert(all_null);
        return PAM_ACCT_EXPIRED;
    }

    return PAM_SUCCESS;
}

int main(void)
{
    /* 1. Standard preset PAM_AUTHTOK authentication */
    g_mode = MODE_PRESET_OK;
    g_conv_calls = 0;
    assert(pamauth_authenticate("testuser", g_expected_pw) == 0);
    assert(g_conv_calls == 0);
    assert(pamauth_authenticate("testuser", "wrongpw") == PAM_AUTH_ERR);

    /* 2. Password aging warning path (Oracle 6911225 regression check) */
    g_mode = MODE_AGING_WARNING_OK;
    g_conv_calls = 0;
    assert(pamauth_authenticate("testuser", g_expected_pw) == 0);
    assert(g_conv_calls == 1);

    /* 3. Expired password & expired account paths */
    g_mode = MODE_PASSWORD_EXPIRED;
    g_conv_calls = 0;
    assert(pamauth_authenticate("testuser", g_expected_pw) == PAM_NEW_AUTHTOK_REQD);
    assert(g_conv_calls == 1);

    g_mode = MODE_ACCT_EXPIRED;
    g_conv_calls = 0;
    assert(pamauth_authenticate("testuser", g_expected_pw) == PAM_ACCT_EXPIRED);
    assert(g_conv_calls == 1);

    /* 4. Conversation-driven PAM_PROMPT_ECHO_OFF path */
    g_mode = MODE_PROMPT_ECHO_OFF;
    g_conv_calls = 0;
    assert(pamauth_authenticate("testuser", g_expected_pw) == 0);
    assert(g_conv_calls == 1);
    assert(pamauth_authenticate("testuser", "wrongpw") == PAM_AUTH_ERR);

    /* 5. Direct converse() edge cases and mid-batch cleanup */
    struct pam_response *resp = NULL;
    struct pam_message m_off = { PAM_PROMPT_ECHO_OFF, "Password:" };
    const struct pam_message *valid_one[1] = { &m_off };
    const struct pam_message *partial_null[2] = { &m_off, NULL };

    assert(converse(0, valid_one, &resp, (void *)"pw") == PAM_CONV_ERR);
    assert(converse(-1, valid_one, &resp, (void *)"pw") == PAM_CONV_ERR);
    assert(converse(1, NULL, &resp, (void *)"pw") == PAM_CONV_ERR);
    assert(converse(1, valid_one, NULL, (void *)"pw") == PAM_CONV_ERR);

    /* Mid-batch NULL message must zero and free reply[0].resp without leaking */
    assert(converse(2, partial_null, &resp, (void *)"pw") == PAM_CONV_ERR);

    /* NULL appdata_ptr with PAM_PROMPT_ECHO_OFF leaves resp[0].resp == NULL */
    resp = NULL;
    assert(converse(1, valid_one, &resp, NULL) == PAM_SUCCESS);
    assert(resp != NULL && resp[0].resp == NULL);
    free_resp(1, resp);

    /* 6. pamauth_userexists and pamauth_get_groups_for_user */
    assert(pamauth_userexists("root") == LAS_EVAL_TRUE);
    assert(pamauth_userexists("__nonexistent_heliod_user__") == LAS_EVAL_FALSE);

    char *groups = NULL;
    int ng = pamauth_get_groups_for_user("root", &groups);
    assert(ng >= 2 && groups != NULL && strlen(groups) > 0);
    free(groups);

    groups = NULL;
    assert(pamauth_get_groups_for_user("__nonexistent_heliod_user__", &groups) == -1);
    assert(groups == NULL);

    printf("PASS: pam_auth_smoke all checks passed\n");
    return 0;
}
EOF

CC_BIN="${CC:-gcc}"
"${CC_BIN}" -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DLINUX -DLinux \
    -I"${TMP_DIR}/include" \
    -I"${REPO_ROOT}/src/server/libaccess" \
    "${TMP_DIR}/test_pamauth.c" \
    -o "${TMP_DIR}/test_pamauth"

"${TMP_DIR}/test_pamauth"

# If a built work-root is provided, verify that FEAT_PAM is enabled in the
# built libns-httpd40.so and run a live webservd PAM <auth-db> integration test.
if [ $# -ge 1 ] && [ -d "$1" ]; then
    WORK_ROOT=$(cd "$1" && pwd)
    LIBHTTPD="${WORK_ROOT}/lib/libns-httpd40.so"
    if [ ! -f "${LIBHTTPD}" ]; then
        echo "FAIL: ${LIBHTTPD} not found"
        exit 1
    fi

    NM_OUT=$(nm -D "${LIBHTTPD}")
    for sym in pam_authenticate_user pam_user_ismember_get pam_userexists_get \
               pam_parse_url pamauth_authenticate pamauth_get_groups_for_user \
               pamauth_userexists; do
        if ! printf '%s\n' "${NM_OUT}" | grep -w "${sym}" >/dev/null; then
            echo "FAIL: ${sym} not exported in ${LIBHTTPD} (FEAT_PAM not enabled)"
            exit 1
        fi
    done

    READELF_OUT=$(readelf -d "${LIBHTTPD}")
    if ! printf '%s\n' "${READELF_OUT}" | grep "libpam.so" >/dev/null; then
        echo "FAIL: libpam.so not linked as NEEDED in ${LIBHTTPD}"
        exit 1
    fi
    echo "PASS: libns-httpd40.so exports PAM auth-db symbols and links libpam.so"

    INSTANCE_DIR="${WORK_ROOT}/https-test"
    CONFIG_DIR="${INSTANCE_DIR}/config"
    DOCS_DIR="${INSTANCE_DIR}/docs"
    if [ -d "${INSTANCE_DIR}" ]; then
        BACKUP_DIR="${TMP_DIR}/config-backup"
        cp -a "${CONFIG_DIR}" "${BACKUP_DIR}"
        restore_instance() {
            "${INSTANCE_DIR}/bin/stopserv" >/dev/null 2>&1 || true
            pkill -9 -f "${INSTANCE_DIR}/config" >/dev/null 2>&1 || true
            rm -rf "${CONFIG_DIR}"
            cp -a "${BACKUP_DIR}" "${CONFIG_DIR}"
            rm -rf "${DOCS_DIR}/pam-protected"
            rm -rf "${TMP_DIR}"
        }
        trap restore_instance EXIT

        "${INSTANCE_DIR}/bin/stopserv" >/dev/null 2>&1 || true
        pkill -9 -f "${INSTANCE_DIR}/config" >/dev/null 2>&1 || true

        cat > "${TMP_DIR}/libpam_preload.c" <<'PRELOAD_EOF'
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>

struct pam_handle {
    char *user;
    char *authtok;
    struct pam_conv conv;
};

static void preload_free_resp(int num_msg, struct pam_response *resp)
{
    if (resp) {
        for (int i = 0; i < num_msg; i++) {
            if (resp[i].resp) {
                memset(resp[i].resp, 0, strlen(resp[i].resp));
                free(resp[i].resp);
            }
        }
        free(resp);
    }
}

int pam_start(const char *service_name, const char *user,
              const struct pam_conv *pam_conversation,
              pam_handle_t **pamh)
{
    (void)service_name;
    if (!user || !pam_conversation || !pamh)
        return PAM_SYSTEM_ERR;
    pam_handle_t *h = (pam_handle_t *)calloc(1, sizeof(pam_handle_t));
    if (!h)
        return PAM_BUF_ERR;
    h->user = strdup(user);
    h->conv = *pam_conversation;
    *pamh = h;
    return PAM_SUCCESS;
}

int pam_end(pam_handle_t *pamh, int pam_status)
{
    (void)pam_status;
    if (!pamh)
        return PAM_SUCCESS;
    free(pamh->user);
    if (pamh->authtok) {
        memset(pamh->authtok, 0, strlen(pamh->authtok));
        free(pamh->authtok);
    }
    free(pamh);
    return PAM_SUCCESS;
}

int pam_set_item(pam_handle_t *pamh, int item_type, const void *item)
{
    if (!pamh)
        return PAM_SYSTEM_ERR;
    if (item_type == PAM_AUTHTOK) {
        free(pamh->authtok);
        pamh->authtok = item ? strdup((const char *)item) : NULL;
        return PAM_SUCCESS;
    }
    return PAM_SUCCESS;
}

int pam_authenticate(pam_handle_t *pamh, int flags)
{
    (void)flags;
    if (!pamh || !pamh->user || !pamh->authtok)
        return PAM_AUTH_ERR;
    if (strcmp(pamh->user, "root") == 0 &&
        strcmp(pamh->authtok, "AgingPass123") == 0)
        return PAM_SUCCESS;
    if (strcmp(pamh->user, "nobody") == 0 &&
        strcmp(pamh->authtok, "ExpiredPass123") == 0)
        return PAM_SUCCESS;
    if (strcmp(pamh->user, "daemon") == 0) {
        struct pam_message msg = { PAM_PROMPT_ECHO_OFF, "Password: " };
        const struct pam_message *ptrs[1] = { &msg };
        struct pam_response *resp = NULL;
        int rc = pamh->conv.conv(1, ptrs, &resp, pamh->conv.appdata_ptr);
        if (rc != PAM_SUCCESS || !resp)
            return PAM_CONV_ERR;
        int ok = (resp[0].resp && strcmp(resp[0].resp, "PromptPass123") == 0);
        preload_free_resp(1, resp);
        return ok ? PAM_SUCCESS : PAM_AUTH_ERR;
    }
    return PAM_AUTH_ERR;
}

int pam_acct_mgmt(pam_handle_t *pamh, int flags)
{
    (void)flags;
    /* Emit password-aging warning / error messages via conversation and free
     * via preload_free_resp() on every login. */
    struct pam_message msgs[2] = {
        { PAM_TEXT_INFO, "Warning: your password will expire in 3 days." },
        { PAM_ERROR_MSG, "Password policy notice." }
    };
    const struct pam_message *ptrs[2] = { &msgs[0], &msgs[1] };
    struct pam_response *resp = NULL;
    int rc = pamh->conv.conv(2, ptrs, &resp, pamh->conv.appdata_ptr);
    if (rc != PAM_SUCCESS || !resp)
        return PAM_CONV_ERR;
    preload_free_resp(2, resp);

    if (strcmp(pamh->user, "nobody") == 0)
        return PAM_NEW_AUTHTOK_REQD;
    return PAM_SUCCESS;
}
PRELOAD_EOF

        "${CC_BIN}" -std=gnu11 -O2 -fPIC -shared \
            -DLINUX -DLinux \
            -I"${TMP_DIR}/include" \
            "${TMP_DIR}/libpam_preload.c" \
            -o "${TMP_DIR}/libpam_preload.so"

        mkdir -p "${DOCS_DIR}/pam-protected"
        echo "PAM_AUTH_OK" > "${DOCS_DIR}/pam-protected/secret.txt"

        PORT=18180
        python3 - <<PY
import xml.etree.ElementTree as ET
path = "${CONFIG_DIR}/server.xml"
tree = ET.parse(path)
root = tree.getroot()

tp = root.find("temp-path")
if tp is None:
    tp = ET.SubElement(root, "temp-path")
tp.text = "${TMP_DIR}/ws-tmp"

plat = root.find("platform")
if plat is None:
    plat = ET.SubElement(root, "platform")
plat.text = "64"

for listener in list(root.findall("http-listener")):
    root.remove(listener)
ls = ET.SubElement(root, "http-listener")
ET.SubElement(ls, "name").text = "http-listener-1"
ET.SubElement(ls, "port").text = "${PORT}"
ET.SubElement(ls, "server-name").text = "localhost"
ET.SubElement(ls, "default-virtual-server-name").text = "test"

authdb = ET.SubElement(root, "auth-db")
ET.SubElement(authdb, "name").text = "pamdb"
ET.SubElement(authdb, "url").text = "pam"

vs = root.find("virtual-server")
for el in list(vs.findall("http-listener-name")):
    vs.remove(el)
ET.SubElement(vs, "http-listener-name").text = "http-listener-1"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, encoding="utf-8", xml_declaration=False)
PY

        cat > "${CONFIG_DIR}/default.acl" <<ACL_EOF
version 3.0;
acl "default";
authenticate (user, group) {
  database = "default";
  method = "basic";
};
allow (all) user = "anyone";

acl "uri=/pam-protected/*";
authenticate (user, group) {
  prompt = "PAM Area";
  database = "pamdb";
  method = "basic";
};
deny (all) user = "anyone";
allow (all) user = "root" and group = "root";
allow (all) user = "daemon";
ACL_EOF

        : > "${INSTANCE_DIR}/logs/errors"
        LD_PRELOAD="${TMP_DIR}/libpam_preload.so" "${INSTANCE_DIR}/bin/startserv"

        for _ in $(seq 1 30); do
            if curl -s -o /dev/null "http://127.0.0.1:${PORT}/" 2>/dev/null; then
                break
            fi
            sleep 0.2
        done

        # 1. Unauthenticated request -> 401
        code=$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:${PORT}/pam-protected/secret.txt")
        [ "${code}" = "401" ] || { echo "FAIL: unauthenticated expected 401, got ${code}"; exit 1; }

        # 2. Wrong password -> 401
        code=$(curl -s -o /dev/null -w '%{http_code}' -u "root:WrongPass" "http://127.0.0.1:${PORT}/pam-protected/secret.txt")
        [ "${code}" = "401" ] || { echo "FAIL: wrong password expected 401, got ${code}"; exit 1; }

        # 3. Expired password (triggers converse() + free_resp() + PAM_NEW_AUTHTOK_REQD) -> 401, no crash
        code=$(curl -s -o /dev/null -w '%{http_code}' -u "nobody:ExpiredPass123" "http://127.0.0.1:${PORT}/pam-protected/secret.txt")
        [ "${code}" = "401" ] || { echo "FAIL: expired password expected 401, got ${code}"; exit 1; }

        # 4. Valid user + password-aging warning + Linux group membership check -> 200
        body=$(curl -s -f -u "root:AgingPass123" "http://127.0.0.1:${PORT}/pam-protected/secret.txt")
        [ "${body}" = "PAM_AUTH_OK" ] || { echo "FAIL: valid PAM auth body mismatch: ${body}"; exit 1; }

        # 5. Valid user via PAM_PROMPT_ECHO_OFF conversation prompt -> 200
        body=$(curl -s -f -u "daemon:PromptPass123" "http://127.0.0.1:${PORT}/pam-protected/secret.txt")
        [ "${body}" = "PAM_AUTH_OK" ] || { echo "FAIL: daemon PAM auth body mismatch: ${body}"; exit 1; }

        if grep -E "SIGSEGV|SEGV|segfault|CORE3260" "${INSTANCE_DIR}/logs/errors"; then
            echo "FAIL: crash detected in error log"
            exit 1
        fi
        echo "PASS: live webservd PAM auth-db integration checks passed"
    fi
fi

