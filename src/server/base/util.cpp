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
 * util.c: A hodge podge of utility functions and standard functions which 
 *         are unavailable on certain systems
 * 
 * Rob McCool
 */

#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "prthread.h"
#endif /* XP_UNIX */

#include "nspr.h"
#include "base/util.h"
#include "base/url64.h"
#include "base/dbtbase.h"
#include "base/ereport.h"
#include "support/stringvalue.h"
#include "NsprWrap/NsprError.h"
#include "frame/conf.h"
#include "nscperror.h"


static const int MAX_SECONDS = 21600; // fits in PRIntervalTime on all systems
static const int MAX_SECONDS_DIGITS = 5; // 5 decimal digits in 21600
static const int MAX_INTERVAL = PR_SecondsToInterval(MAX_SECONDS);
static const int TICKS_PER_SECOND = PR_SecondsToInterval(1);
static const int TICKS_PER_MILLISECOND = PR_MillisecondsToInterval(1);


/* ----------------------------- util_init_PRNetAddr ----------------------------- */
/* Used to ensure IPv6 is handled correctly */
NSAPI_PUBLIC int util_init_PRNetAddr(PRNetAddr * naddr, char * ipstr, int iplen, int type) {

/* ipstr is assumed to be either a 4 byte IPv4 address or a 16 byte IPv6 address */
    PR_ASSERT((type==PR_AF_INET)||(type==PR_AF_INET6));
    memset((void *)naddr, 0, sizeof(PRNetAddr));
    naddr->ipv6.family = PR_AF_INET6;
    if (AF_INET == type) {
        PR_ConvertIPv4AddrToIPv6(*(PRUint32 *)ipstr, &(naddr->ipv6.ip));
    } else {
        memcpy((void *)&(naddr->ipv6.ip), ipstr, iplen);
    }
    return 0;
}

/* ----------------------------- util_getline ----------------------------- */

#define LF 10
#define CR 13

NSAPI_PUBLIC int util_getline(filebuf_t *buf, int lineno, int maxlen, char *l) {
    int i, x;

    x = 0;
    while(1) {
        switch(i = filebuf_getc(buf)) {
          case IO_EOF:
            l[x] = '\0';
            return 1;
          case LF:
            if(x && (l[x-1] == '\\')) {
                --x;
                continue;
            }
            l[x] = '\0';
            return 0;
          case IO_ERROR:
            util_snprintf(l, maxlen, system_errmsg());
            return -1;
          case CR:
            continue;
          default:
            l[x] = (char) i;
            if(++x == maxlen) {
                NsprError::setErrorf(PR_BUFFER_OVERFLOW_ERROR,
                                     XP_GetAdminStr(DBT_LineXTooLong),
                                     lineno);
                util_snprintf(l, maxlen, system_errmsg());
                return -1;
            }
            break;
        }
    }
}


/* ---------------------------- util_can_exec ----------------------------- */

#ifdef XP_UNIX
NSAPI_PUBLIC int util_can_exec(struct stat *fi, uid_t uid, gid_t gid) 
{
    int ngroups = 0;
    gid_t groupsID[NGROUPS_MAX];

    /* first check if the given uid & gid are sufficient */
    if(!uid)
       return 1;
    if((fi->st_mode & S_IXOTH) || 
       ((gid == fi->st_gid) && (fi->st_mode & S_IXGRP)) ||
       ((uid == fi->st_uid) && (fi->st_mode & S_IXUSR)))
        return 1;

    /* now we check if the user belongs to multiple groups */
    ngroups = getgroups(NGROUPS_MAX, groupsID);
    if (ngroups != -1) {
       for (int i = 0; i < ngroups; i++) {
            if ((fi->st_gid == groupsID[i]) && (fi->st_mode & S_IXGRP))
                   return 1;
       }
   }

   return 0;
}
#endif /* XP_UNIX */


/* --------------------------- util_env_create ---------------------------- */


NSAPI_PUBLIC char **util_env_create(char **env, int n, int *pos)
{
    int x;

    if(!env) {
        *pos = 0;
        return (char **) MALLOC((n + 1)*sizeof(char *));
    }
    else {
        for(x = 0; (env[x]); x++);
        env = (char **) REALLOC(env, (n + x + 1)*(sizeof(char *)));
        *pos = x;
        return env;
    }
}


/* ---------------------------- util_env_free ----------------------------- */


NSAPI_PUBLIC void util_env_free(char **env)
{
    register char **ep = env;

    for(ep = env; *ep; ep++)
        FREE(*ep);
    FREE(env);
}

/* ----------------------------- util_env_str ----------------------------- */


NSAPI_PUBLIC char *util_env_str(const char *name, const char *value) {
    char *t;

    t = (char *) MALLOC(strlen(name)+strlen(value)+2); /* 2: '=' and '\0' */

    sprintf(t, "%s=%s", name, value);

    return t;
}


/* --------------------------- util_env_replace --------------------------- */


NSAPI_PUBLIC void util_env_replace(char **env, const char *name, const char *value)
{
    int x, y, z;
    char *i;

    for(x = 0; env[x]; x++) {
        i = strchr(env[x], '=');
        *i = '\0';
        if(!strcmp(env[x], name)) {
            y = strlen(env[x]);
            z = strlen(value);

            env[x] = (char *) REALLOC(env[x], y + z + 2);
            util_sprintf(&env[x][y], "=%s", value);
            return;
        }
        *i = '=';
    }
}


/* ---------------------------- util_env_find ----------------------------- */


NSAPI_PUBLIC char *util_env_find(char **env, const char *name)
{
    char *i;
    int x, r;

    for(x = 0; env[x]; x++) {
        i = strchr(env[x], '=');
        *i = '\0';
        r = !strcmp(env[x], name);
        *i = '=';
        if(r)
            return i + 1;
    }
    return NULL;
}


/* ---------------------------- util_env_copy ----------------------------- */


NSAPI_PUBLIC char **util_env_copy(char **src, char **dst)
{
    char **src_ptr;
    int src_cnt;
    int index;

    if (!src)
        return NULL;

    for (src_cnt = 0, src_ptr = src; *src_ptr; src_ptr++, src_cnt++);

    if (!src_cnt)
        return NULL;

    dst = util_env_create(dst, src_cnt, &index);

    for (src_ptr = src; *src_ptr; index++, src_ptr++)
        dst[index] = STRDUP(*src_ptr);
    dst[index] = NULL;
    
    return dst;
}


/* --------------------------- util_argv_parse ---------------------------- */


static inline const char *parse_arg(const char *src, char *dst)
{
    while (isspace(*src))
        src++;

    if (!*src)
        return NULL;

    for (char quote = '\0'; *src; src++) {
        if (quote) {
            if (*src == quote) {
                quote = '\0'; // end of quoted span
                continue;
            }
        } else {
            if (*src == '"' || *src == '\'') {
                quote = *src; // beginning of quoted span
                continue;
            } else if (isspace(*src)) {
                break;
            } else if (*src == '\\') {
                if (src[1] == '"' || src[1] == '\'' || isspace(src[1]))
                    src++; // escaped quote or space
            }
        }
        if (dst)
            *dst++ = *src;
    }

    if (dst)
        *dst = '\0';

    return src;
}

static inline int parse_args(const char *args, char **argv)
{
    const char *p;
    int i;

    for (i = 0; p = parse_arg(args, NULL); i++) {
        if (argv) {
            argv[i] = (char *) MALLOC(p - args + 1);
            if (!argv[i])
                break;
            parse_arg(args, argv[i]);
        }
        args = p;
    }

    return i;
}

