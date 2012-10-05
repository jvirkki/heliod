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
 * putil.c: Proxy utilities
 *
 *
 *
 *
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 *
 */

#include "netsite.h"
#include "libproxy/util.h"
#include "base/util.h"
#include "base/shexp.h"
#include "base/pool.h"
#include "frame/conf.h"	/* security_active, server_hostname, port */
#include "base/ereport.h"

#ifdef FEAT_PROXY

#include <ctype.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <sys/time.h>
#endif

#ifdef AIX
#include <sys/select.h>	/* Don't ask my why I need this explicitly. Dunno. */
#endif

/* our OWN stralloccopy and cat */

char *system_StrAllocCopy(char **dest, const char *src)
{
    if ( *dest != NULL ) {
	/* non null, free it first */
	FREE(*dest);
    }
    if ( src == NULL ) {
	return *dest = NULL;
    }

    return *dest = STRDUP(src);
}


char *system_StrAllocCat(char **dest, const char *src)
{
    if (!src)
	return *dest;

    if (!*dest) {
	return *dest = STRDUP(src);
    }
    else {
	int len1 = strlen(*dest);
	int len2 = strlen(src);
	char *p = ( char * ) MALLOC(len1 + len2 + 1 );

	strcpy(p, *dest);
	strcpy(&p[len1], src);

	/* non null, free it first */
	FREE(*dest);

	return *dest = p;
    }
}

/*	Allocate a new copy of a block of binary data, and returns it
 */
char * 
system_BlockAllocCopy (char **destination, const char *source, size_t length)
{
	if(*destination)
	  {
	    FREE(*destination);
		*destination = 0;
	  }

    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) MALLOC (length);
        if (*destination == NULL) 
	        return(NULL);
        memcpy(*destination, source, length);
      }
    return *destination;
}

/*	binary block Allocate and Concatenate
 *
 *   destination_length  is the length of the existing block
 *   source_length   is the length of the block being added to the 
 *   destination block
 */
char * 
system_BlockAllocCat (char **destination, 
		   size_t destination_length, 
		   const char *source, 
		   size_t source_length)
{
    if (source) 
	  {
        if (*destination) 
	      {
      	    *destination = (char *) REALLOC (*destination, destination_length + source_length);
            if (*destination == NULL) 
	          return(NULL);

            memmove (*destination + destination_length, source, source_length);

          } 
		else 
		  {
            *destination = (char *) MALLOC (source_length);
            if (*destination == NULL) 
	          return(NULL);

            memcpy(*destination, source, source_length);
          }
    }

  return *destination;
}



/* ---------------- time cache for performance ------------------- */

static time_t tc_last       = (time_t) -1;
static time_t tc_req_start  = (time_t) -1;
static time_t tc_req_finish = (time_t) -1;

void util_usleep(unsigned delay) {
  
    struct timeval tval;
    
    tval.tv_sec = delay / 1000000;
    tval.tv_usec = delay % 1000000;
    CACHETRACE(("Artificial sleep for %d secs and %d usecs\n",
		tval.tv_sec,tval.tv_usec));
    select(0, NULL, NULL, NULL, &tval);
  }

void util_time_req_reset(void)
{
    tc_last = tc_req_start = tc_req_finish = (time_t) -1;
}


time_t util_time_fresh(void)
{
    return time(&tc_last);
}


time_t util_time_last(void)
{
    return ((tc_last != (time_t)-1) ? tc_last : time(&tc_last));
}


time_t util_time_req_start(void)
{
    if (tc_req_start == (time_t)-1) {
	tc_req_start = util_time_last();	/* doesn't need to be fresh */
    }
    return tc_req_start;
}


time_t util_time_req_finish(void)
{
    if (tc_req_finish == (time_t)-1) {
	tc_req_finish = util_time_fresh();	/* yes, it'll need to be fresh */
    }
    return tc_req_finish;
}


void util_time_set_req_finish(time_t t)
{
#if 0
    /* Unfortunately, if the filesystem time is different from the clock
     * (in the rare case that it would actually be NFS mounted), this
     * would badly skew it.  Best to leave this optimization out.  Bummer.
     */
    tc_req_finish = tc_last = t;
#else
    tc_req_finish = util_time_fresh();
#endif
}



