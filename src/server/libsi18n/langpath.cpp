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
#include "i18n.h"

static inline int
copy_tag(char *buf, const char *tag)
{
    const char *end = tag + XP_MAX_LANGUAGE_TAG_LEN;
    const char *in = tag;
    char *out = buf;

    while (*in && in < end) {
        if (isalpha(*in)) {
            *out++ = tolower(*in);
        } else if (*in == '-') {
            *out++ = '_';
        }
        in++;
    }

    return out - buf;
}

NSAPI_PUBLIC
int
XP_FormatLanguageFile(const char *path,
                      int len,
                      const char *tag,
                      char *buf,
                      int size)
{
    const char *ext = path + len - 1;
#ifdef XP_WIN32
    while (*ext != '/' && *ext != '\\' && *ext != '.' && ext > path)
        ext--;
#else
    while (*ext != '/' && *ext != '.' && ext > path)
        ext--;
#endif

    const char *suffix;
    int prefix_len;
    int suffix_len;
    if (*ext == '.') {
        suffix = ext;
        prefix_len = suffix - path;
        suffix_len = len - prefix_len;
    } else {
        suffix = NULL;
        prefix_len = len;
        suffix_len = 0;
    }

    int pos = 0;

    memcpy(buf + pos, path, prefix_len);
    pos += prefix_len;

    buf[pos++] = '_';
    pos += copy_tag(buf + pos, tag);

    memcpy(buf + pos, suffix, suffix_len);
    pos += suffix_len;

    buf[pos] = '\0';

    return pos;
}

NSAPI_PUBLIC
int
XP_FormatLanguageDir(const char *path,
                     int len,
                     const char *tag,
                     char *buf,
                     int size)
{
    const char *filename = path + len - 1;
#ifdef XP_WIN32
    while (*filename != '/' && *filename != '\\' && filename >= path)
        filename--;
    char separator = (filename >= path && *filename == '\\') ? '\\' : '/';
#else
    while (*filename != '/' && filename >= path)
        filename--;
#endif
    filename++;

    int dir_len = filename - path;
    int filename_len = len - dir_len;

    int pos = 0;

    memcpy(buf + pos, path, dir_len);
    pos += dir_len;

    pos += copy_tag(buf + pos, tag);
#ifdef XP_WIN32
    buf[pos++] = separator;
#else
    buf[pos++] = '/';
#endif

    memcpy(buf + pos, filename, filename_len);
    pos += filename_len;

    buf[pos] = '\0';

    return pos;
}
