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

#ifndef DATE_H
#define DATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include "nspr.h"

/* Types for user-supplied date formatter callbacks */
typedef int (*date_time_formatter_t)(time_t t, char *buffer, size_t size, void *context);
typedef int (*date_local_formatter_t)(struct tm *local, char *buffer, size_t size, void *context);
typedef int (*date_gmt_formatter_t)(struct tm *gmt, char *buffer, size_t size, void *context);
typedef int (*date_prtime_formatter_t)(PRTime prtime, char *buffer, size_t size, void *context);
typedef int (*date_local_prexplodedtime_formatter_t)(PRExplodedTime *local, char *buffer, size_t size, void *context);
typedef int (*date_gmt_prexplodedtime_formatter_t)(PRExplodedTime *gmt, char *buffer, size_t size, void *context);

/* Type for registered date formats */
typedef struct date_format date_format_t;

/* Preregistered date formats */
extern date_format_t *date_format_http;
extern date_format_t *date_format_clf;
extern date_format_t *date_format_shtml_gmt;
extern date_format_t *date_format_shtml_local;
extern date_format_t *date_format_locale;

/* Recommended buffer sizes for preregistered date formats */
#define DATE_SHTML_GMT_FORMATTED_SIZE 42
#define DATE_SHTML_LOCAL_FORMATTED_SIZE 64
#define DATE_LOCALE_FORMATTED_SIZE 256

/* Initialize date subsystem */
int date_init(void);

/* Register a user-supplied date formatter callback */
date_format_t *date_register_time_formatter(date_time_formatter_t fn, size_t size, void *context);
date_format_t *date_register_local_formatter(date_local_formatter_t fn, size_t size, void *context);
date_format_t *date_register_gmt_formatter(date_gmt_formatter_t fn, size_t size, void *context);
date_format_t *date_register_prtime_formatter(date_prtime_formatter_t fn, size_t size, void *context);
date_format_t *date_register_local_prexplodedtime_formatter(date_local_prexplodedtime_formatter_t fn, size_t size, void *context);
date_format_t *date_register_gmt_prexplodedtime_formatter(date_gmt_prexplodedtime_formatter_t fn, size_t size, void *context);

/* Get the current date/time */
time_t date_current_time(void);
PRTime date_current_prtime(void);
void date_current_localtime(struct tm *tm);
void date_current_gmtime(struct tm *tm);
void date_current_local_prexplodedtime(PRExplodedTime *local);
void date_current_gmt_prexplodedtime(PRExplodedTime *gmt);
int date_current_formatted(date_format_t *format, char *buffer, int size);

/* Format a timestamp */
int date_format_time(time_t t, date_format_t *format, char *buffer, int size);
int date_format_prtime(PRTime prtime, date_format_t *format, char *buffer, int size);

/* Date formatter callbacks that accept a util_strftime() format as context */
int date_formatter_local_strftime(struct tm *local, char *buffer, size_t size, void *fmt);
int date_formatter_gmt_strftime(struct tm *gmt, char *buffer, size_t size, void *fmt);

#ifdef __cplusplus
}
#endif

#endif /* DATE_H */