NSAPI_PUBLIC time_t util_make_gmt(time_t t)
{
    struct tm tms;
    long tz;

    if (t==(time_t)0)
	t = util_time_fresh();

    /*
     * Get time zone
     */
     util_localtime(&t, &tms);
#ifdef BSD_TIME
    tz = tms.tm_gmtoff;
#else
    tz = -timezone;
    if (tms.tm_isdst)
        tz += 3600;
#endif

    return t - tz;
}


NSAPI_PUBLIC time_t util_get_current_gmt(void)
{
    return util_make_gmt(util_time_last());
}


NSAPI_PUBLIC time_t util_make_local(time_t t)
{
    return t ? (t + (t - util_make_gmt(t))) : (time_t)0;
}


struct tm *_clf_gmtoff(time_t tt, long *tz)
{
    struct tm t;

    if (!tt)
	tt = util_time_last();

    util_localtime(&tt, &t);
#ifdef BSD_TIME
    *tz = t.tm_gmtoff;
#else
    *tz = - timezone;
    if(t.tm_isdst)
        *tz += 3600;
#endif
    return &t;
}


/*
 * Format the time to format that suits access logs.
 * If tt is (time_t)0, will get the current time.
 */
NSAPI_PUBLIC char *util_make_log_time(time_t tt)
{
    long timz;
    struct tm *t = _clf_gmtoff(tt, &timz);
    char tbuf[28], sign = (timz < 0 ? '-' : '+');
    char *ret = (char *)MALLOC(40);

    if(timz < 0) 
	timz = -timz;

    strftime(tbuf, 28, "%d/%b/%Y:%H:%M:%S", t);
    sprintf(ret, "[%s %c%02d%02d]", tbuf, sign,
	    (int)(timz/3600), (int)(timz%3600));
    return ret;
}


/* ----------------------- util_parse_http_time ---------------------- */


NSAPI_PUBLIC time_t util_parse_http_time(char *date_string)
{
    struct tm time_info;
    char  *ip;
    char   mname[256];
    time_t rv;

    memset(&time_info, 0, sizeof(struct tm));

    if (strlen(date_string) >= 256 || !(ip = strchr(date_string,' ')))
	return 0;

    while (isspace(*ip))
	ip++;

    if(isalpha(*ip)) 
      {
	  /*
	   * ANSI C's asctime() format:
	   * Sun Nov  6 08:49:37 1994
	   *
	   */
	  sscanf(ip, (strstr(ip, "DST") ? "%s %d %d:%d:%d %*s %d"
					: "%s %d %d:%d:%d %d"),
		 mname,
		 &time_info.tm_mday,
		 &time_info.tm_hour,
		 &time_info.tm_min,
		 &time_info.tm_sec,
		 &time_info.tm_year);
	  time_info.tm_year -= 1900;
      }
    else if(ip[2] == '-') 
      {
	  /*
	   * RFC 850 (normal HTTP), obsoleted by RFC 1036:
	   * Sunday, 06-Nov-94 08:49:37 GMT
	   *
	   */
	  char t[256];

	  sscanf(ip,"%s %d:%d:%d", t,
		 &time_info.tm_hour,
		 &time_info.tm_min,
		 &time_info.tm_sec);
	  t[2] = '\0';
	  time_info.tm_mday = atoi(t);
	  t[6] = '\0';
	  strcpy(mname,&t[3]);
	  time_info.tm_year = atoi(&t[7]);
	  /* Prevent wraparound from ambiguity */
	  if(time_info.tm_year < 70)
	      time_info.tm_year += 100;
      }
    else 
      {
	  /*
	   * RFC 822, updated by RFC 1123:
	   * Sun, 06 Nov 1994 08:49:37 GMT
	   *
	   */
	  sscanf(ip,"%d %s %d %d:%d:%d",&time_info.tm_mday,
		 mname,
		 &time_info.tm_year,
		 &time_info.tm_hour,
		 &time_info.tm_min,
		 &time_info.tm_sec);
	  time_info.tm_year -= 1900;
      }

#ifdef XXX_MCC_PROXY
// XXX _mstr2num is defined in base/util.c in the proxy code
    time_info.tm_mon = _mstr2num(mname);
#endif

    time_info.tm_mon = _mstr2num(mname);

    if(time_info.tm_mon == -1)   /* check for error */
	return (time_t)0;

    if ((rv = mktime(&time_info)) == (time_t)-1)
	return (time_t)0;

    if(time_info.tm_isdst)
	return rv - 3600;
    else
	return rv;
}


