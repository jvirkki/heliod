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

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nspr.h>
#include "nstime.h"

#define	min(a, b)	((a) < (b) ? (a) : (b))
#define	max(a, b)	((a) > (b) ? (a) : (b))
#define	sign(x)		((x) < 0 ? -1 : 1)
#define isleap(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#define	SECS_PER_HOUR	(60 * 60)
#define	SECS_PER_DAY	(SECS_PER_HOUR * 24)
#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))

#if !defined(SOLARIS)
#define HAVE_TM_ZONE
#define HAVE_TM_GMTOFF
#endif

typedef enum { J0, J1, M } dsttype_t;

/* This structure contains all the information about a
   timezone given in the POSIX standard TZ envariable.  */
typedef struct {
    const char *name;
    long int offset;		/* Seconds east of GMT (west if < 0).  */
    dsttype_t type;     	/* Interpretation of:  */
    unsigned short int m, n, d;	/* Month, week, day.  */
    unsigned int secs;		/* Time of day.  */
} tz_rule;

/* tz_rules[0] is standard, tz_rules[1] is daylight.  */
static tz_rule tz_rules[2];
static PRBool tz_havedst;

static const unsigned short int mon2dayofyear[2][13] = {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

typedef struct {
    time_t dststart;      // start of DST
    time_t dstend;        // end of DST
    time_t start;         // start of year, GMT
} dstinfo_t;

#define NYEARS (2039 - 1970)

static dstinfo_t dstinfo[NYEARS];

static time_t dst_transition(tz_rule *rule, int year);

/* Interpret the TZ envariable.  */
static PRStatus
tzinit(void)
{
    static int is_initialized = 0;
    register const char *tz;
    register size_t l;
    char *tzbuf;
    unsigned short int hh, mm, ss;
    unsigned short int whichrule;
    time_t t;

    if (is_initialized)
        return PR_SUCCESS;
    is_initialized = 1;

    tz_havedst = PR_FALSE;
    tz_rules[0].name = tz_rules[1].name = "GMT";
    tz_rules[0].type = tz_rules[1].type = J0;
    tz_rules[0].m = tz_rules[0].n = tz_rules[0].d = 0;
    tz_rules[1].m = tz_rules[1].n = tz_rules[1].d = 0;
    tz_rules[0].secs = tz_rules[1].secs = 0;
    tz_rules[0].offset = tz_rules[1].offset = 0L;

    /* Examine the TZ environment variable.  */
    tz = getenv("TZ");
    if (tz == NULL || *tz == '\0')
        return PR_SUCCESS;

    tzbuf = strdup(tz);   // guaranteed to have enough space

    /* A leading colon means "implementation defined syntax".
       We ignore the colon and always use the same algorithm:
       try a data file, and if none exists parse the 1003.1 syntax.  */
    if (tz && *tz == ':')
        ++tz;

    /* Get the standard timezone name.  */
    if (sscanf (tz, "%[^0-9,+-]", tzbuf) != 1 || (l = strlen (tzbuf)) < 3)
        return PR_SUCCESS;
    
    tz_rules[0].name = tzbuf;

    // advance beyond time zone name
    tz += l;

    /* Figure out the standard offset from UTC.  */
    if (*tz == '\0' || (*tz != '+' && *tz != '-' && !isdigit (*tz)))
        // no offset - offset stays at 0 and tz_havedst = PR_FALSE
        return PR_SUCCESS;

    if (*tz == '-' || *tz == '+')
        tz_rules[0].offset = *tz++ == '-' ? 1L : -1L;
    else
        tz_rules[0].offset = -1L;
    switch (sscanf (tz, "%hu:%hu:%hu", &hh, &mm, &ss)) {
    default:
        // no offset - offset stays at 0 and tz_havedst = PR_FALSE
        return PR_SUCCESS;
    case 1:
        mm = 0;
    case 2:
        ss = 0;
    case 3:
        break;
    }
    tz_rules[0].offset *= (min (ss, 59) + (min (mm, 59) * 60) +
                           (min (hh, 23) * 60 * 60));

    // skip the offset specification
    for (l = 0; l < 3; ++l) {
        while (isdigit(*tz))
            ++tz;
        if (l < 2 && *tz == ':')
            ++tz;
    }

    /* Get the DST timezone name (if any).  */
    if (*tz == '\0')
        // There is no DST. tz_havedst = PR_FALSE
        return PR_SUCCESS;

    char *n = tzbuf + strlen (tzbuf) + 1;
    if (sscanf (tz, "%[^0-9,+-]", n) != 1 || (l = strlen (n)) < 3)
        // There is no DST. tz_havedst = PR_FALSE
        return PR_SUCCESS;

    tz_rules[1].name = n;
    tz_havedst = PR_TRUE;

    tz += l;

    /* Figure out the DST offset from GMT.  */
    if (*tz == '-' || *tz == '+')
        tz_rules[1].offset = *tz++ == '-' ? 1L : -1L;
    else
        tz_rules[1].offset = -1L;

    switch (sscanf (tz, "%hu:%hu:%hu", &hh, &mm, &ss)) {
    default:
        /* Default to one hour later than standard time.  */
        tz_rules[1].offset = tz_rules[0].offset + (60 * 60);
        break;

    case 1:
        mm = 0;
    case 2:
        ss = 0;
    case 3:
        tz_rules[1].offset *= (min (ss, 59) + (min (mm, 59) * 60) +
                             (min (hh, 23) * (60 * 60)));
        break;
    }

    // skip the offset specification
    for (l = 0; l < 3; ++l) {
        while (isdigit (*tz))
            ++tz;
        if (l < 2 && *tz == ':')
            ++tz;
    }

done_names:
    /* Figure out the standard <-> DST rules.  */
    for (whichrule = 0; whichrule < 2; ++whichrule) {
        register tz_rule *tzr = &tz_rules[whichrule];

        /* Ignore comma to support string following the incorrect
           specification in early POSIX.1 printings.  */
        tz += *tz == ',';

        /* Get the date of the change.  */
        if (*tz == 'J' || isdigit (*tz)) {
            char *end;
            tzr->type = *tz == 'J' ? J1 : J0;
            if (tzr->type == J1 && !isdigit (*++tz))
              goto do_dstinfo;
            tzr->d = (unsigned short int) strtoul (tz, &end, 10);
            if (end == tz || tzr->d > 365)
              goto do_dstinfo;
            else if (tzr->type == J1 && tzr->d == 0)
              goto do_dstinfo;
            tz = end;
        } else if (*tz == 'M') {
            int n;
            tzr->type = M;
            if (sscanf (tz, "M%hu.%hu.%hu%n",
                        &tzr->m, &tzr->n, &tzr->d, &n) != 3 ||
                tzr->m < 1 || tzr->m > 12 ||
                tzr->n < 1 || tzr->n > 5 || tzr->d > 6)
            {
                goto do_dstinfo;
            }
            tz += n;
        } else if (*tz == '\0') {
            /* United States Federal Law, the equivalent of "M4.1.0,M10.5.0".  */
            tzr->type = M;
            if (tzr == &tz_rules[0]) {
                tzr->m = 4;
                tzr->n = 1;
                tzr->d = 0;
            } else {
                tzr->m = 10;
                tzr->n = 5;
                tzr->d = 0;
            }
        } else
            goto do_dstinfo;

        if (*tz != '\0' && *tz != '/' && *tz != ',')
            goto do_dstinfo;
        else if (*tz == '/') {
            /* Get the time of day of the change.  */
            ++tz;
            if (*tz == '\0')
              goto do_dstinfo;
            switch (sscanf (tz, "%hu:%hu:%hu", &hh, &mm, &ss)) {
            default:
                hh = 2;		/* Default to 2:00 AM.  */
            case 1:
                mm = 0;
            case 2:
                ss = 0;
            case 3:
                break;
            }
            for (l = 0; l < 3; ++l) {
                while (isdigit (*tz))
                    ++tz;
                if (l < 2 && *tz == ':')
                    ++tz;
            }
            tzr->secs = (hh * 60 * 60) + (mm * 60) + ss;
        } else
          /* Default to 2:00 AM.  */
          tzr->secs = 2 * 60 * 60;
    }

do_dstinfo:

    t = 0;
    for (int y = 0; y < NYEARS; y++) {
        dstinfo[y].start = t;
        t += SECS_PER_DAY * (isleap (y + 1970) ? 366 : 365);
        dstinfo[y].dststart = dst_transition(&tz_rules[0], y + 1970);
        dstinfo[y].dstend = dst_transition(&tz_rules[1], y + 1970);
    }

    return PR_SUCCESS;
}

static int
findyearindex(time_t t)
{
    int idx;
    int low = 0, high = NYEARS;

    // binary search in dstinfo to find the right year
    while (1) {
        idx = (high + low) / 2;
        if (t < dstinfo[idx].start)
            high = idx;
        else if (low == idx)
            break;
        else
            low = idx;
    }
    return idx;
}


/* Figure out the exact time (as a time_t) in YEAR
   when the change described by RULE will occur and
   put it in RULE->change, saving YEAR in RULE->computed_for.
   Return nonzero if successful, zero on failure.  */
static time_t
dst_transition(tz_rule *rule, int year)
{
    register time_t t;

    // year is guaranteed to be between 1970 and 2038
    // First set T to January 1st, 0:00:00 GMT in YEAR.
    t = dstinfo[year - 1970].start;

    switch (rule->type) {
    case J1:
        /* Jn - Julian day, 1 == January 1, 60 == March 1 even in leap years.
           In non-leap years, or if the day number is 59 or less, just
           add SECS_PER_DAY times the day number-1 to the time of
           January 1, midnight, to get the day.  */
        t += (rule->d - 1) * SECS_PER_DAY;
        if (rule->d >= 60 && isleap (year))
            t += SECS_PER_DAY;
        break;

    case J0:
        /* n - Day of year.
           Just add SECS_PER_DAY times the day number to the time of Jan 1st.  */
        t += rule->d * SECS_PER_DAY;
        break;

    case M: {
        /* Mm.n.d - Nth "Dth day" of month M.  */
        unsigned int i;
        int d, m1, yy0, yy1, yy2, dow;
        const unsigned short int *myday = &mon2dayofyear[isleap (year)][rule->m];

        /* First add SECS_PER_DAY for each day in months before M.  */
        t += myday[-1] * SECS_PER_DAY;

        /* Use Zeller's Congruence to get day-of-week of first day of month. */
        m1 = (rule->m + 9) % 12 + 1;
        yy0 = (rule->m <= 2) ? (year - 1) : year;
        yy1 = yy0 / 100;
        yy2 = yy0 % 100;
        dow = ((26 * m1 - 2) / 10 + 1 + yy2 + yy2 / 4 + yy1 / 4 - 2 * yy1) % 7;
        if (dow < 0)
            dow += 7;

        /* DOW is the day-of-week of the first day of the month.  Get the
           day-of-month (zero-origin) of the first DOW day of the month.  */
        d = rule->d - dow;
        if (d < 0)
            d += 7;
        for (i = 1; i < rule->n; ++i) {
            if (d + 7 >= (int) myday[0] - myday[-1])
                break;
            d += 7;
        }

        /* D is the day-of-month (zero-origin) of the day we want.  */
        t += d * SECS_PER_DAY;

        }
        break;
    }

    /* T is now the Epoch-relative time of 0:00:00 GMT on the day we want.
       Just add the time of day and local offset from GMT, and we're done.  */

    return t - rule->offset + rule->secs;
}


static PRCallOnceType once = { 0 };

static struct tm *
common(const time_t *timer, int use_localtime, struct tm *tp)
{
    long int days, rem, y;
    const unsigned short int *ip;
    time_t offt;
    long int gmtoff;

    if (timer == NULL || *timer < 0 || tp == NULL)
        return NULL;

#ifdef TEST
    tzinit();
#else
    PR_CallOnce(&once, tzinit);
#endif

    tp->tm_isdst = 0;
#if defined(HAVE_TM_ZONE)
    tp->tm_zone = "GMT";
#endif
#if defined(HAVE_TM_GMTOFF)
    tp->tm_gmtoff = 0L;
#endif
    gmtoff = 0L;

    if (use_localtime) {
        if (tz_havedst) {
            int isdst;
            dstinfo_t *pYI;

            /* first figure out the year for GMT, then find DST change times for this year */
            pYI = dstinfo + findyearindex(*timer);
            if (pYI->dststart < pYI->dstend)
                // northern hemisphere
                isdst = (*timer >= pYI->dststart && *timer < pYI->dstend);
            else 
                // southern hemisphere
                isdst = !(*timer >= pYI->dstend && *timer < pYI->dststart);
            tp->tm_isdst = isdst;
#if defined(HAVE_TM_ZONE)
            tp->tm_zone = tz_rules[isdst].name;
#endif
#if defined(HAVE_TM_GMTOFF)
            tp->tm_gmtoff = tz_rules[isdst].offset;
#endif
            gmtoff = tz_rules[isdst].offset;
        } else {
            tp->tm_isdst = 0;
#if defined(HAVE_TM_ZONE)
            tp->tm_zone = tz_rules[0].name;
#endif
#if defined(HAVE_TM_GMTOFF)
            tp->tm_gmtoff = tz_rules[0].offset;
#endif
            gmtoff = tz_rules[0].offset;
        }
    }

    offt = *timer + gmtoff;
    days = offt / SECS_PER_DAY;
    rem = offt % SECS_PER_DAY;
    tp->tm_hour = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    tp->tm_min = rem / 60;
    tp->tm_sec = rem % 60;
    /* January 1, 1970 was a Thursday.  */
    tp->tm_wday = (4 + days) % 7;
    if (tp->tm_wday < 0)
        tp->tm_wday += 7;
    y = 1970;

    while (days < 0 || days >= (isleap (y) ? 366 : 365)) {
        /* Guess a corrected year, assuming 365 days per year.  */
        long int yg = y + days / 365 - (days % 365 < 0);

        /* Adjust DAYS and Y to match the guessed year.  */
        days -= ((yg - y) * 365
                 + LEAPS_THRU_END_OF (yg - 1)
                 - LEAPS_THRU_END_OF (y - 1));
        y = yg;
    }
    tp->tm_year = y - 1900;
    tp->tm_yday = days;
    ip = mon2dayofyear[isleap(y)];
    for (y = 11; days < (long int) ip[y]; --y)
        continue;
    days -= ip[y];
    tp->tm_mon = y;
    tp->tm_mday = days + 1;

    return tp;
}

/* Return the `struct tm' representation of *T in UTC,
   using *TP to store the result.  */
TIME_DLL_EXPORT struct tm *
reentrant_gmtime(const time_t *t, struct tm *tp)
{
  return common(t, 0, tp);
}

/* Return the `struct tm' representation of *T in the local time zone,
   using *TP to store the result.  */
TIME_DLL_EXPORT struct tm *
reentrant_localtime(const time_t *t, struct tm *tp)
{
  return common(t, 1, tp);
}
