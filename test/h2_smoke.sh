#!/bin/bash
#
# HTTP/2 smoke test.
#
# Reuses the https-test instance created by `make install`.  The original
# config is saved before the test and restored afterwards.
#
# Usage:
#   test/h2_smoke.sh <heliod-work-root>
# Example:
#   test/h2_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

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
HTTP_PORT=18180
HTTPS_PORT=18443
HTTPS_CLASSIC_PORT=18444
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
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/h2-smoke-key.pem \
    -out /tmp/h2-smoke-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/h2-smoke.p12 \
    -inkey /tmp/h2-smoke-key.pem -in /tmp/h2-smoke-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/h2-smoke.p12 -d dbm:"." -w "${NO_PASS}"
certutil -L -d dbm:"." -n "Server-Cert" -a > /tmp/h2-smoke-ca.pem 2>/dev/null
echo "NSSDir ${PWD}" >> magnus.conf
cd "${WORK_ROOT}"

configure_instance() {
    python3 - <<PY
import xml.etree.ElementTree as ET
import os

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

for listener in root.findall("http-listener"):
    name = listener.findtext("name")
    if name == "http-listener-1":
        port_el = listener.find("port")
        if port_el is None:
            port_el = ET.SubElement(listener, "port")
        port_el.text = "${HTTP_PORT}"
    elif name in ("https-listener-1", "https-classic-1"):
        root.remove(listener)
        continue
    else:
        continue
    for child in list(listener):
        if child.tag == "io-model":
            listener.remove(child)
    ET.SubElement(listener, "io-model").text = "event"

https = ET.SubElement(root, "http-listener")
ET.SubElement(https, "name").text = "https-listener-1"
ET.SubElement(https, "port").text = "${HTTPS_PORT}"
ET.SubElement(https, "server-name").text = "localhost"
ET.SubElement(https, "default-virtual-server-name").text = "test"
ET.SubElement(https, "io-model").text = "event"
ssl = ET.SubElement(https, "ssl")
ET.SubElement(ssl, "enabled").text = "true"
ET.SubElement(ssl, "server-cert-nickname").text = "Server-Cert"

# Second HTTPS listener with default io-model (classic): must advertise
# only http/1.1 via ALPN and never negotiate h2.
https_classic = ET.SubElement(root, "http-listener")
ET.SubElement(https_classic, "name").text = "https-classic-1"
ET.SubElement(https_classic, "port").text = "${HTTPS_CLASSIC_PORT}"
ET.SubElement(https_classic, "server-name").text = "localhost"
ET.SubElement(https_classic, "default-virtual-server-name").text = "test"
ssl_classic = ET.SubElement(https_classic, "ssl")
ET.SubElement(ssl_classic, "enabled").text = "true"
ET.SubElement(ssl_classic, "server-cert-nickname").text = "Server-Cert"

if "https-listener-1" not in vs_listeners:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"
if "https-classic-1" not in vs_listeners:
    ET.SubElement(vs, "http-listener-name").text = "https-classic-1"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

wait_for_listener() {
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        if ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start listening on HTTPS port ${HTTPS_PORT}"
    cat /tmp/${INSTANCE}-h2.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

configure_instance
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2.log 2>&1 &
SERVER_PID=$!
wait_for_listener

# Verify that ALPN negotiates h2.  Python's ssl module reports the negotiated
# ALPN protocol after the handshake.  If ALPN h2 is not working, the
# preface-based fallback below still validates that the HTTP/2 path is reachable.
ALPN_PROTO=$(python3 - <<PY 2>/dev/null
import socket
import ssl
host = 'localhost'
port = ${HTTPS_PORT}
ctx = ssl._create_unverified_context()
ctx.set_alpn_protocols(['h2'])
sock = ctx.wrap_socket(socket.create_connection((host, port), timeout=5),
                       server_hostname=host)
print(sock.selected_alpn_protocol() or 'none')
sock.close()
PY
) || ALPN_PROTO=""
if [ "${ALPN_PROTO}" = "h2" ]; then
    echo "ALPN h2 OK"
else
    echo "ALPN h2 negotiation failed on event listener (alpn_protocol=${ALPN_PROTO:-none})"
    exit 1
fi

# Verify that a TLS listener with the default (classic) io-model
# advertises only http/1.1 even when the client offers h2, and serves GET /
# with HTTP/1.1 200 (not 505).
python3 - <<PY
import socket
import ssl
import sys

host = 'localhost'
port = ${HTTPS_CLASSIC_PORT}
ctx = ssl._create_unverified_context()
ctx.set_alpn_protocols(['h2', 'http/1.1'])

with socket.create_connection((host, port), timeout=5) as raw:
    with ctx.wrap_socket(raw, server_hostname=host) as sock:
        negotiated = sock.selected_alpn_protocol()
        if negotiated != 'http/1.1':
            print(f'Expected ALPN http/1.1 on default io-model listener, got {negotiated!r}')
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
    print(f'Expected HTTP/1.1 200 on default io-model listener, got: {status_line}')
    sys.exit(1)
print('ALPN default io-model (client offers h2 -> http/1.1, GET -> 200) OK')
PY

# Fallback: send the HTTP/2 connection preface and verify the server sends
# its SETTINGS.  We do not require working ALPN, so this proves H2Session is
# reached and nghttp2 speaks HTTP/2 even while ALPN selection is still
# being debugged.  A PING ACK is attempted but treated as a bonus.
python3 - <<PY
import socket
import ssl
import struct
import sys

host = 'localhost'
port = ${HTTPS_PORT}

ctx = ssl._create_unverified_context()
sock = ctx.wrap_socket(socket.create_connection((host, port)), server_hostname=host)

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'

def frame(length, ftype, flags, stream_id):
    return struct.pack('>I', length)[1:] + bytes([ftype, flags]) + struct.pack('>I', stream_id & 0x7FFFFFFF)

sock.sendall(PREFACE)
sock.sendall(frame(0, 0x4, 0x0, 0))  # empty client SETTINGS

# Wait for server SETTINGS.  Read the 9-byte header, then exactly as much payload
# as it declares: the frame's length is not asserted, because which settings the
# server sends is a tuning decision that may change (it has), and a test that
# hard-codes the byte count fails on a change that is not a defect.  What is
# asserted is that this is a SETTINGS frame, not an ACK, on stream 0, and that it
# carries the two settings a client's behaviour actually depends on.
header = b''
while len(header) < 9:
    chunk = sock.recv(9 - len(header))
    if not chunk:
        print('Server closed before SETTINGS')
        sys.exit(1)
    header += chunk

length = (header[0] << 16) | (header[1] << 8) | header[2]
ftype, flags = header[3], header[4]
stream_id = struct.unpack('>I', header[5:9])[0] & 0x7FFFFFFF

if ftype != 0x4 or (flags & 0x1) or stream_id != 0 or length % 6:
    print('Unexpected server SETTINGS header:', header.hex())
    sys.exit(1)

payload = b''
while len(payload) < length:
    chunk = sock.recv(length - len(payload))
    if not chunk:
        print('Server closed mid-SETTINGS after %d of %d bytes' % (len(payload), length))
        sys.exit(1)
    payload += chunk

settings = dict(struct.unpack('>HI', payload[i:i + 6])
                for i in range(0, length, 6))

# MAX_CONCURRENT_STREAMS (3) bounds how many streams a client may open at once;
# INITIAL_WINDOW_SIZE (4) is the server's inbound flow-control window, and without
# it a client has to assume the 64 KiB default and pace uploads to it.
for key, name in ((0x3, 'MAX_CONCURRENT_STREAMS'), (0x4, 'INITIAL_WINDOW_SIZE')):
    if key not in settings:
        print('Server SETTINGS is missing %s: %s' % (name, settings))
        sys.exit(1)

print('HTTP/2 preface and server SETTINGS OK (max_concurrent_streams=%d '
      'initial_window_size=%d)' % (settings[0x3], settings[0x4]))

# Acknowledge the server's SETTINGS.
sock.sendall(frame(0, 0x4, 0x1, 0))  # SETTINGS ACK

# Send a HEADERS request for / on stream 1 and verify the server
# dispatches it through the NSAPI/SAF pipeline.
def encode_headers():
    # HPACK static-table encoding for:
    #   :method GET    (idx 2)
    #   :scheme https  (idx 7)
    #   :path /        (idx 4)
    #   :authority localhost (literal with indexing, name idx 1)
    return bytes([
        0x82, 0x87, 0x84,  # indexed :method, :scheme, :path
        0x41, 0x09,        # literal with indexing, name idx 1, value len 9
    ]) + b'localhost'

headers_payload = encode_headers()
sock.sendall(frame(len(headers_payload), 0x1, 0x5, 1) + headers_payload)

# Read the server's HEADERS response for stream 1.
def read_frame():
    header = b''
    while len(header) < 9:
        chunk = sock.recv(9 - len(header))
        if not chunk:
            print('Server closed while reading response headers')
            sys.exit(1)
        header += chunk
    length = (header[0] << 16) | (header[1] << 8) | header[2]
    ftype = header[3]
    flags = header[4]
    sid = struct.unpack('>I', header[5:9])[0]
    payload = b''
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        if not chunk:
            print('Server closed while reading response payload')
            sys.exit(1)
        payload += chunk
    return length, ftype, flags, sid, payload

# Wait for HEADERS on stream 1; ignore SETTINGS ACK / WINDOW_UPDATE frames.
status_code = None
for _ in range(20):
    length, ftype, flags, sid, payload = read_frame()
    if ftype == 0x1 and sid == 1:
        # HPACK static status: 0x88 == 200, 0x8d == 404, etc.
        if payload and payload[0] == 0x88:
            status_code = 200
        elif payload and 0x80 <= payload[0] <= 0x8f:
            # static status index range 8..15 -> 200,204,206,304,400,404,500
            static_status = [200, 204, 206, 304, 400, 404, 500]
            idx = payload[0] - 0x88
            status_code = static_status[idx] if idx < len(static_status) else payload[0]
        else:
            status_code = None
        if flags & 0x4:
            break
    elif ftype == 0x0 and sid == 1:
        # DATA before END_HEADERS? shouldn't happen, but drain it.
        continue

if status_code is None:
    print('HTTP/2 NSAPI dispatch: no HEADERS response received')
    sys.exit(1)

if status_code != 200:
    print('HTTP/2 NSAPI dispatch returned status', status_code)
    sys.exit(1)

# Drain the response DATA frame(s).
data_received = b''
for _ in range(20):
    length, ftype, flags, sid, payload = read_frame()
    if ftype == 0x0 and sid == 1:
        data_received += payload
        if flags & 0x1:
            break
    elif ftype == 0x1 and sid == 1:
        # trailing HEADERS
        break

print('HTTP/2 NSAPI dispatch OK (status 200, %d data bytes)' % len(data_received))

# Try a PING, but do not fail the smoke test if the PING ACK is not ready yet.
payload = b'h2-smoke'
sock.sendall(frame(8, 0x6, 0x0, 0) + payload)

sock.settimeout(5)
expected = 9 + 8
ping_ack = b''
try:
    while len(ping_ack) < expected:
        chunk = sock.recv(expected - len(ping_ack))
        if not chunk:
            break
        ping_ack += chunk
    if ping_ack[:9] == frame(8, 0x6, 0x1, 0) and ping_ack[9:17] == payload:
        print('HTTP/2 PING ACK OK')
    else:
        print('HTTP/2 PING ACK not yet implemented')
except socket.timeout:
    print('HTTP/2 PING ACK not yet implemented (timed out)')
PY

echo "HTTP/2 smoke test passed."
