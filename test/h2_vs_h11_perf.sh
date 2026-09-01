#!/bin/bash
#
# HTTP/2 vs HTTP/1.1 throughput comparison.
#
# Recreates the head-to-head benchmark run during H2 development, which showed
# HTTP/2 at roughly half the throughput of HTTP/1.1 and thereby exposed the
# implementation problems since fixed (worker monopolization, serial in-callback
# dispatch, whole-body buffering, per-stream Connection leaks).  Both protocols
# are driven by the SAME tool (h2load) against the SAME asset on the SAME TLS
# listener, so the only variable is the protocol: `h2load --h1` negotiates
# http/1.1 via ALPN, plain h2load negotiates h2.
#
# h2load is not built here.  Install it once (Debian/Ubuntu: the nghttp2-client
# package) and put it on PATH, or point H2LOAD at it.  If absent the test SKIPs.
#
# This generates real load; run it alone (nothing else touching the instance),
# and note the host may be shared.  Tunable via env: REQUESTS, CLIENTS, STREAMS.
#
# Usage: test/h2_vs_h11_perf.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
DOCROOT="${INSTANCE_DIR}/docs"
HTTP_PORT=18180
HTTPS_PORT=18443
REQUESTS="${REQUESTS:-20000}"
CLIENTS="${CLIENTS:-50}"
STREAMS="${STREAMS:-32}"       # concurrent streams per connection for the h2 run
# Repeat the pair of runs this many times and report the mean.  A single sample on a
# small or busy host -- and the load generator competes with the server for CPU here,
# since both are on this box -- is noise, so the default takes three.
REPEAT="${REPEAT:-3}"
# h2load's full reports are kept rather than discarded: the req/s line alone hides
# whether every request actually succeeded, and the latency spread is the interesting
# part of an h2-vs-h1 comparison.
RESULTS_DIR="${RESULTS_DIR:-/tmp/h2-vs-h11-perf}"
# Regression guard: fail if h2 throughput drops below this fraction of h1's.
# The bug being guarded against had the ratio near 0.5.
MIN_RATIO="${MIN_RATIO:-0.80}"
TEMP_DIR=""
SERVER_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

H2LOAD="${H2LOAD:-}"
[ -z "${H2LOAD}" ] && command -v h2load >/dev/null 2>&1 && H2LOAD="$(command -v h2load)"
if [ -z "${H2LOAD}" ]; then
    echo "[perf] SKIP: h2load not found (install nghttp2-client or set H2LOAD)."
    exit 0
fi

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
    fi
    for p in $(pgrep -x webservd 2>/dev/null; pgrep -x webservd-wdog 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null)
        case "$cmd" in *"Linux_debian_f_x86_64_32_OPT/https-test"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"; mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
    rm -f "${DOCROOT}/perf.html" 2>/dev/null || true
    [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ] && rm -rf "${TEMP_DIR}"
    # RESULTS_DIR is deliberately left in place -- the h2load reports are the output.
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
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-perf-XXXXXX)
chmod 700 "${TEMP_DIR}"

# A representative static asset (~8 KB of HTML).
python3 - "${DOCROOT}/perf.html" <<'PY'
import sys
body = "<p>" + ("x"*80) + "</p>\n"
with open(sys.argv[1], "w") as f:
    f.write("<!doctype html><title>perf</title>\n")
    for _ in range(100):
        f.write(body)
PY

# Self-signed cert + h2/TLS listener on 18443, event I/O.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp); printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/perf-key.pem -out /tmp/perf-cert.pem \
    -days 90 -nodes -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" 2>/dev/null
openssl pkcs12 -export -out /tmp/perf.p12 -inkey /tmp/perf-key.pem -in /tmp/perf-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/perf.p12 -d dbm:"." -w "${NO_PASS}"
echo "NSSDir ${PWD}" >> magnus.conf
rm -f "${NO_PASS}" /tmp/perf-key.pem /tmp/perf-cert.pem /tmp/perf.p12
cd "${WORK_ROOT}"

python3 - <<PY
import xml.etree.ElementTree as ET, os
path = os.path.join("${CONFIG_DIR}", "server.xml")
t = ET.parse(path); r = t.getroot()
def ce(par, tag):
    e = par.find(tag)
    if e is None: e = ET.SubElement(par, tag)
    return e
ce(r, "temp-path").text = "${TEMP_DIR}"
ce(r, "platform").text = "64"
for l in list(r.findall("http-listener")):
    if l.findtext("name") == "https-listener-1": r.remove(l)
for l in r.findall("http-listener"):
    if l.findtext("name") == "http-listener-1":
        ce(l, "port").text = "${HTTP_PORT}"
        for c in list(l):
            if c.tag == "io-model": l.remove(c)
        ET.SubElement(l, "io-model").text = "event"
vs = r.find("virtual-server")
vsl = set(e.text for e in vs.findall("http-listener-name"))
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

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-perf.log 2>&1 &
SERVER_PID=$!
waited=0
while [ "${waited}" -lt 20 ]; do
    ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} " && break
    sleep 1; waited=$((waited+1))
done

URL="https://localhost:${HTTPS_PORT}/perf.html"
echo "[perf] asset=$(wc -c < "${DOCROOT}/perf.html")B  requests=${REQUESTS} clients=${CLIENTS} h2-streams=${STREAMS} repeat=${REPEAT}"
echo "[perf] host: $(nproc) cpu, load$(cut -d' ' -f1-3 /proc/loadavg | sed 's/^/ /')"
echo "[perf] warming up..."
"${H2LOAD}" -n 1000 -c 10 "${URL}" >/dev/null 2>&1 || true

