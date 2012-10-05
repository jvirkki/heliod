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

#include "base/util.h"
#include "base/systhr.h"
#include "time/nstime.h"
#include "date.h"

typedef enum date_formatter_arg {
    DATE_TIME,
    DATE_LOCAL,
    DATE_GMT,
    DATE_PRTIME,
    DATE_LOCAL_PREXPLODEDTIME,
    DATE_GMT_PREXPLODEDTIME
} date_formatter_arg_t;

typedef union date_formatter {
    date_time_formatter_t t;
    date_local_formatter_t local;
    date_gmt_formatter_t gmt;
    date_prtime_formatter_t prtime;
    date_local_prexplodedtime_formatter_t local_prexplodedtime;
    date_gmt_prexplodedtime_formatter_t gmt_prexplodedtime;
} date_formatter_t;

struct date_format {
    date_format_t *next;
    date_formatter_arg_t arg;
    date_formatter_t fn;
    int size;
    void *context;
    int index;
};

typedef struct date_formatted_entry {
    char *buffer;
    int len;
} date_formatted_entry_t;

typedef struct date_entry date_entry_t;

struct date_entry {
    time_t t;
    struct tm local;
    struct tm gmt;
    PRTime prtime;
    PRExplodedTime local_prexplodedtime;
    PRExplodedTime gmt_prexplodedtime;
    date_formatted_entry_t *formatted_entries;
};

static int date_formatter_locale(struct tm *local, char *buffer, size_t size, void *context);

#define NUM_DATE_ENTRIES 3
#define MICROSECONDS_PER_SECOND 1000000

date_format_t *date_format_http = date_register_gmt_formatter(date_formatter_gmt_strftime, HTTP_DATE_LEN, (void *)HTTP_DATE_FMT);
date_format_t *date_format_clf = date_register_local_formatter(date_formatter_local_strftime, 27, (void *)"%d/%b/%Y:%H:%M:%S %z");
date_format_t *date_format_shtml_gmt = date_register_gmt_formatter(date_formatter_gmt_strftime, DATE_SHTML_GMT_FORMATTED_SIZE, (void *)"%A, %d-%b-%y %T GMT");
date_format_t *date_format_shtml_local = date_register_local_formatter(date_formatter_local_strftime, DATE_SHTML_LOCAL_FORMATTED_SIZE, (void *)"%A, %d-%b-%y %T %Z");
date_format_t *date_format_locale = date_register_local_formatter(date_formatter_locale, DATE_LOCALE_FORMATTED_SIZE, NULL);

static int date_initialized = 0;
static date_format_t *date_formats = NULL;
static date_entry date_entries[NUM_DATE_ENTRIES];
static volatile int date_current_index = -1;
static int date_num_formats = 0;

/*
 * date_current_time()
 */
time_t date_current_time(void)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        return ft_time();
    } else {
        /* Use cached entry */
        return date_entries[current_index].t;
    }
}

/*
 * date_current_prtime()
 */
PRTime date_current_prtime(void)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        return PR_Now();
    } else {
        /* Use cached entry */
        return date_entries[current_index].prtime;
    }
}

/*
 * date_current_localtime()
 */
void date_current_localtime(struct tm *tm)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        time_t t = ft_time();
        util_localtime(&t, tm);
    } else {
        /* Use cached entry */
        *tm = date_entries[current_index].local;
    }
}

/*
 * date_current_gmtime()
 */
void date_current_gmtime(struct tm *tm)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        time_t t = ft_time();
        util_gmtime(&t, tm);
    } else {
        /* Use cached entry */
        *tm = date_entries[current_index].gmt;
    }
}

/*
 * date_current_local_prexplodedtime()
 */
void date_current_local_prexplodedtime(PRExplodedTime *local)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, local);
    } else {
        /* Use cached entry */
        *local = date_entries[current_index].local_prexplodedtime;
    }
}

/*
 * date_current_gmt_prexplodedtime()
 */
