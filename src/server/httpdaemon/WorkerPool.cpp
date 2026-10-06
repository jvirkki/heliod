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

#include "httpdaemon/WorkerPool.h"

#include "base/ereport.h"
#include "httpdaemon/connqueue.h"
#include "NsprWrap/Thread.h"

WorkerPool::WorkerPool(PRUint32 minThreads, PRUint32 maxThreads,
                       WorkerPoolCallback* callback)
    : minThreads_(minThreads),
      maxThreads_(maxThreads),
      curThreads_(0),
      idleThreads_(0),
      callback_(callback),
      threads_(NULL),
      shutdown_(PR_FALSE),
      queueHead_(NULL),
      queueTail_(NULL),
      queueLen_(0)
{
    if (minThreads_ > maxThreads_)
        minThreads_ = maxThreads_;
    XP_InitLock(&lock_);
    XP_InitCondition(&available_, &lock_);
}

WorkerPool::~WorkerPool(void)
{
    shutdown();
    XP_DestroyCondition(&available_);
    XP_DestroyLock(&lock_);
}

// Caller must hold lock_.  Creates one worker thread and records it.  The new
// thread's runWorker() blocks on lock_ until the caller releases it, then picks
// up whatever is queued.
PRBool WorkerPool::spawnWorkerLocked(void)
{
    if (curThreads_ >= maxThreads_)
        return PR_FALSE;

    PRThread* t = PR_CreateThread(
        PR_USER_THREAD,
        workerMain,
        this,
        PR_PRIORITY_NORMAL,
        PR_GLOBAL_THREAD,
        PR_UNJOINABLE_THREAD,
        Thread::getDefaultStackSize());
    if (!t) {
        ereport(LOG_FAILURE,
                "WorkerPool: failed to create worker thread (%u live)",
                curThreads_);
        return PR_FALSE;
    }

    threads_[curThreads_] = t;
    curThreads_++;
    ereport(LOG_VERBOSE,
            "WorkerPool: grew to %u of %u worker threads (stack %u bytes)",
            curThreads_, maxThreads_, Thread::getDefaultStackSize());
    return PR_TRUE;
}

PRStatus WorkerPool::start(void)
{
    if (!callback_ || maxThreads_ == 0)
        return PR_FAILURE;

    // Room for the ceiling; only `min` are spawned now, the rest on demand.
    threads_ = new PRThread*[maxThreads_];
    if (!threads_)
        return PR_FAILURE;

    XP_Lock(&lock_);
    for (PRUint32 i = 0; i < minThreads_; i++) {
        if (!spawnWorkerLocked()) {
            XP_Unlock(&lock_);
            shutdown();
            return PR_FAILURE;
        }
    }
    XP_Unlock(&lock_);
    return PR_SUCCESS;
}

void WorkerPool::shutdown(void)
{
    // XP_Broadcast (like XP_Signal) requires the associated lock to be held
    // by the caller; see the comment in enqueue().
    XP_Lock(&lock_);
    shutdown_ = PR_TRUE;
    XP_Broadcast(&available_);
    XP_Unlock(&lock_);

    // We created unjoinable threads; rely on callback shutdown to stop them.
    if (threads_) {
        PR_Sleep(PR_MillisecondsToInterval(100));
        delete[] threads_;
        threads_ = NULL;
    }
}

void WorkerPool::enqueue(Connection* conn)
{
    if (!conn)
        return;

    QueueNode* node = new QueueNode;
    node->conn = conn;
    node->job = NULL;
    node->next = NULL;
    enqueueNode(node);
}

void WorkerPool::enqueue(WorkerJob* job)
{
    if (!job)
        return;

    QueueNode* node = new QueueNode;
    node->conn = NULL;
    node->job = job;
    node->next = NULL;
    enqueueNode(node);
}

void WorkerPool::enqueueNode(QueueNode* node)
{
    XP_Lock(&lock_);
    if (queueTail_) {
        queueTail_->next = node;
    } else {
        queueHead_ = node;
    }
    queueTail_ = node;
    queueLen_++;

    if (idleThreads_ > 0) {
        // A worker is waiting; hand the item to it.
        XP_Signal(&available_);
    } else if (curThreads_ < maxThreads_) {
        // Every worker is busy and we are below the ceiling: grow to meet
        // demand.  The new worker will pick this item up.  If the spawn fails,
        // a busy worker drains the queue when it loops back.
        spawnWorkerLocked();
    }
    // else: all workers busy at max -- the item waits, and whichever worker
    // finishes next takes it on its next loop (bounded concurrency = max).

    XP_Unlock(&lock_);
}

PRUint32 WorkerPool::getQueueLength(void) const
{
    XP_Lock((XPLock*)&lock_);
    PRUint32 len = queueLen_;
    XP_Unlock((XPLock*)&lock_);
    return len;
}

void PR_CALLBACK WorkerPool::workerMain(void* arg)
{
    WorkerPool* pool = (WorkerPool*)arg;
    pool->runWorker();
}

void WorkerPool::runWorker(void)
{
    while (!shutdown_) {
        Connection* conn = NULL;
        WorkerJob* job = NULL;

        XP_Lock(&lock_);
        while (!queueHead_ && !shutdown_) {
            // Counted as idle only while actually blocked here, so enqueueNode's
            // "is any worker free?" test (idleThreads_ > 0) is exact and it grows
            // the pool precisely when no waiting worker can take the new item.
            idleThreads_++;
            XP_Wait(&available_, XP_INTERVAL_INFINITE, NULL);
            idleThreads_--;
        }
        if (shutdown_) {
            XP_Unlock(&lock_);
            break;
        }

        QueueNode* node = queueHead_;
        queueHead_ = node->next;
        if (!queueHead_)
            queueTail_ = NULL;
        queueLen_--;
        conn = node->conn;
        job = node->job;
        delete node;
        ereport(LOG_VERBOSE,
                "WorkerPool::runWorker dequeued conn=%p job=%p queueLen=%u",
                conn, job, queueLen_);
        XP_Unlock(&lock_);

        if (job) {
            // The job may free itself inside runJob(), so it must not be
            // touched again here.
            job->runJob();
        } else if (conn && callback_) {
            callback_->processConnection(conn);
        }
    }
}
