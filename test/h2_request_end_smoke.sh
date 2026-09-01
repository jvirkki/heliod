#!/bin/bash
#
# HTTP/2 request-completion test.
#
# A stream runs when its request is complete, and the only thing that says a
# request is complete is END_STREAM.  There is more than one frame it can ride
# on, and this test drives every one of them:
#
#   1. HEADERS with END_STREAM               -- a bodyless request
#   2. HEADERS, then an empty DATA frame     -- a bodyless request from a peer
#      with END_STREAM                          that did not know it was bodyless
#   3. HEADERS, then DATA with END_STREAM    -- a request with a body
#   4. HEADERS, DATA, then an empty DATA     -- a body whose terminator is a
#      frame with END_STREAM                    frame of its own
#
# Then the same shapes multiplexed and mixed with ordinary streams, and finally a
# body over the per-stream cap, where completing the request is exactly what must
# not happen: the stream has been reset and its buffered body abandoned part-way.
#
# Case 2 is the regression this test exists for.  It used to hang for ever:
# END_STREAM on a DATA frame was noticed only in nghttp2's data-chunk callback,
# and nghttp2 does not call that callback for a DATA frame with no payload, so
# nothing ever marked the request complete and the stream was never dispatched.
# The client waited until it gave up; the connection went on serving its other
# streams perfectly, which is what made it look like a client problem.
#
# That shape is not exotic.  Go's HTTP/2 client sends an empty final DATA frame
# for any request whose body is non-nil but of unknown length, and a reverse
# proxy produces exactly that when the request being forwarded arrived over
# HTTP/3 -- quic-go cannot know the length up front, so ContentLength is -1, so
# net/http/httputil's "no body means nil body" shortcut does not fire.  In front
# of Caddy with an h2c upstream, every HTTP/3 request hung while HTTP/1.1 and
# HTTP/2 clients were served normally.
#
# Runs over cleartext h2c with prior knowledge, which is the upstream path a
# reverse proxy uses, and needs no certificate.  Reuses the https-test instance
# created by `make install`; the original config is saved and restored.
#
# Usage:
#   test/h2_request_end_smoke.sh <heliod-work-root>
# Example:
#   test/h2_request_end_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    echo "Example: $0 /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT"
    exit 1
fi

WORK_ROOT=$(cd "$1" 2>/dev/null && pwd) || {
    echo "Work root '$1' not found"
    exit 1
}
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
HTTP_PORT=18180
TEMP_DIR=""
CGI_DIR=""
SERVER_PID=""

# Streams for the multiplexing check.  All of them are case 2: the point is that
# the fix dispatches every such stream, not just one, and that they do not
# serialize behind each other.
N_STREAMS=8

# Pinned low so the refusal case can be provoked with a few KB instead of the
# 16 MB default.  1024 is the floor the server accepts.
BODY_LIMIT=1024

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
    if [ -n "${CGI_DIR:-}" ] && [ -d "${CGI_DIR}" ]; then
        rm -rf "${CGI_DIR}"
    fi
}

trap cleanup_all EXIT

cd "${WORK_ROOT}"

# Stop any running instance so we can safely rewrite its config.
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2end-XXXXXX)
chmod 700 "${TEMP_DIR}"

# The CGI reports back what the server made of the request, which is how the two
# body cases are checked: a body that is buffered but mismeasured would still
# answer 200, so the assertion has to be on CONTENT_LENGTH and the bytes.
CGI_DIR=$(mktemp -d /tmp/${INSTANCE}-h2endcgi-XXXXXX)
chmod 755 "${CGI_DIR}"
cat > "${CGI_DIR}/echo.sh" <<'CGI'
#!/bin/sh
printf 'Content-type: text/plain\r\n'
printf '\r\n'
echo "method=${REQUEST_METHOD}"
echo "protocol=${SERVER_PROTOCOL}"
echo "content-length=${CONTENT_LENGTH}"
printf 'body='
if [ -n "${CONTENT_LENGTH}" ] && [ "${CONTENT_LENGTH}" -gt 0 ]; then
    head -c "${CONTENT_LENGTH}"
fi
echo ""
CGI
chmod 755 "${CGI_DIR}/echo.sh"

# One cleartext listener on the event model.  The stock HTTPS listener is removed
# rather than configured: this test speaks h2c with prior knowledge, so there is
# no ALPN to negotiate and no certificate to create.
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

