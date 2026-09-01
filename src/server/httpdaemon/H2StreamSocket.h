/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright (c) 2026, The Heliod Web Server Project. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _H2_STREAM_SOCKET_H_
#define _H2_STREAM_SOCKET_H_

#include "nspr.h"
#include "base/pool.h"

namespace heliod {

class H2Stream;

/**
 * Lightweight NSPR I/O layer used by a single HTTP/2 stream.
 *
 * The layer is not backed by a real socket.  Instead, reads drain the
 * per-stream request-body buffer and writes append to the per-stream
 * response-body buffer.  This lets the existing HTTP/1.1 request-processing
 * pipeline (HttpRequest / filters / SAFs) run unchanged for an HTTP/2
 * stream, with the HTTP/2 framing layer capturing the response bytes and
 * re-framing them as HEADERS/DATA frames.
 */
class H2StreamSocket {
public:
    /**
     * One-time global initialization of the NSPR identity and method table.
     */
    static PRStatus initialize();

    /**
     * Create a new pseudo-socket for the given stream.  The returned
     * PRFileDesc is owned by the stream and must be destroyed with PR_Close.
     */
    static PRFileDesc* create(H2Stream* stream);

private:
    static PRDescIdentity identity_;
    static PRIOMethods methods_;
    static PRBool initialized_;
};

} // namespace heliod

#endif // _H2_STREAM_SOCKET_H_
