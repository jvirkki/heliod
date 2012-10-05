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
 * digest.c: Handle Digest authentication.
 *
 * Rob Crittenden
 */


#include "netsite.h"

// Stupid client stuff...
#ifndef XP_CPLUSPLUS
#define XP_CPLUSPLUS
#endif
#include "base64.h"
#include "base/daemon.h"
#include "base/session.h"
#include "frame/req.h"

#include "frame/protocol.h"
#include "frame/log.h"
#include "frame/func.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/conf.h"
#include "frame/conf_api.h"

#include "base/buffer.h"
#include "base/util.h"

#include "libaccess/acl.h"
#include "libaccess/aclproto.h"

#include "safs/digest.h"
#include "safs/init.h"

#include "safs/dbtsafs.h"

#define AUTH_MAX_LINE 1024
#define MAX_DIGEST_OPTIONS 16

/* local function declarations */
static char **sanitize(const char *);
static char *get_var(const char *, char **);

/* From RFC 2617, the Authorization header is going to look something like:
 *
 * Authorization: Digest username="Mufasa",
 *         realm="testrealm@host.com",
 *         nonce="dcd98b7102dd2f0e8b11d0f600bfb0c093",
 *         uri="/dir/index.html",
 *         qop=auth,
 *         nc=00000001,
 *         cnonce="0a4f113b",
 *         response="6629fae49393a05397450978507c4ef1",
 *         opaque="5ccc069c403ebaf9f0171e9517f40e41"
 *
 */
NSAPI_PUBLIC int
parse_digest_user_login(const char *auth, char **user, char **realm, char **nonce, char **uri, char **qop, char **nc, char **cnonce, char **response, char **opaque, char **algorithm)
{
    char **list;

    /* Skip leading whitespace */
    while (*auth && (*auth == ' ')) ++auth;

    if (!(*auth)) return LAS_EVAL_FAIL;

    /* Confirm that this is digest authentication */
    if((strlen(auth) < 7) || strncasecmp(auth, "digest ", 7))
        return LAS_EVAL_FAIL;

    /* Skip the word 'Digest ' and any other trailing whitespace*/
    auth += 7;

    while(*auth && (*auth == ' '))
        ++auth;

    if(!*auth)
        return LAS_EVAL_FAIL;

    /* What's left is a white space-delimited list of name/value pairs */
    list = sanitize(auth);

    if (user) *user = get_var("username", list);
    if (realm) *realm = get_var("realm", list);
    if (nonce) *nonce = get_var("nonce", list);
    if (uri) *uri = get_var("uri", list);
    if (qop) *qop = get_var("qop", list);
    if (nc) *nc = get_var("nc", list);
    if (cnonce) *cnonce = get_var("cnonce", list);
    if (response) *response = get_var("response", list);
    if (opaque) *opaque = get_var("opaque", list);

    FREE(list);

    return LAS_EVAL_TRUE;
}

/* Given a string of name/value pairs delimited by white space, break into
 * a character array for easier searching. Also remove any \" that were added
 * when the headers were parsed.
 */
static char **
sanitize(const char *in)
{
    char **list;
    char *w, *tmp, *t;
    char *work = NULL;
    int i;

    /* Only handle up to MAX_DIGEST_OPTIONS which is enough for valid uses */
    list = (char **) MALLOC(MAX_DIGEST_OPTIONS*(sizeof(char *)));
    for (i=0; i < MAX_DIGEST_OPTIONS; i++) {
       list[i] = NULL;
    }

    work = (char *) MALLOC(strlen(in) + 1);
    strcpy(work, in);

    w = tmp = t = work;

    i=0;
    while (*w) {
       if ((*w != ',')) {
           if ((*w != '\\') && (*w != '"')) {
               *tmp++ = *w;
           }
           *w++;
       } else {
           do {
               *w++;
           } while (*w && (*w == ' ')); /* eat up trailing white space */
           if (t) {
               *tmp = '\0';
               list[i] = STRDUP(t);     // XXX this is currently not free'd
               i++;

               if (i == (MAX_DIGEST_OPTIONS - 1)) {
                   // There should not be this many entries...
                   // Stop processing them, something is wrong by now.
                   ereport(LOG_SECURITY, XP_GetAdminStr(DBT_digestTooMany));
                   goto bail;
               }

               t = tmp;
           }
       }
    }

    /* catch the last entry */
    *tmp = '\0';
    list[i] = STRDUP(t);        // XXX this is currently not free'd
 bail:
    FREE(work);

    return list;
}

/* return the value of a name/value pair specified by varname, otherwise return
 * NULL
 */
static char *
get_var(const char *varname, char **input)
{
    register int x = 0;
    int len = strlen(varname);
    char *ans = NULL;

    while(input[x])  {
    /*  We want to get rid of the =, so len, len+1 */
        if((!strncmp(input[x], varname, len)) && (*(input[x]+len) == '='))  {
            ans = STRDUP(input[x] + len + 1);
            if(!strcmp(ans, ""))
                ans = NULL;
            break;
        }  else
            x++;
    }
    if (ans == NULL)  {
        return NULL;
    }
    return(ans);
}
