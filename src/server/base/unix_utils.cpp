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

/*
 * Non-NSPR Unix Utility functions
 *
 * This file is used by the watchdog and hence must be NSPR-free.
 */

#ifndef AIX
#define HAVE_GETRLIMIT
#endif

#ifdef HAVE_GETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>          // getrlimit()
#else
#include <sys/limits.h>
#endif
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>                // dup2()


/*
 * Generic routines for getting/setting the file descriptor table on 
 * various platforms...
 *
 * AIX-		OPEN_MAX		-/2000
 * BSD-		GETRLIMIT		64/9830
 * DEC-		GETRLIMIT		4096/4096
 * HPUX-	GETRLIMIT		60/1024
 * IRIX-	GETRLIMIT		200/2500
 * SOLARIS-	GETRLIMIT		64/1024
 * SUNOS-	GETRLIMIT		64/256
 *
 * Mike Belshe
 * 01-12-96
 *
 */

int
maxfd_get(void)
{
#ifdef HAVE_GETRLIMIT		/* BEST */
	struct rlimit rlim;
	if ( getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		return -1;
	}
	return rlim.rlim_cur;
#elif defined(OPEN_MAX)
	return OPEN_MAX;
#else
	MAXFD_GET WILL NOT WORK
#endif
}

int
maxfd_getmax(void)
{
#ifdef HAVE_GETRLIMIT		/* BEST */
	struct rlimit rlim;
	if ( getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		return -1;
	}
	return rlim.rlim_max;
#elif defined(OPEN_MAX)
	return OPEN_MAX;
#else
	MAXFD_GETMAX WILL NOT WORK
#endif
}

int
maxfd_getopen(void)
{
    int nfd = 0;

    DIR *dir = opendir("/proc/self/fd");
    if (dir) {
        while (readdir(dir))
            nfd++;
        closedir(dir);
    }

    if (nfd >= 3) {
        /*
         * opendir() used up one fd, and a working /proc/self/fd should always
         * have entries for least . and ..
         */
        nfd -= 3;
    } else {
        /* No /proc/self/fd on this system, do it the slow way */
        int maxfd = maxfd_getmax();
        nfd = 0;
        for (int fd = 0; fd < maxfd; fd++) {
            if (fcntl(fd, F_GETFD, 0) != -1)
                nfd++;
        }
    }

    return nfd;
}

int
maxfd_set(int num_files)
{
#ifdef HAVE_GETRLIMIT
	struct rlimit rlim;
	int maxfd;

	if ( (maxfd = maxfd_getmax()) < 0)  
		return -1;
	if ( maxfd < num_files)
		return -1;

	rlim.rlim_max = maxfd;
	rlim.rlim_cur = num_files;

	if ( setrlimit(RLIMIT_NOFILE, &rlim) < 0) 
		return -1;

	return rlim.rlim_cur;
#elif defined(OPEN_MAX)
	return -1;
#else
	MAXFD_SET WILL NOT WORK
#endif
}

/*
 * Prevent the file descriptor from being inherited across CGI fork/exec()s
 */
int
setFDNonInheritable(const int fd)
{
    int status = 0;
#ifdef IRIX
    /* FD_CLOEXEC is the one and only flag */
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
        status = -1;
#else
    /* OR the FD_CLOEXEC flag with the existing value of the flag */
    int flags = fcntl(fd, F_GETFD, 0);
    if(flags == -1)
        status = -1;
    else
    {
        flags |= FD_CLOEXEC;
        if (fcntl(fd, F_SETFD, flags) == -1)
            status = -1;
    }
#endif
    return status;
}

void
redirectStdStreams(void)
{
    int fd = open("/dev/null", O_RDWR, 0);
    if (fd >= 0)
    {
        if (fd != 0)
            dup2(fd, 0);           // redirect stdin to /dev/null

        if (fd != 1)
            dup2(fd, 1);           // redirect stdout to /dev/null

        if (fd != 2)
            dup2(fd, 2);           // redirect stderr to /dev/null

        if (fd > 2)
            close(fd);
    }

}

void
redirectStream(const int fd)
{
    int newfd = open("/dev/null", O_RDWR, 0);
    if (newfd >= 0)
    {
        if (newfd != fd)
        {
            dup2(newfd, fd);
            close(newfd);
        }
    }

}
