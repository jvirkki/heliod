#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2015 Jyri J. Virkki
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

BUILD_ROOT=..

include $(BUILD_ROOT)/make/base.mk

# Smoke tests for the protocol modernization workstream.  The event, TLS and
# HTTP/2 tests use the https-test instance produced by `make install` and
# always restore it when they finish.  See TESTING.md for prerequisites.

SMOKE_H2_LOG=$(WORK_ROOT)/.smoke-h2.log

all: tests

# HTTP/2 smoke test: ALPN h2 negotiation and a curl --http2 request.
# This is the primary target run by `make -C test`.
test-h2:
	@echo "----- Running HTTP/2 smoke test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 smoke test passed -----"

# HTTP/2 connection lifecycle test: pins an 8-thread worker pool and then opens
# far more simultaneous H2 connections than that.  Guards against a regression
# to the blocking H2Session::run(), which capped H2 connection concurrency at
# the worker-thread count.
test-h2-lifecycle:
	@echo "----- Running HTTP/2 lifecycle test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_lifecycle_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 lifecycle test passed -----"

# Event-loop idle timeout test: proves the loop reclaims connections that sit
# idle past their deadline (with a GOAWAY for HTTP/2) and leaves busy ones
# alone.  Runs the server with a short HELIOD_EVENT_IDLE_TIMEOUT so it finishes
# in well under a minute.
test-h2-idle:
	@echo "----- Running event-loop idle timeout test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_idle_timeout_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- idle timeout test passed -----"

# HTTP/2 concurrent stream test: proves streams on one connection execute in
# parallel rather than one after another, by timing sleeping CGIs against a
# calibrated single-stream baseline.  Guards against a regression to running the
# SAF pipeline inside the nghttp2 callback, which serialized every stream on a
# connection behind the thread driving it.
test-h2-parallel:
	@echo "----- Running HTTP/2 concurrent stream test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_parallel_streams_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 concurrent stream test passed -----"

# HTTP/2 streaming response test: proves a response reaches the client as it is
# produced instead of being collected in memory first.  Watches the server's peak
# RSS while a 64 MB download is held at its flow-control window, checks a
# trickling CGI arrives line by line, and verifies every byte of a 64 MB static
# and a 64 MB chunked response by position.  Guards against a regression to
# buffering the whole body before the first byte is sent.
#
# The longest of these targets: the two 64 MB transfers dominate, so allow a
# couple of minutes.
test-h2-stream:
	@echo "----- Running HTTP/2 streaming response test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_streaming_response_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 streaming response test passed -----"

# HTTP/2 request-completion test: drives every frame END_STREAM can ride on,
# including a DATA frame that carries END_STREAM and no payload.  That last shape
# used to hang for ever -- nghttp2 does not call the data-chunk callback for an
# empty DATA frame, so nothing marked the request complete -- and it is what a
# reverse proxy sends for every request that reached it over HTTP/3.  Speaks h2c
# with prior knowledge, so it needs no certificate.
test-h2-request-end:
	@echo "----- Running HTTP/2 request-completion test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_request_end_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 request-completion test passed -----"

# HTTP/2 cookie reassembly test: RFC 7540 section 8.1.2.5 lets a client split the
# Cookie header into one field per cookie and requires the receiver to rejoin them
# with "; " before the request reaches a non-HTTP/2 context.  Go's h2 transport --
# so every reverse proxy dialing an h2c upstream -- does that splitting, and
# heliod's duplicate-header fold uses ", ", which leaves the application one cookie
# whose value has swallowed the rest.  Sends multi-cookie requests already split,
# the shape a single-cookie smoke test cannot see.  Speaks h2c with prior
# knowledge, so it needs no certificate.
test-h2-cookie:
	@echo "----- Running HTTP/2 cookie reassembly test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_cookie_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 cookie reassembly test passed -----"

