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
HTTPS_CLAUTH_PORT=18444
HTTPS_H2_PORT=18445
HTTPS_REQ_PORT=18446
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
    rm -rf /tmp/tls-smoke-ca-dir
    rm -f /tmp/tls-smoke.p12 /tmp/tls-smoke-key.pem /tmp/tls-smoke-cert.pem /tmp/tls-smoke-ca.pem \
          /tmp/tls-smoke-client-valid-key.pem /tmp/tls-smoke-client-valid-cert.pem \
          /tmp/tls-smoke-client-expired-key.pem /tmp/tls-smoke-client-expired-cert.pem
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

# Create a test CA, server cert, valid client cert, and expired client cert.
CA_DIR="/tmp/tls-smoke-ca-dir"
rm -rf "${CA_DIR}"
mkdir -p "${CA_DIR}/newcerts"
touch "${CA_DIR}/index.txt"
echo "01" > "${CA_DIR}/serial"
cat > "${CA_DIR}/ca.cnf" <<EOF
[ca]
default_ca = CA_default

[CA_default]
dir = ${CA_DIR}
new_certs_dir = ${CA_DIR}/newcerts
database = ${CA_DIR}/index.txt
serial = ${CA_DIR}/serial
private_key = ${CA_DIR}/ca-key.pem
certificate = /tmp/tls-smoke-ca.pem
default_md = sha256
policy = policy_any
unique_subject = no
copy_extensions = copy

[policy_any]
commonName = supplied

[v3_server]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:localhost

[v3_client]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth
EOF

CA_FILE="/tmp/tls-smoke-ca.pem"
VALID_CLIENT_CERT="/tmp/tls-smoke-client-valid-cert.pem"
VALID_CLIENT_KEY="/tmp/tls-smoke-client-valid-key.pem"
EXPIRED_CLIENT_CERT="/tmp/tls-smoke-client-expired-cert.pem"
EXPIRED_CLIENT_KEY="/tmp/tls-smoke-client-expired-key.pem"

openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout "${CA_DIR}/ca-key.pem" -out "${CA_FILE}" \
    -days 365 -subj "/CN=Heliod-TLS-Smoke-CA" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null

openssl req -newkey rsa:2048 -nodes \
    -keyout /tmp/tls-smoke-key.pem -out "${CA_DIR}/server.csr" \
    -subj "/CN=localhost" 2>/dev/null
openssl ca -config "${CA_DIR}/ca.cnf" -extensions v3_server -batch -notext \
    -days 90 -in "${CA_DIR}/server.csr" -out /tmp/tls-smoke-cert.pem 2>/dev/null

openssl req -newkey rsa:2048 -nodes \
    -keyout "${VALID_CLIENT_KEY}" -out "${CA_DIR}/client-valid.csr" \
    -subj "/CN=ValidClient" 2>/dev/null
openssl ca -config "${CA_DIR}/ca.cnf" -extensions v3_client -batch -notext \
    -days 90 -in "${CA_DIR}/client-valid.csr" -out "${VALID_CLIENT_CERT}" 2>/dev/null

openssl req -newkey rsa:2048 -nodes \
    -keyout "${EXPIRED_CLIENT_KEY}" -out "${CA_DIR}/client-expired.csr" \
    -subj "/CN=ExpiredClient" 2>/dev/null
openssl ca -config "${CA_DIR}/ca.cnf" -extensions v3_client -batch -notext \
    -startdate 20200101000000Z -enddate 20200102000000Z \
    -in "${CA_DIR}/client-expired.csr" -out "${EXPIRED_CLIENT_CERT}" 2>/dev/null

cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
rm -f cert8.db key3.db secmod.db cert9.db key4.db pkcs11.txt
certutil -N -d dbm:"." -f "${NO_PASS}" </dev/null 2>/dev/null
certutil -A -d dbm:"." -n "Test-CA" -t "CT,C,C" -a -i "${CA_FILE}" -f "${NO_PASS}" </dev/null 2>/dev/null
openssl pkcs12 -export -out /tmp/tls-smoke.p12 \
    -inkey /tmp/tls-smoke-key.pem -in /tmp/tls-smoke-cert.pem \
    -certfile "${CA_FILE}" -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/tls-smoke.p12 -d dbm:"." -w "${NO_PASS}" </dev/null
