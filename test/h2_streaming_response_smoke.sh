#!/bin/bash
#
# HTTP/2 streaming response test.
#
# The other h2 tests all ask for small responses, so they pass whether the
# response body is streamed to the client as it is produced or accumulated whole
# in memory first.  This test distinguishes the two.
#
# The discriminator is the peer's flow-control window.  A client that reads one
# window's worth of a large response and then stops sending WINDOW_UPDATE gives
# the server nowhere to put the rest of the body.  What the server does then is
# the whole question:
#
#   buffered   the SAF pipeline runs to completion regardless, because nothing
#              back-pressures it, so the entire body lands in the server's
#              address space.  For a 64 MB response that is a 64 MB spike in
#              RSS, and it happens whether or not the client ever reads a byte.
#   streamed   the pipeline fills a small fixed-size ring, blocks, and waits.
#              RSS does not move.
#
# Peak RSS is read from /proc/<pid>/status VmHWM rather than sampled from VmRSS,
# so the check cannot miss a spike by looking at the wrong moment.
#
# The rest of the checks cover what streaming has to not break:
#
#   time to first byte     a response is submittable as soon as its headers have
#                          been parsed, so the client sees them long before the
#                          body is finished.  Buffered, nothing arrives until the
#                          pipeline exits.
#   large body, exact      streaming splits the body across ring wraps, DATA
#                          frames and flow-control stalls.  Every byte still has
#                          to arrive, once, in order.  Checked by position, not
#                          just by length, so a duplicated or reordered ring
#                          segment fails.
#   chunked at scale       a CGI response has no content-length, so it arrives
#                          chunk-encoded and is de-chunked in transit by an
#                          incremental decoder.  A large one crosses ring
#                          boundaries mid-chunk, which is what the decoder's
#                          resumability is for.
#   producers unwind       a blocked producer holds a worker thread.  If the
#                          client vanishes, that worker has to be released --
#                          not left waiting out its I/O timeout -- or a handful
#                          of abandoned downloads would exhaust the pool.
#
# Reuses the https-test instance created by `make install`.  The original config
# is saved before the test and restored afterwards.
#
# Usage:
#   test/h2_streaming_response_smoke.sh <heliod-work-root>
# Example:
#   test/h2_streaming_response_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

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
DOCROOT="${INSTANCE_DIR}/docs"
HTTP_PORT=18180
HTTPS_PORT=18443
TEMP_DIR=""
CGI_DIR=""
SERVER_PID=""
BIG_FILE=""

# Small enough to build and transfer quickly, large enough that buffering it
# whole is unmistakable next to any other allocation the server makes.
BIG_MB=64

# Pinned small, as in the other h2 tests: a blocked producer occupies a worker,
# and the point of the last check is that the pool recovers.
MAX_THREADS=8

# How long the trickling CGI keeps writing.  Long enough that the test can watch
# it mid-response with seconds to spare either side.
DRIP_SECS=6

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
    if [ -n "${BIG_FILE:-}" ] && [ -f "${BIG_FILE}" ]; then
        rm -f "${BIG_FILE}"
    fi
    if [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ]; then
        rm -rf "${TEMP_DIR}"
    fi
    if [ -n "${CGI_DIR:-}" ] && [ -d "${CGI_DIR}" ]; then
        rm -rf "${CGI_DIR}"
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

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2str-XXXXXX)
chmod 700 "${TEMP_DIR}"

# The body is a repeating 0..255 ramp so the client can verify it by position.
# Length alone would not catch a ring segment that was handed out twice or out of
# order, which is exactly the class of bug a wrapping buffer invites.
BIG_FILE="${DOCROOT}/h2-stream-big.bin"
python3 - "${BIG_FILE}" "${BIG_MB}" <<'PY'
import sys
path, mb = sys.argv[1], int(sys.argv[2])
ramp = bytes(range(256)) * 256          # 64 KiB, and 64 KiB % 256 == 0, so the
                                        # pattern stays aligned across writes
with open(path, 'wb') as f:
    for _ in range(mb * 16):
        f.write(ramp)
PY
chmod 644 "${BIG_FILE}"

