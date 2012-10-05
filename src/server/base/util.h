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

#ifndef BASE_UTIL_H
#define BASE_UTIL_H

#ifndef NOINTNSAPI
#define INTNSAPI
#endif /* !NOINTNSAPI */

/*
 * util.h: A hodge podge of utility functions and standard functions which 
 *         are unavailable on certain systems
 * 
 * Rob McCool
 */

/* Needed for various reentrant functions */
#define DEF_CTIMEBUF 26
#define DEF_ERRBUF 256
#define DEF_PWBUF 1024

#ifndef BASE_BUFFER_H
#include "buffer.h"    /* filebuf for getline */
#endif /* !BASE_BUFFER_H */

/*
 * UTIL_ITOA_SIZE is the minimum size for buffers passed to util_itoa().
 */
#define UTIL_ITOA_SIZE 12

/*
 * UTIL_I64TOA_SIZE is the minimum size for buffers passed to util_i64toa().
 */
#define UTIL_I64TOA_SIZE 21

/* --- Begin common function prototypes --- */

#ifdef INTNSAPI

NSPR_BEGIN_EXTERN_C

NSAPI_PUBLIC
int INTutil_init_PRNetAddr(PRNetAddr * naddr, char * ipstr, int iplen, int type);

NSAPI_PUBLIC
int INTutil_getline(filebuffer *buf, int lineno, int maxlen, char *l);

NSAPI_PUBLIC char **INTutil_env_create(char **env, int n, int *pos);

NSAPI_PUBLIC char *INTutil_env_str(const char *name, const char *value);

NSAPI_PUBLIC void INTutil_env_replace(char **env, const char *name, const char *value);

NSAPI_PUBLIC void INTutil_env_free(char **env);

NSAPI_PUBLIC char **INTutil_env_copy(char **src, char **dst);

NSAPI_PUBLIC char *INTutil_env_find(char **env, const char *name);

NSAPI_PUBLIC char **util_argv_parse(const char *cmdline);

NSAPI_PUBLIC char *INTutil_hostname(void);

NSAPI_PUBLIC int INTutil_chdir2path(char *path);

NSAPI_PUBLIC int INTutil_chdir(const char *path);

NSAPI_PUBLIC char *INTutil_getcwd(void);

NSAPI_PUBLIC int INTutil_is_mozilla(char *ua, char *major, char *minor);

NSAPI_PUBLIC int INTutil_is_url(const char *url);

NSAPI_PUBLIC int INTutil_mstr2num(const char *s);

NSAPI_PUBLIC int INTutil_later_than(const struct tm *lms, const char *ims);

NSAPI_PUBLIC int INTutil_time_equal(const struct tm *lms, const char *ims);

NSAPI_PUBLIC int INTutil_str_time_equal(const char *t1, const char *t2);

NSAPI_PUBLIC int INTutil_uri_is_evil(const char *t);

NSAPI_PUBLIC int INTutil_uri_is_evil_internal(const char *t, int, int);

NSAPI_PUBLIC void INTutil_uri_parse(char *uri);

#ifdef XP_WIN32
NSAPI_PUBLIC int INTutil_uri_unescape_and_normalize(pool_handle_t *pool, char *s, char *unnormalized);
#endif /* XP_WIN32 */

NSAPI_PUBLIC void INTutil_uri_normalize_slashes(char *s);

NSAPI_PUBLIC void INTutil_uri_unescape		(char *s);

NSAPI_PUBLIC int INTutil_uri_unescape_strict		(char *s);

NSAPI_PUBLIC int  INTutil_uri_unescape_plus (const char *src, char *trg, int len);

NSAPI_PUBLIC char *INTutil_uri_escape(char *d, const char *s);

NSAPI_PUBLIC char *INTutil_uri_strip_params(char *uri);

NSAPI_PUBLIC char* util_canonicalize_uri(pool_handle_t *pool, const char *uri, int len, int *pcanonlen);

NSAPI_PUBLIC char* util_canonicalize_redirect(pool_handle_t *pool, const char *baseUri, const char *newUri);

NSAPI_PUBLIC char *INTutil_url_escape(char *d, const char *s);

NSAPI_PUBLIC char *INTutil_sh_escape(char *s);

NSAPI_PUBLIC int INTutil_mime_separator(char *sep);

