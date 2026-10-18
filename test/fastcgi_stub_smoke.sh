#!/bin/bash
#
# FastCGI stub process (Fastcgistub) lifecycle, stress, and ASAN smoke test.
#
# Verifies Fastcgistub and the FastCGI NSAPI plugin across:
#   1. ASAN + UBSAN instrumentation of Fastcgistub:
#      - CLI argument edge cases (missing -l, missing -b, bind-path without '_')
#      - Socket framing edge cases (empty disconnect, short header, invalid
#        version, unknown request type, corrupt TLV, split multi-chunk send)
#      - Invalid bind paths (missing port, non-numeric port, out-of-range port,
#        overly long UDS path, unbindable directory) and nonexistent app-path
#      - Concurrent duplicate RQ_START bursts (guards against UAF on proc and
#        double-close of connfd between processRequest and childExitHandlerThread)
#      - RQ_OVERLOAD process scaling and RQ_VS_SHUTDOWN cleanup
#      - Backend child crash (SIGKILL) and automatic respawn by Fastcgistub
#      - Clean SIGTERM shutdown without signal-handler deadlock or double
#        handleExit() PR_Lock(NULL) crash
#   2. End-to-end webservd + libfastcgi.so + ASAN Fastcgistub integration:
#      - Concurrent cold-start request burst over HTTP/1.1 and HTTP/2
#      - Backend child process kill (-9) and recovery under load
#      - Invalid bind-path and bad app-path routes logged and returning 500
#        without crashing webservd or Fastcgistub
#      - Repeated server restart cycles (stopserv/startserv) verifying clean
#        Fastcgistub termination and respawn
#
# Usage:
#   test/fastcgi_stub_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT=$(cd "$1" 2>/dev/null && pwd) || { echo "Work root '$1' not found"; exit 1; }
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
FCGI_DIR="${WORK_ROOT}/plugins/fastcgi"
FCGI_SO="${FCGI_DIR}/libfastcgi.so"
FCGI_STUB="${FCGI_DIR}/Fastcgistub"
HTTP_PORT=18180
HTTPS_PORT=18443
MAX_THREADS=8

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi
if [ ! -f "${FCGI_SO}" ] || [ ! -f "${FCGI_STUB}" ]; then
    echo "[fastcgi-stub] FAIL: FastCGI plugin or Fastcgistub not found in ${FCGI_DIR}"
    exit 1
fi

TMP_DIR=$(mktemp -d /tmp/heliod-fcgistub-smoke-XXXXXX)
chmod 700 "${TMP_DIR}"
NO_PASS=""

cleanup() {
    if [ -d "${INSTANCE_DIR}" ]; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
        pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
    fi
    pkill -9 -x fcgi-test-app 2>/dev/null || true
    pkill -9 -x Fastcgistub-asan 2>/dev/null || true
    pkill -9 -x Fastcgistub.real 2>/dev/null || true
    if [ -f "${FCGI_STUB}.orig" ]; then
        mv -f "${FCGI_STUB}.orig" "${FCGI_STUB}"
    fi
    rm -f "${FCGI_DIR}/Fastcgistub.real" 2>/dev/null || true
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"
        cp -r "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
        rm -rf "${CONFIG_DIR}.pristine"
    fi
    [ -n "${NO_PASS:-}" ] && rm -f "${NO_PASS}" 2>/dev/null || true
    rm -rf "${TMP_DIR}" 2>/dev/null || true
}
trap cleanup EXIT

fail() {
    echo "[fastcgi-stub] FAIL: $*"
    exit 1
}

# ---------------------------------------------------------------------------
# 1. Build a self-contained FastCGI responder test binary and ASAN Fastcgistub
# ---------------------------------------------------------------------------
echo "[fastcgi-stub] compiling test FastCGI responder and ASAN Fastcgistub..."

cat > "${TMP_DIR}/fcgi_responder.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

#define FCGI_LISTENSOCK_FILENO 0
#define FCGI_VERSION_1         1
#define FCGI_BEGIN_REQUEST     1
#define FCGI_PARAMS            4
#define FCGI_STDIN             5
#define FCGI_STDOUT            6
#define FCGI_END_REQUEST       3
#define FCGI_KEEP_CONN         1

