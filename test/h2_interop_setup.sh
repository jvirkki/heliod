#!/bin/bash
#
# HTTP/2 real-browser interop setup (Phase 3 exit criterion).
#
# Unlike the smoke tests, this does NOT restore the config or stop the server on
# exit: it configures the https-test instance for HTTP/2 over TLS on an
# externally reachable port and LEAVES IT RUNNING so a real browser (Chrome,
# Firefox) can connect from another machine and multiplex streams over one
# connection.  A pristine copy of the config is saved so it can be restored
# afterwards; this script prints how.
#
# The server certificate is self-signed, so a browser will warn on first
# connect -- that is expected; accept/proceed.  ALPN still negotiates h2 through
# the warning, which is all the interop check needs.  The cert's SAN is built
# from the host's own names/addresses (hostname, global IPv4s, and the AWS public
# IPv4 via IMDS if reachable) so the name at least matches when possible.
#
# Usage:
#   test/h2_interop_setup.sh <heliod-work-root>
# Optional env:
#   HTTPS_PORT (default 18443), HTTP_PORT (default 18180)
#   EXTRA_SAN  extra comma-separated SAN entries, e.g. "DNS:h2.example.com,IP:203.0.113.5"

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
HTTPS_PORT="${HTTPS_PORT:-18443}"
HTTP_PORT="${HTTP_PORT:-18180}"
TEMP_DIR="/tmp/${INSTANCE}-interop"

if [ ! -d "${INSTANCE_DIR}" ]; then
    echo "Instance ${INSTANCE_DIR} not found. Run 'make install' first."
    exit 1
fi

cd "${WORK_ROOT}"

# Stop any running instance so we can rewrite its config.
"${INSTANCE_DIR}/bin/stopserv" 2>/dev/null || true
pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null | xargs -r kill -9 2>/dev/null || true
sleep 1

# Save a pristine copy of the config the first time only, so re-running this
# script does not overwrite the real pristine with an already-modified one.
if [ ! -d "${CONFIG_DIR}.pristine" ]; then
    cp -r "${CONFIG_DIR}" "${CONFIG_DIR}.pristine"
fi

mkdir -p "${TEMP_DIR}"
chmod 700 "${TEMP_DIR}"

# --- Build the certificate SAN list from the host's own identity ------------
HOSTS_DNS="DNS:$(hostname),DNS:localhost"
IPS=""
for ip in $(hostname -I 2>/dev/null); do
    IPS="${IPS},IP:${ip}"
done
IPS="${IPS},IP:127.0.0.1"
# AWS public IPv4 via IMDSv2, best effort.
PUB=""
TOKEN=$(curl -sS -m 3 -X PUT "http://169.254.169.254/latest/api/token" \
        -H "X-aws-ec2-metadata-token-ttl-seconds: 60" 2>/dev/null || true)