void date_current_gmt_prexplodedtime(PRExplodedTime *gmt)
{
    int current_index = date_current_index;
    if (current_index == -1) {
        /* No cached entry available */
        PR_ExplodeTime(PR_Now(), PR_GMTParameters, gmt);
    } else {
        /* Use cached entry */
        *gmt = date_entries[current_index].gmt_prexplodedtime;
    }
}

/*
 * date_current_formatted()
 */
int date_current_formatted(date_format_t *format, char *buffer, int size)
{
    int len = 0;

    PR_ASSERT(format->index >= 0 && format->index < date_num_formats);

    PR_ASSERT(size > 0);
    if (size < 1)
        return 0; /* We need room for at least a nul */

    int current_index;
    do {
        current_index = date_current_index;

        if (current_index == -1) {
            /* Create a temporary buffer if caller's is too small */
            char *p = buffer;
            if (size < format->size)
                p = (char *)MALLOC(format->size);

            len = date_format_time(ft_time(), format, p, size);

            /* Move data from the temporary buffer into the caller's */
            if (p != buffer) {
                if (len >= size)
                    len = size - 1;
                memcpy(buffer, p, len);
                FREE(p);
            }

            break;
        }

        date_formatted_entry_t *entry = &date_entries[current_index].formatted_entries[format->index];

        len = size - 1;
        if (len > entry->len)
            len = entry->len;
        if (len < 0)
            len = 0;
        memcpy(buffer, entry->buffer, len);
        buffer[len] = '\0';
    } while (current_index != date_current_index);

    return len;
}

/*
 * date_time_to_prtime()
 */
static PRTime date_time_to_prtime(time_t t)
{
    /* XXX We assume the time_t and PRTime epochs are the same */
    PRTime prtime;
    LL_MUL(prtime, t, MICROSECONDS_PER_SECOND);
    return prtime;
}

/*
 * date_prtime_to_time()
 */
static time_t date_prtime_to_time(PRTime prtime)
{
    /* XXX We assume the time_t and PRTime epochs are the same */
    LL_DIV(prtime, prtime, MICROSECONDS_PER_SECOND);
    return (time_t)prtime;
}

/*
 * date_format_time()
 */
int date_format_time(time_t t, date_format_t *format, char *buffer, int size)
{
    struct tm tm;
    PRExplodedTime prexplodedtime;
    int len;

    PR_ASSERT(size > 0);
    if (size < 1)
        return 0; /* We need room for at least a nul */

    /* Create a temporary buffer if caller's is too small */
    char *p = buffer;
    if (size < format->size)
        p = (char *)MALLOC(format->size);

    switch (format->arg) {
    case DATE_TIME:
        len = format->fn.t(t, p, format->size, format->context);
        break;
    case DATE_LOCAL:
        util_localtime(&t, &tm);
        len = format->fn.local(&tm, p, format->size, format->context);
        break;
    case DATE_GMT:
        util_gmtime(&t, &tm);
        len = format->fn.gmt(&tm, p, format->size, format->context);
        break;
    case DATE_PRTIME:
        len = format->fn.prtime(date_time_to_prtime(t), p, format->size, format->context);
        break;
    case DATE_LOCAL_PREXPLODEDTIME:
        PR_ExplodeTime(date_time_to_prtime(t), PR_LocalTimeParameters, &prexplodedtime);
        len = format->fn.local_prexplodedtime(&prexplodedtime, p, format->size, format->context);
        break;
    case DATE_GMT_PREXPLODEDTIME:
        PR_ExplodeTime(date_time_to_prtime(t), PR_GMTParameters, &prexplodedtime);
        len = format->fn.gmt_prexplodedtime(&prexplodedtime, p, format->size, format->context);
        break;
    default:
        PR_ASSERT(0);
        len = 0;
        break;
    }

    /* Move data from the temporary buffer into the caller's */
    if (p != buffer) {
        if (len >= size)
            len = size - 1;
        memcpy(buffer, p, len);
        FREE(p);
    }

    buffer[len] = '\0';

    return len;
}

/*
 * date_format_prtime()
 */
