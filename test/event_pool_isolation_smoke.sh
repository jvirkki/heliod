#!/bin/bash
#
# Classic/event pool isolation test
#
# When NO listener uses io-model=classic, the classic DaemonSession thread
# pool must start empty and the keep-alive poll (KAPollThread) threads must
# not be created at all: the server logs "No http-listener uses
# io-model=classic" and "Started 0 request processing threads", and the
# worker process carries measurably fewer threads (min-threads DaemonSessions
# plus the KAPollThreads, which scale with CPU count, are absent).
#
# It also proves the pools are independent: traffic on an event listener must
# never grow the classic pool, even with a classic listener configured --
# the classic DaemonSession::processConnection() path used to increment the
# shared nActiveSessions_ counter for event traffic, so Clock()->PostThreads()
# grew a classic pool that event connections never use.
#
# The classic pool is never hard-disabled: the acceptor falls back to the
# classic ConnectionQueue if the event dispatcher rejects a connection, so a
# second phase with a real classic listener verifies the pool still grows
# under genuine classic demand.
#
# Probes are the LOG_VERBOSE growth lines ("WorkerPool: grew to N of M" and
# "DaemonSession pool grew to N of M"), so the instance runs at log-level
# finest.  Two configurations are run one after the other; config is saved
# and restored around each.
#
# Usage: test/event_pool_isolation_smoke.sh <heliod-work-root>

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
PORT2=18181
BASE="http://localhost:${PORT}"
BASE2="http://localhost:${PORT2}"
MIN_THREADS=2
MAX_THREADS=32
CONCURRENCY=16          # concurrent slow requests; between MIN and MAX
SLOW_SECS=5             # how long each request occupies its worker
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
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-epi-XXXXXX)
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

ERRLOG="${INSTANCE_DIR}/logs/errors"

# configure_instance <with-classic-listener: 0|1>
configure_instance() {
    python3 - "$1" <<PY
import xml.etree.ElementTree as ET, os, sys
want_classic = sys.argv[1] == "1"
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

# Log at finest so LOG_VERBOSE growth lines appear.
log = r.find("log")
if log is None: log = ET.SubElement(r, "log")
ll = log.find("log-level")
if ll is None: ll = ET.SubElement(log, "log-level")
ll.text = "finest"

import copy
listener2 = None
for l in r.findall("http-listener"):
    if l.findtext("name") == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${PORT}"
        for c in list(l):
            if c.tag == "io-model": l.remove(c)
        ET.SubElement(l, "io-model").text = "event"
        listener1 = l
    elif l.findtext("name") == "http-listener-2":
        r.remove(l)

if want_classic:
    listener2 = copy.deepcopy(listener1)
    listener2.find("name").text = "http-listener-2"
    listener2.find("port").text = "${PORT2}"
    for c in list(listener2):
        if c.tag == "io-model": listener2.remove(c)
    ET.SubElement(listener2, "io-model").text = "classic"
    r.append(listener2)
    vs = r.find("virtual-server")
    names = [e.text for e in vs.findall("http-listener-name")]
    if "http-listener-2" not in names:
        ET.SubElement(vs, "http-listener-name").text = "http-listener-2"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

# Route /cgi-bin through the stock cgi object and expose /perfdump.  The
# mappings go ahead of every existing NameTrans: whichever pfx2dir for a
# prefix comes first wins.
add_mappings() {
    python3 - <<PY
import os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
lines = open(path).read().splitlines(True)
mappings = ['NameTrans fn="assign-name" from="/perfdump" name="epi-perf"\n',
            'NameTrans fn="pfx2dir" from="/cgi-bin" dir="${CGI_DIR}" '
            'name="cgi"\n']
for i, line in enumerate(lines):
    if line.startswith('NameTrans'):
        break
else:
    raise SystemExit('obj.conf: no NameTrans line to insert before')
text = ''.join(lines[:i] + mappings + lines[i:])
text += '\n<Object name="epi-perf">\nService fn="service-dump"\n</Object>\n'
open(path, 'w').write(text)
PY
}

start_server() {
    : > "${ERRLOG}" 2>/dev/null || true
    "${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-epi.log 2>&1 &
    SERVER_PID=$!
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        ss -tln 2>/dev/null | grep -q ":${PORT} " && break
        sleep 1; waited=$((waited+1))
    done
    sleep 1
}

stop_server() {
    "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
    SERVER_PID=""
    for p in $(pgrep -x webservd 2>/dev/null; pgrep -x webservd-wdog 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)
        case "$cmd" in *"Linux_debian_f_x86_64_32_OPT/https-test"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
    sleep 1
}

check() { if [ "$2" -eq 0 ]; then echo "[epi] $1: OK $3"; else echo "[epi] $1: FAIL $3"; FAILURES+=("$1"); fi; }

# Highest N in "WorkerPool: grew to N of M" across the log.
wp_peak() {
    grep -oE 'WorkerPool: grew to [0-9]+ of' "${ERRLOG}" 2>/dev/null \
      | grep -oE '[0-9]+' | sort -n | tail -1
}

# Highest N in "DaemonSession pool grew to N of M" across the log.
ds_peak() {
    grep -oE 'DaemonSession pool grew to [0-9]+ of' "${ERRLOG}" 2>/dev/null \
      | grep -oE '[0-9]+' | sort -n | tail -1
}

# OS thread count of the worker webservd process (the one with >5 tasks).
worker_threads() {
    for p in $(pgrep -x webservd 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)
        case "$cmd" in
            *"Linux_debian_f_x86_64_32_OPT/https-test"*)
                t=$(ls /proc/$p/task 2>/dev/null | wc -l)
                [ "${t}" -gt 5 ] && { echo "${t}"; return; }
                ;;
        esac
    done
    echo 0
}

