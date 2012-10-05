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
 * httpd.h: header for simple (ha! not anymore) http daemon
 *
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 */


/* Grow allow,deny,auth tables by this amount whenever needed */
#define SECTABLE_ALLOC_UNIT 5

/* Define this to be what your per-directory security files are called */
#define DEFAULT_ACCESS_FNAME ".htaccess"

/* The default string lengths */
#define MAX_STRING_LEN 256

/* The method numbers are retrieved from the dynamic HttpMethodRegistry
 * to support custom methods.  .htaccess uses bit shifting of size int so 
 * it is unsafe to have more methods than bits in an int, limiting to 32.  
 * GET == HEAD for our purposes.
 */
#define MAX_METHODS 32

#define NO_MEMORY 6992


#include <stdio.h>
#include <stdlib.h>
#ifdef XP_UNIX
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#ifndef MAXPATHLEN
#include <sys/param.h>
#endif /* !MAXPATHLEN */
#include <pwd.h>
#include <grp.h>
#else
#include <winsock.h>
#endif /* XP_UNIX */
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>  /* for ctime */
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

/* cfg_* uses netscape functions now */
#include "base/file.h"

/* For access control */
#define DENY_THEN_ALLOW 0
#define ALLOW_THEN_DENY 1
#define MUTUAL_FAILURE 2

#define ACCESS_OK 0
#define ACCESS_FORBIDDEN 1
#define ACCESS_AUTHFAIL 2

/* Struct shared by access and auth */
typedef struct secdata {
    char *d;

    int order[MAX_METHODS];

    int max_num_allow;
    int num_allow;
    char ** allow;
    int * allow_methods;
    
    int max_num_auth;
    int num_auth;
    char ** auth;
    int * auth_methods;

    int max_num_deny;
    int num_deny;
    char ** deny;
    int * deny_methods;

    char *auth_type;
    char *auth_name;
    char *auth_pwfile;
    char *auth_grpfile;
    int auth_nsdb;

    struct secdata * next;
} security_data;

/* number of security directives in access config file (defined to zero 
   because there is none here --Rob) */
#define num_sec_config 0

/* Prototypes moved to htaccess.h to avoid struct def snafus --Rob */
