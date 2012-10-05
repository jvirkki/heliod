/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
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

// Manages access log files

#include <string.h>
#include <nspr.h>
#include <private/pprio.h>
#include <limits.h>

#include "NsprWrap/NsprError.h"
#include "support/stringvalue.h"
#include "base/util.h"
#include "base/file.h"
#include "base/systhr.h"
#include "base/ereport.h"
#include "frame/conf.h"
#include "httpdaemon/WebServer.h"
#include "httpdaemon/logmanager.h"
#include "httpdaemon/dbthttpdaemon.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif // MIN

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif // MAX

#ifndef IOV_MAX
#define IOV_MAX 16
#endif

#define MAX_BUFFERS_PER_WRITEV 16

//-----------------------------------------------------------------------------
// Private data types
//-----------------------------------------------------------------------------

struct LogBuffer;

struct LogFile {
    LogFile* nextHash;
    LogFile* nextSorted;
    LogFile* prevSorted;

    PRUint32 hash;
    char* filename;
    PRFileDesc* fd;
    PRThread* thread;
    PRInt32 refcount;
    char* header;
    PRBool flagLastOpenFailed;
    PRBool flagLastWriteFailed;
    PRBool flagTerminateThread;
    PRBool flagThreadTerminated;
    PRBool flagStdout;
    PRBool flagFlocked;
    PRBool flagReopen;

    PRUint32 countArchive;
    char* archive;

    PRLock* lockAddBuffer;
    PRUint32 offsetBuffer;
    PRUint32 countBuffers;
    LogBuffer* volatile * vectorBuffers;

    PRLock* lockFullBuffers;
    PRCondVar* condFullBuffers;
    LogBuffer* headFullBuffers;
    LogBuffer* tailFullBuffers;
    PRUint32 countFullBuffers;

    PRInt32 countEntriesWritten;
    PRUint32 workload;
};

struct LogBuffer {
    LogBuffer* next;

    PRLock* lock;

    enum {
        IDLE = 0,
        BUSY,
        FULL,
        OLD
    } status;

    LogFile* file;
    PRUint32 index;

    PRUint32 used;
    PRUint32 size;
    PRUint32 sizeRequested;
    char* buffer;
    PRInt32 countEntries;

    PRIntervalTime ticksAdded;
};

struct LogManagerConfig {
    static PRUint32 sizeBuffer;
    static PRUint32 countMaxBuffersPerFile;
    static PRUint32 countMaxBuffers;
    static PRUint32 countMaxBuffersPerWritev;
    static PRUint32 sizeMinAvailable;
    static PRUint32 countMaxThreads;
    static PRUint32 sizeFileHashTable;
    static PRUint32 modeLogFile;
    static PRIntervalTime ticksMaxDirty;
    static PRIntervalTime ticksMaxUnused;
    static PRIntervalTime ticksSortInterval;
    static PRBool flagDirectIo;
    static PRBool flagFlock;
};

//-----------------------------------------------------------------------------
// Static member variable definitions
//-----------------------------------------------------------------------------

const PRUint32 LogManager::sizeMaxLogLine = 4096;

PRUint32 LogManagerConfig::sizeBuffer = 8128;
PRUint32 LogManagerConfig::countMaxBuffersPerFile = 8;
PRUint32 LogManagerConfig::countMaxBuffers = 1000;
PRUint32 LogManagerConfig::countMaxBuffersPerWritev = 16;
PRUint32 LogManagerConfig::sizeMinAvailable = 120;
PRUint32 LogManagerConfig::countMaxThreads = 10;
PRUint32 LogManagerConfig::sizeFileHashTable = 1021;
PRUint32 LogManagerConfig::modeLogFile = 0644;
PRIntervalTime LogManagerConfig::ticksMaxDirty = PR_SecondsToInterval(1);
PRIntervalTime LogManagerConfig::ticksMaxUnused = PR_SecondsToInterval(30);
PRIntervalTime LogManagerConfig::ticksSortInterval = PR_SecondsToInterval(30);
PRBool LogManagerConfig::flagDirectIo = PR_FALSE;
PRBool LogManagerConfig::flagFlock = PR_TRUE;

//-----------------------------------------------------------------------------
// Private global variables
//-----------------------------------------------------------------------------

static PRBool flagInitialized = PR_FALSE;

static PRLock* lockGlobalFlushThread;
static PRCondVar* condGlobalFlushThread;
static PRThread* threadGlobalFlushThread;

static PRLock* lockFiles;
static PRUint32 sizeFiles;
static LogFile** hashFiles;
static LogFile* headSortedFiles;
static LogFile* tailSortedFiles;

static PRLock* lockCleanBuffers;
static LogBuffer* listCleanBuffers;
static PRCondVar* condCleanBuffers;
static PRUint32 countCleanBuffers;
static PRUint32 countCleanWaiters;

static PRInt32 countTotalBuffers;

static PRUint32 countThreads;

static PRLock* lockArchive;
static PRUint32 countArchive;

static PRBool flagShutdown;

static void (*fnRotateCallback)(const char* filenameNew, const char* filenameOld);

//-----------------------------------------------------------------------------
// computeHash
//-----------------------------------------------------------------------------

static PRUint32 computeHash(const char* string)
{
    PRUint32 hash = 0;

    while (*string) {
        hash = hash ^ (hash << 5) ^ ((unsigned)*string);
        string++;
    }

    return hash;
}

//-----------------------------------------------------------------------------
// findFile
//-----------------------------------------------------------------------------

static inline LogFile* findFile(const char* filename, PRUint32 hash, PRUint32 index)
{
    LogFile* file = hashFiles[index];
    while (file) {
        if (file->hash == hash && !strcmp(file->filename, filename)) {
            return file;
        }
        file = file->nextHash;
    }
    return 0;
}

//-----------------------------------------------------------------------------
// allocFile
//-----------------------------------------------------------------------------

static LogFile* allocFile(const char* filename)
{
    PRUint32 countMaxBuffersPerFile = LogManagerConfig::countMaxBuffersPerFile;

    LogFile* file = (LogFile*)PERM_CALLOC(sizeof(*file) + countMaxBuffersPerFile*sizeof(file->vectorBuffers[0]));
    if (!file) return 0;

    file->filename = PERM_STRDUP(filename);
    file->lockAddBuffer = PR_NewLock();
    file->countBuffers = countMaxBuffersPerFile;
    file->vectorBuffers = (LogBuffer**)(file + 1);
    file->lockFullBuffers = PR_NewLock();
    file->condFullBuffers = PR_NewCondVar(file->lockFullBuffers);

    if (!file->filename || !file->lockAddBuffer || !file->lockFullBuffers || !file->condFullBuffers) {
        if (file->filename) PERM_FREE(file->filename);
        if (file->lockAddBuffer) PR_DestroyLock(file->lockAddBuffer);
        if (file->lockFullBuffers) PR_DestroyLock(file->lockFullBuffers);
        if (file->condFullBuffers) PR_DestroyCondVar(file->condFullBuffers);
        PERM_FREE(file);
        return 0;
    }

    return file;
}

//-----------------------------------------------------------------------------
// addSortedFile
//-----------------------------------------------------------------------------

