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
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EVENT_DISPATCHER_H_
#define _EVENT_DISPATCHER_H_

#include "httpdaemon/EventLoop.h"
#include "httpdaemon/WorkerPool.h"
#include "httpdaemon/libdaemon.h"

class Connection;

/**
 * EventDispatcher owns the event loop and worker pool for the event-driven
 * I/O model. It is created when at least one listener is configured to use
 * the event-driven model and lives for the lifetime of the server process.
 *
 * The current implementation is a transitional bridge: a worker thread calls
 * the existing classic request-processing path on a Connection that the event
 * loop has identified as readable. Idle keep-alive connections therefore do
 * not consume a worker thread.
 */
class HTTPDAEMON_DLL EventDispatcher : public EventHandler,
                                       public WorkerPoolCallback
{
public:
    EventDispatcher(PRUint32 minWorkers, PRUint32 maxWorkers);
    ~EventDispatcher(void);

    PRStatus start(void);
    void shutdown(void);

    PRBool isStarted(void) const { return started_; }

    /**
     * Register an accepted connection with the event loop. The connection
     * will be dispatched to a worker as soon as data is available.
     */
    PRStatus addConnection(Connection* conn);

    /**
     * Hand a standalone job to the worker pool. An HTTP/2 session uses this to
     * run each of its streams' SAF pipelines on an independent worker instead of
     * executing them one after another on the thread driving the connection.
     *
     * Returns PR_FALSE if the pool cannot take the job (not started, or shutting
     * down), in which case the caller still owns it and must run or discard it
     * itself. Reporting this matters: the caller has usually already counted the
     * job as outstanding, and a job that is counted but never runs would leave
     * whatever is waiting on it waiting forever.
     */
    PRBool enqueueJob(WorkerJob* job);

    /**
     * Put an already-owned connection back on the worker queue for another
     * dispatch, without going through the event loop.
     *
     * An HTTP/2 session uses this to yield: it has work that does not depend on
     * the peer sending anything more, so parking the connection and waiting for
     * readability could strand that work, but continuing to serve one connection
     * indefinitely would starve the others. Requeueing releases the worker and
     * takes a fresh turn.
     */
    PRBool requeueConnection(Connection* conn);

    PRUint32 getNumWorkers(void) const { return workerPool_.getNumThreads(); }

    // Worker-pool / event-loop stats, for the perfdump report.
    PRUint32 getWorkerThreads(void) const { return workerPool_.getNumThreads(); }
    PRUint32 getWorkerMinThreads(void) const { return workerPool_.getMinThreads(); }
    PRUint32 getWorkerMaxThreads(void) const { return workerPool_.getMaxThreads(); }
    PRUint32 getWorkerIdleThreads(void) const { return workerPool_.getIdleThreads(); }
    PRUint32 getWorkerQueueLength(void) const { return workerPool_.getQueueLength(); }
    PRUint32 getEventLoopConnections(void) const { return eventLoop_.getNumConnections(); }

    // EventHandler interface: called by EventLoop thread.
    virtual void onReadable(Connection* conn);
    virtual void onWritable(Connection* conn);
    virtual void onError(Connection* conn);
    virtual void onTimeout(Connection* conn);

    // WorkerPoolCallback interface: called by worker thread.
    virtual void processConnection(Connection* conn);

private:
    EventLoop eventLoop_;
    WorkerPool workerPool_;
    PRBool started_;
};

/**
 * Global event dispatcher instance. NULL until explicitly started by the
 * server bootstrap. Acceptor threads consult this pointer to decide whether
 * to hand accepted connections to the event-driven path.
 */
extern EventDispatcher* globalEventDispatcher;

/**
 * Returns PR_TRUE if the environment requests the event-driven I/O model.
 * Used as a transitional switch until a per-listener configuration option
 * is wired through server.xml.
 */
PRBool httpdaemon_use_event_model(void);

#endif /* _EVENT_DISPATCHER_H_ */
