# Heliod Smoke Testing

This directory contains the lightweight integration/smoke tests used during
protocol modernization and other large changes.  The tests are designed to be
run from a freshly built work tree on a build host (Ubuntu 26.04 LTS is the
reference environment).

## Test harness overview

The entry point is the top-level `make -C test` target.  It is expected to run
after a full `make` + `make install`:

```bash
make SBC=/opt/heliod-sbc
make install SBC=/opt/heliod-sbc
make -C test
```

`make install` creates a fixed `https-test` instance in the work tree.  The
smoke tests reuse that instance: they save its original `config/`, install a
temporary self-signed certificate and listener configuration, run the checks,
and restore the original configuration on exit.

This avoids the `create-instance` and `post-install` tooling, which is not
fully wired in the open-source tree (the `https-template` directory and
installer templates required by `create-instance` are not present).

## Test configuration requirements

A smoke-test run needs the following to exercise HTTP/1.1, event I/O, TLS,
and HTTP/2:

- A work root produced by `make install`.
- The `https-test` instance in the work root.
- `python3`, `openssl`, `curl`, `certutil`, `pk12util`, `ss`.
- A self-signed server certificate imported into the instance's NSS DB under
  the nickname `Server-Cert` (generated at test time).
- `server.xml` temporarily modified to:
  - set `platform` to `64`
  - point `temp-path` at a test-created directory
  - set `io-model=event` on HTTP listener(s)
  - add an HTTPS listener with `ssl.enabled=true` and
    `ssl.server-cert-nickname=Server-Cert`
- `magnus.conf` with `NSSDir <config-dir>`.

## Scripts

- `event_smoke.sh <work-root>` — event-driven HTTP/1.1 parity tests
  (keep-alive, pipelining, 100-continue, chunked, slow client, slowloris,
  concurrency, TLS, idle connections).  Reuses `https-test`.
- `tls_smoke.sh <work-root>` — TLS version and ALPN checks.  Reuses
  `https-test`.
- `h2_smoke.sh <work-root>` — HTTP/2 ALPN negotiation (`openssl s_client
  -alpn h2`) and a `curl --http2` request.  Reuses `https-test`.
- `h2_lifecycle_smoke.sh <work-root>` — HTTP/2 connection *lifecycle*: session
  persistence across event-loop dispatches, resume after an idle period, many
  more concurrent H2 connections than worker threads, and multiple in-flight
  streams on one connection.  Pins `min-threads`/`max-threads` to 8 and opens 32
  connections; override the count with `H2_LIFECYCLE_CONNS`.  Reuses
  `https-test`.  This is the regression guard for the blocking
  `H2Session::run()` that capped H2 concurrency at the worker-thread count.
- `h2_parallel_streams_smoke.sh <work-root>` — HTTP/2 stream *concurrency*:
  streams on one connection execute in parallel rather than one after another.
  The lifecycle test above proves 20 streams are all answered but cannot tell
  whether they were answered concurrently, because a static file is served too
  fast to time; this one uses a CGI that sleeps and calibrates against a
  single-stream baseline measured on the same host, so it does not depend on how
  fast the machine is.  Also covers the two paths that only exist once dispatch
  is concurrent — `RST_STREAM` of a stream that is mid-pipeline, and the peer
  disconnecting with streams in flight — and scans the error log, because a
  use-after-free there shows up as a crash rather than a wrong answer.  Pins the
  thread pool to 8 and installs a `/cgi-bin` mapping for the duration of the run.
  Reuses `https-test`.  This is the regression guard for running the SAF pipeline
  inside the nghttp2 callback, which serialized every stream on a connection.
