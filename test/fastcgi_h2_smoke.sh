#!/bin/bash
#
# FastCGI-over-HTTP/2 test.
#
# The FastCGI responder (plugins/fastcgi) relays between two endpoints with a
# PR_Poll() loop: the FastCGI application socket (a real OS socket) and the
# client socket.  Over HTTP/1.1 the client socket is a real socket too, so the
# loop blocks in poll() the way it was designed to.  Over HTTP/2 the client is a
# synthetic per-stream NSPR descriptor with no underlying OS file descriptor
# (see httpdaemon/H2StreamSocket.cpp), and PR_Poll() cannot poll it -- it flags
# it PR_POLL_NVAL.  Three distinct defects lived on that path and this test
# guards all three:
#
#   crash      the synthetic H2 request never ran DaemonSession::prepareConnection(),
#              so serverIP was NULL and the responder's SERVER_ADDR env var
#              dereferenced it -- an immediate SIGSEGV in responder-fastcgi on the
#              very first request.
#   spin       the un-pollable client made every PR_Poll() return instantly, so
#              the responder never blocked for the application's response and
#              never timed out: a worker thread pegged at 100% CPU forever, one
#              per request, exactly as seen in production.
#   leak       when the client goes away mid-response the worker must unwind, not
#              sit buffering the application's output into memory it can never
#              deliver.
#
# The application is php-fpm (a real, ubiquitous FastCGI server), reached as a
# remote server over 127.0.0.1.  If no php-fpm binary is found the test SKIPs.
#
# Reuses the https-test instance created by `make install`.  The original config
# is saved before the test and restored afterwards.
#
# Usage:
#   test/fastcgi_h2_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT=$(cd "$1" 2>/dev/null && pwd) || { echo "Work root '$1' not found"; exit 1; }
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
DOCROOT="${INSTANCE_DIR}/docs"
FCGI_SO="${WORK_ROOT}/plugins/fastcgi/libfastcgi.so"
HTTP_PORT=18180
HTTPS_PORT=18443
FCGI_PORT=19000
MAX_THREADS=8
TEMP_DIR=""
FPM_DIR=""
NO_PASS=""
SERVER_PID=""
FPM_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi
if [ ! -f "${FCGI_SO}" ]; then
    echo "[fastcgi-h2] SKIP: FastCGI plugin not built (${FCGI_SO} missing)."
    exit 0
fi

# Find a php-fpm binary.
PHP_FPM="${PHP_FPM:-}"
if [ -z "${PHP_FPM}" ]; then
    for c in php-fpm php-fpm8.5 php-fpm8.4 php-fpm8.3 php-fpm8.2 php-fpm8.1 php-fpm8.0; do
        p=$(command -v "$c" 2>/dev/null) && { PHP_FPM="$p"; break; }
        [ -x "/usr/sbin/$c" ] && { PHP_FPM="/usr/sbin/$c"; break; }
    done
fi
if [ -z "${PHP_FPM}" ]; then
    echo "[fastcgi-h2] SKIP: no php-fpm binary found (set PHP_FPM to override)."
    exit 0
fi
echo "[fastcgi-h2] using ${PHP_FPM}"

find_worker() {
    # The request-serving webservd child: our instance, more than a few threads.
    for p in $(pgrep -x webservd 2>/dev/null); do
        local cmd; cmd=$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null)
        case "$cmd" in
            *"${INSTANCE_DIR}"*)
                [ "$(ls "/proc/$p/task" 2>/dev/null | wc -l)" -gt 5 ] && { echo "$p"; return; };;
        esac
    done
}

# Highest per-thread CPU% among the worker's threads.
max_thread_cpu() {
    local wk="$1"
    [ -z "$wk" ] && { echo 0; return; }
    top -H -b -n1 -p "$wk" 2>/dev/null | awk '
        NR>7 && $NF=="webservd" { if ($9+0 > m) m=$9+0 } END { printf "%d", m+0 }'
}

cleanup() {
    if [ -n "${SERVER_PID:-}" ] || pgrep -f "webservd.*${INSTANCE_DIR}" >/dev/null 2>&1; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
    fi
    [ -n "${FPM_PID:-}" ] && kill -9 "${FPM_PID}" 2>/dev/null || true
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"; cp -r "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"; rm -rf "${CONFIG_DIR}.pristine"
    fi
    rm -f "${DOCROOT}/fcgi-hello.php" "${DOCROOT}/fcgi-slow.php" 2>/dev/null || true
    [ -n "${NO_PASS:-}" ] && rm -f "${NO_PASS}" 2>/dev/null || true
    [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ] && rm -rf "${TEMP_DIR}"
    [ -n "${FPM_DIR:-}" ] && [ -d "${FPM_DIR}" ] && rm -rf "${FPM_DIR}"
}
trap cleanup EXIT

cd "${WORK_ROOT}"
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-fcgih2-XXXXXX); chmod 700 "${TEMP_DIR}"