NSAPI_PUBLIC char **util_argv_parse(const char *cmdline)
{
    int n = parse_args(cmdline, NULL);

    int i;
    char **argv = util_env_create(NULL, n, &i);
    if (argv) {
        parse_args(cmdline, &argv[i]);
        argv[i + n] = NULL;
    }

    return argv;
}


/* ---------------------------- util_hostname ----------------------------- */


/*
 * MOVED TO NET.C TO AVOID INTERDEPENDENCIES
 */


/* --------------------------- util_chdir2path ---------------------------- */


NSAPI_PUBLIC int util_chdir2path(char *path) 
{  
	/* use FILE_PATHSEP to accomodate WIN32 */
    char *t = strrchr(path, FILE_PATHSEP);
    int ret;

    if(!t)
        return -1;

    *t = '\0';
    ret = util_chdir(path);

	/* use FILE_PATHSEP instead of chdir to accomodate WIN32 */
    *t = FILE_PATHSEP;

    return ret;
}


/* ------------------------------ util_chdir ------------------------------ */

NSAPI_PUBLIC int util_chdir(const char *path)
{
    int rv;

#ifdef XP_WIN32
    rv = SetCurrentDirectory(path);
    if (rv == TRUE) {
        rv = 0;
    } else {
        rv = -1;
        NsprError::mapWin32Error();
    }
#else
    rv = chdir(path);
    if (rv == -1)
        NsprError::mapUnixErrno();
#endif

    return rv;
}


/* ----------------------------- util_getcwd ------------------------------ */

NSAPI_PUBLIC char *util_getcwd(void)
{
#ifdef XP_WIN32
    size_t size = MAX_PATH;
#else
    size_t size = PATH_MAX;
#endif

    char *buffer = (char *)MALLOC(size);
    if (!buffer)
        return NULL;

    char *cwd = buffer;

#ifdef XP_WIN32
    if (GetCurrentDirectory(size, cwd) == 0) {
        NsprError::mapWin32Error();
        cwd = NULL;
    }
#else
    cwd = getcwd(buffer, size);
    if (!cwd)
        NsprError::mapUnixErrno();
#endif

    if (cwd != buffer)
        FREE(buffer);

    return cwd;
}


/* --------------------------- util_is_mozilla ---------------------------- */


NSAPI_PUBLIC int util_is_mozilla(char *ua, char *major, char *minor)
{
    if((!ua) || strncasecmp(ua, "Mozilla/", 8))
        return 0;

    /* Major version. I punted on supporting versions like 10.0 */
    if(ua[8] > major[0])
        return 1;
    else if((ua[8] < major[0]) || (ua[9] != '.'))
        return 0;

    /* Minor version. Support version numbers like 0.96 */
    if(ua[10] < minor[0])
        return 0;
    else if((ua[10] > minor[0]) || (!minor[1]))
        return 1;

    if((!isdigit(ua[11])) || (ua[11] < minor[1]))
        return 0;
    else
        return 1;
}


/* ----------------------------- util_is_url ------------------------------ */


#include <ctype.h>     /* isalpha */

NSAPI_PUBLIC int util_is_url(const char *url)
{
    const char *t = url;

    while(*t) {
        if(*t == ':')
            return 1;
        if(!isalpha(*t))
            return 0;
        ++t;
    }
    return 0;
}


/* ---------------------------- util_mstr2num ----------------------------- */

static const int MSTR2NUM_HT_MASK = 0xf;

static const struct {
    unsigned ucmstr; // Uppercase 3 character month string in a machine word
    int mnum; // 0-based month number for this month string
} MSTR2NUM_HT[MSTR2NUM_HT_MASK + 1] = {
    { 'A' << 16 | 'P' << 8 | 'R', 3 },
    { 'S' << 16 | 'E' << 8 | 'P', 8 },
    { 'M' << 16 | 'A' << 8 | 'Y', 4 },
    { 0, -1 },
    { 'M' << 16 | 'A' << 8 | 'R', 2 },
    { 'F' << 16 | 'E' << 8 | 'B', 1 },
    { 0, -1 },
    { 'D' << 16 | 'E' << 8 | 'C', 11 },
    { 'O' << 16 | 'C' << 8 | 'T', 9 },
    { 'J' << 16 | 'U' << 8 | 'N', 5 },
    { 0, -1 },
    { 'A' << 16 | 'U' << 8 | 'G', 7 },
    { 'J' << 16 | 'A' << 8 | 'N', 0 },
    { 'J' << 16 | 'U' << 8 | 'L', 6 },
    { 0, -1 },
    { 'N' << 16 | 'O' << 8 | 'V', 10 }
};

static inline int _mstr2num(const char *s)
{
    const unsigned char *mstr = (const unsigned char *) s;

    /*
     * We compute ucmstr (an uppercase 3 character month string stored in a
     * machine word) and hash (a perfect hash based on the last 2 characters
     * of the 3 character uppercase month string) from the input string s.
     * Note that each character from the input string is masked by 0xdf; in
     * ASCII, this has the effect of converting alphabetic characters to
     * uppercase while 1. not changing any nonalphabetic characters into
     * alphabetic characters and 2. leaving any nul characters unchanged.
     *
     * The hash value is used as an index into the MSTR2NUM_HT[] hash table.
     * If the ucmstr at that index matches our computed ucmstr, the mnum at
     * that index is the 0-based month number corresponding to the input
     * string.
     *
     * Note that we never read past the end of the input string and always
     * return -1 if the input string doesn't begin with a valid 3 character
     * month string.
     */

    unsigned char ucmstr0 = mstr[0] & 0xdf;
    unsigned ucmstr = ucmstr0 << 16;
    if (ucmstr0 != '\0') {
        unsigned char ucmstr1 = mstr[1] & 0xdf;
        ucmstr |= ucmstr1 << 8;
        if (ucmstr1 != '\0') {
            unsigned char ucmstr2 = mstr[2] & 0xdf;
            ucmstr |= ucmstr2;

            unsigned hash = (ucmstr1 >> 2) ^ (ucmstr2 << 1);

            int i = hash & MSTR2NUM_HT_MASK;

            if (MSTR2NUM_HT[i].ucmstr == ucmstr)
                return MSTR2NUM_HT[i].mnum;
        }
    }

    return -1;
}

NSAPI_PUBLIC int util_mstr2num(const char *s)
{
    return _mstr2num(s);
}


/* ------------------------- util_str_time_equal -------------------------- */

/*
 * Function to compare if two time strings are equal
 *
 * Acceptable date formats:
 *     Sun, 06 Nov 1994 08:49:37 GMT  ; RFC 822, updated by RFC 1123
 *     Sunday, 06-Nov-94 08:49:37 GMT ; RFC 850, obsoleted by RFC 1036
 *     Sun Nov  6 08:49:37 1994       ; ANSI C's asctime() format
 *
 * Return 0 if equal, -1 if not equal.
 */

static inline const char * _parse_day_month(const char *p, int& day, int& month)
{
    day = 0;

    if (*p == ',') {
        // Parse day and month: ", 06 Nov", ", 06-Nov"
        p++;
        if (*p == ' ')
            p++;
        while (*p >= '0' && *p <= '9')
            day = day * 10 + (*p++ - '0');
        if (*p == ' ' || *p == '-')
            p++;
        month = _mstr2num(p);
        if (month != -1)
            p += 3;
    } else {
        // Parse month and day: " Nov  6"
        if (*p == ' ')
            p++;
        month = _mstr2num(p);
        if (month != -1)
            p += 3;
        while (*p == ' ')
            p++;
        while (*p >= '0' && *p <= '9')
            day = day * 10 + (*p++ - '0');
    }

    return p;
}

