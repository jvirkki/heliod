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
#   6. Outbound (origin) TLS channel hardening: reverse proxy to TLS 1.2-only
#      and TLS 1.3-only origins, TLS client certificate authentication via
#      ssl-client-config client-cert-nickname, and verification that the
#      proxy ClientHello uses standard TLS record encoding (no SSLv2-compatible
#      ClientHello) with SNI.
#   7. Origin health detection and configurable offline-check-interval:
#      intermittently delayed responses exceeding http-client-config timeout
#      return 504 without marking a healthy origin offline, while a genuinely
#      unreachable origin is marked offline, fails over to a healthy peer, and
#      is automatically recovered within the configured <offline-check-interval>.
#   8. Proxy error logging and error page generation on origin unreachable
#      (connection refused), DNS lookup failure, and connection reset / EOF
#      mid-headers without crashing webservd.
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
TLS12_ORIGIN_PORT=18283
TLS13_ORIGIN_PORT=18284
MTLS_ORIGIN_PORT=18285
HELLO_INSPECT_PORT=18286
DEAD_ORIGIN_PORT=18289
TEMP_DIR=""
SERVER_PID=""
ORIGIN_A_PID=""
ORIGIN_B_PID=""
TLS12_ORIGIN_PID=""
TLS13_ORIGIN_PID=""
MTLS_ORIGIN_PID=""
HELLO_INSPECT_PID=""

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
    for pid_var in ORIGIN_A_PID ORIGIN_B_PID TLS12_ORIGIN_PID TLS13_ORIGIN_PID MTLS_ORIGIN_PID HELLO_INSPECT_PID; do
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
        if self.path.startswith("/app/slow"):
            import time
            time.sleep(2.2)
            body = f"origin={origin_id} slow_ok\n".encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except Exception:
                pass
            return

        if self.path.startswith("/app/drop-hdr"):
            self.close_connection = True
            return

        if self.path.startswith("/app/partial-hdr"):
            try:
                self.wfile.write(b"HTTP/1.1 200 OK\r\nX-Partial: ")
                self.wfile.flush()
            except Exception:
                pass
            self.close_connection = True
            return

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

# Generate CA, origin server certificate, and proxy client certificate in NSS DB
NO_PASS="${TEMP_DIR}/no_pass.txt"
printf '\n' > "${NO_PASS}"
CA_KEY="${TEMP_DIR}/ca-key.pem"
CA_CERT="${TEMP_DIR}/ca-cert.pem"
ORIG_KEY="${TEMP_DIR}/origin-key.pem"
ORIG_CSR="${TEMP_DIR}/origin.csr"
ORIG_CERT="${TEMP_DIR}/origin-cert.pem"
ORIG_EXT="${TEMP_DIR}/origin-ext.cnf"
CLI_KEY="${TEMP_DIR}/client-key.pem"
CLI_CSR="${TEMP_DIR}/client.csr"
CLI_CERT="${TEMP_DIR}/client-cert.pem"
CLI_EXT="${TEMP_DIR}/client-ext.cnf"
CLI_P12="${TEMP_DIR}/client.p12"

openssl req -x509 -newkey rsa:2048 -keyout "${CA_KEY}" -out "${CA_CERT}" \
    -days 30 -nodes -subj "/CN=ProxyTestCA" \
    -addext "basicConstraints=critical,CA:TRUE" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null

cat > "${ORIG_EXT}" <<'EOF'
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost,IP:127.0.0.1
EOF
openssl req -newkey rsa:2048 -keyout "${ORIG_KEY}" -out "${ORIG_CSR}" \
    -nodes -subj "/CN=localhost" 2>/dev/null
openssl x509 -req -in "${ORIG_CSR}" -CA "${CA_CERT}" -CAkey "${CA_KEY}" \
    -CAcreateserial -out "${ORIG_CERT}" -days 30 -extfile "${ORIG_EXT}" 2>/dev/null

cat > "${CLI_EXT}" <<'EOF'
basicConstraints=CA:FALSE
keyUsage=digitalSignature,keyEncipherment
extendedKeyUsage=clientAuth
EOF
openssl req -newkey rsa:2048 -keyout "${CLI_KEY}" -out "${CLI_CSR}" \
    -nodes -subj "/CN=ProxyClientCert" 2>/dev/null
openssl x509 -req -in "${CLI_CSR}" -CA "${CA_CERT}" -CAkey "${CA_KEY}" \
    -CAcreateserial -out "${CLI_CERT}" -days 30 -extfile "${CLI_EXT}" 2>/dev/null
