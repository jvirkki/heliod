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

#ifndef BASE_SYSTEMS_H
#define BASE_SYSTEMS_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * systems.h: Lists of defines for systems
 * 
 * This sets what general flavor the system is (UNIX, etc.), 
 * and defines what extra functions your particular system needs.
 */


/* --- Begin common definitions for all supported platforms --- */

#define DAEMON_ANY
#define DAEMON_STATS

/* --- End common definitions for all supported platforms --- */

/* --- Begin platform-specific definitions --- */

#if defined(AIX)

#define HAS_IPV6
#define AUTH_DBM
#define BSD_RLIMIT
#undef BSD_SIGNALS
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW|RTLD_GLOBAL
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATFS
#define HAVE_ATEXIT
#define HAVE_STRERROR_R
#define HAVE_STRTOK_R
#define HAVE_TIME_R 2 /* arg count */
#define HAVE_STRFTIME /* no cftime */
#define JAVA_STATIC_LINK
#undef NEED_CRYPT_H
#define NEED_STRINGS_H /* for strcasecmp */
#define NET_SOCKETS
#define SA_HANDLER_T(x) (void (*)(int))x
#ifdef NS_OLDES3X
#define SA_NOCLDWAIT 0 /* AIX don't got this */
#endif
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#ifdef HW_THREADS
#define THREAD_ANY
#endif
#elif defined(BSDI)

#define AUTH_DBM
#define BSD_MAIL
#define BSD_RLIMIT
#define BSD_SIGNALS
#define BSD_TIME
#define DAEMON_UNIX_MOBRULE
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS (MAP_FILE | MAP_SHARED)
#define HAS_STATFS
#define HAVE_ATEXIT
#undef NEED_CRYPT_PROTO
#define NET_SOCKETS
#define NO_DOMAINNAME
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#define JAVA_STATIC_LINK

#elif defined(HPUX)

#define HAVE_TIME_R 2 /* arg count */
#define AUTH_DBM
#undef BSD_RLIMIT
#undef BSD_SIGNALS
#ifdef MCC_PROXY
#define DAEMON_NEEDS_SEMAPHORE
#else
#define DAEMON_NEEDS_SEMAPHORE
#endif
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_HPSHL
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_PRIVATE
#define HAS_STATFS
#define HAVE_ATEXIT
#define HAVE_STRFTIME
#define JAVA_STATIC_LINK
#undef NEED_CRYPT_H
#define NET_SOCKETS
#define SA_HANDLER_T(x) (void (*)(int))x
#define SEM_FLOCK
/* warning: mmap doesn't work under 9.04 */
#define SHMEM_MMAP_FLAGS MAP_FILE | MAP_VARIABLE | MAP_SHARED

#elif defined (IRIX)

#define AUTH_DBM
#define BSD_RLIMIT
#undef BSD_SIGNALS
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define HAVE_STRTOK_R
#define HAVE_TIME_R 2 /* arg count */
#define JAVA_STATIC_LINK
#define NEED_CRYPT_H
#define NET_SOCKETS
#define SA_HANDLER_T(x) (void (*)(int))x
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#define THROW_HACK throw()

#elif defined(Linux)

#define HAS_IPV6
#define AUTH_DBM
#define BSD_RLIMIT
#undef BSD_SIGNALS
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define HAVE_STRTOK_R
#define HAVE_TIME_R 2 /* arg count */
#define NEED_CRYPT_H
#undef NEED_FILIO
#define NEED_GHN_PROTO
#define NET_SOCKETS
#define SA_HANDLER_T(x) (void (*)(int))x
#undef NEED_GHN_PROTO 

#elif defined(NCR)

#define AUTH_DBM
#undef BSD_RLIMIT
/* #define DAEMON_NEEDS_SEMAPHORE */
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define HAVE_STRTOK_R
#define JAVA_STATIC_LINK
#define NEED_CRYPT_H
#define NEED_FILIO
#define NEED_GHN_PROTO
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined(NEC)