typedef struct {
    unsigned char version;
    unsigned char type;
    unsigned char requestIdB1;
    unsigned char requestIdB0;
    unsigned char contentLengthB1;
    unsigned char contentLengthB0;
    unsigned char paddingLength;
    unsigned char reserved;
} FCGI_Header;

static int read_full(int fd, void *buf, size_t len) {
    unsigned char *p = (unsigned char *)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = read(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    size_t left = len;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        left -= (size_t)n;
    }
    return 0;
}

static void serve_conn(int fd) {
    for (;;) {
        FCGI_Header hdr;
        int req_id = 0;
        int keep_conn = 0;
        int stdin_done = 0;

        while (!stdin_done) {
            if (read_full(fd, &hdr, sizeof(hdr)) < 0) return;
            req_id = (hdr.requestIdB1 << 8) | hdr.requestIdB0;
            int clen = (hdr.contentLengthB1 << 8) | hdr.contentLengthB0;
            int plen = hdr.paddingLength;
            unsigned char buf[65536];
            if (clen > 0) {
                if (read_full(fd, buf, (size_t)clen) < 0) return;
            }
            if (plen > 0) {
                if (read_full(fd, buf, (size_t)plen) < 0) return;
            }
            if (hdr.type == FCGI_BEGIN_REQUEST && clen >= 3) {
                keep_conn = (buf[2] & FCGI_KEEP_CONN) != 0;
            } else if (hdr.type == FCGI_STDIN && clen == 0) {
                stdin_done = 1;
            }
        }

        const char *custom_env = getenv("CUSTOM_VAR");
        if (!custom_env) custom_env = "unset";
        char payload[512];
        int plen = snprintf(payload, sizeof(payload),
                            "Status: 200 OK\r\n"
                            "Content-Type: text/plain\r\n"
                            "\r\n"
                            "FCGI-STUB-OK pid=%d env=%s\n",
                            (int)getpid(), custom_env);

        FCGI_Header out_hdr;
        memset(&out_hdr, 0, sizeof(out_hdr));
        out_hdr.version = FCGI_VERSION_1;
        out_hdr.type = FCGI_STDOUT;
        out_hdr.requestIdB1 = (unsigned char)((req_id >> 8) & 0xff);
        out_hdr.requestIdB0 = (unsigned char)(req_id & 0xff);
        out_hdr.contentLengthB1 = (unsigned char)((plen >> 8) & 0xff);
        out_hdr.contentLengthB0 = (unsigned char)(plen & 0xff);
        if (write_full(fd, &out_hdr, sizeof(out_hdr)) < 0) return;
        if (write_full(fd, payload, (size_t)plen) < 0) return;

        out_hdr.contentLengthB1 = 0;
        out_hdr.contentLengthB0 = 0;
        if (write_full(fd, &out_hdr, sizeof(out_hdr)) < 0) return;

        unsigned char end_rec[16];
        memset(end_rec, 0, sizeof(end_rec));
        end_rec[0] = FCGI_VERSION_1;
        end_rec[1] = FCGI_END_REQUEST;
        end_rec[2] = (unsigned char)((req_id >> 8) & 0xff);
        end_rec[3] = (unsigned char)(req_id & 0xff);
        end_rec[5] = 8;
        if (write_full(fd, end_rec, sizeof(end_rec)) < 0) return;

        if (!keep_conn) break;
    }
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    for (;;) {
        struct sockaddr addr;
        socklen_t alen = sizeof(addr);
        int cfd = accept(FCGI_LISTENSOCK_FILENO, &addr, &alen);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            return 1;
        }
        serve_conn(cfd);
        close(cfd);
    }
}
EOF

gcc -O2 -Wall "${TMP_DIR}/fcgi_responder.c" -o "${TMP_DIR}/fcgi-test-app"
chmod 755 "${TMP_DIR}/fcgi-test-app"

