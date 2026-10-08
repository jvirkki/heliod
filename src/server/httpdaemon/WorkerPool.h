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

#ifndef _WORKER_POOL_H_
#define _WORKER_POOL_H_

#include "nspr.h"
#include "xp/xpsynch.h"

class Connection;

/**
 * Callback interface for work dispatched by WorkerPool.
 */
class WorkerPoolCallback
{
public:
    virtual ~WorkerPoolCallback() {}

    /**
     * Process a connection that has been marked ready by the event loop.
     * This runs on a worker thread. The callback should consume input,
     * run the request/response cycle, and return the connection to the
     * event loop if it should remain open (keep-alive).
     */
    virtual void processConnection(Connection* conn) = 0;
};

/**
 * A unit of work that is not a connection dispatch.
 *
 * HTTP/2 needs this. One stream's SAF pipeline is an independent piece of work
 * that has to run on its own thread for concurrent streams on a single
 * connection to actually run concurrently, but a stream is not a Connection and
 * cannot be handed to processConnection().
 *
 * runJob() is called exactly once, on a worker thread. The job owns its own
 * lifetime: the pool never deletes it, and the job may free itself inside
 * runJob() provided it touches nothing afterwards.
 */
class WorkerJob
{
public:
    virtual ~WorkerJob() {}

    virtual void runJob(void) = 0;
};

/**
 * A pool of worker threads that execute connection-processing callbacks
 * asynchronously. The pool is intentionally separate from the event loop so
 * that idle connections do not hold threads.
 *
 * Sizing mirrors the classic DaemonSession thread pool: the pool starts with
 * `min` threads and grows one at a time toward `max` whenever work arrives and
 * every existing worker is already busy. Like the classic model, it does not
 * shrink -- once grown, threads persist for the process lifetime.
 */
class WorkerPool
{
public:
    WorkerPool(PRUint32 minThreads, PRUint32 maxThreads,
               WorkerPoolCallback* callback);
    ~WorkerPool(void);

    PRStatus start(void);
    void shutdown(void);

    void enqueue(Connection* conn);

    /**
     * Queue a standalone job. Ordered against enqueue(Connection*) in the same
     * FIFO, so a job cannot be starved by a backlog of connection dispatches or
     * vice versa.
     */
    void enqueue(WorkerJob* job);

    /** Live worker-thread count (grows on demand, never shrinks). */
    PRUint32 getNumThreads(void) const { return curThreads_; }
    PRUint32 getMinThreads(void) const { return minThreads_; }
    PRUint32 getMaxThreads(void) const { return maxThreads_; }
    /** Workers currently blocked waiting for work (racy read; for stats only). */
    PRUint32 getIdleThreads(void) const { return idleThreads_; }
    PRUint32 getQueueLength(void) const;

private:
    static void PR_CALLBACK workerMain(void* arg);

    void runWorker(void);

    /** Create one more worker thread. Caller must hold lock_. Returns whether a
     *  thread was created (false if already at max or the OS refused). */
    PRBool spawnWorkerLocked(void);

    PRUint32 minThreads_;   // threads spawned at start()
    PRUint32 maxThreads_;   // hard ceiling on worker threads
    PRUint32 curThreads_;   // live worker threads right now
    PRUint32 idleThreads_;  // workers currently blocked waiting for work
    WorkerPoolCallback* callback_;
    PRThread** threads_;
    PRBool shutdown_;

    // Simple linked queue of work items. Exactly one of conn/job is set; the
    // other is NULL.
    struct QueueNode {
        Connection* conn;
        WorkerJob* job;
        QueueNode* next;
    };

    void enqueueNode(QueueNode* node);

    QueueNode* queueHead_;
    QueueNode* queueTail_;
    PRUint32 queueLen_;
    XPLock lock_;
    XPCondition available_;
};

#endif /* _WORKER_POOL_H_ */