#define DNS_CACHE
#define AUTH_DBM
#undef BSD_RLIMIT
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DLL_CAPABLE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define HAVE_STRTOK_R
#define HAVE_TIME_R 2 /* arg count */
#define JAVA_STATIC_LINK
#define NEED_CRYPT_H
#define NEED_FILIO
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined(OSF1)

#define HAS_IPV6
#define AUTH_DBM
#define BSD_RLIMIT
#undef BSD_SIGNALS
#define BSD_TIME
#define DAEMON_UNIX_MOBRULE
#define DAEMON_NEEDS_SEMAPHORE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAVE_ATEXIT
#define HAVE_STRFTIME /* no cftime */
#define HAVE_TIME_R 2 /* ctime_r arg count */
#define NET_SOCKETS
#define SA_HANDLER_T(x) (void (*)(int))x
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined(SCO)

#define AUTH_DBM
#undef BSD_RLIMIT
#undef BSD_SIGNALS
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#undef NEED_CRYPT_H
#undef NEED_FILIO
#undef NEED_GHN_PROTO
#undef NEED_SETEID_PROTO /* setegid, seteuid */
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#define SA_HANDLER_T(x) (void (*)(int))x


#elif defined(SNI)

#define AUTH_DBM
#undef BSD_RLIMIT
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define JAVA_STATIC_LINK
#define NEED_CRYPT_H
#define NEED_FILIO
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED
#define USE_PIPE

#elif defined(SOLARIS)

#if defined(ENABLE_IPV6)
#define HAS_IPV6
#endif
#define AUTH_DBM
#define BSD_RLIMIT
#undef BSD_SIGNALS
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW|RTLD_FIRST
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define HAVE_STRTOK_R
#define HAVE_TIME_R 3 /* arg count */
#define NEED_CRYPT_H
#define NEED_FILIO
#define NEED_GHN_PROTO
#define NET_SOCKETS
#define SA_HANDLER_T(x) x 
#undef NEED_GHN_PROTO 
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined (SONY)

#define AUTH_DBM
#undef BSD_RLIMIT
#define DAEMON_NEEDS_SEMAPHORE
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAVE_ATEXIT
#define NEED_CRYPT_H
#define NEED_FILIO
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined(SUNOS4)

#define AUTH_DBM
#define BSD_MAIL
#define BSD_RLIMIT
#define BSD_SIGNALS
#define BSD_TIME
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS 1
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATFS
#undef HAVE_ATEXIT
#undef NEED_CRYPT_H
#define NEED_CRYPT_PROTO
#define NEED_FILIO
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#elif defined(UNIXWARE)

#define AUTH_DBM
#undef BSD_RLIMIT
#define DAEMON_UNIX_MOBRULE
#define DLL_CAPABLE
#define DLL_DLOPEN
#define DLL_DLOPEN_FLAGS RTLD_NOW
#define DNS_CACHE
#define FILE_INHERIT_FCNTL
#define FILE_MMAP_FLAGS MAP_SHARED
#define HAS_STATVFS
#define HAVE_ATEXIT
#define NEED_CRYPT_H
#define NEED_FILIO
#define NEED_GHN_PROTO
#define NEED_SETEID_PROTO /* setegid, seteuid */
#define NET_SOCKETS
#define SEM_FLOCK
#define SHMEM_MMAP_FLAGS MAP_SHARED

#ifndef boolean
#define boolean boolean
#endif

#elif defined (XP_WIN32)      /* Windows NT */

#include <wtypes.h>
#include <winbase.h>