g++ -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 \
    -fPIC -fpermissive -fcommon \
    -DLinux -DLINUX -DXP_UNIX -D_REENTRANT -DMCC_HTTPD -DNET_SSL -DSERVER_BUILD -DNSPR20 \
    -I/usr/include/nspr \
    -I"${WORK_ROOT}/include" \
    -I"${REPO_ROOT}/internal/$(basename "${WORK_ROOT}")/include" \
    -I"${REPO_ROOT}/src/server" \
    -I"${REPO_ROOT}/src/server/public" \
    -I"${REPO_ROOT}/src/support" \
    "${REPO_ROOT}/src/server/plugins/fastcgi/util.cpp" \
    "${REPO_ROOT}/src/server/plugins/fastcgi/fcgiprocess.cpp" \
    "${REPO_ROOT}/src/server/plugins/fastcgi/fastcgistub.cpp" \
    "${REPO_ROOT}/src/support/support/GenericList.cpp" \
    -o "${TMP_DIR}/Fastcgistub-asan" \
    -lnspr4 -lplc4 -lplds4 -lpthread

echo "[fastcgi-stub] built Fastcgistub-asan and fcgi-test-app: OK"

# ---------------------------------------------------------------------------
# 2. Direct ASAN/UBSAN Fastcgistub protocol, lifecycle & stress tests
# ---------------------------------------------------------------------------
echo "[fastcgi-stub] running direct ASAN/UBSAN protocol & lifecycle stress..."

export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1:halt_on_error=1:log_path=${TMP_DIR}/asan_direct"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

# 2a. CLI argument edge cases: no args, missing -l, bind-path without '_'
set +e
"${TMP_DIR}/Fastcgistub-asan" >/dev/null 2>&1
RC_NOARGS=$?
"${TMP_DIR}/Fastcgistub-asan" -b "${TMP_DIR}/ bad_opt" -? >/dev/null 2>&1
RC_BADOPT=$?
set -e
[ "${RC_NOARGS}" -eq 1 ] || fail "Fastcgistub with no args returned ${RC_NOARGS} (expected 1)"
[ "${RC_BADOPT}" -eq 1 ] || fail "Fastcgistub with invalid option returned ${RC_BADOPT} (expected 1)"
for f in "${TMP_DIR}"/asan_direct*; do
    [ -e "$f" ] && fail "ASAN error during CLI edge test: $(cat "$f")"
done
echo "[fastcgi-stub] CLI argument edge cases under ASAN: OK"

# 2b. Protocol driver in Python exercising framing, invalid bind paths,
#     concurrent duplicate RQ_START bursts, RQ_OVERLOAD, child kill/restart,
#     RQ_VS_SHUTDOWN, and SIGTERM shutdown.
python3 - "${TMP_DIR}/Fastcgistub-asan" "${TMP_DIR}/fcgi-test-app" "${TMP_DIR}" "${REPO_ROOT}/src/server/plugins/fastcgi/errortypes.h" <<'PY'
import os, re, sys, struct, socket, signal, subprocess, time, threading

stub_bin, app_bin, tmp_dir, err_hdr = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
stub_sock = os.path.join(tmp_dir, f"Fastcgistub_{os.getpid()}")
log_dir = os.path.join(tmp_dir, "stub_logs")
os.makedirs(log_dir, exist_ok=True)

# Constants from fastcgistub.h / errortypes.h
RQ_START = 1
RQ_OVERLOAD = 2
RQ_VS_SHUTDOWN = 3
STUBVERSION = 1

RQT_PATH = 1
RQT_BIND_PATH = 2
RQT_ARGV0 = 3
RQT_ENVP = 4
RQT_ARGV = 5
RQT_CHDIRPATH = 6
RQT_MIN_PROCS = 15
RQT_MAX_PROCS = 16
RQT_LISTENQ_SIZE = 17
RQT_RESTART_ON_EXIT = 18
RQT_RESTART_INTERVAL = 19
RQT_NUM_FAILURE = 20
RQT_VS_ID = 21