static inline void addSortedFile(LogFile* file, LogFile*& head, LogFile*& tail)
{
    // Maintain list in sorted order with smallest workload at the head
    if (head) {
        if (file->workload < head->workload) {
            // Insert at head (low workload end) of the list
            file->nextSorted = head;
            file->prevSorted = 0;
            head->prevSorted = file;
            head = file;
        } else if (file->workload >= tail->workload) {
            // Insert at tail (high workload end) of the list
            file->nextSorted = 0;
            file->prevSorted = tail;
            tail->nextSorted = file;
            tail = file;
        } else {
            // Insert somewhere inside the list.  Start looking for our spot
            // from the tail to optimize for the case where we're copying an
            // old list from head to tail.
            LogFile* target = tail;
            while (target->workload > file->workload) {
                target = target->prevSorted;
            }

            // Insert ourselves after target (that is, on the high workload
            // side of target)
            file->nextSorted = target->nextSorted;
            file->prevSorted = target;
            file->nextSorted->prevSorted = file;
            file->prevSorted->nextSorted = file;
        }
    } else {
        // First node in the list
        file->nextSorted = 0;
        file->prevSorted = 0;
        tail = file;
        head = file;
    }
}

static inline void addSortedFile(LogFile* file)
{
    // Caller must hold lockFiles
    addSortedFile(file, headSortedFiles, tailSortedFiles);
}

//-----------------------------------------------------------------------------
// addCleanBuffers
//-----------------------------------------------------------------------------

static void addCleanBuffers(LogBuffer* headBuffers)
{
    // Tidy up and count the buffers
    PRUint32 countBuffers = 0;
    LogBuffer* tailBuffers = 0;
    LogBuffer* buffer = headBuffers;
    while (buffer) {
        PR_Lock(buffer->lock);
        buffer->status = LogBuffer::IDLE;
        buffer->file = 0;
        PR_Unlock(buffer->lock);

        countBuffers++;
        tailBuffers = buffer;
        buffer = buffer->next;
    }

    if (countBuffers) {
        PR_Lock(lockCleanBuffers);

        // Add buffers to the list of clean buffers.  The list is in LIFO order
        // so that superfluous buffers can stagnate at the end of the list and
        // get paged out.
        tailBuffers->next = listCleanBuffers;
        listCleanBuffers = headBuffers;
        countCleanBuffers += countBuffers;

        // Wake up anyone waiting for a clean buffer
        if (countBuffers < countCleanWaiters) {
            while (countBuffers--) PR_NotifyCondVar(condCleanBuffers);
        } else {
            PR_NotifyAllCondVar(condCleanBuffers);
        }

        PR_Unlock(lockCleanBuffers);
    }
}

//-----------------------------------------------------------------------------
// addCleanBuffer
//-----------------------------------------------------------------------------

static void addCleanBuffer(LogBuffer* buffer)
{
    buffer->next = 0;
    addCleanBuffers(buffer);
}

//-----------------------------------------------------------------------------
// allocBuffer
//-----------------------------------------------------------------------------

static LogBuffer* allocBuffer()
{
    LogBuffer* buffer = 0;

    if (PR_AtomicIncrement(&countTotalBuffers) <= (PRInt32)LogManagerConfig::countMaxBuffers) {
        // Allocate a new buffer
        PRUint32 sizeBuffer = LogManagerConfig::sizeBuffer;
        buffer = (LogBuffer*)PERM_MALLOC(sizeof(*buffer) + sizeBuffer);
        if (buffer) {
            memset(buffer, 0, sizeof(*buffer));
            buffer->lock = PR_NewLock();
            if (buffer->lock) {
                buffer->size = sizeBuffer;
                buffer->buffer = (char*)(buffer + 1);
                return buffer;
            }
            PERM_FREE(buffer);
            buffer = 0;
        }
    }
    PR_AtomicDecrement(&countTotalBuffers);

    return buffer;
}

//-----------------------------------------------------------------------------
// getCleanBuffer
//-----------------------------------------------------------------------------

static LogBuffer* getCleanBuffer()
{
    LogBuffer* buffer;

    // If there aren't any buffers available and we're below the maximum number
    // of buffers allowed...
    if (countCleanBuffers < 1 && countTotalBuffers < (PRInt32)LogManagerConfig::countMaxBuffers) {
        // Allocate a new buffer
        buffer = allocBuffer();
        if (buffer) return buffer;
    }

    PR_Lock(lockCleanBuffers);
    countCleanWaiters++;

    // If there aren't any buffers, wait for one to be added
    while (countCleanBuffers < 1) PR_WaitCondVar(condCleanBuffers, PR_INTERVAL_NO_TIMEOUT);

    // Get an existing buffer
    buffer = listCleanBuffers;
    listCleanBuffers = buffer->next;
    countCleanBuffers--;

    countCleanWaiters--;
    PR_Unlock(lockCleanBuffers);

    PR_ASSERT(buffer);

    return buffer;
}

//-----------------------------------------------------------------------------
// addFullBuffer
//-----------------------------------------------------------------------------

static void addFullBuffer(LogFile* file, LogBuffer* buffer)
{
    // buffer->lock must be held
    buffer->status = LogBuffer::FULL;
    buffer->file = file;
    buffer->next = 0;

    PR_Lock(file->lockFullBuffers);

    // Add buffer to the tail of the full list
    file->countFullBuffers++;
    if (file->tailFullBuffers) {
        // There are other buffers already in the full list
        file->tailFullBuffers->next = buffer;
        file->tailFullBuffers = buffer;
    } else {
        // We're the first buffer in the full list
        file->headFullBuffers = buffer;
        file->tailFullBuffers = buffer;
        PR_NotifyCondVar(file->condFullBuffers);
    }

    PR_Unlock(file->lockFullBuffers);
}

//-----------------------------------------------------------------------------
// removeFileBuffer
//-----------------------------------------------------------------------------

static inline void removeFileBuffer(LogFile* file, LogBuffer* buffer)
{
    // buffer->lock must be held
    PR_ASSERT(buffer->status == LogBuffer::IDLE);
    PR_ASSERT(buffer->file == file);
    PR_ASSERT(file->vectorBuffers[buffer->index] == buffer);
    file->vectorBuffers[buffer->index] = 0;
    buffer->file = 0;
}

//-----------------------------------------------------------------------------
// lockFileBuffer
//-----------------------------------------------------------------------------