- `h2_streaming_response_smoke.sh <work-root>` — HTTP/2 response *streaming*: a
  response reaches the client as it is produced rather than being collected in
  memory first.  Reads the server's `VmHWM` from `/proc` while a 64 MB download is
  held at its flow-control window — a buffered body would show up as tens of
  megabytes of peak RSS — then reopens the window and verifies every byte by
  position, so a ring segment handed out twice or out of order is caught.  Also
  covers a trickling CGI (must arrive line by line, not all at once when the
  program exits), a 64 MB chunked CGI response (de-chunked in transit), a sibling
  stream served on a connection whose other stream is stalled, and recovery after
  the whole worker pool is left holding abandoned downloads.  A final section
  caps the HTTP/2 stream pool with the `Http2MaxStreams` magnus.conf directive
  (set to 32 for the run), opens more streams than that which cannot complete,
  and checks the excess is refused with `RST_STREAM(REFUSED_STREAM)` — not a
  GOAWAY, and not buffered without bound — and that the freed slots let a fresh
  client be served once the holder goes away.  Pins the thread pool
  to 8, writes a 64 MB file into the docroot, and installs a `/cgi-bin` mapping
  with `flushTimer="0"` for the duration of the run — without that last part the
  HTTP filter's own 3-second response buffering, not the transport, would decide
  when a trickling CGI's bytes moved.  Takes a couple of minutes, most of it the
  two 64 MB transfers.  Reuses `https-test`.  This is the regression guard for
  buffering an entire response body in RAM before sending the first byte, for
  the `H2StreamSocket` methods left at their layered NSPR defaults (which crashed
  the worker on any `writev` or `sendfile`), and for the per-stream `Connection`
  pool that bounds total in-flight streams.  Three magnus.conf knobs tune the HTTP/2
  limits: `Http2MaxStreams` (process-wide cap on live streams; default
  self-scales from the thread-pool size), `Http2MaxConcurrentStreams` (the
  per-connection `SETTINGS_MAX_CONCURRENT_STREAMS` advertised to clients; default
  100), and `Http2MaxRequestBodySize` (per-stream cap on the buffered request body;
  default 16 MB, and a lower `MaxRequestBodySize` wins where one is set).  The last
  one has no HTTP/1.1 equivalent for a reason: an HTTP/2 stream is not dispatched
  until END_STREAM, so its whole body is buffered before the pipeline runs and flow
  control cannot be what bounds it — crediting only as the pipeline consumed the body
  would deadlock, because the pipeline does not start until the body is complete.  A
  stream over the cap is refused with `RST_STREAM(ENHANCE_YOUR_CALM)` rather than a
  413, there being no response to put a status on.
- `h2_request_end_smoke.sh <work-root>` — HTTP/2 *request completion*: a stream
  runs when its request is complete, and the only thing that says so is
  END_STREAM.  Drives every frame it can arrive on — on HEADERS, on a DATA frame
  with a payload, on a DATA frame with **no** payload, and on an empty DATA frame
  that terminates a body — then the same shapes multiplexed eight to a connection
  and mixed with ordinary streams, and finally a body over
  `Http2MaxRequestBodySize`, where completing the request is precisely what must
  *not* happen.  Speaks h2c with prior knowledge on 18180, so it needs no
  certificate and exercises the same cleartext upstream leg a reverse proxy uses;
  installs a `/cgi-bin` echo script so the two body cases can assert on
  `CONTENT_LENGTH` and the bytes rather than just a 200, and pins
  `Http2MaxRequestBodySize` to 1 KB so the refusal case costs a few KB instead of
  16 MB.  Reuses `https-test`.  Each case runs on its own connection and its own
  `try`, so a hang reports as one failed case instead of ending the run.
  This is the regression guard for **the empty final DATA frame**: END_STREAM on
  a DATA frame was noticed only in nghttp2's data-chunk callback, and nghttp2
  does not call that callback for a DATA frame with no payload bytes, so nothing
  marked the request complete and the stream was never dispatched — for ever, on
  a connection that went on serving its other streams perfectly.  Its own
  documentation says as much: *"you should use `nghttp2_on_frame_recv_callback`
  to know all data frames are received"*.  That shape is not exotic.  Go's
  HTTP/2 client sends an empty final DATA frame for any request whose body is
  non-nil but of unknown length, and a reverse proxy produces exactly that when
  the request it is forwarding arrived over HTTP/3: quic-go cannot know the
  length up front, so `ContentLength` is `-1`, so `net/http/httputil`'s
  `if outreq.ContentLength == 0 { outreq.Body = nil }` shortcut does not fire and
  the body survives.  In front of Caddy with an h2c upstream this hung **every**
  HTTP/3 request while HTTP/1.1 and HTTP/2 clients — whose bodyless requests do
  get END_STREAM on HEADERS — were served normally, which is what made it look
  like a client fault.  Reverting the upstream to HTTP/1.1 fixed it because there
  the same request is just an empty chunked body.  The refusal case guards the
  other half: a stream whose body was refused has been RST_STREAMed and its
  buffer abandoned part-way, so the completion path must decline it rather than
  hand a SAF a truncated request.
