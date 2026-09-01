#!/bin/bash
#
# HTTP/2 concurrent stream execution test.
#
# h2_lifecycle_smoke.sh proves 20 streams on one connection all get *answered*.
# It cannot tell whether they were answered concurrently or one after another,
# because a static file is served too fast to time.  This test proves the
# concurrency: streams whose work takes real time overlap instead of queueing
# behind each other.
#
# The slow work is a CGI that sleeps.  Sleeping is exactly the shape of the
# problem being tested -- a request that occupies its pipeline without consuming
# CPU -- and it makes the difference between serial and parallel dispatch large
# enough to measure without being sensitive to how fast the machine is.
#
# What each check would look like if streams were dispatched serially, i.e. if
# the SAF pipeline still ran inside the nghttp2 callback on the thread driving
# the connection:
#
#   single slow stream        - passes either way; it is the calibration run.
#   N slow streams            - takes N x as long, so the ratio check fails.
#   fast stream behind slow   - the fast stream waits for the slow one, so the
#                               head-of-line check fails.
#   reset while in flight     - the reset could not arrive mid-pipeline at all,
#                               so this only tests anything once dispatch is
#                               concurrent: the stream is freed while a worker
#                               is still running it.
#   disconnect while in flight- likewise: teardown has to wait for workers that
#                               are still executing streams of a session that is
#                               already finished.
#
# Reuses the https-test instance created by `make install`.  The original config
# is saved before the test and restored afterwards.
#
# Usage:
#   test/h2_parallel_streams_smoke.sh <heliod-work-root>
# Example:
#   test/h2_parallel_streams_smoke.sh /tmp/heliod-build/work/Linux_debian_f_x86_64_32_OPT

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
HTTPS_PORT=18443
TEMP_DIR=""
CGI_DIR=""
SERVER_PID=""

# The worker pool is pinned small on purpose.  Concurrency has to be visible
# with a pool this size, and a pool that is smaller than the stream count is
# what makes the queueing behaviour matter.
MAX_THREADS=8

# Streams per connection for the concurrency check, and how long each one's CGI
# sleeps.  N_STREAMS is deliberately equal to the pool size: one more and the
# expected time would be two batches, which makes the ratio check muddier.
N_STREAMS=8
SLOW_SECS=2

# Head-of-line check: one long stream, one static request behind it.
HOL_SECS=4

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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2par-XXXXXX)
chmod 700 "${TEMP_DIR}"

# The CGI lives outside the docroot and is reached through its own pfx2dir, so
# nothing in this test changes how the document tree is served.  It gets its own
# directory rather than a subdirectory of TEMP_DIR: the server refuses to start
# if its temp-path is accessible to anyone else, so TEMP_DIR has to stay 0700.
CGI_DIR=$(mktemp -d /tmp/${INSTANCE}-h2cgi-XXXXXX)
chmod 755 "${CGI_DIR}"
cat > "${CGI_DIR}/slow.sh" <<'CGI'
#!/bin/sh
# Sleep for QUERY_STRING seconds, then answer.  Occupying the pipeline without
# burning CPU is the point: it isolates dispatch concurrency from throughput.
sleep "${QUERY_STRING:-1}"
printf 'Content-type: text/plain\r\n'
printf '\r\n'
echo "slept ${QUERY_STRING:-1}"
CGI
chmod 755 "${CGI_DIR}/slow.sh"

# Create a self-signed NSS certificate for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/h2-par-key.pem \
    -out /tmp/h2-par-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/h2-par.p12 \
    -inkey /tmp/h2-par-key.pem -in /tmp/h2-par-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/h2-par.p12 -d dbm:"." -w "${NO_PASS}"
echo "NSSDir ${PWD}" >> magnus.conf
cd "${WORK_ROOT}"

# Same instance shape as h2_lifecycle_smoke.sh: event I/O on both listeners, a
# TLS listener for ALPN, and a pinned thread pool.
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

# Route /cgi-bin at the sleeping script.  The stock obj.conf already has the
# "cgi" object that forces the CGI type and calls send-cgi; all that is missing
# is a NameTrans pointing at a directory.  It goes ahead of uri-clean so it is
# reached before the document-tree path checks.
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
        if ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start listening on HTTPS port ${HTTPS_PORT}"
    cat /tmp/${INSTANCE}-h2par.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"

configure_instance
add_cgi_mapping
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2par.log 2>&1 &
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
PORT = ${HTTPS_PORT}
MAX_THREADS = ${MAX_THREADS}
N_STREAMS = ${N_STREAMS}
SLOW_SECS = ${SLOW_SECS}
HOL_SECS = ${HOL_SECS}

