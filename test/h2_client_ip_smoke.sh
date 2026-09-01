#!/bin/bash
#
# The client address perfdump reports for an HTTP/2 request.
#
# A connection's peer address reaches a thread's realtime stats slot in exactly
# one place: DaemonSession::prepareConnection() calls
# StatsSession::beginClient(&conn->remoteAddress).  An HTTP/2 stream never goes
# through prepareConnection() -- H2Stream::dispatchRequest() borrows a worker's
# persistent DaemonSession and hands it a synthetic per-stream Connection -- so
# nothing recorded the peer for the stream, and the /perfdump "Client" column
# printed whatever address that worker's slot happened to hold:
#
#   * 0.0.0.0 on a worker that had never prepared a connection of its own, and
#   * the peer of some unrelated earlier connection on one that had --
#     including, on a server behind a reverse proxy, an address that looks like
#     a real remote client but belongs to a connection that ended long ago.
#
# "Reported Client" was unaffected, because HandleRequest() sets it per request.
# Both columns are checked here: the fix records the peer *before* the request
# runs, and beginClient() clears the reported address, so an ordering mistake
# there would blank or stale the column that used to be right.
#
# Every connection is made from a different loopback source address (127.0.0.1,
# 127.0.0.2, 127.0.0.3), which is what makes the expected value knowable: the
# address perfdump prints has to be the one that particular connection came
# from, not merely a plausible-looking address.
#
# Speaks h2c with prior knowledge, so it needs no certificate.
#
# Usage: test/h2_client_ip_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
HTTP_PORT=18180
TEMP_DIR=""
CGI_DIR=""
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
    [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ] && rm -rf "${TEMP_DIR}"
    [ -n "${CGI_DIR:-}" ] && [ -d "${CGI_DIR}" ] && rm -rf "${CGI_DIR}"
}

trap cleanup_all EXIT

cd "${WORK_ROOT}"

"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2ip-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Occupies a worker long enough for a concurrent /perfdump to see it, and long
# enough for the address it stamped into that worker's slot to still be there
# when the next request lands on the same worker.
CGI_DIR=$(mktemp -d /tmp/${INSTANCE}-h2ipcgi-XXXXXX)
chmod 755 "${CGI_DIR}"
cat > "${CGI_DIR}/slow.sh" <<'CGI'
#!/bin/sh
printf 'Content-type: text/plain\r\n'
printf '\r\n'
printf 'start\n'
sleep 4
printf 'done\n'
CGI
chmod 755 "${CGI_DIR}/slow.sh"

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

# The shape this bug was found in: heliod behind a reverse proxy on loopback,
# reporting both the proxy (the peer) and the client the proxy names.
configure_reverse_proxy() {
    cat >> "${CONFIG_DIR}/magnus.conf" <<'MAGNUS'

ReverseProxyIPHeader X-Real-IP
ReverseProxyTrustedIPs 127.0.0.0/8
ReverseProxyLogAttribution both
MAGNUS
}

# /perfdump is the only way to read the realtime stats slots.  Both mappings go
# in ahead of every NameTrans already there, because whichever pfx2dir for a
# prefix comes first wins and an instance may well map some of these already --
# a shadowed mapping would silently serve 404s and take the test's concurrency
# with it.  The CGI prefix is deliberately not /cgi-bin for the same reason.
add_mappings() {
    python3 - <<PY
import os

path = os.path.join("${CONFIG_DIR}", "obj.conf")
lines = open(path).read().splitlines(True)
mappings = ['NameTrans fn="assign-name" from="/perfdump" name="h2ip-perf"\n',
            'NameTrans fn="pfx2dir" from="/h2ip-cgi" dir="${CGI_DIR}" '
            'name="cgi"\n']
for i, line in enumerate(lines):
    if line.startswith('NameTrans'):
        break
else:
    raise SystemExit('obj.conf: no NameTrans line to insert before')
text = ''.join(lines[:i] + mappings + lines[i:])
text += '\n<Object name="h2ip-perf">\nService fn="service-dump"\n</Object>\n'
open(path, 'w').write(text)
PY
}

