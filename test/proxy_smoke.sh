#!/bin/bash
#
# Reverse proxy configuration, routing, sticky session, and dynamic
# reconfiguration smoke test.
#
# Verifies:
#   1. Single-origin reverse-proxy startup and request forwarding (NameTrans
#      fn="map", NameTrans fn="reverse-map", Route fn="set-origin-server",
#      Service fn="proxy-retrieve") over classic HTTP/1.1, event HTTP/1.1,
#      and HTTP/2, including Location and Set-Cookie domain reverse-mapping
#      and repeated root ("/") map translation without mutating the directive
#      pblock in place.
#   2. Multi-origin reverse-proxy configuration applied via dynamic
#      reconfiguration distributes requests across multiple origin servers.
#   3. Multi-origin sticky routing via default JSESSIONID + JROUTE cookie,
#      URI ;jsessionid=...:<jroute> parameter, and custom sticky-cookie /
#      sticky-param / route-cookie / route-hdr settings.
#   4. Dynamic reconfiguration rollback when set-origin-server has an invalid
#      origin URL (or missing server parameter): the server rolls back cleanly
#      without crashing or corrupting VSManager state, continues serving
#      requests, accepts a subsequent valid reconfiguration, and restarts
#      cleanly.
#   5. Repeated dynamic reconfigurations cycling across single-origin,
#      multi-origin, and custom sticky-routing configurations while concurrent
#      clients drive load complete with zero worker crashes or socket errors.
#
# Usage:
#   test/proxy_smoke.sh <heliod-work-root>

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
CLASSIC_PORT=18180
EVENT_PORT=18181
ORIGIN_A_PORT=18281
ORIGIN_B_PORT=18282
TEMP_DIR=""
SERVER_PID=""
ORIGIN_A_PID=""
ORIGIN_B_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup_server() {
    "${INSTANCE_DIR}/bin/stopserv" >/dev/null 2>&1 || true
    if [ -n "${SERVER_PID:-}" ]; then
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    for p in $(pgrep -x webservd-wdog 2>/dev/null; pgrep -x webservd 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in *"${INSTANCE_DIR}"*) kill -9 "$p" 2>/dev/null || true;; esac
    done
}

cleanup_origins() {
    for pid_var in ORIGIN_A_PID ORIGIN_B_PID; do
        pid="${!pid_var:-}"
        if [ -n "${pid}" ] && kill -0 "${pid}" 2>/dev/null; then
            kill "${pid}" 2>/dev/null || true
            wait "${pid}" 2>/dev/null || true
        fi
    done
}

restore_config() {
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"
        mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
}

cleanup_all() {
    cleanup_server
    cleanup_origins
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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-proxy-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Start two lightweight HTTP/1.1 origin servers
start_origin() {
    local port="$1"
    local origin_id="$2"
    local ready_file="${TEMP_DIR}/origin-${origin_id}.ready"
    python3 - "${port}" "${origin_id}" "${ready_file}" <<'PY' &
import http.server
import socketserver
import sys

port = int(sys.argv[1])
origin_id = sys.argv[2]
ready_file = sys.argv[3]

class ReuseThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

class OriginHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def do_OPTIONS(self):
        body = f"origin={origin_id}\n".encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Origin-ID", origin_id)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/app/redir"):
            loc = f"http://127.0.0.1:{port}/app/dest"
            body = b"redirecting\n"
            self.send_response(302)
            self.send_header("Location", loc)
            self.send_header("Content-Location", loc)
            self.send_header("Set-Cookie", " backend_dom=1; domain=127.0.0.1; path=/")
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("X-Origin-ID", origin_id)
            self.end_headers()
            self.wfile.write(body)
            return

        jroute_hdr = self.headers.get("Proxy-jroute", self.headers.get("X-Custom-Jroute", ""))
        body = f"origin={origin_id} path={self.path} jroute_hdr={jroute_hdr}\n".encode("utf-8")
        self.send_response(200)
        if self.path.startswith("/app/login"):
            self.send_header("Set-Cookie", "JSESSIONID=sess123; Path=/app")
        elif self.path.startswith("/app/custom-login"):
            self.send_header("Set-Cookie", "APPSTICKY=cust456; Path=/app")
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Origin-ID", origin_id)
        self.end_headers()
        self.wfile.write(body)

httpd = ReuseThreadingHTTPServer(("127.0.0.1", port), OriginHandler)
open(ready_file, "w").write("ready\n")
httpd.serve_forever()
PY
}

