#!/bin/bash
#
# Event-loop idle timeout test.
#
# The event loop parks idle connections instead of dedicating a thread to each
# one, which means nothing reclaims a connection unless the loop itself expires
# it.  This test proves the loop does: that an idle connection is closed on its
# deadline, that a busy connection is not, and that HTTP/2 is told the session
# is going away instead of just having its socket dropped.
#
# The server is started with a deliberately short HELIOD_EVENT_IDLE_TIMEOUT so
# the test finishes in under a minute; the production default is the configured
# keep-alive timeout.
#
# Reuses the https-test instance created by `make install`.  The original
# config is saved before the test and restored afterwards.
#
# Usage:
#   test/h2_idle_timeout_smoke.sh <heliod-work-root>
# Example:
#   test/h2_idle_timeout_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    echo "Example: $0 /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT"
    exit 1
fi

# Resolve to an absolute path up front: this script cd's into the work root and
# then into the config directory, so a relative argument would break the second
# hop with a confusing "cannot stat .../config" error.
WORK_ROOT=$(cd "$1" 2>/dev/null && pwd) || {
    echo "Work root '$1' not found"
    exit 1
}
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
HTTP_PORT=18180
HTTPS_PORT=18443
TEMP_DIR=""
SERVER_PID=""

# Short enough to keep the test quick, long enough that a single request/response
# cycle cannot accidentally straddle it.
IDLE_TIMEOUT=5

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
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-idle-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/idle-smoke-key.pem \
    -out /tmp/idle-smoke-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/idle-smoke.p12 \
    -inkey /tmp/idle-smoke-key.pem -in /tmp/idle-smoke-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/idle-smoke.p12 -d dbm:"." -w "${NO_PASS}"
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
    elif name == "https-listener-1":
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

if "https-listener-1" not in vs_listeners:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"

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
    cat /tmp/${INSTANCE}-idle.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

configure_instance

# HELIOD_EVENT_IDLE_TIMEOUT overrides the configured keep-alive timeout for the
# event loop only, which is what makes this test fast enough to be worth running.
HELIOD_EVENT_IDLE_TIMEOUT=${IDLE_TIMEOUT} \
    "${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-idle.log 2>&1 &
SERVER_PID=$!
wait_for_listener

# Record the current end of the error log so we only inspect what this run adds.
ERRLOG="${INSTANCE_DIR}/logs/errors"
if [ -f "${ERRLOG}" ]; then
    ERRLOG_START=$(wc -l < "${ERRLOG}")
else
    ERRLOG_START=0
fi

python3 - <<PY
import socket
import ssl
import struct
import sys
import time

HOST = 'localhost'
HTTPS_PORT = ${HTTPS_PORT}
HTTP_PORT = ${HTTP_PORT}
IDLE = ${IDLE_TIMEOUT}

# How long past the deadline we are willing to wait for the close.  The loop
# sweeps once a second, so a correct server closes within about IDLE + 1.
GRACE = 6

# Closing earlier than this means the deadline is not being measured from the
# last event -- e.g. armed once at accept time and never refreshed.
EARLY = IDLE - 1

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'
REQUEST_HEADERS = bytes([0x82, 0x87, 0x84, 0x41, 0x09]) + b'localhost'
STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


