#!/bin/bash
#
# Per-connection request cap (max-requests-per-connection) smoke test.
#
# Verifies max-requests-per-connection across:
#   1. HTTP/1.1 classic listener (io-model=classic): N requests on one socket
#      succeed; the Nth response carries "Connection: close" and the server
#      closes the socket rather than returning it to PollManager (both
#      sequential and pipelined N+1 requests).
#   2. HTTP/1.1 event listener (io-model=event): same sequential and pipelined
#      N+1 verification; the connection is not re-registered with EventLoop.
#   3. HTTP/2 (h2c on io-model=event): after N streams on one connection, all N
#      streams receive 200 OK, the server sends GOAWAY(last_stream_id=2N-1,
#      NO_ERROR) and closes the connection; an (N+1)th stream sent in the same
#      flight before GOAWAY is refused/not served.
#   4. Per-listener <http-listener><max-requests-per-connection> overrides the
#      server-wide <keep-alive><max-requests-per-connection>, including an
#      explicit 0 (unlimited) override.
#   5. /perfdump exposes MaxRequestsPerConn in KeepAliveInfo and
#      MaxRequestsPerConnection on each ListenSocket.
#
# Usage:
#   test/max_requests_smoke.sh <heliod-work-root>

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
OVERRIDE_PORT=18182
UNLIMITED_PORT=18183
GLOBAL_CAP=3
OVERRIDE_CAP=2
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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-maxreq-XXXXXX)
chmod 700 "${TEMP_DIR}"

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

for existing in root.findall("keep-alive"):
    root.remove(existing)
ka = ET.SubElement(root, "keep-alive")
ET.SubElement(ka, "enabled").text = "true"
ET.SubElement(ka, "max-requests-per-connection").text = "${GLOBAL_CAP}"

for listener in list(root.findall("http-listener")):
    root.remove(listener)

def add_listener(name, port, io_model, max_req=None):
    ls = ET.SubElement(root, "http-listener")
    ET.SubElement(ls, "name").text = name
    ET.SubElement(ls, "port").text = str(port)
    ET.SubElement(ls, "server-name").text = "localhost"
    ET.SubElement(ls, "default-virtual-server-name").text = "test"
    ET.SubElement(ls, "io-model").text = io_model
    if max_req is not None:
        ET.SubElement(ls, "max-requests-per-connection").text = str(max_req)

add_listener("http-listener-1", ${CLASSIC_PORT}, "classic")
add_listener("http-listener-2", ${EVENT_PORT}, "event")
add_listener("http-listener-3", ${OVERRIDE_PORT}, "event", ${OVERRIDE_CAP})
add_listener("http-listener-4", ${UNLIMITED_PORT}, "event", 0)

vs = root.find("virtual-server")
for el in list(vs.findall("http-listener-name")):
    vs.remove(el)
for name in ("http-listener-1", "http-listener-2", "http-listener-3", "http-listener-4"):
    ET.SubElement(vs, "http-listener-name").text = name

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY

python3 - <<PY
import os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
lines = open(path).read().splitlines(True)
mapping = ['NameTrans fn="assign-name" from="/perfdump" name="mr-perf"\n']
for i, line in enumerate(lines):
    if line.startswith('NameTrans'):
        break
else:
    raise SystemExit('obj.conf: no NameTrans line to insert before')
text = ''.join(lines[:i] + mapping + lines[i:])
text += '\n<Object name="mr-perf">\nService fn="service-dump"\n</Object>\n'
open(path, 'w').write(text)
PY

ERRLOG="${INSTANCE_DIR}/logs/errors"
: > "${ERRLOG}" 2>/dev/null || true

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-maxreq.log 2>&1 &
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
    cat /tmp/${INSTANCE}-maxreq.log 2>/dev/null || true
    cat "${ERRLOG}" 2>/dev/null || true
    exit 1
fi

python3 - <<PY
import re
import socket
import struct
import sys

