# Heliod Technical Design: Tech Stack Modernization

Status: proposal / working document
Companion documents: `PROTOCOL_GAP_ANALYSIS.md` (what's missing and why),
`MODERNIZATION_IMPLEMENTATION_PLAN.md` (phased delivery + testing)

## 1. Current stack (as-built, grounded in source)

| Layer | Current implementation | Key files |
|---|---|---|
| Threading/concurrency | Acceptor-Worker: `Acceptor` threads `accept()` and enqueue onto `ConnectionQueue`; a pool of `DaemonSession` threads dequeue and block on `HandleRequest()` for the life of one connection | `httpdaemon/daemonsession.h`, `httpdaemon/connqueue.cpp` |
| I/O readiness | NSPR poll wrapped by `PollManager`/`NsprPollAdapter`/`kapollthr.cpp` (Solaris `/dev/poll`), used mainly for *keep-alive idle polling*, not general async I/O | `httpdaemon/pollmanager.cpp`, `httpdaemon/kapollthr.cpp`, `httpdaemon/uxpolladapter.cpp` |
| TLS | NSS via a custom `sslconf.cpp`/`servnss.cpp` layer, boolean `SSL_ENABLE_TLS` flag, hardcoded CBC/RC4/3DES cipher table, SSLv2/v3 still selectable | `base/sslconf.cpp`, `base/servnss.cpp`, `base/minissl.h` |
| Request/response model | One `HttpRequest` object embeds one `NSAPISession`/`NSAPIRequest` pair per `DaemonSession`; request framing via `netbuf` (`base/netbuf.cpp`) | `httpdaemon/httprequest.h`, `base/netbuf.cpp` |
| HTTP parsing | Header parsing in `httpheader.cpp`; connection-level semantics (keep-alive, 100-continue, chunked) in `frame/httpfilter.cpp` | `httpdaemon/httpheader.cpp`, `frame/httpfilter.cpp` |
| Extensibility | NSAPI SAF pipeline (`AuthTrans`→`NameTrans`→`PathCheck`→`ObjectType`→`Service`→`AddLog`), configured via `obj.conf`/`server.xml` | `frame/req.cpp`, `httpdaemon/HttpMethodRegistry.cpp` |
| Compression | zlib-based gzip only | `safs/httpcompression.cpp` |
| Build | Recursive `make`, per-platform `defines_*.mk`, SBC (Software Bill of Components) prebuilt deps (MozLDAP, NSS/NSPR, Xerces-C, Xalan-C) | `make/`, `sbc/` |

## 2. Target stack

| Layer | Target | Rationale |
|---|---|---|
| Concurrency | Event-driven reactor (epoll on Linux, kqueue on BSD/macOS, event ports on Solaris) with a bounded worker thread pool for CPU-bound SAF execution; connections are **not** bound 1:1 to OS threads | Required by WebSocket (§ idle connection scaling) and HTTP/2 (§ stream multiplexing); see `PROTOCOL_GAP_ANALYSIS.md` §3 and §5 |
| TLS | Keep NSS *or* migrate to OpenSSL 3.x — see §2.1 decision below. Either way: TLS 1.2 minimum, TLS 1.3 preferred, AEAD-only cipher policy, ALPN callback wired to protocol selection, SNI-based per-hostname certificate selection (dynamic, hostname-keyed cert lookup replacing the fixed-size `servercert[MAX_SERVER_CERTS]` array), and automated certificate lifecycle via ACME. See `ACME_CERTIFICATE_MANAGER_PLAN.md` and `ACME_TLS_ALPN01_DESIGN.md`. | `PROTOCOL_GAP_ANALYSIS.md` §1 |
| HTTP/1.1 | Keep existing `httpheader.cpp`/`httpfilter.cpp` logic, refactored to be driver-agnostic (works whether invoked from blocking or event-driven I/O) | Minimizes risk — HTTP/1.1 semantics are already solid, don't rewrite what works |
| HTTP/2 | New frame-layer module: HPACK codec, per-connection stream table, flow-control windows, mapped onto the *existing* NSAPI SAF pipeline per logical stream | Reuse `Service`/`ObjectType`/etc. SAFs unchanged — HTTP/2 is a framing/multiplexing concern, not a content-generation concern |
| HTTP/3 | Third-party QUIC library (`ngtcp2` + `nghttp3`, or `quiche`) behind the same internal "protocol handler" interface as HTTP/2, terminating into the same SAF pipeline | Avoid writing a QUIC/congestion-control stack from scratch; treat as a transport plugin |
| WebSocket | New protocol handler triggered by `Upgrade: websocket` in the event loop; frames delivered to a distinct SAF-like callback (`WSService`), not reusing `Service` synchronous-return semantics | Needs push-capable, async-friendly callback shape distinct from request/response `Service` SAFs |
| Compression | Add Brotli (`libbrotli`) and optionally zstd alongside existing gzip, content-negotiated via `Accept-Encoding` q-values (logic already exists, just needs new encoders registered) | Additive, low-risk — extends `httpcompression.cpp`'s existing q-value parser |
| Extensibility | NSAPI SAF model preserved as the primary customization point; new protocol layers plug in *below* SAF dispatch, not by replacing it | Minimizes blast radius — third-party SAFs and existing `obj.conf` deployments keep working |
| Build | Existing recursive `make` retained; new modules (`h2/`, `http3/`, `websocket/`) get their own `Makefile`/`objs.mk` following the existing per-component convention (see `HELIOD_BUILD_PLAN.md` commit-grouping precedent) | Consistent with how `libproxy`, `webdav`-equivalent, etc. are already structured |

### 2.1 TLS library selection — decision needed, not prescribed here

NSS and OpenSSL aren't the only options. This section surveys the realistic
candidates for a C/C++ server codebase and evaluates each against Heliod's specific
constraints: BSD 3-clause license (a copyleft dependency would be a real problem),
an existing SBC (Software Bill of Components) prebuilt-dependency build system, a
need for ALPN/SNI/TLS 1.3 today, and an eventual path to HTTP/3-via-QUIC.

| Library | License | TLS 1.3 | ALPN/SNI | QUIC API | Maturity for this use case | Key weakness for Heliod |
|---|---|---|---|---|---|---|
| **NSS** (current) | MPL 2.0 | Yes (library-dependent on version) | Yes, but not wired up in Heliod's glue code (§1.4, §1.5) | Has QUIC-enabling primitives (record-layer separation + secret/HKDF exposure APIs, added ad hoc via Bugzilla bugs 1471126/1529813/1578883/1577643 to support Mozilla's own `neqo` QUIC stack), but **no standardized external QUIC API** — no formal NSS QUIC roadmap document exists; the API shape is bespoke to `neqo`'s needs, not the `SSL_provide_quic_data`/`quic_method` callback convention OpenSSL/BoringSSL/wolfSSL converged on | Battle-tested (Firefox), but Heliod's own `sslconf.cpp`/`servnss.cpp` glue predates modern NSS idioms | No third-party QUIC library (`ngtcp2`, `quiche`, etc.) ships a ready-made NSS backend — integrating NSS-based QUIC would mean following `neqo-crypto`'s bespoke usage pattern rather than a documented, reusable API |
| **OpenSSL 3.x** | Apache 2.0 (since 3.0) | Yes | Yes | Yes — server-side QUIC landed in **OpenSSL 3.5** (`OSSL_QUIC_server_method`), client-side since 3.2 | The de facto default; every QUIC library (`ngtcp2`, `msquic`, `quiche` via BoringSSL-shim) supports it | Heaviest API surface to integrate; 3.0's provider/engine rework broke some legacy call patterns other projects relied on |
| **BoringSSL** | Mixed (OpenSSL + ISC-style), effectively permissive | Yes | Yes | Yes — originated the `SSL_provide_quic_data` QUIC API that OpenSSL/wolfSSL later adopted | Google explicitly states BoringSSL is **not intended for external use**: no stable API/ABI, no versioned releases, no support commitment | Explicitly unsupported for third-party consumption — a real risk for a project without Google-scale in-house maintenance capacity |
| **LibreSSL** | ISC (permissive) | Yes (from 3.x releases) | ALPN yes; historically slower to adopt new extensions | No QUIC API | OpenBSD-maintained, security-hygiene-focused fork; smaller than OpenSSL | Smallest ecosystem of the major forks; no QUIC story at all — would block HTTP/3 (Phase 6) entirely |
| **wolfSSL** | **GPLv3** or commercial | Yes | Yes | Yes — wolfSSL contributed QUIC integration to `ngtcp2`, API modeled on the OpenSSL/quictls QUIC API | Small footprint (20-100KB), strong embedded pedigree, OpenSSL compatibility shim eases porting | **License conflict**: GPLv3 cannot be linked into a BSD-licensed project without triggering GPL obligations on the whole binary; would require purchasing a commercial license — a real cost/legal decision, not just an engineering one |
| **mbedTLS** (Mbed TLS / TrustedFirmware) | Apache 2.0 | Yes | Yes (ALPN/SNI supported) | No native QUIC API (some third-party QUIC stacks embed it as a crypto backend, but it's not a first-class target) | Designed for embedded/constrained devices; smaller API, easier to audit, but far less proven at high-concurrency, many-thousands-of-connections server workloads | Not designed or benchmarked for this workload class; would need real load validation before trusting it under a production web server |
| **GnuTLS** | LGPL 2.1+ | Yes | Yes | No mainstream QUIC integration | Used by several Linux userland tools (wget); permissive enough to link (LGPL allows dynamic/static linking without relicensing the whole project, unlike GPL) | Smaller server-side track record than OpenSSL; slower feature/CVE-response cadence historically |
| **s2n-tls** (AWS) | Apache 2.0 | Yes | Yes | Separate project (`s2n-quic`, Rust); s2n-tls itself has no native QUIC API | Minimalist, formally-verified-subset design philosophy, security-audited by AWS; used in S3/CloudFront | Smaller general-purpose community/docs outside AWS; less prior art integrating it into arbitrary non-AWS C/C++ servers |
| **rustls** (via `rustls-ffi`) | Apache 2.0 / MIT | Yes (TLS 1.2/1.3 only — no legacy protocol code to accidentally misconfigure) | Yes | No QUIC API exposed via `rustls-ffi` today (per upstream maintainer discussion, Sept 2025) | Memory-safe by construction (Rust) — eliminates an entire CVE class (buffer overflows, use-after-free) that has historically hit OpenSSL/NSS | `rustls-ffi` is **explicitly experimental**: no ABI stability guarantee across releases, dynamic linking marked experimental by upstream; adds a Rust toolchain (`cargo-c`) dependency to Heliod's `make`+SBC build system |

### Why NSS/OpenSSL were highlighted as the realistic short-list

- **BoringSSL** is disqualified by its own upstream's explicit "don't use this if you
  aren't Google" guidance — no stable API/ABI is an unacceptable risk for a project
  maintained outside Google's internal lockstep-build model.
- **wolfSSL** is disqualified primarily on **licensing**, not technical merit — GPLv3
  is incompatible with keeping Heliod's BSD license intact unless a commercial
  wolfSSL license is purchased, which is a business decision outside this document's
  scope. If that cost is acceptable, wolfSSL's QUIC support and small footprint make
  it a legitimate second look.
- **LibreSSL** has no QUIC path at all, which would foreclose Phase 6 (HTTP/3)
  outright — disqualifying if HTTP/3 is a stated goal, though it remains a reasonable
  choice if HTTP/3 is explicitly descoped.
- **mbedTLS** and **s2n-tls** are both credible, permissively-licensed, well-audited
  options, but neither has the same depth of prior art running as the TLS layer
  under a general-purpose, high-concurrency web server (their strongest track
  records are embedded/IoT and AWS-internal services, respectively) — worth a
  prototype/bake-off if the team wants a smaller, more auditable codebase than
  OpenSSL, but riskier than OpenSSL as a default choice.
- **rustls** is the most interesting long-term option on pure security grounds
  (memory safety eliminates a whole CVE category) but is disqualified **for now** by
  `rustls-ffi`'s own explicit experimental/no-ABI-stability status and by the
  Rust-toolchain build-system integration cost against Heliod's existing `make`/SBC
  pipeline. Worth revisiting once `rustls-ffi` reaches 1.0 (tracked by upstream as
  gated on the core `rustls` crate reaching 1.x).

### Recommendation

- **Stay on NSS**: lower migration risk (the existing `sslconf.cpp`/`servnss.cpp`
  glue already speaks NSS's `SSL_*` API). Needs: `SSL_VersionRangeSet` instead of the
  boolean `SSL_ENABLE_TLS` flag, `SSL_SetNextProtoNego`/ALPN callback wiring
  (`SSL_SetNextProtoCallback` or `SSL_OptionSet(SSL_ENABLE_ALPN, ...)` depending on
  NSS version), a rewritten cipher table using `SSL_CipherPrefSet` with modern
  AEAD suite IDs, and an `SSL_SNISocketConfigHook` callback that replaces the
  fixed-size `servercert[MAX_SERVER_CERTS]` array (`base/sslconf.cpp:156-186`) with a
  hostname-keyed certificate lookup consulted during the handshake. NSS's QUIC path
  (record-layer separation + secret exposure, as used by Mozilla's own `neqo` stack)
  is real but unstandardized and undocumented as a reusable external API — choosing
  NSS means building a bespoke QUIC integration against `neqo-crypto`'s usage pattern
  rather than plugging into an off-the-shelf `ngtcp2`/`quiche` NSS backend, which
  meaningfully raises Phase 6 (HTTP/3) effort versus the OpenSSL path.
- **Migrate to OpenSSL 3.x**: broader ecosystem support, and as of **OpenSSL 3.5**,
  first-class server-side QUIC support (`OSSL_QUIC_server_method`) — the only
  library in this survey combining a mature general-purpose server track record,
  a permissive license, and production-grade QUIC on both client and server sides
  today. Higher near-term migration cost: `servnss.cpp`/`sslconf.cpp` (~30KB
  combined) would need a substantial rewrite, and `libaccess/lasssl.cpp`
  (SSL-based ACL checks) has an NSS-specific dependency too.

**Bottom line**: if HTTP/3 is in scope at all (even as a later phase), OpenSSL 3.x is
the clear choice — it's the only option in this survey with mature, permissively
licensed, server-side QUIC support today. If HTTP/3 is explicitly out of scope,
staying on NSS and just fixing the cipher/version/ALPN/SNI gaps is the lower-risk,
faster path to closing the CRITICAL/HIGH items in the gap analysis. mbedTLS, s2n-tls,
and (once `rustls-ffi` stabilizes) rustls are reasonable candidates for a future
re-evaluation but are not recommended as the first move given their weaker track
record for this specific workload class or build-system integration cost.

## 3. The core architectural fix: decoupling connection lifetime from thread lifetime

This is the single highest-leverage change, and it directly enables three of the four
missing protocol features (HTTP/2, HTTP/3, WebSocket) rather than requiring three
separate threading rewrites.

### 3.1 Problem statement (grounded in source)

`DaemonSession : public Thread` (`httpdaemon/daemonsession.h` line 83) blocks a
dedicated OS thread on `GetConnection()` → `HandleRequest(netbuf*, timeout)` for the
full lifetime of a connection, including keep-alive re-polling
(`HandleKeepAlive()`). `HttpRequest` (`httpdaemon/httprequest.h` lines 124-146) embeds
exactly one `NSAPISession rqSn` / `NSAPIRequest rqRq` pair — the data model has no
concept of "N logical exchanges in flight on one connection," which is required by
both HTTP/2 stream multiplexing and WebSocket's independent bidirectional message
flow.

### 3.2 Proposed shape

Introduce a **connection object** that is independent of any specific OS thread:

```
Connection
  ├─ fd / TLS session state
  ├─ protocol handler (HTTP/1.1 | HTTP/2 | WebSocket)  [selected via ALPN or Upgrade:]
  └─ N x logical-exchange state
       (HTTP/1.1: exactly 1 in flight, pipelined queue of ready-to-send)
       (HTTP/2:   N concurrent streams, each with its own NSAPISession/NSAPIRequest)
       (WebSocket: 1 duplex message stream, no NSAPIRequest at all)
```

- The event loop (epoll/kqueue/event-ports) owns *readiness*; a small, fixed-size
  worker pool executes SAF/application logic *only* while data is actually available,
  then returns the thread to the pool rather than blocking on the next byte.
- `HttpRequest`'s single embedded `NSAPISession`/`NSAPIRequest` becomes one of
  *possibly several* per-stream request objects owned by the connection, keyed by
  HTTP/2 stream ID (or absent entirely for WebSocket frames).
- Existing SAFs (`Service`, `ObjectType`, `PathCheck`, etc.) are invoked **unchanged**
  per logical HTTP exchange, regardless of whether that exchange arrived via
  HTTP/1.1, an HTTP/2 stream, or (for WS upgrade requests specifically) the initial
  HTTP/1.1 handshake that precedes the protocol switch.

### 3.3 Migration strategy (avoid a big-bang rewrite)

1. Introduce the event loop and `Connection` abstraction **alongside** the existing
   `DaemonSession` blocking model, gated behind a config flag/listener setting.
   New listeners can opt into the event-driven path; existing HTTP/1.1-only
   deployments keep using the proven blocking path unchanged during the transition.
2. Port HTTP/1.1 handling onto the new `Connection`/event-loop model first, with **no
   new protocol features** — this validates the abstraction against the
   best-understood, most heavily tested code path before adding complexity.
3. Add HTTP/2 as a second protocol handler on the same `Connection` abstraction.
4. Add WebSocket as a third protocol handler.
5. Retire the old blocking `DaemonSession` path once the event-driven path has
   demonstrated parity in the test suite (see `MODERNIZATION_IMPLEMENTATION_PLAN.md`).
6. HTTP/3/QUIC is naturally a fourth protocol handler, but sits on top of a UDP
   listener rather than the TCP `ListenSocket`/`Acceptor` path — it can be developed
   in parallel without depending on steps 1-5, since it doesn't share the TCP
   `Connection` object at all (only the SAF pipeline it terminates into).

### 3.4 What does *not* need to change

- The NSAPI SAF interface and `obj.conf`/`server.xml` configuration surface —
  preserving this is critical for backward compatibility with existing deployments
  and third-party SAF modules.
- `libaccess`/ACL, `ldaputil`, `libadmin` — orthogonal to the transport/protocol layer.
- The build system's per-component `Makefile`/`objs.mk` structure — new protocol
  modules follow the existing convention (see `libproxy/Makefile`,
  `webdav`-equivalent modules for precedent).

## 4. New/changed dependencies

| Dependency | Purpose | Notes |
|---|---|---|
| `OpenSSL 3.x` (if migrating per §2.1) | TLS 1.3, ALPN, QUIC integration surface | Replaces NSS/NSPR SSL usage; NSPR itself (non-SSL parts) can likely remain for portability shims |
| `nghttp2` or a from-scratch HPACK codec | HTTP/2 framing + header compression | `nghttp2` is the pragmatic choice — battle-tested, permissive license (MIT) |
| `ngtcp2` + `nghttp3` (or `quiche`) | QUIC transport + HTTP/3 framing | Only needed if HTTP/3 is in scope; `ngtcp2` pairs naturally with OpenSSL 3.x's QUIC APIs |
| `libbrotli` | Brotli compression | Small, additive dependency alongside existing zlib |
| (optional) `libzstd` | zstd compression | Optional, lower priority than Brotli per gap analysis |
| External ACME client (`acme.sh`, `certbot`, or `lego`) | Automated certificate issuance/renewal | Short-term dependency for the external-script approach in `ACME_CERTIFICATE_MANAGER_PLAN.md`; may be replaced by a built-in client later |

## 5. Risks

- **NSS→OpenSSL migration risk**: `libaccess/lasssl.cpp` and `base/servnss.cpp` are
  both NSS-coupled; a partial migration (e.g. OpenSSL for new listeners, NSS for
  legacy) is possible but adds long-term maintenance surface. Recommend a clean
  full-migration decision rather than dual-stack SSL, if OpenSSL is chosen at all.
- **NSS 3.x runtime quirks that the TLS glue must guard against while on NSS**:
  - *Unverified `ss->sec.peerCert` after failed client-certificate verification*:
    `ssl3_CompleteHandleCertificate()` and `tls13_HandleCertificate()` populate
    `ss->sec.peerCert` before calling `ssl3_AuthCertificate()` and do not clear it
    on verification failure, while `SSL_PeerCertificate()` returns `ss->sec.peerCert`
    without checking verification status. On-demand client-auth (`clauth.cpp`) must
    explicitly poison the session on handshake failure so a subsequent
    `CA_GetClientCert()` call cannot accept an expired or untrusted certificate.
  - *Uninitialized `globalCache` division-by-zero when the SSL session cache is
    disabled*: `ssl_ServerUncacheSessionID()` / `SIDindex()` divides by
    `globalCache.numSIDCacheSets` (`SIGFPE`) without checking whether the server
    session cache was initialized, relying on `SSL_NO_CACHE` (`ss->opt.noCache`) to
    short-circuit `ssl_UncacheSessionID()`. `servssl_init_late()` must set
    `SSL_OptionSetDefault(SSL_NO_CACHE, PR_TRUE)` whenever `Vssl_cache_entries == 0`,
    and `SSL_InvalidateSession()` calls must be skipped when the cache is disabled.
  - *TLS 1.3 post-handshake client authentication (`SSL_SendCertificateRequest`)*:
    `tls13_HandlePostHelloHandshakeMessage()` transitions back to `idle_handshake`
    without invoking `SSL_HandshakeCallback` when `ss->firstHsDone != 0`, so
    `ca_GetClientCert()` must detect completion when
    `SSL_ForceHandshakeWithTimeout()` succeeds and `!SSL_DataPending()`.
  - *Zero-length `SECItem` from `SSL_GetSessionID()`*: when `ssl3_NewSessionID(ss, PR_FALSE)`
    is used, `SSL_GetSessionID()` returns a non-`NULL` `SECItem*` with `len == 0`,
    which causes `BTOA_DataToAscii()` (`NSSBase64_EncodeItem_Util`) to fail and set
    `SEC_ERROR_INVALID_ARGS` in `PR_GetError()`.
  - *Non-thread-safe `ss->fd` mutation in `ssl_FindSocket()` and legacy
    `SSL_ENABLE_SSL3`/`SSL_ENABLE_TLS` range downgrade*: `ssl_FindSocket()` writes
    the matched `PRFileDesc*` layer pointer into `ss->fd` on every lookup (so
    multiplexed HTTP/2 streams must capture TLS metadata once on the connection
    driver thread rather than calling `SSL_*` getters concurrently from stream
    workers), and legacy `SSL_OptionSet(SSL_ENABLE_SSL3/TLS, PR_TRUE)` calls invoke
    NSS's contiguous-range shim and downgrade `SSLVersionRange.min`.
- **Event-loop rewrite risk to existing deployments**: the phased/parallel-path
  strategy in §3.3 is specifically designed to avoid regressing the currently-solid
  HTTP/1.1 behavior. Skipping the "port HTTP/1.1 first, unchanged" step would be the
  single biggest risk-amplifier in this plan.
- **Third-party SAF compatibility**: any change to how `NSAPISession`/`NSAPIRequest`
  are allocated/scoped (now per-stream instead of per-connection) must preserve the
  public NSAPI struct layout and lifetime guarantees third-party SAFs depend on.
  Audit `public/nsapi.h`-equivalent headers for lifetime assumptions before
  implementation.

See `MODERNIZATION_IMPLEMENTATION_PLAN.md` for phased delivery, milestones, and the
corresponding test strategy for each phase.