int date_format_prtime(PRTime prtime, date_format_t *format, char *buffer, int size)
{
    time_t t;
    struct tm tm;
    PRExplodedTime prexplodedtime;
    int len;

    PR_ASSERT(size > 0);
    if (size < 1)
        return 0; /* We need room for at least a nul */

    /* Create a temporary buffer if caller's is too small */
    char *p = buffer;
    if (size < format->size)
        p = (char *)MALLOC(format->size);

    switch (format->arg) {
    case DATE_TIME:
        t = date_prtime_to_time(prtime);
        len = format->fn.t(t, p, format->size, format->context);
        break;
    case DATE_LOCAL:
        t = date_prtime_to_time(prtime);
        util_localtime(&t, &tm);
        len = format->fn.local(&tm, p, format->size, format->context);
        break;
    case DATE_GMT:
        t = date_prtime_to_time(prtime);
        util_gmtime(&t, &tm);
        len = format->fn.gmt(&tm, p, format->size, format->context);
        break;
    case DATE_PRTIME:
        len = format->fn.prtime(prtime, p, format->size, format->context);
        break;
    case DATE_LOCAL_PREXPLODEDTIME:
        PR_ExplodeTime(prtime, PR_LocalTimeParameters, &prexplodedtime);
        len = format->fn.local_prexplodedtime(&prexplodedtime, p, format->size, format->context);
        break;
    case DATE_GMT_PREXPLODEDTIME:
        PR_ExplodeTime(prtime, PR_GMTParameters, &prexplodedtime);
        len = format->fn.gmt_prexplodedtime(&prexplodedtime, p, format->size, format->context);
        break;
    default:
        PR_ASSERT(0);
        len = 0;
        break;
    }

    /* Move data from the temporary buffer into the caller's */
    if (p != buffer) {
        if (len >= size)
            len = size - 1;
        memcpy(buffer, p, len);
        FREE(p);
    }

    buffer[len] = '\0';

    return len;
}

/*
 * date_register_formatter()
 */
static date_format_t *date_register_formatter(date_formatter_arg_t arg, date_formatter_t fn, size_t size, void *context)
{
    /* Can't register new formats after we've been initialized */
    if (date_initialized)
        return NULL;

    date_format_t *format = (date_format_t *)malloc(sizeof(date_format_t));
    if (!format)
        return NULL;

    format->next = date_formats;
    format->arg = arg;
    format->fn = fn;
    format->size = size;
    format->context = context;
    format->index = date_num_formats;

    date_formats = format;
    date_num_formats++;

    return format;
}

date_format_t *date_register_time_formatter(date_time_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.t = fn;
    return date_register_formatter(DATE_TIME, u, size, context);
}

date_format_t *date_register_local_formatter(date_local_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.local = fn;
    return date_register_formatter(DATE_LOCAL, u, size, context);
}

date_format_t *date_register_gmt_formatter(date_gmt_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.gmt = fn;
    return date_register_formatter(DATE_GMT, u, size, context);
}

date_format_t *date_register_prtime_formatter(date_prtime_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.prtime = fn;
    return date_register_formatter(DATE_PRTIME, u, size, context);
}

date_format_t *date_register_local_prexplodedtime_formatter(date_local_prexplodedtime_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.local_prexplodedtime = fn;
    return date_register_formatter(DATE_LOCAL_PREXPLODEDTIME, u, size, context);
}

date_format_t *date_register_gmt_prexplodedtime_formatter(date_gmt_prexplodedtime_formatter_t fn, size_t size, void *context)
{
    date_formatter_t u;
    u.gmt_prexplodedtime = fn;
    return date_register_formatter(DATE_GMT_PREXPLODEDTIME, u, size, context);
}

/*
 * date_formatter_local_strftime()
 */
int date_formatter_local_strftime(struct tm *local, char *buffer, size_t size, void *fmt)
{
    return util_strftime(buffer, (const char *)fmt, local);
}

/*
 * date_formatter_gmt_strftime()
 */
int date_formatter_gmt_strftime(struct tm *gmt, char *buffer, size_t size, void *fmt)
{
    return util_strftime(buffer, (const char *)fmt, gmt);
}

/*
 * date_formatter_locale()
 */