static PRBool lockFileBuffer(LogFile* file, LogBuffer* buffer, PRUint32 length)
{
    PR_Lock(buffer->lock);
    if (buffer->file == file && buffer->status == LogBuffer::IDLE) {
        PR_ASSERT(file->vectorBuffers[buffer->index] == buffer);
        if ((buffer->size - buffer->used) < length) {
            // buffer is too full to be useful to us.  Move buffer from 
            // vectorBuffers to file's full list.
            removeFileBuffer(file, buffer);
            addFullBuffer(file, buffer);
        } else {
            // Return with a lock on buffer
            buffer->status = LogBuffer::BUSY;
            return PR_TRUE;
        }
    }
    PR_Unlock(buffer->lock);
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// unlockFileBuffer
//-----------------------------------------------------------------------------

static void unlockFileBuffer(LogBuffer* buffer, PRUint32 length)
{
    LogFile* file = buffer->file;

    if (length) {
        buffer->countEntries++;
        buffer->used += length;
    }

    PR_ASSERT(buffer->status == LogBuffer::BUSY);
    buffer->status = LogBuffer::IDLE;

    if ((buffer->size - buffer->used) < LogManagerConfig::sizeMinAvailable) {
        // buffer is too full to be useful to us.  Move buffer from
        // vectorBuffers to file's full list.
        removeFileBuffer(file, buffer);
        addFullBuffer(file, buffer);
    }

    PR_Unlock(buffer->lock);
}

//-----------------------------------------------------------------------------
// setFileBuffer
//-----------------------------------------------------------------------------

static void setFileBuffer(LogFile* file, PRUint32 indexBuffer, LogBuffer* buffer, PRIntervalTime ticksNow)
{
    // file->lockAddBuffer and buffer->lock must have been acquired in order
    buffer->file = file;
    buffer->index = indexBuffer;
    buffer->ticksAdded = ticksNow;
    PR_ASSERT(!file->vectorBuffers[indexBuffer]);
    file->vectorBuffers[indexBuffer] = buffer;
}

//-----------------------------------------------------------------------------
// addBusyFileBuffer
//-----------------------------------------------------------------------------

static PRBool addBusyFileBuffer(LogFile* file, PRUint32 offsetBuffer, LogBuffer* buffer)
{
    // buffer->lock must NOT be held
    PRIntervalTime ticksNow = PR_IntervalNow();
    PRUint32 countBuffers = file->countBuffers;
    PRInt32 indexBuffer = offsetBuffer % countBuffers;
    PRUint32 iteration;
    for (iteration = 0; iteration < countBuffers; iteration++, indexBuffer--) {
        if (indexBuffer < 0) indexBuffer += countBuffers;

        // If this index is unused...
        if (!file->vectorBuffers[indexBuffer]) {
            PR_Lock(file->lockAddBuffer);
            if (!file->vectorBuffers[indexBuffer]) {
                // Store buffer at indexBuffer and return with it locked
                PR_Lock(buffer->lock);
                buffer->status = LogBuffer::BUSY;
                setFileBuffer(file, indexBuffer, buffer, ticksNow);
                buffer = 0;
            }
            PR_Unlock(file->lockAddBuffer);

            if (!buffer) return PR_TRUE;
        }
    }
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// addIdleFileBuffers
//-----------------------------------------------------------------------------

static LogBuffer* addIdleFileBuffers(LogFile* file, LogBuffer* listFileBuffers, PRUint32 countFileBuffers)
{
    // listFileBuffers' locks must NOT be held
    PRIntervalTime ticksNow = PR_IntervalNow();
    PRUint32 countBuffers = file->countBuffers;
    PRInt32 indexBuffer = file->offsetBuffer % countBuffers;
    PRUint32 countBuffersAdded = 0;
    PRUint32 iteration;

    PR_Lock(file->lockAddBuffer);

    for (iteration = 0; iteration < countBuffers; iteration++, indexBuffer--) {
        if (indexBuffer < 0) indexBuffer += countBuffers;

        // If this index is unused...
        if (!file->vectorBuffers[indexBuffer]) {
            LogBuffer* next = listFileBuffers->next;

            // Store head of the listFileBuffers list at indexBuffer
            PR_Lock(listFileBuffers->lock);
            listFileBuffers->status = LogBuffer::IDLE;
            setFileBuffer(file, indexBuffer, listFileBuffers, ticksNow);
            PR_Unlock(listFileBuffers->lock);

            listFileBuffers = next;
            countBuffersAdded++;
            if (countBuffersAdded >= countFileBuffers) break;
        }
    }

    PR_Unlock(file->lockAddBuffer);

    return listFileBuffers;
}

//-----------------------------------------------------------------------------
// getFileBuffer
//-----------------------------------------------------------------------------

static inline LogBuffer* getFileBuffer(LogFile* file, PRUint32 length)
{
    PRUint32 countBuffers = file->countBuffers;
    PRUint32 offsetBuffer;
    PRInt32 indexBuffer;
    PRUint32 iteration;
    PRUint32 countVacantBuffers;

    if (length > LogManagerConfig::sizeBuffer) {
        // Should not occur
        ereport(LOG_FAILURE, "Requested %d byte log buffer (maximum is %d bytes)", length, LogManager::sizeMaxLogLine);
        PR_ASSERT(0);
        return 0;
    }

    // Derive offset from our stack address to reduce collisions
    offsetBuffer = ((size_t)&offsetBuffer / 17) % countBuffers;

    // Look for an idle buffer
    countVacantBuffers = 0;
    indexBuffer = offsetBuffer;
    for (iteration = 0; iteration < countBuffers; iteration++, indexBuffer--) {
        if (indexBuffer < 0) indexBuffer += countBuffers;

        // Use this buffer if it's idle and has enough room
        LogBuffer* buffer = file->vectorBuffers[indexBuffer];
        if (buffer && buffer->status == LogBuffer::IDLE &&
            length <= (buffer->size - buffer->used) &&
            lockFileBuffer(file, buffer, length))
        {
            return buffer;
        }

        if (!file->vectorBuffers[indexBuffer]) countVacantBuffers++;
    }

    PRUint32 retries;
    for (retries = 0;; retries++) {
        // We need to reevaluate this approach if we ever sit in here spinning
        PR_ASSERT(retries < 5);

        // Get another LogBuffer for this LogFile if appropriate
        if (countVacantBuffers) {
            // We don't have any buffers available, grab a clean one
            LogBuffer* buffer = getCleanBuffer();
            PR_ASSERT(buffer);
            PR_ASSERT(buffer->status == LogBuffer::IDLE);

            // Try to add buffer to file
            if (addBusyFileBuffer(file, offsetBuffer, buffer)) {
                // Buffer was added to file.  Return locked buffer to caller.
                return buffer;
            }

            // Ack, no place to stick it!  Give it back.
            addCleanBuffer(buffer);
        }

        // Look for a buffer
        countVacantBuffers = 0;
        indexBuffer = offsetBuffer;
        for (iteration = 0; iteration < countBuffers; iteration++, indexBuffer--) {
            if (indexBuffer < 0) indexBuffer += countBuffers;

            // Use this buffer if it exists
            LogBuffer* buffer = file->vectorBuffers[indexBuffer];
            if (buffer && lockFileBuffer(file, buffer, length)) {
                return buffer;
            }

            if (!file->vectorBuffers[indexBuffer]) countVacantBuffers++;
        }
    }
}

//-----------------------------------------------------------------------------
// lockFile
//-----------------------------------------------------------------------------

static void lockFile(LogFile* file)
{
    PR_ASSERT(!file->flagFlocked);
    PR_ASSERT(file->fd);
    if (!file->flagFlocked) {
        if (!file->flagStdout) {
            int retries;
            for (retries = 0; retries < 3000; retries++) {
                // Attempt to acquire lock
                if (system_flock(file->fd) != IO_ERROR) {
                    file->flagFlocked = PR_TRUE;
                    break;
                }
                if (PR_GetError() != PR_DEADLOCK_ERROR) {
                    // Uh, something really odd happened
                    break;
                }

                // We got a deadlock error.  As system_flock() deadlock
                // checking semantics may be per-process instead of
                // per-thread, we will try again.
                systhread_sleep(10);
            }

            // Don't fill the logs with locking error messages
            static PRBool flagLoggedFlockError = PR_FALSE;
            if (!file->flagFlocked && !flagLoggedFlockError) {
                flagLoggedFlockError = PR_TRUE;
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorLocking), file->filename, system_errmsg());
            }
        } else {
            file->flagFlocked = PR_TRUE;        
        }
    }
}

