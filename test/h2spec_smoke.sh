#!/bin/bash
#
# HTTP/2 protocol conformance test (h2spec).
#
# Runs the h2spec conformance suite (https://github.com/summerwind/h2spec)
# against the https-test instance's HTTP/2 listener.  This is the RFC-conformance
# check for Phase 3: framing, flow control, stream states, HPACK, error handling
# and the generic request/response cases, none of which the hand-written smoke
# tests exercise systematically.
#
# h2spec is a single Go binary and is not built here.  Install it once (a release
# binary is enough -- no Go toolchain needed) and either put it on PATH or point
# H2SPEC at it:
#   curl -sSL https://github.com/summerwind/h2spec/releases/download/v2.6.0/h2spec_linux_amd64.tar.gz | tar -xz
#   mv h2spec ~/bin/            # or set H2SPEC=/path/to/h2spec
# If h2spec is not found the test SKIPs (exit 0) with a message rather than
# failing, so a tree without it still builds green; set H2SPEC_REQUIRED=1 to turn
# a missing binary into a failure.
#
# Optional overrides:
#   H2SPEC        path to the h2spec binary (default: search PATH, then ~/bin)
#   H2SPEC_ARGS   extra args, e.g. a section id to run just part of the suite:
#                 H2SPEC_ARGS="http2/6.9" make -C test test-h2-conformance
#
# Usage:
#   test/h2spec_smoke.sh <heliod-work-root>

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
HTTPS_PORT=18443
TEMP_DIR=""
SERVER_PID=""

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

# Locate h2spec before touching anything, so a SKIP costs nothing.
H2SPEC_BIN="${H2SPEC:-}"
if [ -z "${H2SPEC_BIN}" ]; then
    if command -v h2spec >/dev/null 2>&1; then
        H2SPEC_BIN="$(command -v h2spec)"
    elif [ -x "${HOME}/bin/h2spec" ]; then
        H2SPEC_BIN="${HOME}/bin/h2spec"
    fi
fi
if [ -z "${H2SPEC_BIN}" ] || [ ! -x "${H2SPEC_BIN}" ]; then
    echo "[h2spec] SKIP: h2spec binary not found (set H2SPEC or put it on PATH)."
    if [ "${H2SPEC_REQUIRED:-0}" = "1" ]; then
        echo "[h2spec] H2SPEC_REQUIRED=1, treating missing binary as failure."
        exit 1
    fi
    exit 0
fi
echo "[h2spec] using ${H2SPEC_BIN} ($(${H2SPEC_BIN} --version 2>&1 | head -1))"

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
    # Debug aid: H2SPEC_KEEP=1 leaves the configured instance running and its
    # config in place, so a crash can be reproduced under gdb outside this
    # script.  Restore by hand afterwards (the .pristine copy is left in place).
    if [ "${H2SPEC_KEEP:-0}" = "1" ]; then
        echo "[h2spec] KEEP: leaving server (pid ${SERVER_PID:-?}) running; config NOT restored."
        echo "[h2spec] pristine config saved at ${CONFIG_DIR}.pristine"
        return
    fi
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

TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-h2spec-XXXXXX)
chmod 700 "${TEMP_DIR}"

# Self-signed cert for the HTTPS listener.
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout /tmp/h2spec-key.pem \
    -out /tmp/h2spec-cert.pem -days 90 -nodes \
    -subj "/CN=localhost" \
    -addext "subjectAltName=DNS:localhost" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out /tmp/h2spec.p12 \
    -inkey /tmp/h2spec-key.pem -in /tmp/h2spec-cert.pem \
    -name "Server-Cert" -passout pass: 2>/dev/null
pk12util -i /tmp/h2spec.p12 -d dbm:"." -w "${NO_PASS}"
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
    cat /tmp/${INSTANCE}-h2spec.log 2>/dev/null || true
    return 1
}

cd "${INSTANCE_DIR}"
configure_instance
# Debug aid: with H2_CORES=1 the worker is allowed to dump core (core_pattern is
# a plain "core" in the worker's CWD on this host), so an intermittent crash
# under a conformance case can be caught and backtraced.  Off by default.
if [ "${H2_CORES:-0}" = "1" ]; then
    ulimit -c unlimited
    echo "[h2spec] core dumps enabled (ulimit -c = $(ulimit -c))"
