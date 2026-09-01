#!/bin/bash
#
# Event-model worker-pool elasticity test.
#
# The event I/O model's WorkerPool must mirror the blocking DaemonSession pool:
# start at min-threads and grow toward max-threads on demand under load (and,
# like the blocking model, never shrink).  The original implementation instead
# pre-spawned max-threads at startup; this guards against that regression.
#
# It reads the pool's own growth log ("WorkerPool: grew to N of M worker
# threads", emitted at LOG_VERBOSE by spawnWorkerLocked()), so the instance is
# run at log-level finest.  The highest N seen is the pool's peak size:
#   - at idle just after startup it must equal min (proving max was NOT
#     pre-spawned), and
#   - after a burst of concurrent slow requests it must be strictly greater than
#     min and no greater than max (proving on-demand growth).
#
# Plain HTTP on 18180; reuses https-test; saves/restores config.
#
# Usage: test/worker_pool_smoke.sh <heliod-work-root>

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
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-wp-XXXXXX)
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

# Configure: event I/O on 18180, thread pool min/max pinned, log-level finest so
# the pool's growth log is emitted.
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

# Log at finest so LOG_VERBOSE growth lines appear.
log = r.find("log")
if log is None: log = ET.SubElement(r, "log")
ll = log.find("log-level")
if ll is None: ll = ET.SubElement(log, "log-level")
ll.text = "finest"

for l in r.findall("http-listener"):
    if l.findtext("name") == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${PORT}"
        for c in list(l):
            if c.tag == "io-model": l.remove(c)
        ET.SubElement(l, "io-model").text = "event"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY

# Route /cgi-bin through the stock cgi object.
python3 - <<PY
import re, os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
s = open(path).read()
nt = 'NameTrans fn="pfx2dir" from="/cgi-bin" dir="${CGI_DIR}" name="cgi"'
s2 = re.sub(r'(<Object name="default">\n)', r'\1' + nt + '\n', s, count=1)
assert s2 != s, "obj.conf: no default object"
open(path, "w").write(s2)
PY

ERRLOG="${INSTANCE_DIR}/logs/errors"
: > "${ERRLOG}" 2>/dev/null || true

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-wp.log 2>&1 &
SERVER_PID=$!
waited=0
while [ "${waited}" -lt 20 ]; do
    ss -tln 2>/dev/null | grep -q ":${PORT} " && break
    sleep 1; waited=$((waited+1))
done
sleep 1   # let startup logging settle

# Highest N in "grew to N of M worker threads" across the log (== peak pool size,
# robust to more than one server process each logging its own pool).
peak_pool() {
    grep -oE 'grew to [0-9]+ of' "${ERRLOG}" 2>/dev/null \
      | grep -oE '[0-9]+' | sort -n | tail -1
}

IDLE_PEAK=$(peak_pool); IDLE_PEAK=${IDLE_PEAK:-0}
echo "[wp] peak worker threads at idle: ${IDLE_PEAK} (min=${MIN_THREADS}, max=${MAX_THREADS})"

# Burst of concurrent slow requests to force growth.
for i in $(seq 1 ${CONCURRENCY}); do
    curl -s -o /dev/null "${BASE}/cgi-bin/slow.py?${SLOW_SECS}" &
done
sleep $(( SLOW_SECS - 1 ))   # sample while they are all still in flight
LOAD_PEAK=$(peak_pool); LOAD_PEAK=${LOAD_PEAK:-0}
echo "[wp] peak worker threads under load (${CONCURRENCY} concurrent): ${LOAD_PEAK}"
wait 2>/dev/null || true

check() { if [ "$2" -eq 0 ]; then echo "[wp] $1: OK $3"; else echo "[wp] $1: FAIL $3"; FAILURES+=("$1"); fi; }

# The bug was pre-spawning max at startup.  At idle the pool must be at min.
check "pool starts at min-threads, not max" \
      "$([ "${IDLE_PEAK}" -eq "${MIN_THREADS}" ] && echo 0 || echo 1)" \
      "(idle peak=${IDLE_PEAK}, expected ${MIN_THREADS})"
check "pool did NOT pre-spawn max at startup" \
      "$([ "${IDLE_PEAK}" -lt "${MAX_THREADS}" ] && echo 0 || echo 1)" \
      "(idle peak=${IDLE_PEAK} < ${MAX_THREADS})"
# Under load it must have grown beyond min...
check "pool grows under load" \
      "$([ "${LOAD_PEAK}" -gt "${IDLE_PEAK}" ] && echo 0 || echo 1)" \
      "(load peak=${LOAD_PEAK} > idle peak=${IDLE_PEAK})"
# ...substantially (a burst of CONCURRENCY should add many workers)...
check "growth tracks demand" \
      "$([ "${LOAD_PEAK}" -ge $(( MIN_THREADS + 5 )) ] && echo 0 || echo 1)" \
      "(load peak=${LOAD_PEAK} >= $(( MIN_THREADS + 5 )))"
# ...but never past max.
check "growth is bounded by max-threads" \
      "$([ "${LOAD_PEAK}" -le "${MAX_THREADS}" ] && echo 0 || echo 1)" \
      "(load peak=${LOAD_PEAK} <= ${MAX_THREADS})"

# error log clean of crashes/assertions
if tail -200 "${ERRLOG}" 2>/dev/null | grep -qiE "SIGSEGV|catastrophe|assertion"; then
    check "error log clean" 1 "(see ${ERRLOG})"
else
    check "error log clean" 0 ""
fi

if [ ${#FAILURES[@]} -ne 0 ]; then
    echo "worker pool test FAILED: ${FAILURES[*]}"
    exit 1
fi
echo "worker pool test passed."