openssl pkcs12 -export -out "${CLI_P12}" -inkey "${CLI_KEY}" -in "${CLI_CERT}" \
    -certfile "${CA_CERT}" -name "Proxy-Client-Cert" -passout pass: 2>/dev/null

rm -f "${CONFIG_DIR}/cert8.db" "${CONFIG_DIR}/key3.db" "${CONFIG_DIR}/secmod.db" \
      "${CONFIG_DIR}/cert9.db" "${CONFIG_DIR}/key4.db" "${CONFIG_DIR}/pkcs11.txt"
certutil -N -d "dbm:${CONFIG_DIR}" -f "${NO_PASS}" </dev/null 2>/dev/null
certutil -A -d "dbm:${CONFIG_DIR}" -n "ProxyTestCA" -t "CT,C,C" -a -i "${CA_CERT}" </dev/null 2>/dev/null
pk12util -i "${CLI_P12}" -d "dbm:${CONFIG_DIR}" -w "${NO_PASS}" </dev/null >/dev/null 2>&1
if ! grep -q '^NSSDir ' "${CONFIG_DIR}/magnus.conf"; then
    echo "NSSDir ${CONFIG_DIR}" >> "${CONFIG_DIR}/magnus.conf"
fi

# Start TLS 1.2-only, TLS 1.3-only, and mTLS (TLS 1.2 + client cert required) origins
start_tls_origin() {
    local port="$1"
    local origin_id="$2"
    local tls_mode="$3"
    local require_client_cert="$4"
    local ready_file="${TEMP_DIR}/origin-${origin_id}.ready"
    python3 - "${port}" "${origin_id}" "${tls_mode}" "${require_client_cert}" "${ORIG_CERT}" "${ORIG_KEY}" "${CA_CERT}" "${ready_file}" <<'PY' &
import http.server
import socketserver
import ssl
import sys

port = int(sys.argv[1])
origin_id = sys.argv[2]
tls_mode = sys.argv[3]
require_client_cert = sys.argv[4] == "true"
cert_file = sys.argv[5]
key_file = sys.argv[6]
ca_file = sys.argv[7]
ready_file = sys.argv[8]

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(certfile=cert_file, keyfile=key_file)
if tls_mode == "tls12":
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
elif tls_mode == "tls13":
    ctx.minimum_version = ssl.TLSVersion.TLSv1_3
    ctx.maximum_version = ssl.TLSVersion.TLSv1_3
else:
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_3

if require_client_cert:
    ctx.verify_mode = ssl.CERT_REQUIRED
    ctx.load_verify_locations(cafile=ca_file)

class ReuseTLSServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

    def get_request(self):
        newsock, addr = super().get_request()
        ssock = ctx.wrap_socket(newsock, server_side=True)
        return ssock, addr

class TLSOriginHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def do_OPTIONS(self):
        body = f"origin={origin_id}\n".encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        tls_ver = self.connection.version()
        peercert = self.connection.getpeercert()
        client_cn = "none"
        if peercert:
            for rdn in peercert.get("subject", ()):
                for k, v in rdn:
                    if k == "commonName":
                        client_cn = v
        body = f"origin={origin_id} tls={tls_ver} client_cn={client_cn} path={self.path}\n".encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

httpd = ReuseTLSServer(("127.0.0.1", port), TLSOriginHandler)
open(ready_file, "w").write("ready\n")
httpd.serve_forever()
PY
}

start_tls_origin "${TLS12_ORIGIN_PORT}" "tls12-origin" "tls12" "false"
TLS12_ORIGIN_PID=$!
start_tls_origin "${TLS13_ORIGIN_PORT}" "tls13-origin" "tls13" "false"
TLS13_ORIGIN_PID=$!
start_tls_origin "${MTLS_ORIGIN_PORT}" "mtls-origin" "tls12" "true"
MTLS_ORIGIN_PID=$!

# Start a raw TCP listener that inspects the first ClientHello bytes sent by
# the proxy before handing the socket off to TLS
HELLO_RECORD_FILE="${TEMP_DIR}/client_hello_info.txt"
HELLO_READY_FILE="${TEMP_DIR}/hello-inspect.ready"
python3 - "${HELLO_INSPECT_PORT}" "${ORIG_CERT}" "${ORIG_KEY}" "${HELLO_RECORD_FILE}" "${HELLO_READY_FILE}" <<'PY' &
import socket
import ssl
import sys