NSAPI_PUBLIC int INTutil_itoa(int i, char *a);

NSAPI_PUBLIC int INTutil_i64toa(PRInt64 i, char *a);

NSAPI_PUBLIC
int INTutil_vsprintf(char *s, register const char *fmt, va_list args);

NSAPI_PUBLIC int INTutil_sprintf(char *s, const char *fmt, ...);

NSAPI_PUBLIC int INTutil_vsnprintf(char *s, int n, register const char *fmt, 
                                  va_list args);

NSAPI_PUBLIC int INTutil_snprintf(char *s, int n, const char *fmt, ...);

NSAPI_PUBLIC int util_strlftime(char *dst, size_t dstsize, const char *format, const struct tm *t);

NSAPI_PUBLIC int INTutil_strftime(char *s, const char *format, const struct tm *t);

NSAPI_PUBLIC char *INTutil_strtok(char *s1, const char *s2, char **lasts);

NSAPI_PUBLIC struct tm *INTutil_localtime(const time_t *clock, struct tm *res);

NSAPI_PUBLIC char *INTutil_ctime(const time_t *clock, char *buf, int buflen);

NSAPI_PUBLIC char *INTutil_strerror(int errnum, char *msg, int buflen);

NSAPI_PUBLIC struct tm *INTutil_gmtime(const time_t *clock, struct tm *res);

NSAPI_PUBLIC char *INTutil_asctime(const struct tm *tm,char *buf, int buflen);

NSAPI_PUBLIC char *INTutil_cookie_find(char *cookie, const char *name);

NSAPI_PUBLIC char *INTutil_cookie_next(char *cookie, char **name, char **value);

NSAPI_PUBLIC char *INTutil_cookie_next_av_pair(char *cookie, char **name, char **value);

NSAPI_PUBLIC void INTutil_random(void *buf, size_t sz);

NSAPI_PUBLIC PRBool INTutil_format_http_version(const char *v, int *protv_num, char *buffer, int size);

NSAPI_PUBLIC int INTutil_getboolean(const char *v, int def);
NSAPI_PUBLIC PRIntervalTime INTutil_getinterval(const char *v, PRIntervalTime def);

#ifdef NEED_STRCASECMP
NSAPI_PUBLIC int INTutil_strcasecmp(const char *one, const char *two);
#endif /* NEED_STRCASECMP */

#ifdef NEED_STRNCASECMP
NSAPI_PUBLIC int INTutil_strncasecmp(const char *one, const char *two, int n);
#endif /* NEED_STRNCASECMP */

NSAPI_PUBLIC char *INTutil_strcasestr(char *str, const char *substr);

NSAPI_PUBLIC size_t util_strlcpy(char *dst, const char *src, size_t dstsize);

NSAPI_PUBLIC size_t util_strlcat(char *dst, const char *src, size_t dstsize);

NSAPI_PUBLIC char *INTutil_uuencode(const char *src, int len);

NSAPI_PUBLIC char *INTutil_uudecode(const char *src);

NSAPI_PUBLIC char *util_strlower(char *s);

NSAPI_PUBLIC char *util_decrement_string(char *s);

NSAPI_PUBLIC PRInt64 util_atoi64(const char *a);

NSAPI_PUBLIC char *util_html_escape(const char *s);

NSAPI_PUBLIC int util_qtoi(const char *q, const char **p);

/* --- End common function prototypes --- */

/* --- Begin Unix-only function prototypes --- */

#ifdef XP_UNIX

NSAPI_PUBLIC int INTutil_can_exec(struct stat *finfo, uid_t uid, gid_t gid);

NSAPI_PUBLIC
struct passwd *INTutil_getpwnam(const char *name, struct passwd *result,
                               char *buffer,  int buflen);

NSAPI_PUBLIC
struct passwd *INTutil_getpwuid(uid_t uid, struct passwd *result,
                                char *buffer, int buflen);

NSAPI_PUBLIC pid_t INTutil_waitpid(pid_t pid, int *statptr, int options);

#endif /* XP_UNIX */

/* --- End Unix-only function prototypes --- */

/* --- Begin Windows-only function prototypes --- */

#ifdef XP_WIN32

NSAPI_PUBLIC
VOID INTutil_delete_directory(char *FileName, BOOL delete_directory);