# CGIs live outside the docroot and are reached through their own pfx2dir, so
# nothing here changes how the document tree is served.  Their own directory,
# not a subdirectory of TEMP_DIR: the server refuses to start if its temp-path is
# accessible to anyone else, so TEMP_DIR has to stay 0700.
CGI_DIR=$(mktemp -d /tmp/${INSTANCE}-h2scgi-XXXXXX)
chmod 755 "${CGI_DIR}"

# A large body with no content-length, so the server has to chunk it and the h2
# layer has to de-chunk it on the way past.  Same ramp as the static file.
cat > "${CGI_DIR}/big.py" <<'CGI'
#!/usr/bin/env python3
import os
import sys

want = int(os.environ.get('QUERY_STRING') or '0')
out = sys.stdout.buffer
out.write(b'Content-type: application/octet-stream\r\n\r\n')
# 64 KiB of a 0..255 ramp.  Its length is a multiple of 256, so writing whole
# buffers keeps the pattern aligned to the absolute offset and the client can
# check any byte from its position alone.
ramp = bytes(range(256)) * 256
sent = 0
while sent < want:
    n = min(len(ramp), want - sent)
    out.write(ramp[:n])
    sent += n
out.flush()
CGI
chmod 755 "${CGI_DIR}/big.py"

# A trickling response: one short line every quarter second for QUERY_STRING
# seconds.  The shape of an SSE or long-poll endpoint, and the case a streaming
# transport exists for -- a client watching this must see the lines as they are
# written, not all of them when the program exits.
cat > "${CGI_DIR}/drip.py" <<'CGI'
#!/usr/bin/env python3
import os
import sys
import time

secs = float(os.environ.get('QUERY_STRING') or '6')
step = 0.25
out = sys.stdout.buffer
# Flush after the headers and after every line, the way a real streaming
# producer does.  Without the flushes Python's BufferedWriter would coalesce
# them and the test would be measuring itself.
out.write(b'Content-type: text/plain\r\n\r\n')
out.flush()
deadline = time.time() + secs
n = 0
while time.time() < deadline:
    out.write(b'line %04d\n' % n)
    out.flush()
    n += 1
    time.sleep(step)
CGI
chmod 755 "${CGI_DIR}/drip.py"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/h2-str-key.pem \
    -out /tmp/h2-str-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/h2-str.p12 \
    -inkey /tmp/h2-str-key.pem -in /tmp/h2-str-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/h2-str.p12 -d dbm:"." -w "${NO_PASS}"
echo "NSSDir ${PWD}" >> magnus.conf
# Cap the HTTP/2 stream pool for Test 6, which checks that the excess is refused
# with REFUSED_STREAM.  32 is well above the handful of concurrent streams tests
# 1-5 ever open, so it is inert for them; the default would self-scale to a few
# thousand, far more than a single connection could open past nghttp2's own
# per-connection limit.
echo "Http2MaxStreams 32" >> magnus.conf
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

for existing in root.findall("thread-pool"):
    root.remove(existing)
pool = ET.SubElement(root, "thread-pool")
ET.SubElement(pool, "min-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "max-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "queue-size").text = "512"

vs = root.find("virtual-server")
vs_listeners = set(el.text for el in vs.findall("http-listener-name"))

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

# Drop any /cgi-bin mapping the instance already carries.  NameTrans rules are
# applied in order, so an existing one placed higher up wins and every CGI here
# 404s -- which the assertions below report as a streaming failure rather than as
# the misconfiguration it is.
lines = [l for l in text.splitlines(True)
         if not (l.lstrip().startswith('NameTrans fn="pfx2dir"') and
                 'from="/cgi-bin"' in l)]
text = ''.join(lines)

text = text.replace(anchor, mapping + anchor, 1)

# Take the HTTP filter's flush timer out of the picture for these CGIs.
#
# This matters for what the tests are allowed to assert.  The filter buffers
# small response-body writes in its own buffer and only hands them down to the
# transport when that buffer fills or when flushTimer (default 3s) has elapsed
# -- and send-cgi switches that buffering back on explicitly, so
# UseOutputStreamSize cannot be used to defeat it.  With the default timer, a
# CGI that trickles delivers nothing for the first three seconds over *any*
# transport, so a time-to-first-byte check would be measuring the filter rather
# than the HTTP/2 layer.  With the timer at zero the filter hands each write
# down as it is made, and the HTTP/2 path is then held to the same standard as
# HTTP/1.1: what the SAF writes, the client gets.
service = 'Service fn="send-cgi"'
if service not in text:
    raise SystemExit('obj.conf: no send-cgi Service to configure')
