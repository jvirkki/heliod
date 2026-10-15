#!/bin/bash
#
# TLS smoke test for Phase 1a security hardening and ALPN io-model gating.
#
# Reuses the https-test instance created by `make install`.  The original
# config is saved before the test and restored afterwards.

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    echo "Example: $0 /tmp/heliod-tls-smoke/work/Linux_debian_f_x86_64_32_OPT"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
HTTP_PORT=18180
HTTPS_PORT=18443
TEMP_DIR=""
SERVER_PID=""
NO_PASS=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup_server() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
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
    if [ -n "${NO_PASS:-}" ] && [ -f "${NO_PASS}" ]; then
        rm -f "${NO_PASS}"
    fi
    rm -f /tmp/tls-smoke.p12 /tmp/tls-smoke-key.pem /tmp/tls-smoke-cert.pem /tmp/tls-smoke-ca.pem
}

trap cleanup_all EXIT

cd "${WORK_ROOT}"

# Stop any running instance so we can safely rewrite its config.
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

# Save a pristine copy of the config.
rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

# Create a temp directory for the smoke test instance.
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-tls-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
if [ ! -f cert8.db ] && [ ! -f cert9.db ]; then
    certutil -N -d dbm:"." -f "${NO_PASS}" </dev/null 2>/dev/null
fi
openssl req -x509 -newkey rsa:2048 -keyout /tmp/tls-smoke-key.pem \
    -out /tmp/tls-smoke-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/tls-smoke.p12 \
    -inkey /tmp/tls-smoke-key.pem -in /tmp/tls-smoke-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/tls-smoke.p12 -d dbm:"." -w "${NO_PASS}" </dev/null
CA_FILE="/tmp/tls-smoke-ca.pem"
certutil -L -d dbm:"." -n "Server-Cert" -a > "${CA_FILE}" 2>/dev/null
echo "NSSDir ${PWD}" >> magnus.conf
cd "${WORK_ROOT}"

# Configure server.xml with default (classic) io-model on both HTTP and
# HTTPS listeners.
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

vs = root.find("virtual-server")
vs_listeners = set()
for el in vs.findall("http-listener-name"):
    vs_listeners.add(el.text)

for listener in list(root.findall("http-listener")):
    name = listener.findtext("name")
    if name == "http-listener-1":
        port_el = listener.find("port")
        if port_el is None:
            port_el = ET.SubElement(listener, "port")
        port_el.text = "${HTTP_PORT}"
        for child in list(listener):
            if child.tag == "io-model":
                listener.remove(child)
    elif name == "https-listener-1":
        root.remove(listener)

# HTTPS listener with default io-model (classic).
https = ET.SubElement(root, "http-listener")
ET.SubElement(https, "name").text = "https-listener-1"
ET.SubElement(https, "port").text = "${HTTPS_PORT}"
ET.SubElement(https, "server-name").text = "localhost"
ET.SubElement(https, "default-virtual-server-name").text = "test"
ssl = ET.SubElement(https, "ssl")
ET.SubElement(ssl, "enabled").text = "true"
ET.SubElement(ssl, "server-cert-nickname").text = "Server-Cert"

if "https-listener-1" not in vs_listeners:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY

cd "${INSTANCE_DIR}"
ulimit -n 4096
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-tls.log 2>&1 &
SERVER_PID=$!

waited=0
while [ "${waited}" -lt 20 ]; do
    if ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
        break
    fi
    sleep 1
    waited=$((waited + 1))
done
if ! ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
    echo "Server did not start listening on HTTPS port ${HTTPS_PORT}"
    cat /tmp/${INSTANCE}-tls.log 2>/dev/null || true
    exit 1
fi

# First verify the HTTP port is serving; if not, the server did not start.
if ! curl -sf "http://localhost:${HTTP_PORT}/" >/dev/null; then
    echo "Server failed to start on HTTP port ${HTTP_PORT}; aborting TLS tests."
    exit 1
fi
echo "HTTP listener OK"

run_client() {
    openssl s_client -connect "localhost:${HTTPS_PORT}" "$@" -CAfile "${CA_FILE}" </dev/null 2>/dev/null
}

# Verify TLS 1.2 handshake succeeds.
run_client -tls1_2 | grep -q 'Verify return code: 0' || { echo "TLS 1.2 failed"; exit 1; }
echo "TLS 1.2 OK"

# Verify TLS 1.3 handshake succeeds (requires OpenSSL 1.1.1+).
run_client -tls1_3 | grep -q 'Verify return code: 0' || { echo "TLS 1.3 failed"; exit 1; }
echo "TLS 1.3 OK"

# Verify ALPN negotiation advertises http/1.1.
run_client -tls1_2 -alpn http/1.1 | grep -q 'ALPN protocol: http/1.1' || { echo "ALPN failed"; exit 1; }
echo "ALPN http/1.1 OK"

# Regression test: on a TLS listener with default (classic)
# io-model, when a client offers both h2 and http/1.1 via ALPN, the negotiated
# protocol must be http/1.1 (never h2) and GET / must return 200 (not 505).
python3 - <<PY
import socket
import ssl
import sys

host = 'localhost'
port = ${HTTPS_PORT}
ctx = ssl.create_default_context(cafile='${CA_FILE}')
ctx.set_alpn_protocols(['h2', 'http/1.1'])

with socket.create_connection((host, port), timeout=5) as raw:
    with ctx.wrap_socket(raw, server_hostname=host) as sock:
        negotiated = sock.selected_alpn_protocol()
        if negotiated != 'http/1.1':
            print(f'Expected ALPN http/1.1 on classic listener, got {negotiated!r}')
            sys.exit(1)
        sock.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n')
        resp = b''
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            resp += chunk

status_line = resp.split(b'\r\n', 1)[0].decode('latin1', errors='replace')
if not status_line.startswith('HTTP/1.1 200 '):
    print(f'Expected HTTP/1.1 200 on classic listener, got: {status_line}')
    sys.exit(1)
print('Classic listener ALPN (client offered h2,http/1.1 -> http/1.1, GET -> 200) OK')
PY

# Verify legacy protocols are rejected.
if run_client -tls1 | grep -q 'Verify return code: 0'; then
    echo "TLS 1.0 unexpectedly succeeded"; exit 1
fi
echo "TLS 1.0 rejected OK"

if run_client -tls1_1 | grep -q 'Verify return code: 0'; then
    echo "TLS 1.1 unexpectedly succeeded"; exit 1
fi
echo "TLS 1.1 rejected OK"

# If we get here, all checks passed.
echo "TLS smoke test passed."
