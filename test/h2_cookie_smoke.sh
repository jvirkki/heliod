#!/bin/bash
#
# Cookie reassembly on the HTTP/2 request path.
#
# RFC 7540 section 8.1.2.5 lets a client split the Cookie header into several
# header fields ("crumbs") for better HPACK compression, and *requires* the
# receiver to concatenate them with "; " before treating the request with
# HTTP/1.1 semantics:
#
#   "If there are multiple Cookie header fields after decompression, these MUST
#    be concatenated into a single octet string using the two-octet delimiter of
#    0x3B, 0x20 (the ASCII string "; ") before being passed into a non-HTTP/2
#    context, such as an HTTP/1.1 connection, or a generic HTTP server
#    application."
#
# Go's HTTP/2 transport does exactly this splitting, so every request Caddy
# proxies to an h2c upstream arrives with one `cookie` field per cookie.  heliod
# rebuilds an HTTP/1.1 request from the decoded fields, and its header parser
# folds repeated headers with ", " (httprequest.cpp) -- which is right for a
# list-valued header and wrong for Cookie, whose delimiter is ";".  A folded
# Cookie reaches the application as a single cookie whose value contains the
# rest of the string, so every cookie after the first is lost.
#
# That is invisible to a smoke test that sends one cookie, and invisible to a
# browser talking HTTP/1.1 to the same server, which is why it survived the H2
# work: it needs a multi-cookie request that arrives already split.
#
# Usage: test/h2_cookie_smoke.sh <heliod-work-root>

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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2ck-XXXXXX)
chmod 700 "${TEMP_DIR}"

# The CGI echoes HTTP_COOKIE verbatim, because the separator *is* the test: a
# folded value and a correctly joined one differ by one character.
CGI_DIR=$(mktemp -d /tmp/${INSTANCE}-h2ckcgi-XXXXXX)
chmod 755 "${CGI_DIR}"
cat > "${CGI_DIR}/cookie.sh" <<'CGI'
#!/bin/sh
printf 'Content-type: text/plain\r\n'
printf '\r\n'
printf 'cookie=%s\n' "${HTTP_COOKIE}"
printf 'accept_encoding=%s\n' "${HTTP_ACCEPT_ENCODING}"
CGI
chmod 755 "${CGI_DIR}/cookie.sh"

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
        if curl -s -o /dev/null --max-time 3 "http://localhost:${HTTP_PORT}/" \
                2>/dev/null; then
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "Server did not start serving on port ${HTTP_PORT}"
    cat /tmp/${INSTANCE}-h2ck.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"
configure_instance
add_cgi_mapping

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2ck.log 2>&1 &
SERVER_PID=$!
wait_for_listener

python3 - <<PY
import socket
import struct
import sys

HOST = 'localhost'
PORT = ${HTTP_PORT}
TIMEOUT = 15
PATH = '/cgi-bin/cookie.sh'

PREFACE = b'PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n'
STATIC_STATUS = {0x88: 200, 0x89: 204, 0x8a: 206, 0x8b: 304,
                 0x8c: 400, 0x8d: 404, 0x8e: 500}


def frame_header(length, ftype, flags, stream_id):
    return (struct.pack('>I', length)[1:] + bytes([ftype, flags]) +
            struct.pack('>I', stream_id & 0x7FFFFFFF))


def hpack_str(s):
    """HPACK string literal, no Huffman, with a proper 7-bit prefix length."""
    b = s.encode()
    if len(b) < 127:
        return bytes([len(b)]) + b
    out = bytearray([127])
    rem = len(b) - 127
    while rem >= 128:
        out.append((rem & 0x7f) | 0x80)
        rem >>= 7
    out.append(rem)
    return bytes(out) + b


def hpack_literal(name, value):
    """Literal header field without indexing, literal name.

    Everything goes out unindexed so the server's dynamic table stays empty and
    each request encodes identically -- one less moving part when a case fails.
    """
    return b'\x00' + hpack_str(name) + hpack_str(value)