//-----------------------------------------------------------------------------
// unlockFile
//-----------------------------------------------------------------------------

static void unlockFile(LogFile* file)
{
    if (file->flagFlocked) {
        PR_ASSERT(file->fd);
        if (!file->flagStdout && system_ulock(file->fd) == IO_ERROR) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorUnlocking), file->filename, system_errmsg());
        }
        file->flagFlocked = PR_FALSE;
    }
}

//-----------------------------------------------------------------------------
// closeFile
//-----------------------------------------------------------------------------

static void closeFile(LogFile* file)
{
    if (file->flagStdout) return;

    if (file->fd) {
        if (file->flagFlocked) unlockFile(file);
        PR_Close(file->fd);
        file->fd = 0;
    }
}

//-----------------------------------------------------------------------------
// rotate
//-----------------------------------------------------------------------------

static PRBool rotate(LogFile* file, char* archive)
{
    PRBool flagRotated = PR_FALSE;

    PR_ASSERT(!file->flagFlocked);

#ifdef XP_WIN32
    // We can't MoveFileEx a file that's open
    closeFile(file);

    // Perform the rotation
    if (MoveFileEx(file->filename, archive, MOVEFILE_REPLACE_EXISTING)) {
        flagRotated = PR_TRUE;
    } else {
        NsprError::mapWin32Error();
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorRenaming), file->filename, archive, system_errmsg());
    }
#else
    // We need to be concerned about multiple processes trying to rotate the 
    // same log file at roughly the same time

    // Serialize processes
    PRBool flagFlocked = PR_TRUE;
    if (system_flock(file->fd) == IO_ERROR) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorLocking), file->filename, system_errmsg());
        flagFlocked = PR_FALSE;
    }

    // Try to reopen the log file by name
    PRFileDesc* fd;
    fd = PR_Open(file->filename, PR_WRONLY | PR_APPEND, 0);

    // If the new fd and old fd refer to the same file (that is, the file 
    // referenced by the old fd is still named file->filename)...
    if (fd && !file_are_files_distinct(file->fd, fd)) {
        // Perform the rotation
        if (rename(file->filename, archive)) {
            NsprError::mapUnixErrno();
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorRenaming), file->filename, archive, system_errmsg());
        } else {
            flagRotated = PR_TRUE;
        }
    }

    // End cross-process serialization
    if (flagFlocked) {
        if (system_ulock(file->fd) == IO_ERROR) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorUnlocking), file->filename, system_errmsg());
        }
    }

    // Close these fds as they reference the archived file
    if (fd) PR_Close(fd);
    closeFile(file);
#endif

    // Open/create the new log file
    LogManager::openFile(file);

    return flagRotated;
}

static inline void rotate(LogFile* file)
{
    if (file->flagStdout) return;

    if (file->countArchive != countArchive) {
        if (file->fd) {
            // Get the archive file name
            PR_Lock(lockArchive);
            file->countArchive = countArchive;
            char* archive = file->archive;
            file->archive = 0;
            PR_Unlock(lockArchive);

            // Archive the existing log file
            if (archive) {
                if (rotate(file, archive)) {
                    // Post-rotation callback
                    if (fnRotateCallback) {
                        (*fnRotateCallback)(file->filename, archive);
                    }
                }
                PERM_FREE(archive);
            }
        }
    }
}

//-----------------------------------------------------------------------------
// XP_Writev
//-----------------------------------------------------------------------------

static PRInt32 XP_Writev(PRFileDesc* fd, struct iovec* iov, PRUint32 count)
{
    PRInt32 rv = 0;

#if XP_WIN32
    // WriteFileGather() could be used on later Win32 versions
    PRUint32 i;
    for (i = 0; i < count; i++) {
        PRInt32 thisrv = PR_Write(fd, iov[i].iov_base, iov[i].iov_len);
        if (thisrv == PR_FAILURE) return PR_FAILURE;
        rv += thisrv;
    }
#else
    // Call writev directly
    int osfd = (int)PR_FileDesc2NativeHandle(fd);
    do {
        rv = writev(osfd, iov, count);
    } while (rv == -1 && (errno == EAGAIN || errno == EINTR));
    if (rv == -1)
        NsprError::mapUnixErrno();
#endif

    return rv;
}

//-----------------------------------------------------------------------------
// writeBuffers
//-----------------------------------------------------------------------------

static PRStatus writeBuffers(LogFile* file, struct iovec* iov, PRUint32 countWriteBuffers, PRUint32 sizeWriteBuffers)
{
    PRBool flagFlushed = PR_FALSE;

    // Write out the contents of the buffers
    if (file->fd && countWriteBuffers && sizeWriteBuffers) {
        // NSPR doesn't implement PR_Writev for files
        // PRInt32 rv = PR_Writev(file->fd, iov, countWriteBuffers, PR_INTERVAL_NO_TIMEOUT);
        PRInt32 rv = XP_Writev(file->fd, iov, countWriteBuffers);
        if (rv == (PRInt32)sizeWriteBuffers) {
            flagFlushed = PR_TRUE;
            file->flagLastWriteFailed = PR_FALSE;
        } else if (rv == PR_FAILURE) {
            if (!file->flagLastWriteFailed) {
                file->flagLastWriteFailed = PR_TRUE;
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorWriting), file->filename, system_errmsg());
            }
            closeFile(file);
        } else {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_PartialWrite), file->filename, rv, sizeWriteBuffers);
        }
    }

    return flagFlushed ? PR_SUCCESS : PR_FAILURE;
}

//-----------------------------------------------------------------------------
// processFile
//-----------------------------------------------------------------------------