wait_for_listener() {
    local waited=0
    while [ "${waited}" -lt 20 ]; do
        if curl -s -o /dev/null --max-time 3 "http://localhost:${HTTP_PORT}/" \
                2>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start serving on port ${HTTP_PORT}"
    cat /tmp/${INSTANCE}-h2ip.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"
configure_instance
configure_reverse_proxy
add_mappings

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2ip.log 2>&1 &
SERVER_PID=$!
wait_for_listener

python3 - <<PY
import socket
import struct
import sys
import threading
import time

PORT = ${HTTP_PORT}
TIMEOUT = 20
PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def hpack_str(s):
    b = s.encode()
    assert len(b) < 127
    return bytes([len(b)]) + b


def connect(source):
    """Connect to the listener from a chosen loopback source address."""
    sock = socket.socket()
    sock.settimeout(TIMEOUT)
    sock.bind((source, 0))
    sock.connect(('127.0.0.1', PORT))
    return sock


def h2c_get(path, source, extra=()):
    sock = connect(source)
    sock.sendall(PREFACE + frame_header(0, 0x4, 0x0, 0))

    hdrs = bytearray()
    hdrs += bytes([0x82, 0x86])                    # :method GET, :scheme http
    hdrs += b'\x04' + hpack_str(path)              # :path
    hdrs += b'\x01' + hpack_str('localhost')       # :authority
    for name, value in extra:
        hdrs += b'\x00' + hpack_str(name) + hpack_str(value)
    sock.sendall(frame_header(len(hdrs), 0x1, 0x4 | 0x1, 1) + bytes(hdrs))

    body = b''
    while True:
        try:
            hdr = b''
            while len(hdr) < 9:
                chunk = sock.recv(9 - len(hdr))
                if not chunk:
                    break
                hdr += chunk
            if len(hdr) < 9:
                break
            length = (hdr[0] << 16) | (hdr[1] << 8) | hdr[2]
            ftype, flags = hdr[3], hdr[4]
            sid = struct.unpack('>I', hdr[5:9])[0] & 0x7FFFFFFF
            payload = b''
            while len(payload) < length:
                chunk = sock.recv(length - len(payload))
                if not chunk:
                    break
                payload += chunk
            if ftype == 0x6 and not (flags & 0x1):
                sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
                continue
            if ftype == 0x7:
                raise AssertionError('server sent GOAWAY: %r' % payload[:32])
            if ftype == 0x3 and sid == 1:
                raise AssertionError('server reset the stream: %r' % payload[:8])
            if ftype == 0x0 and sid == 1:
                body += payload
            if (flags & 0x1) and sid == 1:
                break
        except socket.timeout:
            raise AssertionError('no response within %ds' % TIMEOUT)
    sock.close()
    return body.decode('utf-8', 'replace')


def http11_get(path, source, extra=()):
    sock = connect(source)
    request = 'GET %s HTTP/1.1\r\nHost: localhost\r\n' % path
    for name, value in extra:
        request += '%s: %s\r\n' % (name, value)
    request += 'Connection: close\r\n\r\n'
    sock.sendall(request.encode())
    data = b''
    while True:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        data += chunk
    sock.close()
    return data.decode('utf-8', 'replace')


LABELS = ['Process', 'Status', 'Client', 'Reported Client', 'Age', 'VS',
          'Protocol', 'Method', 'URI', 'Function']


def sessions(text):
    """Parse perfdump's Sessions table by the offsets of its column labels.

    The columns are padded to their widest value, and "Client" is blank in some
    configurations, so splitting on whitespace would silently shift the fields.
    """
    lines = text.splitlines()
    header = None
    seen_heading = False
    for line in lines:
        if line.startswith('Sessions:'):
            seen_heading = True
        elif seen_heading and line.lstrip().startswith('Process'):
            header = line
            break
    if header is None:
        return []

    offsets = []
    cursor = 0
    for label in LABELS:
        pos = header.index(label, cursor)
        offsets.append(pos)
        cursor = pos + len(label)
    offsets.append(len(header) + 1024)

    rows = []
    for line in lines[lines.index(header) + 1:]:
        if not line.strip():
            continue
        if not line[:1].isdigit():
            break
        rows.append(dict(zip(LABELS,
                             [line[offsets[i]:offsets[i + 1]].strip()
                              for i in range(len(LABELS))])))
    return rows


def perfdump_row(text):
    """The row for the /perfdump request that produced this dump."""
    for row in sessions(text):
        if row['URI'] == '/perfdump':
            return row
    return None


failures = []


def check(name, cond, detail=''):
    if cond:
        print('[h2-client-ip] %s: OK%s'
              % (name, (' (%s)' % detail) if detail else ''))
    else:
        print('[h2-client-ip] %s: FAIL %s' % (name, detail))
        failures.append(name)


def check_row(name, text, protocol, client, reported):
    row = perfdump_row(text)
    if row is None:
        check(name, False, 'no /perfdump row in the Sessions table')
        return
    detail = ('Protocol=%s Client=%r Reported Client=%r'
              % (row['Protocol'], row['Client'], row['Reported Client']))
    check(name,
          row['Protocol'] == protocol and row['Client'] == client and
          row['Reported Client'] == reported,
          detail)


# -------------------------------------------------------------------- case 1
# Control: the HTTP/1.1 path, which goes through prepareConnection() and has
# always recorded the peer.  A failure here means the harness is wrong.
check_row('HTTP/1.1 reports the connection peer',
          http11_get('/perfdump', '127.0.0.1'),
          'HTTP/1.1', '127.0.0.1', '')

# -------------------------------------------------------------------- case 2
# The regression, in its simplest form.  Before the fix this read 0.0.0.0
# whenever the stream landed on a worker that had never prepared a connection.
check_row('HTTP/2 reports the connection peer',
          h2c_get('/perfdump', '127.0.0.1'),
          'HTTP/2', '127.0.0.1', '')

# -------------------------------------------------------------------- case 3
# The production symptom: a worker that served an earlier connection from a
# different address, then serves an HTTP/2 stream.  The stale address is the one
# that made this look like a real (and alarming) remote client rather than a
# blank field.  The HTTP/1.1 requests are still in the CGI when /perfdump runs,
# which is also asserted -- if they had failed fast the workers would not have
# been holding 127.0.0.2 and the case would pass without testing anything.
warm = []
for _ in range(8):
    thread = threading.Thread(target=http11_get,
                              args=('/h2ip-cgi/slow.sh', '127.0.0.2'))
    thread.daemon = True
    thread.start()
    warm.append(thread)
time.sleep(1.5)
dump = h2c_get('/perfdump', '127.0.0.1')
cgi_rows = [r for r in sessions(dump) if r['URI'] == '/h2ip-cgi/slow.sh']
check('concurrent HTTP/1.1 requests are in flight from 127.0.0.2',
      len(cgi_rows) > 0 and
      all(r['Client'] == '127.0.0.2' for r in cgi_rows),
      '%d row(s): %s' % (len(cgi_rows),
                         ', '.join(r['Client'] for r in cgi_rows) or 'none'))
check_row('HTTP/2 peer is not another connection\'s stale address',
          dump, 'HTTP/2', '127.0.0.1', '')
for thread in warm:
    thread.join(15)

# -------------------------------------------------------------------- case 4
# ...and the value tracks this connection's peer rather than being a constant
# that happens to match on loopback.
check_row('HTTP/2 peer is this connection\'s source address',
          h2c_get('/perfdump', '127.0.0.3'),
          'HTTP/2', '127.0.0.3', '')

# -------------------------------------------------------------------- case 5
# Behind a trusted reverse proxy, with log-attribution "both": the peer is the
# proxy and the reported client is what it named.  Recording the peer must not
# cost the reported address -- beginClient() clears it, so it has to run before
# the request resolves it, not after.
check_row('a proxied HTTP/2 request reports peer and reported client',
          h2c_get('/perfdump', '127.0.0.1',
                  extra=[('x-real-ip', '203.0.113.7')]),
          'HTTP/2', '127.0.0.1', '203.0.113.7')

# -------------------------------------------------------------------- case 6
# The other direction of that clearing: a request with no forwarded address,
# after one that had one, must not inherit it.  This is the reported-client
# column's version of case 3.
check_row('a request without a forwarded address reports none',
          h2c_get('/perfdump', '127.0.0.1'),
          'HTTP/2', '127.0.0.1', '')

print()
if failures:
    print('[h2-client-ip] FAILURES: %s' % ', '.join(failures))
    sys.exit(1)
print('[h2-client-ip] ALL OK')
PY
rc=$?

if [ "${rc}" -ne 0 ]; then
    echo "=== HTTP/2 client-address test FAILED ==="
    exit 1
fi
echo "=== HTTP/2 client-address test passed ==="