NSAPI_PUBLIC pb_param *pblock_nlinsert(char *name, long value, pblock *pb)
{
    if (name && pb) {
	char buf[20];
	sprintf(buf, "%ld", value);
	return pblock_nvinsert(name, buf, pb);
    }
    return NULL;
}


NSAPI_PUBLIC long pblock_findlong(char *name, pblock *pb)
{
    char *tmp = pblock_findval(name, pb);

    return tmp ? atol(tmp) : -1L;
}


/*
 * Replace the name of the pblock parameter, retaining the value.
 *
 */
NSAPI_PUBLIC void pblock_replace_name(char *oname, char *nname, pblock *pb)
{
    pb_param *pp = pblock_remove(oname, pb);

    if (pp) {
	FREE(pp->name);
	pp->name = STRDUP(nname);
	pblock_pinsert(pp, pb);
    }
}


void util_string_array_free(char **a)
{
    if (a) {
	int i;
	for (i=0; a[i]; i++)
	    FREE(a[i]);
	FREE(a);
    }
}


char *util_lowcase_strdup(char *str)
{
    char *rv, *p;

    if (!str) return NULL;

    rv = STRDUP(str);
    for(p=rv; *p; p++)	/* make sure it's all-lower-case */
	if (isupper(*p)) *p = tolower(*p);

    return rv;
}

int wildpat_header_match(char *hdr, char *regexp)
{
    int rv = 0;

    if (hdr && regexp) {
	char *p = strchr(hdr, ':');
	char saved = *hdr;

	*hdr = tolower(*hdr);
	if (p) *p = '\0';
	rv = !WILDPAT_CMP(hdr, regexp);
	if (p) *p = ':';
	*hdr = saved;
    }

    return rv;
}


char **util_string_array_parse_header(char *hdr_block)
{
    char saved, *p, *q, **a;
    int cnt = 0;

    /* Count the number of lines and allocate enough space */
    for(p=hdr_block; *p; p++) {
	if (*p == LF) cnt++;
    }
    a = (char **)CALLOC((cnt+2) * sizeof(char *));

    cnt = 0;
    for(p=q=hdr_block; *q; q++) {
	if (*q == LF) {
	    if (q-p==1 || ((p-q)==2 && *p==CR))
		break;		/* End of headers */
	    saved = *(++q);
	    *q = '\0';
	    a[cnt++] = STRDUP(p);
	    *q = saved;
	    p = q;
	}
    }
    a[cnt] = NULL;
    return a;
}


/*
 * Replace the headers in 'cache_hdrs' with ones from 'resp_hdrs',
 * but only the ones that match either 'regexp_relevant_1' or
 * or 'regexp_relevant_2' shell regexp.
 * Invalidate the headers in 'resp_hdrs' that were placed in
 * 'cache_hdrs'.
 */
void util_string_array_replace_headers(char **cache_hdrs,
				       LineNode *resp_hdrs,
				       char *regexp_relevant_1,
				       char *regexp_relevant_2)
{
    char **a, *p;
    LineNode *b;
    int len;

    if (!cache_hdrs || !resp_hdrs || (!regexp_relevant_1 && !regexp_relevant_2))
	return;

    for(a=cache_hdrs; a && *a; a++) {
	if ((p = strchr(*a, ':'))) {
	    len = (int)(p - *a + 1);
	    if (wildpat_header_match(*a, regexp_relevant_1) ||
		wildpat_header_match(*a, regexp_relevant_2)) {
		for(b=resp_hdrs; b; b=b->next) {
		    if (b->line && !strncasecmp(*a, b->line, len)) {
			FREE(*a);
			*a = STRDUP(b->line);
			b->line[0] = '\0';	/* Invalidate */
			break;
		    }
		}
	    }
	}
    }
}


/*
 * Append all the valid header lines in resp_hdrs to cache_hdrs,
 * except those matching either regexp_exclude_1 or regexp_exclude_2.
 *
 */