text = text.replace(service, service + ' flushTimer="0"', 1)

open(path, 'w').write(text)
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
    cat /tmp/${INSTANCE}-h2str.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

configure_instance
add_cgi_mapping
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2str.log 2>&1 &
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
INSTANCE_DIR = "${INSTANCE_DIR}"
BIG_BYTES = ${BIG_MB} * 1024 * 1024
MAX_THREADS = ${MAX_THREADS}
DRIP_SECS = ${DRIP_SECS}

BIG_STATIC = '/h2-stream-big.bin'
BIG_CGI = '/cgi-bin/big.py?%d' % BIG_BYTES
DRIP_PATH = '/cgi-bin/drip.py?%d' % DRIP_SECS
FAST_PATH = '/'

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'

# The window both peers start with, per RFC 7540, because this client sends an
# empty SETTINGS frame and so accepts every default.  It is also exactly how much
# of a response a client that never sends WINDOW_UPDATE can be given.
DEFAULT_WINDOW = 65535

STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def encode_headers(path):
    """Minimal HPACK: indexed :method/:scheme, literal :path and :authority."""
    out = bytearray([0x82, 0x87])              # :method GET, :scheme https
    p = path.encode()
    out += bytes([0x04, len(p)]) + p           # :path, literal, no indexing
    out += bytes([0x41, 0x09]) + b'localhost'  # :authority
    return bytes(out)


# --------------------------------------------------------------------------
# Server process introspection.
#
# Peak RSS, not current RSS: the kernel keeps a high-water mark, so a spike that
# has already been freed is still visible.  Sampling VmRSS instead would make the
# memory check a race against whatever moment the sample was taken.
# --------------------------------------------------------------------------
def server_pid():
    for entry in os.listdir('/proc'):
        if not entry.isdigit():
            continue
        try:
            with open('/proc/%s/cmdline' % entry, 'rb') as f:
                cmd = f.read().replace(b'\0', b' ').decode('utf-8', 'replace')
        except (IOError, OSError):
            continue
        if 'webservd' in cmd and 'wdog' not in cmd and INSTANCE_DIR in cmd:
            return int(entry)
    return None


def proc_kb(pid, field):
    try:
        with open('/proc/%d/status' % pid) as f:
            for line in f:
                if line.startswith(field + ':'):
                    return int(line.split()[1])
    except (IOError, OSError):
        pass
    return None