# HTTP/2 client-address test: an HTTP/2 stream is dispatched on a worker that
# never ran prepareConnection(), which is the one place a connection's peer
# address reaches the thread's realtime stats slot -- so perfdump's "Client"
# column printed whatever that worker last recorded: 0.0.0.0, or the peer of an
# unrelated earlier connection, which behind a reverse proxy looks exactly like a
# real remote client.  Every connection here is made from a different loopback
# source address, so the address perfdump owes us is known rather than merely
# plausible.  Also covers the reverse-proxy shape ("Reported Client").  Speaks
# h2c with prior knowledge, so it needs no certificate.
test-h2-client-ip:
	@echo "----- Running HTTP/2 client-address test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_client_ip_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 client-address test passed -----"

# HTTP/2 protocol conformance test (h2spec).  Runs the h2spec suite against the
# H2 listener: framing, flow control, stream states, HPACK, error handling.  This
# is the RFC-conformance guard for Phase 3.  h2spec is not built here; install the
# release binary and put it on PATH or set H2SPEC (see TESTING.md).  If h2spec is
# absent the script SKIPs rather than fails.  A handful of cases are known deltas
# that reflect nghttp2's own default behavior (relaxed RFC 7540 priorities and
# some tolerated stream-state edges) rather than defects here -- see TESTING.md.
test-h2-conformance:
	@echo "----- Running HTTP/2 conformance (h2spec) test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2spec_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 conformance test finished -----"

# Generates real load -- run it alone, and note the load generator competes with the
# server for CPU on this host.  SKIPs if h2load is not on PATH.
test-h2-perf:
	@echo "----- Running HTTP/2 vs HTTP/1.1 throughput comparison -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/h2_vs_h11_perf.sh $(realpath $(WORK_ROOT))
	@echo "----- HTTP/2 vs HTTP/1.1 comparison finished -----"

# Content-compression test: the http-compression output filter now offers Brotli
# (br) alongside gzip.  Checks the Accept-Encoding negotiation matrix and that
# each coding round-trips (compressed on the wire, decodes back to the original),
# with gzip behaviour unchanged.  Uses curl as the br/gzip decoder.
test-compression:
	@echo "----- Running content-compression test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/compression_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- content-compression test passed -----"

# Event worker-pool elasticity test: the event model's WorkerPool must start at
# min-threads and grow to max-threads on demand (never shrink), mirroring the
# classic DaemonSession pool -- not pre-spawn max at startup.  Reads the pool's
# growth log at finest level and drives concurrent slow CGIs to force growth.
test-worker-pool:
	@echo "----- Running event worker-pool elasticity test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/worker_pool_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- worker-pool elasticity test passed -----"

# Classic DaemonSession pool sizing test: the pool must size on demand (one
# thread per busy session, one per queued connection, plus a spare) rather than
# one per open socket -- idle keep-alive connections parked in PollManager must
# not pull threads into existence.  Reads perfdump's "Total Sessions Created"
# and "KeepAliveCount" while holding idle keep-alives open, then under a burst
# of concurrent slow CGIs.
test-daemon-pool:
	@echo "----- Running classic daemon-pool sizing test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/daemon_pool_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- daemon-pool sizing test passed -----"

# Classic/event pool isolation test: with no io-model=classic listener the
# classic DaemonSession pool must start empty and no keep-alive poll threads
# may exist; and traffic on event listeners must never grow the classic pool
# (the shared nActiveSessions_ counter used to leak event demand into the
# classic pool's sizing).  Also verifies a real classic listener still grows
# its pool.  Reads the LOG_VERBOSE pool-growth lines at log-level finest.
test-event-pool-isolation:
	@echo "----- Running classic/event pool isolation test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/event_pool_isolation_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- pool isolation test passed -----"

# FastCGI-over-HTTP/2 test: the FastCGI responder relays between the application
# socket and the client socket with PR_Poll(); the HTTP/2 client socket is a
# synthetic descriptor with no OS fd, which used to crash (NULL SERVER_ADDR),
# then busy-spin a worker (un-pollable client -> PR_Poll never blocks), and leak
# memory when the client left mid-response.  Drives php-fpm as a remote FastCGI
# server; SKIPs if no php-fpm binary is present (set PHP_FPM to override).
test-fastcgi-h2:
	@echo "----- Running FastCGI-over-HTTP/2 test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/fastcgi_h2_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- FastCGI-over-HTTP/2 test finished -----"