fi

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-h2spec.log 2>&1 &
SERVER_PID=$!
wait_for_listener

ERRLOG="${INSTANCE_DIR}/logs/errors"
ERRLOG_START=0
[ -f "${ERRLOG}" ] && ERRLOG_START=$(wc -l < "${ERRLOG}")

# Run the suite.  -t = TLS, -k = don't verify the self-signed cert, -o 5 caps the
# per-case timeout so a hung case cannot stall the run.  Extra args (e.g. a
# section id) come from H2SPEC_ARGS.
H2SPEC_LOG=$(mktemp /tmp/h2spec-out-XXXXXX.txt)
echo "[h2spec] running conformance suite against localhost:${HTTPS_PORT} ..."
# H2SPEC_LOOP>1 repeats the run to shake out an intermittent crash; the loop
# stops early the moment the server error log shows a crash, leaving the core.
LOOPS="${H2SPEC_LOOP:-1}"
H2SPEC_RC=0
set +e
for iter in $(seq 1 "${LOOPS}"); do
    [ "${LOOPS}" -gt 1 ] && echo "[h2spec] --- iteration ${iter}/${LOOPS} ---"
    "${H2SPEC_BIN}" -t -k -h localhost -p "${HTTPS_PORT}" -o 5 ${H2SPEC_ARGS:-} \
        2>&1 | tee "${H2SPEC_LOG}"
    H2SPEC_RC=${PIPESTATUS[0]}
    if [ -f "${ERRLOG}" ] && tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
           grep -qE 'SIGSEGV|CORE3260|catastrophe'; then
        echo "[h2spec] crash detected on iteration ${iter}; stopping the loop"
        break
    fi
done
set -e

# h2spec prints a final summary line like:
#   146 tests, 145 passed, 1 skipped, 0 failed
SUMMARY=$(grep -E '[0-9]+ tests, [0-9]+ passed' "${H2SPEC_LOG}" | tail -1)
echo "[h2spec] summary: ${SUMMARY:-<no summary line>}"

# Scan the server error log for a crash or assertion during the run -- a
# conformance case that trips a use-after-free shows up as a crash, not a
# reported failure.
if [ -f "${ERRLOG}" ]; then
    NEW_ERRORS=$(tail -n +$((ERRLOG_START + 1)) "${ERRLOG}" |
                 grep -E 'SIGSEGV|SEGV|segfault|Assertion|assertion|PR_ASSERT|catastrophe' || true)
    if [ -n "${NEW_ERRORS}" ]; then
        echo "[h2spec] server error log reports a crash or assertion:"
        echo "${NEW_ERRORS}"
        rm -f "${H2SPEC_LOG}"
        exit 1
    fi
fi
echo "[h2spec] no crashes or assertions in server error log"

FAILED=$(printf '%s' "${SUMMARY}" | grep -oE '[0-9]+ failed' | grep -oE '[0-9]+' || echo "")
rm -f "${H2SPEC_LOG}"

# This is a regression guard, not an absolute-conformance gate.  A known set of
# cases fail because of nghttp2 1.68's own default behaviour (relaxed RFC 7540
# priority validation, tolerated closed-stream edges, GOAWAY-vs-RST on a stream
# flow-window overflow), not defects here -- see TESTING.md and the plan document.
# So the test passes as long as the failure count does not exceed that documented
# baseline, and fails if a regression pushes it higher (or h2spec did not run).
# Override the baseline with H2SPEC_ALLOWED_FAILURES if the picture changes.
ALLOWED="${H2SPEC_ALLOWED_FAILURES:-8}"

if [ -z "${FAILED}" ]; then
    echo "[h2spec] no summary line parsed -- h2spec did not complete a run"
    exit 1
fi
if [ "${FAILED}" -gt "${ALLOWED}" ]; then
    echo "[h2spec] conformance REGRESSED: ${FAILED} failures exceeds the"
    echo "         documented baseline of ${ALLOWED} nghttp2-level deltas."
    exit 1
fi
echo "[h2spec] conformance OK: ${FAILED} failure(s), within the documented"
echo "         baseline of ${ALLOWED} nghttp2-level deltas (see TESTING.md)."