def encode_headers(method, path, extra):
    out = bytearray()
    out += bytes([0x82]) if method == 'GET' else bytes([0x83])  # :method
    out += bytes([0x86])                                        # :scheme http
    p = path.encode()
    out += bytes([0x04, len(p)]) + p                            # :path
    out += bytes([0x01, 0x09]) + b'localhost'                   # :authority
    for name, value in extra:
        out += hpack_literal(name, value)
    return bytes(out)


class H2Conn(object):
    def __init__(self, timeout=TIMEOUT):
        self.sock = socket.create_connection((HOST, PORT), timeout=timeout)
        self.sock.settimeout(timeout)
        self.next_stream_id = 1
        self.streams = {}
        self.sock.sendall(PREFACE)
        self.sock.sendall(frame_header(0, 0x4, 0x0, 0))

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
        sid = struct.unpack('>I', header[5:9])[0] & 0x7FFFFFFF
        payload = self._recv_exactly(length) if length else b''
        return header[3], header[4], sid, payload

    def send_request(self, extra):
        sid = self.next_stream_id
        self.next_stream_id += 2
        self.streams[sid] = {'status': None, 'body': b'', 'done': False,
                             'reset': None}
        hdrs = encode_headers('GET', PATH, extra)
        self.sock.sendall(frame_header(len(hdrs), 0x1, 0x4 | 0x1, sid) + hdrs)
        return sid

    def _pump_one_frame(self):
        ftype, flags, sid, payload = self.read_frame()
        if ftype == 0x6 and not (flags & 0x1):
            self.sock.sendall(frame_header(8, 0x6, 0x1, 0) + payload)
            return
        if ftype == 0x7:
            raise AssertionError('server sent GOAWAY: %r' % payload[:32])
        st = self.streams.get(sid)
        if st is None:
            return
        if ftype == 0x1:
            if st['status'] is None and payload:
                st['status'] = STATIC_STATUS.get(payload[0])
        elif ftype == 0x0:
            st['body'] += payload
        elif ftype == 0x3:
            st['reset'] = (struct.unpack('>I', payload[:4])[0]
                           if len(payload) >= 4 else -1)
            st['done'] = True
            return
        if flags & 0x1:
            st['done'] = True

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
                raise AssertionError('stream %d: no response within %ds'
                                     % (sid, TIMEOUT))
        raise AssertionError('no END_STREAM for stream %d' % sid)

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


failures = []


def check(name, cond, detail=''):
    if cond:
        print('[h2-cookie] %s: OK%s' % (name, (' (%s)' % detail) if detail else ''))
    else:
        print('[h2-cookie] %s: FAIL %s' % (name, detail))
        failures.append(name)


def field_of(body, key):
    prefix = key + '='
    for line in body.decode('utf-8', 'replace').splitlines():
        if line.startswith(prefix):
            return line[len(prefix):]
    return None


def fetch(extra, key='cookie'):
    conn = H2Conn()
    try:
        sid = conn.send_request(extra)
        status, body = conn.read_response(sid)
        return status, field_of(body, key)
    finally:
        conn.close()


def parse_like_php(value):
    """Split HTTP_COOKIE the way RFC 6265 (and PHP, and phpBB) does: on ';'."""
    jar = {}
    for part in (value or '').split(';'):
        part = part.strip()
        if not part or '=' not in part:
            continue
        k, v = part.split('=', 1)
        jar[k.strip()] = v
    return jar


# -------------------------------------------------------------------- case 1
# Control: a single cookie header field carrying every cookie, the shape an
# HTTP/1.1 client sends.  This has always worked, so a failure here means the
# harness is wrong rather than the server.
status, cookie = fetch([('cookie', 'a=1; b=2; c=3')])
check('single cookie field passes through',
      status == 200 and cookie == 'a=1; b=2; c=3',
      'status %s cookie=%r' % (status, cookie))

