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

/**************************************************************************/
/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE                               */
/* OF NETSCAPE COMMUNICATIONS CORPORATION                                 */
/*                                                                        */
/* Copyright © 1996,1997 Netscape Communications Corporation.  All Rights */
/* Reserved.  Use of this Source Code is subject to the terms of the      */
/* applicable license agreement from Netscape Communications Corporation. */
/*                                                                        */
/* The copyright notice(s) in this Source Code does not indicate actual   */
/* or intended publication of this Source Code.                           */
/**************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "netsite.h"
#include "i18n.h"

/*
 * NUM_AUDIENCES is the number of audience types.
 *
 * Because XPLanguageAudience values are used as indeces into arrays that
 * contain NUM_AUDIENCES elements, XPLanguageAudience values must fall between
 * 0 and NUM_AUDIENCES - 1, inclusive.
 */
#define NUM_AUDIENCES 3

/*
 * LanguageNode tracks a string corresponding to a most preferred language.
 */
struct LanguageNode {
    LanguageNode *next;
    char *language;
};

/*
 * CallbackNode is a node in a linked list of XPGetLanguageRangesFn callback
 * functions.
 */
struct CallbackNode {
    CallbackNode *next;
    XPGetLanguageRangesFn *fn;
    void *data;    
};

/*
 * Languages tracks registered callback functions and language-range strings
 * for a single audience type.
 */
struct Languages {
    CallbackNode *callbacks;
    char *ranges;
};

/*
 * AudiencesLanguages tracks language settings for all audience types.
 */
struct AudiencesLanguages {
    Languages audiences[NUM_AUDIENCES];
};

/*
 * ThreadLanguageData tracks language settings and language enumeration states
 * for a single thread.
 */
struct ThreadLanguageData : AudiencesLanguages {
    XPLanguageEnumState *enums;
};

/*
 * XPLanguageEnumState tracks language enumeration state.  Every
 * XPLanguageEnumState object is part of a particular ThreadLanguageData's
 * enums linked list and is uniquely identified by id, the address of a
 * variable on the caller's stack.
 */
struct XPLanguageEnumState {
    XPLanguageEnumState *next;
    PRBool busy;
    ThreadLanguageData *tld;
    char **tags;
    int pos;
    PRBool abbrev;
    XPLanguageAudience audience;
    char buf[XP_MAX_LANGUAGE_PRIMARY_TAG_LEN + 1];
};

NSPR_BEGIN_EXTERN_C
NSAPI_PUBLIC const char *GetClientLanguage(void);
NSAPI_PUBLIC const char *GetAdminLanguage(void);
static void thread_language_destructor(void *data);
NSPR_END_EXTERN_C

static AudiencesLanguages process_languages;
static PRUintn thread_language_key;
static PRStatus thread_language_key_rv = PR_NewThreadPrivateIndex(&thread_language_key, thread_language_destructor);
static PRLock *language_lock = PR_NewLock();
static LanguageNode *language_nodes;

void
thread_language_destructor(void *data)
{
    ThreadLanguageData *tld = (ThreadLanguageData *) data;

    for (int i = 0; i < NUM_AUDIENCES; i++) {
        Languages *l = &tld->audiences[i];

        CallbackNode *cb = l->callbacks;
        while (cb) {
            CallbackNode *next = cb->next;
            free(cb);
            cb = next;
        }

        free(l->ranges);
    }

    free(tld);
}

static ThreadLanguageData *
get_thread_language_data(void)
{
    ThreadLanguageData *tld;

    tld = (ThreadLanguageData *) PR_GetThreadPrivate(thread_language_key);
    if (tld == NULL) {
        tld = (ThreadLanguageData *) calloc(1, sizeof(ThreadLanguageData));
        PR_SetThreadPrivate(thread_language_key, tld);
    }

    return tld;
}

static Languages *
get_languages(XPLanguageScope scope, XPLanguageAudience audience)
{
    AudiencesLanguages *al;

    if (audience < 0 || audience >= NUM_AUDIENCES)
        return NULL;

    /* Get all audiences' language settings for the requested scope */
    if (scope == XP_LANGUAGE_SCOPE_PROCESS) {
        al = &process_languages;
    } else {
        al = get_thread_language_data();
    }

    /* Return the specific audience's language settings */
    return &al->audiences[audience];
}

NSAPI_PUBLIC
void
XP_SetLanguageRanges(XPLanguageScope scope, XPLanguageAudience audience, const char *ranges)
{
    Languages *l = get_languages(scope, audience);
    if (l) {
        free(l->ranges);
        l->ranges = strdup(ranges);
    }
}

NSAPI_PUBLIC
void
XP_RegisterGetLanguageRangesCallback(XPLanguageScope scope,
                                     XPLanguageAudience audience,
                                     XPGetLanguageRangesFn *fn,
                                     void *data)
{
    Languages *l = get_languages(scope, audience);
    if (l) {
        /* Add a new callback to the front of the list */
        CallbackNode *cb = (CallbackNode *) calloc(1, sizeof(CallbackNode));
        cb->next = l->callbacks;
        cb->fn = fn;
        cb->data = data;
        l->callbacks = cb;
    }
}

static const char *
do_callbacks(Languages *l)
{
    /* Try each registered callback in turn */
    for (CallbackNode *cb = l->callbacks; cb; cb = cb->next) {
        const char *rv = (*cb->fn)(cb->data);
        if (rv)
            return rv;
    }

    return NULL;
}