echo "NSSDir ${PWD}" >> magnus.conf
cd "${WORK_ROOT}"

# Create protected directories under docs for client-cert directive testing.
mkdir -p "${INSTANCE_DIR}/docs/clauth-init" \
         "${INSTANCE_DIR}/docs/clauth-dorequest" \
         "${INSTANCE_DIR}/docs/clauth-opt-dorequest"
echo "clauth-init-ok" > "${INSTANCE_DIR}/docs/clauth-init/index.html"
echo "clauth-dorequest-ok" > "${INSTANCE_DIR}/docs/clauth-dorequest/index.html"
echo "clauth-opt-dorequest-ok" > "${INSTANCE_DIR}/docs/clauth-opt-dorequest/index.html"

# Append client-cert PathCheck objects to obj.conf if not already present.
if ! grep -q 'clauth-init' "${CONFIG_DIR}/obj.conf"; then
    cat >> "${CONFIG_DIR}/obj.conf" <<'EOF'

<Object ppath="*/clauth-init/*">
PathCheck fn="get-client-cert" dorequest="0" require="1"
</Object>

<Object ppath="*/clauth-dorequest/*">
PathCheck fn="get-client-cert" dorequest="1" require="1"
</Object>

<Object ppath="*/clauth-opt-dorequest/*">
PathCheck fn="get-client-cert" dorequest="1" require="0"
PathCheck fn="get-client-cert" dorequest="0" require="1"
</Object>
EOF
fi

configure_server_xml() {
    local cache_enabled="$1"
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

sc = root.find("ssl-session-cache")
if sc is None:
    sc = ET.SubElement(root, "ssl-session-cache")
sc.clear()
ET.SubElement(sc, "enabled").text = "${cache_enabled}"

vs = root.find("virtual-server")
for el in list(vs.findall("http-listener-name")):
    vs.remove(el)

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
    else:
        root.remove(listener)

def add_https_listener(name, port, io_model, client_auth):
    https = ET.SubElement(root, "http-listener")
    ET.SubElement(https, "name").text = name
    ET.SubElement(https, "port").text = str(port)
    ET.SubElement(https, "server-name").text = "localhost"
    ET.SubElement(https, "default-virtual-server-name").text = "test"
    if io_model:
        ET.SubElement(https, "io-model").text = io_model
    ssl = ET.SubElement(https, "ssl")
    ET.SubElement(ssl, "enabled").text = "true"
    ET.SubElement(ssl, "server-cert-nickname").text = "Server-Cert"
    ET.SubElement(ssl, "ssl3").text = "false"
    ET.SubElement(ssl, "tls").text = "true"
    ET.SubElement(ssl, "client-auth").text = client_auth

add_https_listener("https-listener-1", "${HTTPS_PORT}", None, "false")
add_https_listener("https-listener-clauth", "${HTTPS_CLAUTH_PORT}", None, "optional")
add_https_listener("https-listener-h2", "${HTTPS_H2_PORT}", "event", "optional")
add_https_listener("https-listener-req", "${HTTPS_REQ_PORT}", None, "required")

for lname in ("http-listener-1", "https-listener-1", "https-listener-clauth",
              "https-listener-h2", "https-listener-req"):
    ET.SubElement(vs, "http-listener-name").text = lname

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

start_and_wait() {
    cleanup_server
    rm -f "${INSTANCE_DIR}/logs/errors"
    cd "${INSTANCE_DIR}"
    ulimit -n 4096
    "${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-tls.log 2>&1 &
    SERVER_PID=$!

    local waited=0
    while [ "${waited}" -lt 20 ]; do
        if ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} " && \
           ss -tln 2>/dev/null | grep -q ":${HTTPS_CLAUTH_PORT} " && \
           ss -tln 2>/dev/null | grep -q ":${HTTPS_H2_PORT} " && \
           ss -tln 2>/dev/null | grep -q ":${HTTPS_REQ_PORT} "; then
            break
        fi
        sleep 1
        waited=$((waited + 1))
    done
    if ! ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
        echo "Server did not start listening on HTTPS port ${HTTPS_PORT}"
        cat /tmp/${INSTANCE}-tls.log 2>/dev/null || true
        cat "${INSTANCE_DIR}/logs/errors" 2>/dev/null || true
        exit 1
    fi
    if ! curl -sf "http://localhost:${HTTP_PORT}/" >/dev/null; then
        echo "Server failed to start on HTTP port ${HTTP_PORT}; aborting TLS tests."
        exit 1
    fi
}