port = int(sys.argv[1])
cert_file = sys.argv[2]
key_file = sys.argv[3]
out_file = sys.argv[4]
ready_file = sys.argv[5]

ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.load_cert_chain(certfile=cert_file, keyfile=key_file)

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(5)
open(ready_file, "w").write("ready\n")

while True:
    conn, _ = srv.accept()
    try:
        peek = conn.recv(512, socket.MSG_PEEK)
        if peek:
            first_byte = peek[0]
            rec_major = peek[1] if len(peek) > 1 else 0
            rec_minor = peek[2] if len(peek) > 2 else 0
            is_v2_hello = (first_byte & 0x80) != 0
            has_sni = b"localhost" in peek
            with open(out_file, "w") as f:
                f.write(f"first_byte=0x{first_byte:02x} version=0x{rec_major:02x}{rec_minor:02x} v2_compat={is_v2_hello} has_sni={has_sni}\n")
        ssock = ctx.wrap_socket(conn, server_side=True)
        req = ssock.recv(4096)
        body = b"origin=hello-inspect\n"
        resp = (
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Type: text/plain\r\n"
            b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n"
            b"Connection: close\r\n\r\n" + body
        )
        ssock.sendall(resp)
        ssock.close()
    except Exception:
        try:
            conn.close()
        except Exception:
            pass
PY
HELLO_INSPECT_PID=$!

for rf in "${TEMP_DIR}/origin-tls12-origin.ready" "${TEMP_DIR}/origin-tls13-origin.ready" "${TEMP_DIR}/origin-mtls-origin.ready" "${HELLO_READY_FILE}"; do
    w=0
    while [ ! -f "${rf}" ] && [ "${w}" -lt 50 ]; do
        sleep 0.1
        w=$((w + 1))
    done
    if [ ! -f "${rf}" ]; then
        echo "FAIL: TLS origin server did not start (${rf})"
        exit 1
    fi
done

# Configure server.xml with classic and event listeners, pkcs11 enabled, and offline-check-interval=2
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

pkcs11 = root.find("pkcs11")
if pkcs11 is None:
    pkcs11 = ET.SubElement(root, "pkcs11")
p_en = pkcs11.find("enabled")
if p_en is None:
    p_en = ET.SubElement(pkcs11, "enabled")
p_en.text = "true"

oci = root.find("offline-check-interval")
if oci is None:
    oci = ET.SubElement(root, "offline-check-interval")
oci.text = "2"

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
    python3 - "${CONFIG_DIR}/obj.conf.pristine_base" "${CONFIG_DIR}/obj.conf" \
        "${ORIGIN_A_PORT}" "${ORIGIN_B_PORT}" "${CLASSIC_PORT}" \
        "${TLS12_ORIGIN_PORT}" "${TLS13_ORIGIN_PORT}" "${MTLS_ORIGIN_PORT}" \
        "${HELLO_INSPECT_PORT}" "${DEAD_ORIGIN_PORT}" "${mode}" <<'PY'
import sys

(
    base_path,
    out_path,
    port_a,
    port_b,
    proxy_port,
    tls12_port,
    tls13_port,
    mtls_port,
    hello_port,
    dead_port,
    mode,
) = sys.argv[1:12]
lines = open(base_path).read().splitlines(True)

out = []
inserted_nametrans = False
for line in lines:
    if not inserted_nametrans and line.startswith("NameTrans"):
        out.append(f'NameTrans fn="reverse-map" from="http://127.0.0.1:{port_a}/app" to="http://localhost:{proxy_port}/app" rewrite-location="true" rewrite-content-location="true" rewrite-set-cookie="true"\n')
        out.append('NameTrans fn="map" from="/app" name="reverse-proxy-app" to="http:/app"\n')
        out.append('NameTrans fn="map" from="/rootproxy/" name="reverse-proxy-app" to="http:/app"\n')
        out.append('NameTrans fn="map" from="/tls12" name="reverse-proxy-tls12" to="https:/tls12"\n')
        out.append('NameTrans fn="map" from="/tls13" name="reverse-proxy-tls13" to="https:/tls13"\n')
        out.append('NameTrans fn="map" from="/mtls" name="reverse-proxy-mtls" to="https:/mtls"\n')
        out.append('NameTrans fn="map" from="/hello-inspect" name="reverse-proxy-hello" to="https:/hello-inspect"\n')
        out.append('NameTrans fn="map" from="/dead" name="reverse-proxy-dead" to="http:/dead"\n')
        out.append('NameTrans fn="map" from="/bad-dns" name="reverse-proxy-baddns" to="http:/bad-dns"\n')
        out.append('NameTrans fn="assign-name" from="/gateway-dump" name="gateway-dump-obj"\n')
        inserted_nametrans = True
    out.append(line)