static inline void _parse_year_time(const char *p, int& year, const char * & time)
{
    year = 0;

    if (*p == '-') {
        // Parse year and time: "-94 08:49:37"
        p++;
        while (*p >= '0' && *p <= '9')
            year = year * 10 + (*p++ - '0');
        if (year < 70) {
            year += 2000;
        } else {
            year += 1900;
        }
        if (*p == ' ')
            p++;
        time = p;
    } else {
        // Parse year and time or time and year
        if (*p == ' ')
            p++;
        if (p[0] && p[1] && p[2] == ':') {
            // Parse time and year: "08:49:37 1994"
            time = p;
            p += 3;
            while (*p && *p != ' ')
                p++;
            if (*p == ' ')
                p++;
            while (*p >= '0' && *p <= '9')
                year = year * 10 + (*p++ - '0');
        } else {
            // Parse year and time: "1994 08:49:37"
            while (*p >= '0' && *p <= '9')
                year = year * 10 + (*p++ - '0');
            if (*p == ' ')
                p++;
            time = p;
        }
    }
}

NSAPI_PUBLIC int util_str_time_equal(const char *t1, const char *t2)
{
    // Skip leading whitespace and day of week
    while (isspace(*t1))
        t1++;
    while (isalpha(*t1))
        t1++;
    while (isspace(*t2))
        t2++;
    while (isalpha(*t2))
        t2++;

    // Day and month: ", 06 Nov", ", 06-Nov", or " Nov  6"
    int day1;
    int month1;
    t1 = _parse_day_month(t1, day1, month1);
    int day2;
    int month2;
    t2 = _parse_day_month(t2, day2, month2);
    if (day1 != day2)
        return -1;
    if (month1 != month2)
        return -1;

    // Year and time: " 1994 08:49:37", "-94 08:49:37", or " 08:49:37 1994"
    int year1;
    const char *time1;
    _parse_year_time(t1, year1, time1);
    int year2;
    const char *time2;
    _parse_year_time(t2, year2, time2);
    if (year1 != year2)
        return -1;
    while (*time1 && *time1 != ' ' && *time1 == *time2) {
        time1++;
        time2++;
    }
    if (*time2 && *time2 != ' ')
        return -1;

    return 0;
}


/* --------------------------- util_later_than ---------------------------- */

static int _time_compare(const struct tm *lms, const char *ims)
{
    while (isspace(*ims))
        ims++;
    while (isalpha(*ims))
        ims++;

    int day;
    int month;
    ims = _parse_day_month(ims, day, month);
    if (month == -1)
        return 1;

    int year;
    const char *time;
    _parse_year_time(ims, year, time);

    int rv;

    rv = (lms->tm_year + 1900) - year;
    if (rv)
        return rv;

    rv = lms->tm_mon - month;
    if (rv)
        return rv;

    rv = lms->tm_mday - day;
    if (rv)
        return rv;

    const char *p = time;

    int hour = 0;
    while (*p >= '0' && *p <= '9')
        hour = hour * 10 + (*p++ - '0');
    if (*p == ':')
        p++;

    rv = lms->tm_hour - hour;
    if (rv)
        return rv;

    int minutes = 0;
    while (*p >= '0' && *p <= '9')
        minutes = minutes * 10 + (*p++ - '0');
    if (*p == ':')
        p++;

    rv = lms->tm_min - minutes;
    if (rv)
        return rv;

    int seconds = 0;
    while (*p >= '0' && *p <= '9')
        seconds = seconds * 10 + (*p++ - '0');
    if (*p == ':')
        p++;

    rv = lms->tm_sec - seconds;
    if (rv)
        return rv;

    return 0;
}

NSAPI_PUBLIC int util_later_than(const struct tm *lms, const char *ims)
{
    /*
     * Returns 0 if lms later than ims
     *         0 if ims is malformed
     *         1 if ims later than lms
     *         1 if equal
     */

    return _time_compare(lms, ims) <= 0;
}

NSAPI_PUBLIC int util_time_equal(const struct tm *lms, const char *ims)
{
    return _time_compare(lms, ims) == 0;
}


/* ---------------------------- util_uri_parse ---------------------------- */

NSAPI_PUBLIC void util_uri_parse(char *uri)  
{
    int spos = 0, tpos = 0;
    int l = strlen(uri);

    while(uri[spos])  {
        if(uri[spos] == '/')  {
            if((spos != l) && (uri[spos+1] == '.'))  {
                if(uri[spos+2] == '/')
                    spos += 2;
                else
                    if((spos <= (l-3)) && 
                       (uri[spos+2] == '.') && (uri[spos+3] == '/'))  {
                        spos += 3;
                        while((tpos > 0) && (uri[--tpos] != '/'))    
                            uri[tpos] = '\0';
                    }  else
                        uri[tpos++] = uri[spos++];
            }  else  {
                if(uri[spos+1] != '/')
                    uri[tpos++] = uri[spos++];
                else 
                    spos++;
            }
        }  else
            uri[tpos++] = uri[spos++];
    }
    uri[tpos] = '\0';
}


/* -------------------------- util_uri_unescape --------------------------- */

NSAPI_PUBLIC void util_uri_unescape(char *s)
{
    char *t, *u;

    for(t = s, u = s; *t; ++t, ++u) {
        if((*t == '%') && t[1] && t[2]) {
            *u = ((t[1] >= 'A' ? ((t[1] & 0xdf) - 'A')+10 : (t[1] - '0'))*16) +
                  (t[2] >= 'A' ? ((t[2] & 0xdf) - 'A')+10 : (t[2] - '0'));
            t += 2;
        }
        else
            if(u != t)
                *u = *t;
    }
    *u = *t;
}

/*
 * Same as util_uri_unescape, but returns success/failure
 */
NSAPI_PUBLIC int util_uri_unescape_strict(char *s)
{
    char *t, *u, t1, t2;
    int rv = 1;

    for(t = s, u = s; *t; ++t, ++u) {
        if (*t == '%') {
            t1 = t[1] & 0xdf; /* [a-f] -> [A-F] */
            if ((t1 < 'A' || t1 > 'F') && (t[1] < '0' || t[1] > '9'))
                rv = 0;

            t2 = t[2] & 0xdf; /* [a-f] -> [A-F] */
            if ((t2 < 'A' || t2 > 'F') && (t[2] < '0' || t[2] > '9'))
                rv = 0;

            *u = ((t[1] >= 'A' ? ((t[1] & 0xdf) - 'A')+10 : (t[1] - '0'))*16) +
                  (t[2] >= 'A' ? ((t[2] & 0xdf) - 'A')+10 : (t[2] - '0'));
            t += 2;
        }
        else if (u != t)
            *u = *t;
    }
    *u = *t;

    return rv;
}


NSAPI_PUBLIC int
util_uri_unescape_plus (const char *src, char *trg, int len)
{
    const char *t = src;
    char *u = trg == NULL ? (char *)src : trg;
    int	rlen = 0;

    if (len == -1)
        len = strlen (src);

    for( ; len && *t; ++t, ++u, len--, rlen++)
    {
        if((*t == '%') && t[1] && t[2])
        {
            *u = ((t[1] >= 'A' ? ((t[1] & 0xdf) - 'A') + 10 : (t[1] - '0')) * 16) +
                  (t[2] >= 'A' ? ((t[2] & 0xdf) - 'A') + 10 : (t[2] - '0'));
            t  += 2;
            len-= 2;
        }
        else
            if (*t == '+')
                *u = ' ';
            else
                *u = *t;
    }
    *u = 0;
    return rlen;
}