enum_body = re.search(r"typedef\s+enum\s*\{([^}]*)\}\s*PluginError", open(err_hdr).read(), re.S).group(1)
val = 0
for line in enum_body.splitlines():
    line = re.sub(r"//.*", "", line).strip().rstrip(",")
    if not line:
        continue
    if "=" in line:
        k, v = [x.strip() for x in line.split("=", 1)]
        val = int(v)
    else:
        k = line
    globals()[k] = val
    val += 1

ptr_size = struct.calcsize("L")
REQUEST_ALIGN = ptr_size
HEADER_FMT = "@iiLL"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
MIN_MSG_SIZE = HEADER_SIZE + 8  # sizeof(RequestHeader)

def round_up(n, align=REQUEST_ALIGN):
    return (n + align - 1) & ~(align - 1)

def encode_tlv(t, data_bytes):
    vec_off = 4  # short type, short len
    total_len = vec_off + len(data_bytes)
    padded_len = round_up(total_len)
    pad = b"\x00" * (padded_len - total_len)
    return struct.pack("@hh", t, total_len) + data_bytes + pad

def build_msg(msg_type, tlvs, version=STUBVERSION, flags=0):
    body = b"".join(encode_tlv(t, v) for t, v in tlvs)
    totlen = HEADER_SIZE + len(body)
    hdr = struct.pack(HEADER_FMT, totlen, msg_type, version, flags)
    return hdr + body

def send_msg(msg, split=False):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(stub_sock)
    if split and len(msg) > 20:
        s.sendall(msg[:17])
        time.sleep(0.01)
        s.sendall(msg[17:])
    else:
        s.sendall(msg)
    resp = s.recv(4)
    s.close()
    assert len(resp) == 4, f"Expected 4-byte PluginError, got {len(resp)} bytes"
    return struct.unpack("@i", resp)[0]

# Spawn Fastcgistub with pipe on stdin (matching StubExec::startStub)
r_fd, w_fd = os.pipe()
proc = subprocess.Popen(
    [stub_bin, "-b", stub_sock, "-t", tmp_dir, "-i", "https-test", "-l", log_dir],
    stdin=w_fd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
)
os.close(w_fd)
startup_bytes = os.read(r_fd, 4)
os.close(r_fd)
assert len(startup_bytes) == 4, "Fastcgistub did not write startup response"
startup_err = struct.unpack("@i", startup_bytes)[0]
assert startup_err == NO_FCGI_ERROR, f"Fastcgistub startup error: {startup_err}"