static PRUint32 processFile(LogFile* file)
{
    PRUint32 countBuffers = file->countBuffers;
    PRUint32 indexBuffer;

    // Get list of full buffers.  Caller must hold file->lockFullBuffers.
    LogBuffer* headFullBuffers = file->headFullBuffers;
    LogBuffer* tailFullBuffers = file->tailFullBuffers;
    PRUint32 countFullBuffers = file->countFullBuffers;
    if (countFullBuffers) {
        PR_ASSERT(!file->tailFullBuffers->next);
        file->headFullBuffers = 0;
        file->tailFullBuffers = 0;
        file->countFullBuffers = 0;
    }
    PR_Unlock(file->lockFullBuffers);

    // Get list of stale buffers for this LogFile
    PRIntervalTime ticksNow = PR_IntervalNow();
    LogBuffer* listPendingBuffers = 0;
    PRUint32 countUnusedBuffers = 0;
    PRUint32 countVacantBuffers = 0;
    for (indexBuffer = 0; indexBuffer < countBuffers; indexBuffer++) {
        LogBuffer* buffer = file->vectorBuffers[indexBuffer];
        if (buffer) {
            // If this buffer is old...
            if (flagShutdown ||
                (buffer->used && (PRIntervalTime)(ticksNow - buffer->ticksAdded) >= LogManagerConfig::ticksMaxDirty) ||
                (PRIntervalTime)(ticksNow - buffer->ticksAdded) >= LogManagerConfig::ticksMaxUnused)
            {
                PR_Lock(buffer->lock);
                if (buffer->file == file && buffer->status == LogBuffer::IDLE) {
                    // Move buffer from its LogFile to our flush list
                    removeFileBuffer(file, buffer);
                    buffer->status = LogBuffer::OLD;
                    buffer->next = listPendingBuffers;
                    listPendingBuffers = buffer;

                    // The buffer needs to be detached from its file, or it's
                    // possible a request thread may start using it when we
                    // unlock buffer->lock.
                    PR_ASSERT(!buffer->file);

                    if (!buffer->used) countUnusedBuffers++;
                }
                PR_Unlock(buffer->lock);
            }
        }
        if (!file->vectorBuffers[indexBuffer]) countVacantBuffers++;
    }

    // Place the full buffers at the front of listPendingBuffers
    if (tailFullBuffers) {
        tailFullBuffers->next = listPendingBuffers;
        listPendingBuffers = headFullBuffers;
    }

    // Decide how many buffers we'd like to return to the file.  This value 
    // should increase the number of buffers attached to busy files and
    // decrease the number attached to idle files.  We try to return as many
    // buffers as possible when there are buffers in the clean list.
    PRUint32 countReturnBuffers = countVacantBuffers;
    if (countCleanBuffers < countReturnBuffers) {
        if (countReturnBuffers > countFullBuffers) {
            countReturnBuffers = (countReturnBuffers+countFullBuffers)/2;
        }
        if (countReturnBuffers > (countUnusedBuffers+1)/2) {
            countReturnBuffers -= (countUnusedBuffers+1)/2;
        } else {
            countReturnBuffers = 0;
        }
    }
    PR_ASSERT(countReturnBuffers <= countBuffers);

    // Rotate the log file if necessary
    rotate(file);

    if (file->refcount) {
        // Attempt to open/create the log file
        LogManager::openFile(file);
    }

    // Do we need to lock the log file before writing?
    PRBool flagFlock = LogManagerConfig::flagFlock;
    PR_ASSERT(!file->flagFlocked);

    // For every buffer in listPendingBuffers...
    PRUint32 countEntriesWritten = 0;
    while (listPendingBuffers) {
        // Build iov while moving buffers from listPendingBuffers to 
        // tailFlushBuffers
        struct iovec iov[MAX_BUFFERS_PER_WRITEV];
        LogBuffer* headFlushBuffers = listPendingBuffers;
        LogBuffer* tailFlushBuffers = listPendingBuffers;
        PRUint32 sizeDirtyBuffers = 0;
        PRUint32 countDirtyBuffers = 0;
        PRUint32 countFlushBuffers = 0;
        PRUint32 countDirtyEntries = 0;
        while (listPendingBuffers && countDirtyBuffers < LogManagerConfig::countMaxBuffersPerWritev) {
            if (listPendingBuffers->used) {
                // Add this buffer to iov
                iov[countDirtyBuffers].iov_base = listPendingBuffers->buffer;
                iov[countDirtyBuffers].iov_len = listPendingBuffers->used;
                sizeDirtyBuffers += listPendingBuffers->used;
                countDirtyBuffers++;
                PR_ASSERT(listPendingBuffers->countEntries > 0);
            } else {
                PR_ASSERT(listPendingBuffers->countEntries == 0);
            }

            countFlushBuffers++;
            countDirtyEntries += listPendingBuffers->countEntries;

            // Mark the buffer as empty
            listPendingBuffers->used = 0;
            listPendingBuffers->countEntries = 0;

            tailFlushBuffers = listPendingBuffers;
            listPendingBuffers = listPendingBuffers->next;
        }

        // Break the link between tailFlushBuffers and listPendingBuffers.
        // tailFlushBuffers is the last buffer we will flush this pass and 
        // listPendingBuffers is the head of the list of buffers we still need
        // to attend to.
        PR_ASSERT(tailFlushBuffers->next == listPendingBuffers);
        tailFlushBuffers->next = 0;

        // If there are buffers in iov and the file is open...
        if (countDirtyBuffers && LogManager::openFile(file) == PR_SUCCESS) {
            // Lock the log file if necessary
            if (flagFlock && !file->flagFlocked) lockFile(file);

            // Write out dirty buffers
            if (writeBuffers(file, iov, countDirtyBuffers, sizeDirtyBuffers) == PR_SUCCESS) {
                countEntriesWritten += countDirtyEntries;
            }
        }

        // Try to return some buffers to the file
        if (!flagShutdown && countReturnBuffers) {
            PRUint32 countReturnedBuffers = MIN(countFlushBuffers, countReturnBuffers);
            headFlushBuffers = addIdleFileBuffers(file, headFlushBuffers, countReturnedBuffers);
            countReturnBuffers -= countReturnedBuffers;
        }

        // Add any buffers we didn't return to the file to the clean list
        addCleanBuffers(headFlushBuffers);
    }

    // Unlock the log file
    if (file->flagFlocked) {
        unlockFile(file);
    }

    // Close the file if it's not in use
    if (!file->refcount && file->fd) {
        closeFile(file);
    }

    return countEntriesWritten;
}

//-----------------------------------------------------------------------------
// FileFlushThread
//-----------------------------------------------------------------------------

static void FileFlushThread(void* arg)
{
    LogFile* file = (LogFile*)arg;

    while (!flagShutdown && !file->flagTerminateThread) {
        // Wait for a full buffer or a timeout
        PR_Lock(file->lockFullBuffers);
        if (file->countFullBuffers < 1) PR_WaitCondVar(file->condFullBuffers, LogManagerConfig::ticksMaxDirty);

        // Process file (note that processFile() unlocks file->lockFullBuffers)
        PRUint32 countEntriesWritten;
        countEntriesWritten = processFile(file);

        // Track thread activity
        PR_AtomicAdd(&file->countEntriesWritten, countEntriesWritten);
    }

    file->flagThreadTerminated = PR_TRUE;
}

//-----------------------------------------------------------------------------
// startFileFlushThread
//-----------------------------------------------------------------------------

static void startFileFlushThread(LogFile* file)
{
    if (countThreads < LogManagerConfig::countMaxThreads) {
        file->thread = PR_CreateThread(PR_USER_THREAD,
                                       FileFlushThread,
                                       file,
                                       PR_PRIORITY_NORMAL,
                                       PR_GLOBAL_THREAD,
                                       PR_JOINABLE_THREAD,
                                       0);
        if (file->thread) {
            countThreads++;
        } else {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_FailedToCreateThread), system_errmsg());
        }
    }
}

//-----------------------------------------------------------------------------
// GlobalFlushThread
//-----------------------------------------------------------------------------

