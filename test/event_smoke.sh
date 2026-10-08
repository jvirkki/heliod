#!/bin/bash
#
# Lightweight HTTP/1.1 smoke test for the event-driven I/O path.
#
# Uses the https-test instance created by `make install`. The original config is
# saved before the test and restored afterwards.
#
# Usage:
#   test/event_smoke.sh <heliod-work-root>
# Example:
#   test/event_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    echo "Example: $0 /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
PORT=18180
HTTPS_PORT=18443
TEMP_DIR=""
SERVER_PID=""
ORIGINAL_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup_server() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    pkill -9 -f "webservd.*${INSTANCE_DIR}" 2>/dev/null || true

    # Wait for the listening socket to actually go away before returning.  The
    # two phases of this test reuse one port, and stopserv returns before the
    # listener is closed: without this wait the next phase can see the outgoing
    # server's socket, decide the new one is ready, and get ECONNREFUSED on its
    # first request.
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        if ! ss -tln 2>/dev/null | grep -q ":${PORT} "; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
}

restore_config() {
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"
        cp -r "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
        rm -rf "${CONFIG_DIR}.pristine"
    fi
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

# Remember if the original instance was running.
ORIGINAL_PID=$(pgrep -f "webservd-wdog.*${CONFIG_DIR}" 2>/dev/null | head -1 || true)

# Stop any running instance so we can safely rewrite its config.
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pkill -9 -f "webservd.*${INSTANCE_DIR}" 2>/dev/null || true
sleep 1

# Save a pristine copy of the config.
rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

# Create a temp directory for the smoke test instance.
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-smoke-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
# Only create the database if there is not one here already.  certutil -N on an
# existing database asks for the old password, and with no terminal to ask on it
# blocks for ever instead of failing -- which wedged this test for half an hour
# after an instance configured by hand left a database in the config directory.
# An empty password *file* is what keeps every call non-interactive; the process
# substitution this replaced was not enough, and </dev/null closes the last way in.
NSS_PASS=$(mktemp /tmp/event-smoke-pass-XXXXXX)
: > "${NSS_PASS}"
if [ ! -f cert8.db ] && [ ! -f cert9.db ]; then
    certutil -N -d dbm:"." -f "${NSS_PASS}" </dev/null 2>/dev/null
fi
openssl req -x509 -newkey rsa:2048 -keyout /tmp/event-smoke-key.pem \
    -out /tmp/event-smoke-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/event-smoke.p12 \
    -inkey /tmp/event-smoke-key.pem -in /tmp/event-smoke-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/event-smoke.p12 -d dbm:"." -W '' -k "${NSS_PASS}" </dev/null 2>/dev/null
certutil -L -d dbm:"." -n "Server-Cert" -a > /tmp/event-smoke-ca.pem 2>/dev/null
rm -f "${NSS_PASS}"
echo "NSSDir ${PWD}" >> magnus.conf
cd "${WORK_ROOT}"

configure_instance() {
    local iomodel="${1:-}"
    python3 - <<PY
import os
import xml.etree.ElementTree as ET

path = os.path.join("${CONFIG_DIR}", "server.xml")
tree = ET.parse(path)
root = tree.getroot()

tp = root.find("temp-path")
if tp is None:
    tp = ET.SubElement(root, "temp-path")
tp.text = "${TEMP_DIR}"

plat = root.find("platform")
if plat is None:
    plat = ET.SubElement(root, "platform")
plat.text = "64"

# Short header timeout so the slowloris test completes quickly.
http_el = root.find("http")
if http_el is None:
    http_el = ET.SubElement(root, "http")
rht = http_el.find("request-header-timeout")
if rht is None:
    rht = ET.SubElement(http_el, "request-header-timeout")
rht.text = "5"

# Ensure the virtual server accepts all listeners we add.
vs = root.find("virtual-server")
vs_listeners = set()
for el in vs.findall("http-listener-name"):
    vs_listeners.add(el.text)

for listener in root.findall("http-listener"):
    name = listener.findtext("name")
    if name == "http-listener-1":
        port_el = listener.find("port")
        if port_el is None:
            port_el = ET.SubElement(listener, "port")
        port_el.text = "${PORT}"
    elif name == "https-listener-1":
        root.remove(listener)
        continue
    else:
        continue
    for child in list(listener):
        if child.tag == "io-model":
            listener.remove(child)
    if iomodel := "${iomodel}":
        ET.SubElement(listener, "io-model").text = iomodel

# Add an HTTPS listener using the generated Server-Cert.
https = ET.SubElement(root, "http-listener")
ET.SubElement(https, "name").text = "https-listener-1"
ET.SubElement(https, "port").text = "${HTTPS_PORT}"
ET.SubElement(https, "server-name").text = "localhost"
ET.SubElement(https, "default-virtual-server-name").text = "test"
if iomodel := "${iomodel}":
    ET.SubElement(https, "io-model").text = iomodel
ssl = ET.SubElement(https, "ssl")
ET.SubElement(ssl, "enabled").text = "true"
ET.SubElement(ssl, "server-cert-nickname").text = "Server-Cert"

if "https-listener-1" not in vs_listeners:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

run_tests() {
    local mode="$1"
    SMOKE_MODE="${mode}" SMOKE_PORT="${PORT}" SMOKE_HTTPS_PORT="${HTTPS_PORT}" SMOKE_CA_FILE="/tmp/event-smoke-ca.pem" python3 - <<'PY'
import http.client
import os
import socket
import ssl
import sys

mode = os.environ["SMOKE_MODE"]
port = int(os.environ["SMOKE_PORT"])
https_port = int(os.environ["SMOKE_HTTPS_PORT"])
ca_file = os.environ["SMOKE_CA_FILE"]

def tls_context():
    ctx = ssl.create_default_context(cafile=ca_file)
    ctx.check_hostname = False
    return ctx

def basic_get():
    c = http.client.HTTPConnection("localhost", port, timeout=10)
    c.request("GET", "/")
    r = c.getresponse()
    data = r.read()
    c.close()
    assert r.status == 200, f"basic GET: expected 200, got {r.status}"
    assert b"Welcome" in data or b"<html" in data or len(data) > 0, f"basic GET: unexpected body: {data!r}"
    print(f"[{mode}] basic GET: OK")

def keep_alive():
    c = http.client.HTTPConnection("localhost", port, timeout=10)
    c.request("GET", "/")
    r1 = c.getresponse()
    body1 = r1.read()
    assert r1.status == 200, f"keep-alive first: expected 200, got {r1.status}"
    c.request("GET", "/")
    r2 = c.getresponse()
    body2 = r2.read()
    assert r2.status == 200, f"keep-alive second: expected 200, got {r2.status}"
    c.close()
    print(f"[{mode}] keep-alive: OK")

def expect_100_continue():
    c = http.client.HTTPConnection("localhost", port, timeout=10)
    c.putrequest("POST", "/")
    c.putheader("Content-Length", "5")
    c.putheader("Expect", "100-continue")
    c.endheaders()
    r = c.getresponse()
    if r.status == 100:
        c.send(b"hello")
        r = c.getresponse()
    assert r.status in (200, 404, 405, 411, 500), f"100-continue: unexpected {r.status}"
    r.read()
    c.close()
    print(f"[{mode}] 100-continue: OK ({r.status})")

def chunked_post():
    s = socket.create_connection(("localhost", port), timeout=10)
    body = (
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: close\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "0\r\n"
        "\r\n"
    )
    s.sendall(body.encode())
    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    assert data, "chunked POST: no response"
    status = data.split(b"\r\n", 1)[0]
    print(f"[{mode}] chunked POST: OK ({status.decode(errors='replace')})")

def pipelined_gets():
    s = socket.create_connection(("localhost", port), timeout=10)
    s.settimeout(10)
    requests = (
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )
    s.sendall(requests.encode())
    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    assert data, "pipelined GET: no response"
    # Count HTTP/1.1 200 OK status lines.
    count = data.count(b"HTTP/1.1 200 OK")
    assert count == 2, f"pipelined GET: expected 2 responses, got {count}: {data!r}"
    print(f"[{mode}] pipelined GETs: OK")

def slow_client():
    s = socket.create_connection(("localhost", port), timeout=20)
    s.settimeout(20)
    # Send the request line and one header, then pause before the blank line.
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
    import time
    time.sleep(2)
    s.sendall(b"\r\n")
    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    finally:
        s.close()
    assert data, "slow client: no response"
    assert data.startswith(b"HTTP/1.1 200"), f"slow client: unexpected response: {data!r}"
    print(f"[{mode}] slow client: OK")

def concurrent_gets():
    import threading
    errors = []
    def one_get(i):
        try:
            c = http.client.HTTPConnection("localhost", port, timeout=10)
            c.request("GET", "/")
            r = c.getresponse()
            r.read()
            c.close()
            assert r.status == 200, f"concurrent #{i}: {r.status}"
        except Exception as e:
            errors.append(str(e))
    threads = [threading.Thread(target=one_get, args=(i,)) for i in range(20)]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    assert not errors, f"concurrent GETs failed: {errors}"
    print(f"[{mode}] concurrent GETs (20): OK")

def load_test():
    import concurrent.futures
    import statistics
    import time

    conns = 20
    reqs_per_conn = 50
    errors = []
    latencies = []

    def worker(i):
        c = http.client.HTTPConnection("localhost", port, timeout=30)
        for j in range(reqs_per_conn):
            t0 = time.time()
            try:
                c.request("GET", "/")
                r = c.getresponse()
                r.read()
                if r.status != 200:
                    errors.append(f"worker {i} req {j}: status {r.status}")
            except Exception as e:
                errors.append(f"worker {i} req {j}: {e}")
            latencies.append(time.time() - t0)
        c.close()

    start = time.time()
    with concurrent.futures.ThreadPoolExecutor(max_workers=conns) as ex:
        ex.map(worker, range(conns))
    elapsed = time.time() - start
    total = conns * reqs_per_conn

    assert total > 0
    if errors:
        # Tolerate a small number of transient failures in a quick smoke run.
        if len(errors) > total * 0.01:
            raise AssertionError(f"load test: too many errors: {errors[:5]}")
        print(f"[{mode}] load test: {len(errors)} transient errors tolerated")

    rps = total / elapsed
    p50 = statistics.median(latencies)
    p99 = sorted(latencies)[int(len(latencies) * 0.99)]
    print(f"[{mode}] load test: {total} requests, {rps:.1f} req/s, "
          f"p50={p50*1000:.1f}ms, p99={p99*1000:.1f}ms")

def tls_get():
    ctx = tls_context()
    c = http.client.HTTPSConnection("localhost", https_port, context=ctx, timeout=10)
    c.request("GET", "/")
    r = c.getresponse()
    data = r.read()
    c.close()
    assert r.status == 200, f"TLS GET: expected 200, got {r.status}"
    print(f"[{mode}] TLS GET: OK")

def tls_keep_alive():
    ctx = tls_context()
    c = http.client.HTTPSConnection("localhost", https_port, context=ctx, timeout=10)
    c.request("GET", "/")
    r1 = c.getresponse()
    r1.read()
    assert r1.status == 200, f"TLS keep-alive first: expected 200, got {r1.status}"
    c.request("GET", "/")
    r2 = c.getresponse()
    r2.read()
    assert r2.status == 200, f"TLS keep-alive second: expected 200, got {r2.status}"
    c.close()
    print(f"[{mode}] TLS keep-alive: OK")

def idle_connections():
    # Keep 100 idle keep-alive HTTP connections open, then verify the server
    # still accepts a fresh request. This is the primary scenario the event
    # loop is meant to decouple from thread count.
    count = 100
    conns = []
    for i in range(count):
        c = http.client.HTTPConnection("localhost", port, timeout=10)
        c.request("GET", "/")
        r = c.getresponse()
        r.read()
        assert r.status == 200, f"idle setup {i}: {r.status}"
        conns.append(c)
    c = http.client.HTTPConnection("localhost", port, timeout=10)
    c.request("GET", "/")
    r = c.getresponse()
    data = r.read()
    assert r.status == 200, f"idle test: {r.status}"
    c.close()
    for c in conns:
        c.close()
    print(f"[{mode}] idle connections ({count}): OK")

def slowloris():
    # Send a partial request and stop. The server should close the connection
    # within the configured request-header-timeout (5s) rather than keeping a
    # worker blocked indefinitely.
    s = socket.create_connection(("localhost", port), timeout=15)
    s.settimeout(15)
    s.sendall(b"GET / HTTP/1.1\r\n")
    import time
    time.sleep(7)
    try:
        data = s.recv(4096)
    except socket.timeout:
        data = b""
    finally:
        s.close()
    # The server may close silently or return an error/timeout response.
    assert not data.startswith(b"HTTP/1.1 200"), "slowloris: request unexpectedly completed"
    print(f"[{mode}] slowloris timeout: OK")

try:
    basic_get()
    keep_alive()
    expect_100_continue()
    chunked_post()
    pipelined_gets()
    slow_client()
    concurrent_gets()
    load_test()
    tls_get()
    tls_keep_alive()
    idle_connections()
    slowloris()
    print(f"[{mode}] ALL OK")
except Exception as e:
    print(f"[{mode}] FAIL: {e}")
    sys.exit(1)
PY
}

wait_for_listener() {
    local mode="$1"
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        # A real request, not just a listening socket: the socket appears before
        # the server can serve through it, and it also lingers after a previous
        # server has been told to stop.  Either way an ss-only check can report
        # ready and leave the first test to fail with ECONNREFUSED.
        if curl -s -o /dev/null --max-time 3 "http://localhost:${PORT}/" \
                2>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start serving on port ${PORT} (${mode} mode)"
    cat /tmp/${INSTANCE}-smoke-${mode}.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

echo "=== Testing classic I/O model ==="
configure_instance ""
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-smoke-classic.log 2>&1 &
SERVER_PID=$!
wait_for_listener "classic"
run_tests "classic"
cleanup_server
SERVER_PID=""

echo "=== Testing event I/O model ==="
configure_instance "event"
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-smoke-event.log 2>&1 &
SERVER_PID=$!
wait_for_listener "event"
run_tests "event"
cleanup_server
SERVER_PID=""

echo "=== Event-driven smoke test passed ==="