start_origin "${ORIGIN_A_PORT}" "origin-a"
ORIGIN_A_PID=$!
start_origin "${ORIGIN_B_PORT}" "origin-b"
ORIGIN_B_PID=$!

for rf in "${TEMP_DIR}/origin-origin-a.ready" "${TEMP_DIR}/origin-origin-b.ready"; do
    w=0
    while [ ! -f "${rf}" ] && [ "${w}" -lt 50 ]; do
        sleep 0.1
        w=$((w + 1))
    done
    if [ ! -f "${rf}" ]; then
        echo "FAIL: origin server did not start (${rf})"
        exit 1
    fi
done

# Configure server.xml with classic and event listeners
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

for el in list(root.findall("http-listener")):
    root.remove(el)

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

write_obj_conf() {
    local mode="$1"
    python3 - "${CONFIG_DIR}/obj.conf.pristine_base" "${CONFIG_DIR}/obj.conf" "${ORIGIN_A_PORT}" "${ORIGIN_B_PORT}" "${CLASSIC_PORT}" "${mode}" <<'PY'
import sys

base_path, out_path, port_a, port_b, proxy_port, mode = sys.argv[1:7]
lines = open(base_path).read().splitlines(True)

out = []
inserted_nametrans = False
for line in lines:
    if not inserted_nametrans and line.startswith("NameTrans"):
        out.append(f'NameTrans fn="reverse-map" from="http://127.0.0.1:{port_a}/app" to="http://localhost:{proxy_port}/app" rewrite-location="true" rewrite-content-location="true" rewrite-set-cookie="true"\n')
        out.append(f'NameTrans fn="map" from="/app" name="reverse-proxy-app" to="http:/app"\n')
        out.append(f'NameTrans fn="map" from="/rootproxy/" name="reverse-proxy-app" to="http:/app"\n')
        inserted_nametrans = True
    out.append(line)

if mode == "single_origin":
    route_line = f'Route fn="set-origin-server" server="http://127.0.0.1:{port_a}" rewrite-host="true"\n'
elif mode == "multi_origin_default":
    route_line = f'Route fn="set-origin-server" server="http://127.0.0.1:{port_a}" server="http://127.0.0.1:{port_b}"\n'
elif mode == "multi_origin_custom_sticky":
    route_line = (
        f'Route fn="set-origin-server" '
        f'server="http://127.0.0.1:{port_a}" '
        f'server="http://127.0.0.1:{port_b}" '
        f'sticky-cookie="APPSTICKY" '
        f'sticky-param="appsticky" '
        f'route-cookie="APPROUTE" '
        f'route-hdr="X-Custom-Jroute" '
        f'rewrite-host="false"\n'
    )
elif mode == "legacy_host_port":
    route_line = f'Route fn="set-origin-server" hostname="127.0.0.1" port="{port_a}"\n'
elif mode == "invalid_url_with_path":
    route_line = f'Route fn="set-origin-server" server="http://127.0.0.1:{port_a}/invalid/subpath"\n'
elif mode == "invalid_missing_server":
    route_line = 'Route fn="set-origin-server"\n'
else:
    raise ValueError(f"Unknown mode: {mode}")

out.append('\n<Object name="reverse-proxy-app">\n')
out.append('ObjectType fn="http-client-config" keep-alive="true" timeout="10" connect-timeout="5" retries="1"\n')
out.append(route_line)
out.append('</Object>\n')
out.append('\n<Object ppath="http:*">\n')
out.append('Service fn="proxy-retrieve" method="*"\n')
out.append('</Object>\n')

open(out_path, "w").write("".join(out))
PY
}

