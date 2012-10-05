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



#include "base/crit.h"
#include "base/pblock.h"
#include "base/util.h"
#include "frame/func.h"
#include "frame/protocol.h"
#include "frame/log.h"

#include "htaccess.h"
#include "libaccess/cryptwrapper.h"

/* Flag to indicate if AuthUserFile file contains groups for each user */
int _ht_gwu = 0;

/* Name of SAF to handle HTTP Basic authentication */
char *_ht_fn_basic = "basic-auth";

/* Name of SAF to handle user database lookup */
char *_ht_fn_user = 0;

/* Name of SAF to handle combined user/group database lookup */
char *_ht_fn_usrgrp = 0;


#ifdef __cplusplus
extern "C"
#endif

NSAPI_PUBLIC int
htaccess_userdb(pblock *pb, Session *sn, Request *rq)
{
    char *userdb = pblock_findval("userdb", pb);
    char *user = pblock_findval("user", pb);
    char *pw = pblock_findval("pw", pb);
    char *t;
    char *cp;
    SYS_FILE fd;
    filebuffer *buf;
    int ln;
    int eof;
    struct stat finfo;
    char line[1024];

    if (!userdb) {
        log_error(LOG_MISCONFIG, "htaccess-userdb", sn, rq,
                  "missing \"userdb\" argument for SAF");
        return REQ_ABORTED;
    }

    if (!user) {
        log_error(LOG_MISCONFIG, "htaccess-userdb", sn, rq,
                  "missing \"user\" argument for SAF");
        return REQ_ABORTED;
    }

    if (!pw) {
        log_error(LOG_MISCONFIG, "htaccess-userdb", sn, rq,
                  "missing \"pw\" argument for SAF");
        return REQ_ABORTED;
    }

    if ((system_stat(userdb, &finfo) < 0) || !S_ISREG(finfo.st_mode)) {
        log_error(LOG_MISCONFIG, "htaccess-userdb", sn, rq,
                  "invalid user database file %s", userdb);
        return REQ_ABORTED;
    }

    /* Open user file */

    fd = system_fopenRO(userdb);
    if (fd == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, "htaccess-userdb", sn, rq, 
                  "can't open basic user/group file %s (%s)", userdb, 
                  system_errmsg());
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    buf = filebuf_open(fd, FILE_BUFFERSIZE);
    if(!buf) {
        log_error(LOG_FAILURE, "htaccess-userdb", sn, rq, 
                  "can't open buffer from password file %s (%s)", userdb, 
                  system_errmsg());
        protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        system_fclose(fd);
        return REQ_ABORTED;
    }

    /* Scan user file for desired user */
    for (eof = 0, ln = 1; !eof; ++ln) {

        eof = util_getline(buf, ln, sizeof(line), line);
        if (line[0]) {

            /* Look for ':' terminating user name */
            t = strchr(line, ':');
            if (t) {

                /* Terminate user name */
                *t++ = '\0';

                /* Is this the desired user? */
                if (!strcmp(line, user)) {

                    /* Look for colon at end of password */
                    cp = strchr(t, ':');
                    if (cp) {

                        /* Terminate password, advance to start of groups */
                        *cp++ = '\0';
                    }

                    if (ACL_CryptCompare(pw, t, t) != 0) {
                        log_error(LOG_SECURITY, "htaccess-userdb", sn, rq, 
                          "user %s password did not match user database %s", 
                                  user, userdb);
                        break;
                    }

                    /* Got a match, so try for groups  */
                    if (cp) {

                        /* Save start of groups */
                        t = cp;

                        /* Terminate groups at next ':' or end-of-line */
                        cp = strchr(t, ':');
                        if (cp) *cp = 0;

                        /* Set comma-separated list of groups */
                        pblock_nvinsert("auth-group", t, rq->vars);
                    }
                    else {
                        /* Set an empty group list */
                        pblock_nvinsert("auth-group", "", rq->vars);
                    }

                    filebuf_close(buf);
                    return REQ_PROCEED;
                }
            }
        }

    }

    /* The desired user was not found, or the password didn't match */
    filebuf_close(buf);
    return REQ_NOACTION;
}

#ifdef __cplusplus
extern "C"
#endif

NSAPI_PUBLIC int
htaccess_init(pblock *pb, Session *unused2, Request *unused3)
{
    char *gwu = pblock_findval("groups-with-users", pb);
    char *fnbasic = pblock_findval("basic-auth-fn", pb);
    char *fnuser = pblock_findval("user-auth-fn", pb);

    _ht_gwu = (gwu && !strcasecmp(gwu, "yes"));
    _ht_fn_basic = (fnbasic) ? fnbasic : (char *)"basic-auth";
    _ht_fn_user = (fnuser) ? fnuser : 0;

    if (!func_find("htaccess-userdb")) {
        func_insert("htaccess-userdb", htaccess_userdb);
    }
    if (!func_find("htaccess-register")) {
        func_insert("htaccess-register", htaccess_register);
    }

    return REQ_PROCEED;
}

#ifdef __cplusplus
extern "C"
#endif

NSAPI_PUBLIC int 
htaccess_find(pblock *pb, Session *sn, Request *rq)
{
    int rv;

    rv = htaccess_evaluate(pb, sn, rq);

    /*
     * This must be done at the end, because AuthTrans functions
     * we call may set it to 1.
     */
    rq->directive_is_cacheable = 0;

    return rv;
}