perfdump_sessions() {
    curl -s "http://localhost:$1/perfdump" | grep -E 'Total Sessions Created' || true
}

NPROC=$(nproc 2>/dev/null || echo 1)

# ============================ config 1: all-event ============================
cd "${INSTANCE_DIR}"
configure_instance 0
add_mappings
start_server

THREADS_ALLEVENT=$(worker_threads)
echo "[epi] all-event: worker OS threads at idle: ${THREADS_ALLEVENT} (nproc=${NPROC})"

check "all-event: classic pool disabled logged" \
      "$(grep -q 'No http-listener uses io-model=classic' "${ERRLOG}" && echo 0 || echo 1)" \
      "(errors log)"
check "all-event: started 0 request processing threads" \
      "$(grep -q 'Started 0 request processing threads' "${ERRLOG}" && echo 0 || echo 1)" \
      ""
check "all-event: classic pool never grew at idle" \
      "$([ -z "$(ds_peak)" ] && echo 0 || echo 1)" \
      "(ds_peak=$(ds_peak))"

for i in $(seq 1 ${CONCURRENCY}); do
    curl -s -o /dev/null "${BASE}/cgi-bin/slow.py?${SLOW_SECS}" &
done
sleep $(( SLOW_SECS - 1 ))
WP1=$(wp_peak); WP1=${WP1:-0}
echo "[epi] all-event: WorkerPool peak under load: ${WP1}; perfdump: $(perfdump_sessions ${PORT})"
wait 2>/dev/null || true

check "all-event: event worker pool grows under load" \
      "$([ "${WP1}" -ge $(( MIN_THREADS + 5 )) ] && echo 0 || echo 1)" \
      "(peak=${WP1} >= $(( MIN_THREADS + 5 )))"
check "all-event: event traffic never grows classic pool" \
      "$([ -z "$(ds_peak)" ] && echo 0 || echo 1)" \
      "(ds_peak=$(ds_peak))"
if tail -200 "${ERRLOG}" 2>/dev/null | grep -qiE "SIGSEGV|catastrophe|assertion"; then
    check "all-event: error log clean" 1 "(see ${ERRLOG})"
else
    check "all-event: error log clean" 0 ""
fi

stop_server

# ==================== config 2: event + classic listener ====================
rm -rf "${CONFIG_DIR}"; cp -r "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
configure_instance 1
add_mappings
start_server

THREADS_MIXED=$(worker_threads)
DELTA=$(( THREADS_MIXED - THREADS_ALLEVENT ))
echo "[epi] mixed: worker OS threads at idle: ${THREADS_MIXED} (delta vs all-event: ${DELTA}, expected >= $(( 2 * NPROC )))"

check "mixed: no classic-pool-disabled message" \
      "$(grep -q 'No http-listener uses io-model=classic' "${ERRLOG}" && echo 1 || echo 0)" \
      ""
check "mixed: started min request processing threads" \
      "$(grep -q "Started ${MIN_THREADS} request processing threads" "${ERRLOG}" && echo 0 || echo 1)" \
      ""
check "mixed: idle thread count includes classic pool + KA pollers" \
      "$([ "${DELTA}" -ge $(( 2 * NPROC )) ] && echo 0 || echo 1)" \
      "(delta=${DELTA} >= $(( 2 * NPROC )))"

for i in $(seq 1 ${CONCURRENCY}); do
    curl -s -o /dev/null "${BASE}/cgi-bin/slow.py?${SLOW_SECS}" &
done
sleep $(( SLOW_SECS - 1 ))
WP2=$(wp_peak); WP2=${WP2:-0}
echo "[epi] mixed: WorkerPool peak under event load: ${WP2}; perfdump: $(perfdump_sessions ${PORT})"
wait 2>/dev/null || true

check "mixed: event worker pool grows under load" \
      "$([ "${WP2}" -ge $(( MIN_THREADS + 5 )) ] && echo 0 || echo 1)" \
      "(peak=${WP2})"
DSP=$(ds_peak); DSP=${DSP:-0}
check "mixed: event traffic still does not grow classic pool" \
      "$([ "${DSP}" -le "${MIN_THREADS}" ] && echo 0 || echo 1)" \
      "(ds_peak=${DSP}, startup min-floor spawn of ${MIN_THREADS} is expected) -- ticket 20"

for i in $(seq 1 ${CONCURRENCY}); do
    curl -s -o /dev/null "${BASE2}/cgi-bin/slow.py?${SLOW_SECS}" &
done
sleep $(( SLOW_SECS - 1 ))
DS2=$(ds_peak); DS2=${DS2:-0}
echo "[epi] mixed: DaemonSession pool peak under classic load: ${DS2}; perfdump: $(perfdump_sessions ${PORT2})"
wait 2>/dev/null || true

check "mixed: classic pool still grows for classic demand" \
      "$([ "${DS2}" -ge $(( MIN_THREADS + 5 )) ] && echo 0 || echo 1)" \
      "(ds_peak=${DS2} >= $(( MIN_THREADS + 5 )))"

if tail -200 "${ERRLOG}" 2>/dev/null | grep -qiE "SIGSEGV|catastrophe|assertion"; then
    check "mixed: error log clean" 1 "(see ${ERRLOG})"
else
    check "mixed: error log clean" 0 ""
fi

stop_server

if [ ${#FAILURES[@]} -ne 0 ]; then
    echo "event pool isolation test FAILED: ${FAILURES[*]}"
    exit 1
fi
echo "event pool isolation test passed."