class H2Conn(object):
    """Minimal HTTP/2 client with explicit flow-control accounting.

    Unlike the clients in the other h2 tests this one does not automatically
    replenish the receive window.  Withholding WINDOW_UPDATE is the mechanism the
    memory checks rely on, so the window is managed by hand: credit() gives some
    back, and without it a response stops after DEFAULT_WINDOW bytes.
    """

    def __init__(self, timeout=120, verify=False):
        ctx = ssl._create_unverified_context()
        ctx.set_alpn_protocols(['h2'])
        raw = socket.create_connection((HOST, PORT), timeout=timeout)
        self.sock = ctx.wrap_socket(raw, server_hostname=HOST)
        self.sock.settimeout(timeout)
        self.alpn = self.sock.selected_alpn_protocol()
        self.next_stream_id = 1
        self.streams = {}
        self.verify = verify
        self.goaway = None
        self.sock.sendall(PREFACE)
        self.sock.sendall(frame_header(0, 0x4, 0x0, 0))   # empty SETTINGS

    def _recv_exactly(self, n):
        # A timeout is only allowed to escape at a frame boundary.  Abandoning a
        # half-read frame would desynchronize the connection, and the stall checks
        # below deliberately read with a short timeout, so this matters: they must
        # be able to stop reading without breaking the reads that follow.
        buf = b''
        stalls = 0
        while len(buf) < n:
            try:
                chunk = self.sock.recv(n - len(buf))
            except socket.timeout:
                if not buf:
                    raise
                stalls += 1
                if stalls > 20:
                    raise AssertionError('server stopped mid-frame after %d of '
                                         '%d bytes' % (len(buf), n))
                continue
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

    def send_request(self, path):
        sid = self.next_stream_id
        self.next_stream_id += 2
        self.streams[sid] = {'status': None, 'body': 0, 'done': False,
                             'reset': None, 'first_data': None, 'bad_at': None,
                             'headers_at': None}
        hdrs = encode_headers(path)
        self.sock.sendall(frame_header(len(hdrs), 0x1, 0x5, sid) + hdrs)
        return sid

    def credit(self, sid, amount):
        # Hand 'amount' of receive window back, at both levels.  (Comment, not a
        # docstring: this whole block is an unquoted heredoc, so the shell would
        # try to run anything in backticks and Python never sees the quotes.)
        self.credit_conn(amount)
        inc = struct.pack('>I', amount)
        self.sock.sendall(frame_header(4, 0x8, 0x0, sid) + inc)

    def credit_conn(self, amount):
        # The connection level only, leaving the stream windows alone.  A response
        # needs room at both levels, so this is how one stream is held at its
        # window without stopping the whole connection: a real client does the
        # same when it wants to throttle one download and not the rest.
        inc = struct.pack('>I', amount)
        self.sock.sendall(frame_header(4, 0x8, 0x0, 0) + inc)

    def _pump_one_frame(self):
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
            if st['headers_at'] is None:
                st['headers_at'] = time.time()
            if st['status'] is None and payload:
                st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x0:                                # DATA
            if payload and st['first_data'] is None:
                st['first_data'] = time.time()
            if self.verify and st['bad_at'] is None:
                # The body is a 0..255 ramp, so every byte is determined by its
                # offset.  Checking it here rather than accumulating the whole
                # body keeps the client's own memory use out of the picture.
                base = st['body']
                for i, byte in enumerate(payload):
                    if byte != (base + i) % 256:
                        st['bad_at'] = base + i
                        break
            st['body'] += len(payload)
        elif ftype == 0x3:                                # RST_STREAM
            st['reset'] = (struct.unpack('>I', payload[:4])[0]
                           if len(payload) >= 4 else -1)
            st['done'] = True
            return

        if flags & 0x1:                                   # END_STREAM
            st['done'] = True

    def read_until_stalled(self, sid, quiet=1.0):
        """Read until the server stops sending for 'quiet' seconds.

        Used where the expected outcome is a flow-control stall, which has no
        frame of its own: the server simply goes quiet.
        """
        st = self.streams[sid]
        old = self.sock.gettimeout()
        self.sock.settimeout(quiet)
        try:
            while not st['done']:
                try:
                    self._pump_one_frame()
                except (socket.timeout, ssl.SSLWantReadError):
                    break
        finally:
            self.sock.settimeout(old)
        return st

    def read_for(self, sid, secs, slice_secs=0.5):
        """Read for 'secs' of wall clock, then stop wherever we are.

        For a producer that trickles: read_until_stalled() would return on the
        first gap longer than its quiet period, and a gap is the normal state of a
        response that writes a line every quarter second.  What matters here is
        how much has arrived by a fixed deadline, so read to the clock instead and
        treat a timeout as 'nothing right now', not as the end.
        """
        st = self.streams[sid]
        old = self.sock.gettimeout()
        deadline = time.time() + secs
        try:
            while not st['done'] and time.time() < deadline:
                self.sock.settimeout(min(slice_secs, max(0.05,
                                                         deadline - time.time())))
                try:
                    self._pump_one_frame()
                except (socket.timeout, ssl.SSLWantReadError):
                    continue
        finally:
            self.sock.settimeout(old)
        return st

    def drain(self, sid, credit_every=32768, limit_secs=180):
        """Read a stream to END_STREAM, replenishing the window as we go.

        credit_every has to stay comfortably under DEFAULT_WINDOW: crediting in
        larger steps than the window would let the window close while this loop
        was still waiting to reach its step size, and then both ends would wait
        for each other.
        """
        st = self.streams[sid]
        deadline = time.time() + limit_secs
        given = 0
        while not st['done']:
            if time.time() > deadline:
                raise AssertionError('stream %d did not finish in %ds (got %d)'
                                     % (sid, limit_secs, st['body']))
            if st['body'] - given >= credit_every:
                self.credit(sid, st['body'] - given)
                given = st['body']
            self._pump_one_frame()
        if st['reset'] is not None:
            raise AssertionError('stream %d reset, code %d' % (sid, st['reset']))
        return st

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