SLOW_PATH = '/cgi-bin/slow.sh?%d' % SLOW_SECS
HOL_PATH = '/cgi-bin/slow.sh?%d' % HOL_SECS
FAST_PATH = '/'

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'

STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def encode_headers(path):
    """Minimal HPACK: indexed :method/:scheme, literal :path and :authority.

    :path is a literal *without* indexing (prefix 0x04, name index 4) rather
    than the indexed 0x84 the other tests use, because these requests do not
    all ask for '/'.  Without indexing keeps the server's dynamic table from
    growing one entry per request, which would otherwise change what is being
    measured as the test runs.
    """
    out = bytearray([0x82, 0x87])            # :method GET, :scheme https
    p = path.encode()
    out += bytes([0x04, len(p)]) + p         # :path
    out += bytes([0x41, 0x09]) + b'localhost'  # :authority
    return bytes(out)


class H2Conn(object):
    """Minimal HTTP/2 client, demultiplexing frames into per-stream state.

    Same shape as the one in h2_lifecycle_smoke.sh: a multiplexing server is
    free to interleave streams, so frames have to be folded into per-stream
    state as they arrive rather than matched against one expected stream.
    """

    def __init__(self, timeout=60):
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
                             'reset': None}
        hdrs = encode_headers(path)
        # END_STREAM | END_HEADERS
        self.sock.sendall(frame_header(len(hdrs), 0x1, 0x5, sid) + hdrs)
        return sid

    def send_rst(self, sid, code=0x8):        # CANCEL
        self.sock.sendall(frame_header(4, 0x3, 0x0, sid) +
                          struct.pack('>I', code))

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
        st = self.streams[sid]
        for _ in range(4000):
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
        print('[h2-parallel] %s: OK%s' % (label, detail))
    else:
        print('[h2-parallel] %s: FAIL%s' % (label, detail))
        failures.append(label)


# ---------------------------------------------------------------------------
# Test 1 -- calibration: one slow stream on its own.
#
# Establishes that the CGI works at all and gives every later ratio a baseline
# measured on this machine, under this load, rather than a hardcoded number
# that a slow or busy host would trip over.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
check('ALPN h2', conn.alpn == 'h2', ' (%s)' % conn.alpn)

t0 = time.time()
sid = conn.send_request(SLOW_PATH)
try:
    status, body = conn.read_response(sid)
except Exception as exc:
    status, body = None, 0
    print('[h2-parallel] single slow stream raised %r' % (exc,))
single = time.time() - t0
check('single %ds CGI stream' % SLOW_SECS,
      status == 200 and body > 0 and single >= SLOW_SECS * 0.5,
      ' (status=%s body=%d elapsed=%.1fs)' % (status, body, single))
conn.close()

if failures:
    # Nothing below can mean anything if a single slow request does not work,
    # and a broken baseline would make the ratio checks report nonsense.
    print('h2 parallel streams test FAILED: %s' % ', '.join(failures))
    sys.exit(1)

# ---------------------------------------------------------------------------
# Test 2 -- N slow streams on one connection run concurrently.
#
# The measurement the whole test exists for.  N streams that each occupy their
# pipeline for SLOW_SECS are submitted back-to-back on a single connection.
# Served concurrently they finish in about the time of one; served one after
# another they take N times as long.
#
# The bar is 3x the calibrated single-stream time, against an expected 8x for
# serial dispatch.  That is a wide gap on purpose: the point is to catch a
# regression to serial execution, not to police scheduling overhead.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()

t0 = time.time()
sids = [conn.send_request(SLOW_PATH) for _ in range(N_STREAMS)]
got = 0
err = ''
for s in sids:
    try:
        status, body = conn.read_response(s)
        if status == 200 and body > 0:
            got += 1
        else:
            err = ' (stream %d: status=%s body=%d)' % (s, status, body)
    except Exception as exc:
        err = ' %r' % (exc,)
        break
concurrent = time.time() - t0
budget = single * 3.0
serial = single * N_STREAMS

check('%d x %ds streams on one connection all answered'
      % (N_STREAMS, SLOW_SECS),
      got == N_STREAMS, ' (completed=%d/%d%s)' % (got, N_STREAMS, err))
check('%d slow streams overlap instead of queueing' % N_STREAMS,
      got == N_STREAMS and concurrent < budget,
      ' (elapsed=%.1fs budget=%.1fs serial_would_be=%.1fs)'
      % (concurrent, budget, serial))
conn.close()

