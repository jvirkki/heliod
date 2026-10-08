#!/bin/bash
#
# Classic-model DaemonSession pool sizing test.
#
# PostThreads() used to size the pool as Connection::countActive + 1, and
# countActive counts every live Connection -- including idle keep-alives
# parked in PollManager.  With no shrink path, the pool grew to the lifetime
# high-water mark of open sockets (observed 61/64 behind a reverse proxy that
# holds idle upstream keep-alives).  The fix sizes on demand instead: one
# thread per busy session, one per queued connection, plus one spare.
#
# Measurement is perfdump's "Total Sessions Created N/M" line (N = non-empty
# StatsThreadSlots = DaemonSession count) and "KeepAliveCount X/Y".
#
# Phase 1 opens N_IDLE HTTP/1.1 keep-alive connections sequentially and leaves
# the sockets open: the keep-alive count must reach N_IDLE while the session
# count stays at min-threads (the old code would have created one thread per
# connection).  Phase 2 fires concurrent slow requests with the idle sockets
# still open and asserts the pool still grows under real demand.
#
# Plain HTTP on 18180 with the classic io-model; reuses https-test;
# saves/restores config.
#
# Usage: test/daemon_pool_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
PORT=18180
BASE="http://localhost:${PORT}"
MIN_THREADS=2
MAX_THREADS=32
N_IDLE=16             # idle keep-alive connections parked in PollManager
CONCURRENCY=16        # concurrent slow requests; between MIN and MAX
SLOW_SECS=5           # how long each request occupies its worker
TEMP_DIR=""
CGI_DIR=""
SERVER_PID=""
FAILURES=()

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
    fi
    # Kill only this instance's webservd (never production); match by exact
    # process name + verified cmdline, never `pkill -f <path>` (self-matches).
    for p in $(pgrep -x webservd 2>/dev/null; pgrep -x webservd-wdog 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)
        case "$cmd" in *"Linux_debian_f_x86_64_32_OPT/https-test"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"; mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
    [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ] && rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

cd "${WORK_ROOT}"
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
for p in $(pgrep -x webservd 2>/dev/null; pgrep -x webservd-wdog 2>/dev/null); do
    cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)
    case "$cmd" in *"Linux_debian_f_x86_64_32_OPT/https-test"*) kill -9 "$p" 2>/dev/null || true;; esac
done
sleep 1

[ -d "${CONFIG_DIR}.pristine" ] || cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-dp-XXXXXX)
chmod 700 "${TEMP_DIR}"
CGI_DIR="${TEMP_DIR}/cgi-bin"
mkdir -p "${CGI_DIR}"

# A CGI that occupies its worker for a while, so concurrent requests force the
# pool to grow.
cat > "${CGI_DIR}/slow.py" <<PY
#!/usr/bin/env python3
import sys, time, os
secs = int((os.environ.get("QUERY_STRING") or "${SLOW_SECS}") or ${SLOW_SECS})
time.sleep(secs)
sys.stdout.write("Status: 200\r\nContent-Type: text/plain\r\n\r\nslept %d\n" % secs)
PY
chmod +x "${CGI_DIR}/slow.py"

# Configure: classic I/O on 18180 (keep-alive stays on; the keep-alive element
# is omitted, which implicitly enables it), thread pool min/max pinned.
python3 - <<PY
import xml.etree.ElementTree as ET, os
path = os.path.join("${CONFIG_DIR}", "server.xml")
t = ET.parse(path); r = t.getroot()

tp = r.find("temp-path")
if tp is None: tp = ET.SubElement(r, "temp-path")
tp.text = "${TEMP_DIR}"
plat = r.find("platform")
if plat is None: plat = ET.SubElement(r, "platform")
plat.text = "64"

# Pin the thread pool.
for existing in r.findall("thread-pool"):
    r.remove(existing)