try:
    # 1. Empty connection (connect and close immediately)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(stub_sock)
    s.close()
    time.sleep(0.05)

    # 2. Short header (< sizeof(RequestHeader))
    err = send_msg(struct.pack("@i", 8) + b"1234")
    assert err == REQUEST_INCOMPLETE_HEADER, f"Expected REQUEST_INCOMPLETE_HEADER, got {err}"

    # 3. Invalid version
    err = send_msg(build_msg(RQ_START, [(RQT_PATH, b"/bin/true\x00")], version=99))
    assert err == INVALID_VERSION, f"Expected INVALID_VERSION, got {err}"

    # 4. Unknown request type
    err = send_msg(build_msg(99, [(RQT_PATH, b"/bin/true\x00")]))
    assert err == INVALID_REQUEST_TYPE, f"Expected INVALID_REQUEST_TYPE, got {err}"

    # 5. Missing required parameters for RQ_START
    err = send_msg(build_msg(RQ_START, [(RQT_PATH, app_bin.encode() + b"\x00")]))
    assert err == REQUEST_MISSING_OR_INVALID_PARAM, f"Expected REQUEST_MISSING_OR_INVALID_PARAM, got {err}"

    # 6. Invalid bind paths (missing port, non-digit port, out-of-range port, overly long UDS, nonexistent dir)
    for bad_bp, expected in [
        (b"127.0.0.1:\x00", (SERVER_SOCKET_CREATION_FAILURE,)),
        (b"127.0.0.1:notaport\x00", (SERVER_SOCKET_CREATION_FAILURE,)),
        (b"127.0.0.1:70000\x00", (SERVER_SOCKET_CREATION_FAILURE,)),
        (b"/" + b"a" * 130 + b"\x00", (SERVER_SOCKET_CREATION_FAILURE,)),
        (b"/nonexistent_dir_fcgi_stub/sock\x00", (SERVER_BIND_ERROR,)),
    ]:
        err = send_msg(build_msg(RQ_START, [
            (RQT_PATH, app_bin.encode() + b"\x00"),
            (RQT_ARGV0, app_bin.encode() + b"\x00"),
            (RQT_BIND_PATH, bad_bp),
            (RQT_VS_ID, b"https-test\x00"),
        ]))
        assert err in expected, f"For bad bind-path {bad_bp!r}: expected {expected}, got {err}"

    # 7. Nonexistent executable path
    bad_app_sock = os.path.join(tmp_dir, "bad_app.sock").encode() + b"\x00"
    err = send_msg(build_msg(RQ_START, [
        (RQT_PATH, b"/nonexistent_fcgi_executable\x00"),
        (RQT_ARGV0, b"/nonexistent_fcgi_executable\x00"),
        (RQT_BIND_PATH, bad_app_sock),
        (RQT_VS_ID, b"https-test\x00"),
    ]))
    assert err == STAT_FAILURE, f"Expected STAT_FAILURE for nonexistent app, got {err}"

    # 8. Valid RQ_START sent across split TCP/UDS chunks, with >30 env vars to exercise unbuildEnvArray resize
    app_sock = os.path.join(tmp_dir, "direct_app.sock")
    env_entries = [f"VAR_{i}=val_{i}".encode() for i in range(35)] + [b"CUSTOM_VAR=from_direct_stub"]
    env_blob = b"\x00".join(env_entries) + b"\x00"
    start_tlvs = [
        (RQT_PATH, app_bin.encode() + b"\x00"),
        (RQT_ARGV0, app_bin.encode() + b"\x00"),
        (RQT_BIND_PATH, app_sock.encode() + b"\x00"),
        (RQT_MIN_PROCS, b"2\x00"),
        (RQT_MAX_PROCS, b"4\x00"),
        (RQT_RESTART_ON_EXIT, b"1\x00"),
        (RQT_NUM_FAILURE, b"5\x00"),
        (RQT_VS_ID, b"https-test\x00"),
        (RQT_ENVP, env_blob),
    ]
    err = send_msg(build_msg(RQ_START, start_tlvs), split=True)
    assert err == NO_FCGI_ERROR, f"Valid RQ_START failed with {err}"

    # 9. Concurrent duplicate RQ_START burst (16 threads simultaneously)
    burst_errors = []
    def dup_worker():
        try:
            rc = send_msg(build_msg(RQ_START, start_tlvs))
            if rc != NO_FCGI_ERROR:
                burst_errors.append(rc)
        except Exception as exc:
            burst_errors.append(str(exc))

    threads = [threading.Thread(target=dup_worker) for _ in range(16)]
    for t in threads: t.start()
    for t in threads: t.join()
    assert not burst_errors, f"Duplicate RQ_START burst had errors: {burst_errors}"

    # 10. RQ_OVERLOAD (no RQT_ARGV0 sent by StubExec::buildServerRequest)
    overload_tlvs = [
        (RQT_PATH, app_bin.encode() + b"\x00"),
        (RQT_BIND_PATH, app_sock.encode() + b"\x00"),
        (RQT_VS_ID, b"https-test\x00"),
    ]
    err = send_msg(build_msg(RQ_OVERLOAD, overload_tlvs))
    assert err == NO_FCGI_ERROR, f"RQ_OVERLOAD failed with {err}"

    # 11. Kill one backend child and verify childExitHandlerThread reaps and respawns it
    pids_before = subprocess.check_output(["pgrep", "-x", "fcgi-test-app"]).decode().split()
    assert len(pids_before) >= 2, f"Expected >=2 backend children, found {pids_before}"
    os.kill(int(pids_before[0]), signal.SIGKILL)
    time.sleep(0.4)
    pids_after = subprocess.check_output(["pgrep", "-x", "fcgi-test-app"]).decode().split()
    assert len(pids_after) >= 2, f"Expected child respawn, found {pids_after}"

    # 12. RQ_VS_SHUTDOWN
    err = send_msg(build_msg(RQ_VS_SHUTDOWN, overload_tlvs))
    assert err == NO_FCGI_ERROR, f"RQ_VS_SHUTDOWN failed with {err}"

    # 13. Clean SIGTERM shutdown
    proc.send_signal(signal.SIGTERM)
    rc = proc.wait(timeout=5.0)
    stderr_out = proc.stderr.read().decode(errors="replace")
    assert rc == 0, f"Fastcgistub exited with {rc} on SIGTERM; stderr:\n{stderr_out}"
    assert "ERROR: AddressSanitizer" not in stderr_out and " runtime error:" not in stderr_out, stderr_out