/* ------------------------- util_mime_separator -------------------------- */


NSAPI_PUBLIC int util_mime_separator(char *sep)
{
    int size = 35; // documented in nsapi.h
    int pos = 0;

    sep[pos++] = CR;
    sep[pos++] = LF;
    sep[pos++] = '-';
    sep[pos++] = '-';

    unsigned char buf[15]; // 20 * 6 / 8
    util_random(buf, sizeof(buf));
    pos += url64_encode(buf, sizeof(buf), sep + pos, size - pos - 1);

    sep[pos] = '\0';

    return pos;
}


/* ------------------------------ util_itoa ------------------------------- */


NSAPI_PUBLIC int util_itoa(int i, char *a)
{
    int len = util_i64toa(i, a);

    PR_ASSERT(len < UTIL_ITOA_SIZE);

    return len;
}


/* ----------------------------- util_i64toa ------------------------------ */

/* 
 * Assumption: Reversing the digits will be faster in the general case 
 * than doing a log10 or some nasty trick to find the # of digits.
 */

NSAPI_PUBLIC int util_i64toa(PRInt64 i, char *a)
{
    register int x, y, p;
    register char c;
    int negative;

    negative = 0;
    if(i < 0) {
        *a++ = '-';
        negative = 1;
        i = -i;
    }
    p = 0;
    while(i > 9) {
        a[p++] = (i%10) + '0';
        i /= 10;
    }
    a[p++] = i + '0';

    if(p > 1) {
        for(x = 0, y = p - 1; x < y; ++x, --y) {
            c = a[x];
            a[x] = a[y];
            a[y] = c;
        }
    }
    a[p] = '\0';

    PR_ASSERT(p + negative < UTIL_I64TOA_SIZE);

    return p + negative;
}


/* ----------------------------- util_sprintf ----------------------------- */


#include "prprf.h"

/* 
   XXXrobm the NSPR interfaces don't allow me to just pass in a buffer 
   without a size
 */
#define UTIL_PRF_MAXSIZE 1048576

NSAPI_PUBLIC int util_vsnprintf(char *s, int n, register const char *fmt, 
                                va_list args)
{
    return PR_vsnprintf(s, n, fmt, args);
}

NSAPI_PUBLIC int util_snprintf(char *s, int n, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return PR_vsnprintf(s, n, fmt, args);
}

NSAPI_PUBLIC int util_vsprintf(char *s, register const char *fmt, va_list args)
{
    return PR_vsnprintf(s, UTIL_PRF_MAXSIZE, fmt, args);
}

NSAPI_PUBLIC int util_sprintf(char *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    return PR_vsnprintf(s, UTIL_PRF_MAXSIZE, fmt, args);
}

/* ---------------------------- util_sh_escape ---------------------------- */


NSAPI_PUBLIC char *util_sh_escape(char *s)
{
    char *ns = (char *) MALLOC(strlen(s) * 2 + 1);   /* worst case */
    register char *t, *u;

    for(t = s, u = ns; *t; ++t, ++u) {
        if(strchr("&;`'\"|*?~<>^()[]{}$\\ #!", *t))
            *u++ = '\\';
        *u = *t;
    }
    *u = '\0';
    return ns;
}

/* --------------------------- util_strcasecmp ---------------------------- */

#ifdef NEED_STRCASECMP
/* These are stolen from mcom/lib/xp */
NSAPI_PUBLIC 
int util_strcasecmp(const char *one, const char *two)
{
    const char *pA;
    const char *pB;

    for(pA=one, pB=two; *pA && *pB; pA++, pB++)
      {
        int tmp = tolower(*pA) - tolower(*pB);
        if (tmp)
            return tmp;
      }
    if (*pA)
        return 1;
    if (*pB)
        return -1;
    return 0;
}
#endif /* NEED_STRCASECMP */

#ifdef NEED_STRNCASECMP
NSAPI_PUBLIC 
int util_strncasecmp(const char *one, const char *two, int n)
{
    const char *pA;
    const char *pB;

    for(pA=one, pB=two;; pA++, pB++)
      {
        int tmp;
        if (pA == one+n)
            return 0;
        if (!(*pA && *pB))
            return *pA - *pB;
        tmp = tolower(*pA) - tolower(*pB);
        if (tmp)
            return tmp;
      }
}
#endif /* NEED_STRNCASECMP */


/* --------------------------- util_strcasestr ---------------------------- */

/*
 * Case-insensitive strstr stolen from ns/lib/xp
 */

NSAPI_PUBLIC char *
util_strcasestr(char *str, const char *substr)
{
    if (!str)
        return NULL;

    for (char *pA = str; *pA; pA++) {
        if (toupper(*pA) == toupper(*substr)) {
            for (const char *pB = pA, *pC = substr; ; pB++, pC++) {
                if (!*pC)
                    return pA;

                if (!*pB)
                    break;

                if (toupper(*pB) != toupper(*pC))
                    break;
            }
        }
    }

    return NULL;
}


/* ----------------------------- util_strlcpy ----------------------------- */

/*
 * Copies src to the dstsize buffer at dst. The copy will never
 * overflow the destination buffer and the buffer will always be null
 * terminated.
 *
 * Implementation borrowed from Solaris 10.
 */

NSAPI_PUBLIC size_t
util_strlcpy(char *dst, const char *src, size_t len)
{
    size_t slen = strlen(src);
    size_t copied;

    if (len == 0)
        return (slen);
    if (slen >= len)
        copied = len - 1;
    else
        copied = slen;
    memcpy(dst, src, copied);
    dst[copied] = '\0';
    return (slen);
}


/* ----------------------------- util_strlcat ----------------------------- */

/*
 * Appends src to the dstsize buffer at dst. The append will never
 * overflow the destination buffer and the buffer will always be null
 * terminated.
 *
 * Implementation borrowed from Solaris 10.
 */

NSAPI_PUBLIC size_t
util_strlcat(char *dst, const char *src, size_t dstsize)
{
    size_t l1 = strlen(dst);
    size_t l2 = strlen(src);
    size_t copied;

    if (dstsize == 0 || l1 >= dstsize - 1)
        return (l1 + l2);

    copied = l1 + l2 >= dstsize ? dstsize - l1 - 1 : l2;
    memcpy(dst + l1, src, copied);
    dst[l1+copied] = '\0';
    return (l1 + l2);
}


/* ---------------------------- util_strlower ----------------------------- */

NSAPI_PUBLIC char *
util_strlower(char *s)
{
    char *p = s;

    while (*p) {
        *p = tolower(*p);
        p++;
    }

    return s;
}


/* ------------------------ util_decrement_string ------------------------- */

NSAPI_PUBLIC char *
util_decrement_string(char *s)
{
    // Skip leading whitespace
    while (isspace(*s))
        s++;

    // Find the trailing nul (or trailing non-digit character)
    char *end = s;
    while (isdigit(*end))
        end++;

    // We need at least one digit to do anything constructive
    if (end == s)
        return s;

    // Subtract 1 from the string, starting with the last digit
    char *digit;
    for (digit = end - 1; digit >= s; digit--) {
        // If we can subtract without borrowing...
        if (*digit != '0') {
            // Subtract 1 from this digit and we're done
            *digit = (*digit - 1); // works for ASCII
            break;
        }

        // Change this digit from '0' to '9'.  We'll need to borrow from the
        // next digit.
        *digit = '9';
    }

    // If we ran out of digits to borrow from...
    if (digit < s) {
        // Return an empty string
        *s = '\0';
        return s;
    }

    // Strip leading zeros
    while (s[0] == '0' && isdigit(s[1]))
        s++;

    return s;
}


