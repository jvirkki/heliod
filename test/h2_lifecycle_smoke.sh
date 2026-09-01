#!/bin/bash
#
# HTTP/2 connection lifecycle test.
#
# h2_smoke.sh proves that a single HTTP/2 request works.  This test proves the
# connection *lifecycle* works: that an H2Session survives across event-loop
# dispatches instead of pinning a worker thread for the life of the TCP
# connection.  It pins a deliberately small worker pool and then exceeds it.
#
# Reuses the https-test instance created by `make install`.  The original
# config is saved before the test and restored afterwards.
#
# Usage:
#   test/h2_lifecycle_smoke.sh <heliod-work-root>
# Example:
#   test/h2_lifecycle_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

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
    cat /tmp/${INSTANCE}-h2life.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

# Pin a deliberately small worker pool.  The whole point of this test is to
# prove that HTTP/2 connection concurrency is no longer bounded by the number
# of worker threads, so the bound has to be small enough to exceed easily.
MAX_THREADS=8

pin_thread_pool() {
    python3 - <<PY
import xml.etree.ElementTree as ET
import os

path = os.path.join("${CONFIG_DIR}", "server.xml")
tree = ET.parse(path)
root = tree.getroot()

for existing in root.findall("thread-pool"):
    root.remove(existing)

tp = ET.SubElement(root, "thread-pool")
ET.SubElement(tp, "min-threads").text = "${MAX_THREADS}"
ET.SubElement(tp, "max-threads").text = "${MAX_THREADS}"
ET.SubElement(tp, "queue-size").text = "512"

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

configure_instance
pin_thread_pool
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2life.log 2>&1 &
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
import os
import socket
import ssl
import struct
import sys
import time

HOST = 'localhost'
PORT = ${HTTPS_PORT}
MAX_THREADS = ${MAX_THREADS}

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'

# HPACK: indexed :method GET, :scheme https, :path /, then :authority as a
# literal with incremental indexing (name index 1).
REQUEST_HEADERS = bytes([0x82, 0x87, 0x84, 0x41, 0x09]) + b'localhost'

STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


class H2Conn(object):
    """Minimal HTTP/2 client: enough to drive real request/response cycles.

    Frames are demultiplexed into per-stream state as they arrive rather than
    being matched against one expected stream, because a multiplexing server is
    free to interleave streams -- and does: it emits HEADERS for every open
    stream before any DATA.  A reader that skipped frames for other streams
    would throw away their HEADERS and never see their status.
    """

    def __init__(self, timeout=30):
        ctx = ssl._create_unverified_context()
        ctx.set_alpn_protocols(['h2'])
        raw = socket.create_connection((HOST, PORT), timeout=timeout)
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
        """Read frames until the server's initial SETTINGS arrives, then ACK."""
        for _ in range(10):
            ftype, flags, sid, payload = self.read_frame()
            if ftype == 0x4 and not (flags & 0x1):
                self.sock.sendall(frame_header(0, 0x4, 0x1, 0))  # SETTINGS ACK
                return
        raise AssertionError('no server SETTINGS received')

    def send_request(self):
        sid = self.next_stream_id
        self.next_stream_id += 2
        self.streams[sid] = {'status': None, 'body': 0, 'done': False,
                             'reset': None}
        # END_STREAM | END_HEADERS
        self.sock.sendall(frame_header(len(REQUEST_HEADERS), 0x1, 0x5, sid) +
                          REQUEST_HEADERS)
        return sid

    def _pump_one_frame(self):
        """Read a single frame and fold it into per-stream state."""
        ftype, flags, sid, payload = self.read_frame()

        if ftype == 0x6 and not (flags & 0x1):            # PING -> ACK
            self.sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
            return
        if ftype == 0x7:                                  # GOAWAY
            self.goaway = payload
            raise AssertionError('server sent GOAWAY: %r' % payload[:32])

        st = self.streams.get(sid)
        if st is None:
            return                                        # connection-level

        if ftype == 0x1:                                  # HEADERS
            if st['status'] is None and payload:
                st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x0:                                # DATA
            st['body'] += len(payload)
        elif ftype == 0x3:                                # RST_STREAM
            st['reset'] = (struct.unpack('>I', payload[:4])[0]
                           if len(payload) >= 4 else -1)
            st['done'] = True
            return

        if flags & 0x1:                                   # END_STREAM
            st['done'] = True

    def read_response(self, sid):
        """Return (status, body_len) for stream sid, demuxing other streams."""
        st = self.streams[sid]
        for _ in range(2000):
            if st['done']:
                if st['reset'] is not None:
                    raise AssertionError('stream %d reset, code %d'
                                         % (sid, st['reset']))
                return st['status'], st['body']
            self._pump_one_frame()
        raise AssertionError('no END_STREAM for stream %d' % sid)

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


failures = []


def check(label, condition, detail=''):
    if condition:
        print('[h2-lifecycle] %s: OK%s' % (label, detail))
    else:
        print('[h2-lifecycle] %s: FAIL%s' % (label, detail))
        failures.append(label)


# ---------------------------------------------------------------------------
# Test 1 -- session persistence.
#
# Ten sequential requests on ONE connection.  Each response returns the worker
# to the pool and parks the fd back on the event loop, so this only passes if
# the H2Session survives across dispatches (Connection::h2Session) instead of
# being rebuilt or torn down after the first request.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
alpn_ok = (conn.alpn == 'h2')
check('ALPN h2', alpn_ok, ' (%s)' % conn.alpn)

ok = True
detail = ''
for i in range(10):
    sid = conn.send_request()
    status, body = conn.read_response(sid)
    if status != 200 or body <= 0:
        ok = False
        detail = ' (request %d: status=%s body=%d)' % (i + 1, status, body)
        break
check('10 sequential requests on one connection', ok, detail)
conn.close()

# ---------------------------------------------------------------------------
# Test 2 -- idle connection resumes after a delay.
#
# Request, then sit idle long enough that the connection is definitely parked
# on the event loop with no worker attached, then request again on a new
# stream.  Proves resumeConnection() picks the session back up.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
sid = conn.send_request()
first = conn.read_response(sid)
time.sleep(4)
sid = conn.send_request()
second = conn.read_response(sid)
check('request after 4s idle on parked connection',
      first[0] == 200 and second[0] == 200,
      ' (before=%s after=%s)' % (first[0], second[0]))
conn.close()

# ---------------------------------------------------------------------------
# Test 3 -- concurrency beyond the worker-thread count.
#
# Open 4x more simultaneous HTTP/2 connections than there are worker threads,
# leave them all idle at once, then drive a request on every one.  Under the
# old blocking H2Session::run() each live connection pinned a worker for its
# whole lifetime, so only MAX_THREADS of these could ever be served and the
# rest would hang.  All of them must succeed.
# ---------------------------------------------------------------------------
n_conns = int(os.environ.get('H2_LIFECYCLE_CONNS', MAX_THREADS * 4))
conns = []
open_error = ''
try:
    for i in range(n_conns):
        try:
            c = H2Conn(timeout=15)
            c.await_settings()
            conns.append(c)
        except Exception as exc:
            open_error = ' stalled opening connection %d/%d: %r' % (
                i + 1, n_conns, exc)
            break
    opened = len(conns)
    print('[h2-lifecycle] opened %d/%d connections%s'
          % (opened, n_conns, open_error))

    # All idle simultaneously, with no worker able to be attached to any of them.
    time.sleep(3)

    sids = [c.send_request() for c in conns]
    served = 0
    bad = []
    for c, sid in zip(conns, sids):
        try:
            status, body = c.read_response(sid)
            if status == 200 and body > 0:
                served += 1
            else:
                bad.append('status=%s body=%d' % (status, body))
        except Exception as exc:
            bad.append(repr(exc))

    check('%d concurrent connections with only %d worker threads'
          % (n_conns, MAX_THREADS),
          opened == n_conns and served == n_conns,
          ' (opened=%d served=%d%s%s)'
          % (opened, served, open_error,
             '' if not bad else ' first_error=%s' % bad[0]))
finally:
    for c in conns:
        c.close()

# ---------------------------------------------------------------------------
# Test 4 -- concurrent streams on a single connection.
#
# Submit 20 requests back-to-back without reading, so they are in flight
# together, then collect all responses.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
sids = [conn.send_request() for _ in range(20)]
got = 0
err = ''
for sid in sids:
    try:
        status, body = conn.read_response(sid)
        if status == 200 and body > 0:
            got += 1
        else:
            err = ' (stream %d: status=%s body=%d)' % (sid, status, body)
    except Exception as exc:
        err = ' %r' % (exc,)
        break
check('20 concurrent streams on one connection', got == 20,
      ' (completed=%d/20%s)' % (got, err))
conn.close()

if failures:
    print('h2 lifecycle test FAILED: %s' % ', '.join(failures))
    sys.exit(1)
print('h2 lifecycle test passed.')
PY

# Fail the test if this run logged a crash or an assertion failure.
if [ -f "${ERRLOG}" ]; then
    NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                 grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
    if [ -n "${NEW_ERRORS}" ]; then
        echo "[h2-lifecycle] server error log reports a crash or assertion:"
        echo "${NEW_ERRORS}"
        exit 1
    fi
fi
echo "[h2-lifecycle] no crashes or assertions in server error log"