check_no_crashes() {
    local label="$1"
    if [ -f "${INSTANCE_DIR}/logs/errors" ]; then
        if grep -E 'SIGSEGV|SEGV|SIGFPE|SIGABRT|SIGBUS|segfault|Child process .* died' "${INSTANCE_DIR}/logs/errors"; then
            echo "FAIL: crash detected in error log during ${label}"
            cat "${INSTANCE_DIR}/logs/errors"
            exit 1
        fi
    fi
    if ! curl -sf "http://localhost:${HTTP_PORT}/" >/dev/null; then
        echo "FAIL: server unresponsive on HTTP port after ${label}"
        exit 1
    fi
}

run_client() {
    openssl s_client -connect "localhost:${HTTPS_PORT}" "$@" -CAfile "${CA_FILE}" </dev/null 2>/dev/null
}

run_tls_and_clauth_suite() {
    local mode_label="$1"
    echo "=== Running TLS & client-auth suite (${mode_label}) ==="

    # Verify TLS 1.2 handshake succeeds.
    run_client -tls1_2 | grep -q 'Verify return code: 0' || { echo "TLS 1.2 failed (${mode_label})"; exit 1; }
    echo "TLS 1.2 OK (${mode_label})"

    # Verify TLS 1.3 handshake succeeds (requires OpenSSL 1.1.1+).
    run_client -tls1_3 | grep -q 'Verify return code: 0' || { echo "TLS 1.3 failed (${mode_label})"; exit 1; }
    echo "TLS 1.3 OK (${mode_label})"

    # Verify ALPN negotiation advertises http/1.1.
    run_client -tls1_2 -alpn http/1.1 | grep -q 'ALPN protocol: http/1.1' || { echo "ALPN failed (${mode_label})"; exit 1; }
    echo "ALPN http/1.1 OK (${mode_label})"

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
print('Classic listener ALPN (client offered h2,http/1.1 -> http/1.1, GET -> 200) OK (${mode_label})')
PY

    # Verify legacy protocols are rejected.
    if run_client -tls1 | grep -q 'Verify return code: 0'; then
        echo "TLS 1.0 unexpectedly succeeded (${mode_label})"; exit 1
    fi
    echo "TLS 1.0 rejected OK (${mode_label})"

    if run_client -tls1_1 | grep -q 'Verify return code: 0'; then
        echo "TLS 1.1 unexpectedly succeeded (${mode_label})"; exit 1
    fi
    echo "TLS 1.1 rejected OK (${mode_label})"

    # Client-certificate authentication matrix:
    # 1. Initial-handshake client-auth (optional listener + get-client-cert on /clauth-init/
    #    and required listener on HTTPS_REQ_PORT) with ssl3=false, tls=true:
    #    - valid cert (TLS 1.2 & TLS 1.3) -> 200
    #    - no cert (TLS 1.2 & TLS 1.3) -> 403 on /clauth-init/ and HTTPS_REQ_PORT, 200 on /
    #    - expired cert (TLS 1.2 & TLS 1.3) -> handshake/read rejected, no server crash
    # 2. On-demand client-auth (dorequest=1 on listener with client-auth=false):
    #    - TLS 1.2 renegotiation: valid cert -> 200, no cert -> 403, expired cert -> rejected
    #      (including /clauth-opt-dorequest/ double get-client-cert regression check)
    #    - TLS 1.3 post-handshake auth (PHA): valid cert -> 200, no cert -> 403,
    #      expired cert -> rejected (including /clauth-opt-dorequest/)
    python3 - <<PY
import socket
import ssl
import sys

CA_FILE = '${CA_FILE}'
VALID_CERT = '${VALID_CLIENT_CERT}'
VALID_KEY = '${VALID_CLIENT_KEY}'
EXPIRED_CERT = '${EXPIRED_CLIENT_CERT}'
EXPIRED_KEY = '${EXPIRED_CLIENT_KEY}'

PORT_DOREQ = ${HTTPS_PORT}
PORT_CLAUTH = ${HTTPS_CLAUTH_PORT}
PORT_H2 = ${HTTPS_H2_PORT}
PORT_REQ = ${HTTPS_REQ_PORT}

def make_ctx(tls_version, cert=None, key=None, enable_pha=False):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.load_verify_locations(cafile=CA_FILE)
    ctx.check_hostname = True
    ctx.minimum_version = tls_version
    ctx.maximum_version = tls_version
    if cert and key:
        ctx.load_cert_chain(certfile=cert, keyfile=key)
    if enable_pha and hasattr(ctx, 'post_handshake_auth'):
        ctx.post_handshake_auth = True
    return ctx

def do_http11_get(port, path, tls_version, cert=None, key=None, enable_pha=False):
    ctx = make_ctx(tls_version, cert=cert, key=key, enable_pha=enable_pha)
    try:
        with socket.create_connection(('localhost', port), timeout=5) as raw:
            with ctx.wrap_socket(raw, server_hostname='localhost') as sock:
                req = f'GET {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'.encode('ascii')
                sock.sendall(req)
                resp = b''
                while True:
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    resp += chunk
    except (ssl.SSLError, OSError) as exc:
        return None, str(exc)
    if not resp:
        return None, 'empty response'
    status_line = resp.split(b'\r\n', 1)[0].decode('latin1', errors='replace')
    parts = status_line.split(' ')
    code = int(parts[1]) if len(parts) >= 2 and parts[1].isdigit() else -1
    return code, status_line

for ver_name, ver in [('TLSv1.2', ssl.TLSVersion.TLSv1_2),
                      ('TLSv1.3', ssl.TLSVersion.TLSv1_3)]:
    # Initial handshake on optional client-auth listener (PORT_CLAUTH) and event listener (PORT_H2)
    for l_name, l_port in [('classic-clauth', PORT_CLAUTH), ('event-clauth', PORT_H2)]:
        code, info = do_http11_get(l_port, '/clauth-init/index.html', ver, VALID_CERT, VALID_KEY)
        if code != 200:
            print(f'FAIL [{ver_name} {l_name}] valid cert on /clauth-init/: expected 200, got {code} ({info})')
            sys.exit(1)

        code, info = do_http11_get(l_port, '/clauth-init/index.html', ver)
        if code != 403:
            print(f'FAIL [{ver_name} {l_name}] no cert on /clauth-init/: expected 403, got {code} ({info})')
            sys.exit(1)

        code, info = do_http11_get(l_port, '/', ver)
        if code != 200:
            print(f'FAIL [{ver_name} {l_name}] no cert on /: expected 200, got {code} ({info})')
            sys.exit(1)

        code, info = do_http11_get(l_port, '/clauth-init/index.html', ver, EXPIRED_CERT, EXPIRED_KEY)
        if code is not None and code < 400:
            print(f'FAIL [{ver_name} {l_name}] expired cert on /clauth-init/ unexpectedly succeeded: {code} ({info})')
            sys.exit(1)

    # Required client-auth listener (PORT_REQ)
    code, info = do_http11_get(PORT_REQ, '/', ver, VALID_CERT, VALID_KEY)
    if code != 200:
        print(f'FAIL [{ver_name} required-clauth] valid cert on /: expected 200, got {code} ({info})')
        sys.exit(1)

    code, info = do_http11_get(PORT_REQ, '/', ver)
    if code != 403:
        print(f'FAIL [{ver_name} required-clauth] no cert on /: expected 403, got {code} ({info})')
        sys.exit(1)

    code, info = do_http11_get(PORT_REQ, '/', ver, EXPIRED_CERT, EXPIRED_KEY)
    if code is not None and code < 400:
        print(f'FAIL [{ver_name} required-clauth] expired cert on / unexpectedly succeeded: {code} ({info})')
        sys.exit(1)

    # On-demand (dorequest=1) client-auth on listener with client-auth=false (PORT_DOREQ)
    pha = (ver == ssl.TLSVersion.TLSv1_3)
    code, info = do_http11_get(PORT_DOREQ, '/clauth-dorequest/index.html', ver,
                               VALID_CERT, VALID_KEY, enable_pha=pha)
    if code != 200:
        print(f'FAIL [{ver_name} dorequest=1] valid cert: expected 200, got {code} ({info})')
        sys.exit(1)

    code, info = do_http11_get(PORT_DOREQ, '/clauth-dorequest/index.html', ver,
                               enable_pha=pha)
    if code != 403:
        print(f'FAIL [{ver_name} dorequest=1] no cert: expected 403, got {code} ({info})')
        sys.exit(1)

    code, info = do_http11_get(PORT_DOREQ, '/clauth-dorequest/index.html', ver,
                               EXPIRED_CERT, EXPIRED_KEY, enable_pha=pha)
    if code is not None and code < 400:
        print(f'FAIL [{ver_name} dorequest=1] expired cert unexpectedly succeeded: {code} ({info})')
        sys.exit(1)

    code, info = do_http11_get(PORT_DOREQ, '/clauth-opt-dorequest/index.html', ver,
                               EXPIRED_CERT, EXPIRED_KEY, enable_pha=pha)
    if code is not None and code < 400:
        print(f'FAIL [{ver_name} opt-dorequest] expired cert accepted by second get-client-cert: {code} ({info})')
        sys.exit(1)

print('Client-certificate auth matrix (TLS 1.2/1.3, initial & dorequest=1, valid/none/expired) OK (${mode_label})')
PY

    # Verify HTTP/2 (h2) client-certificate propagation on the event listener.
    if curl --version 2>/dev/null | grep -q 'HTTP2'; then
        h2_valid_code=$(curl -s -o /dev/null -w '%{http_code}' --http2 \
            --cacert "${CA_FILE}" --cert "${VALID_CLIENT_CERT}" --key "${VALID_CLIENT_KEY}" \
            "https://localhost:${HTTPS_H2_PORT}/clauth-init/index.html")
        if [ "${h2_valid_code}" != "200" ]; then
            echo "FAIL: HTTP/2 valid client cert on /clauth-init/ returned ${h2_valid_code} (${mode_label})"
            exit 1
        fi
        h2_nocert_code=$(curl -s -o /dev/null -w '%{http_code}' --http2 \
            --cacert "${CA_FILE}" \
            "https://localhost:${HTTPS_H2_PORT}/clauth-init/index.html")
        if [ "${h2_nocert_code}" != "403" ]; then
            echo "FAIL: HTTP/2 missing client cert on /clauth-init/ returned ${h2_nocert_code} (${mode_label})"
            exit 1
        fi
        if curl -sf --http2 --cacert "${CA_FILE}" \
            --cert "${EXPIRED_CLIENT_CERT}" --key "${EXPIRED_CLIENT_KEY}" \
            "https://localhost:${HTTPS_H2_PORT}/clauth-init/index.html" >/dev/null 2>&1; then
            echo "FAIL: HTTP/2 expired client cert unexpectedly succeeded (${mode_label})"
            exit 1
        fi
        echo "HTTP/2 client-certificate auth (valid->200, missing->403, expired->rejected) OK (${mode_label})"
    fi

    check_no_crashes "${mode_label}"
}

# Pass 1: SSL session cache enabled.
configure_server_xml "true"
start_and_wait
echo "HTTP listener OK (session-cache=true)"
run_tls_and_clauth_suite "session-cache=true"

# Pass 2: SSL session cache disabled.
configure_server_xml "false"
start_and_wait
echo "HTTP listener OK (session-cache=false)"
run_tls_and_clauth_suite "session-cache=false"

# If we get here, all checks passed.
echo "TLS smoke test passed."