failures = []


def check(label, condition, detail=''):
    if condition:
        print('[h2-stream] %s: OK%s' % (label, detail))
    else:
        print('[h2-stream] %s: FAIL%s' % (label, detail))
        failures.append(label)


pid = server_pid()
if pid is None:
    print('[h2-stream] could not find the webservd process for %s' % INSTANCE_DIR)
    sys.exit(1)

# Warm up: one ordinary request, so that whatever the server allocates on its
# first request through each path is already accounted for in the baseline.
warm = H2Conn()
warm.await_settings()
check('ALPN h2', warm.alpn == 'h2', ' (%s)' % warm.alpn)
warm_sid = warm.send_request(FAST_PATH)
warm.drain(warm_sid)
warm.close()

# ---------------------------------------------------------------------------
# Test 1 -- a large static response does not materialize in memory.
#
# Ask for BIG_BYTES, read the one window the server is allowed to send, then go
# quiet.  A server that streams stops there, having spent a fixed-size buffer.  A
# server that buffers has already read the whole file into its address space by
# the time the first byte reaches the client, and its peak RSS says so.
# ---------------------------------------------------------------------------
base_hwm = proc_kb(pid, 'VmHWM')
base_rss = proc_kb(pid, 'VmRSS')

stalled = H2Conn()
stalled.await_settings()
big_sid = stalled.send_request(BIG_STATIC)
st = stalled.read_until_stalled(big_sid, quiet=3.0)

check('large static response starts before it is finished',
      st['status'] == 200 and st['body'] > 0,
      ' (status=%s bytes=%d)' % (st['status'], st['body']))
check('stalled client is held at its flow-control window',
      not st['done'] and st['body'] <= DEFAULT_WINDOW,
      ' (bytes=%d window=%d done=%s)'
      % (st['body'], DEFAULT_WINDOW, st['done']))

# Give the pipeline every chance to run away with the file before measuring.
time.sleep(2)
stall_hwm = proc_kb(pid, 'VmHWM')
stall_rss = proc_kb(pid, 'VmRSS')