cp "${CONFIG_DIR}/obj.conf" "${CONFIG_DIR}/obj.conf.pristine_base"
write_obj_conf "single_origin"

ERRLOG="${INSTANCE_DIR}/logs/errors"
: > "${ERRLOG}" 2>/dev/null || true

start_instance() {
    "${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-proxy.log 2>&1 &
    SERVER_PID=$!

    local waited=0
    while [ "${waited}" -lt 20 ]; do
        if curl -s -o /dev/null --max-time 3 "http://localhost:${CLASSIC_PORT}/" 2>/dev/null && \
           curl -s -o /dev/null --max-time 3 "http://localhost:${EVENT_PORT}/" 2>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done

    echo "FAIL: server did not start on ${CLASSIC_PORT}/${EVENT_PORT}"
    cat /tmp/${INSTANCE}-proxy.log 2>/dev/null || true
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
}

get_worker_pid() {
    for p in $(pgrep -x webservd 2>/dev/null); do
        cmd=$(tr '\0' ' ' < /proc/$p/cmdline 2>/dev/null || true)
        case "$cmd" in
            *"${INSTANCE_DIR}"*)
                echo "$p"
                ;;
        esac
    done | sort -n | tr '\n' ' '
}

start_instance
INITIAL_PIDS=$(get_worker_pid)
echo "  Server started (webservd PIDs: ${INITIAL_PIDS})"

trigger_reconfig() {
    local mode="$1"
    local expect_rollback="${2:-false}"
    local marker="CORE3280"
    if [ "${expect_rollback}" = "true" ]; then
        marker="CORE3279"
    fi
    local prev_count
    prev_count=$(grep -c "${marker}" "${ERRLOG}" 2>/dev/null || true)
    write_obj_conf "${mode}"
    "${INSTANCE_DIR}/bin/reconfig" >/dev/null 2>&1 || true
    local tries=0
    while [ "${tries}" -lt 50 ]; do
        local now_count
        now_count=$(grep -c "${marker}" "${ERRLOG}" 2>/dev/null || true)
        if [ "${now_count}" -gt "${prev_count}" ]; then
            return 0
        fi
        sleep 0.1
        tries=$((tries + 1))
    done
    echo "FAIL: timed out waiting for ${marker} on mode=${mode}"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
}

# ---------------------------------------------------------------------------
# Phase 1: Single-origin reverse-proxy forwarding and reverse-map rewriting
# ---------------------------------------------------------------------------
echo "  Phase 1: Single-origin reverse-proxy startup, forwarding, and reverse-map"

c_body=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/hello")
e_body=$(curl -s --max-time 5 "http://localhost:${EVENT_PORT}/app/hello")
h2_body=$(curl -s --http2-prior-knowledge --max-time 5 "http://localhost:${EVENT_PORT}/app/hello")

for b in "${c_body}" "${e_body}" "${h2_body}"; do
    if ! echo "${b}" | grep -q "origin=origin-a path=/app/hello"; then
        echo "FAIL: unexpected single-origin response: ${b}"
        exit 1
    fi
done
echo "  PASS: single-origin forwarding works on classic HTTP/1.1, event HTTP/1.1, and HTTP/2"

# Verify trailing-slash map ("from=/rootproxy/") works across repeated requests
# without mutating the directive pblock in place and breaking exact-match redirect
redir_code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/rootproxy")
map1=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/rootproxy/test1")
map2=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/rootproxy/test2")
redir_code2=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "http://localhost:${CLASSIC_PORT}/rootproxy")
if [ "${redir_code}" != "302" ] || [ "${redir_code2}" != "302" ] || \
   ! echo "${map1}" | grep -q "path=/app/test1" || \
   ! echo "${map2}" | grep -q "path=/app/test2"; then
    echo "FAIL: trailing-slash map or exact-match redirect failed (redir1=${redir_code}, redir2=${redir_code2}, map1=${map1}, map2=${map2})"
    exit 1
