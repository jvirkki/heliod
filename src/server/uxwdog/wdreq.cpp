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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "wdlog.h"

/* Globals in watchdog.c */
extern int _watchdog_death;
extern int _watchdog_server_death;
int watchdog_pwd_prompt(const char *prompt, char **pwdvalue, bool smfwatchdog);
int watchdog_pwd_save(char *pwdname, char *pwdvalue);
int watchdog_pwd_lookup(char *pwdname, char **pwdvalue);


static char *
getqstring(char *instr, char **outstr)
{
    char qc = '"';              /* quote character - any non-space */

    *outstr = 0;

    if (instr) {

        /* Skip leading spaces */
        while (*instr && isspace(*instr)) ++instr;

        if (*instr) {
            /* First non-space is the quote character */
            qc = *instr++;
        }

        /* Return start of quoted string */
        *outstr = instr;

        /* Find closing quote character */
        while (*instr && (*instr != qc)) ++instr;

        /* Null-terminate the output string */
        if (*instr && (*instr == qc)) {
            *instr++ = 0;
        }
    }

    /* Return starting point for next token */
    return instr;
}

int
watchdog_do_request(int server_starts, int pipe_in, int pipe_out)
{
    FILE *pinfile;
    FILE *poutfile;
    char *cp;
    char *pwdname;
    char *pwdvalue;
    char reqbuf[512];
    int reqlen;
    int rv;

    rv = -1;

    pipe_in = dup(pipe_in);
    if (pipe_in < 0) {
        return -1;
    }

    pipe_out = dup(pipe_out);
    if (pipe_out < 0) {
        close(pipe_in);
        return -1;
    }

    pinfile = fdopen(pipe_in, "r");
    poutfile = fdopen(pipe_out, "w");
    if (pinfile && poutfile) {

        /*
         * Turn off buffering on output pipe to avoid leaving a password
         * or PIN in the output buffer.
         */
        setbuf(poutfile, NULL);

        reqbuf[0] = 0;
        fgets(reqbuf, sizeof(reqbuf), pinfile);

        reqlen = strlen(reqbuf);
        if ((reqlen > 0) && (reqbuf[reqlen-1] == '\n')) {
            reqbuf[--reqlen] = 0;
        }

        if (!strncmp(reqbuf, "GETPWD", 6)) {
            cp = getqstring(reqbuf+6, &pwdname);
            if (server_starts == 0) {
                rv = watchdog_pwd_prompt(pwdname, &pwdvalue, false);
                if (rv < 0) {
                    const char *errstr;
                    switch (rv) {
                    case -1:
                        errstr = "end-of-file while reading password";
                        break;
                    case -2:
                        errstr = "invalid password";
                        break;
                    default:
                        errstr = "error while reading password";
                        break;
                    }
                    if (guse_stderr) {
                        fprintf(stderr, "failed: %s.\n", errstr);
                    }
                    watchdog_log(LOG_ERR,
				 "%s.", errstr);
                    _watchdog_death = 1;
#if 0
            /*
             * The exit is by ns-httpd exiting. Letz not force
             * the watchdog to die pramod
             */
                    _watchdog_server_death = 1;
                    goto out;
#endif
                }

                rv = watchdog_pwd_save(pwdname, pwdvalue);
            }
            else {
                rv = watchdog_pwd_lookup(pwdname, &pwdvalue);
            }

            /*
             * Don't use fprintf() to write to output pipe to avoid
             * leaving the password around in memory.
             */
            if (pwdvalue) {
                fputs(pwdvalue, poutfile);
                /* Clear the cleartext password string */
                memset((void *)pwdvalue, 0, strlen(pwdvalue));
                free(pwdvalue);
            }
            fputs("\n", poutfile);
            fflush(poutfile);
        }
        else {
            /* Invalid request */
            if (reqlen > 0) {
                if (guse_stderr) {
                    fprintf(stderr,
                        "Server watchdog received invalid request (%s) from server\n",
                        reqbuf);
                }
		watchdog_log(LOG_ERR,
                        "Server watchdog received invalid request (%s) from server\n",
                        reqbuf);
            }

            /* Encourage the watchdog to give it up */
            _watchdog_death = 1;
            _watchdog_server_death = 1;
        }
    }

  out:

    if (poutfile) {
        /* This flushes output and closes the underlying pipe fd */
        fclose(poutfile);
        pipe_out = -1;
    }

    if (pinfile) {
        /* The underlying pipe fd is already closed at this point */
        fclose(pinfile);
        pipe_in = -1;
    }

    if (pipe_in >= 0) {
        close(pipe_in);
    }

    if (pipe_out >= 0) {
        close(pipe_out);
    }

    return rv;
}