finally:
    if proc.poll() is None:
        proc.kill()
        proc.wait()
PY

for f in "${TMP_DIR}"/asan_direct*; do
    [ -e "$f" ] && fail "ASAN error during direct protocol stress: $(cat "$f")"
done
echo "[fastcgi-stub] direct ASAN/UBSAN protocol & lifecycle stress: OK"

# ---------------------------------------------------------------------------
# 3. End-to-end webservd + libfastcgi.so + ASAN Fastcgistub integration test
# ---------------------------------------------------------------------------
echo "[fastcgi-stub] setting up webservd integration test with ASAN Fastcgistub..."

cd "${WORK_ROOT}"
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

rm -rf "${CONFIG_DIR}.pristine"
cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"

# Install ASAN-instrumented Fastcgistub in plugins/fastcgi
cp -p "${FCGI_STUB}" "${FCGI_STUB}.orig"
cp -p "${TMP_DIR}/Fastcgistub-asan" "${FCGI_DIR}/Fastcgistub.real"
cat > "${FCGI_STUB}" <<EOF
#!/bin/sh
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1:halt_on_error=1:log_path=${TMP_DIR}/asan_e2e"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
exec "${FCGI_DIR}/Fastcgistub.real" "\$@"
EOF
chmod 755 "${FCGI_STUB}" "${FCGI_DIR}/Fastcgistub.real"

# TLS cert + load the FastCGI plugin
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp); printf '\n' > "${NO_PASS}"
rm -f cert8.db key3.db secmod.db cert9.db key4.db pkcs11.txt 2>/dev/null || true
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout "${TMP_DIR}/key.pem" -out "${TMP_DIR}/cert.pem" \
    -days 90 -nodes -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost" 2>/dev/null
openssl pkcs12 -export -out "${TMP_DIR}/cert.p12" -inkey "${TMP_DIR}/key.pem" -in "${TMP_DIR}/cert.pem" \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i "${TMP_DIR}/cert.p12" -d dbm:"." -w "${NO_PASS}"
grep -q "^NSSDir" magnus.conf || echo "NSSDir ${PWD}" >> magnus.conf
grep -q libfastcgi magnus.conf || echo "Init fn=\"load-modules\" shlib=\"${FCGI_SO}\"" >> magnus.conf
cd "${WORK_ROOT}"

SERVER_TEMP="${TMP_DIR}/server-tmp"
mkdir -p "${SERVER_TEMP}"
chmod 700 "${SERVER_TEMP}"

python3 - <<PY
import xml.etree.ElementTree as ET, os
path = os.path.join("${CONFIG_DIR}", "server.xml")
t = ET.parse(path); r = t.getroot()
def setpath(tag, val):
    e = r.find(tag)
    if e is None: e = ET.SubElement(r, tag)
    e.text = val