fi
echo "  PASS: NameTrans fn=map with trailing slash preserves directive pblock across requests"

# Verify reverse-map rewrites Location, Content-Location, and Set-Cookie domain
hdr_file="${TEMP_DIR}/redir_hdrs.txt"
curl -s -D "${hdr_file}" -o /dev/null --max-time 5 "http://localhost:${CLASSIC_PORT}/app/redir"
if ! grep -qi "Location: http://localhost:${CLASSIC_PORT}/app/dest" "${hdr_file}" || \
   ! grep -qi "Content-Location: http://localhost:${CLASSIC_PORT}/app/dest" "${hdr_file}" || \
   ! grep -qi "domain=localhost" "${hdr_file}"; then
    echo "FAIL: reverse-map did not rewrite Location / Content-Location / Set-Cookie domain:"
    cat "${hdr_file}"
    exit 1
fi
echo "  PASS: reverse-map rewrites Location, Content-Location, and Set-Cookie domain"

# ---------------------------------------------------------------------------
# Phase 2: Dynamic reconfiguration to multi-origin + sticky routing
# ---------------------------------------------------------------------------
echo "  Phase 2: Multi-origin routing and sticky session affinity (cookie and URI param)"
trigger_reconfig "multi_origin_default"

seen_a=0
seen_b=0
for i in $(seq 1 20); do
    resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/ping?i=${i}")
    case "${resp}" in
        *origin=origin-a*) seen_a=$((seen_a + 1)) ;;
        *origin=origin-b*) seen_b=$((seen_b + 1)) ;;
    esac
done
if [ "${seen_a}" -eq 0 ] || [ "${seen_b}" -eq 0 ]; then
    echo "FAIL: multi-origin did not distribute across both origins (origin-a=${seen_a}, origin-b=${seen_b})"
    exit 1
fi
echo "  PASS: multi-origin distributes non-sticky requests across origins (origin-a=${seen_a}, origin-b=${seen_b})"

# Obtain sticky cookie + JROUTE from /app/login
login_hdrs="${TEMP_DIR}/login_hdrs.txt"
login_body=$(curl -s -D "${login_hdrs}" --max-time 5 "http://localhost:${CLASSIC_PORT}/app/login")
sticky_origin=$(echo "${login_body}" | sed -n 's/.*origin=\([^ ]*\).*/\1/p')
jroute_val=$(grep -i '^Set-cookie: *JROUTE=' "${login_hdrs}" | sed -n 's/.*JROUTE=\([^;[:space:]]*\).*/\1/p' | tr -d '\r')

if [ -z "${sticky_origin}" ] || [ "${#jroute_val}" -ne 16 ]; then
    echo "FAIL: expected 16-char JROUTE cookie from /app/login, got jroute='${jroute_val}' body='${login_body}'"
    cat "${login_hdrs}"
    exit 1
fi

# Verify 12 consecutive requests with JROUTE cookie stick to sticky_origin
for i in $(seq 1 12); do
    resp=$(curl -s --max-time 5 -H "Cookie: JSESSIONID=sess123; JROUTE=${jroute_val}" "http://localhost:${CLASSIC_PORT}/app/check?i=${i}")
    if ! echo "${resp}" | grep -q "origin=${sticky_origin}"; then
        echo "FAIL: JROUTE cookie request ${i} routed to wrong origin: ${resp} (expected ${sticky_origin})"
        exit 1
    fi
done

# Verify 12 consecutive requests with ;jsessionid=sess123:<jroute> URI param stick to sticky_origin
for i in $(seq 1 12); do
    resp=$(curl -s --max-time 5 "http://localhost:${EVENT_PORT}/app/check;jsessionid=sess123:${jroute_val}?i=${i}")
    if ! echo "${resp}" | grep -q "origin=${sticky_origin}"; then
        echo "FAIL: jsessionid URI param request ${i} routed to wrong origin: ${resp} (expected ${sticky_origin})"
        exit 1
    fi