/* ----------------------------- util_atoi64 ------------------------------ */

NSAPI_PUBLIC PRInt64
util_atoi64(const char *a)
{
    PRInt64 i = 0;
    PRBool negative = PR_FALSE;

    if (*a == '-') {
        negative = PR_TRUE;
        a++;
    }


    while (*a >= '0' && *a <= '9') {
        i *= 10;
        i += (*a - '0');
        a++;
    }

    if (negative)
        i = -i;

    return i;
}


// get next cookie attribute value pair
// This function is deprecated

NSAPI_PUBLIC char *
util_cookie_next_av_pair(char *p, char **a, char **v)
{
    char *attr, *attrend, *value, *valueend;

    // sanity check
    if (p == NULL || a == NULL || v == NULL)
        return NULL;

    while (isspace(*p)) p++;    // skip whitespace

    if (!*p)                    // if at end already, we just saw trailing whitespace
        return NULL;            // and are done


    // loop over attribute
    attr = p;
    while (*p && *p != '=' && !isspace(*p)) {
        *p = tolower(*p);
        p++;
    }
    attrend = p;

    while (isspace(*p)) p++;    // skip whitespace

    // do we have an attribute without a value?
    if ( (*p == '\0' || *p == ',' || *p == ';')) {
        *attrend = '\0';
        *a = attr;
        *v = NULL;
        if (*p) {
            p++;
            while (isspace(*p)) p++;    // skip whitespace
        }
        return p;
    }

    // otherwise, we must have arrived at "="
    if (*p != '=')
        return NULL;            // this is our way of saying "syntax error"

    p++;                        // point beyond the "="
    while (isspace(*p)) p++;    // skip whitespace

    // now we stand at the first non-whitespace char after the "="
    // see if it's a quote start
    int quotedvalue = 0;
    if (*p == '"') {
        p++;
        quotedvalue = 1;
    }
    value = p;

    for (;
         *p &&                       // must not be end of string
         *p != '"' &&                // or beginning or end of quote
         (quotedvalue ||             // and if the value is not quouted,
          (*p != ';' && *p != ','))  // it may not be one of the commata, too
         ; p++)
     ;
    if ((*p == '\0' && quotedvalue) || (*p == '"' && !quotedvalue))
        return NULL;            // we reached end of string, but were expecting a quote
                                // or we got an unexpected quote character

    // now p points to the end of the value
    valueend = p;

    // p points either to whitespace, '\0', '"', ';' or ',' now
    if (isspace(*p) || *p == '"') {
        p++;                                    // skip trailing '"' or whitespace
        while (isspace(*p)) p++;                // skip rest of whitespace
        if (*p && *p != ';' && *p != ',')     // must hit EOS, ',' or ';'
          return NULL;
    }

    // p points either to '\0', ';' or ',' now
    if (*p) {
        p++;
        while (isspace(*p)) p++;                // skip whitespace
    }

    // p points either to '\0' or beginning of next token
    *attrend = '\0';
    *valueend = '\0';
    *a = attr;
    *v = value;

    return p;
}

PRBool 
is_delimiter(int version, char p)
{
    PRBool res = PR_FALSE;
    if (p == '\0') {
        res = PR_TRUE;
    }
    else {
        if ((version == 0) && ((p == ';') || (p == ','))) {
           res = PR_TRUE;
        }
        else if (p == ';') {
           res = PR_TRUE;
        }
    }

    return res;
}


/* ------------------------- util_cookie_next_av_pair --------------------------- */

// get next cookie attribute value pair
char *
_util_cookie_next_av_pair(char *p, char **a, char **v, int *version)
{
    char *attr, *attrend, *value, *valueend;

    // sanity check
    if (p == NULL || a == NULL || v == NULL)
	return NULL;

    while (isspace(*p)) p++;	// skip whitespace

    if (!*p)			// if at end already, we just saw trailing whitespace
	return NULL;		// and are done

   if (!strncasecmp(p, "$version=", 9))
      *version = ((p[9] == '"') ? p[10] : p[9]) - '0';

    // loop over attribute
    attr = p;
    while (*p && *p != '=' && !isspace(*p)) {
	*p = tolower(*p);
	p++;
    }
    attrend = p;

    while (isspace(*p)) p++;	// skip whitespace

    // do we have an attribute without a value?
    if ( ( (*version == 0) && (*p == '\0' || *p == ',' || *p == ';')) ||
         ((*version > 0) && (*p == '\0' || *p == ';')) ) {
        *attrend = '\0';
        *a = attr;
        *v = NULL;
        if (*p) {
            p++;
            while (isspace(*p)) p++;    // skip whitespace
        }
        return p;
    }

    // otherwise, we must have arrived at "="
    if (*p != '=')
	return NULL;		// this is our way of saying "syntax error"

    p++;			// point beyond the "="
    while (isspace(*p)) p++;	// skip whitespace

    // now we stand at the first non-whitespace char after the "="
    // see if it's a quote start
    int quotedvalue = 0;
    if (*p == '"') {
	p++;
	quotedvalue = 1;
    }
    value = p;

    if (*version == 0)
       for (;
            *p &&                       // must not be end of string
            *p != '"' &&                // or beginning or end of quote
            (quotedvalue ||             // and if the value is not quouted,
             (*p != ';' && *p != ','))  // it may not be one of the commata, too
            ; p++)
        ;
    else
       for (;
            *p &&                       // must not be end of string
            *p != '"' &&                // or beginning or end of quote
            (quotedvalue ||             // and if the value is not quouted,
             (*p != ';' ))      // it may not be one of the commata, too
            ; p++)
        ;

    if ((*p == '\0' && quotedvalue) || (*p == '"' && !quotedvalue))
	return NULL;		// we reached end of string, but were expecting a quote
				// or we got an unexpected quote character

    // now p points to the end of the value
    valueend = p;

    // p points either to whitespace, '\0', '"', ';' or ',' now
    if (isspace(*p) || *p == '"') {
	p++;					// skip trailing '"' or whitespace
	while (isspace(*p)) p++;		// skip rest of whitespace
        if (*version == 0) {
          if (*p && *p != ';' && *p != ',')     // must hit EOS, ',' or ';'
            return NULL;
        }
        else {
          if (*p && *p != ';')  // must hit EOS, ',' or ';'
            return NULL;
        }
    }

    // p points either to '\0', ';' or ',' now
    if (*p) {
	p++;
	while (isspace(*p)) p++;		// skip whitespace
    }

    // p points either to '\0' or beginning of next token
    *attrend = '\0';
    *valueend = '\0';
    *a = attr;
    *v = value;

    return p;
}

/* --------------------------- util_cookie_find ---------------------------- */

NSAPI_PUBLIC char *
util_cookie_find(char *cookie, const char *name)
{
    // no cookie? no name?
    if (cookie == NULL || name == NULL)
        return NULL;
    char *a = NULL, *v = NULL;
    int version = 0;

    // skip $version etc.
    while (cookie != NULL) {
	cookie = _util_cookie_next_av_pair(cookie, &a, &v, &version);
	if (a && (strcasecmp(name, a) == 0))
	    return v;
    }
    return NULL;
}

/* --------------------------- util_cookie_next ---------------------------- */