char **util_string_array_merge_hdrs(char **cache_hdrs,
				    LineNode *resp_hdrs,
				    char *regexp_exclude_1,
				    char *regexp_exclude_2,
				    int *total_length)
{
    char **a;
    LineNode *b;
    int a_len=0;
    int b_len=0;
    int tot = 0;
    int len = 0;
    char *p = NULL;

    if (total_length)
	*total_length = 0;

    if (!cache_hdrs)
	return NULL;

    for(a=cache_hdrs; *a; a++) {
	a_len++;
	tot += strlen(*a);
    }

    if (!resp_hdrs) {
	if (total_length)
	    *total_length = tot;
	return cache_hdrs;
    }

    for(b=resp_hdrs; b; b=b->next) {
	b_len++;
    }

    a = (char **)REALLOC(cache_hdrs, (a_len + b_len + 1)*sizeof(char*));
    for(b=resp_hdrs; b; b=b->next) {
	if (b->line && strchr(b->line, ':') &&
	    !wildpat_header_match(b->line, regexp_exclude_1) &&
	    !wildpat_header_match(b->line, regexp_exclude_2))
	  {
	      tot += len = strlen(b->line);
	      a[a_len++] = p = (char*) MALLOC(len + 1);
	      strcpy(p, b->line);
	  }
    }
    a[a_len] = NULL;

    if (total_length)
	*total_length = tot;

    return a;
}


/*
 * Returns 0 or 1 depending on whether or not URL references a
 * Fully Qualified Domain Name. Useful, because the proxy doesn't
 * cache non-FQDN's.
 */

NSAPI_PUBLIC int util_url_has_FQDN(char * url)
{
    char *h, *p1, *p2;

    if ( url && (h=strstr(url, "://")) ) {
        p1 = strchr( h, '.' );
        p2 = strchr( h+3, '/' );
        if( (p1 && p2 && p1<p2) || (p1 && !p2) )
            return 1;
        else
            return 0;
    } else {
        return 0;
    }
}

#endif // FEAT_PROXY

/*
 * Turns hostname in URL to all-lower-case, and removes redundant
 * port numbers, i.e. 80 for http, 70 for gopher and 21 for ftp.
 * Added 443 for https.
 * Modifies its parameter string directly.
 *
 * Also turns the protocol specifier to all lower-case.
 */

NSAPI_PUBLIC void util_url_fix_hostname(char * url)
{
    char *h, *g, *p;

    for(h=url; *h && *h != ':' && *h != '/'; h++)
	*h = tolower(*h);
    if (!strncmp(h, "://", 3))
      {
	  for(g = h + 3; *g && *g != '@' && *g != '/'; g++);
	  if (*g == '@')
	      h = g + 1;
	  else
	      h += 3;

	  while (*h && *h != ':' && *h != '/')
	    {
		*h = tolower(*h);
		h++;
	    }
	  if (*h == ':')
	    {
		int port = atoi(h+1);
		if ((port==80 && !strncmp(url,"http:",5)) ||
		    (port==70 && !strncmp(url,"gopher:",7)) ||
		    (port==21 && !strncmp(url,"ftp:",4)))
		  {
		      for(p = h+3; (*h = *p); h++, p++);
		  }
		else if (port==443 && !strncmp(url, "https:", 6))
		  {
		      for(p = h+4; (*h = *p); h++, p++);
		  }
	    }
      }
}


#ifdef FEAT_PROXY

/*
 * Compare two ULRs.
 * Return values as for strcmp().
 *
 */
NSAPI_PUBLIC int util_url_cmp(char *s1, char *s2)
{
    return strcmp(s1,s2);
}


/*
 * Check that the URL looks ok.
 *
 *
 */
int util_uri_check(char *uri)
{
    if (!uri)
	return 0;

    if (*uri == '/')
	return 1;

    if (!strncasecmp(uri, "http:",     5) ||
	!strncasecmp(uri, "https:",    6) ||
	!strncasecmp(uri, "ftp:",      4) ||
	!strncasecmp(uri, "gopher:",   7) ||
	!strncasecmp(uri, "connect:",  8)) {
	uri = strchr(uri, ':');
	return (!strncmp(uri, "://", 3));
    }

    return (strchr(uri, ':') != NULL);
}