done
echo "  PASS: default JROUTE cookie and ;jsessionid URI parameter pin requests to ${sticky_origin}"

# Test custom sticky-cookie / sticky-param / route-cookie / route-hdr via reconfig
trigger_reconfig "multi_origin_custom_sticky"
cust_hdrs="${TEMP_DIR}/cust_hdrs.txt"
cust_body=$(curl -s -D "${cust_hdrs}" --max-time 5 "http://localhost:${CLASSIC_PORT}/app/custom-login")
cust_origin=$(echo "${cust_body}" | sed -n 's/.*origin=\([^ ]*\).*/\1/p')
approute_val=$(grep -i '^Set-cookie: *APPROUTE=' "${cust_hdrs}" | sed -n 's/.*APPROUTE=\([^;[:space:]]*\).*/\1/p' | tr -d '\r')

if [ -z "${cust_origin}" ] || [ "${#approute_val}" -ne 16 ]; then
    echo "FAIL: expected 16-char APPROUTE cookie from /app/custom-login, got '${approute_val}'"
    cat "${cust_hdrs}"
    exit 1
fi

for i in $(seq 1 10); do
    resp=$(curl -s --max-time 5 -H "Cookie: APPSTICKY=cust456; APPROUTE=${approute_val}" "http://localhost:${CLASSIC_PORT}/app/custom-check?i=${i}")
    if ! echo "${resp}" | grep -q "origin=${cust_origin}" || ! echo "${resp}" | grep -q "jroute_hdr=${approute_val}"; then
        echo "FAIL: custom sticky cookie/header failed on iteration ${i}: ${resp}"
        exit 1
    fi
done
echo "  PASS: custom sticky-cookie, route-cookie, and route-hdr work after dynamic reconfig"

# ---------------------------------------------------------------------------
# Phase 3: Invalid route config rollback + subsequent valid reconfig & restart
# ---------------------------------------------------------------------------
echo "  Phase 3: Invalid set-origin-server rollback and clean server restart"
trigger_reconfig "invalid_url_with_path" "true"
trigger_reconfig "invalid_missing_server" "true"

# Previous valid configuration must still serve traffic and worker PIDs must be unchanged
post_rollback_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/after-rollback")
if ! echo "${post_rollback_resp}" | grep -q "origin=origin-"; then
    echo "FAIL: reverse proxy stopped serving after configuration rollback: ${post_rollback_resp}"
    exit 1
fi

trigger_reconfig "legacy_host_port"
legacy_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/legacy")
if ! echo "${legacy_resp}" | grep -q "origin=origin-a"; then
    echo "FAIL: legacy hostname/port reconfiguration failed: ${legacy_resp}"
    exit 1
fi

PIDS_BEFORE_RESTART=$(get_worker_pid)
if [ "${INITIAL_PIDS}" != "${PIDS_BEFORE_RESTART}" ]; then
    echo "FAIL: worker PIDs changed across reconfigs/rollbacks (initial: ${INITIAL_PIDS}, now: ${PIDS_BEFORE_RESTART})"
    exit 1
fi

# Stop and restart the server with the reverse-proxy configuration on disk
cleanup_server
sleep 1
start_instance
INITIAL_PIDS=$(get_worker_pid)
restart_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/after-restart")
if ! echo "${restart_resp}" | grep -q "origin=origin-a"; then
    echo "FAIL: server did not forward traffic after full restart: ${restart_resp}"
    exit 1
fi
echo "  PASS: invalid configurations roll back cleanly, subsequent reconfigs succeed, and server restarts cleanly"

# ---------------------------------------------------------------------------
# Phase 4: Repeated dynamic reconfiguration under concurrent proxy load
# ---------------------------------------------------------------------------
echo "  Phase 4: Repeated dynamic reconfiguration under concurrent reverse-proxy load"