# The bar is a quarter of the response.  A streamed response should not move RSS
# measurably at all; the margin is there so that unrelated allocation on the
# request path cannot fail the test, while a buffered body -- which costs the
# whole BIG_BYTES -- cannot pass it.
budget_kb = BIG_BYTES // 1024 // 4
grew_kb = (stall_hwm - base_hwm) if (stall_hwm and base_hwm) else -1
check('stalled %d MB static response does not buffer in memory'
      % (BIG_BYTES // 1024 // 1024),
      grew_kb >= 0 and grew_kb < budget_kb,
      ' (peak RSS +%d KB, budget %d KB, rss %d -> %d KB)'
      % (grew_kb, budget_kb, base_rss or -1, stall_rss or -1))

# ---------------------------------------------------------------------------
# Test 2 -- the stalled response resumes and delivers every byte, in order.
#
# The same connection and the same stream: opening the window has to restart a
# producer that has been parked at its ring's high-water mark for seconds, and
# what comes out has to be the rest of the file and nothing else.  Verified by
# position, so a ring segment handed out twice or out of order is caught.
# ---------------------------------------------------------------------------
stalled.verify = True
resumed = False
resume_err = ''
try:
    st = stalled.drain(big_sid)
    resumed = True
except Exception as exc:
    resume_err = ' %r' % (exc,)
stalled.close()

check('stalled response resumes when the window reopens',
      resumed and st['body'] == BIG_BYTES,
      ' (bytes=%d expected=%d%s)' % (st['body'], BIG_BYTES, resume_err))
# bad_at is only meaningful for the part read with verify on, i.e. everything
# after the stall, which is where a wrapping ring would go wrong.
check('resumed body arrives in order with no gaps or repeats',
      st['bad_at'] is None, ' (first bad offset=%s)' % (st['bad_at'],))

# ---------------------------------------------------------------------------
# Test 3 -- a trickling response is delivered as it is produced.
#
# The CGI writes a line every quarter second for DRIP_SECS seconds and flushes
# each one.  Streaming means the client has the headers and most of those lines
# while the CGI is still running; batching means it gets nothing until the CGI
# exits, which is what happens if the transport only moves body bytes when its
# buffer fills or when the pipeline finishes.
#
# The Service directive sets flushTimer="0" so the HTTP filter hands each write
# straight down -- see add_cgi_mapping().  Without that the filter, not the
# transport, would decide when these bytes moved.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
t0 = time.time()
drip_sid = conn.send_request(DRIP_PATH)

# Stop watching two seconds before the CGI is due to exit, so that everything
# seen here is unambiguously mid-response.  read_until_stalled() would return on
# the first quiet quarter-second, which is shorter than the drip interval, so
# read for a fixed span instead.
watch_secs = DRIP_SECS - 2.0
early = conn.read_for(drip_sid, watch_secs)
first_at = (early['first_data'] - t0) if early['first_data'] else None
hdr_at = (early['headers_at'] - t0) if early['headers_at'] else None
# Snapshot, because "early" is the live per-stream dict, not a copy: comparing
# early['body'] against it after draining would compare a value with itself.
mid_body = early['body']
mid_done = early['done']

# A budget rather than an exact figure: flushTimer="0" is compared against a
# one-second-granularity clock, so the filter can hold the first write for up to
# a second before it starts handing them down.
budget = 2.0
check('response headers arrive while the CGI is still running',
      hdr_at is not None and hdr_at < budget,
      ' (HEADERS at %s s, budget %.1f s)'
      % ('%.2f' % hdr_at if hdr_at is not None else 'never', budget))
check('first body bytes arrive while the CGI is still running',
      first_at is not None and first_at < budget,
      ' (first DATA at %s s, budget %.1f s)'
      % ('%.2f' % first_at if first_at is not None else 'never', budget))

# Most of the response, not just its start: a single early flush followed by
# silence would still be a batched response.  The CGI writes 10 bytes every
# 0.25s, so watch_secs of it is about 40 * watch_secs bytes; a third of that is a
# generous floor that still fails outright if delivery is deferred to the end.
expected = 40 * watch_secs
check('the response keeps arriving rather than only starting',
      mid_body > expected / 3.0 and not mid_done,
      ' (%d bytes in %.1f s, floor %d, done=%s)'
      % (mid_body, watch_secs, int(expected / 3.0), mid_done))

drip_ok, drip_err = False, ''
st = conn.streams[drip_sid]
try:
    st = conn.drain(drip_sid)
    drip_ok = (st['status'] == 200 and st['body'] > mid_body)
except Exception as exc:
    drip_err = ' %r' % (exc,)
check('trickling response completes',
      drip_ok, ' (status=%s bytes=%d%s)'
      % (st['status'], st['body'], drip_err))
conn.close()

# ---------------------------------------------------------------------------
# Test 4 -- a large chunked response survives de-chunking in transit.
#
# A CGI response has no content-length, so it is chunk-encoded, and the h2 layer
# strips that encoding as the bytes go past rather than after collecting them.
# At this size the decoder is repeatedly suspended mid-chunk when the ring fills,
# which is the case its resumability exists for.
# ---------------------------------------------------------------------------
conn = H2Conn(verify=True)
conn.await_settings()
t0 = time.time()
cgi_sid = conn.send_request(BIG_CGI)
cgi_ok, cgi_err = False, ''
try:
    st = conn.drain(cgi_sid)
    cgi_ok = True
except Exception as exc:
    st = conn.streams[cgi_sid]
    cgi_err = ' %r' % (exc,)
cgi_elapsed = time.time() - t0
first_at = ((st['first_data'] - t0) if st['first_data'] else None)

check('%d MB chunked CGI response delivered exactly'
      % (BIG_BYTES // 1024 // 1024),
      cgi_ok and st['body'] == BIG_BYTES and st['bad_at'] is None,
      ' (bytes=%d expected=%d bad_at=%s elapsed=%.1fs%s)'
      % (st['body'], BIG_BYTES, st['bad_at'], cgi_elapsed, cgi_err))
check('chunked response also starts before it is finished',
      first_at is not None and first_at < cgi_elapsed / 2.0,
      ' (first DATA at %s s of %.1f s)'
      % ('%.2f' % first_at if first_at is not None else 'never', cgi_elapsed))
conn.close()

# ---------------------------------------------------------------------------
# Test 5a -- a stalled producer does not wedge its own connection.
#
# One stalled download, so exactly one worker is parked at its ring's high-water
# mark and the rest of the pool is free.  A second request on the same connection
# then has to be served: the blocked producer holds only its own stream, and it
# keeps taking driver turns while it waits, so the session's other streams still
# make progress.
#
# One, not MAX_THREADS.  Every blocked producer costs a worker for as long as it
# is blocked -- that is the documented cost of this design -- so filling the pool
# with stalls and then asking for another request would be asserting that the
# server can run more concurrent work than it has threads.  That is test 5b's
# subject, and it tests recovery rather than service.
#
# The connection window has to be given back before the sibling request, and only
# the connection window.  A stalled download exhausts both levels, and while the
# connection window is closed the server is *right* to send nothing on any stream
# -- so without this the test would be asserting a flow-control violation.  With
# the connection window open and the stream window still shut, stream 1 stays
# parked exactly where it was and stream 2 has room to be served, which is the
# question actually being asked.
# ---------------------------------------------------------------------------
solo = H2Conn()
solo.await_settings()
solo_sid = solo.send_request(BIG_STATIC)
solo.read_until_stalled(solo_sid, quiet=1.0)
# A precondition, not a check -- test 1 already asserts this behaviour.  It is
# here because the sibling check below means nothing if the download it is meant
# to be competing with has already finished.
if solo.streams[solo_sid]['done']:
    raise AssertionError('the download meant to stall finished instead')

solo.credit_conn(solo.streams[solo_sid]['body'])

side_status, side_err, side_elapsed = None, '', 0.0
t0 = time.time()
try:
    side_sid = solo.send_request(FAST_PATH)
    side = solo.drain(side_sid, limit_secs=15)
    side_status = side['status']
except Exception as exc:
    side_err = ' %r' % (exc,)
side_elapsed = time.time() - t0

check('a stream is served on a connection with a stalled download',
      side_status == 200 and side_elapsed < 10.0,
      ' (status=%s elapsed=%.1fs%s)' % (side_status, side_elapsed, side_err))

# The download must still be stalled, or the check above proved nothing: a server
# that had finished sending it would have a free worker and an idle session, which
# is not the state under test.  Its stream window was never reopened, so the only
# way it could have finished is if the server ignored it.
check('the stalled download is still parked mid-response',
      not solo.streams[solo_sid]['done'],
      ' (%d of %d bytes)' % (solo.streams[solo_sid]['body'], BIG_BYTES))
solo.close()

# ---------------------------------------------------------------------------
# Test 5b -- blocked producers do not hold the pool when clients go away.
#
# MAX_THREADS stalled downloads on separate connections is the whole worker pool
# parked, which is legitimate while the clients are there.  What it must not do
# is stay parked after they leave: each producer has to notice its client is gone
# and unwind, rather than wait out its I/O timeout.  So all of them vanish at
# once and the server has to be serving again promptly, with no sleep in between
# -- the recovery probe is the measurement.
# ---------------------------------------------------------------------------
hogs = []
for _ in range(MAX_THREADS):
    c = H2Conn()
    c.await_settings()
    sid = c.send_request(BIG_STATIC)
    c.read_until_stalled(sid, quiet=1.0)
    hogs.append(c)

for c in hogs:
    c.close()                       # vanish, leaving MAX_THREADS producers blocked

t0 = time.time()
health_status, health_err = None, ''
try:
    probe = H2Conn(timeout=30)
    probe.await_settings()
    s = probe.send_request(FAST_PATH)
    probe.drain(s, limit_secs=25)
    health_status = probe.streams[s]['status']
    probe.close()
except Exception as exc:
    health_err = ' %r' % (exc,)
recovery = time.time() - t0

check('pool recovers after %d abandoned downloads' % MAX_THREADS,
      health_status == 200 and recovery < 15.0,
      ' (status=%s recovered in %.1fs%s)'
      % (health_status, recovery, health_err))

# ---------------------------------------------------------------------------
# Test 6 -- the stream pool caps total in-flight streams and refuses the excess.
#
# magnus.conf sets Http2MaxStreams=32 for this run.  Opening more than that many
# streams that cannot complete -- BIG_STATIC held at the flow window, so each one
# stays alive holding a pool slot -- must leave the first slots-worth accepted and
# reset the rest with REFUSED_STREAM (0x7).  The connection itself must stay up
# (no GOAWAY): refusing a stream is not tearing the connection down.  A pool slot
# is taken when the server parses a stream's HEADERS, before it is dispatched to a
# worker, so the cap bites regardless of the 8-thread pool -- the excess is
# refused, not merely queued.  Then, once the holder goes away, the slots are
# returned and the server serves again, which is what proves release() works.
#
# The assertions use ">=" for the refusal count: if an earlier test happened to
# leave a slot held, more get refused, not fewer, and that is still correct.
# ---------------------------------------------------------------------------
POOL_CAP = 32
OPEN_N = POOL_CAP + 12           # < nghttp2's own 100-stream per-connection limit

pool_conn = H2Conn(timeout=30)
pool_conn.await_settings()
opened = [pool_conn.send_request(BIG_STATIC) for _ in range(OPEN_N)]

# Read frames until the excess has been refused or a short deadline passes; the
# refusals are emitted as the server parses each over-cap HEADERS frame, so they
# come promptly, interleaved with the stalled streams' HEADERS/DATA.
deadline = time.time() + 15.0
old_to = pool_conn.sock.gettimeout()
try:
    while time.time() < deadline:
        refused_now = [s for s in opened if pool_conn.streams[s]['reset'] == 7]
        if len(refused_now) >= OPEN_N - POOL_CAP:
            break
        pool_conn.sock.settimeout(0.5)
        try:
            pool_conn._pump_one_frame()
        except (socket.timeout, ssl.SSLWantReadError):
            continue
finally:
    pool_conn.sock.settimeout(old_to)

refused = [s for s in opened if pool_conn.streams[s]['reset'] == 7]
other_reset = [s for s in opened
               if pool_conn.streams[s]['reset'] not in (None, 7)]
check('excess streams are refused with REFUSED_STREAM once the pool is full',
      len(refused) >= OPEN_N - POOL_CAP and not other_reset
      and pool_conn.goaway is None,
      ' (refused %d, expected >= %d, cap %d; other resets=%s; goaway=%r)'
      % (len(refused), OPEN_N - POOL_CAP, POOL_CAP, other_reset, pool_conn.goaway))

# Freeing the holder returns its slots to the pool; the server must then serve a
# fresh client again.  While the holder is still up the pool is legitimately full
# and other clients are refused too -- that is the point of a global cap -- so the
# recovery, not mid-hold service, is the correct thing to assert.
pool_conn.close()

t0 = time.time()
pool_health, pool_err = None, ''
try:
    probe = H2Conn(timeout=30)
    probe.await_settings()
    ps = probe.send_request(FAST_PATH)
    probe.drain(ps, limit_secs=20)
    pool_health = probe.streams[ps]['status']
    probe.close()
except Exception as exc:
    pool_err = ' %r' % (exc,)
pool_recovery = time.time() - t0
check('stream pool recovers after the holder releases its slots',
      pool_health == 200 and pool_recovery < 15.0,
      ' (status=%s recovered in %.1fs%s)'
      % (pool_health, pool_recovery, pool_err))

final_hwm = proc_kb(pid, 'VmHWM')
print('[h2-stream] peak RSS over the whole run: %d -> %d KB (+%d KB)'
      % (base_hwm or -1, final_hwm or -1,
         (final_hwm - base_hwm) if (final_hwm and base_hwm) else -1))

if failures:
    print('h2 streaming response test FAILED: %s' % ', '.join(failures))
    sys.exit(1)
print('h2 streaming response test passed.')
PY

# Fail the test if this run logged a crash or an assertion failure.
if [ -f "${ERRLOG}" ]; then
    NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                 grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
    if [ -n "${NEW_ERRORS}" ]; then
        echo "[h2-stream] server error log reports a crash or assertion:"
        echo "${NEW_ERRORS}"
        exit 1
    fi
fi
echo "[h2-stream] no crashes or assertions in server error log"