NSAPI_PUBLIC char *
util_cookie_next(char *cookie, char **name, char **value)
{
    char *a, *v;
    
    int version = 0;

    // no cookie? no name?
    if (cookie == NULL || name == NULL || value == NULL)
	return NULL;

    // skip $version etc.
    while (cookie != NULL) {
	cookie = util_cookie_next_av_pair(cookie, &a, &v);
	if (cookie && *a != '$')
	    break;		// found a "regular" attribute
    }
    if (cookie != NULL) {
	*name = a;
	*value = v;
    }
    return cookie;
}


#ifdef XP_WIN32

/* util_delete_directory()
 * This routine deletes all the files in a directory.  If delete_directory is 
 * TRUE it will also delete the directory itself.
 */
VOID
util_delete_directory(char *FileName, BOOL delete_directory)
{
    HANDLE firstFile;
    WIN32_FIND_DATA findData;
    char *TmpFile, *NewFile;

    if (FileName == NULL)
        return;

    TmpFile = (char *)MALLOC(strlen(FileName) + 5);
    sprintf(TmpFile, "%s\\*.*", FileName);
    firstFile = FindFirstFile(TmpFile, &findData);
    FREE(TmpFile);

    if (firstFile == INVALID_HANDLE_VALUE) 
        return;

    if(strcmp(findData.cFileName, ".") &&
        strcmp(findData.cFileName, "..")) {
            NewFile = (char *)MALLOC(strlen(FileName) + 1 +
                strlen(findData.cFileName) + 1);
            sprintf(NewFile, "%s\\%s",FileName, findData.cFileName);
            DeleteFile(NewFile);
            FREE(NewFile);
    }
    while (TRUE) {
        if(!(FindNextFile(firstFile, &findData))) {
            if (GetLastError() != ERROR_NO_MORE_FILES) {
                ereport(LOG_WARN, XP_GetAdminStr(DBT_couldNotRemoveTemporaryDirectory_), FileName, GetLastError());
            } else {
                FindClose(firstFile);
				if (delete_directory)
					if(!RemoveDirectory(FileName)) {
						ereport(LOG_WARN,
							XP_GetAdminStr(DBT_couldNotRemoveTemporaryDirectory_1),
							FileName, GetLastError());
					}
                return;
            }
        } else {
            if(strcmp(findData.cFileName, ".") &&
                strcmp(findData.cFileName, "..")) {
                NewFile = (char *)MALLOC(strlen(FileName) + 5 +
                    strlen(findData.cFileName) + 1);
                sprintf(NewFile,"%s\\%s", FileName, findData.cFileName);
                DeleteFile(NewFile);
                FREE(NewFile);
            }
        }
    }
}
#endif


/* ---------------------------- util_strlftime ---------------------------- */

NSAPI_PUBLIC int util_strlftime(char *dst, size_t dstsize, const char *format, const struct tm *t)
{
    if (dstsize > 0)
        dst[0] = '\0';

    // "%c" -> "Fri Jun 24 07:53:16 PDT 2005", so we may need a buffer 14x
    // bigger than the format string
    int needed = strlen(format) * 14 + 1;

    char *p;
    if (dstsize < needed) {
        p = (char *) MALLOC(needed);
        if (!p)
            return -1;
    } else {
        p = dst;
    }

    int rv = util_strftime(p, format, t);

    if (p != dst) {
        if (rv > 0 && dstsize > 0) {
            if (rv > dstsize - 1)
                rv = dstsize - 1;
            memcpy(dst, p, rv);
            dst[rv] = '\0';
        }
        FREE(p);
    }

    return rv;
}


/* ------------------------------ util_strftime --------------------------- */
/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strftime.c	5.11 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#endif

static char *afmt[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};
static char *Afmt[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday",
};

static char *bfmt[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec",
};
static char *Bfmt[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December",
};

#define TM_YEAR_BASE 1900

static int _util_strftime_isdst(const struct tm *t);
static void _util_strftime_conv(char *, int, int, char);

#define _util_strftime_add(str) for (;(*pt = *str++); pt++);
#define _util_strftime_copy(str, len) memcpy(pt, str, len); pt += len;
#define _util_strftime_fmt util_strftime

/* util_strftime()
 * This is an optimized version of strftime for speed.  Avoids the thread
 * unsafeness of BSD strftime calls. 
 */
int
util_strftime(char *pt, const char *format, const struct tm *t)
{
    // XXX this needs bounds checks
    char *start = pt;
	char *scrap;
	int isdst = daylight ? -1 /* unknown */ : 0 /* TZ doesn't have DST */;

	for (; *format; ++format) {
		if (*format == '%')
			switch(*++format) {
			case 'a': /* abbreviated weekday name */
				*pt++ = afmt[t->tm_wday][0];
				*pt++ = afmt[t->tm_wday][1];
				*pt++ = afmt[t->tm_wday][2];
				continue;
			case 'd': /* day of month */
				_util_strftime_conv(pt, t->tm_mday, 2, '0');
				pt += 2;
				continue;
			case 'S':
				 _util_strftime_conv(pt, t->tm_sec, 2, '0');
				pt += 2;
				continue;
			case 'M':
				 _util_strftime_conv(pt, t->tm_min, 2, '0');
				pt += 2;
				continue;
			case 'H':
				_util_strftime_conv(pt, t->tm_hour, 2, '0');
				pt += 2;
				continue;
			case 'Y':
				if (t->tm_year < 100) {
					*pt++ = '1';
					*pt++ = '9';
					_util_strftime_conv(pt, t->tm_year, 2, '0');
				} else {
					/* will fail after 2100; but who cares? */
					*pt++ = '2';
					*pt++ = '0';
					_util_strftime_conv(pt, t->tm_year-100, 2, '0');
				}
				pt += 2;
				continue;
			case 'b': /* abbreviated month name */
			case 'h':
				*pt++ = bfmt[t->tm_mon][0];
				*pt++ = bfmt[t->tm_mon][1];
				*pt++ = bfmt[t->tm_mon][2];
				continue;
			case 'T':
			case 'X':
				pt += _util_strftime_fmt(pt, "%H:%M:%S", t);
				continue;
			case '\0':
				--format;
				break;
			case 'A':
				if (t->tm_wday < 0 || t->tm_wday > 6)
					return(0);
				scrap = Afmt[t->tm_wday];
				_util_strftime_add(scrap);
				continue;
			case 'B':
				if (t->tm_mon < 0 || t->tm_mon > 11)
					return(0);
				scrap = Bfmt[t->tm_mon];
				_util_strftime_add(scrap);
				continue;
			case 'C':
				pt += _util_strftime_fmt(pt, "%a %b %e %H:%M:%S %Y", t);
				continue;
			case 'c':
				pt += _util_strftime_fmt(pt, "%m/%d/%y %H:%M:%S", t);
				continue;
			case 'D':
				pt += _util_strftime_fmt(pt, "%m/%d/%y", t);
				continue;
			case 'e':
				_util_strftime_conv(pt, t->tm_mday, 2, ' ');
				pt += 2;
				continue;
			case 'I':
				_util_strftime_conv(pt, t->tm_hour % 12 ?
				    t->tm_hour % 12 : 12, 2, '0');
				pt += 2;
				continue;
			case 'j':
				_util_strftime_conv(pt, t->tm_yday + 1, 3, '0');
				pt += 3;
				continue;
			case 'k':
				_util_strftime_conv(pt, t->tm_hour, 2, ' ');
				pt += 2;
				continue;
			case 'l':
				 _util_strftime_conv(pt, t->tm_hour % 12 ?
				    t->tm_hour % 12 : 12, 2, ' ');
				pt += 2;
				continue;
			case 'm':
				 _util_strftime_conv(pt, t->tm_mon + 1, 2, '0');
				pt += 2;
				continue;
			case 'n':
				*pt = '\n';
				pt++;
				continue;
			case 'p':
				if (t->tm_hour >= 12) {
					*pt = 'P';
					pt++;
				} else {
					*pt = 'A';
					pt++;
				}
				*pt = 'M';
				pt++;
				continue;
			case 'R':
				pt += _util_strftime_fmt(pt, "%H:%M", t);
				continue;
			case 'r':
				pt += _util_strftime_fmt(pt, "%I:%M:%S %p", t);
				continue;
			case 't':
				*pt = '\t';
				pt++;
				continue;
			case 'U':
				 _util_strftime_conv(pt, (t->tm_yday + 7 - t->tm_wday) / 7,
				    2, '0');
				pt += 2;
				continue;
			case 'W':
				 _util_strftime_conv(pt, (t->tm_yday + 7 -
				    (t->tm_wday ? (t->tm_wday - 1) : 6))
				    / 7, 2, '0');
				pt += 2;
				continue;
			case 'w':
                                *pt++ = t->tm_wday + '0';
				continue;
			case 'x':
				pt += _util_strftime_fmt(pt, "%m/%d/%y", t);
				continue;
			case 'y':
				 _util_strftime_conv(pt, (t->tm_year + TM_YEAR_BASE)
				    % 100, 2, '0');
				pt += 2;
				continue;
			/* TZ added to handle shtml parsing requirements */
			case 'Z': 
				if (isdst < 0)
					isdst = _util_strftime_isdst(t);
				pt += util_snprintf(pt, 4, "%s", tzname[isdst]);
				continue;
			case 'z':
				{
#ifdef BSD_TIME
					int tz = t->tm_gmtoff;
#else
					int tz = -timezone;
					if (isdst < 0)
						isdst = _util_strftime_isdst(t);
					if (isdst)
						tz += 3600;
#endif
					char sign = tz < 0 ? '-' : '+';
					if (tz < 0)
						tz = -tz;
					tz = (tz + 30) / 60;
					pt += util_snprintf(pt, 6, "%c%02.2d%02.2d",
					    sign, tz / 60, tz % 60);
				}
				continue;
			case '%':
			/*
			 * X311J/88-090 (4.12.3.5): if conversion char is
			 * undefined, behavior is undefined.  Print out the
			 * character itself as printf(3) does.
			 */
			default:
				break;
		}
		*pt = *format;
		pt++;
	}

    start[pt-start] = '\0';

	return pt - start;
}