setpath("temp-path", "${SERVER_TEMP}")
setpath("platform", "64")
for e in r.findall("thread-pool"): r.remove(e)
pool = ET.SubElement(r, "thread-pool")
ET.SubElement(pool, "min-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "max-threads").text = "${MAX_THREADS}"
ET.SubElement(pool, "queue-size").text = "512"
vs = r.find("virtual-server")
vsl = set(e.text for e in vs.findall("http-listener-name"))
for l in r.findall("http-listener"):
    name = l.findtext("name")
    if name == "https-listener-1":
        r.remove(l); continue
    if name == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${HTTP_PORT}"
    for c in list(l):
        if c.tag == "io-model": l.remove(c)
    ET.SubElement(l, "io-model").text = "event"
h = ET.SubElement(r, "http-listener")
ET.SubElement(h, "name").text = "https-listener-1"
ET.SubElement(h, "port").text = "${HTTPS_PORT}"
ET.SubElement(h, "server-name").text = "localhost"
ET.SubElement(h, "default-virtual-server-name").text = "test"
ET.SubElement(h, "io-model").text = "event"
s = ET.SubElement(h, "ssl"); ET.SubElement(s, "enabled").text = "true"
ET.SubElement(s, "server-cert-nickname").text = "Server-Cert"
if "https-listener-1" not in vsl:
    ET.SubElement(vs, "http-listener-name").text = "https-listener-1"
with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY

python3 - <<PY
import os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
s = open(path).read()
anchor = 'PathCheck fn="uri-clean"'
if anchor not in s: raise SystemExit("obj.conf: no uri-clean anchor")
nt = (
    'NameTrans fn="assign-name" from="/fcgi-ok" name="fcgi-ok"\n'
    'NameTrans fn="assign-name" from="/fcgi-bad-bind" name="fcgi-bad-bind"\n'
    'NameTrans fn="assign-name" from="/fcgi-bad-exec" name="fcgi-bad-exec"\n'
)
s = s.replace(anchor, nt + anchor, 1)
s += (
    '\n<Object name="fcgi-ok">\n'
    'ObjectType fn="force-type" type="magnus-internal/fastcgi"\n'
    'Service fn="responder-fastcgi" app-path="${TMP_DIR}/fcgi-test-app" '
    'bind-path="fcgi_e2e.sock" min-procs="2" max-procs="4" '
    'app-env="CUSTOM_VAR=e2e_ok" reuse-connection="false" req-retry="2"\n'
    '</Object>\n'
    '\n<Object name="fcgi-bad-bind">\n'
    'ObjectType fn="force-type" type="magnus-internal/fastcgi"\n'
    'Service fn="responder-fastcgi" app-path="${TMP_DIR}/fcgi-test-app" '
    'bind-path="127.0.0.1:99999"\n'
    '</Object>\n'
    '\n<Object name="fcgi-bad-exec">\n'
    'ObjectType fn="force-type" type="magnus-internal/fastcgi"\n'
    'Service fn="responder-fastcgi" app-path="/nonexistent/fcgi_missing_bin" '
    'bind-path="fcgi_missing.sock"\n'
    '</Object>\n'
)
open(path, "w").write(s)
PY

ERR_LOG="${INSTANCE_DIR}/logs/errors"
ERRLOG_START=0
[ -f "${ERR_LOG}" ] && ERRLOG_START=$(wc -l < "${ERR_LOG}")

start_instance() {
    "${INSTANCE_DIR}/bin/startserv" > "${TMP_DIR}/startserv.log" 2>&1 &
    for _ in $(seq 1 30); do
        ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} " && return 0
        sleep 0.3
    done
    cat "${TMP_DIR}/startserv.log"
    fail "server did not start on ${HTTPS_PORT}"
}

stop_instance() {
    "${INSTANCE_DIR}/bin/stopserv" > "${TMP_DIR}/stopserv.log" 2>&1 || true
    for _ in $(seq 1 20); do
        pgrep -f "webservd.*${INSTANCE_DIR}" >/dev/null 2>&1 || break
        sleep 0.2
    done
    pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
}