#endif /* XP_WIN32 */

/* --- End Windows-only function prototypes --- */

NSPR_END_EXTERN_C

#ifdef __cplusplus

NSAPI_PUBLIC char *util_host_port_suffix(char *h);

NSAPI_PUBLIC const char *util_host_port_suffix(const char *h);

#endif

#define util_init_PRNetAddr INTutil_init_PRNetAddr
#define util_getline INTutil_getline
#define util_env_create INTutil_env_create
#define util_env_str INTutil_env_str
#define util_env_replace INTutil_env_replace
#define util_env_free INTutil_env_free
#define util_env_copy INTutil_env_copy
#define util_env_find INTutil_env_find
#define util_hostname INTutil_hostname
#define util_chdir2path INTutil_chdir2path
#define util_chdir INTutil_chdir
#define util_getcwd INTutil_getcwd
#define util_is_mozilla INTutil_is_mozilla
#define util_is_url INTutil_is_url
#define util_mstr2num INTutil_mstr2num
#define util_later_than INTutil_later_than
#define util_time_equal INTutil_time_equal
#define util_str_time_equal INTutil_str_time_equal
#define util_uri_is_evil INTutil_uri_is_evil
#define util_uri_is_evil_internal INTutil_uri_is_evil_internal
#define util_uri_parse INTutil_uri_parse
#ifdef XP_WIN32
#define util_uri_unescape_and_normalize INTutil_uri_unescape_and_normalize
#endif /* XP_WIN32 */
#define util_uri_normalize_slashes INTutil_uri_normalize_slashes
#define util_uri_unescape	   INTutil_uri_unescape
#define util_uri_unescape_strict	   INTutil_uri_unescape_strict
#define util_uri_unescape_plus INTutil_uri_unescape_plus

#define util_uri_escape INTutil_uri_escape
#define util_uri_strip_params INTutil_uri_strip_params
#define util_url_escape INTutil_url_escape
#define util_sh_escape INTutil_sh_escape
#define util_mime_separator INTutil_mime_separator
#define util_itoa INTutil_itoa
#define util_i64toa INTutil_i64toa
#define util_vsprintf INTutil_vsprintf
#define util_sprintf INTutil_sprintf
#define util_vsnprintf INTutil_vsnprintf
#define util_snprintf INTutil_snprintf
#define util_strftime INTutil_strftime
#define util_strcasecmp INTutil_strcasecmp
#define util_strncasecmp INTutil_strncasecmp
#define util_strcasestr INTutil_strcasestr
#define util_strtok INTutil_strtok
#define util_localtime INTutil_localtime
#define util_ctime INTutil_ctime
#define util_strerror INTutil_strerror
#define util_gmtime INTutil_gmtime
#define util_asctime INTutil_asctime
#define util_uuencode INTutil_uuencode
#define util_uudecode INTutil_uudecode

#ifdef XP_UNIX
#define util_can_exec INTutil_can_exec
#define util_getpwnam INTutil_getpwnam
#define util_getpwuid INTutil_getpwuid
#define util_waitpid INTutil_waitpid
#endif /* XP_UNIX */

#ifdef XP_WIN32
#define util_delete_directory INTutil_delete_directory
#endif /* XP_WIN32 */

#ifdef NEED_STRCASECMP
#define util_strcasecmp INTutil_strcasecmp
#define strcasecmp INTutil_strcasecmp
#endif /* NEED_STRCASECMP */

#ifdef NEED_STRINGS_H /* usually for strcasecmp */
#include <strings.h>
#endif

#ifdef NEED_STRNCASECMP
#define util_strncasecmp INTutil_strncasecmp
#define strncasecmp INTutil_strncasecmp
#endif /* NEED_STRNCASECMP */

#define util_cookie_find INTutil_cookie_find
#define util_cookie_next INTutil_cookie_next
#define util_cookie_next_av_pair INTutil_cookie_next_av_pair

#define util_random INTutil_random
#define util_format_http_version INTutil_format_http_version
#define util_getboolean INTutil_getboolean
#define util_getinterval INTutil_getinterval

#ifdef XP_WIN32
void set_fullpathname(PRBool b);
#endif /* XP_WIN32 */
#endif /* INTNSAPI */

#endif /* !BASE_UTIL_H */