# --- PHP scripts in the docroot -------------------------------------------
# Instant response: exercises the crash/hang path and reports SERVER_ADDR so a
# NULL server IP would be visible.
cat > "${DOCROOT}/fcgi-hello.php" <<'PHP'
<?php
header("Content-Type: text/plain");
echo "FCGI-H2-OK addr=" . ($_SERVER["SERVER_ADDR"] ?? "UNSET") . "\n";
PHP
# Slow, large stream: for the mid-response client-disconnect check.
cat > "${DOCROOT}/fcgi-slow.php" <<'PHP'
<?php
header("Content-Type: application/octet-stream");
@ob_end_flush(); @ob_implicit_flush(true);
$chunk = str_repeat("X", 16384);
for ($i = 0; $i < 6000; $i++) { echo $chunk; flush(); usleep(20000); }
PHP
chmod 644 "${DOCROOT}/fcgi-hello.php" "${DOCROOT}/fcgi-slow.php"

# --- php-fpm pool ----------------------------------------------------------
FPM_DIR=$(mktemp -d /tmp/${INSTANCE}-fpm-XXXXXX); chmod 755 "${FPM_DIR}"
cat > "${FPM_DIR}/php-fpm.conf" <<EOF
[global]
pid = ${FPM_DIR}/php-fpm.pid
error_log = ${FPM_DIR}/php-fpm.log
daemonize = no
[www]
listen = 127.0.0.1:${FCGI_PORT}
pm = static
pm.max_children = ${MAX_THREADS}
EOF
"${PHP_FPM}" -y "${FPM_DIR}/php-fpm.conf" -t >/dev/null 2>&1 || {
    echo "[fastcgi-h2] SKIP: php-fpm config test failed."; exit 0; }
setsid "${PHP_FPM}" -y "${FPM_DIR}/php-fpm.conf" -F >"${FPM_DIR}/out" 2>&1 < /dev/null &
FPM_PID=$!
for _ in $(seq 1 20); do ss -tln 2>/dev/null | grep -q ":${FCGI_PORT} " && break; sleep 0.3; done
if ! ss -tln 2>/dev/null | grep -q ":${FCGI_PORT} "; then
    echo "[fastcgi-h2] SKIP: php-fpm did not come up on ${FCGI_PORT}."; exit 0
fi

# --- TLS cert + load the FastCGI plugin ------------------------------------
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp); printf '\n' > "${NO_PASS}"
rm -f cert8.db key3.db secmod.db cert9.db key4.db pkcs11.txt 2>/dev/null || true
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/fcgih2-key.pem -out /tmp/fcgih2-cert.pem \
    -days 90 -nodes -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost" 2>/dev/null
openssl pkcs12 -export -out /tmp/fcgih2.p12 -inkey /tmp/fcgih2-key.pem -in /tmp/fcgih2-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/fcgih2.p12 -d dbm:"." -w "${NO_PASS}"
grep -q "^NSSDir" magnus.conf || echo "NSSDir ${PWD}" >> magnus.conf
grep -q libfastcgi magnus.conf || echo "Init fn=\"load-modules\" shlib=\"${FCGI_SO}\"" >> magnus.conf
rm -f /tmp/fcgih2-key.pem /tmp/fcgih2-cert.pem /tmp/fcgih2.p12
cd "${WORK_ROOT}"

# --- server.xml: event listeners + small pool ------------------------------
python3 - <<PY
import xml.etree.ElementTree as ET, os
path = os.path.join("${CONFIG_DIR}", "server.xml")
t = ET.parse(path); r = t.getroot()
def setpath(tag, val):
    e = r.find(tag)
    if e is None: e = ET.SubElement(r, tag)
    e.text = val
setpath("temp-path", "${TEMP_DIR}")
setpath("platform", "64")
for e in r.findall("thread-pool"): r.remove(e)
pool = ET.SubElement(r, "thread-pool")
ET.SubElement(pool, "min-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "max-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "queue-size").text = "512"
vs = r.find("virtual-server")
vsl = set(e.text for e in vs.findall("http-listener-name"))
# Modify the existing http-listener in place; drop any old https-listener-1.
for l in r.findall("http-listener"):
    name = l.findtext("name")
    if name == "https-listener-1":
        r.remove(l); continue
    if name == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${HTTP_PORT}"
    for c in list(l):
        if c.tag == "io-model": l.remove(c)
    ET.SubElement(l, "io-model").text = "event"
# Append the TLS/event listener (element order after virtual-server is accepted).
h = ET.SubElement(r, "http-listener")
ET.SubElement(h, "name").text = "https-listener-1"
ET.SubElement(h, "port").text = "${HTTPS_PORT}"
ET.SubElement(h, "server-name").text = "localhost"
ET.SubElement(h, "default-virtual-server-name").text = "test"
ET.SubElement(h, "io-model").text = "event"
s = ET.SubElement(h, "ssl"); ET.SubElement(s, "enabled").text = "true"
ET.SubElement(s, "server-cert-nickname").text = "Server-Cert"
if "https-listener-1" not in vsl:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"
with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY

# --- obj.conf: route the .php scripts to the FastCGI responder -------------
python3 - <<PY
import os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
s = open(path).read()
anchor = 'PathCheck fn="uri-clean"'
if anchor not in s: raise SystemExit("obj.conf: no uri-clean anchor")
nt = ('NameTrans fn="assign-name" from="/fcgi-hello.php" name="fcgi"\n'
      'NameTrans fn="assign-name" from="/fcgi-slow.php" name="fcgi"\n')
s = s.replace(anchor, nt + anchor, 1)
s += ('\n<Object name="fcgi">\n'
      'ObjectType fn="force-type" type="magnus-internal/fastcgi"\n'
      'Service fn="responder-fastcgi" bind-path="127.0.0.1:${FCGI_PORT}"\n'
      '</Object>\n')
open(path, "w").write(s)
PY

"${INSTANCE_DIR}/bin/startserv" > "${TEMP_DIR}/startserv.log" 2>&1 &
SERVER_PID=$!
for _ in $(seq 1 20); do ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} " && break; sleep 0.5; done
if ! ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
    echo "[fastcgi-h2] FAIL: server did not start"; cat "${TEMP_DIR}/startserv.log"; exit 1
fi

ERR_LOG="${INSTANCE_DIR}/logs/errors"
# Only inspect what this run adds to the (cumulative) error log.
ERRLOG_START=0
[ -f "${ERR_LOG}" ] && ERRLOG_START=$(wc -l < "${ERR_LOG}")
URL_HELLO="https://localhost:${HTTPS_PORT}/fcgi-hello.php"
URL_SLOW="https://localhost:${HTTPS_PORT}/fcgi-slow.php"
fail() { echo "[fastcgi-h2] FAIL: $*"; exit 1; }

# --- Test 1: an HTTP/2 FastCGI request returns (no crash, no hang) ---------
BODY=$(curl --http2 -sk --max-time 10 "${URL_HELLO}" \
       -w $'\n[hv=%{http_version} code=%{response_code}]' 2>&1) || true
echo "[fastcgi-h2] hello: ${BODY##*$'\n'}"
case "${BODY}" in
    *"FCGI-H2-OK"*"[hv=2 code=200]"*) echo "[fastcgi-h2] H2 FastCGI request served: OK";;
    *) fail "H2 FastCGI request did not return a 200 (got: ${BODY})";;
esac
case "${BODY}" in *"addr=UNSET"*|*"addr= "*) fail "SERVER_ADDR was not populated (NULL serverIP regression)";; esac

# --- Test 2: no worker thread is left spinning -----------------------------
sleep 2
WK=$(find_worker)
CPU=$(max_thread_cpu "$WK")
echo "[fastcgi-h2] worker=${WK} peak per-thread CPU after request: ${CPU}%"
[ "${CPU}" -lt 50 ] || fail "a worker thread is spinning (${CPU}%) after a completed FastCGI/H2 request"
echo "[fastcgi-h2] no busy-spin after a completed request: OK"

# --- Test 3: client disconnect mid-response leaves no spinner, no leak ------
RSS0=$(awk '/VmRSS/{print $2}' "/proc/${WK}/status" 2>/dev/null || echo 0)
curl --http2 -sk "${URL_SLOW}" -o /dev/null 2>/dev/null & CP=$!
sleep 1.5
kill -9 "${CP}" 2>/dev/null || true
sleep 4
WK=$(find_worker)
CPU=$(max_thread_cpu "$WK")
RSS1=$(awk '/VmRSS/{print $2}' "/proc/${WK}/status" 2>/dev/null || echo 0)
echo "[fastcgi-h2] after mid-stream disconnect: peak CPU=${CPU}% rss ${RSS0}kB -> ${RSS1}kB"
[ "${CPU}" -lt 50 ] || fail "a worker thread is spinning (${CPU}%) after the client disconnected mid-response"
# Bounded buffering: the relay must unwind, not accumulate the whole backend
# stream.  Allow a generous slack for one ring buffer's worth plus noise.
if [ "${RSS0}" -gt 0 ] && [ "${RSS1}" -gt $((RSS0 + 32768)) ]; then
    fail "RSS grew ${RSS0}->${RSS1} kB after disconnect (backend output is being buffered, not unwound)"
fi
echo "[fastcgi-h2] worker unwinds after client disconnect, no runaway buffering: OK"

# --- Test 4: server still healthy, no crashes ------------------------------
BODY=$(curl --http2 -sk --max-time 10 "${URL_HELLO}" -w '%{response_code}' 2>&1) || true
case "${BODY}" in *"200"*) echo "[fastcgi-h2] server still serving after disconnect: OK";; *) fail "server not healthy after disconnect (got: ${BODY})";; esac
NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERR_LOG}" 2>/dev/null |
             grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
[ -z "${NEW_ERRORS}" ] || fail "crash/assertion in error log: ${NEW_ERRORS}"
echo "[fastcgi-h2] no crashes or assertions in server error log"

echo "FastCGI-over-HTTP/2 test passed."