NSAPI_PUBLIC int util_sterilize_hostname(char *host)
{
    int len = 0;
    char *p = host;

    if (!host)
	return 0;

    while (*p) {
	if (++len >= MAX_STERILIZED_HLEN || (!isalnum(*p) && !ispunct(*p))) {
	    *p = '\0';
	    ereport(LOG_SECURITY, "bad hostname truncated to %s", host);
	    return 0;
	}
	p++;
    }

    return 1;
}


char *make_into_complete_self_reference(char *path)
{
    char *new_url;
    char portstr[16];

    if (!path)
	return NULL;

    if ((path[0] && path[0] != '/') || !server_hostname)
	return STRDUP(path);

    if (!server_portnum ||
	(!security_active && server_portnum == 80) ||
	(security_active && server_portnum == 443))
	portstr[0] = '\0';
    else
	sprintf(portstr, ":%d", server_portnum);

    new_url = (char *)MALLOC(strlen(server_hostname) + strlen(path) + 32);

    sprintf(new_url, "http%s://%s%s%s",
	    security_active ? "s" : "",
	    server_hostname, portstr, path);

    return new_url;
}


char *make_self_ref_and_add_trail_slash(char *url)
{
    char *new_url = NULL;
    int len;

    if ((len = strlen(url)) > 0 && url[len-1] == '/')
	return NULL;		/* trailing slash already there */

    if (url[0] == '/') {	/* local path, make into full URL */
	if (!(new_url = make_into_complete_self_reference(url)))
	    return NULL;
    }
    else {			/* already full URL, just add slash */
	new_url = (char *)MALLOC(strlen(url) + 2);
	strcpy(new_url, url);
    }
    len = strlen(new_url);
    new_url[len++] = '/';	/* there's extra spc past end, see above */
    new_url[len] = '\0';

    return new_url;
}


int util_get_onoff_fuzzy(const char *str)
{
    if (!str)
	return -1;

    if (isdigit(str[0]))
	return (str[0] - '0');

    if (!strcasecmp(str, "on") ||
	!strcasecmp(str, "yes") ||
	!strcasecmp(str, "true") ||
	!strcasecmp(str, "enabled"))
	return 1;

    if (!strcasecmp(str, "off") ||
	!strcasecmp(str, "no") ||
	!strcasecmp(str, "false") ||
	!strcasecmp(str, "disabled"))
	return 0;

    return -1;
}


int util_get_onoff(const char *str)
{
    int rv = util_get_onoff_fuzzy(str);

    if (rv == -1) {
	ereport(LOG_WARN, "Bad value '%s' -- 'on' or 'off' expected", str);
	return 0;
    }

    return rv;
}

void replace_srvhdrs( Request *rq, pblock *resp_304_hdrs, char *rep_default, char *cache_replace)
{
 // replace the values that match rep_default or cache_replace in rq->srvhdrs with resp_304_hdrs

   if (!resp_304_hdrs) {
      return;
   }

       // resp_304_hdrs -- 304 response headers
       // rq->srvhdrs   -- hdrs from cache file

       char *expires = pblock_findval("expires", resp_304_hdrs);
       char *expires1 = pblock_findval("expires", rq->srvhdrs);

       if (expires && expires1) {
           pb_param *pp = pblock_remove("expires", rq->srvhdrs);
           if (pp)
               param_free(pp);
           pblock_nvinsert("expires", expires, rq->srvhdrs);
       }
}

