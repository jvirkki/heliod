#!/bin/bash
#
# TLS smoke test for Phase 1a security hardening.
#
# Creates a temporary Heliod instance with an HTTPS listener, generates a
# self-signed NSS certificate, starts the server, and validates the listener
# with openssl s_client.
#
# This script is intentionally self-contained and destructive to the temporary
# instance directory it creates.

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <heliod-work-root>"
    echo "Example: $0 /tmp/heliod-tls-smoke/work/Linux_debian_f_x86_64_32_OPT"
    exit 1
fi

WORK_ROOT="$1"
HOSTNAME="tls-smoke"
INSTANCE_NAME="https-${HOSTNAME}"
INSTANCE_DIR="${WORK_ROOT}/${INSTANCE_NAME}"
HTTP_PORT=18088
HTTPS_PORT=18443

cd "${WORK_ROOT}"

# Clean up any previous run.
for pid in $(pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null || true); do
    kill -9 "$pid" 2>/dev/null || true
done
rm -rf "${INSTANCE_DIR}"

# Create an HTTP-only instance first.
./bin/create-instance -hostname "${HOSTNAME}" -user "${USER}" -port "${HTTP_PORT}"

# Create an NSS certificate database in the instance directory. Use the old DBM
# format (cert8.db/key3.db) because the legacy server expects SECMOD_DB.
cd "${INSTANCE_DIR}"
certutil -N -d dbm:"." --empty-password 2>/dev/null || certutil -N -d dbm:"." -f <(echo "")

# Create a self-signed certificate (and key) with serverAuth EKU, then
# import the PKCS#12 bundle into the NSS database so the private key is
# available for TLS.
openssl req -x509 -newkey rsa:2048 -keyout /tmp/tls-smoke-key.pem \
    -out /tmp/tls-smoke-cert.pem -days 90 -nodes \
    -subj "/CN=${HOSTNAME}" \
    -addext "subjectAltName=DNS:${HOSTNAME}" \
    -addext "extendedKeyUsage=serverAuth" \
    -addext "keyUsage=digitalSignature,keyEncipherment"

openssl pkcs12 -export -out /tmp/tls-smoke.p12 \
    -inkey /tmp/tls-smoke-key.pem -in /tmp/tls-smoke-cert.pem \
    -name "Server-Cert" -passout pass:

pk12util -i /tmp/tls-smoke.p12 -d dbm:"." -W ''

# Verify the certificate exists.
certutil -L -d dbm:"." -n "Server-Cert"

# Modify server.xml to add an HTTPS listener and attach it to the existing
# virtual-server created by create-instance.
export HOSTNAME INSTANCE_NAME HTTPS_PORT
python3 - <<'PY'
import os
import xml.etree.ElementTree as ET

path = 'config/server.xml'
tree = ET.parse(path)
root = tree.getroot()

https_listener = ET.SubElement(root, 'http-listener')
ET.SubElement(https_listener, 'name').text = 'https-listener-1'
ET.SubElement(https_listener, 'port').text = os.environ['HTTPS_PORT']
ET.SubElement(https_listener, 'server-name').text = os.environ['HOSTNAME']
ET.SubElement(https_listener, 'default-virtual-server-name').text = os.environ['INSTANCE_NAME']

ssl = ET.SubElement(https_listener, 'ssl')
ET.SubElement(ssl, 'enabled').text = 'true'
ET.SubElement(ssl, 'server-cert-nickname').text = 'Server-Cert'

# Find the existing virtual-server and add a reference to the HTTPS listener.
for vs in root.findall('virtual-server'):
    if vs.findtext('name') == os.environ['INSTANCE_NAME']:
        ET.SubElement(vs, 'http-listener-name').text = 'https-listener-1'
        break

tree.write(path, xml_declaration=True, encoding='UTF-8')
PY

# Tell the server where the NSS certificate database lives.
echo "NSSDir ${PWD}" >> config/magnus.conf

# Start the server.
ulimit -n 4096
./bin/startserv &
SERVER_PID=$!

# Give the server time to start.
sleep 5

cleanup() {
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        ./bin/stopserv 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    for pid in $(pgrep -f "webservd.*${INSTANCE_DIR}" 2>/dev/null || true); do
        kill -9 "$pid" 2>/dev/null || true
    done
    rm -rf "${INSTANCE_DIR}" /tmp/tls-smoke.p12 /tmp/tls-smoke-key.pem /tmp/tls-smoke-cert.pem /tmp/tls-smoke-ca.pem
}
trap cleanup EXIT

# First verify the HTTP port is serving; if not, the server did not start.
if ! curl -sf "http://localhost:${HTTP_PORT}" | grep -q Welcome; then
    echo "Server failed to start on HTTP port ${HTTP_PORT}; aborting TLS tests."
    exit 1
fi
echo "HTTP listener OK"

CA_FILE="/tmp/tls-smoke-ca.pem"
certutil -L -d dbm:"." -n "Server-Cert" -a > "${CA_FILE}"

run_client() {
    openssl s_client -connect "localhost:${HTTPS_PORT}" "$@" -CAfile "${CA_FILE}" </dev/null 2>/dev/null
}

# Verify TLS 1.2 handshake succeeds.
run_client -tls1_2 | grep -q 'Verify return code: 0' || { echo "TLS 1.2 failed"; exit 1; }
echo "TLS 1.2 OK"

# Verify TLS 1.3 handshake succeeds (requires OpenSSL 1.1.1+).
run_client -tls1_3 | grep -q 'Verify return code: 0' || { echo "TLS 1.3 failed"; exit 1; }
echo "TLS 1.3 OK"

# Verify ALPN negotiation advertises http/1.1.
run_client -tls1_2 -alpn http/1.1 | grep -q 'ALPN protocol: http/1.1' || { echo "ALPN failed"; exit 1; }
echo "ALPN http/1.1 OK"

# Verify legacy protocols are rejected.
if run_client -tls1 | grep -q 'Verify return code: 0'; then
    echo "TLS 1.0 unexpectedly succeeded"; exit 1
fi
echo "TLS 1.0 rejected OK"

if run_client -tls1_1 | grep -q 'Verify return code: 0'; then
    echo "TLS 1.1 unexpectedly succeeded"; exit 1
fi
echo "TLS 1.1 rejected OK"

# If we get here, all checks passed.
echo "TLS smoke test passed."
