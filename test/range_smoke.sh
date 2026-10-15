#!/bin/bash
#
# Byte-range request and Range header DoS protection smoke test.
#
# Verifies byte-range handling in send-file / send-range across:
#   1. Valid single byte ranges: prefix (bytes=0-99), middle (bytes=100-199),
#      open-ended (bytes=900-), and suffix (bytes=-50) -> 206 Partial Content
#      with exact Content-Range, Content-Length, and body bytes.
#   2. Valid ascending non-overlapping multi-range requests
#      (bytes=0-9,20-29,40-49) -> 206 Partial Content with multipart/byteranges
#      body containing each requested part, and HEAD multi-range emitting no
#      stray multipart closing boundary on the keep-alive socket.
#   3. Unsatisfiable ranges (bytes=5000-6000, bytes=-0) -> 416 Requested range
#      not satisfiable with Content-Range: bytes */<size> (and 200 OK when
#      If-Range is present).
#   4. Abusive Range headers:
#      - 1000-range header (bytes=0-0,1-1,...,999-999 and bytes=0-,1-,...,999-)
#        exceeding the default cap (20) -> Range ignored, 200 OK with full body
#        in bounded time (< 1.0s).
#      - Default cap boundary: 20 non-overlapping ascending ranges -> 206;
#        21 non-overlapping ascending ranges -> 200 OK with full body.
#      - Overlapping ranges (bytes=0-50,25-75 and adjacent overlap bytes=0-10,10-20)
#        -> 200 OK with full body.
#      - Descending / out-of-order ranges (bytes=50-60,10-20)
#        -> 200 OK with full body.
#      - Amplification ranges summing to more than file size (bytes=0-,0-,0-)
#        -> 200 OK with full body.
#      - Malformed multi-range header (valid ranges followed by syntax error)
#        -> 200 OK with full body and no crash/leak.
#   5. Configurable max-ranges parameter on send-file in obj.conf (max-ranges=3):
#      3 ranges -> 206 Partial Content; 4 ranges -> 200 OK with full body.
#   6. Exercised on both io-model=classic and io-model=event listeners.
#
# Usage:
#   test/range_smoke.sh <heliod-work-root>

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
DOCS_DIR="${INSTANCE_DIR}/docs"
CLASSIC_PORT=18180
EVENT_PORT=18181
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
    rm -f "${DOCS_DIR}/range_test.bin" "${DOCS_DIR}/capped_range.bin"
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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-range-XXXXXX)
chmod 700 "${TEMP_DIR}"

python3 - <<PY
import xml.etree.ElementTree as ET
import os

# Create deterministic 2048-byte test files in docs/
payload = bytes((i * 7 + 13) % 256 for i in range(2048))
for name in ("range_test.bin", "capped_range.bin"):
    with open(os.path.join("${DOCS_DIR}", name), "wb") as f:
        f.write(payload)

# Configure server.xml with classic and event listeners and a 32 KiB header buffer
# so large Range headers are accepted at the HTTP parser layer and tested at the SAF layer.
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

for existing in root.findall("http"):
    root.remove(existing)
http_el = ET.SubElement(root, "http")
ET.SubElement(http_el, "request-header-buffer-size").text = "32768"

for listener in list(root.findall("http-listener")):
    root.remove(listener)

def add_listener(name, port, io_model):
    ls = ET.SubElement(root, "http-listener")
    ET.SubElement(ls, "name").text = name
    ET.SubElement(ls, "port").text = str(port)
    ET.SubElement(ls, "server-name").text = "localhost"
    ET.SubElement(ls, "default-virtual-server-name").text = "test"
    ET.SubElement(ls, "io-model").text = io_model

add_listener("http-listener-1", ${CLASSIC_PORT}, "classic")
add_listener("http-listener-2", ${EVENT_PORT}, "event")

vs = root.find("virtual-server")
for el in list(vs.findall("http-listener-name")):
    vs.remove(el)
for name in ("http-listener-1", "http-listener-2"):
    ET.SubElement(vs, "http-listener-name").text = name

with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
    tree.write(f, encoding="utf-8", xml_declaration=False)

# Add a <Client uri="*/capped_range.bin"> block in obj.conf with max-ranges="3"
for obj_name in os.listdir("${CONFIG_DIR}"):
    if obj_name.endswith("obj.conf"):
        obj_path = os.path.join("${CONFIG_DIR}", obj_name)
        with open(obj_path, "r", encoding="utf-8") as f:
            content = f.read()
        target = 'Service method="(GET|HEAD|POST)" type="*~magnus-internal/*" fn="send-file"'
        replacement = (
            '<Client uri="*/capped_range.bin">\n'
            'Service method="(GET|HEAD|POST)" type="*~magnus-internal/*" fn="send-file" max-ranges="3"\n'
            '</Client>\n'
            + target
        )
        if target in content:
            content = content.replace(target, replacement, 1)
            with open(obj_path, "w", encoding="utf-8") as f:
                f.write(content)
PY