reqs_per_sec() { grep -oE 'finished in .*, [0-9.]+ req/s' "$1" | grep -oE '[0-9.]+ req/s' | grep -oE '[0-9.]+'; }

# h2load reports "N total, ... N succeeded, N failed, N errored, N timeout".  A run
# that fails requests can still post a fine req/s, so throughput is only meaningful
# once this says everything succeeded.
requests_ok() {
    awk '/^requests:/ {
             for (i = 1; i <= NF; i++) {
                 if ($(i+1) ~ /^(failed|errored|timeout)/ && $i + 0 > 0) bad = 1
             }
             print (bad ? "no" : "yes"); found = 1
         }
         END { if (!found) print "no" }' "$1"
}
# The two lines worth seeing next to each other: per-request service time, and how
# long the client waited for the first byte.
latency() { grep -E '^(time for request|time to 1st byte):' "$1" | sed 's/^/      /'; }

mkdir -p "${RESULTS_DIR}"
H1_LIST=""; H2_LIST=""
run=1
while [ "${run}" -le "${REPEAT}" ]; do
    H1_OUT="${RESULTS_DIR}/h2load-h1-run${run}.txt"
    H2_OUT="${RESULTS_DIR}/h2load-h2-run${run}.txt"
    echo "[perf] run ${run}/${REPEAT}: HTTP/1.1 (h2load --h1)..."
    "${H2LOAD}" --h1 -n "${REQUESTS}" -c "${CLIENTS}" "${URL}" > "${H1_OUT}" 2>&1 || true
    echo "[perf] run ${run}/${REPEAT}: HTTP/2 (h2load, ${STREAMS} streams/conn)..."
    "${H2LOAD}" -n "${REQUESTS}" -c "${CLIENTS}" -m "${STREAMS}" "${URL}" > "${H2_OUT}" 2>&1 || true

    h1=$(reqs_per_sec "${H1_OUT}"); h1=${h1:-0}
    h2=$(reqs_per_sec "${H2_OUT}"); h2=${h2:-0}
    H1_LIST="${H1_LIST} ${h1}"; H2_LIST="${H2_LIST} ${h2}"
    r=$(awk "BEGIN{ if (${h1}>0) printf \"%.3f\", ${h2}/${h1}; else print \"0\" }")
    echo "    HTTP/1.1 ${h1} req/s (all succeeded: $(requests_ok "${H1_OUT}"))"
    latency "${H1_OUT}"
    echo "    HTTP/2   ${h2} req/s (all succeeded: $(requests_ok "${H2_OUT}"))"
    latency "${H2_OUT}"
    echo "    ratio ${r}"
    run=$((run+1))
done

# Report the mean across runs; a single sample on a busy or small host is noise.
H1=$(echo ${H1_LIST} | awk '{s=0; for(i=1;i<=NF;i++) s+=$i; printf "%.2f", s/NF}')
H2=$(echo ${H2_LIST} | awk '{s=0; for(i=1;i<=NF;i++) s+=$i; printf "%.2f", s/NF}')
RATIO=$(awk "BEGIN{ if (${H1}>0) printf \"%.3f\", ${H2}/${H1}; else print \"0\" }")
echo "=========================================================================="
echo " HTTP/1.1 : ${H1} req/s   (runs:${H1_LIST} )"
echo " HTTP/2   : ${H2} req/s   (runs:${H2_LIST} )"
echo " H2/H1 ratio : ${RATIO}   (was ~0.5 before the H2 fixes; guard >= ${MIN_RATIO})"
echo "=========================================================================="
echo "[perf] full h2load reports kept in ${RESULTS_DIR}"

# A run that dropped requests invalidates the throughput number, so check that
# before comparing rates.
FAILED_ARM=""
for f in "${RESULTS_DIR}"/h2load-h1-run*.txt; do
    [ "$(requests_ok "$f")" = "yes" ] || FAILED_ARM="${FAILED_ARM} $(basename "$f")"
done
for f in "${RESULTS_DIR}"/h2load-h2-run*.txt; do
    [ "$(requests_ok "$f")" = "yes" ] || FAILED_ARM="${FAILED_ARM} $(basename "$f")"
done
if [ -n "${FAILED_ARM}" ]; then
    echo "[perf] FAIL: not every request succeeded in:${FAILED_ARM}"
    for f in ${FAILED_ARM}; do echo "--- ${f} ---"; cat "${RESULTS_DIR}/${f}"; done
    exit 1
fi

# Fail only if h2 fell back to roughly half of h1 (the original defect).
OK=$(awk "BEGIN{ print (${H1}>0 && ${RATIO} >= ${MIN_RATIO}) ? 1 : 0 }")
if [ "${OK}" != "1" ]; then
    echo "[perf] FAIL: HTTP/2 throughput is below ${MIN_RATIO} of HTTP/1.1"
    for f in "${RESULTS_DIR}"/h2load-*.txt; do echo "--- $(basename "$f") ---"; cat "$f"; done
    exit 1
fi
echo "[perf] OK: HTTP/2 is at ${RATIO} of HTTP/1.1 (no half-throughput regression)."