static void GlobalFlushThread(void* arg)
{
    PRIntervalTime ticksSorted = PR_IntervalNow();

    while (!flagShutdown) {
        PRBool flagBacklog = PR_FALSE;
        LogFile* file;

        // Flush files and handle teardown of FileFlushThreads
        file = headSortedFiles;
        while (file) {
            if (file->flagThreadTerminated) {
                // Cleanup after a terminated FileFlushThread
                PR_JoinThread(file->thread);
                file->thread = 0;
                file->flagTerminateThread = PR_FALSE;
                file->flagThreadTerminated = PR_FALSE;
                countThreads--;
            }

            // Process log file if it doesn't have a FileFlushThread
            if (!file->thread) {
                // Track thread activity
                if (file->countFullBuffers) flagBacklog = PR_TRUE;

                // Note that processFile() unlocks file->lockFullBuffers
                PR_Lock(file->lockFullBuffers);
                PRUint32 countEntriesWritten = processFile(file);

                // Track file activity
                file->countEntriesWritten += countEntriesWritten;
            }

            file = file->nextSorted;
        }

        // Start FileFlushThreads for busy files
        file = tailSortedFiles;
        while (file && countThreads < LogManagerConfig::countMaxThreads) {
            // Try to give file its own thread
            if ((file->countEntriesWritten || file->workload) && !file->thread) {
                startFileFlushThread(file);
            }
            file = file->prevSorted;
        }

        // If it's been a while since we sorted the file list...
        PRIntervalTime ticksNow = PR_IntervalNow();
        if ((PRIntervalTime)(ticksNow - ticksSorted) >= LogManagerConfig::ticksSortInterval) {
            ticksSorted = ticksNow;

            // Request exclusive access to headSortedFiles/tailSortedFiles
            PR_Lock(lockFiles);

            // Sort the file list based on workload
            LogFile* headNew = 0;
            LogFile* tailNew = 0;
            file = headSortedFiles;
            while (file) {
                LogFile* next = file->nextSorted;

                // Exponential moving average of workloads with a=0.5.  Round
                // down so workload can fall to 0 when the file is idle.
                PRInt32 workload = PR_AtomicSet(&file->countEntriesWritten, 0);
                file->workload = (file->workload + workload) / 2;

                // Add this file to the new sorted list
                addSortedFile(file, headNew, tailNew);

                file = next;
            }
            headSortedFiles = headNew;
            tailSortedFiles = tailNew;

            // If we're starved for threads...
            PRUint32 countMaxThreads = LogManagerConfig::countMaxThreads;
            if (countMaxThreads && countThreads >= countMaxThreads) {
                // Ask one low workload FileFlushThread to die for each high 
                // workload file that lacks its own thread
                LogFile* fileBusy = tailSortedFiles;
                LogFile* fileIdle = headSortedFiles;
                PRUint32 countBusyFiles = countMaxThreads - 1;
                while (countBusyFiles--) {
                    if (!fileBusy->thread) {
                        // This busy file doesn't have a thread.  Find an idle
                        // file with a thread and tell it to die.
                        while (!fileIdle->thread) {
                            fileIdle = fileIdle->nextSorted;
                        }
                        fileIdle->flagTerminateThread = PR_TRUE;
                        fileIdle = fileIdle->nextSorted;
                    }
                    fileBusy = fileBusy->prevSorted;
                }
            }

            // Release exclusive access to headSortedFiles/tailSortedFiles
            PR_Unlock(lockFiles);
        }

        // Sleep if we didn't get any work done this pass
        if (!flagBacklog) {
            PR_Lock(lockGlobalFlushThread);
            PR_WaitCondVar(condGlobalFlushThread, LogManagerConfig::ticksMaxDirty);
            PR_Unlock(lockGlobalFlushThread);
        }
    }
}

//-----------------------------------------------------------------------------
// LogManager::isParam
//-----------------------------------------------------------------------------

PRBool LogManager::isParam(const char* name)
{
    if (!strcmp(name, "buffer-size")) return PR_TRUE;
    if (!strcmp(name, "num-buffers")) return PR_TRUE;
    if (!strcmp(name, "buffers-per-file")) return PR_TRUE;
    if (!strcmp(name, "thread-buffer-size")) return PR_TRUE;
    if (!strcmp(name, "max-threads")) return PR_TRUE;
    if (!strcmp(name, "file-mode")) return PR_TRUE;
    if (!strcmp(name, "milliseconds-dirty")) return PR_TRUE;
    if (!strcmp(name, "flock")) return PR_TRUE;
    return PR_FALSE;
}

//-----------------------------------------------------------------------------
// LogManager::setParams
//-----------------------------------------------------------------------------

void LogManager::setParams(const ServerXMLSchema::AccessLogBuffer& config)
{
    if (config.enabled) {
        LogManagerConfig::sizeBuffer = MAX(LogManager::sizeMaxLogLine + 1, config.bufferSize);
        LogManagerConfig::countMaxBuffers = config.maxBuffers;
        LogManagerConfig::countMaxBuffersPerFile = WebServer::GetConcurrency(config.getMaxBuffersPerFile());
        LogManagerConfig::ticksMaxDirty = config.maxAge.getPRIntervalTimeValue();
        LogManagerConfig::flagDirectIo = config.directIo;
    } else {
        LogManagerConfig::sizeBuffer = LogManager::sizeMaxLogLine + 1;
        LogManagerConfig::sizeMinAvailable = LogManager::sizeMaxLogLine + 1;
    }
}

void LogManager::setParams(pblock* pb)
{
    const char* param;

    if (param = pblock_findval("buffer-size", pb)) {
        LogManagerConfig::sizeBuffer = MAX(LogManager::sizeMaxLogLine + 1, strtoul(param, 0, 0));
    }

    if (param = pblock_findval("num-buffers", pb)) {
        LogManagerConfig::countMaxBuffers = MAX(2, strtoul(param, 0, 0));
    }

    if (param = pblock_findval("buffers-per-file", pb)) {
        LogManagerConfig::countMaxBuffersPerFile = MAX(1, strtoul(param, 0, 0));
    }

    if (param = pblock_findval("max-threads", pb)) {
        LogManagerConfig::countMaxThreads = strtoul(param, 0, 0);
    }

    if (param = pblock_findval("file-mode", pb)) {
        LogManagerConfig::modeLogFile = strtoul(param, 0, 8);
    }
    
    if (param = pblock_findval("milliseconds-dirty", pb)) {
        LogManagerConfig::ticksMaxDirty = MAX(1, PR_MillisecondsToInterval(strtoul(param, 0, 0)));
    }

    if (param = pblock_findval("flock", pb)) {
        LogManagerConfig::flagFlock = StringValue::getBoolean(param);
    }
}

//-----------------------------------------------------------------------------
// LogManager::initEarly
//-----------------------------------------------------------------------------

PRStatus LogManager::initEarly()
{
#ifndef XP_WIN32
    PR_ASSERT(IOV_MAX >= MAX_BUFFERS_PER_WRITEV);
#endif

    lockGlobalFlushThread = PR_NewLock();
    condGlobalFlushThread = PR_NewCondVar(lockGlobalFlushThread);

    lockFiles = PR_NewLock();
    sizeFiles = LogManagerConfig::sizeFileHashTable;

    hashFiles = (LogFile**)PERM_CALLOC(sizeFiles * sizeof(LogFile*));

    lockCleanBuffers = PR_NewLock();
    condCleanBuffers = PR_NewCondVar(lockCleanBuffers);

    lockArchive = PR_NewLock();

    return (lockGlobalFlushThread && condGlobalFlushThread && lockFiles && hashFiles && lockCleanBuffers && condCleanBuffers && lockArchive) ? PR_SUCCESS : PR_FAILURE;
}