for listener in root.findall("http-listener"):
    name = listener.findtext("name")
    if name == "https-listener-1":
        root.remove(listener)
        continue
    if name != "http-listener-1":
        continue
    port_el = listener.find("port")
    if port_el is None:
        port_el = ET.SubElement(listener, "port")
    port_el.text = "${HTTP_PORT}"
    for child in list(listener):
        if child.tag == "io-model":
            listener.remove(child)
    ET.SubElement(listener, "io-model").text = "event"

vs = root.find("virtual-server")
for el in vs.findall("http-listener-name"):
    if el.text == "https-listener-1":
        vs.remove(el)

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, xml_declaration=False, encoding="UTF-8")
PY
}

# Route /cgi-bin at the echo script, ahead of the document-tree path checks.
add_cgi_mapping() {
    python3 - <<PY
import os

path = os.path.join("${CONFIG_DIR}", "obj.conf")
text = open(path).read()
anchor = 'PathCheck fn="uri-clean"'
mapping = ('NameTrans fn="pfx2dir" from="/cgi-bin" dir="${CGI_DIR}" '
           'name="cgi"\n')
if anchor not in text:
    raise SystemExit('obj.conf: no uri-clean anchor to insert before')
open(path, 'w').write(text.replace(anchor, mapping + anchor, 1))
PY
}