HOST = '127.0.0.1'
CLASSIC_PORT = ${CLASSIC_PORT}
EVENT_PORT = ${EVENT_PORT}
OVERRIDE_PORT = ${OVERRIDE_PORT}
UNLIMITED_PORT = ${UNLIMITED_PORT}
GLOBAL_CAP = ${GLOBAL_CAP}
OVERRIDE_CAP = ${OVERRIDE_CAP}
TIMEOUT = 10

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'
STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}

failures = []


def check(name, cond, detail=''):
    if cond:
        print('  PASS: %s' % name)
    else:
        msg = '%s (%s)' % (name, detail) if detail else name
        print('  FAIL: %s' % msg)
        failures.append(msg)


def read_http11_response(sock, leftover=None):
    """Read one HTTP/1.1 response from sock; return (status, headers_dict, body) or None on EOF."""
    if leftover is None:
        leftover = [b'']
    buf = leftover[0]
    while b'\r\n\r\n' not in buf:
        try:
            chunk = sock.recv(4096)
        except ConnectionResetError:
            leftover[0] = b''
            return None
        if not chunk:
            leftover[0] = b''
            if not buf:
                return None
            raise AssertionError('EOF mid-headers: %r' % buf)
        buf += chunk
    head, rest = buf.split(b'\r\n\r\n', 1)
    lines = head.decode('iso-8859-1').split('\r\n')
    status = int(lines[0].split()[1])
    hdrs = {}
    for line in lines[1:]:
        if ':' in line:
            k, v = line.split(':', 1)
            hdrs[k.strip().lower()] = v.strip()
    clen = int(hdrs.get('content-length', '0'))
    while len(rest) < clen:
        chunk = sock.recv(max(4096, clen - len(rest)))
        if not chunk:
            raise AssertionError('EOF mid-body')
        rest += chunk
    leftover[0] = rest[clen:]
    return status, hdrs, rest[:clen]


def test_http11_sequential(label, port, cap):
    s = socket.create_connection((HOST, port), timeout=TIMEOUT)
    try:
        for i in range(1, cap + 1):
            req = ('GET / HTTP/1.1\r\nHost: localhost\r\n\r\n').encode('ascii')
            s.sendall(req)
            resp = read_http11_response(s)
            if resp is None:
                check('%s: request %d/%d answered' % (label, i, cap), False, 'unexpected EOF')
                return
            status, hdrs, _ = resp
            conn_hdr = hdrs.get('connection', '').lower()
            if i < cap:
                check('%s: req %d/%d is 200 keep-alive' % (label, i, cap),
                      status == 200 and 'close' not in conn_hdr,
                      'status=%d connection=%r' % (status, conn_hdr))
            else:
                check('%s: Nth req (%d/%d) is 200 with Connection: close' % (label, i, cap),
                      status == 200 and 'close' in conn_hdr,
                      'status=%d connection=%r' % (status, conn_hdr))

        # Sending (N+1)th request on the same socket must see EOF/reset.
        closed = False
        try:
            s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n')
            extra = read_http11_response(s)
            closed = (extra is None)
        except (BrokenPipeError, ConnectionResetError, OSError):
            closed = True
        check('%s: (N+1)th request not served on closed socket' % label, closed)
    finally:
        s.close()


def test_http11_pipelined(label, port, cap):
    s = socket.create_connection((HOST, port), timeout=TIMEOUT)
    try:
        single = b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n'
        s.sendall(single * (cap + 1))
        responses = []
        leftover = [b'']
        while True:
            resp = read_http11_response(s, leftover)
            if resp is None:
                break
            responses.append(resp)
        check('%s pipelined: exactly %d of %d requests answered before close' %
              (label, cap, cap + 1),
              len(responses) == cap,
              'got %d responses' % len(responses))
        if len(responses) == cap:
            last_conn = responses[-1][1].get('connection', '').lower()
            check('%s pipelined: Nth response has Connection: close' % label,
                  'close' in last_conn,
                  'connection=%r' % last_conn)
    finally:
        s.close()


