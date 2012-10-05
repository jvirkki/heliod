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

#include "netsite.h"
#include "base/util.h"
#include "i18n.h"

/*
 * RFC 2616
 *
 * Accept-Language = "Accept-Language" ":"
 *                   1#( language-range [ ";" "q" "=" qvalue ] )
 *
 * language-range = ( ( 1*8ALPHA *( "-" 1*8ALPHA ) ) | "*" )
 *
 * qvalue = ( "0" [ "." 0*3DIGIT ] )
 *        | ( "1" [ "." 0*3("0") ] )
 */

NSAPI_PUBLIC
char **
XP_ParseLanguageTags(const char *ranges)
{
    /* Determine the number and size of the tags we may need to store */
    int len = 0;
    int n = 0;
    if (ranges) {
        const char *p = ranges;
        while (*p) {
            while (*p && !isalpha(*p))
                p++;
            while (*p && *p != ',')
                p++;
            n++;
        }
        len = p - ranges;
    }

    /*
     * Create a buffer that will store a NULL-terminated char * tag array,
     * followed by an int quality array, followed by nul-delimited tag char
     * content.
     */

    int size = ((n + 1) * sizeof(char *)) + (n * sizeof(int)) + len + 1;
    void *buf = malloc(size);
    if (!buf)
        return NULL;

    char **tags = (char **) buf;
    int *qualities = (int *) &tags[n + 1];
    char *content = (char *) &qualities[n];

    /* Give each tag its own array element */
    int prev_quality = 1000;
    PRBool need_quality_sort = PR_FALSE;
    int i = 0;
    if (ranges) {
        const char *p = ranges;
        while (*p) {
            while (isspace(*p) || *p == ',')
                p++;

            /* Parse the tag ( 1*8ALPHA *( "-" 1*8ALPHA ) ) */
            tags[i] = content;
            const char *tag = p;
            while (isalpha(*p))
                *content++ = *p++;
            int primary_len = p - tag;
            int sub_len = 0;
            if (*p == '-') {
                p++;
                if (isalpha(*p)) {
                    *content++ = '-';
                    const char *sub = p;
                    while (isalpha(*p))
                        *content++ = *p++;
                    sub_len = p - sub;
                }
            }
            *content++ = '\0';

            /* Skip to the end or quality of the current language-range */
            while (*p && *p != ',' && *p != ';')
                p++;

            /* Parse the quality ( ";" "q" "=" qvalue ) */
            qualities[i] = util_qtoi(p, &p);
            if (prev_quality < qualities[i])
                need_quality_sort = PR_TRUE;
            prev_quality = qualities[i];

            /* If the tag is valid... */
            if (primary_len > 0 && primary_len <= XP_MAX_LANGUAGE_PRIMARY_TAG_LEN) {
                if (sub_len <= XP_MAX_LANGUAGE_SUB_TAG_LEN) {
                    /* Remember the tag if its quality is non-zero */
                    if (qualities[i] > 0)
                        i++;
                }
            }

            /* Skip to the end of the current language-range */
            while (*p && *p != ',')
                p++;
        }
    }

    /* Terminate the tags array */
    PR_ASSERT(i <= n);
    tags[i] = NULL;
    n = i;

    /* Sort the tags based on quality */
    if (need_quality_sort) {
        /* Adjust qualities to preserve order of tags with equal qualities */
        for (i = 0; i < n; i++)
            qualities[i] = (qualities[i] << 16) + (n - i);

        /* Simple bubble sort assuming small n */
        for (i = 0; i < n - 1; i++) {
            for (int j = i + 1; j < n; j++) {
                if (qualities[j] > qualities[i]) {
                    int quality = qualities[j];
                    qualities[j] = qualities[i];
                    qualities[i] = quality;
                    char *tag = tags[j];
                    tags[j] = tags[i];
                    tags[i] = tag;
                }
            }
        }
    }

    return tags;
}

NSAPI_PUBLIC
void
XP_FreeLanguageTags(char **tags)
{
    free(tags);
}
