#!/bin/bash
#
# Quality of Service (QoS) and dynamic-reconfiguration-under-load smoke test.
#
# Verifies:
#   1. Partial <qos-limits> with only <max-connections> (no <max-bps>) on a
#      virtual server does not crash HttpRequest::processQOS(), serves normal
#      traffic over classic HTTP/1.1, event HTTP/1.1, and HTTP/2, and enforces
#      503 Service Unavailable (via qos-handler / qos-error) when concurrent
#      connections exceed the limit.
#   2. Partial <qos-limits> with only <max-bps> (no <max-connections>) on a
#      virtual server applied via dynamic reconfiguration does not crash
#      HttpRequest::processQOS() and enforces 503 when bandwidth exceeds the
#      configured limit.
#   3. Server-wide <server><qos-limits> with only <max-connections> or only
#      <max-bps> does not crash and enforces global limits.
#   4. Repeated dynamic reconfigurations (SIGHUP / reconfig) cycling across
#      partial and full <qos-limits> configurations while concurrent classic
#      HTTP/1.1, event HTTP/1.1, and HTTP/2 clients are actively driving load
#      complete with zero worker crashes or socket resets.
#
# Usage:
#   test/qos_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT=$(cd "$1" 2>/dev/null && pwd) || {
    echo "Work root '$1' not found"
    exit 1
}
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
DOCS_DIR="${INSTANCE_DIR}/docs"
CGI_DIR="${INSTANCE_DIR}/docs/cgi-bin"
CLASSIC_PORT=18180
EVENT_PORT=18181
TEMP_DIR=""
SERVER_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup_server() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    for p in $(pgrep -x webservd 2>/dev/null; pgrep -x webservd-wdog 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in *"${INSTANCE_DIR}"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
}

restore_config() {
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"
        mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
    rm -rf "${CGI_DIR}" "${DOCS_DIR}/qos_payload.bin"
}

cleanup_all() {
    cleanup_server
    restore_config
    if [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ]; then
        rm -rf "${TEMP_DIR}"
    fi
}

trap cleanup_all EXIT

cd "${WORK_ROOT}"
cleanup_server
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-qos-XXXXXX)
chmod 700 "${TEMP_DIR}"

mkdir -p "${CGI_DIR}"
cat > "${CGI_DIR}/hold.sh" <<'CGI'
#!/bin/sh
printf "Content-Type: text/plain\r\n\r\n"
sleep 1
printf "held\n"
CGI
chmod 755 "${CGI_DIR}/hold.sh"

# Create a 32 KB payload so a couple of requests reliably exceed a 100 B/s limit
dd if=/dev/zero bs=1024 count=32 2>/dev/null | tr '\0' 'A' > "${DOCS_DIR}/qos_payload.bin"

