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

/**
 * sem.c: Attempt to provide multi-process semaphores across platforms
 * 
 * Rob McCool
 */


#include "systems.h"
#include "util.h"
#include "NsprWrap/NsprError.h"
#include "sem.h"

#if defined(SEM_FLOCK)

#include "file.h"
#include "systhr.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if !defined (MAXPATHLEN)
#define	MAXPATHLEN	1024
#endif

/**
 * SEM_FLOCK
 *
 * ruslan:
 * this is being reimplemented as we cannot use PR_LockFile because it serialises
 * operation across threads; we we need to use lockf
 *
 */

/**
 * Sem_init - initialize cross-process semaphore which uses file locks
 *
 * @param	name	name of the lock
 * @param	ignored
 *
 * @return	if (which is PRFileDesc in this case
 */
SEMAPHORE
Sem_init (char *name, int number)
{
    char tn[MAXPATHLEN];
    int fd;

    if (strlen (name) + strlen (system_get_temp_dir()) > MAXPATHLEN - 10)
        return SEM_ERROR;

    util_snprintf (tn, sizeof(tn), "%s/%s.%d", system_get_temp_dir(), name, 
                   number);
    unlink (tn);

    if( (fd = open (tn, O_RDWR|O_CREAT, 0644)) == -1 ) {
        NsprError::mapUnixErrno();
        return SEM_ERROR;
    }

    unlink(tn);
    return fd;
}

/**
 * Sem_terminate - close the semaphore
 *
 */
void
Sem_terminate (SEMAPHORE id)
{
	if (id != SEM_ERROR)
		close (id);
}


int
Sem_grab (SEMAPHORE id)
{
    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

#ifdef THREAD_NSPR_USER
    /* Poll to avoid blocking. XXXrobm If errno is wrong this may go awry. */
    while (lockf (id, F_TLOCK) < 0)
        systhread_sleep (1000);
#else
    // ruslan: actually check if the call was interrupted
    int rv;
    do	{
        rv = lockf (id, F_LOCK, 0);
    }
    while (rv == -1 && errno == EINTR);

    if (rv == -1)
        NsprError::mapUnixErrno();

    return rv;
#endif
}

int
Sem_tgrab(SEMAPHORE id)
{
    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    int rv = (id, F_TLOCK, 0);
    if (rv == -1)
        NsprError::mapUnixErrno();

    return rv;
}

int
Sem_release(SEMAPHORE id)
{
    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    int rv = lockf(id, F_ULOCK, 0);
    if (rv == -1)
        NsprError::mapUnixErrno();

    return rv;
}


#elif defined(SEM_WIN32)


/* ------------------------------- sem_init ------------------------------- */


NSAPI_PUBLIC SEMAPHORE Sem_init(char *name, int number)
{
    char tn[MAX_PATH];
    util_snprintf(tn, sizeof(tn), "%s.%d", name, number);
    
    SEMAPHORE sem =  CreateSemaphore(NULL, 1, SEM_MAXVALUE, tn);
    if (sem == SEM_ERROR)
        NsprError::mapWin32Error();
    return sem;
}


/* ---------------------------- sem_terminate ----------------------------- */


NSAPI_PUBLIC void Sem_terminate(SEMAPHORE id)
{
	CloseHandle(id);
}


/* ------------------------ sem_grab, sem_release ------------------------- */


NSAPI_PUBLIC int Sem_grab(SEMAPHORE id)
{
    if (WaitForSingleObject(id, INFINITE) == WAIT_FAILED) {
        NsprError::mapWin32Error();
        return -1;
    }

    return 0;
}

/* sem_tgrab is supposed to prevent a block on the acquisition of
 * the semaphore. NT uses kernel threads and so the block doesn't
 * matter */

NSAPI_PUBLIC int Sem_tgrab(SEMAPHORE id)
{
    if (WaitForSingleObject(id, 0) == WAIT_FAILED) {
        NsprError::mapWin32Error();
        return -1;
    }

    return 0;
}

NSAPI_PUBLIC int Sem_release(SEMAPHORE id)
{
    if (!ReleaseSemaphore(id, 1, NULL)) {
        NsprError::mapWin32Error();
        return -1;
    }

    return 0;
}

#elif defined (SEM_POSIX)

#include <semaphore.h>

NSAPI_PUBLIC SEMAPHORE Sem_init (char *name, int number)
{
    char tn[256];
    int value = number;

#ifdef HPUX
    util_sprintf(tn, "/tmp/%s.%d", name, number);
    value = 1;
#else
    util_sprintf(tn, "/%s.%d", name, number);
#endif


    sem_t *sp = sem_open (tn, O_CREAT, 0755, value); /* Number of simultaneous workers=1 */
    if (sp == (sem_t*)(-1)) {
        NsprError::mapUnixErrno();
    } else {
        sem_unlink (tn);
    }

    return (void *)sp;
}

NSAPI_PUBLIC int Sem_grab (SEMAPHORE id)
{
    sem_t *sp = (sem_t *) id;

    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    return sem_wait (sp);
}

NSAPI_PUBLIC int Sem_tgrab (SEMAPHORE id)
{
    sem_t *sp = (sem_t *) id;

    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    return sem_trywait (sp);
}

NSAPI_PUBLIC int Sem_release (SEMAPHORE id)
{
    sem_t *sp = (sem_t *) id;

    if (id == SEM_ERROR) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    return sem_post (sp);
}

NSAPI_PUBLIC void Sem_terminate (SEMAPHORE id)
{
    sem_t *sp = (sem_t *) id;

    if (id != SEM_ERROR)
        sem_close (sp);
}

#else
#error "undefined semaphore type"
#endif