static int date_formatter_locale(struct tm *local, char *buffer, size_t size, void *context)
{
    return strftime(buffer, size, "%c", local);
}

/*
 * date_call_formatter
 */
static int date_call_formatter(date_format_t *format, date_entry_t *entry, char *buffer)
{
    switch (format->arg) {
    case DATE_TIME:
        return format->fn.t(entry->t, buffer, format->size, format->context);
    case DATE_LOCAL:
        return format->fn.local(&entry->local, buffer, format->size, format->context);
    case DATE_GMT:
        return format->fn.gmt(&entry->gmt, buffer, format->size, format->context);
    case DATE_PRTIME:
        return format->fn.prtime(entry->prtime, buffer, format->size, format->context);
    case DATE_LOCAL_PREXPLODEDTIME:
        return format->fn.local_prexplodedtime(&entry->local_prexplodedtime, buffer, format->size, format->context);
    case DATE_GMT_PREXPLODEDTIME:
        return format->fn.gmt_prexplodedtime(&entry->gmt_prexplodedtime, buffer, format->size, format->context);
    default:
        PR_ASSERT(0);
    }

    /* Error */
    buffer[0] = '\0';
    return 0;
}

/*
 * date_update_entry()
 */
static void date_update_entry(date_entry_t *entry, int offset = 0)
{
    /* Get the time it will be in offset seconds */
    entry->t = ft_time() + offset;
    entry->prtime = PR_Now();
    LL_ADD(entry->prtime, entry->prtime, offset * MICROSECONDS_PER_SECOND);

    /* Explode time */
    util_localtime(&entry->t, &entry->local);
    util_gmtime(&entry->t, &entry->gmt);
    PR_ExplodeTime(entry->prtime, PR_LocalTimeParameters, &entry->local_prexplodedtime);
    PR_ExplodeTime(entry->prtime, PR_GMTParameters, &entry->gmt_prexplodedtime);

    /* Format timestamp strings */
    date_format_t *format = date_formats;
    while (format) {
        char *buffer = entry->formatted_entries[format->index].buffer;
        int len = date_call_formatter(format, entry, buffer);
        entry->formatted_entries[format->index].len = len;

        PR_ASSERT(len < format->size);
        PR_ASSERT(strlen(buffer) == len);

        format = format->next;
    }
}

/*
 * date_callback()
 */
PR_BEGIN_EXTERN_C
static void date_callback(void *context)
{
    /* Tell everybody about the timestamps we formatted a second ago */
    int current_index = (date_current_index + 1) % NUM_DATE_ENTRIES;
    date_current_index = current_index;

    /* Format timestamps for the time one second from now */
    int next_index = (current_index + 1) % NUM_DATE_ENTRIES;
    date_entry_t *entry = &date_entries[next_index];
    date_update_entry(entry, 1);
}
PR_END_EXTERN_C

/*
 * date_init()
 */
int date_init(void)
{
    /* Initialization can occur only once */
    if (date_initialized)
        return -1;
    date_initialized = 1;

#ifdef DEBUG
    /* XXX We expect the time_t and PRTime epochs to both be January 1, 1970 */
    time_t t = 1;
    struct tm gmt;
    util_gmtime(&t, &gmt);
    PR_ASSERT(gmt.tm_year == 70);
#endif

    /* Allocate space for the formatted timestamps */
    for (int i = 0; i < NUM_DATE_ENTRIES; i++) {
        date_entries[i].formatted_entries = (date_formatted_entry_t *)calloc(date_num_formats, sizeof(date_formatted_entry_t));
        date_format_t *format = date_formats;
        while (format) {
            date_entries[i].formatted_entries[format->index].buffer = (char*)malloc(format->size);
            format = format->next;
        }
    }

    /* Get the current timestamps (we assume no other threads are active) */
    date_update_entry(&date_entries[0]);
    date_update_entry(&date_entries[1]);
    date_current_index = 0;

    /* Register our once-a-second timestamp callback function */
    if (ft_register_cb(date_callback, NULL) == -1)
        return -1;

    return 0;
}
