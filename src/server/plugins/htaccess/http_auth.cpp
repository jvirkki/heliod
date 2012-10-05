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
 * http_auth: authentication
 * 
 * All code contained herein is covered by the Copyright as distributed
 * in the README file in the main directory of the distribution of 
 * NCSA HTTPD.
 *
 * Based on NCSA HTTPd 1.3 by Rob McCool
 * 
 */



#include "base/pblock.h"
#include "base/util.h"
#include "frame/func.h"
#include "frame/protocol.h"
#include "frame/log.h"

#include "htaccess.h"


int htaccess_check_auth(security_data *sec, int m, htaccess_context_s *ctxt)
{
    Request *rq = ctxt->rq;
    char *auth_type = pblock_findval("auth-type", rq->vars);
    char *auth_user;
    char *auth_group = NULL;
    char *auth_fn;
    char *user_fn;
    char *group_fn;
    char *group_db = NULL;
    char *group;
    pblock *npb;
    httoken_t token;
    int rv;
    int x;
    int ulen;
    int glen;
    int grpstatus = 0;

    /* If no AuthType directive, assume HTTP Basic authentication */
    if (!ctxt->auth_type) {
        ctxt->auth_type = STRDUP("basic");
    }

    /* If no AuthName directive, provide a default */
    if (!ctxt->auth_name) {
        ctxt->auth_name = STRDUP("unspecified");
    }

    /* Has the desired type of authentication already happened? */
    if (!auth_type || strcasecmp(auth_type, ctxt->auth_type)) {

        /* No, set up to do it */
        if (!strcasecmp(ctxt->auth_type, "basic")) {

            /* Get name of "basic" authentication SAF */
            auth_fn = _ht_fn_basic;
            if (!auth_fn) {
                log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, rq,
                          "missing Init fn=htaccess-init directive");
                protocol_status(ctxt->sn, rq, PROTOCOL_SERVER_ERROR, NULL);
                return REQ_ABORTED;
            }

            /* Ensure that user database is present */
            if (!ctxt->auth_pwfile && !ctxt->user_check_fn) {
                log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, rq,
                          "missing user database for authentication");
                protocol_status(ctxt->sn, rq, PROTOCOL_SERVER_ERROR, NULL);
                return REQ_ABORTED;
            }

            /*
             * Get name of SAF to handle user database lookup, and set
             * group_db if groups are in a separate database.
             */
            group_db = ctxt->auth_grpfile;

            /* Using custom user/group database? */
            if (!ctxt->auth_pwfile && ctxt->group_check_fn) {
                group_fn = ctxt->group_check_fn;
            } 
#ifdef AUTHNSDBFILE
            /* Using Enterprise 2.0 user database? */
            if (ctxt->auth_nsdb) {

                /* Combined user/group database in this case */
                user_fn = "dbm-userdb";
                group_db = ctxt->auth_pwfile;
            }
            else
#endif /* AUTHNSDBFILE */
            {
                /* Handle combined user/group database if indicated */
                if ((_ht_gwu && !group_db) ||
                    (group_db && !strcmp(ctxt->auth_pwfile, group_db))) {

                    /* Combined database, ignore group_db */
                    group_db = ctxt->auth_pwfile;
                    user_fn = (_ht_fn_usrgrp) ? _ht_fn_usrgrp
                        		      : (char *)"htaccess-userdb";
                }
                else {

                    /* Separate user database */
                    user_fn = (ctxt->user_check_fn) ? ctxt->user_check_fn : (char *)"simple-userdb";
                    /* Separate group database */
                    if (group_db)
                       group_fn = (ctxt->group_check_fn) ? ctxt->group_check_fn : (char *)"simple-groupdb";

                }
            }
        }
        else {
            log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, rq,
                      "unsupported authentication type %s", ctxt->auth_type);
            protocol_status(ctxt->sn, rq, PROTOCOL_SERVER_ERROR, NULL);
            return REQ_ABORTED;
        }
        npb = pblock_create(10);
        pblock_copy(ctxt->pb, npb);
        param_free(pblock_remove("fn", npb)); /* don't want duplicate values */

        pblock_nvinsert("fn", auth_fn, npb);
        pblock_nvinsert("auth-type", ctxt->auth_type, npb);
        pblock_nvinsert("userfn", user_fn, npb);
	if (ctxt->auth_pwfile)
            pblock_nvinsert("userdb", ctxt->auth_pwfile, npb);
        else
            pblock_nvinsert("userdb", "custom", npb);

        if (group_db) {
            if (group_fn && strcmp(ctxt->auth_pwfile, group_db)) {
                pblock_nvinsert("groupdb", ctxt->auth_grpfile, npb);
                pblock_nvinsert("groupfn", group_fn, npb);
            }
        }

        /* Execute the authentication SAF */
        rv = func_exec(npb, ctxt->sn, rq);

        pblock_free(npb);

        if (rv != REQ_PROCEED) {

            /* Request authentication if the SAF returns REQ_NOACTION */
            if ((rv == REQ_NOACTION) || (rv == REQ_ABORTED)) {
                goto bong;
            }

            return rv;
        }
    }

    auth_user = pblock_findval("auth-user", rq->vars);
    if (!auth_user) {
        log_error(LOG_FAILURE, "htaccess-find", ctxt->sn, rq,
                  "user authentication failed to set auth-user");
        protocol_status(ctxt->sn, rq, PROTOCOL_SERVER_ERROR, NULL);
        return REQ_ABORTED;
    }

    ulen = strlen(auth_user);

    /* Check authenticated user against constraints of 'require' directives */
    for (x = 0; x < sec->num_auth; x++) {

        if (htaccess_check_method(sec->auth_methods[x],m) == 0)
            continue;

        HTTOKEN_INIT(&token, sec->auth[x]);
        HTTOKEN_NEXT(&token, ' ');

        if (!HTTOKEN_STRCMP(&token, "valid-user")) {

            /* We already know this is a valid user */
            goto found;
        }

        if (!HTTOKEN_STRCMP(&token, "user")) {

            /* Scan space-separated list of user names */
            while (HTTOKEN_CHPEEK(&token)) {

                /* Skip spaces between user names */
                if (HTTOKEN_CHPEEK(&token) == ' ') {
                    HTTOKEN_CHSKIP(&token);
                    continue;
                }

                /* Handle quoted or unquoted user name */
                if (HTTOKEN_CHPEEK(&token) == '\"') {
                    HTTOKEN_CHSKIP(&token);
                    HTTOKEN_NEXT(&token, '\"');
                }
                else {
                    HTTOKEN_NEXT(&token, ' ');
                }

                /* User name match authenticated user name? */
                if (!HTTOKEN_STRNCMP(&token, auth_user, ulen)) {
                    goto found;
                }
            }
        }
        else if (!HTTOKEN_STRCMP(&token, "group")) {

            /* Initialize group checking if necessary */
            if (grpstatus == 0) {

                /*
                 * The authentication function might have set "auth-group"
                 * to a comma-separated list of group names to which the
                 * user belongs.
                 */
                auth_group = pblock_findval("auth-group", rq->vars);
                if (!auth_group) {

                    if (group_db) {

                        /*
                         * Otherwise we need to access a group file to look
                         * up the required groups and check the user's
                         * membership.
                         */
                        rv = htaccess_init_group(group_db, ctxt);
                        if (rv != REQ_PROCEED) {
                            log_error(LOG_MISCONFIG, "htaccess-find",
                                      ctxt->sn, rq,
                                      "failed to open group database %s",
                                      ctxt->auth_pwfile);
                            protocol_status(ctxt->sn, rq,
                                            PROTOCOL_SERVER_ERROR, NULL);
                            rv = REQ_ABORTED;
                            goto finish;
                        }
                    }
                    else if (ctxt->group_check_fn) {
                        /* Do nothing */
                    }
                    else {
                        log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, rq,
                                  "missing group information for user %s",
                                  auth_user);
                        goto bong;
                    }
                }

                grpstatus = 1;
            }

            /* Scan space-separated list of group names */
            while (HTTOKEN_CHPEEK(&token)) {

                /* Skip spaces between group names */
                if (HTTOKEN_CHPEEK(&token) == ' ') {
                    HTTOKEN_CHSKIP(&token);
                    continue;
                }

                /* Handle quoted or unquoted group name */
                if (HTTOKEN_CHPEEK(&token) == '\"') {
                    HTTOKEN_CHSKIP(&token);
                    HTTOKEN_NEXT(&token, '\"');
                }
                else {
                    HTTOKEN_NEXT(&token, ' ');
                }

                group = HTTOKEN_START(&token);
                glen = HTTOKEN_LENGTH(&token);

                if (!auth_group) {

                    /*
                     * We don't have the list of groups the user is in,
                     * so test for membership in the required group.
                     */
                    if (htaccess_in_group(auth_user, group, glen, ctxt)) {

                        strcpy(ctxt->groupname, group);
                        goto found;
                    }
                }
                else {

                    if (htaccess_check_group(group, glen, auth_group)) {

                        strncpy(ctxt->groupname, group, glen);
                        goto found;
                    }
                }
            }
        }
        else {
            log_error(LOG_MISCONFIG, "htaccess-find", ctxt->sn, rq,
                      "require not followed by user or group");
            rv = REQ_ABORTED;
            goto finish;
        }
    }

    log_error(LOG_SECURITY, "htaccess-find", ctxt->sn, rq,
              "denied access to user %s", auth_user);

  bong:
    {
        char buf[256];


        util_snprintf(buf, sizeof(buf), "%s realm=\"%s\"",
                      ctxt->auth_type, ctxt->auth_name);
        pblock_nvinsert("www-authenticate", buf, rq->srvhdrs);
        protocol_status(ctxt->sn, rq, PROTOCOL_UNAUTHORIZED, NULL);
    }
    rv = REQ_ABORTED;
    goto finish;

  found:
    rv = REQ_PROCEED;

  finish:
    if (grpstatus && !auth_group) {
        htaccess_kill_group(ctxt);
    }

    return rv;
}
