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

/**
 * PROPRIETARY/CONFIDENTIAL. Use of this product is subject to
 * license terms. Copyright © 2001 Sun Microsystems, Inc.
 * Some preexisting portions Copyright © 2001 Netscape Communications Corp.
 * All rights reserved.
 */
/*
 * util.h: Proxy utility functions
 *
 *
 *
 * Ari Luotonen
 * Copyright (c) 1995, 1996 Netscape Communcations Corporation
 *
 */

#ifndef PROXY_UTIL_H
#define PROXY_UTIL_H

#include "base/pblock.h"
#include "base/pool.h"
#include <time.h>

#ifdef MCC_PROXY
#include "libproxy/putil.h"	/* private parts of this header file */
#endif


/*
 * Max hostname length after it's sterilized -- that is, not allowing
 * longer than this, plus not allowing any non-alpha non-punct chars.
 *
 */
#define MAX_STERILIZED_HLEN	64


#define UTIL_LOCK_SUFFIX	".lck"

/* strALLOC[copy|cat] */
#define STRALLOCCOPY(s,d)   system_StrAllocCopy(&s,d)
#define STRALLOCCAT(s,d)   system_StrAllocCat(&s,d)

char *system_StrAllocCopy(char **dest, const char *src);
char *system_StrAllocCat(char **dest, const char *src); 

/* blockALLOC[copy|cat] */
#define BLOCKALLOCCOPY(dest, src, src_length) system_BlockAllocCopy((char**)&(dest), src, src_length)
#define BLOCKALLOCCAT(dest, dest_length, src, src_length)  system_BlockAllocCat(&(dest), dest_length, src, src_length)


char *system_BlockAllocCopy(char **destination, const char *source, size_t length);
char *system_BlockAllocCat(char **destination, size_t destination_length, const char *source, size_t source_length);
/*
 * Time cache.
 *
 * util_time_fresh() returns the current time, and caches it so that it
 *		     can be used for next call to util_time_last() or
 *		     util_time_req_start().
 *
 * util_time_last()  returns whatever was the latest cached time; times
 *		     get cached by calls to util_time_fresh(),
 *		     util_time_req_start() and util_time_req_finish().
 *
 * util_time_req_start()  returns the first time after the request was received.
 *
 * util_time_req_finish() returns the time when the cache write, or, if
 *		     no cache file was written, the time when the request
 *		     service was finished.
 *
 * util_time_set_req_finish() can be called to set the time (estimate) of the
 *		     time that the request gets finished (in practice this function
 *		     is called with the last modification time of the written cache
 *		     file -- the fstat() on that file is done anyway, so we avoid
 *		     one more time() call that way).
 *
 * util_time_req_reset()  resets the times in util_time_latest(),
 *		     util_time_req_start() and util_time_req_finish()
 *		     caches, so that next request gets them refreshed.
 *
 * NB: These are not thread-safe, so when we move to threading these'll
 *     have to be made thread safe.
 */
time_t util_time_fresh(void);
time_t util_time_last(void);
time_t util_time_req_start(void);
time_t util_time_req_finish(void);
void   util_time_set_req_finish(time_t t);
void   util_time_req_reset(void);

NSPR_BEGIN_EXTERN_C

/*
 * Returns the given localtime in GMT, or if (time_t)0 given
 * returns current time in GMT.
 *
 */
NSAPI_PUBLIC time_t util_make_gmt(time_t t);


/*
 * Returns the given GMT time in localtime.
 */
NSAPI_PUBLIC time_t util_make_local(time_t t);


/*
 * Returns current time in GMT.
 *
 */
NSAPI_PUBLIC time_t util_get_current_gmt(void);


/*
 * Format the time to format that suits access logs.
 * If tt is (time_t)0, will get the current time.
 */
NSAPI_PUBLIC char *make_log_time(time_t tt);


/*
 * Creates a string representing the time in the common logfile format.
 * Turns the time to GMT.
 */
NSAPI_PUBLIC char *util_make_log_time(time_t tt);


/*
 * Parses the HTTP time string into time_t.
 *
 */
NSAPI_PUBLIC time_t util_parse_http_time(char *date_string);


/*
 * As pblock_nvinsert(), but take an integer or a long
 * instead of a string value.
 *
 */
NSAPI_PUBLIC pb_param *pblock_nlinsert(char *name, long value, pblock *pb);


/*
 * Finds a long int value from pblock.
 *
 */
NSAPI_PUBLIC long pblock_findlong(char *name, pblock *pb);


/*
 * Replace the name of the pblock parameter, retaining the value.
 *
 */