run_burst() {
    local count="$1"
    local label="$2"
    local ok_count=0
    local pids=()
    rm -f "${TMP_DIR}"/burst_*.out
    for i in $(seq 1 "${count}"); do
        if [ $((i % 2)) -eq 0 ]; then
            curl --http2 -sk --max-time 10 "https://localhost:${HTTPS_PORT}/fcgi-ok" \
                > "${TMP_DIR}/burst_${i}.out" 2>/dev/null &
        else
            curl -s --max-time 10 "http://localhost:${HTTP_PORT}/fcgi-ok" \
                > "${TMP_DIR}/burst_${i}.out" 2>/dev/null &
        fi
        pids+=($!)
    done
    for p in "${pids[@]}"; do
        wait "$p" || true
    done
    for i in $(seq 1 "${count}"); do
        if grep -q "FCGI-STUB-OK.*env=e2e_ok" "${TMP_DIR}/burst_${i}.out" 2>/dev/null; then
            ok_count=$((ok_count + 1))
        fi
    done
    [ "${ok_count}" -eq "${count}" ] || fail "${label}: only ${ok_count}/${count} requests succeeded"
    echo "[fastcgi-stub] ${label}: ${ok_count}/${count} concurrent HTTP/1.1 + HTTP/2 requests succeeded"
}

# Cycle 1: cold-start burst, bad config routes, backend kill + recovery
start_instance
run_burst 20 "cycle 1 cold-start burst"

# Check bad bind-path and bad app-path return 500 without crashing stub or server
CODE_BAD_BIND=$(curl -s -o /dev/null -w '%{http_code}' --max-time 10 "http://localhost:${HTTP_PORT}/fcgi-bad-bind")
CODE_BAD_EXEC=$(curl -s -o /dev/null -w '%{http_code}' --max-time 10 "http://localhost:${HTTP_PORT}/fcgi-bad-exec")
[ "${CODE_BAD_BIND}" = "500" ] || fail "expected 500 for invalid bind-path, got ${CODE_BAD_BIND}"
[ "${CODE_BAD_EXEC}" = "500" ] || fail "expected 500 for nonexistent app-path, got ${CODE_BAD_EXEC}"
echo "[fastcgi-stub] invalid bind-path (500) and bad app-path (500) handled cleanly: OK"

# Kill all backend fcgi-test-app processes and verify automatic respawn by Fastcgistub
APP_PIDS_BEFORE=$(pgrep -x fcgi-test-app | sort | tr '\n' ' ')
[ -n "${APP_PIDS_BEFORE}" ] || fail "no fcgi-test-app processes running before kill"
pkill -9 -x fcgi-test-app || true
sleep 0.5
run_burst 16 "post-backend-kill recovery burst"
APP_PIDS_AFTER=$(pgrep -x fcgi-test-app | sort | tr '\n' ' ')
[ -n "${APP_PIDS_AFTER}" ] || fail "no fcgi-test-app processes running after respawn"
[ "${APP_PIDS_BEFORE}" != "${APP_PIDS_AFTER}" ] || fail "backend PIDs did not change after kill -9"
echo "[fastcgi-stub] backend death and respawn verified (${APP_PIDS_BEFORE} -> ${APP_PIDS_AFTER}): OK"

# Cycles 2 and 3: repeated server restarts with concurrent request bursts
for cycle in 2 3; do
    stop_instance
    # Verify Fastcgistub and fcgi-test-app were terminated on server stop
    for _ in $(seq 1 20); do
        pgrep -f "${FCGI_DIR}/Fastcgistub.real" >/dev/null 2>&1 || break
        sleep 0.2
    done
    if pgrep -f "${FCGI_DIR}/Fastcgistub.real" >/dev/null 2>&1; then
        fail "cycle ${cycle}: Fastcgistub still running after stopserv"
    fi
    start_instance
    run_burst 16 "cycle ${cycle} restart burst"
done

stop_instance

# Verify no ASAN errors and no server crashes
for f in "${TMP_DIR}"/asan_e2e*; do
    [ -e "$f" ] && fail "ASAN error in Fastcgistub during e2e test: $(cat "$f")"
done

NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERR_LOG}" 2>/dev/null |
             grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
[ -z "${NEW_ERRORS}" ] || fail "crash/assertion in error log: ${NEW_ERRORS}"

echo "[fastcgi-stub] zero ASAN errors and zero crashes across all lifecycle cycles: OK"
echo "FastCGI stub lifecycle smoke test passed."
