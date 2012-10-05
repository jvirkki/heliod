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
 * wdconf.h Watchdog parsing code for server config files.
 *
 *
 *
 *
 */

#ifndef _WDCONF_H_
#define _WDCONF_H_

#include "wdbind.h"

#define MAXSOCK 1024
#define	RESTART_ON_EXIT_CODE	40

typedef struct watchdog_conf_info_t {
    server_socket_t  sockets[MAXSOCK];    /* server sockets */
    int              numSockets;          /* number of server sockets */
    int              secOn;               /* security on */
    int              rsaOn;               /* RSA cipher on */
    char            *pidPath;             /* path to server PID file */
    char            *serverUser;          /* User to run server as */
#ifdef XP_UNIX
    char            *chroot;              /* Directory for chroot() */
#endif /* XP_UNIX */
    char            *tempDir;             /* Directory for temporary files */
} watchdog_conf_info_t;

watchdog_conf_info_t *watchdog_parse(char *conf_dir, char *conf_file);
void watchdog_confinfo_free(watchdog_conf_info_t *info);

/* debugging routine */
void watchdog_print_confinfo(watchdog_conf_info_t *);

#endif /* _WDCONF_H_ */