STOP_FILE="${TEMP_DIR}/stop_load"
LOAD_FAIL_FILE="${TEMP_DIR}/load_fail"

python3 - "${CLASSIC_PORT}" "${EVENT_PORT}" "${STOP_FILE}" "${LOAD_FAIL_FILE}" <<'PY' &
import http.client
import os
import sys
import threading

classic_port = int(sys.argv[1])
event_port = int(sys.argv[2])
stop_file = sys.argv[3]
fail_file = sys.argv[4]

def record_failure(msg):
    try:
        with open(fail_file, "a") as f:
            f.write(msg + "\n")
    except Exception:
        pass

def worker(port, keep_alive):
    conn = None
    reused = False
    while not os.path.exists(stop_file):
        try:
            if conn is None:
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                reused = False
            hdrs = {
                "Host": f"localhost:{port}",
                "Connection": "keep-alive" if keep_alive else "close",
            }
            conn.request("GET", "/app/load", headers=hdrs)
            resp = conn.getresponse()
            body = resp.read().decode("utf-8", errors="replace")
            if resp.status != 200 or "origin=origin-" not in body:
                record_failure(f"port {port}: unexpected status {resp.status} body={body!r}")
                break
            if not keep_alive or resp.will_close:
                conn.close()
                conn = None
                reused = False
            else:
                reused = True
        except Exception as e:
            if os.path.exists(stop_file):
                break
            if conn is not None:
                try:
                    conn.close()
                except Exception:
                    pass
                conn = None
            if reused:
                # Idle keep-alive socket closed across SIGHUP reconfig; verify fresh request succeeds
                try:
                    c2 = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                    c2.request("GET", "/app/load", headers={"Host": f"localhost:{port}", "Connection": "close"})
                    r2 = c2.getresponse()
                    b2 = r2.read().decode("utf-8", errors="replace")
                    c2.close()
                    if r2.status != 200 or "origin=origin-" not in b2:
                        record_failure(f"port {port} retry unexpected status {r2.status} body={b2!r}")
                        break
                except Exception as e2:
                    if not os.path.exists(stop_file):
                        record_failure(f"port {port} retry exception: {e2}")
                    break
            else:
                record_failure(f"port {port} fresh-conn exception: {e}")
                break
    if conn is not None:
        try:
            conn.close()
        except Exception:
            pass

threads = []
for p in (classic_port, event_port):
    for ka in (True, False):
        t = threading.Thread(target=worker, args=(p, ka), daemon=True)
        t.start()
        threads.append(t)

for t in threads:
    t.join()
PY
LOAD_PID=$!

for round in 1 2 3; do
    for mode in single_origin multi_origin_default multi_origin_custom_sticky legacy_host_port; do
        trigger_reconfig "${mode}"
    done
done

touch "${STOP_FILE}"
wait "${LOAD_PID}" 2>/dev/null || true

FINAL_PIDS=$(get_worker_pid)
if [ "${INITIAL_PIDS}" != "${FINAL_PIDS}" ]; then
    echo "FAIL: webservd worker PIDs changed during concurrent reconfiguration (initial: ${INITIAL_PIDS}, final: ${FINAL_PIDS})"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

if grep -E 'SIGSEGV|SEGV|segfault|AddressSanitizer|LeakSanitizer|catastrophe' "${ERRLOG}" >/dev/null 2>&1; then
    echo "FAIL: crash or sanitizer error detected in error log:"
    cat "${ERRLOG}"
    exit 1
fi

if [ -f "${LOAD_FAIL_FILE}" ]; then
    echo "FAIL: client errors observed during concurrent reverse-proxy reconfiguration:"
    cat "${LOAD_FAIL_FILE}"
    echo "--- error log ---"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

echo "  PASS: 12 dynamic reconfigurations under concurrent proxy load completed with zero crashes (PIDs unchanged: ${FINAL_PIDS})"
echo "Reverse proxy smoke test PASSED"