NSAPI_PUBLIC void pblock_replace_name(char *oname, char *nname, pblock *pb);


/*
 * Parse a comma-separated list of strings, such as:
 *
 *	"one,two,three"
 *
 * and place them to an array of strings, terminated by a NULL pointer,
 * e.g.
 *	a[0] == "one"
 *	a[1] == "two"
 *	a[2] == "three"
 *	a[3] == NULL
 */
char ** util_string_array_parse(char *str);


/*
 * Free an array of strings, including the strings.
 *
 */
void util_string_array_free(char **a);


/*
 * Match a header line's name against the given shell regexp.
 * The header name in the string MUST BE in all-lower-case, except
 * the first character can be either lower or upper-case letter.
 *
 * The header name need's not be there alone, i.e, the colon and
 * the header content may be present.
 *
 */
int wildpat_header_match(char *hdr, char *regexp);


/*
 * As strdup(), but also turns all characters into lower-case.
 *
 */
char *util_lowcase_strdup(char *str);


/*
 * Returns 0 or 1 depending on whether or not URL references a
 * Fully Qualified Domain Name. Useful, because the proxy doesn't
 * cache non-FQDN's.
 */
 
NSAPI_PUBLIC int util_url_has_FQDN(char * url);


/*
 * Turns hostname in URL to all-lower-case, and removes redundant
 * port numbers, i.e. 80 for http, 70 for gopher and 21 for ftp.
 * Modifies its parameter string directly.
 *
 * Also turns the protocol specifier to all lower-case.
 */
NSAPI_PUBLIC void util_url_fix_hostname(char * url);


/*
 * Compare two URLs.
 * Return values as for strcmp().
 *
 */
NSAPI_PUBLIC int util_url_cmp(char *s1, char *s2);


/*
 * Check that the URL looks ok.
 *
 *
 */
NSAPI_PUBLIC int util_uri_check(char *uri);


/*
 * Returns true iff the process with pid exists.
 *
 *
 */
NSAPI_PUBLIC int util_does_process_exist(int pid);


/*
 * Move a file.
 * Preservs permissions, and creation and last access times.
 *
 * First try rename(); if that fails because files are on different
 * filesystems, does it by copying.
 *
 *
 */
NSAPI_PUBLIC int util_move_file(char *src, char *dst);


/*
 * Move a directory.
 * Preservs permissions, and creation and last access times.
 *
 * First try rename(); if that fails because directories are on
 * different filesystems, does it by copying.
 *
 *
 */
NSAPI_PUBLIC int util_move_dir(char *src, char *dst);


/*
 * Looks at a hostname and makes sure that it's no longer than
 * MAX_STERILIZED_HLEN (64) and that it only has alphanumeric and
 * punctuation characters.
 *
 * Truncates longer hostnames, and at the first bad character.
 *
 * Returns 1 if was ok as passed in.
 * Returns 0 if intervention was done (hostname was bad).
 *
 */

/* The following functions are internal. Do not use!! */

NSAPI_PUBLIC int util_sterilize_hostname(char *host);


/*
 * Parses a config file on/off setting; is not case-sensitive;
 * understands the following values to be true:
 *
 *	- all non-zero positive integers
 *	- "on"
 *	- "yes"
 *	- "true"
 *	- "enabled"
 *
 * Everything else returns false.
 *
 */
int util_get_onoff(const char *str);


/*
 * Like above, but returns -1 if the value is not strictly
 * in the allowed category, i.e. the above true values,
 * or the corresponding false values below:
 *
 *	- "0" (zero)
 *	- "off"
 *	- "no"
 *	- "false"
 *	- "disabled"
 *
 */
int util_get_onoff_fuzzy(const char *str);

/*
 * duplicates list of strings using pool_malloc()
 * note that strlist is a list of char pointers terminated by a NULL
 * pointer at the end
 */
NSAPI_PUBLIC char **util_strlist_pool_dup(char **strlist, pool_handle_t *pool);
/*
 * duplicates list of strings using PERM_MALLOC
 * note that strlist is a list of char pointers terminated by a NULL
 * pointer at the end
 * Note that the caller is responsible for freeing the memory
 * using util_strlist_free()
 */
NSAPI_PUBLIC char **util_strlist_dup(char **strlist);

/*
 * frees the memory allocated through util_strlist_dup()
 */
NSAPI_PUBLIC void util_strlist_free(char **strlist);

NSAPI_PUBLIC int get_time_difference(Request *rq, const char *name1, const char *name2);
NSAPI_PUBLIC int record_times(int res, Request *rq);

NSPR_END_EXTERN_C

#endif