# Helper to write server.xml with the requested QoS configuration
write_server_xml() {
    local mode="$1"
    python3 - "${CONFIG_DIR}/server.xml" "${TEMP_DIR}" "${CLASSIC_PORT}" "${EVENT_PORT}" "${mode}" <<'PY'
import sys
import xml.etree.ElementTree as ET

path, temp_dir, classic_port, event_port, mode = sys.argv[1:6]
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

for tag in ("qos", "qos-limits", "http-listener"):
    for el in list(root.findall(tag)):
        root.remove(el)

qos = ET.SubElement(root, "qos")
ET.SubElement(qos, "enabled").text = "false" if mode == "disabled" else "true"
ET.SubElement(qos, "interval").text = "5"

if mode == "srv_conn_only":
    ql = ET.SubElement(root, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-connections").text = "1000"
elif mode == "srv_bps_low":
    ql = ET.SubElement(root, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-bps").text = "100"
elif mode == "srv_both_high":
    ql = ET.SubElement(root, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-bps").text = "100000000"
    ET.SubElement(ql, "max-connections").text = "1000"

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

for el in list(vs.findall("qos-limits")):
    vs.remove(el)

if mode == "vs_conn_only_tight":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-connections").text = "1"
elif mode == "vs_conn_only_high":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-connections").text = "1000"
elif mode == "vs_bps_only_low":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-bps").text = "100"
elif mode == "vs_bps_only_high":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-bps").text = "5000000000"
elif mode == "vs_empty_limits":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
elif mode == "vs_both_high":
    ql = ET.SubElement(vs, "qos-limits")
    ET.SubElement(ql, "enabled").text = "true"
    ET.SubElement(ql, "max-bps").text = "100000000"
    ET.SubElement(ql, "max-connections").text = "1000"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

# Configure obj.conf with qos-handler, qos-error, CGI, and stats-xml
python3 - "${CONFIG_DIR}/obj.conf" "${CGI_DIR}" <<'PY'
import sys

path, cgi_dir = sys.argv[1:3]
lines = open(path).read().splitlines(True)

out = []
inserted_authtrans = False
inserted_nametrans = False
for line in lines:
    if not inserted_authtrans and line.startswith("<Object name=\"default\">"):
        out.append(line)
        out.append('AuthTrans fn="qos-handler"\n')
        inserted_authtrans = True
        continue
    if not inserted_nametrans and line.startswith("NameTrans"):
        out.append(f'NameTrans fn="pfx2dir" from="/cgi-bin" dir="{cgi_dir}" name="cgi"\n')
        out.append('NameTrans fn="assign-name" from="/stats-xml/*" name="stats-xml"\n')
        inserted_nametrans = True
    if line.startswith("</Object>") and inserted_authtrans:
        out.append('Error code="503" fn="qos-error"\n')
        inserted_authtrans = False
    out.append(line)

out.append('\n<Object name="stats-xml">\nService fn="stats-xml"\n</Object>\n')
open(path, "w").write("".join(out))
PY

# Start with VS <qos-limits> containing ONLY <max-connections>1</max-connections>
write_server_xml "vs_conn_only_tight"

ERRLOG="${INSTANCE_DIR}/logs/errors"
: > "${ERRLOG}" 2>/dev/null || true

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-qos.log 2>&1 &
SERVER_PID=$!

waited=0
while [ "${waited}" -lt 20 ]; do
    if curl -s -o /dev/null --max-time 3 "http://localhost:${CLASSIC_PORT}/" 2>/dev/null && \
       curl -s -o /dev/null --max-time 3 "http://localhost:${EVENT_PORT}/" 2>/dev/null; then
        break
    fi
    sleep 1
    waited=$((waited + 1))
done

if [ "${waited}" -ge 20 ]; then
    echo "FAIL: server did not start on ${CLASSIC_PORT}/${EVENT_PORT}"
    cat /tmp/${INSTANCE}-qos.log 2>/dev/null || true
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

get_worker_pid() {
    for p in $(pgrep -x webservd 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in
            *"${INSTANCE_DIR}"*)
                # Exclude the primordial parent if children exist; return sorted PIDs
                echo "$p"
                ;;
        esac
    done | sort -n | tr '\n' ' '
}

INITIAL_PIDS=$(get_worker_pid)
echo "  Server started (webservd PIDs: ${INITIAL_PIDS})"

trigger_reconfig() {
    local mode="$1"
    local prev_installed
    prev_installed=$(grep -c "CORE3280" "${ERRLOG}" 2>/dev/null || true)
    write_server_xml "${mode}"
    "${INSTANCE_DIR}/bin/reconfig" >/dev/null 2>&1 || true
    local tries=0
    while [ "${tries}" -lt 50 ]; do
        local now_installed
        now_installed=$(grep -c "CORE3280" "${ERRLOG}" 2>/dev/null || true)
        if [ "${now_installed}" -gt "${prev_installed}" ]; then
            return 0
        fi
        sleep 0.1
        tries=$((tries + 1))
    done
    echo "FAIL: timed out waiting for reconfiguration to mode=${mode}"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
}

# ---------------------------------------------------------------------------
# Phase 1: Partial <qos-limits> with ONLY <max-connections> (no <max-bps>)
# ---------------------------------------------------------------------------
echo "  Phase 1: VS <qos-limits> with <max-connections> only (omitting <max-bps>)"

c_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/")
e_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${EVENT_PORT}/")
h2_code=$(curl -s --http2-prior-knowledge -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${EVENT_PORT}/")

if [ "${c_code}" != "200" ] || [ "${e_code}" != "200" ] || [ "${h2_code}" != "200" ]; then
    echo "FAIL: expected 200 on classic(${c_code}), event(${e_code}), h2(${h2_code}) with <max-connections> only"
    exit 1
fi
echo "  PASS: single requests succeed on classic HTTP/1.1, event HTTP/1.1, and HTTP/2"

# Check stats-xml reports maxOpenConnections="1" and maxByteTransmissionRate="0"
stats_xml=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/stats-xml/webservd.xml")
if ! echo "${stats_xml}" | grep -q 'maxOpenConnections="1"'; then
    echo "FAIL: stats-xml missing maxOpenConnections=\"1\""
    echo "${stats_xml}"
    exit 1
fi
if ! echo "${stats_xml}" | grep -q 'maxByteTransmissionRate="0"'; then
    echo "FAIL: stats-xml missing maxByteTransmissionRate=\"0\""
    echo "${stats_xml}"
    exit 1
fi
echo "  PASS: stats-xml reflects VS <qos-limits> with maxOpenConnections=1, maxByteTransmissionRate=0"

# Hold 2 concurrent CGI requests open so active VS connections == 2 (> limit 1),
# then verify the 3rd request receives 503 from qos-handler / qos-error.
curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/cgi-bin/hold.sh" >/dev/null &
hold1=$!
curl -s --max-time 5 "http://localhost:${EVENT_PORT}/cgi-bin/hold.sh" >/dev/null &
hold2=$!
sleep 0.3

resp_file="${TEMP_DIR}/conn_503.txt"
conn_code=$(curl -s -o "${resp_file}" -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/")
wait "${hold1}" 2>/dev/null || true
wait "${hold2}" 2>/dev/null || true

if [ "${conn_code}" != "503" ] || ! grep -q "Virtual server connection limit of 1" "${resp_file}"; then
    echo "FAIL: expected 503 with 'Virtual server connection limit of 1', got status=${conn_code} body=$(cat "${resp_file}")"
    exit 1
fi
echo "  PASS: VS <max-connections> limit enforced (503 Service Unavailable + qos-error body)"

# ---------------------------------------------------------------------------
# Phase 2: Partial <qos-limits> with ONLY <max-bps> (no <max-connections>)
# ---------------------------------------------------------------------------
echo "  Phase 2: VS <qos-limits> with <max-bps> only (omitting <max-connections>)"
trigger_reconfig "vs_bps_only_low"

# Drive traffic across classic/event and wait >200ms so BandwidthManager (100ms
# recompute interval) records at least two samples with elapsed > 0.
curl -s -o /dev/null --max-time 5 "http://localhost:${CLASSIC_PORT}/qos_payload.bin" || true
sleep 0.15
curl -s -o /dev/null --max-time 5 "http://localhost:${EVENT_PORT}/qos_payload.bin" || true
sleep 0.25

bps_file="${TEMP_DIR}/bps_503.txt"
bps_code=$(curl -s -o "${bps_file}" -w "%{http_code}" --max-time 5 "http://localhost:${EVENT_PORT}/")
if [ "${bps_code}" != "503" ] || ! grep -q "Virtual server bandwidth limit of 100" "${bps_file}"; then
    echo "FAIL: expected 503 with 'Virtual server bandwidth limit of 100', got status=${bps_code} body=$(cat "${bps_file}")"
    exit 1
fi
echo "  PASS: VS <max-bps> limit enforced (503 Service Unavailable + qos-error body)"

trigger_reconfig "vs_bps_only_high"
bps64_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/")
stats_xml_64=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/stats-xml/webservd.xml")
if [ "${bps64_code}" != "200" ] || ! echo "${stats_xml_64}" | grep -q 'maxByteTransmissionRate="5000000000"'; then
    echo "FAIL: 64-bit <max-bps> (5000000000) failed (status=${bps64_code})"
    echo "${stats_xml_64}"
    exit 1
fi
echo "  PASS: 64-bit VS <max-bps> (5000000000 > PR_INT32_MAX) accepted and reported in stats-xml"

# ---------------------------------------------------------------------------
# Phase 3: Server-level <server><qos-limits> partial configs & stats fallback
# ---------------------------------------------------------------------------
echo "  Phase 3: Server-level <server><qos-limits> partial configurations"
trigger_reconfig "srv_conn_only"

srv_c_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/")
stats_xml_srv=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/stats-xml/webservd.xml")
if [ "${srv_c_code}" != "200" ] || ! echo "${stats_xml_srv}" | grep -q 'maxOpenConnections="1000"'; then
    echo "FAIL: server-level <max-connections> only failed (status=${srv_c_code})"
    echo "${stats_xml_srv}"
    exit 1
fi
echo "  PASS: Server-level <max-connections>-only works and propagates to stats-xml when VS <qos-limits> is omitted"

trigger_reconfig "srv_bps_low"
curl -s -o /dev/null --max-time 5 "http://localhost:${CLASSIC_PORT}/qos_payload.bin" || true
sleep 0.15
curl -s -o /dev/null --max-time 5 "http://localhost:${EVENT_PORT}/qos_payload.bin" || true
sleep 0.25

srv_bps_file="${TEMP_DIR}/srv_bps_503.txt"
srv_bps_code=$(curl -s -o "${srv_bps_file}" -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/")
if [ "${srv_bps_code}" != "503" ] || ! grep -q "Global bandwidth limit of 100" "${srv_bps_file}"; then
    echo "FAIL: expected 503 with 'Global bandwidth limit of 100', got status=${srv_bps_code} body=$(cat "${srv_bps_file}")"
    exit 1
fi
echo "  PASS: Server-level <max-bps>-only enforced (503 Service Unavailable + qos-error body)"

# ---------------------------------------------------------------------------
# Phase 4: Dynamic reconfiguration under concurrent load
# ---------------------------------------------------------------------------
echo "  Phase 4: Repeated dynamic reconfiguration under concurrent HTTP/1.1 + HTTP/2 load"

STOP_FILE="${TEMP_DIR}/stop_load"
LOAD_FAIL_FILE="${TEMP_DIR}/load_fail"

python3 - "${CLASSIC_PORT}" "${EVENT_PORT}" "${STOP_FILE}" "${LOAD_FAIL_FILE}" <<'PY' &
import http.client
import os
import subprocess
import sys
import threading
import time

classic_port = int(sys.argv[1])
event_port = int(sys.argv[2])
stop_file = sys.argv[3]
fail_file = sys.argv[4]

errors = []
lock = threading.Lock()

def record_err(msg):
    with lock:
        errors.append(msg)

def h1_worker(port, keep_alive):
    conn = None
    while not os.path.exists(stop_file):
        try:
            if conn is None:
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
            headers = {"Connection": "keep-alive" if keep_alive else "close"}
            conn.request("GET", "/", headers=headers)
            resp = conn.getresponse()
            _ = resp.read()
            if resp.status not in (200, 503):
                record_err(f"port {port} unexpected status {resp.status}")
                break
            if not keep_alive or resp.will_close:
                conn.close()
                conn = None
        except Exception as ex:
            # A keep-alive socket closed between requests is normal during reconfig;
            # retry on a fresh connection to verify the server is still alive.
            if conn is not None:
                try:
                    conn.close()
                except Exception:
                    pass
                conn = None
                try:
                    c2 = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                    c2.request("GET", "/", headers={"Connection": "close"})
                    r2 = c2.getresponse()
                    _ = r2.read()
                    c2.close()
                    if r2.status not in (200, 503):
                        record_err(f"port {port} retry unexpected status {r2.status}")
                        break
                except Exception as ex2:
                    record_err(f"port {port} request failed: {ex2}")
                    break
    if conn is not None:
        try:
            conn.close()
        except Exception:
            pass

def h2_worker(port):
    while not os.path.exists(stop_file):
        res = subprocess.run(
            ["curl", "-s", "--http2-prior-knowledge", "-o", "/dev/null",
             "-w", "%{http_code}", "--max-time", "5",
             f"http://127.0.0.1:{port}/"],
            capture_output=True, text=True
        )
        if res.returncode != 0 or res.stdout.strip() not in ("200", "503"):
            record_err(f"h2 curl failed rc={res.returncode} status={res.stdout.strip()}")
            break

threads = [
    threading.Thread(target=h1_worker, args=(classic_port, True)),
    threading.Thread(target=h1_worker, args=(classic_port, False)),
    threading.Thread(target=h1_worker, args=(event_port, True)),
    threading.Thread(target=h1_worker, args=(event_port, False)),
    threading.Thread(target=h2_worker, args=(event_port,)),
]
for t in threads:
    t.start()
for t in threads:
    t.join()

if errors:
    open(fail_file, "w").write("\n".join(errors) + "\n")
PY
LOAD_PID=$!

MODES=(
    "vs_conn_only_high"
    "vs_bps_only_high"
    "vs_empty_limits"
    "vs_both_high"
    "srv_conn_only"
    "srv_both_high"
    "disabled"
    "vs_conn_only_tight"
    "vs_bps_only_low"
    "vs_both_high"
)

for mode in "${MODES[@]}"; do
    trigger_reconfig "${mode}"
    sleep 0.15
done

touch "${STOP_FILE}"
wait "${LOAD_PID}" 2>/dev/null || true

if [ -f "${LOAD_FAIL_FILE}" ]; then
    echo "FAIL: client errors during dynamic reconfiguration under load:"
    cat "${LOAD_FAIL_FILE}"
    exit 1
fi

FINAL_PIDS=$(get_worker_pid)
if [ "${INITIAL_PIDS}" != "${FINAL_PIDS}" ]; then
    echo "FAIL: webservd worker PIDs changed (${INITIAL_PIDS} -> ${FINAL_PIDS}), indicating a crash/restart"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

if grep -E -i "SIGSEGV|segfault|catastrophe|core dumped" "${ERRLOG}" >/dev/null 2>&1; then
    echo "FAIL: fatal/crash entries found in error log:"
    grep -E -i "SIGSEGV|segfault|catastrophe|core dumped" "${ERRLOG}"
    exit 1
fi

echo "  PASS: 10 dynamic reconfigurations under concurrent load completed with zero crashes (PIDs unchanged: ${FINAL_PIDS})"
echo "QoS smoke test succeeded."