class H2Conn(object):
    """Minimal HTTP/2 client, sufficient to drive requests and observe GOAWAY."""

    def __init__(self, timeout=30):
        ctx = ssl._create_unverified_context()
        ctx.set_alpn_protocols(['h2'])
        raw = socket.create_connection((HOST, HTTPS_PORT), timeout=timeout)
        self.sock = ctx.wrap_socket(raw, server_hostname=HOST)
        self.sock.settimeout(timeout)
        self.alpn = self.sock.selected_alpn_protocol()
        self.next_stream_id = 1
        self.streams = {}
        self.goaway = None
        self.sock.sendall(PREFACE)
        self.sock.sendall(frame_header(0, 0x4, 0x0, 0))   # empty SETTINGS

    def _recv_exactly(self, n):
        buf = b''
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise EOFError('server closed connection')
            buf += chunk
        return buf

    def read_frame(self):
        header = self._recv_exactly(9)
        length = (header[0] << 16) | (header[1] << 8) | header[2]
        ftype = header[3]
        flags = header[4]
        sid = struct.unpack('>I', header[5:9])[0] & 0x7FFFFFFF
        payload = self._recv_exactly(length) if length else b''
        return ftype, flags, sid, payload

    def await_settings(self):
        for _ in range(10):
            ftype, flags, sid, payload = self.read_frame()
            if ftype == 0x4 and not (flags & 0x1):
                self.sock.sendall(frame_header(0, 0x4, 0x1, 0))  # SETTINGS ACK
                return
        raise AssertionError('no server SETTINGS received')

    def send_request(self):
        sid = self.next_stream_id
        self.next_stream_id += 2
        self.streams[sid] = {'status': None, 'body': 0, 'done': False}
        self.sock.sendall(frame_header(len(REQUEST_HEADERS), 0x1, 0x5, sid) +
                          REQUEST_HEADERS)
        return sid

    def _fold(self, ftype, flags, sid, payload):
        if ftype == 0x6 and not (flags & 0x1):            # PING -> ACK
            self.sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
            return
        if ftype == 0x7:                                  # GOAWAY
            self.goaway = payload
            return

        st = self.streams.get(sid)
        if st is None:
            return
        if ftype == 0x1 and st['status'] is None and payload:
            st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x0:
            st['body'] += len(payload)
        if flags & 0x1:
            st['done'] = True

    def read_response(self, sid):
        st = self.streams[sid]
        for _ in range(2000):
            if st['done']:
                return st['status'], st['body']
            self._fold(*self.read_frame())
        raise AssertionError('no END_STREAM for stream %d' % sid)

    def wait_for_close(self, limit):
        """Read until the peer closes.  Returns (seconds waited, saw_goaway)."""
        started = time.time()
        self.sock.settimeout(limit)
        while True:
            remaining = limit - (time.time() - started)
            if remaining <= 0:
                return None, self.goaway is not None
            self.sock.settimeout(remaining)
            try:
                ftype, flags, sid, payload = self.read_frame()
            except (EOFError, ConnectionResetError):
                return time.time() - started, self.goaway is not None
            except ssl.SSLError:
                # NSS close-notify surfaces here rather than as a clean EOF.
                return time.time() - started, self.goaway is not None
            except socket.timeout:
                return None, self.goaway is not None
            self._fold(ftype, flags, sid, payload)

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


failures = []


def check(label, condition, detail=''):
    if condition:
        print('[h2-idle] %s: OK%s' % (label, detail))
    else:
        print('[h2-idle] %s: FAIL%s' % (label, detail))
        failures.append(label)


# ---------------------------------------------------------------------------
# Test 1 -- an idle HTTP/2 connection is reclaimed on its deadline, with GOAWAY.
#
# Before the event loop had any timer at all, this connection stayed open for
# the life of the process holding an fd and a Connection slot nothing could
# reclaim.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
sid = conn.send_request()
status, body = conn.read_response(sid)
check('baseline request before idling', status == 200 and body > 0,
      ' (status=%s body=%d)' % (status, body))

waited, saw_goaway = conn.wait_for_close(IDLE + GRACE)
check('idle h2 connection closed on deadline',
      waited is not None and waited >= EARLY,
      ' (closed after %s s, deadline %d s)'
      % ('never' if waited is None else '%.1f' % waited, IDLE))
check('h2 close announced with GOAWAY', saw_goaway,
      '' if saw_goaway else ' (socket dropped with no GOAWAY frame)')
conn.close()