void merge_srvhdrs(pblock *resp_304_hdrs)
{
  // append all the headers from resp_304_hdrs to rq->srvhdrs except nomerge ones
   // fi->rq->srvhdrs already has resp_304_hdrs and the cache file headers
   // Remove the following headers that should not be merged
   // Never attach these 304 response headers with 200 returned from cache
   // (mime-version|server|date|last-modified|content-type|content-length|content-encoding|content-language|content-transfer-encoding|content-md5)


   if (!resp_304_hdrs)
       return;

        pb_param *pp;
        pp = pblock_remove("mime-version", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("server", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("date", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("last-modified", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("content-type", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("content-length", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("content-encoding", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("content-language", resp_304_hdrs);
        if (pp)
            param_free(pp);
        pp = pblock_remove("content-transfer-encoding", resp_304_hdrs);
        if (pp)
           param_free(pp);
        pp = pblock_remove("content-md5", resp_304_hdrs);
        if (pp)
           param_free(pp);

}

#endif // FEAT_PROXY



/*
 * duplicates list of strings using pool_malloc
 * note that strlist is a list of char pointers terminated by a NULL
 */
NSAPI_PUBLIC char **util_strlist_pool_dup(char **strlist, pool_handle_t *pool)
{
    char **newstrlist = NULL;
    int len = 0, nstr = 0;
    char **ptr = NULL;

    if (!strlist)
        return NULL;

    // count and allocate storage for newstrlist
    for (nstr = 1, ptr = strlist; *ptr != 0; nstr++, ptr++);
    newstrlist = (char**) pool_malloc(pool, nstr*sizeof(char*));

    // copy strlist
    for (nstr = 0, ptr = strlist; *ptr != 0; nstr++, ptr++) {
        if (*ptr) {
            len = strlen(*ptr) + 1;
            newstrlist[nstr] = (char *) pool_malloc(pool, sizeof(char)*len);
            memcpy(newstrlist[nstr], *ptr, len);
        }
        else {
            newstrlist[nstr] = 0;
        }
    }
    newstrlist[nstr] = 0;

    return newstrlist;
}

/*
 * duplicates list of strings using PERM_MALLOC
 * note that strlist is a list of char pointers terminated by a NULL
 * pointer at the end
 * Note that the caller is responsible for freeing the memory
 * using util_strlist_free()
 */
NSAPI_PUBLIC char **util_strlist_dup(char **strlist)
{
    char **newstrlist = NULL;
    int len = 0, nstr = 0;
    char **ptr = NULL;

    if (!strlist)
        return NULL;

    // count and allocate storage for newstrlist
    for (nstr = 1, ptr = strlist; *ptr != 0; nstr++, ptr++);
    newstrlist = (char**) PERM_MALLOC(nstr*sizeof(char*));

    // copy strlist
    for (nstr = 0, ptr = strlist; *ptr != 0; nstr++, ptr++) {
        if (*ptr) {
            len = strlen(*ptr) + 1;
            newstrlist[nstr] = (char *) PERM_MALLOC(sizeof(char)*len);
            memcpy(newstrlist[nstr], *ptr, len);
        }
        else {
            newstrlist[nstr] = 0;
        }
    }
    newstrlist[nstr] = 0;

    return newstrlist;
}

/*
 * frees the memory allocated through util_strlist_dup()
 */
NSAPI_PUBLIC void util_strlist_free(char **strlist)
{
    char **ptr = NULL;
    int nstr = 0;

    if (!strlist)
        return;

    for (nstr = 0, ptr = strlist; *ptr != 0; nstr++, ptr++)
        PERM_FREE(strlist[nstr]);

    PERM_FREE(strlist);
}

NSAPI_PUBLIC int get_time_difference(Request *rq, const char *name1, const char *name2)
{
    char *v1 = NULL, *v2 = NULL;
    int rv = -1;

    if (name1 && name2)
    {
        v1 = pblock_findval(name1, rq->vars);
        v2 = pblock_findval(name2, rq->vars);

        if (v1 && v2)
        {
            int i1, i2;
 
            i1 = atoi(v1);
            i2 = atoi(v2);
        
            rv = i2 - i1;
        }
    }
    return rv;
}

int record_times(int res, Request *rq)
{
    int t, now = PR_IntervalToMilliseconds(PR_IntervalNow());
    char *v = pblock_findval("xfer-start", rq->vars);

    if (v)
    {
        t = atoi(v);
        pblock_nninsert("xfer-time", (now - t)/1000, rq->vars);
        pblock_nninsert("xfer-time-total", now - t, rq->vars);
    }

    t = get_time_difference(rq, "dns-start", "dns-end");
    if (t >= 0)
        pblock_nninsert("xfer-time-dns", t, rq->vars);

    t = get_time_difference(rq, "conn-start", "conn-end");
    if (t >= 0)
        pblock_nninsert("xfer-time-cwait", t, rq->vars);

    t = get_time_difference(rq, "iwait-start", "iwait-end");
    if (t >= 0)
        pblock_nninsert("xfer-time-iwait", t, rq->vars);

    t = get_time_difference(rq, "iwait-start", "fwait-end");
    if (t >= 0)
        pblock_nninsert("xfer-time-fwait", t, rq->vars);
    return res;
}