//-----------------------------------------------------------------------------
// LogManager::initLate
//-----------------------------------------------------------------------------

static PRStatus _initLate(void)
{
    PR_ASSERT(MAX_BUFFERS_PER_WRITEV >= LogManagerConfig::countMaxBuffersPerWritev);
    PR_ASSERT(LogManagerConfig::sizeBuffer >= LogManager::sizeMaxLogLine + 1);

    // Create some buffers
    LogBuffer* listInitialBuffers = 0;
    PRUint32 count = MIN(2 * LogManagerConfig::countMaxBuffersPerFile + 1, LogManagerConfig::countMaxBuffers);
    while (count--) {
        LogBuffer* buffer = getCleanBuffer();
        if (buffer) {
            buffer->next = listInitialBuffers;
            listInitialBuffers = buffer;
        }
    }

    // Put the buffers up for grabs
    addCleanBuffers(listInitialBuffers);

    threadGlobalFlushThread = PR_CreateThread(PR_USER_THREAD,
                                              GlobalFlushThread,
                                              0,
                                              PR_PRIORITY_NORMAL,
                                              PR_GLOBAL_THREAD,
                                              PR_JOINABLE_THREAD,
                                              0);

    flagInitialized = PR_TRUE;

    return threadGlobalFlushThread ? PR_SUCCESS : PR_FAILURE;
}

PRStatus LogManager::initLate()
{
    static PRCallOnceType once;
    return PR_CallOnce(&once, _initLate);
}

//-----------------------------------------------------------------------------
// LogManager::getFile
//-----------------------------------------------------------------------------

static LogFile* _getFile(const char* filename)
{
    PRUint32 hash = computeHash(filename);
    PRUint32 index = hash % sizeFiles;
    LogFile* file;

    // Check for existing LogFile.  In the steady state, we should find a match
    // here and not need to acquire lockFiles.
    file = findFile(filename, hash, index);
    if (file) return file;

    // No matching LogFile found.  Serialize creation of a new LogFile.
    PR_Lock(lockFiles);
    file = findFile(filename, hash, index);
    if (!file) {
        // Create a new LogFile
        file = allocFile(filename);
        if (file) {
            // Add file to hashFiles
            file->hash = hash;
            file->nextHash = hashFiles[index];
            hashFiles[index] = file;

            // Add file to the sorted files list.  It will end up at the tail.
            addSortedFile(file);
        } else {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_LogManager_ErrorOpening), file->filename, system_errmsg());
        }
    }
    PR_Unlock(lockFiles);

    return file;
}

LogFile* LogManager::getFile(const char* filename)
{
    char *path = ereport_abs_filename(filename);
    if (!path) return 0;

    LogFile* file = _getFile(path);
    if (file) ref(file);

    FREE(path);

    return file;
}

//-----------------------------------------------------------------------------
// LogManager::ref
//-----------------------------------------------------------------------------

LogFile* LogManager::ref(LogFile* file)
{
    if (file) PR_AtomicIncrement(&file->refcount);
    return file;
}

//-----------------------------------------------------------------------------
// LogManager::unref
//-----------------------------------------------------------------------------

LogFile* LogManager::unref(LogFile* file)
{
    if (file) PR_AtomicDecrement(&file->refcount);
    return 0;
}

//-----------------------------------------------------------------------------
// LogManager::setHeader
//-----------------------------------------------------------------------------

void LogManager::setHeader(LogFile* file, const char* header)
{
    if (file && !file->header) file->header = PERM_STRDUP(header);
}

//-----------------------------------------------------------------------------
// LogManager::openFile
//-----------------------------------------------------------------------------

PRStatus LogManager::openFile(LogFile* file)
{
    if (!file) return PR_FAILURE;

    // A filename of "-" means use stdout
    if (!strcmp(file->filename, "-")) {
        file->flagStdout = PR_TRUE;
        file->fd = PR_STDOUT;
        return PR_SUCCESS;
    }

    // Close the log file, if requested
    if (file->flagReopen) {
        file->flagReopen = PR_FALSE;
        if (file->fd) {
            PR_Close(file->fd);
            file->fd = 0;
        }
        ereport(LOG_VERBOSE, "Reopening log file %s", file->filename);

        // Reset any error conditions
        file->flagLastOpenFailed = PR_FALSE;
        file->flagLastWriteFailed = PR_FALSE;
    }

    // Try to open the log file if it isn't open already
    if (!file->fd) {

        //Opening the file in RDWR mode. In case the file is a named
        //pipe, we won't sleep for lack of a reader process. 
        //Note: Currently NSPR does not support Windows named pipes.
        //So, we still cannot use named pipes for log files on Windows.

        file->fd = PR_Open(file->filename, PR_RDWR | PR_CREATE_FILE | PR_APPEND, LogManagerConfig::modeLogFile);
        if (file->fd) {
            file->flagLastOpenFailed = PR_FALSE;

            PR_SetFDInheritable(file->fd, PR_FALSE);

            file_setdirectio(file->fd, LogManagerConfig::flagDirectIo);

            // If there's a header specified for this log file...
            if (file->header) {
                // Serialize processes
                PRBool flagFlocked = PR_TRUE;
                if (system_flock(file->fd) == IO_ERROR) {
                    ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorLocking), file->filename, system_errmsg());
                    flagFlocked = PR_FALSE;
                }

                // If the file is empty...
                PRFileInfo64 finfo;
                if (PR_GetOpenFileInfo64(file->fd, &finfo) == PR_SUCCESS) {
                    if (finfo.size == 0) {
                        // Write out the header
                        PR_Write(file->fd, file->header, strlen(file->header));
                    }
                }

                // End cross-process serialization
                if (flagFlocked) {
                    if (system_ulock(file->fd) == IO_ERROR) {
                        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorUnlocking), file->filename, system_errmsg());
                    }
                }
            }

        } else if (!file->flagLastOpenFailed) {
            file->flagLastOpenFailed = PR_TRUE;
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LogManager_ErrorOpening), file->filename, system_errmsg());
        }
    }

    return file->fd ? PR_SUCCESS : PR_FAILURE;
}

//-----------------------------------------------------------------------------
// LogManager::lockBuffer
//-----------------------------------------------------------------------------

char* LogManager::lockBuffer(LogFile* file, LogBuffer*& handle, PRUint32 size)
{
    if (!file) return 0;

    LogBuffer* buffer = getFileBuffer(file, size + 1);
    if (!buffer) {
        handle = 0;
        return 0;
    }
    PR_ASSERT(buffer->size - buffer->used >= size + 1);

    // Store a sentinel at the end of the buffer
    buffer->sizeRequested = size;
    buffer->buffer[buffer->used + buffer->sizeRequested] = (char)0xCE;

    // Caller passes this back as a handle to unlockBuffer()
    handle = buffer;

    return &buffer->buffer[buffer->used];
}

//-----------------------------------------------------------------------------
// LogManager::unlockBuffer
//-----------------------------------------------------------------------------

void LogManager::unlockBuffer(LogBuffer* handle, PRUint32 size)
{
    LogBuffer* buffer = handle;
    if (buffer) {
        if (size > buffer->sizeRequested ||
            buffer->buffer[buffer->used + buffer->sizeRequested] != (char)0xCE)
        {
            // Should not occur
            ereport(LOG_CATASTROPHE, "Logging buffer overflow, system integrity compromised");
            PR_ASSERT(0);
        }

        unlockFileBuffer(buffer, size);
    }
}

