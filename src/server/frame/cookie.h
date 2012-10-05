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

#ifndef FRAME_COOKIE_H
#define FRAME_COOKIE_H

/*
 * cookie.h: Cookie generation
 *
 * Chris Elving
 */

/*
 * COOKIE_MAX_AGE_SESSION instructs cookie_set to set a cookie that lasts only
 * for the browser's current session.
 */
#define COOKIE_MAX_AGE_SESSION LL_MININT

/*
 * COOKIE_MAX_AGE_INFINITE instructs cookie_set to set a cookie that never
 * expires.
 */
#define COOKIE_MAX_AGE_INFINITE LL_MAXINT

/*
 * COOKIE_DATE_SIZE is the smallest buffer than can safely be passed to
 * cookie_format_date.
 */
#define COOKIE_DATE_SIZE sizeof("Tue, 19-Jan-2038 03:14:07 GMT")

/*
 * COOKIE_DATE_LEN is the length, not including trailing nul, of timestamps
 * returned from cookie_format_date.
 */
#define COOKIE_DATE_LEN (COOKIE_DATE_SIZE - 1)

/*
 * cookie_format_date formats a timestamp using the Netscape cookie date
 * format.  The passed buffer must be at least COOKIE_DATE_SIZE bytes in size.
 * Returns the number of bytes written to buf on success or -1 on error.
 */
int cookie_format_date(char *buf, int size, time_t t);

/*
 * cookie_set adds a cookie to the response.  Any of value, path, domain, and
 * expires may be NULL.  If non-NULL, expires should be a preformatted cookie
 * expiration timestamp (e.g. one returned from cookie_date).  If expires is
 * NULL, max_age is used instead.  Returns REQ_PROCEED on success.  Returns
 * REQ_ABORTED on error; system_errmsg can be used to retrieve a localized
 * description of the error.
 */
int cookie_set(Session *sn, Request *rq, const char *name, const char *value, const char *path, const char *domain, const char *expires, PRInt64 max_age, PRBool secure);

#endif /* FRAME_COOKIE_H */