def test_http11_unlimited(label, port, count):
    s = socket.create_connection((HOST, port), timeout=TIMEOUT)
    try:
        ok = True
        for i in range(1, count + 1):
            s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n')
            resp = read_http11_response(s)
            if resp is None or resp[0] != 200 or 'close' in resp[1].get('connection', '').lower():
                ok = False
                break
        check('%s: all %d requests served keep-alive on one socket' % (label, count), ok)
    finally:
        s.close()


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def encode_get(path='/'):
    p = path.encode()
    return (bytes([0x82, 0x86, 0x04, len(p)]) + p +
            bytes([0x01, 0x09]) + b'localhost')


class H2Client(object):
    def __init__(self, port):
        self.sock = socket.create_connection((HOST, port), timeout=TIMEOUT)
        self.sock.settimeout(TIMEOUT)
        self.next_sid = 1
        self.streams = {}
        self.goaway = None
        self.eof = False
        self.sock.sendall(PREFACE)
        self.sock.sendall(frame_header(0, 0x4, 0x0, 0))

    def _recv_exact(self, n):
        buf = b''
        while len(buf) < n:
            try:
                chunk = self.sock.recv(n - len(buf))
            except ConnectionResetError:
                self.eof = True
                raise EOFError()
            if not chunk:
                self.eof = True
                raise EOFError()
            buf += chunk
        return buf

    def open_stream(self, path='/'):
        sid = self.next_sid
        self.next_sid += 2
        self.streams[sid] = {'status': None, 'done': False, 'reset': None}
        hdrs = encode_get(path)
        self.sock.sendall(frame_header(len(hdrs), 0x1, 0x5, sid) + hdrs)
        return sid

    def pump_once(self):
        hdr = self._recv_exact(9)
        length = (hdr[0] << 16) | (hdr[1] << 8) | hdr[2]
        ftype = hdr[3]
        flags = hdr[4]
        sid = struct.unpack('>I', hdr[5:9])[0] & 0x7FFFFFFF
        payload = self._recv_exact(length) if length else b''

        if ftype == 0x4 and not (flags & 0x1):          # SETTINGS -> ACK
            self.sock.sendall(frame_header(0, 0x4, 0x1, 0))
            return
        if ftype == 0x6 and not (flags & 0x1):          # PING -> ACK
            self.sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
            return
        if ftype == 0x7 and len(payload) >= 8:          # GOAWAY
            last_sid = struct.unpack('>I', payload[:4])[0] & 0x7FFFFFFF
            err_code = struct.unpack('>I', payload[4:8])[0]
            self.goaway = (last_sid, err_code)
            return

        st = self.streams.get(sid)
        if st is None:
            return
        if ftype == 0x1 and st['status'] is None and payload:
            st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x3:
            st['reset'] = struct.unpack('>I', payload[:4])[0] if len(payload) >= 4 else -1
            st['done'] = True
            return
        if flags & 0x1:
            st['done'] = True

    def wait_stream(self, sid):
        st = self.streams[sid]
        while not st['done'] and not self.eof:
            try:
                self.pump_once()
            except EOFError:
                break
        return st

    def drain_to_eof(self):
        while not self.eof:
            try:
                self.pump_once()
            except (EOFError, socket.timeout):
                break

    def close(self):
        self.sock.close()


def test_h2_sequential(label, port, cap):
    c = H2Client(port)
    try:
        sids = []
        for i in range(1, cap + 1):
            sid = c.open_stream('/')
            sids.append(sid)
            st = c.wait_stream(sid)
            check('%s: stream %d/%d (sid=%d) returned 200' % (label, i, cap, sid),
                  st['status'] == 200,
                  'st=%r' % st)
        c.drain_to_eof()
        expected_last_sid = sids[-1]
        check('%s: GOAWAY(last_stream_id=%d, NO_ERROR) received' % (label, expected_last_sid),
              c.goaway == (expected_last_sid, 0),
              'goaway=%r' % (c.goaway,))
        check('%s: connection closed by server after Nth stream' % label,
              c.eof)
    finally:
        c.close()