static int
_util_strftime_isdst(const struct tm *t)
{
	int isdst = t->tm_isdst;
	if (isdst < 0) {
		struct tm mytm = *t;
		mktime(&mytm);
		isdst = mytm.tm_isdst;
	}
	return (isdst > 0);
}

static void
_util_strftime_conv(char *pt, int n, int digits, char pad)
{
	static char buf[10];
	register char *p;

	if (n >= 100) {
		p = buf + sizeof(buf)-2;
		for (; n > 0 && p > buf; n /= 10, --digits)
			*p-- = n % 10 + '0';
    	while (p > buf && digits-- > 0)
		  	*p-- = pad;
		p++;
		_util_strftime_add(p);
    } else {
		int tens;
		int ones = n;

        tens = 0;
        if ( ones >= 10 ) {
            while ( ones >= 10 ) {
                tens++;
                ones = ones - 10;
            }
            *pt++ = '0'+tens;
            digits--;
        }
		else 
			*pt++ = '0';
        *pt++ = '0'+ones;
        digits--;
		while(digits--)
			*pt++ = pad;
    }
	return;
}


#ifdef XP_UNIX
/*
 * Local Thread Safe version of waitpid.  This prevents the process
 * from blocking in the system call.
 */
NSAPI_PUBLIC pid_t 
util_waitpid(pid_t pid, int *stat_loc, int options)
{
    PRIntervalTime _sleepTime = PR_MillisecondsToInterval(2);
    pid_t rv = -1;

    int count = 0;
    for(rv = 0; !rv && (count < 1000); PR_Sleep(_sleepTime)) {
        rv = waitpid(pid, stat_loc, options | WNOHANG);
        if (rv == -1) {
            if (errno == EINTR) {
                rv = 0; /* sleep and try again */
            } else {
                NsprError::mapUnixErrno();
                ereport(LOG_WARN, XP_GetAdminStr(DBT_Waitpid_Failed), 
                        pid, errno, system_errmsg());
            }
        }
        else if (rv == 0) {
            count++;
            ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_Waitpid_Returned),
                    pid, errno, system_errmsg());
        }
    }
    if (count == 1000) {
        PR_SetError(PR_IO_TIMEOUT_ERROR, 0);
        rv = -1;
    }

    return rv;
}
#endif

/*
 * Various reentrant routines by mikep.  See util.h and systems.h
 */

/*
 * These are only necessary if we turn on interrupts in NSPR
 */