extra_obj_lines = []
client_timeout = "10"
connect_timeout = "5"
retries = "1"

if mode == "single_origin":
    route_line = f'Route fn="set-origin-server" server="http://127.0.0.1:{port_a}" rewrite-host="true"\n'
elif mode == "single_origin_fast_timeout":
    client_timeout = "1"
    connect_timeout = "2"
    retries = "1"
    route_line = f'Route fn="set-origin-server" server="http://127.0.0.1:{port_a}" rewrite-host="true"\n'
elif mode == "multi_origin_default":
    retries = "2"
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
out.append(f'ObjectType fn="http-client-config" keep-alive="true" timeout="{client_timeout}" connect-timeout="{connect_timeout}" retries="{retries}" tunnel-non-http-response="false"\n')
for l in extra_obj_lines:
    out.append(l)
out.append(route_line)
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-tls12">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="5" connect-timeout="5" retries="0"\n')
out.append('ObjectType fn="ssl-client-config" validate-server-cert="true"\n')
out.append(f'Route fn="set-origin-server" server="https://localhost:{tls12_port}"\n')
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-tls13">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="5" connect-timeout="5" retries="0"\n')
out.append('ObjectType fn="ssl-client-config" validate-server-cert="true"\n')
out.append(f'Route fn="set-origin-server" server="https://localhost:{tls13_port}"\n')
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-mtls">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="5" connect-timeout="5" retries="0"\n')
out.append('ObjectType fn="ssl-client-config" client-cert-nickname="Proxy-Client-Cert" validate-server-cert="true"\n')
out.append(f'Route fn="set-origin-server" server="https://localhost:{mtls_port}"\n')
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-hello">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="5" connect-timeout="5" retries="0"\n')
out.append('ObjectType fn="ssl-client-config" validate-server-cert="false"\n')
out.append(f'Route fn="set-origin-server" server="https://localhost:{hello_port}"\n')
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-dead">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="3" connect-timeout="1" retries="0"\n')
out.append(f'Route fn="set-origin-server" server="http://127.0.0.1:{dead_port}"\n')
out.append('Error fn="magnus-internal/send-proxy-error"\n')
out.append('</Object>\n')

out.append('\n<Object name="reverse-proxy-baddns">\n')
out.append('ObjectType fn="http-client-config" keep-alive="false" timeout="3" connect-timeout="1" retries="0"\n')
out.append('Route fn="set-origin-server" server="http://nonexistent.invalid:18299"\n')
out.append('Error fn="magnus-internal/send-proxy-error"\n')
out.append('</Object>\n')

out.append('\n<Object name="gateway-dump-obj">\n')
out.append('ObjectType fn="force-type" type="magnus-internal/gateway-dump"\n')
out.append('Service type="magnus-internal/gateway-dump" fn="service-gateway-dump"\n')
out.append('</Object>\n')

out.append('\n<Object ppath="http*:*">\n')
out.append('Service fn="proxy-retrieve" method="*"\n')
out.append('Error fn="magnus-internal/send-proxy-error"\n')
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

# ---------------------------------------------------------------------------
# Phase 5: Outbound TLS channel hardening (TLS 1.2, TLS 1.3, mTLS, ClientHello)
# ---------------------------------------------------------------------------
echo "  Phase 5: Outbound TLS channel hardening (TLS 1.2, TLS 1.3, client-cert auth, ClientHello format)"

tls12_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/tls12/test")
if ! echo "${tls12_resp}" | grep -q "origin=tls12-origin tls=TLSv1.2"; then
    echo "FAIL: TLS 1.2 origin proxy failed: ${tls12_resp}"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

tls13_resp=$(curl -s --max-time 5 "http://localhost:${EVENT_PORT}/tls13/test")
if ! echo "${tls13_resp}" | grep -q "origin=tls13-origin tls=TLSv1.3"; then
    echo "FAIL: TLS 1.3 origin proxy failed: ${tls13_resp}"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