if [ -n "${TOKEN}" ]; then
    PUB=$(curl -sS -m 3 -H "X-aws-ec2-metadata-token: ${TOKEN}" \
          http://169.254.169.254/latest/meta-data/public-ipv4 2>/dev/null || true)
fi
[ -n "${PUB}" ] && IPS="${IPS},IP:${PUB}"
SAN="${HOSTS_DNS}${IPS}"
[ -n "${EXTRA_SAN:-}" ] && SAN="${SAN},${EXTRA_SAN}"

echo "[interop] certificate SAN: ${SAN}"

# --- Self-signed cert into the instance NSS DB ------------------------------
cd "${CONFIG_DIR}"
NO_PASS=$(mktemp)
printf '\n' > "${NO_PASS}"
# Fresh NSS DB (idempotent: -N fails if one exists, so only create if absent).
[ -f cert8.db ] || [ -f cert9.db ] || certutil -N -d dbm:"." -f "${NO_PASS}"
openssl req -x509 -newkey rsa:2048 -keyout "${TEMP_DIR}/key.pem" \
    -out "${TEMP_DIR}/cert.pem" -days 365 -nodes \
    -subj "/CN=$(hostname)" \
    -addext "subjectAltName=${SAN}" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment" 2>/dev/null
openssl pkcs12 -export -out "${TEMP_DIR}/server.p12" \
    -inkey "${TEMP_DIR}/key.pem" -in "${TEMP_DIR}/cert.pem" \
    -name "Server-Cert" -passout pass: 2>/dev/null
# Replace any prior Server-Cert so re-runs do not accumulate nicknames.
certutil -D -d dbm:"." -n "Server-Cert" 2>/dev/null || true
pk12util -i "${TEMP_DIR}/server.p12" -d dbm:"." -w "${NO_PASS}"
grep -q "^NSSDir " magnus.conf || echo "NSSDir ${PWD}" >> magnus.conf
rm -f "${NO_PASS}"
cd "${WORK_ROOT}"

# --- Configure the instance: event I/O + an h2/TLS listener -----------------
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

# Drop any prior https-listener-1 so re-runs are idempotent.
for listener in root.findall("http-listener"):
    if listener.findtext("name") == "https-listener-1":
        root.remove(listener)

for listener in root.findall("http-listener"):
    name = listener.findtext("name")
    if name == "http-listener-1":
        port_el = listener.find("port")
        if port_el is None:
            port_el = ET.SubElement(listener, "port")
        port_el.text = "${HTTP_PORT}"
        for child in list(listener):
            if child.tag == "io-model":
                listener.remove(child)
        ET.SubElement(listener, "io-model").text = "event"

# New HTTPS listener, no explicit <ip> so it binds all interfaces (INADDR_ANY).
https = ET.SubElement(root, "http-listener")
ET.SubElement(https, "name").text = "https-listener-1"
ET.SubElement(https, "port").text = "${HTTPS_PORT}"
ET.SubElement(https, "server-name").text = os.uname().nodename
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

# --- A small multi-resource page so multiplexing is visible in DevTools -----
cat > "${DOCROOT}/h2test.html" <<'HTML'
<!doctype html><html><head><meta charset="utf-8"><title>heliod HTTP/2 interop</title>
<link rel="stylesheet" href="style.css">
</head><body>
<h1>heliod HTTP/2 interop</h1>
<p>If this loaded over h2, DevTools &rarr; Network &rarr; Protocol shows
<code>h2</code> and the resources below share one connection.</p>
<div id="out">loading...</div>
<script src="app.js"></script>
</body></html>
HTML
cat > "${DOCROOT}/style.css" <<'CSS'
body{font-family:sans-serif;max-width:40em;margin:3em auto}
code{background:#eee;padding:.1em .3em}
CSS
cat > "${DOCROOT}/app.js" <<'JS'
// Fetch several resources at once so the browser multiplexes streams on one h2
// connection.  Watch DevTools -> Network: all r0..r7 should be Protocol h2.
Promise.all(Array.from({length:8},(_,i)=>
  fetch('r'+i+'.txt').then(r=>r.text()))).then(t=>{
  document.getElementById('out').textContent =
    'fetched '+t.length+' resources concurrently over one connection';
});
JS
for i in $(seq 0 7); do
    printf 'resource %d\n' "$i" > "${DOCROOT}/r${i}.txt"
done

# --- Start it and leave it running ------------------------------------------
"${INSTANCE_DIR}/bin/startserv" > /tmp/${INSTANCE}-interop.log 2>&1 &
sleep 1

waited=0
while [ "${waited}" -lt 20 ]; do
    if ss -tln 2>/dev/null | grep -q ":${HTTPS_PORT} "; then break; fi
    sleep 1; waited=$((waited + 1))
done

BIND=$(ss -tln 2>/dev/null | grep ":${HTTPS_PORT} " | awk '{print $4}' | head -1)
ALPN=$(echo | timeout 6 openssl s_client -connect "localhost:${HTTPS_PORT}" \
       -alpn h2 2>/dev/null | grep -i "ALPN protocol" || true)

echo
echo "=========================================================================="
echo " heliod HTTP/2 interop instance is RUNNING"
echo "=========================================================================="
echo " HTTPS/HTTP2 listener : port ${HTTPS_PORT}  (bind: ${BIND:-unknown})"
echo " HTTP/1.1 listener    : port ${HTTP_PORT}"
echo " ALPN check (local)   : ${ALPN:-<no h2 negotiated - check logs>}"
echo
echo " Test URLs (accept the self-signed cert warning):"
echo "   https://$(hostname):${HTTPS_PORT}/h2test.html"
[ -n "${PUB:-}" ] && echo "   https://${PUB}:${HTTPS_PORT}/h2test.html   (public)"
for ip in $(hostname -I 2>/dev/null); do
    echo "   https://${ip}:${HTTPS_PORT}/h2test.html"
done
echo
echo " Open TCP ${HTTPS_PORT} (and optionally ${HTTP_PORT}) in the firewall/"
echo " security group to reach it from outside."
echo
echo " Verify h2 in the browser: DevTools -> Network -> Protocol column shows"
echo " 'h2'; r0..r7 on /h2test.html share one connection.  Or chrome://net-export."
echo
echo " Server log : /tmp/${INSTANCE}-interop.log"
echo " To stop    : ${INSTANCE_DIR}/bin/stopserv"
echo " To restore : rm -rf ${CONFIG_DIR} && mv ${CONFIG_DIR}.pristine ${CONFIG_DIR}"
echo "=========================================================================="