def test_h2_flight_overflow(label, port, cap):
    c = H2Client(port)
    try:
        # Send cap+1 streams in one flight before reading GOAWAY.
        sids = [c.open_stream('/') for _ in range(cap + 1)]
        c.drain_to_eof()
        for sid in sids[:cap]:
            st = c.streams[sid]
            check('%s flight: stream sid=%d (<= cap) answered 200' % (label, sid),
                  st['status'] == 200 and st['done'],
                  'st=%r' % st)
        extra_sid = sids[cap]
        extra_st = c.streams[extra_sid]
        check('%s flight: excess stream sid=%d not served 200' % (label, extra_sid),
              extra_st['status'] is None,
              'extra_st=%r' % extra_st)
        check('%s flight: GOAWAY(last_stream_id=%d, NO_ERROR)' % (label, sids[cap - 1]),
              c.goaway == (sids[cap - 1], 0),
              'goaway=%r' % (c.goaway,))
    finally:
        c.close()


def test_h2_unlimited(label, port, count):
    c = H2Client(port)
    try:
        ok = True
        for _ in range(count):
            sid = c.open_stream('/')
            st = c.wait_stream(sid)
            if st['status'] != 200 or c.goaway is not None:
                ok = False
                break
        check('%s: %d streams served with no GOAWAY' % (label, count),
              ok and c.goaway is None,
              'goaway=%r' % (c.goaway,))
    finally:
        c.close()


def test_perfdump():
    import urllib.request
    with urllib.request.urlopen('http://%s:%d/perfdump' % (HOST, CLASSIC_PORT),
                                timeout=TIMEOUT) as r:
        body = r.read().decode('iso-8859-1')
    m_ka = re.search(r'MaxRequestsPerConn\s+(\d+)', body)
    check('perfdump KeepAliveInfo shows MaxRequestsPerConn=%d' % GLOBAL_CAP,
          m_ka is not None and int(m_ka.group(1)) == GLOBAL_CAP,
          'match=%r' % (m_ka.group(0) if m_ka else None))
    for lname, expected in (('http-listener-1', GLOBAL_CAP),
                            ('http-listener-2', GLOBAL_CAP),
                            ('http-listener-3', OVERRIDE_CAP),
                            ('http-listener-4', 0)):
        pat = r'ListenSocket %s:.*?MaxRequestsPerConnection\s+(\d+)' % re.escape(lname)
        m_ls = re.search(pat, body, re.S)
        check('perfdump %s shows MaxRequestsPerConnection=%d' % (lname, expected),
              m_ls is not None and int(m_ls.group(1)) == expected,
              'match=%r' % (m_ls.group(0) if m_ls else None))


print('=== 1. /perfdump configuration reporting ===')
test_perfdump()

print('=== 2. Classic HTTP/1.1 (io-model=classic, cap=%d) ===' % GLOBAL_CAP)
test_http11_sequential('classic HTTP/1.1', CLASSIC_PORT, GLOBAL_CAP)
test_http11_pipelined('classic HTTP/1.1', CLASSIC_PORT, GLOBAL_CAP)

print('=== 3. Event HTTP/1.1 (io-model=event, cap=%d) ===' % GLOBAL_CAP)
test_http11_sequential('event HTTP/1.1', EVENT_PORT, GLOBAL_CAP)
test_http11_pipelined('event HTTP/1.1', EVENT_PORT, GLOBAL_CAP)

print('=== 4. HTTP/2 (io-model=event, cap=%d) ===' % GLOBAL_CAP)
test_h2_sequential('HTTP/2', EVENT_PORT, GLOBAL_CAP)
test_h2_flight_overflow('HTTP/2', EVENT_PORT, GLOBAL_CAP)

print('=== 5. Per-listener overrides (cap=%d and cap=0 unlimited) ===' % OVERRIDE_CAP)
test_http11_sequential('listener override HTTP/1.1', OVERRIDE_PORT, OVERRIDE_CAP)
test_h2_sequential('listener override HTTP/2', OVERRIDE_PORT, OVERRIDE_CAP)
test_http11_unlimited('listener unlimited HTTP/1.1', UNLIMITED_PORT, 6)
test_h2_unlimited('listener unlimited HTTP/2', UNLIMITED_PORT, 6)

if failures:
    print('\nFAILED (%d):' % len(failures))
    for f in failures:
        print('  - %s' % f)
    sys.exit(1)
print('\nAll max-requests-per-connection checks passed.')
PY