mtls_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/mtls/test")
if ! echo "${mtls_resp}" | grep -q "origin=mtls-origin tls=TLSv1.2 client_cn=ProxyClientCert"; then
    echo "FAIL: TLS 1.2 client-certificate origin proxy failed: ${mtls_resp}"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

hello_resp=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/hello-inspect/check")
if ! echo "${hello_resp}" | grep -q "origin=hello-inspect" || [ ! -f "${HELLO_RECORD_FILE}" ]; then
    echo "FAIL: ClientHello inspection origin request failed: ${hello_resp}"
    exit 1
fi
hello_info=$(cat "${HELLO_RECORD_FILE}")
if ! echo "${hello_info}" | grep -q "first_byte=0x16" || \
   ! echo "${hello_info}" | grep -q "v2_compat=False" || \
   ! echo "${hello_info}" | grep -q "has_sni=True"; then
    echo "FAIL: Outbound ClientHello did not use standard TLS record encoding with SNI: ${hello_info}"
    exit 1
fi
echo "  PASS: Outbound TLS 1.2, TLS 1.3, client-cert auth (CN=ProxyClientCert), and standard TLS ClientHello (${hello_info}) verified"

# ---------------------------------------------------------------------------
# Phase 6: Proxy error logging & error page generation (unreachable, DNS, EOF/reset)
# ---------------------------------------------------------------------------
echo "  Phase 6: Proxy error logging and HTML error page generation without crashes"

trigger_reconfig "single_origin_fast_timeout"

# 6a. Unreachable origin (connection refused -> 504 + proxy error HTML + CORE7740)
dead_code=$(curl -s -o "${TEMP_DIR}/dead_body.html" -w "%{http_code}" --max-time 10 "http://localhost:${CLASSIC_PORT}/dead/test")
if [ "${dead_code}" != "504" ] || ! grep -q "The Proxy Was Unable to Fulfill Your Request" "${TEMP_DIR}/dead_body.html"; then
    echo "FAIL: unreachable origin expected 504 with proxy error HTML, got code=${dead_code}"
    cat "${TEMP_DIR}/dead_body.html" 2>/dev/null || true
    exit 1
fi

# 6b. DNS lookup failure (-> 504 + proxy error HTML + CORE7740)
dns_code=$(curl -s -o "${TEMP_DIR}/dns_body.html" -w "%{http_code}" --max-time 10 "http://localhost:${CLASSIC_PORT}/bad-dns/test")
if [ "${dns_code}" != "504" ] || ! grep -q "The Proxy Was Unable to Fulfill Your Request" "${TEMP_DIR}/dns_body.html"; then
    echo "FAIL: DNS failure expected 504 with proxy error HTML, got code=${dns_code}"
    cat "${TEMP_DIR}/dns_body.html" 2>/dev/null || true
    exit 1
fi

# 6c. Connection closed/reset before or mid-response headers (-> 502 + proxy error HTML + HTTP7765/HTTP7755)
drop_code=$(curl -s -o "${TEMP_DIR}/drop_body.html" -w "%{http_code}" --max-time 10 "http://localhost:${CLASSIC_PORT}/app/drop-hdr")
part_code=$(curl -s -o "${TEMP_DIR}/part_body.html" -w "%{http_code}" --max-time 10 "http://localhost:${CLASSIC_PORT}/app/partial-hdr")
if [ "${drop_code}" != "502" ] || [ "${part_code}" != "502" ] || \
   ! grep -q "The Proxy Was Unable to Fulfill Your Request" "${TEMP_DIR}/drop_body.html" || \
   ! grep -q "The Proxy Was Unable to Fulfill Your Request" "${TEMP_DIR}/part_body.html"; then
    echo "FAIL: mid-header close/reset expected 502 with proxy error HTML, got drop=${drop_code} part=${part_code}"
    echo "--- drop_body.html ---"
    cat "${TEMP_DIR}/drop_body.html" 2>/dev/null || true
    echo "--- part_body.html ---"
    cat "${TEMP_DIR}/part_body.html" 2>/dev/null || true
    echo "--- error log ---"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

if ! grep -q "CORE7740" "${ERRLOG}" || ! grep -E -q "HTTP7765|HTTP7755" "${ERRLOG}"; then
    echo "FAIL: expected CORE7740 and HTTP7765/HTTP7755 proxy error log entries in error log"
    cat "${ERRLOG}"
    exit 1