# FastCGI stub process (Fastcgistub) lifecycle and ASAN stress test: compiles
# Fastcgistub with AddressSanitizer + UndefinedBehaviorSanitizer and verifies
# CLI/framing edge cases, invalid bind paths, nonexistent executables,
# concurrent duplicate RQ_START bursts, RQ_OVERLOAD, backend process death and
# automatic respawn, and repeated server restart cycles.
test-fastcgi-stub:
	@echo "----- Running FastCGI stub lifecycle and ASAN test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/fastcgi_stub_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- FastCGI stub lifecycle and ASAN test passed -----"

# Per-connection request cap test: verifies max-requests-per-connection
# across classic HTTP/1.1, event HTTP/1.1, HTTP/2 GOAWAY, per-listener overrides,
# and /perfdump.
test-max-requests:
	@echo "----- Running max-requests-per-connection test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/max_requests_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- max-requests-per-connection test passed -----"

# Byte-range and Range header DoS protection test: verifies single and
# multipart byte ranges, 416 unsatisfiable ranges, HEAD multi-range framing,
# the range count cap (default 20, configurable via max-ranges), and rejection
# of overlapping/descending/amplification Range headers with 200 OK fallback.
test-range:
	@echo "----- Running byte-range and Range DoS test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/range_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- byte-range and Range DoS test passed -----"

# Quality of Service (QoS) and dynamic reconfiguration under load test:
# verifies partial <qos-limits> (<max-connections> only, <max-bps> only) at
# both VS and server scope, 503 enforcement via qos-handler/qos-error,
# stats-xml reporting, and repeated dynamic reconfigurations under concurrent
# HTTP/1.1 (classic + event) and HTTP/2 load.
test-qos:
	@echo "----- Running QoS and dynamic reconfiguration test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/qos_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- QoS and dynamic reconfiguration test passed -----"

# Reverse proxy configuration, routing, sticky session, and dynamic
# reconfiguration under load test: verifies single-origin and multi-origin
# set-origin-server routing, Location/Set-Cookie reverse-map rewriting,
# JROUTE/URI sticky sessions, clean rollback on invalid route configuration,
# and repeated dynamic reconfigurations under concurrent proxy traffic.
test-proxy:
	@echo "----- Running reverse-proxy smoke test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/proxy_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- reverse-proxy smoke test passed -----"

# PAM authentication conversation and password-aging lifecycle test: compiles
# src/server/libaccess/pamauth.c under ASAN/UBSAN against a mock PAM framework
# modeled on Solaris libpam free_resp() and verifies password-aging warnings,
# expired-password errors, PAM_PROMPT_ECHO_OFF responses, and user/group lookup
# error paths.
test-pam:
	@echo "----- Running PAM auth smoke test -----"
	$(BUILD_ROOT)/test/pam_auth_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- PAM auth smoke test passed -----"

# htaccess plugin directive parsing, authentication, IP/domain access control,
# and dynamic .htaccess rewrite under concurrent load test: runs standalone
# ASAN/UBSAN and TSAN tests of src/server/plugins/htaccess/* and cryptwrapper
# plus a live webservd directive + 100ms dynamic .htaccess rewrite load test.
test-htaccess:
	@echo "----- Running htaccess smoke test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/htaccess_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- htaccess smoke test passed -----"

# Event I/O smoke test (uses the https-test instance from make install).
test-event:
	@echo "----- Running event I/O smoke test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/event_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- event I/O smoke test passed -----"

# TLS smoke test (uses the https-test instance from make install).
test-tls:
	@echo "----- Running TLS smoke test -----"
	ulimit -n 4096
	$(BUILD_ROOT)/test/tls_smoke.sh $(realpath $(WORK_ROOT))
	@echo "----- TLS smoke test passed -----"

# Default target runs the HTTP/2 smoke.  Run the other variants explicitly with
# `make -C test test-event`, `test-tls`, `test-h2-lifecycle`, `test-h2-idle`,
# `test-h2-parallel`, `test-h2-stream`, `test-h2-request-end`, `test-h2-cookie`,
# `test-h2-client-ip`, `test-h2-conformance`, `test-h2-perf`, `test-compression`,
# `test-worker-pool`, `test-daemon-pool` or `test-event-pool-isolation`.
tests: test-h2
	@echo "----- tests completed -----"