wait_for_listener() {
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        # A real request, not just a listening socket: the socket is bound before
        # the server can serve through it.
        if curl -s -o /dev/null --max-time 3 "http://localhost:${HTTP_PORT}/" \
                2>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start serving on port ${HTTP_PORT}"
    cat /tmp/${INSTANCE}-h2end.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

configure_instance
add_cgi_mapping

# The refusal case needs a cap it can reach; the 16 MB default would mean pushing
# 16 MB through a flow-controlled connection to test a code path that cares only
# about the crossing.
echo "Http2MaxRequestBodySize ${BODY_LIMIT}" >> "${CONFIG_DIR}/magnus.conf"

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2end.log 2>&1 &
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
import struct
import sys
import time

HOST = 'localhost'
PORT = ${HTTP_PORT}
N_STREAMS = ${N_STREAMS}

# Matches the Http2MaxRequestBodySize written into magnus.conf for this run.
# 1 KB is the floor the server allows; the oversize case sends four times it,
# which is still far inside the initial flow-control window, so nothing here
# depends on WINDOW_UPDATE handling.
BODY_LIMIT = ${BODY_LIMIT}

ENHANCE_YOUR_CALM = 0x0b

# Long enough that a slow machine is not mistaken for a hang, short enough that
# the regression this test guards fails in well under a minute instead of
# hanging the suite.
TIMEOUT = 15

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'

STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def encode_headers(method, path):
    """Minimal HPACK: indexed :method/:scheme, literal :path and :authority.

    Everything literal goes out without indexing, so the server's dynamic table
    stays empty and each request on a connection is encoded identically -- one
    less moving part when a case fails.

    Deliberately no content-length, even on the requests that carry a body.
    HTTP/2 does not need one (DATA framing carries the length) and the proxy
    whose traffic this reproduces does not send one, so the server has to derive
    it from the frames.
    """
    out = bytearray()
    out += bytes([0x82]) if method == 'GET' else bytes([0x83])  # :method
    out += bytes([0x86])                                        # :scheme http
    p = path.encode()
    out += bytes([0x04, len(p)]) + p                            # :path
    out += bytes([0x01, 0x09]) + b'localhost'                   # :authority
    return bytes(out)


class H2Conn(object):
    """Minimal h2c client that folds frames into per-stream state as they land.

    Same shape as the client in h2_parallel_streams_smoke.sh, without TLS: a
    multiplexing server is free to interleave streams, so responses cannot be
    read by assuming the next frame belongs to the stream being waited on.
    """

    def __init__(self, timeout=TIMEOUT):
        self.sock = socket.create_connection((HOST, PORT), timeout=timeout)
        self.sock.settimeout(timeout)
        self.next_stream_id = 1
        self.streams = {}
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

    def _new_stream(self):
        sid = self.next_stream_id
        self.next_stream_id += 2
        self.streams[sid] = {'status': None, 'body': b'', 'done': False,
                             'reset': None}
        return sid

    def send_headers(self, method, path, end_stream):
        """HEADERS for a new stream.  END_HEADERS always; END_STREAM on demand."""
        sid = self._new_stream()
        hdrs = encode_headers(method, path)
        flags = 0x4 | (0x1 if end_stream else 0x0)
        self.sock.sendall(frame_header(len(hdrs), 0x1, flags, sid) + hdrs)
        return sid

    def send_data(self, sid, payload, end_stream):
        """A DATA frame, which may be empty -- that is the case under test."""
        flags = 0x1 if end_stream else 0x0
        self.sock.sendall(frame_header(len(payload), 0x0, flags, sid) + payload)

    def _pump_one_frame(self):
        ftype, flags, sid, payload = self.read_frame()

        if ftype == 0x6 and not (flags & 0x1):            # PING -> ACK
            self.sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
            return
        if ftype == 0x7:                                  # GOAWAY
            raise AssertionError('server sent GOAWAY: %r' % payload[:32])

        st = self.streams.get(sid)
        if st is None:
            return                                        # connection-level

        if ftype == 0x1:                                  # HEADERS
            if st['status'] is None and payload:
                st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x0:                                # DATA
            st['body'] += payload
        elif ftype == 0x3:                                # RST_STREAM
            st['reset'] = (struct.unpack('>I', payload[:4])[0]
                           if len(payload) >= 4 else -1)
            st['done'] = True
            return

        if flags & 0x1:                                   # END_STREAM
            st['done'] = True

    def settle(self, sid):
        """Pump until the stream ends or the peer goes quiet, judging nothing.

        read_response() is the reader for a stream that is meant to succeed, and
        treats a RST_STREAM as a failure.  A stream that is meant to be refused
        needs the opposite: the reset is the answer, and the case decides whether
        it is the right one.
        """
        st = self.streams[sid]
        for _ in range(4000):
            if st['done']:
                return st
            try:
                self._pump_one_frame()
            except socket.timeout:
                return st
        return st

    def read_response(self, sid):
        st = self.streams[sid]
        for _ in range(4000):
            if st['done']:
                if st['reset'] is not None:
                    raise AssertionError('stream %d reset, code %d'
                                         % (sid, st['reset']))
                return st['status'], st['body']
            try:
                self._pump_one_frame()
            except socket.timeout:
                raise AssertionError(
                    'stream %d: no response within %ds -- the request was '
                    'never dispatched' % (sid, TIMEOUT))
        raise AssertionError('no END_STREAM for stream %d' % sid)

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


failures = []


def check(name, cond, detail=''):
    if cond:
        print('[h2-end] %s: OK%s' % (name, (' (%s)' % detail) if detail else ''))
    else:
        print('[h2-end] %s: FAIL %s' % (name, detail))
        failures.append(name)


def cgi_field(body, key):
    for line in body.decode('utf-8', 'replace').splitlines():
        if line.startswith(key + '='):
            return line[len(key) + 1:]
    return None


def run_case(name, body):
    """Run one case on a connection of its own, and keep going if it fails.

    The failure this test is built around is a hang, which arrives as an
    AssertionError out of read_response().  Catching it here rather than letting
    it end the run is the difference between a report that says "the empty-DATA
    shape hangs and the other five are fine" and one that says "case 2 hung, the
    rest untested".  Each case gets its own connection so a stream left for dead
    by one cannot confuse the next.
    """
    conn = H2Conn()
    try:
        body(conn)
    except (AssertionError, OSError, EOFError) as exc:
        print('[h2-end] %s: FAIL %s' % (name, exc))
        failures.append(name)
    finally:
        conn.close()


# ---------------------------------------------------------------- case 1
# END_STREAM on HEADERS.  The shape every ordinary client sends for a GET, and
# the one that kept working while case 2 hung -- included so that a failure here
# distinguishes "this test is broken" from "the DATA path is broken".
def case_headers(conn):
    sid = conn.send_headers('GET', '/', end_stream=True)
    status, body = conn.read_response(sid)
    check('END_STREAM on HEADERS', status == 200,
          'status %s, %d bytes' % (status, len(body)))


# ---------------------------------------------------------------- case 2
# The regression: END_STREAM arrives on a DATA frame with no payload at all.
def case_empty_data(conn):
    sid = conn.send_headers('GET', '/', end_stream=False)
    conn.send_data(sid, b'', end_stream=True)
    status, body = conn.read_response(sid)
    check('END_STREAM on empty DATA', status == 200,
          'status %s, %d bytes' % (status, len(body)))


# ---------------------------------------------------------------- case 3
# A body that ends on its own DATA frame.  Checks the server derived
# CONTENT_LENGTH from the frames, since the request carried no content-length.
def case_body(conn):
    sid = conn.send_headers('POST', '/cgi-bin/echo.sh', end_stream=False)
    conn.send_data(sid, b'hello', end_stream=True)
    status, body = conn.read_response(sid)
    check('END_STREAM on DATA with body',
          status == 200 and cgi_field(body, 'content-length') == '5'
          and cgi_field(body, 'body') == 'hello',
          'status %s, content-length %s, body %r'
          % (status, cgi_field(body, 'content-length'),
             cgi_field(body, 'body')))


# ---------------------------------------------------------------- case 4
# A body whose terminator is an empty frame of its own: the same blind spot as
# case 2, but with bytes already buffered.  A server that only looks at the
# chunk callback sees the body and still never runs the request.
def case_body_then_empty_data(conn):
    sid = conn.send_headers('POST', '/cgi-bin/echo.sh', end_stream=False)
    conn.send_data(sid, b'hello', end_stream=False)
    conn.send_data(sid, b'', end_stream=True)
    status, body = conn.read_response(sid)
    check('END_STREAM on empty DATA after body',
          status == 200 and cgi_field(body, 'content-length') == '5'
          and cgi_field(body, 'body') == 'hello',
          'status %s, content-length %s, body %r'
          % (status, cgi_field(body, 'content-length'),
             cgi_field(body, 'body')))


# ---------------------------------------------------------------- case 5
# Every stream on one connection, all in the case-2 shape, all issued before any
# response is read.  One dispatched stream is not enough: the frame callback has
# to complete each of them independently.
def case_multiplexed(conn):
    sids = []
    for _ in range(N_STREAMS):
        sid = conn.send_headers('GET', '/', end_stream=False)
        conn.send_data(sid, b'', end_stream=True)
        sids.append(sid)
    started = time.time()
    answered = [conn.read_response(sid)[0] for sid in sids]
    elapsed = time.time() - started
    check('%d multiplexed empty-DATA streams' % N_STREAMS,
          all(s == 200 for s in answered),
          '%d/%d answered 200 in %.2fs' %
          (sum(1 for s in answered if s == 200), N_STREAMS, elapsed))


# ---------------------------------------------------------------- case 6
# Interleaved with a normal request on the same connection.  This is the
# production mix -- HTTP/1.1 clients and HTTP/3 clients sharing one h2c upstream
# connection -- and it is why the bug looked like a client fault: the ordinary
# stream is answered whether or not the other one ever is.
def case_mixed(conn):
    hung = conn.send_headers('GET', '/', end_stream=False)
    normal = conn.send_headers('GET', '/', end_stream=True)
    ns, _ = conn.read_response(normal)
    conn.send_data(hung, b'', end_stream=True)
    hs, _ = conn.read_response(hung)
    check('empty-DATA stream alongside a normal one',
          ns == 200 and hs == 200, 'normal %s, empty-DATA %s' % (ns, hs))


# ---------------------------------------------------------------- case 7
# A body over Http2MaxRequestBodySize, ending on an empty DATA frame.  The two
# halves of refusal have to agree: the buffering side stops appending and resets
# the stream, and the completion side declines to run it.  RST_STREAM does not
# recall the DATA already on the wire, so the frames keep coming and the last of
# them carries the END_STREAM that now completes a request -- dispatching on it
# would hand a SAF a body short by everything that was dropped and answer a
# stream the peer has been told is dead.
def case_oversize_body(conn):
    sid = conn.send_headers('POST', '/cgi-bin/echo.sh', end_stream=False)
    conn.send_data(sid, b'x' * (BODY_LIMIT * 4), end_stream=False)
    conn.send_data(sid, b'', end_stream=True)
    st = conn.settle(sid)
    check('oversize body refused, not dispatched',
          st['reset'] == ENHANCE_YOUR_CALM and st['status'] is None,
          'reset %s, status %s, %d body bytes'
          % (st['reset'], st['status'], len(st['body'])))


run_case('END_STREAM on HEADERS', case_headers)
run_case('END_STREAM on empty DATA', case_empty_data)
run_case('END_STREAM on DATA with body', case_body)
run_case('END_STREAM on empty DATA after body', case_body_then_empty_data)
run_case('%d multiplexed empty-DATA streams' % N_STREAMS, case_multiplexed)
run_case('empty-DATA stream alongside a normal one', case_mixed)
run_case('oversize body refused, not dispatched', case_oversize_body)

if failures:
    print('[h2-end] FAILED: %s' % ', '.join(failures))
    sys.exit(1)
print('[h2-end] ALL OK')
PY

# Nothing above should have provoked a server-side error.
if [ -f "${ERRLOG}" ]; then
    NEW=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" | grep -c "CATASTROPHE\|SIGSEGV\|assertion" || true)
    if [ "${NEW}" -ne 0 ]; then
        echo "Server logged ${NEW} fatal error(s) during the run:"
        tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" | grep "CATASTROPHE\|SIGSEGV\|assertion"
        exit 1
    fi
fi

echo "=== HTTP/2 request-completion test passed ==="