fi
echo "  PASS: Unreachable origin, DNS failure, and mid-header EOF/reset produce proxy error pages and log entries without crashing"

# ---------------------------------------------------------------------------
# Phase 7: Origin health detection & configurable offline-check-interval recovery
# ---------------------------------------------------------------------------
echo "  Phase 7: False-offline prevention on slow responses and offline origin recovery within <offline-check-interval>"

# 7a. Intermittently slow response exceeding http-client-config timeout="1" returns 504
# but must NOT mark the healthy origin offline.
slow_code=$(curl -s -o "${TEMP_DIR}/slow_body.html" -w "%{http_code}" --max-time 10 "http://localhost:${CLASSIC_PORT}/app/slow")
if [ "${slow_code}" != "504" ]; then
    echo "FAIL: expected 504 Gateway Timeout on /app/slow with timeout=1s, got ${slow_code}"
    exit 1
fi

gw_dump_after_slow=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/gateway-dump")
if echo "${gw_dump_after_slow}" | grep "127.0.0.1:${ORIGIN_A_PORT}" | grep -q "offline = true"; then
    echo "FAIL: healthy origin-a was falsely marked offline after a slow request timeout:"
    echo "${gw_dump_after_slow}"
    exit 1
fi

fast_after_slow=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/app/fast-after-slow")
if ! echo "${fast_after_slow}" | grep -q "origin=origin-a"; then
    echo "FAIL: healthy origin-a did not serve immediate subsequent request: ${fast_after_slow}"
    exit 1
fi
echo "  PASS: slow response timeout (504) does not mark healthy origin offline"

# 7b. Switch to multi-origin, stop origin-a, verify it is marked offline and fails over to origin-b,
# then restart origin-a and verify it recovers within the configured <offline-check-interval> (2s).
trigger_reconfig "multi_origin_default"

kill "${ORIGIN_A_PID}" 2>/dev/null || true
wait "${ORIGIN_A_PID}" 2>/dev/null || true
ORIGIN_A_PID=""

for i in $(seq 1 6); do
    fo_resp=$(curl -s --max-time 10 "http://localhost:${CLASSIC_PORT}/app/failover?i=${i}")
    if ! echo "${fo_resp}" | grep -q "origin=origin-b"; then
        echo "FAIL: request ${i} did not fail over to origin-b when origin-a was down: ${fo_resp}"
        exit 1
    fi
done

gw_dump_down=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/gateway-dump")
if ! echo "${gw_dump_down}" | grep "127.0.0.1:${ORIGIN_A_PORT}" | grep -q "offline = true"; then
    echo "FAIL: down origin-a was not marked offline in gateway-dump:"
    echo "${gw_dump_down}"
    exit 1
fi

# Restart origin-a and wait up to 5s (configured offline-check-interval is 2s, vs old 30s hardcoded)
rm -f "${TEMP_DIR}/origin-origin-a.ready"
start_origin "${ORIGIN_A_PORT}" "origin-a"
ORIGIN_A_PID=$!
w=0
while [ ! -f "${TEMP_DIR}/origin-origin-a.ready" ] && [ "${w}" -lt 50 ]; do
    sleep 0.1
    w=$((w + 1))
done

recovered="false"
for w in $(seq 1 30); do
    gw_now=$(curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/gateway-dump")
    if echo "${gw_now}" | grep "127.0.0.1:${ORIGIN_A_PORT}" | grep -q "offline = false"; then
        recovered="true"
        break
    fi
    sleep 0.2
done

if [ "${recovered}" != "true" ]; then
    echo "FAIL: origin-a was not recovered within configured <offline-check-interval> (2s):"
    curl -s --max-time 5 "http://localhost:${CLASSIC_PORT}/gateway-dump" || true
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi
echo "  PASS: offline origin marked offline, failed over to peer, and recovered within configured 2s <offline-check-interval>"

PIDS_END=$(get_worker_pid)
if [ "${INITIAL_PIDS}" != "${PIDS_END}" ]; then
    echo "FAIL: webservd worker PIDs changed during test (initial: ${INITIAL_PIDS}, end: ${PIDS_END})"
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

if grep -E 'SIGSEGV|SEGV|segfault|AddressSanitizer|LeakSanitizer|catastrophe' "${ERRLOG}" >/dev/null 2>&1; then
    echo "FAIL: crash or sanitizer error detected in error log:"
    cat "${ERRLOG}"
    exit 1
fi

echo "Reverse proxy smoke test PASSED"