//-----------------------------------------------------------------------------
// LogManager::logf
//-----------------------------------------------------------------------------

PRInt32 LogManager::logf(LogFile* file, PRUint32 size, const char* fmt, ...)
{
    if (!file) return PR_FAILURE;

    va_list args;

    if (!flagInitialized) {
        // Special case for prefork logging
        PRInt32 rv = PR_FAILURE;
        if (file && file->fd) {
            va_start(args, fmt);
            rv = PR_vfprintf(file->fd, fmt, args);
            va_end(args);
        }
        return rv;
    }

    PR_ASSERT(flagInitialized);

    LogBuffer* buffer = getFileBuffer(file, size);
    if (!buffer) return 0;
    PR_ASSERT(buffer->size - buffer->used >= size);

    va_start(args, fmt);

    size = util_vsnprintf(buffer->buffer + buffer->used, size, fmt, args);

    va_end(args);

    unlockFileBuffer(buffer, size);

    return size;
}

//-----------------------------------------------------------------------------
// LogManager::log
//-----------------------------------------------------------------------------

PRInt32 LogManager::log(LogFile* file, const char* string)
{
    return log(file, string, strlen(string));
}

PRInt32 LogManager::log(LogFile* file, const char* string, PRUint32 size)
{
    if (!file) return PR_FAILURE;

    if (!flagInitialized) {
        // Special case for prefork logging
        if (file && file->fd) {
            return PR_Write(file->fd, string, size);
        } else {
            return PR_FAILURE;
        }
    }

    PR_ASSERT(flagInitialized);

    LogBuffer* buffer = getFileBuffer(file, size);
    if (!buffer) return 0;
    PR_ASSERT(buffer->size - buffer->used >= size);

    memcpy(buffer->buffer + buffer->used, string, size);

    unlockFileBuffer(buffer, size);

    return size;
}

//-----------------------------------------------------------------------------
// LogManager::reopen
//-----------------------------------------------------------------------------

void LogManager::reopen()
{
    PR_Lock(lockFiles);

    // Set flagReopen for every file
    LogFile* file = headSortedFiles;
    while (file) {
        file->flagReopen = PR_TRUE;
        file = file->nextSorted;
    }

    PR_Unlock(lockFiles);
}

//-----------------------------------------------------------------------------
// LogManager::rotate
//-----------------------------------------------------------------------------

void LogManager::rotate(const char* ext)
{
    int lenExt = strlen(ext);

    PR_Lock(lockFiles);

    PR_Lock(lockArchive);

    // Set file->archive, the archive file name, for every file
    LogFile* file = headSortedFiles;
    while (file) {
        if (file->archive) PERM_FREE(file->archive);

        int lenFilename = strlen(file->filename);
        file->archive = (char*)PERM_MALLOC(lenFilename + lenExt + 1);
        if (file->archive) {
            memcpy(file->archive, file->filename, lenFilename);
            memcpy(file->archive + lenFilename, ext, lenExt + 1);
        }

        file = file->nextSorted;
    }

    countArchive++;

    PR_Unlock(lockArchive);

    PR_Unlock(lockFiles);
}

//-----------------------------------------------------------------------------
// LogManager::setRotateCallback
//-----------------------------------------------------------------------------

void LogManager::setRotateCallback(void (*fn)(const char* filenameNew, const char* filenameOld))
{
    fnRotateCallback = fn;
}

//-----------------------------------------------------------------------------
// LogManager::terminate
//-----------------------------------------------------------------------------

void LogManager::terminate()
{
    LogFile* file;

    if (!threadGlobalFlushThread) return;

    // To ensure all files are shutdown cleanly, we should be called after all
    // DaemonSession threads have terminated

    // Setting flagShutdown causes *FlushThreads to terminate and alters 
    // processFile() behavior
    flagShutdown = PR_TRUE;

    // Wake up GlobalFlushThread
    PR_Lock(lockGlobalFlushThread);
    PR_NotifyCondVar(condGlobalFlushThread);
    PR_Unlock(lockGlobalFlushThread);

    // Wake up each FileFlushThread
    PR_Lock(lockFiles);
    file = headSortedFiles;
    while (file) {
        PR_Lock(file->lockFullBuffers);
        PR_NotifyCondVar(file->condFullBuffers);
        PR_Unlock(file->lockFullBuffers);
        file = file->nextSorted;
    }
    PR_Unlock(lockFiles);

    // Wait for GlobalFlushThread to terminate
    PR_JoinThread(threadGlobalFlushThread);
    threadGlobalFlushThread = 0;

    // Shutdown each LogFile in turn
    PR_Lock(lockFiles);
    file = headSortedFiles;
    while (file) {
        // Terminate the file's FileFlushThread
        if (file->thread) {
            PR_JoinThread(file->thread);
            file->thread = 0;
        }

        // Flush the file
        do {
            PR_Lock(file->lockFullBuffers);
        } while(processFile(file));

        // Close the file
        closeFile(file);

        file = file->nextSorted;
    }
    PR_Unlock(lockFiles);

    // All buffers should have been returned to the clean list
    if (countCleanBuffers != (PRUint32)countTotalBuffers) {
        // Should not occur
        ereport(LOG_FAILURE, "Failed to flush %d log buffers", countTotalBuffers - countCleanBuffers);
        PR_ASSERT(0);
    }
    PR_ASSERT(countCleanBuffers == countTotalBuffers);
}

//-----------------------------------------------------------------------------
// LogManager::getLogCount
//-----------------------------------------------------------------------------

int LogManager::getLogCount()
{
    LogFile* file;
    int log_count = 0; 

    PR_Lock(lockFiles);

    file = headSortedFiles;
    while (file) {
       log_count++;
       file = file->nextSorted;
    }

    PR_Unlock(lockFiles);

    return log_count;
}

void
LogManager::updatePosition(LogBuffer *buffer, PRUint32 size)
{
    if (buffer) {
        PR_ASSERT(buffer->status == LogBuffer::BUSY);
        if (size > buffer->sizeRequested ||
            buffer->buffer[buffer->used + buffer->sizeRequested] != (char)0xCE)
        {
            // Should not occur
            ereport(LOG_CATASTROPHE, "Logging buffer overflow, system integrity compromised");
            PR_ASSERT(0);
        }

        if (size) {
            buffer->countEntries++;
            buffer->used += size;
            buffer->sizeRequested = 0;
        }
    }
}

PRBool
LogManager::isSpaceAvailable(LogBuffer *buffer, PRUint32 size)
{
    PR_ASSERT(buffer->status == LogBuffer::BUSY);

    if ((buffer->size - buffer->used) > (size +1)) {
        // Store a sentinel at the end of the buffer
        buffer->sizeRequested = size;
        buffer->buffer[buffer->used + buffer->sizeRequested] = (char)0xCE;
        return PR_TRUE;
    }

    return PR_FALSE;
}

char*
LogManager::getBuffer(LogBuffer *buffer)
{
    PR_ASSERT(buffer->status == LogBuffer::BUSY);
    return &buffer->buffer[buffer->used];
}