static const char *
get_language_ranges(XPLanguageScope scope, XPLanguageAudience audience)
{
    Languages *l = get_languages(scope, audience);
    if (l) {
        const char *rv = do_callbacks(l);
        if (rv)
            return rv;

        if (l->ranges)
            return l->ranges;
    }

    return NULL;
}

NSAPI_PUBLIC
const char *
XP_GetLanguageRanges(XPLanguageAudience audience)
{
    const char *rv = get_language_ranges(XP_LANGUAGE_SCOPE_THREAD, audience);
    if (rv)
        return rv;

    return get_language_ranges(XP_LANGUAGE_SCOPE_PROCESS, audience);
}

static const char *
get_language(const char *ranges)
{
    const char *rv = NULL;

    /* If ranges specifies a preferred language... */
    char **tags = XP_ParseLanguageTags(ranges);
    if (tags != NULL) {
        if (tags[0] != NULL) {
            PR_ASSERT(strlen(tags[0]) > 0);

            PR_Lock(language_lock);

            /* Look for an existing copy of this language */
            LanguageNode *node = language_nodes;
            while (node) {
                if (!strcmp(tags[0], node->language))
                    break;
                node = node->next;
            }

            /* Make a new copy of this language if necessary */
            if (node == NULL) {
                node = (LanguageNode *) calloc(1, sizeof(LanguageNode));
                if (node) {
                    node->language = strdup(tags[0]);
                    node->next = language_nodes;
                    language_nodes = node;
                }
            }

            /* Point caller to the copy of the language */
            if (node)
                rv = node->language;

            PR_Unlock(language_lock);
        }

        XP_FreeLanguageTags(tags);
    }

    return rv;
}

NSAPI_PUBLIC
const char *
XP_GetLanguage(XPLanguageAudience audience)
{
    const char *rv = get_language(XP_GetLanguageRanges(audience));
    if (rv == NULL && audience != XP_LANGUAGE_AUDIENCE_DEFAULT)
        rv = get_language(XP_GetLanguageRanges(XP_LANGUAGE_AUDIENCE_DEFAULT));

    return rv;
}

NSAPI_PUBLIC
const char *
GetClientLanguage(void)
{
    const char *rv = XP_GetLanguage(XP_LANGUAGE_AUDIENCE_CLIENT);
    if (!rv)
        rv = "";

    return rv;
}
 
NSAPI_PUBLIC
const char *
GetAdminLanguage(void)
{
    const char *rv = XP_GetLanguage(XP_LANGUAGE_AUDIENCE_ADMIN);
    if (!rv)
        rv = "";

    return rv;
}

static void
update_enum_tags(XPLanguageEnumState *e, XPLanguageAudience audience)
{
    const char *ranges = XP_GetLanguageRanges(audience);
    XP_FreeLanguageTags(e->tags);
    e->tags = XP_ParseLanguageTags(ranges);
    e->pos = 0;
    e->abbrev = PR_FALSE;
    e->audience = audience;
}

NSAPI_PUBLIC
const char *
XP_EnumAudienceLanguageTags(XPLanguageAudience audience,
                            XPLanguageEnumState **pstate)
{
    /* If this is the beginning of a new enumeration... */
    if (*pstate == NULL) {
        ThreadLanguageData *tld = get_thread_language_data();
        if (tld == NULL)
            return NULL;

        /* Look for an XPLanguageEnumState we can reuse */
        XPLanguageEnumState *e = tld->enums;
        while (e) {
            if (!e->busy)
                break;
            e = e->next;
        }

        /* Allocate a new XPLanguageEnumState if necessary */
        if (e == NULL) {
            e = (XPLanguageEnumState *) calloc(1, sizeof(XPLanguageEnumState));
            if (e == NULL)
                return NULL;
            e->next = tld->enums;
            e->tld = tld;
            tld->enums = e;
        }

        /* Reset the XPLanguageEnumState to prepare for a new enumeration */
        e->busy = PR_TRUE;
        update_enum_tags(e, audience);

        *pstate = e;
    }

    XPLanguageEnumState *e = *pstate;

    for (;;) {
        while (e->tags == NULL || e->tags[e->pos] == NULL) {
            /* No more tags in this array, what next? */
            if (e->pos > 0 && !e->abbrev) {
                /* On to the abbreviated tags */
                e->pos = 0;
                e->abbrev = PR_TRUE;
            } else if (e->audience != XP_LANGUAGE_AUDIENCE_DEFAULT) {
                /* On to the default audience */
                update_enum_tags(e, XP_LANGUAGE_AUDIENCE_DEFAULT);
            } else {
                /* End of enumeration */
                return NULL;
            }
        }

        /* Get the next tag from the tags array */
        const char *tag = e->tags[e->pos];
        e->pos++;

        PR_ASSERT(strlen(tag) > 0);
        PR_ASSERT(strlen(tag) < XP_MAX_LANGUAGE_TAG_LEN);

        if (e->abbrev) {
            /* Use the abbreviated tag (e.g. the "en" in "en-us") */
            int i;
            for (i = 0; i < sizeof(e->buf) - 1 && isalpha(tag[i]); i++)
                e->buf[i] = tag[i];
            if (tag[i] != '-' && tag[i] != '_')
                continue;
            e->buf[i] = '\0';
            tag = e->buf;
        }

        return tag;
    }
}

NSAPI_PUBLIC
void
XP_EndLanguageEnum(XPLanguageEnumState **pstate)
{
    if (*pstate) {
        (*pstate)->busy = PR_FALSE;
        XP_FreeLanguageTags((*pstate)->tags);
        (*pstate)->tags = NULL;
        *pstate = NULL;
    }
}