#ifdef NEED_RELOCKS
#include "crit.h"
#define RE_LOCK(name) \
    static CRITICAL name##_crit = 0; \
    if (name##_crit == 0) name##_crit = crit_init(); \
    crit_enter(name##_crit)

#define RE_UNLOCK(name)  crit_exit(name##_crit)

#else
#define RE_LOCK(name) /* nada */
#define RE_UNLOCK(name) /* nil */
#endif


#ifndef XP_WIN32
NSAPI_PUBLIC struct passwd *
util_getpwnam(const char *name, struct passwd *result, char *buffer, 
	int buflen)
{
    struct passwd *rv;

#if defined(AIX) || defined(LINUX) || defined(HPUX)
    errno = getpwnam_r(name, result, buffer, buflen, &rv);
    if (errno != 0)
        rv = NULL;
#else
    rv = getpwnam_r(name, result, buffer, buflen);
#endif
    if (rv == NULL)
        NsprError::mapUnixErrno();

    return rv;
}
#endif


#ifndef XP_WIN32
NSAPI_PUBLIC struct passwd *
util_getpwuid(uid_t uid, struct passwd *result, char *buffer, int buflen)
{
    struct passwd *rv;

#if defined(AIX) || defined(LINUX) || defined(HPUX)
    errno = getpwuid_r(uid, result, buffer, buflen, &rv);
    if (errno != 0)
        rv = NULL;
#else
    rv = getpwuid_r(uid, result, buffer, buflen);
#endif
    if (rv == NULL)
        NsprError::mapUnixErrno();

    return rv;
}
#endif


NSAPI_PUBLIC struct tm *
util_localtime(const time_t *clock, struct tm *res)
{
#ifdef HAVE_TIME_R
    return localtime_r(clock, res);
#else
    struct tm *rv;
    time_t zero = 0x7fffffff;

    RE_LOCK(localtime);
    rv = localtime(clock);
    if (!rv)
        rv = localtime(&zero);
    if (rv)
        *res = *rv;
    else
        res = NULL;
    RE_UNLOCK(localtime);

    return res;
#endif
}


NSAPI_PUBLIC char *
util_ctime(const time_t *clock, char *buf, int buflen)
{
/* 
 * From cgi-src/restore.c refering to XP_WIN32:
 * 	MLM - gross, but it works, better now FLC
 */
#if !defined(HAVE_TIME_R) || defined(XP_WIN32)
    RE_LOCK(ctime);
    strncpy(buf, ctime(clock), buflen);
    buf[buflen - 1] = '\0';
    RE_UNLOCK(ctime);
    return buf;
#elif HAVE_TIME_R == 2
    return ctime_r(clock, buf);
#else /* HAVE_TIME_R == 3 */
    return ctime_r(clock, buf, buflen);
#endif
}

NSAPI_PUBLIC struct tm *
util_gmtime(const time_t *clock, struct tm *res)
{
#ifdef HAVE_TIME_R
    return gmtime_r(clock, res);
#else
    struct tm *rv;
    time_t zero = 0x7fffffff;

    RE_LOCK(gmtime);
    rv = gmtime(clock);
    if (!rv)
        rv = gmtime(&zero);
    if (rv)
        *res = *rv;
    else 
        res = NULL;
    RE_UNLOCK(gmtime);
    
    return res;
#endif
}

NSAPI_PUBLIC char *
util_asctime(const struct tm *tm, char *buf, int buflen)
{
#if HAVE_TIME_R == 2
    return asctime_r(tm, buf);
#elif HAVE_TIME_R == 3
    return asctime_r(tm, buf, buflen);
#else
    RE_LOCK(asctime);
    strncpy(buf, asctime(tm), buflen);
    buf[buflen - 1] = '\0';
    RE_UNLOCK(asctime);
    return buf;
#endif
}

NSAPI_PUBLIC char *
util_strerror(int errnum, char *buf, int buflen)
{
    const char *errmsg = NULL;

    /* This function returns NULL if it can't find a description of the error.
     * This is consistent with strerror() and differs from system_errmsg().
     */

    if (errmsg < 0) {
        /* Not really an OS error -- must be one of ours */
        errmsg = nscperror_lookup(errnum);
    }

    if (errmsg == NULL) {
#ifdef HAVE_STRERROR_R
        /* More IBM real-genius */
        return ((int)strerror_r(errnum, buf, buflen) > 0) ? buf : NULL;
#else
        /* RE_LOCK(strerror); I don't think this is worth the trouble */
        errmsg = strerror(errnum);
#endif
    }

    if (buflen < 1)
        return NULL;

    int errlen = strlen(errmsg);
    if (errlen >= buflen)
        errlen = buflen - 1;

    memcpy(buf, errmsg, errlen);
    buf[errlen] = '\0';

    return buf;
    /* RE_UNLOCK(strerror); */
}

NSAPI_PUBLIC PRBool util_format_http_version(const char *v, int *protv_num, char *buffer, int size)
{
    if (!v || !*v)
        return PR_FALSE;

    // Get major and minor version
    int major = 0, minor = 0;
    const char *t = v;
    while (*t && !isdigit(*t)) t++;
    if (sscanf(t, "%d.%d", &major, &minor) != 2) {
        return PR_FALSE;
    }

    if (protv_num) {
        *protv_num = major * 100 + minor;
    }

    // Build a string with protocol name and version (e.g. HTTP/1.1)
    if (buffer) {
        // Ignore any leading quotation mark
        if (*v == '"') v++;

        int pos = 0;
        if (isdigit(*v)) {
            pos = util_snprintf(buffer, size, "HTTP/");
        }
        pos += util_snprintf(&buffer[pos], size - pos, "%s", v);

        // Ignore any trailing quotation mark
        if (pos > 0 && buffer[pos-1] == '"') {
            buffer[pos-1] = '\0';
        }
    }

    return PR_TRUE;
}

NSAPI_PUBLIC int util_getboolean(const char *v, int def)
{
    if (!StringValue::hasBoolean(v)) {
        return def;
    }

    return StringValue::getBoolean(v);
}

NSAPI_PUBLIC PRIntervalTime util_getinterval(const char *v, PRIntervalTime def)
{
    const char *p = v;
    PRIntervalTime interval;

    /*
     * "99999.9999" -> PR_MillisecondsToInterval(21600000)
     * "1.00000001" -> PR_MillisecondsToInterval(1000)
     * "0.00000001" -> PR_MillisecondsToInterval(1)
     * ".01" -> PR_MillisecondsToInterval(10)
     * "0.0" -> PR_MillisecondsToInterval(0)
     * "hi" -> def
     * "1-" -> def
     * "-2" -> def
     * "-1" -> PR_INTERVAL_NO_TIMEOUT
     */

    PR_ASSERT(PR_IntervalToSeconds(MAX_INTERVAL) == MAX_SECONDS);
    PR_ASSERT(TICKS_PER_MILLISECOND > 0);

    while (isspace(*p))
        p++;

    if (*p == '-') {
        if (p[1] == '1') {
            interval = PR_INTERVAL_NO_TIMEOUT;
            p += 2;
        }
    } else {
        int seconds = 0;
        while (*p >= '0' && *p <= '9')
            seconds = seconds * 10 + (*p++ - '0');
        if (p - v > MAX_SECONDS_DIGITS)
            seconds = MAX_SECONDS;
        int milliseconds = 0;
        if (*p == '.') {
            p++;
            if (*p >= '0' && *p <= '9') {
                milliseconds = (*p++ - '0') * 100;
                if (*p >= '0' && *p <= '9') {
                    milliseconds += (*p++ - '0') * 10;
                    if (*p >= '0' && *p <= '9') {
                        milliseconds += (*p++ - '0');
                        if (seconds == 0 && milliseconds == 0) {
                            while (*p == '0') 
                                p++;
                            if (*p)
                                milliseconds = 1;
                        }
                        while (*p >= '0' && *p <= '9')
                            p++;
                    }
                }
            }
        }
        if (seconds < MAX_SECONDS) {
            interval = seconds * TICKS_PER_SECOND +
                       milliseconds * TICKS_PER_MILLISECOND;
        } else {
            interval = MAX_INTERVAL;
        }
    }

    if (*p) {
        while (isspace(*p))
            p++;
        if (*p)
            return def;
    }

    return interval;
}


/* --------------------------- util_html_escape --------------------------- */

NSAPI_PUBLIC char *util_html_escape(const char *s)
{
    const char *in;

    int len = 0;
    for (in = s; *in; in++) {
        switch (*in) {
        case '<':
            len += 4; // &lt;
            break;
        case '>':
            len += 4; // &gt;
            break;
        case '&':
            len += 5; // &amp;
            break;
        case '"':
            len += 6; // &quot;
            break;
        case '\'':
            len += 6; // &apos;
            break;
        case '+':
            len += 5; // &#43;
            break;
        default:
            len++;
            break;
        }
    }

    char *ns = (char *) MALLOC(len + 1);
    if (!ns)
        return ns;

    char *out = ns;
    for (in = s; *in; in++) {
        switch (*in) {
        case '<':
            *out++ = '&';
            *out++ = 'l';
            *out++ = 't';
            *out++ = ';';
            break;
        case '>':
            *out++ = '&';
            *out++ = 'g';
            *out++ = 't';
            *out++ = ';';
            break;
        case '&':
            *out++ = '&';
            *out++ = 'a';
            *out++ = 'm';
            *out++ = 'p';
            *out++ = ';';
            break;
        case '"':
            *out++ = '&';
            *out++ = 'q';
            *out++ = 'u';
            *out++ = 'o';
            *out++ = 't';
            *out++ = ';';
            break;
        case '\'':
            *out++ = '&';
            *out++ = 'a';
            *out++ = 'p';
            *out++ = 'o';
            *out++ = 's';
            *out++ = ';';
            break;
        case '+':
            *out++ = '&';
            *out++ = '#';
            *out++ = '4';
            *out++ = '3';
            *out++ = ';';
            break;
        default:
            *out++ = *in;
            break;
        }
    }
    *out = '\0';

    return ns;
}