echo "Starting ${INSTANCE} on classic=${CLASSIC_PORT}, event=${EVENT_PORT}..."
"${INSTANCE_DIR}/bin/startserv"
sleep 2

SERVER_PID=$(cat "${TEMP_DIR}/pid" 2>/dev/null || true)
if [ -z "${SERVER_PID}" ] || ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "Server failed to start; error log:"
    cat "${INSTANCE_DIR}/logs/errors" || true
    exit 1
fi

python3 - <<PY
import http.client
import re
import time

CLASSIC_PORT = ${CLASSIC_PORT}
EVENT_PORT = ${EVENT_PORT}
EXPECTED = bytes((i * 7 + 13) % 256 for i in range(2048))
SIZE = len(EXPECTED)

def request(port, method, path, headers=None):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    try:
        conn.request(method, path, headers=headers or {})
        resp = conn.getresponse()
        body = resp.read()
        hdrs = {k.lower(): v for k, v in resp.getheaders()}
        return resp.status, hdrs, body
    finally:
        conn.close()

def verify_listener(port, label):
    print(f"=== Testing Range behavior on {label} (port {port}) ===")

    # 1a. Single prefix range: bytes=0-99
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=0-99"})
    assert st == 206, f"{label}: bytes=0-99 expected 206, got {st}"
    assert hdrs.get("content-range") == f"bytes 0-99/{SIZE}", f"bad Content-Range: {hdrs.get('content-range')}"
    assert int(hdrs.get("content-length", "-1")) == 100, f"bad Content-Length: {hdrs.get('content-length')}"
    assert body == EXPECTED[0:100], f"{label}: bytes=0-99 body mismatch"

    # 1b. Single middle range with whitespace: bytes= 100 - 199
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes= 100 - 199 "})
    assert st == 206, f"{label}: bytes=100-199 expected 206, got {st}"
    assert hdrs.get("content-range") == f"bytes 100-199/{SIZE}", f"bad Content-Range: {hdrs.get('content-range')}"
    assert body == EXPECTED[100:200], f"{label}: bytes=100-199 body mismatch"

    # 1c. Open-ended range: bytes=1900-
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=1900-"})
    assert st == 206, f"{label}: bytes=1900- expected 206, got {st}"
    assert hdrs.get("content-range") == f"bytes 1900-{SIZE - 1}/{SIZE}", f"bad Content-Range: {hdrs.get('content-range')}"
    assert body == EXPECTED[1900:], f"{label}: bytes=1900- body mismatch"

    # 1d. Suffix range: bytes=-50
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=-50"})
    assert st == 206, f"{label}: bytes=-50 expected 206, got {st}"
    assert hdrs.get("content-range") == f"bytes {SIZE - 50}-{SIZE - 1}/{SIZE}", f"bad Content-Range: {hdrs.get('content-range')}"
    assert body == EXPECTED[SIZE - 50:], f"{label}: bytes=-50 body mismatch"

    # 1e. End beyond EOF clamped: bytes=2000-999999
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=2000-999999"})
    assert st == 206, f"{label}: bytes=2000-999999 expected 206, got {st}"
    assert hdrs.get("content-range") == f"bytes 2000-{SIZE - 1}/{SIZE}"
    assert body == EXPECTED[2000:]

    # 2a. Valid ascending multi-range: bytes=0-9, 20-29, 40-49
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=0-9, 20-29, 40-49"})
    assert st == 206, f"{label}: multi-range expected 206, got {st}"
    ct = hdrs.get("content-type", "")
    m = re.match(r"multipart/byteranges;\s*boundary=(.+)", ct)
    assert m, f"{label}: expected multipart/byteranges, got {ct!r}"
    boundary = m.group(1).encode("ascii")
    assert b"--" + boundary + b"--\r\n" in body, f"{label}: missing closing boundary"
    for r_start, r_end in ((0, 9), (20, 29), (40, 49)):
        cr_line = f"Content-range: bytes {r_start}-{r_end}/{SIZE}\n\n".encode("ascii")
        idx = body.find(cr_line)
        assert idx != -1, f"{label}: missing Content-range for {r_start}-{r_end}"
        slice_start = idx + len(cr_line)
        slice_len = r_end - r_start + 1
        assert body[slice_start:slice_start + slice_len] == EXPECTED[r_start:r_end + 1], (
            f"{label}: multipart payload mismatch for {r_start}-{r_end}"
        )

    # 2b. HEAD with multi-range on a keep-alive connection must not leak boundary bytes
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    try:
        conn.request("HEAD", "/range_test.bin", headers={"Range": "bytes=0-9,20-29"})
        r1 = conn.getresponse()
        b1 = r1.read()
        assert r1.status == 206 and b1 == b"", f"{label}: HEAD multi-range failed: status={r1.status}, body={b1!r}"
        conn.request("GET", "/range_test.bin", headers={"Range": "bytes=0-15"})
        r2 = conn.getresponse()
        b2 = r2.read()
        assert r2.status == 206 and b2 == EXPECTED[0:16], f"{label}: follow-up GET after HEAD multi-range failed: status={r2.status}"
    finally:
        conn.close()

    # 3a. Unsatisfiable range: bytes=5000-6000 -> 416
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=5000-6000"})
    assert st == 416, f"{label}: bytes=5000-6000 expected 416, got {st}"
    assert hdrs.get("content-range") == f"bytes */{SIZE}", f"bad 416 Content-Range: {hdrs.get('content-range')}"

    # 3b. Unsatisfiable suffix range: bytes=-0 -> 416
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=-0"})
    assert st == 416, f"{label}: bytes=-0 expected 416, got {st}"
    assert hdrs.get("content-range") == f"bytes */{SIZE}"

    # 3c. Unsatisfiable range with huge 64-bit overflow offset -> 416
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": "bytes=9999999999999999999999999-"})
    assert st == 416, f"{label}: overflow start expected 416, got {st}"

    # 3d. Unsatisfiable range with If-Range falls back to 200 OK
    st, hdrs, body = request(port, "GET", "/range_test.bin", {
        "Range": "bytes=5000-6000",
        "If-Range": '"nonmatching-etag"',
    })
    assert st == 200 and body == EXPECTED, f"{label}: unsatisfiable + If-Range expected 200 full body, got {st}"

    # 4a. Default cap boundary: 20 disjoint ascending ranges -> 206; 21 ranges -> 200 OK
    r20 = ",".join(f"{i * 4}-{i * 4 + 1}" for i in range(20))
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": f"bytes={r20}"})
    assert st == 206, f"{label}: 20 ascending ranges expected 206, got {st}"

    r21 = ",".join(f"{i * 4}-{i * 4 + 1}" for i in range(21))
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": f"bytes={r21}"})
    assert st == 200 and body == EXPECTED, f"{label}: 21 ranges expected 200 OK full body, got {st} (len={len(body)})"

    # 4b. 1000-range DoS header (CVE-2011-3192 pattern: both disjoint and overlapping) -> 200 OK in bounded time
    r1000_disjoint = ",".join(f"{i}-{i}" for i in range(1000))
    t0 = time.monotonic()
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": f"bytes={r1000_disjoint}"})
    elapsed = time.monotonic() - t0
    assert st == 200 and body == EXPECTED, f"{label}: 1000 disjoint ranges expected 200 OK full body, got {st}"
    assert elapsed < 1.0, f"{label}: 1000-range response took too long ({elapsed:.3f}s)"

    r1000_open = ",".join(f"{i}-" for i in range(1000))
    t0 = time.monotonic()
    st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": f"bytes={r1000_open}"})
    elapsed = time.monotonic() - t0
    assert st == 200 and body == EXPECTED, f"{label}: 1000 open-ended ranges expected 200 OK full body, got {st}"
    assert elapsed < 1.0, f"{label}: 1000 open-ended ranges took too long ({elapsed:.3f}s)"

    # 4c. Overlapping ranges -> 200 OK full body
    for bad_range in ("bytes=0-50,25-75", "bytes=0-10,10-20", "bytes=0-100,10-20", "bytes=0-,0-,0-"):
        st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": bad_range})
        assert st == 200 and body == EXPECTED, f"{label}: overlapping {bad_range!r} expected 200 OK full body, got {st}"

    # 4d. Descending / out-of-order ranges -> 200 OK full body
    for bad_range in ("bytes=50-60,10-20", "bytes=0-10,50-60,20-30", "bytes=-50,0-10"):
        st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": bad_range})
        assert st == 200 and body == EXPECTED, f"{label}: descending {bad_range!r} expected 200 OK full body, got {st}"

    # 4e. Malformed multi-range (valid ranges followed by syntax error) -> 200 OK full body
    for bad_range in ("bytes=0-10,20-30,bad", "bytes=0-10,20-15", "bytes=0-10,-", "bytes=0abc-10"):
        st, hdrs, body = request(port, "GET", "/range_test.bin", {"Range": bad_range})
        assert st == 200 and body == EXPECTED, f"{label}: malformed {bad_range!r} expected 200 OK full body, got {st}"

    # 5. Configurable max-ranges="3" on /capped_range.bin
    r3 = "bytes=0-9,20-29,40-49"
    st, hdrs, body = request(port, "GET", "/capped_range.bin", {"Range": r3})
    assert st == 206, f"{label}: 3 ranges on max-ranges=3 expected 206, got {st}"

    r4 = "bytes=0-9,20-29,40-49,60-69"
    st, hdrs, body = request(port, "GET", "/capped_range.bin", {"Range": r4})
    assert st == 200 and body == EXPECTED, f"{label}: 4 ranges on max-ranges=3 expected 200 OK full body, got {st}"

    print(f"  [PASS] {label} passed all single-range, multi-range, 416, DoS cap, overlap/descending, and max-ranges checks")

verify_listener(CLASSIC_PORT, "classic")
verify_listener(EVENT_PORT, "event")
PY

echo "All byte-range and Range DoS smoke checks passed."