# ---------------------------------------------------------------------------
# Test 2 -- a connection that keeps working is never reclaimed.
#
# The deadline has to be measured from the last event, not from when the
# connection was accepted.  Drive requests across three whole timeout periods:
# a deadline armed once at accept time would kill this connection mid-run.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
rounds = (IDLE * 3) // 2
ok = True
detail = ''
i = 0
try:
    for i in range(rounds):
        sid = conn.send_request()
        status, body = conn.read_response(sid)
        if status != 200 or body <= 0:
            ok = False
            detail = ' (round %d: status=%s body=%d)' % (i + 1, status, body)
            break
        time.sleep(2)
except Exception as exc:
    ok = False
    detail = ' (round %d: %r)' % (i + 1, exc)
check('active h2 connection survives %d s of use' % (rounds * 2),
      ok and conn.goaway is None,
      detail if detail else (' (unexpected GOAWAY)' if conn.goaway else ''))
conn.close()

# ---------------------------------------------------------------------------
# Test 3 -- the same reclaim applies to event-mode HTTP/1.1 keep-alive.
#
# The idle timeout is a property of the event loop, not of HTTP/2: before this
# work an idle event-mode keep-alive connection was just as unreclaimable.
# ---------------------------------------------------------------------------
sock = socket.create_connection((HOST, HTTP_PORT), timeout=IDLE * 2 + GRACE)
sock.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n'
             b'Connection: keep-alive\r\n\r\n')
first = sock.recv(65536)
check('baseline HTTP/1.1 keep-alive request',
      first.startswith(b'HTTP/1.1 200'),
      ' (%s)' % first.split(b'\r\n')[0].decode('latin-1'))

started = time.time()
closed = None
sock.settimeout(IDLE + GRACE)
try:
    while True:
        chunk = sock.recv(65536)
        if not chunk:
            closed = time.time() - started
            break
except ConnectionResetError:
    # A reset is still a close, just a less graceful one.
    closed = time.time() - started
except socket.timeout:
    # Still open past the grace period: the deadline was never serviced.
    closed = None
check('idle HTTP/1.1 keep-alive connection closed on deadline',
      closed is not None and closed >= EARLY,
      ' (closed after %s s, deadline %d s)'
      % ('never' if closed is None else '%.1f' % closed, IDLE))
sock.close()

# ---------------------------------------------------------------------------
# Test 4 -- reaping idle connections leaves the server healthy.
#
# A sweep that corrupted its registry or double-freed a Connection would show
# up here rather than as a clean failure above.
# ---------------------------------------------------------------------------
try:
    conn = H2Conn()
    conn.await_settings()
    sid = conn.send_request()
    status, body = conn.read_response(sid)
    conn.close()
    check('server still serving after reaping idle connections',
          status == 200 and body > 0, ' (status=%s body=%d)' % (status, body))
except Exception as exc:
    check('server still serving after reaping idle connections', False,
          ' (%r)' % (exc,))

if failures:
    print('h2 idle timeout test FAILED: %s' % ', '.join(failures))
    sys.exit(1)
print('h2 idle timeout test passed.')
PY

# Reclaiming an idle connection is routine behaviour, so it must not be logged
# as a fault, and it must not trip an assertion in the sweep.
if [ -f "${ERRLOG}" ]; then
    NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                 grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
    if [ -n "${NEW_ERRORS}" ]; then
        echo "[h2-idle] server error log reports a crash or assertion:"
        echo "${NEW_ERRORS}"
        exit 1
    fi
    NEW_WARNINGS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                   grep -E 'EventLoop|H2Session|EventDispatcher' |
                   grep -E 'WARNING|FAILURE' || true)
    if [ -n "${NEW_WARNINGS}" ]; then
        echo "[h2-idle] idle reclaim logged warnings it should not have:"
        echo "${NEW_WARNINGS}"
        exit 1
    fi
fi
echo "[h2-idle] error log clean: no crashes, assertions or warnings"