#define AUTH_DBM
#define DAEMON_WIN32
#define DLL_CAPABLE
#define DLL_WIN32
#define DNS_CACHE
#define LOG_BUFFERING
#define HAVE_STRFTIME /* no cftime */
#define NEED_CRYPT_PROTO
#define NEEDS_WRITEV
#define NET_SOCKETS
#define NO_DOMAINNAME
#ifdef BUILD_DLL
#if defined (NSAPI_PUBLIC)
#undef  NSAPI_PUBLIC
#endif
#define NSAPI_PUBLIC __declspec(dllexport)
#else
#if defined (NSAPI_PUBLIC)
#undef  NSAPI_PUBLIC
#endif
#define NSAPI_PUBLIC
#endif /* BUILD_DLL */
#define SEM_WIN32
#define THREAD_ANY
#define THREAD_NSPR_KERNEL
#define USE_NSPR
#define USE_STRFTIME /* no cftime */
#define FILE_DEV_NULL "\\\\.\NUL"

#endif	/* Windows NT */

/* --- Begin defaults for values not defined above --- */

#ifndef DAEMON_LISTEN_SIZE
#define DAEMON_LISTEN_SIZE 128
#endif /* !DAEMON_LISTEN_SIZE */

#ifndef NSAPI_PUBLIC
#define NSAPI_PUBLIC
#endif

#ifndef SA_HANDLER_T
#define SA_HANDLER_T(x) (void (*)())x 
#endif

#ifndef THROW_HACK
#define THROW_HACK /* as nothing */
#endif

#ifndef FILE_DEV_NULL
#define FILE_DEV_NULL "/dev/null"
#endif

/* --- End defaults for values not defined above --- */

/* --- Begin the great debate --- */

/* NS_MAIL builds sec-key.c which calls systhread_init, which requires */
/* that USE_NSPR is defined when systhr.c is compiled.  --lachman */
/* MCC_PROXY does the same thing now --nbreslow -- LIKE HELL --ari */
#if (defined(MCC_HTTPD) || defined(MCC_ADMSERV) || defined(MCC_PROXY) || defined(NS_MAIL)) && defined(XP_UNIX)
#define USE_NSPR
/* XXXrobm This is UNIX-only for the moment */
#define LOG_BUFFERING
#ifdef SW_THREADS
#define THREAD_NSPR_USER
#else
#define THREAD_NSPR_KERNEL
#endif
#define THREAD_ANY
#endif

/* --- End the great debate --- */

#ifndef APSTUDIO_READONLY_SYMBOLS

#ifndef NSPR_PRIO_H
#include "prio.h"
#define NSPR_PRIO_H
#endif /* !NSPR_PRIO_H */

/*
 * These types have to be defined early, because they are defined
 * as (void *) in the public API.
 */

#ifndef SYS_FILE_T
typedef PRFileDesc *SYS_FILE;
#define SYS_FILE_T PRFileDesc *
#endif /* !SYS_FILE_T */

#ifndef SYS_NETFD_T
typedef PRFileDesc *SYS_NETFD;
#define SYS_NETFD_T PRFileDesc *
#endif /* !SYS_NETFD_T */

#ifdef SEM_WIN32

typedef HANDLE SEMAPHORE;
#define SEMAPHORE_T HANDLE
#define SEM_ERROR NULL
/* That oughta hold them (I hope) */
#define SEM_MAXVALUE 32767

#elif defined(SEM_FLOCK)

#define SEMAPHORE_T int
typedef int SEMAPHORE;
#define SEM_ERROR -1

#elif defined(SEM_POSIX)

#define SEM_ERROR ((void *)(-1))
typedef	void* SEMAPHORE_T;

#else /* ! SEM_WIN32 */

typedef int SEMAPHORE;
#define SEMAPHORE_T int
#define SEM_ERROR -1

#endif /* SEM_WIN32 */

#endif /* !APSTUDIO_READONLY_SYMBOLS */

#ifndef XP_CPLUSPLUS
#ifdef __cplusplus
#define XP_CPLUSPLUS
#endif /* __cplusplus */
#endif /* !XP_CPLUSPLUS */

#endif /* BASE_SYSTEMS_H */