pool = ET.SubElement(r, "thread-pool")
ET.SubElement(pool, "min-threads").text = "${MIN_THREADS}"
ET.SubElement(pool, "max-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "queue-size").text = "512"

for l in r.findall("http-listener"):
    if l.findtext("name") == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${PORT}"
        for c in list(l):
            if c.tag == "io-model": l.remove(c)
        ET.SubElement(l, "io-model").text = "classic"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY

# /perfdump is the only way to read the realtime stats slots.  The mapping goes
# in ahead of every NameTrans already there, because whichever pfx2dir for a
# prefix comes first wins.  Same for the slow-CGI prefix.
python3 - <<PY
import os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
lines = open(path).read().splitlines(True)
mappings = ['NameTrans fn="assign-name" from="/perfdump" name="dp-perf"\n',
            'NameTrans fn="pfx2dir" from="/cgi-bin" dir="${CGI_DIR}" '
            'name="cgi"\n']
for i, line in enumerate(lines):
    if line.startswith('NameTrans'):
        break
else:
    raise SystemExit('obj.conf: no NameTrans line to insert before')
text = ''.join(lines[:i] + mappings + lines[i:])
text += '\n<Object name="dp-perf">\nService fn="service-dump"\n</Object>\n'
open(path, 'w').write(text)
PY

ERRLOG="${INSTANCE_DIR}/logs/errors"
: > "${ERRLOG}" 2>/dev/null || true

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-dp.log 2>&1 &
SERVER_PID=$!
waited=0
while [ "${waited}" -lt 20 ]; do
    ss -tln 2>/dev/null | grep -q ":${PORT} " && break
    sleep 1; waited=$((waited+1))
done
sleep 1   # let startup settle

rc=0
python3 - <<PY || rc=$?
import re
import socket
import subprocess
import sys
import time

PORT = ${PORT}
BASE = "${BASE}"
N_IDLE = ${N_IDLE}
MIN_THREADS = ${MIN_THREADS}
MAX_THREADS = ${MAX_THREADS}
SLOW_SECS = ${SLOW_SECS}
TIMEOUT = 20

failures = []


def check(name, cond, detail=''):
    if cond:
        print('[dp] %s: OK%s' % (name, (' (%s)' % detail) if detail else ''))
    else:
        print('[dp] %s: FAIL %s' % (name, detail))
        failures.append(name)


def get(path, keep_alive=False):
    """One HTTP/1.1 request; returns (socket, body).  The socket stays open
    when keep_alive is set so the caller can park it; otherwise it is closed
    before returning."""
    sock = socket.socket()
    sock.settimeout(TIMEOUT)
    sock.connect(('127.0.0.1', PORT))
    sock.sendall(('GET %s HTTP/1.1\r\nHost: localhost\r\n'
                  'Connection: %s\r\n\r\n'
                  % (path, 'keep-alive' if keep_alive else 'close')).encode())
    data = b''
    while b'\r\n\r\n' not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
    head, _, rest = data.partition(b'\r\n\r\n')
    headers = head.decode('latin1')
    m = re.search(r'Content-Length:\s*(\d+)', headers, re.I)
    if m is not None:
        need = int(m.group(1))
        body = rest
        while len(body) < need:
            chunk = sock.recv(need - len(body))
            if not chunk:
                break
            body += chunk
    else:
        # No length: read until EOF (Connection: close) or one buffer.
        body = rest
        if not keep_alive:
            while True:
                try:
                    chunk = sock.recv(4096)
                except socket.timeout:
                    break
                if not chunk:
                    break
                body += chunk
    if not keep_alive:
        sock.close()
        sock = None
    return sock, body.decode('latin1')


def perfdump():
    """Fetch /perfdump over a fresh connection; return (sessions_n,
    sessions_limit, keepalive_count).  sessions_limit is perfdump's own
    denominator (max-threads plus the keep-alive poll threads, whose stats
    slots are included in sessions_n as well)."""
    _, body = get('/perfdump')
    ms = re.search(r'Total Sessions Created\s+(\d+)/(\d+)', body)
    mk = re.search(r'KeepAliveCount\s+(\d+)/(\d+)', body)
    if ms is None or mk is None:
        check('perfdump is reachable', False,
              'missing counters in response: %r' % body[:400])
        return None, None, None
    return int(ms.group(1)), int(ms.group(2)), int(mk.group(1))


# ------------------------------------------------------------- phase 1: idle
# Open N_IDLE keep-alive connections sequentially so the connection queue
# never backs up; leave every socket open.
idle_socks = []
try:
    for _ in range(N_IDLE):
        sock, _ = get('/', keep_alive=True)
        idle_socks.append(sock)

    time.sleep(6)   # several Clock() ticks

    n_idle, _, ka_idle = perfdump()
    if n_idle is not None:
        print('[dp] idle: Total Sessions Created=%d  KeepAliveCount=%d'
              % (n_idle, ka_idle))
        check('keep-alives parked in keep-alive subsystem', ka_idle >= N_IDLE,
              '(KeepAliveCount=%d, expected >= %d -- if this fails the test '
              'is not exercising the bug)' % (ka_idle, N_IDLE))
        check('idle keep-alives do not grow the pool',
              n_idle <= MIN_THREADS + 3,
              '(sessions=%d, expected <= %d; transient queueing while opening '
              'the connections can legitimately add a couple of threads over '
              'min; old code gives >= %d)'
              % (n_idle, MIN_THREADS + 3, N_IDLE + 1))
        check('no thread per idle connection', n_idle < N_IDLE,
              '(sessions=%d, idle conns=%d)' % (n_idle, N_IDLE))

    # -------------------------------------------------------- phase 2: demand
    procs = [subprocess.Popen(['curl', '-s', '-o', '/dev/null',
                               '%s/cgi-bin/slow.py?%d' % (BASE, SLOW_SECS)])
             for _ in range(${CONCURRENCY})]
    time.sleep(SLOW_SECS - 1)   # sample while they are all still in flight
    n_load, limit, _ = perfdump()
    for p in procs:
        p.wait()
    if n_load is not None:
        print('[dp] load: Total Sessions Created=%d/%d (%d concurrent slow requests)'
              % (n_load, limit, ${CONCURRENCY}))
        check('pool grows under real demand', n_load >= MIN_THREADS + 5,
              '(sessions=%d, expected >= %d)' % (n_load, MIN_THREADS + 5))
        check('growth bounded by max-threads', n_load <= limit,
              '(sessions=%d <= %d)' % (n_load, limit))
finally:
    for sock in idle_socks:
        sock.close()

print()
if failures:
    print('[dp] FAILURES: %s' % ', '.join(failures))
    sys.exit(1)
PY

check() { if [ "$2" -eq 0 ]; then echo "[dp] $1: OK $3"; else echo "[dp] $1: FAIL $3"; FAILURES+=("$1"); fi; }

if [ "${rc}" -ne 0 ]; then
    FAILURES+=("perfdump assertions")
fi

# error log clean of crashes/assertions
if tail -200 "${ERRLOG}" 2>/dev/null | grep -qiE "SIGSEGV|catastrophe|assertion"; then
    check "error log clean" 1 "(see ${ERRLOG})"
else
    check "error log clean" 0 ""
fi

if [ ${#FAILURES[@]} -ne 0 ]; then
    echo "daemon pool test FAILED: ${FAILURES[*]}"
    exit 1
fi
echo "daemon pool test passed."
