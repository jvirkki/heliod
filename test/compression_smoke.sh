#!/bin/bash
#
# HTTP content-compression smoke test (Phase 5).
#
# Exercises the http-compression output filter (safs/httpcompression.cpp),
# which now offers Brotli (br) alongside the original gzip.  Checks the
# Accept-Encoding negotiation matrix and that each coding round-trips: the bytes
# on the wire are actually compressed and decode back to the original.  gzip
# behaviour must be unchanged (it is the regression half of this test); br is the
# added half.
#
# Plain HTTP on 18180 -- compression is orthogonal to TLS, so no cert is needed.
# Reuses the https-test instance; saves and restores its config.
#
# curl is the decoder: `curl --compressed` decodes both br and gzip (this build
# of curl advertises brotli), so a decoded-equals-original check needs no extra
# tooling.
#
# Usage: test/compression_smoke.sh <heliod-work-root>

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    exit 1
fi

WORK_ROOT="$1"
INSTANCE="https-test"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE}"
CONFIG_DIR="${INSTANCE_DIR}/config"
DOCROOT="${INSTANCE_DIR}/docs"
PORT=18180
BASE="http://localhost:${PORT}"
TEMP_DIR=""
SERVER_PID=""
FAILURES=()

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cleanup() {
    if [ -n "${SERVER_PID:-}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        "${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
    fi
    pkill -9 -f "Linux_debian_f_x86_64_32_OPT/https-test" 2>/dev/null || true
    if [ -d "${CONFIG_DIR}.pristine" ]; then
        rm -rf "${CONFIG_DIR}"; mv "${CONFIG_DIR}.pristine" "${CONFIG_DIR}"
    fi
    rm -f "${DOCROOT}/comptest.html" 2>/dev/null || true
    [ -n "${TEMP_DIR:-}" ] && [ -d "${TEMP_DIR}" ] && rm -rf "${TEMP_DIR}"
    rm -f /tmp/comp-*.bin 2>/dev/null || true
}
trap cleanup EXIT

cd "${WORK_ROOT}"
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pkill -9 -f "Linux_debian_f_x86_64_32_OPT/https-test" 2>/dev/null || true
sleep 1

[ -d "${CONFIG_DIR}.pristine" ] || cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"
TEMP_DIR=$(mktemp -d /tmp/${INSTANCE}-comp-XXXXXX)
chmod 700 "${TEMP_DIR}"

# A compressible payload: repetitive text so every coding shrinks it clearly.
python3 - "${DOCROOT}/comptest.html" <<'PY'
import sys
line = "The quick brown fox jumps over the lazy dog. " * 20 + "\n"
with open(sys.argv[1], "w") as f:
    f.write("<!doctype html><title>c</title>\n")
    for _ in range(2000):
        f.write(line)
PY
ORIG_SIZE=$(wc -c < "${DOCROOT}/comptest.html")
echo "[comp] test payload: ${ORIG_SIZE} bytes"

# Configure: platform 64, temp-path, http-listener-1 -> 18180 (8080 is a
# production port on this host), event I/O.
python3 - <<PY
import xml.etree.ElementTree as ET, os
path = os.path.join("${CONFIG_DIR}", "server.xml")
t = ET.parse(path); r = t.getroot()
tp = r.find("temp-path")
if tp is None: tp = ET.SubElement(r, "temp-path")
tp.text = "${TEMP_DIR}"
plat = r.find("platform")
if plat is None: plat = ET.SubElement(r, "platform")
plat.text = "64"
for l in r.findall("http-listener"):
    if l.findtext("name") == "http-listener-1":
        p = l.find("port")
        if p is None: p = ET.SubElement(l, "port")
        p.text = "${PORT}"
        for c in list(l):
            if c.tag == "io-model": l.remove(c)
        ET.SubElement(l, "io-model").text = "event"
with open(path, "wb") as f:
    f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n'); t.write(f, xml_declaration=False, encoding="UTF-8")
PY

# Route responses through the compression filter: add the Output insert-filter to
# the default object (obj.conf ships it only in a compress-on-demand object that
# nothing assigns by default).
python3 - <<PY
import re, os
path = os.path.join("${CONFIG_DIR}", "obj.conf")
s = open(path).read()
ins = 'Output fn="insert-filter" filter="http-compression"'
# Config is restored from pristine each run, so a single unconditional insert
# right after the default object opens is correct.
s2 = re.sub(r'(<Object name="default">\n)', r'\1' + ins + '\n', s, count=1)
assert s2 != s, "obj.conf: no default object to add the compression filter to"
open(path, "w").write(s2)
PY

"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-comp.log 2>&1 &
SERVER_PID=$!
waited=0
while [ "${waited}" -lt 20 ]; do
    ss -tln 2>/dev/null | grep -q ":${PORT} " && break
    sleep 1; waited=$((waited+1))
done

check() { # name  condition-rc  detail
    if [ "$2" -eq 0 ]; then echo "[comp] $1: OK $3";
    else echo "[comp] $1: FAIL $3"; FAILURES+=("$1"); fi
}

# Wire encoding for a given Accept-Encoding: prints the Content-Encoding header.
wire_ce() { # accept-encoding
    curl -s -D - -o /dev/null -H "Accept-Encoding: $1" "${BASE}/comptest.html" \
      | tr -d '\r' | awk 'tolower($1)=="content-encoding:"{print $2}'
}
raw_size() { # accept-encoding  -> bytes on the wire (compressed)
    curl -s -H "Accept-Encoding: $1" "${BASE}/comptest.html" | wc -c
}
decoded_ok() { # accept-encoding -> 0 if curl-decoded body equals original
    curl -s --compressed -H "Accept-Encoding: $1" "${BASE}/comptest.html" \
      > /tmp/comp-dec.bin
    cmp -s /tmp/comp-dec.bin "${DOCROOT}/comptest.html"
}
vary_has_ae() { # accept-encoding
    curl -s -D - -o /dev/null -H "Accept-Encoding: $1" "${BASE}/comptest.html" \
      | tr -d '\r' | grep -iq '^vary:.*accept-encoding'
}

# --- Brotli (the new half) ---
CE=$(wire_ce "br")
check "br: Content-Encoding is br" "$([ "$CE" = br ] && echo 0 || echo 1)" "(got '${CE}')"
BRSZ=$(raw_size "br")
check "br: body is actually compressed" "$([ "$BRSZ" -lt "$ORIG_SIZE" ] && echo 0 || echo 1)" "(${BRSZ} < ${ORIG_SIZE})"
check "br: decodes back to the original" "$(decoded_ok br; echo $?)" ""
check "br: Vary: accept-encoding present" "$(vary_has_ae br; echo $?)" ""

# --- gzip (the regression half) ---
CE=$(wire_ce "gzip")
check "gzip: Content-Encoding is gzip" "$([ "$CE" = gzip ] || [ "$CE" = x-gzip ] && echo 0 || echo 1)" "(got '${CE}')"
GZSZ=$(raw_size "gzip")
check "gzip: body is actually compressed" "$([ "$GZSZ" -lt "$ORIG_SIZE" ] && echo 0 || echo 1)" "(${GZSZ} < ${ORIG_SIZE})"
check "gzip: decodes back to the original" "$(decoded_ok gzip; echo $?)" ""

# --- zstd (the other new half) ---
CE=$(wire_ce "zstd")
check "zstd: Content-Encoding is zstd" "$([ "$CE" = zstd ] && echo 0 || echo 1)" "(got '${CE}')"
ZSTDSZ=$(raw_size "zstd")
check "zstd: body is actually compressed" "$([ "$ZSTDSZ" -lt "$ORIG_SIZE" ] && echo 0 || echo 1)" "(${ZSTDSZ} < ${ORIG_SIZE})"
check "zstd: decodes back to the original" "$(decoded_ok zstd; echo $?)" ""

# --- negotiation ---
CE=$(wire_ce "gzip, br")
check "both offered: br is preferred" "$([ "$CE" = br ] && echo 0 || echo 1)" "(got '${CE}')"
CE=$(wire_ce "gzip, zstd, br")
check "all three offered: br is preferred" "$([ "$CE" = br ] && echo 0 || echo 1)" "(got '${CE}')"
CE=$(wire_ce "gzip, zstd")
check "zstd preferred over gzip on a tie" "$([ "$CE" = zstd ] && echo 0 || echo 1)" "(got '${CE}')"
CE=$(wire_ce "zstd;q=1.0, br;q=0.5")
check "q-values honoured: zstd wins over br when preferred" "$([ "$CE" = zstd ] && echo 0 || echo 1)" "(got '${CE}')"
CE=$(wire_ce "br;q=0.5, gzip;q=1.0")
check "q-values honoured: gzip wins when preferred" "$([ "$CE" = gzip ] || [ "$CE" = x-gzip ] && echo 0 || echo 1)" "(got '${CE}')"
CE=$(wire_ce "identity")
check "identity only: no compression" "$([ -z "$CE" ] || [ "$CE" = identity ] && echo 0 || echo 1)" "(got '${CE:-<none>}')"
IDSZ=$(curl -s "${BASE}/comptest.html" | wc -c)
check "no Accept-Encoding: served uncompressed" "$([ "$IDSZ" -eq "$ORIG_SIZE" ] && echo 0 || echo 1)" "(${IDSZ} == ${ORIG_SIZE})"

# --- error log clean ---
ERRLOG="${INSTANCE_DIR}/logs/errors"
if [ -f "${ERRLOG}" ] && tail -50 "${ERRLOG}" | grep -qiE "SIGSEGV|catastrophe|Brotli.*error|zlib"; then
    check "error log clean" 1 "(see ${ERRLOG})"
else
    check "error log clean" 0 ""
fi

echo "[comp] sizes: original=${ORIG_SIZE}B  br=${BRSZ}B  zstd=${ZSTDSZ}B  gzip=${GZSZ}B" \
     "(br $(awk "BEGIN{printf \"%.2f\", ${BRSZ}*100/${ORIG_SIZE}}")%," \
     "zstd $(awk "BEGIN{printf \"%.2f\", ${ZSTDSZ}*100/${ORIG_SIZE}}")%," \
     "gzip $(awk "BEGIN{printf \"%.2f\", ${GZSZ}*100/${ORIG_SIZE}}")% of original)"

if [ ${#FAILURES[@]} -ne 0 ]; then
    echo "compression smoke test FAILED: ${FAILURES[*]}"
    exit 1
fi
echo "compression smoke test passed."