- `h2spec_smoke.sh <work-root>` — HTTP/2 *protocol conformance* via
  [h2spec](https://github.com/summerwind/h2spec): framing, flow control, stream
  states, HPACK, and error handling — the systematic RFC 9113 check the
  hand-written smoke tests do not attempt.  h2spec is a single Go binary and is
  **not** built here; install a release binary once and put it on `PATH` or point
  `H2SPEC` at it (`curl -sSL .../h2spec_linux_amd64.tar.gz | tar -xz`).  If it is
  not found the script SKIPs (exit 0) rather than failing; set `H2SPEC_REQUIRED=1`
  to make a missing binary fail.  Debug affordances: `H2_CORES=1` lets the worker
  dump core (for catching an intermittent crash under a case), `H2SPEC_LOOP=N`
  repeats the run to shake out a rare crash, `H2SPEC_KEEP=1` leaves the server up
  and config in place for reproducing under gdb, and `H2SPEC_ARGS` passes a
  section id (e.g. `H2SPEC_ARGS=http2/6.9` to run just part of the suite).  Reuses
  `https-test`.  **Current result: 138/146.** The 8 remaining failures are known
  deltas that reflect nghttp2's own default behavior, not defects in this server:
  nghttp2 1.68 relaxes deprecated RFC 7540 priority validation (PRIORITY on stream
  0, self-dependency), silently tolerates several closed/half-closed stream edge
  cases (DATA/HEADERS after RST_STREAM or on a closed stream, a numerically
  smaller stream id), and answers a stream flow-control-window overflow with a
  connection GOAWAY(FLOW_CONTROL_ERROR) rather than the stream-level RST_STREAM
  h2spec expects — RFC 9113 §6.9.1 permits either.  This suite is also the
  regression guard for the double-enqueue use-after-free in
  `WorkerPool::runWorker()` (a frame with END_STREAM on an already-dispatched
  stream re-queued the stream, and the second `runJob()` ran on freed memory) and
  for HTTP/2 trailers not being dispatched.
- `h2_idle_timeout_smoke.sh <work-root>` — event-loop idle reclaim: an idle
  HTTP/2 connection is closed on its deadline and told so with a GOAWAY, an
  actively used connection is left alone, and an idle event-mode HTTP/1.1
  keep-alive connection is closed too.  Starts the server with
  `HELIOD_EVENT_IDLE_TIMEOUT=5` so the run takes well under a minute.  Reuses
  `https-test`.  This is the regression guard for the event loop having no timer
  facility, which left parked connections unreclaimable.
- `compression_smoke.sh <work-root>` — HTTP content-compression: the
  `http-compression` output filter (`safs/httpcompression.cpp`) now offers Brotli
  (`br`) and Zstandard (`zstd`) in addition to gzip.  Serves a compressible file
  through the filter over plain HTTP on 18180 and checks the `Accept-Encoding`
  negotiation matrix (br/gzip/zstd, all preference combinations, q-values,
  identity/none → uncompressed) and that each coding round-trips: the bytes on the
  wire are actually smaller and decode back to the original (curl, which
  advertises brotli and zstd, is the decoder).  gzip is the regression half of
  this test; br and zstd are the added halves.  Reuses `https-test`.  Notes: on a
  tie the three modern codings are preferred **br > zstd > gzip** (client q-values
  override); Brotli quality defaults to 5 and zstd level to 3 (speed/ratio
  balances for on-the-fly compression, each on its own scale, unrelated to zlib's
  1-9); the precompressed-`.gz` SAFs (`find-compressed`/`compress-file`) are
  gzip-only and unchanged.
- `worker_pool_smoke.sh <work-root>` — event-model worker-pool *elasticity*: the
  event I/O `WorkerPool` must start at `min-threads` and grow to `max-threads` on
  demand (never shrinking), mirroring the blocking `DaemonSession` pool, rather
  than pre-spawning `max` at startup.  Pins `min-threads=2, max-threads=32`, runs
  the instance at `log-level finest`, and reads the pool's own growth log
  (`WorkerPool: grew to N of M`): it asserts the peak is exactly `min` at idle
  (proving `max` was not pre-spawned) and grows beyond `min` — bounded by `max` —
  under a burst of concurrent slow CGIs.  Reuses `https-test` over plain HTTP on
  18180.  This is the regression guard for the event pool reverting to a
  fixed-size, pre-spawn-max design.
- `fastcgi_h2_smoke.sh <work-root>` — FastCGI served over HTTP/2.  The FastCGI
  responder (`plugins/fastcgi`) is a `PR_Poll()`-driven relay between the
  application socket and the client socket; over HTTP/2 the client is a synthetic
  `H2StreamSocket` with no underlying OS fd, which broke the relay in three ways
  this test guards: (1) the synthetic request never ran
  `DaemonSession::prepareConnection()`, so `serverIP` was NULL and the responder's
  `SERVER_ADDR` env var crashed the worker (`util_env_str` → `strlen(NULL)`) on the
  first request; (2) an un-pollable client makes `PR_Poll()` return
  `PR_POLL_NVAL` instantly every iteration, so the responder never blocked for the
  application's response and never timed out — a worker pegged at 100% CPU forever,
  one per request; (3) when the client left mid-response the worker had to unwind
  rather than buffer the application's output without bound.  Drives **php-fpm** as
  a remote FastCGI server on `127.0.0.1:19000`, sets up an event/TLS listener, and
  checks: an H2 FastCGI GET returns 200 with a populated `SERVER_ADDR`; no worker
  thread spins after it completes; a mid-response client disconnect leaves no
  spinner and no runaway RSS growth; and the server stays healthy with no crash in
  the error log.  **SKIPs** (exit 0) if no `php-fpm` binary is found — set
  `PHP_FPM` to point at one.  Reuses `https-test`.  This is the regression guard
  for the FastCGI-over-HTTP/2 crash, busy-spin, and buffering defects.
- `h2_cookie_smoke.sh <work-root>` — `Cookie` reassembly on the HTTP/2 request path.
  RFC 7540 §8.1.2.5 lets a client split `Cookie` into one header field per cookie
  ("crumbs") for HPACK efficiency and **requires** the receiver to rejoin them with
  `"; "` before handing the request to a non-HTTP/2 context such as a CGI or FastCGI
  application.  Go's HTTP/2 transport does exactly this splitting, so every request
  Caddy proxies over h2c arrives split.  heliod rebuilt the HTTP/1.1 request with one
  `Cookie:` line per crumb and its parser folded the duplicates with `", "`
  (`httprequest.cpp`, where `lduphdrs` marks `cookie` duplicable) — correct for a
  list-valued header, wrong for `Cookie`, whose delimiter is `;`.  The application
  then saw a single cookie whose value had swallowed all the others.  Seven cases over
  h2c on 18180 against a CGI that echoes `HTTP_COOKIE` and `HTTP_ACCEPT_ENCODING`: a
  single cookie field (the HTTP/1.1 shape, the control that always worked); three
  crumbs; two crumbs; a phpBB-shaped `_u`/`_k`/`_sid` set asserted on the *parsed* jar
  rather than the raw string, because that is what the application sees; a cookie value
  legitimately containing a comma, which is why a folded `Cookie` cannot be recovered
  downstream by splitting on `","`; crumbs *separated by other header fields*, since
  §8.2.3 puts no ordering requirement on them and the rejoin walks the whole field list
  rather than a run of neighbours; and a repeated `Accept-Encoding` that must still fold
  with `", "`, which pins the other side of the contract — `Cookie` is the only field
  rejoined here, and the h2 path is not entitled to a header policy of its own.  This is
  the regression guard for logged-in sessions breaking behind an HTTP/2 reverse proxy —
  note it is invisible to any test that sends only one cookie, and invisible to a
  browser speaking HTTP/1.1 to the same server.
- `h2_vs_h11_perf.sh <work-root>` — HTTP/2 vs HTTP/1.1 throughput, the benchmark
  that originally exposed the H2 defects by showing HTTP/2 at roughly **half**
  HTTP/1.1's throughput.  Both arms are driven by the same tool against the same
  ~8.8 KB asset on the same TLS/event listener on 18443, so the only variable is the
  protocol: `h2load --h1` negotiates http/1.1 by ALPN, plain `h2load` negotiates h2.
  Runs the pair `REPEAT` times (default 3) and reports the mean, because a single
  sample is noise — the load generator competes with the server for CPU, as both are
  on the same host.  Fails if the ratio falls below `MIN_RATIO` (default 0.80), or if
  any run did not have every request succeed: a run that drops requests can still
  post a healthy req/s, so the rate only means something once the request tally is
  clean.  Full `h2load` reports are kept in `RESULTS_DIR` (default
  `/tmp/h2-vs-h11-perf`).  Tunable: `REQUESTS` (20000), `CLIENTS` (50), `STREAMS`
  (32, streams per connection for the h2 arm), `REPEAT`, `MIN_RATIO`, `H2LOAD`.
  **SKIPs** (exit 0) if `h2load` is not found — install the `nghttp2-client`
  package, or extract it without root with `apt-get download nghttp2-client
  libev4t64` and `dpkg-deb -x`, then point `H2LOAD` at the binary.  Reuses
  `https-test`; **generates real load, so run it alone.**
  Read the two numbers as answering different questions.  Throughput says HTTP/2 is
  no longer the slow path.  Per-request latency is *expected* to be worse for h2
  here and is not a defect: the h2 arm carries `CLIENTS × STREAMS` requests in
  flight (50 × 32 = 1600) against HTTP/1.1's 50, so each request queues behind its
  siblings.  Comparing latency between the arms is only meaningful at matched
  concurrency (`-m 1` for the h2 arm).

## Diagnosing a live HTTP/2 instance

`/perfdump` (the `service-dump` SAF) has two sections that describe the event
model and HTTP/2, both scoped to the process that serves the request rather than
aggregated across processes:

```
Event/Async thread pools (this process):
----------------------------
EventWorkerPool:
Idle/Live/Min/Max             2/3/2/128
Work Queue Length             0
EventLoop Connections         0

HTTP/2 (this process):
----------------------------
Sessions Live/Peak/Total      0/5/62
Streams InUse/Peak/Max        0/2/2048
Stream Structs Allocated      2
Streams Refused (pool full)   0
```

That capture is a real one, taken after 62 h2c connections (40 of them
concurrent) had been served and drained: `Sessions Live` and `Streams InUse` are
back to 0 while the peaks record the burst, which is exactly what a healthy
connection lifecycle looks like.  The `Streams …`/`Stream Structs …` lines are
omitted entirely until the process has served at least one HTTP/2 session,
because the stream pool is a lazily-created singleton and rendering a stats page
must not be the thing that constructs it.

Reading them:

- `EventWorkerPool` **Live never shrinks** — it is a high-water mark, not a
  measure of current load.  A large Live with a large Idle means a past burst,
  not a present problem.  `Work Queue Length` is the number of items waiting for
  a worker; a persistently non-zero value is real back-pressure.
- HTTP/2 `Sessions Live` is one per HTTP/2 connection.  If it climbs and stays
  high while the peers are not actually holding that many connections open
  (compare against `ss -tn state established '( sport = :PORT )' | wc -l`),
  sessions are being leaked — a failure that is invisible from the client's end,
  because the socket still looks healthy there.
- `Streams Peak` reaching `Max`, or any non-zero `Streams Refused`, means streams
  were turned away with `RST_STREAM(REFUSED_STREAM)` and clients saw errors or
  retries.  Raise the cap with `Http2MaxStreams` in `magnus.conf`.
- The `Sessions:` list is per **worker thread**, not per connection, and a row
  appears for any thread whose stats slot is not idle/keep-alive.  Rows with a
  Status but blank Age/VS/Protocol/Method/URI were a bug in the event path (the
  slot's mode was never reset when the worker finished), fixed by resetting it in
  `DaemonSession::processConnection()` and `H2Stream::dispatchRequest()`.  On a
  build without that fix, treat those rows — and the inflated "Active Sessions"
  count — as reporting noise rather than stuck requests.

Behind Caddy or another reverse proxy, reach `/perfdump` **directly** rather than
through the proxy when diagnosing a hang, so the proxy's own connection pooling
is not part of what you are measuring:

```bash
curl --http2-prior-knowledge -s http://127.0.0.1:8080/perfdump   # h2c
curl -s http://127.0.0.1:8080/perfdump                           # HTTP/1.1
```

## Reproducing the reverse-proxy h2c topology

None of the suites above puts a real reverse proxy in front of the server, and the
h2c hang that prompted the event-path lifetime work only ever appeared that way:
Caddy multiplexing many client connections onto a few long-lived h2c connections
to heliod.  Recreating it takes a Caddyfile and two loops.

```caddyfile
{
	admin off
	auto_https off
}
:8091 {
	log {
		output file /tmp/caddy-h2c.log
		format console
	}
	reverse_proxy 127.0.0.1:18190 {
		transport http {
			versions h2c
		}
	}
}
```

The document tree needs `page.txt`, a `bigpage.txt` of at least a few hundred KB
(one flow-control window is 64 KB, so a body needing several `WINDOW_UPDATE`s is a
different test from a small one), a CGI that echoes the request body back, and a
CGI that sleeps for `$QUERY_STRING` seconds.  `make install` rewrites `docs/` and
`obj.conf`, so this has to be redone after every install: the `cgi-bin` `pfx2dir`
mapping and the `/perfdump` `assign-name` both go in the default object *ahead of*
the `PathCheck` lines, and `/perfdump` also needs `Init fn="perf-init"` in
`magnus.conf`.

Two things to measure, both of which need a **fixed** thread pool
(`<min-threads>8</min-threads><max-threads>8</max-threads>`) so that saturation is
reached predictably:

- **A mixed load at high concurrency** — 50-way, a few thousand requests of
  static/big/CGI-GET/CGI-POST — censused by *status and download size per URL*, not
  just by status.  A truncated body is the failure this catches and a status-only
  check misses: every URL should produce exactly one size.
- **Convoy behaviour** — N concurrent 4-second CGIs on one h2c connection.  Up to
  the pool's width this must complete in ~4s total; beyond it, in whole waves.
  Anything longer means streams are serialising in the protocol layer rather than
  queueing for a worker.

Always take the same measurement **direct over HTTP/1.1** as well.  A slow static
response while 20 CGIs are in flight looks alarming until the h1 baseline shows the
identical time — the wait is worker starvation, and the proxy and h2c contribute
nothing to it.

## Running

```bash
make SBC=/opt/heliod-sbc
make install SBC=/opt/heliod-sbc
make -C test                    # default: run h2_smoke.sh
make -C test test-event         # run event_smoke.sh
make -C test test-tls           # run tls_smoke.sh
make -C test test-h2-lifecycle  # run h2_lifecycle_smoke.sh
make -C test test-h2-parallel   # run h2_parallel_streams_smoke.sh
make -C test test-h2-stream     # run h2_streaming_response_smoke.sh
make -C test test-h2-request-end # run h2_request_end_smoke.sh (h2c, no cert needed)
make -C test test-h2-cookie     # run h2_cookie_smoke.sh (h2c, no cert needed)
make -C test test-h2-conformance # run h2spec_smoke.sh (needs h2spec on PATH)
make -C test test-h2-perf       # run h2_vs_h11_perf.sh (needs h2load; generates load)
make -C test test-compression   # run compression_smoke.sh (gzip + brotli + zstd)
make -C test test-worker-pool   # run worker_pool_smoke.sh (event pool growth)
make -C test test-h2-idle       # run h2_idle_timeout_smoke.sh
make -C test test-fastcgi-h2    # run fastcgi_h2_smoke.sh (needs php-fpm; else SKIPs)
```

### Rebuilding after a header change

The build system does not track header dependencies.  A change to a widely
included header — `connqueue.h` above all, which defines `Connection` — leaves
object files behind that were compiled against the old struct layout, and mixing
them produces field reads at the wrong offsets rather than a link error.  Delete
every object and library before rebuilding:

```bash
find . -name "*.o" -delete
find . -name "*.so" -delete
find . -name "*.a" -delete
make SBC=/opt/heliod-sbc && make install SBC=/opt/heliod-sbc
```

Then confirm the installed library really contains the new code, because the
link step is also unreliable about noticing that it needs to re-run.  **Do not
use `nm` for this.**  `libns-httpd40.so` is linked with `-s`, so it is stripped:
`nm -C` reports "no symbols" outright, and `nm -DC` sees only the dynamic symbol
table, which does not include most `heliod::` internals — a symbol you just added
will be missing whether or not the relink happened.  Grep for a **string literal**
you just added instead; that survives stripping:

```bash
strings work/*/lib/libns-httpd40.so | grep 'HTTP/2 (this process)'
```

Beware substring false positives if you do reach for `nm`: `grep liveSessions`
matches the unrelated `DaemonSession::nKeepAliveSessions_`.

The specific stale-link trap: `src/server/webservd/<plat>/libns-httpd40.so` is
the real link output, and `make stage` only `cp -f`s it into `work/<plat>/lib`.
That link rule does **not** depend on the per-directory archives
(`liblibsafs.a`, `libhttpdaemon.a`, …), so recompiling a `.cpp` updates its `.o`
and its `.a` and then the `.so` is copied unchanged — with a *newer* mtime on the
copy, which makes it look freshly built.  Force it:

```bash
rm -f src/server/webservd/*/libns-httpd40.so src/server/webservd/*/webservd
make SBC=/opt/heliod-sbc stage
```

Pass an **absolute** work root when invoking these scripts by hand; they `cd`
into it and then into the instance config directory.  `h2_lifecycle_smoke.sh`
resolves its argument to an absolute path itself; the older scripts do not.

All tests restore the `https-test/config` and stop the server on exit.  If a
test is interrupted, a `.pristine` backup and the temporary files in `/tmp`
may be left behind and should be removed manually.

## Running an instance under valgrind memcheck

Worth reaching for the moment a crash stops being reproducible in one place. The
intermittent worker SIGSEGV on the HTTP/2 streaming path cost days of gdb work — three
different crash sites in three different allocators, and ctor/dtor pairing
instrumentation that correctly reported no double free — and memcheck named the
use-after-free, with the freeing stack and the allocating stack, on the first round.

The worker is not the process you launch. `webservd-wdog` execs the primordial
`webservd`, which forks the worker, so `--trace-children=yes` is not optional: without
it memcheck watches the watchdog and misses everything. Wrap the whole tree by editing
`SERVER_BIN` in a copy of the instance's `startserv`:

```bash
sed -e 's|^SERVER_BIN=webservd-wdog|SERVER_BIN="valgrind --tool=memcheck \
    --trace-children=yes --trace-children-skip=/bin/*,/usr/bin/*,/sbin/* \
    --undef-value-errors=no --num-callers=25 --error-limit=no \
    --log-file=/tmp/vg.%p.log webservd-wdog"|' \
    <instance>/bin/startserv > /tmp/vgserv.sh
chmod +x /tmp/vgserv.sh && /tmp/vgserv.sh --start
```

`--undef-value-errors=no` matters more than it looks: NSS and NSPR produce a great deal
of uninitialised-value noise that buries real reports, and turning it off roughly halves
the slowdown. Leave it on only when an uninitialised read is what you are hunting.

Two practical notes. The library must be unstripped or the stacks are useless — build
with `make stage LD_DEBUG=`, since `make/defines_Linux.mk` otherwise passes `-s`. And
poll the listen port rather than sleeping to decide the server is up; start-up is slow
enough to fool a fixed sleep, though 20 s was enough in practice on the build host.

Scale the workload down. A 64 MB static file is pointless under memcheck: anything past
the flow-control window leaves the producer parked, which is all the abandoned-producer
scenario needs, so 4 MB does the same job many times faster.

Read the log by looking for the *first* report and reading all three of its stacks —
where the bad access happened, where the block was freed, and where it was allocated.
Also check `grep -c "Invalid free\|Mismatched free"`: zero there rules out a double free
and is what distinguishes "somebody is using a freed block" from "somebody freed it
twice", which look identical from a crash dump.

Launch it detached, with `setsid nohup /tmp/vgserv.sh --start >/tmp/vgstart.log 2>&1
</dev/null &`.  A server started from an ssh session dies with that session, and a worker
that vanished on hangup is indistinguishable at a glance from one that crashed — which
here produced an hour of chasing a crash that had not happened.

Finding the processes afterwards is fiddly because valgrind execs its tool, so the process
is not called `valgrind`: on the build host the command line is `/bin/valgrind.bin`, and
the tool's own name, `memcheck-amd64-linux`, is over pgrep's 15-character `comm` limit, so
`pgrep -x` silently matches nothing whatever you give it.  Match the command line
instead — `pgrep -u "$(id -u)" -f 'valgrind.bin --tool'`.  Two cautions with `pkill -f`
here: never use a pattern that also appears in your own command line (over ssh it kills
the shell running the script, which returns 255 and skips everything after it), and never
`pkill` on the name `webservd`, because production runs `webservd` too.  `stopserv` alone
was enough to bring the instrumented instance down in practice.

**A clean memcheck run does not close a race.**  The slowdown changes the interleaving:
the reference-count fix reached zero invalid reads/writes under memcheck while still
crashing once in fifteen rounds at full speed.  Treat memcheck as the tool that *names*
the bug and a full-speed soak as the one that decides whether it is gone.