# -------------------------------------------------------------------- case 2
# The regression: three crumbs, exactly as Go's h2 transport emits them.  They
# must arrive joined with "; ", not folded with ", ".
status, cookie = fetch([('cookie', 'a=1'), ('cookie', 'b=2'), ('cookie', 'c=3')])
check('split crumbs rejoined with "; "',
      status == 200 and cookie == 'a=1; b=2; c=3',
      'status %s cookie=%r' % (status, cookie))

# -------------------------------------------------------------------- case 3
# Two crumbs -- the smallest split that can be folded wrongly.
status, cookie = fetch([('cookie', 'a=1'), ('cookie', 'b=2')])
check('two crumbs rejoined with "; "',
      status == 200 and cookie == 'a=1; b=2',
      'status %s cookie=%r' % (status, cookie))

# -------------------------------------------------------------------- case 4
# The production symptom, in the shape phpBB uses.  Asserting on the parsed jar
# rather than the raw string is the point: this is what the application sees, and
# a folded value yields one cookie whose value has swallowed the other two, so
# the session id and the "keep me logged in" key both vanish.
NAME = 'phpbb3_a1b2c'
status, cookie = fetch([('cookie', '%s_u=2' % NAME),
                        ('cookie', '%s_k=0123456789abcdef' % NAME),
                        ('cookie', '%s_sid=fedcba9876543210' % NAME)])
jar = parse_like_php(cookie)
check('phpBB cookie set survives as three cookies',
      status == 200 and
      jar.get('%s_u' % NAME) == '2' and
      jar.get('%s_k' % NAME) == '0123456789abcdef' and
      jar.get('%s_sid' % NAME) == 'fedcba9876543210',
      'parsed %d cookie(s) %r from %r' % (len(jar), sorted(jar), cookie))

# -------------------------------------------------------------------- case 5
# A cookie value may legitimately contain a comma (RFC 6265 permits it in a
# value), so a receiver cannot recover a folded Cookie by splitting on ",".  This
# documents why the join has to be right at the point of reassembly.
status, cookie = fetch([('cookie', 'pref=a,b,c'), ('cookie', 'sid=xyz')])
jar = parse_like_php(cookie)
check('comma inside a cookie value is preserved',
      status == 200 and jar.get('pref') == 'a,b,c' and jar.get('sid') == 'xyz',
      'parsed %r from %r' % (jar, cookie))

# -------------------------------------------------------------------- case 6
# The crumbs need not be adjacent.  RFC 9113 8.2.3 puts no ordering requirement on
# them, and an encoder interleaving other fields between them is free to do so --
# the rejoin walks the whole field list, not a run of neighbours, and the joined
# line takes the position of the first crumb rather than the last.
status, cookie = fetch([('cookie', 'a=1'),
                        ('accept', 'text/html'),
                        ('cookie', 'b=2'),
                        ('user-agent', 'h2-cookie-test'),
                        ('cookie', 'c=3')])
check('crumbs separated by other header fields still rejoin',
      status == 200 and cookie == 'a=1; b=2; c=3',
      'status %s cookie=%r' % (status, cookie))

# -------------------------------------------------------------------- case 7
# The other side of the contract.  Cookie is the *only* field rejoined here,
# because it is the only one HTTP/2 lets arrive split whose delimiter is not a
# comma.  A genuinely list-valued field must keep the comma fold it gets on an
# HTTP/1.1 connection -- the h2 path is not entitled to its own header policy, and
# a "fix" that coalesced everything with "; " would corrupt this one instead.
status, encoding = fetch([('accept-encoding', 'gzip'), ('accept-encoding', 'br')],
                         key='accept_encoding')
check('a list-valued field still folds with ", "',
      status == 200 and encoding == 'gzip, br',
      'status %s accept-encoding=%r' % (status, encoding))

print()
if failures:
    print('[h2-cookie] FAILURES: %s' % ', '.join(failures))
    sys.exit(1)
print('[h2-cookie] ALL OK')
PY
rc=$?

if [ "${rc}" -ne 0 ]; then
    echo "=== HTTP/2 cookie test FAILED ==="
    exit 1
fi
echo "=== HTTP/2 cookie test passed ==="