# ---------------------------------------------------------------------------
# Test 3 -- a fast stream is not stuck behind a slow one.
#
# Head-of-line blocking within a connection, which is the user-visible symptom
# of serial dispatch and the thing HTTP/2 multiplexing is supposed to remove.
# A long CGI is started first, then a static request on the same connection;
# the static one has to come back long before the CGI does.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()

t0 = time.time()
slow_sid = conn.send_request(HOL_PATH)
fast_sid = conn.send_request(FAST_PATH)
fast_status, fast_body, fast_err = None, 0, ''
try:
    fast_status, fast_body = conn.read_response(fast_sid)
except Exception as exc:
    fast_err = ' %r' % (exc,)
fast_elapsed = time.time() - t0

# Half the slow stream's own duration: comfortably longer than a static file
# takes, comfortably shorter than waiting for the CGI.
hol_budget = HOL_SECS * 0.5
check('static stream answered while a %ds stream runs' % HOL_SECS,
      fast_status == 200 and fast_body > 0 and fast_elapsed < hol_budget,
      ' (status=%s body=%d elapsed=%.1fs budget=%.1fs%s)'
      % (fast_status, fast_body, fast_elapsed, hol_budget, fast_err))

# The slow stream still has to finish: overtaking it must not mean losing it.
slow_status, slow_err = None, ''
try:
    slow_status, _ = conn.read_response(slow_sid)
except Exception as exc:
    slow_err = ' %r' % (exc,)
check('overtaken %ds stream still completes' % HOL_SECS,
      slow_status == 200, ' (status=%s%s)' % (slow_status, slow_err))
conn.close()

# ---------------------------------------------------------------------------
# Test 4 -- RST_STREAM while a stream is mid-pipeline.
#
# Only reachable once streams run on their own threads: the reset arrives while
# a worker is still executing the stream, so the stream is closed out from under
# it.  The worker owns the free in that case.  What must be true afterwards is
# that the rest of the connection is unharmed.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()

reset_sid = conn.send_request(SLOW_PATH)
other_sids = [conn.send_request(SLOW_PATH) for _ in range(3)]
time.sleep(0.3)                      # let the pipeline actually get going
conn.send_rst(reset_sid)

survived = 0
rst_err = ''
for s in other_sids:
    try:
        status, body = conn.read_response(s)
        if status == 200 and body > 0:
            survived += 1
    except Exception as exc:
        rst_err = ' %r' % (exc,)
        break
check('streams survive a peer reset of a stream in flight',
      survived == len(other_sids),
      ' (completed=%d/%d%s)' % (survived, len(other_sids), rst_err))

# And the connection itself is still usable afterwards.
reuse_status, reuse_err = None, ''
try:
    s = conn.send_request(FAST_PATH)
    reuse_status, _ = conn.read_response(s)
except Exception as exc:
    reuse_err = ' %r' % (exc,)
check('connection still serves after a mid-flight reset',
      reuse_status == 200, ' (status=%s%s)' % (reuse_status, reuse_err))
conn.close()

# ---------------------------------------------------------------------------
# Test 5 -- peer disconnects while streams are in flight.
#
# Teardown cannot free the session while workers are still inside its streams,
# so the close has to be deferred until they report back.  Getting that wrong
# is a use-after-free, which shows up as a crash rather than a failed
# assertion -- hence the health check afterwards and the error-log scan at the
# end of the script.
# ---------------------------------------------------------------------------
conn = H2Conn()
conn.await_settings()
for _ in range(4):
    conn.send_request(SLOW_PATH)
time.sleep(0.3)
conn.close()                          # vanish mid-pipeline

# Long enough for those workers to finish and find their session going away.
time.sleep(SLOW_SECS + 2)

health_status, health_err = None, ''
try:
    probe = H2Conn()
    probe.await_settings()
    s = probe.send_request(FAST_PATH)
    health_status, _ = probe.read_response(s)
    probe.close()
except Exception as exc:
    health_err = ' %r' % (exc,)
check('server healthy after a disconnect with streams in flight',
      health_status == 200, ' (status=%s%s)' % (health_status, health_err))

if failures:
    print('h2 parallel streams test FAILED: %s' % ', '.join(failures))
    sys.exit(1)
print('h2 parallel streams test passed.')
PY

# Fail the test if this run logged a crash or an assertion failure.  Test 5 in
# particular fails this way rather than by returning a wrong answer.
if [ -f "${ERRLOG}" ]; then
    NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                 grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
    if [ -n "${NEW_ERRORS}" ]; then
        echo "[h2-parallel] server error log reports a crash or assertion:"
        echo "${NEW_ERRORS}"
        exit 1
    fi
fi
echo "[h2-parallel] no crashes or assertions in server error log"
