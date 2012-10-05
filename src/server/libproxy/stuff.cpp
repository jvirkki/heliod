/**
 * PROPRIETARY/CONFIDENTIAL. Use of this product is subject to
 * license terms. Copyright © 2001 Sun Microsystems, Inc.
 * Some preexisting portions Copyright © 2001 Netscape Communications Corp.
 * All rights reserved.
 */
/*
 * stuff.c: Functions to be used in the proxy configuration.
 * 
 * These actually belong to their respective modules, ntrans.c
 * and pcheck.c, but they are here for now because they belong
 * only to the proxy version of the server.
 *
 *
 *
 * Ari Luotonen
 * Copyright (c) 1995 Netscape Communcations Corporation
 *
 */


#include "netsite.h"
#include "base64.h"
#include "base/shexp.h"
#include "base/regexp.h"
#include "base/session.h"
#include "base/cinfo.h"
#include "base/daemon.h"
#include "base/pool.h"
#include "base/util.h"            /* util_sprintf */
#include "frame/log.h"            /* log_error */
#include "frame/req.h"
#include "frame/http_ext.h"
#include "frame/http.h"
#include "libproxy/stuff.h"
#include "libproxy/httpclient.h"
#include "libproxy/util.h"

#ifndef CACHETRACE
#define CACHETRACE(s)
#endif


/* ----------------------- General::append-header ------------------------- */


int general_append_header2(const char *name, const char *value, Session *sn, Request *rq)
{
    pb_param *ptr = pblock_find("full-headers", rq->reqpb);
    if (ptr) {
        int len = 0;
        if (ptr->value)
            len = strlen(ptr->value);

        ptr->value = (char*) REALLOC(ptr->value, len + strlen(name) + strlen(value) + 5);
        sprintf(&ptr->value[len], "%s: %s%c%c", name, value, CR, LF);
        ptr->value[len] = toupper(ptr->value[len]);
    }

    pblock_nvinsert(name, value, rq->headers);

    return REQ_PROCEED;
}


int general_append_header(pblock *pb, Session *sn, Request *rq)
{
    char *name = pblock_findval("name", pb);
    char *value = pblock_findval("value", pb);

    if (!name || !value) {
        ereport(LOG_MISCONFIG, "set-header: requires both name and value");
        return REQ_NOACTION;
    }

    return general_append_header2(name, value, sn, rq);
}



/* ------------------- General::set-or-replace-header ---------------------- */


int general_set_or_replace_header2(const char *name, const char *value, Session *sn, Request *rq)
{
    pb_param *pp = pblock_find(name, rq->headers);
    if (pp) {
        FREE(pp->value);
        pp->value = STRDUP(value);
    }
    else {
        pblock_nvinsert(name, value, rq->headers);
    }

    pb_param *hdr_pp = pblock_find("full-headers", rq->reqpb);
    if (!hdr_pp)
        return REQ_PROCEED;

    int namlen = strlen(name);
    int vallen = strlen(value);

    char *hdrs = NULL;
    char *s, *t, *u, *v;
    int hdrlen = 0;
    if (hdr_pp->value)
        hdrlen = strlen(hdr_pp->value);
    char *search_string = (char *)MALLOC(namlen + 3);


    sprintf(search_string, "%c%s:", LF, name);

    hdrs = hdr_pp->value = (char*) REALLOC(hdr_pp->value, hdrlen + namlen + vallen + 6);

    if (!strncasecmp(hdrs, name, namlen) && hdrs[namlen] == ':') {
        s = hdrs + namlen + 1;
    }
    else if ((s = util_strcasestr(hdrs, search_string))) {
        s += namlen + 2;
    }
    else {
        s = NULL;
    }

    if (s) {
        if (isspace(*s)) s++;
        if ((t = strchr(s, CR))) {
            if (vallen > (int)(t - s)) {    /* need more spc */
                u = hdrs + hdrlen;
                v = hdrs + hdrlen + (vallen - (int)(t - s));
                while (u > t) {
                    *v-- = *u--;
                }
                strcpy(s, value);
                *v = CR;
            }
            else if (vallen < (int)(t - s)) { /* less spc */
                strcpy(s, value);
                u = t;
                v = s + vallen;
                while (*u) {
                    *v++ = *u++;
                }
                *v = '\0';
            }
            else {    /* exact same spc */
                strcpy(s, value);
                *t = CR;
            }
        }
    }
    else {
        sprintf(&hdrs[hdrlen], "%s: %s%c%c", name, value, CR, LF);
        hdrs[hdrlen] = toupper(hdrs[hdrlen]);
    }

    return REQ_PROCEED;
}


int general_set_or_replace_header(pblock *pb, Session *sn, Request *rq)
{
    char *name = pblock_findval("name", pb);
    char *value = pblock_findval("value", pb);

    if (!name || !value) {
        ereport(LOG_MISCONFIG, "replace-header: requires both name and value");
        return REQ_NOACTION;
    }

    return general_set_or_replace_header2(name, value, sn, rq);
}



/* --------------------- General::fix-host-header ------------------------ */


int general_set_host_header(const char *host, Session *sn, Request *rq)
{
    return general_set_or_replace_header2("host", host, sn, rq);
}


int general_set_host_header_from_url(char *url, Session *sn, Request *rq)
{
    char *p, *q;
    char *host = NULL;
    char saved;
    int rv;

    if (!url || !url[0] || url[0] == '/' || !(p = strstr(url, "://")) ||
	!strncmp(url, "file:", 5) ||
	!strncmp(url, "ftp:", 4) ||
	!strncmp(url, "gopher:", 7) ||
	!strncmp(url, "connect:", 8))
      {
	  return REQ_NOACTION;
      }

    p += 3;
    for(q=p; *q && *q != '/'; q++);
    saved = *q;
    *q = '\0';
    rv = general_set_host_header(p, sn, rq);
    *q = saved;

    return rv;
}


int general_fix_host_header(pblock *pb, Session *sn, Request *rq)
{
    char *url = pblock_findval("escaped", rq->vars);
    if (!url)
        url = pblock_findval("uri", rq->reqpb);

    return general_set_host_header_from_url(url, sn, rq);
}


/* ------------------------- NameTrans::pac-map ---------------------------- */
/*
    CJN - God this routine sucks - I had to add it as a hack to  
    handle reverse proxying as well as multiple PAC files. The hack 
    basically says, if the request is an exact match to "from", then
    assume it's to return REQ_PROCEED (don't do the rest of the NameTrans).
    Since we never act as a server, this is OK.
 */
#ifdef FEAT_PROXY
int ntrans_pac_map(pblock *pb, Session *sn, Request *rq)
{
    char *t = NULL;
    char *from   = pblock_findval("from", pb);
    char *to     = pblock_findval("to", pb);
    pb_param *pp = pblock_find("ppath", rq->vars);
    pb_param *es = pblock_find("escaped", rq->vars);
    char* pauth  = pblock_findval("pauth", pb);

    if (!es)
        es = pblock_find("uri", rq->reqpb);

    CACHETRACE(("*** PAC MAPPING es->value %s pp->value %s from %s to %s\n", es->value, pp->value, from, to));

    if( !pp || strcmp( es->value, from ) )
        return REQ_NOACTION;

    if ((!from) || (!to)) {
	log_error(LOG_MISCONFIG, "pac-map", sn, rq,
		  "missing parameter for \"pac-map\" (need \"from\" and \"to\")");
	return REQ_ABORTED;
    }

    /*
     * Free both the escaped and unescaped versions
     */
    FREE(pp->value);
    FREE(es->value);

    CACHETRACE(("*** MAPPING \"%s\" TO \"%s\"\n", from, to));
    /*
     * t is the escaped version, and from it we unescape the other
     * one that will be used in further translations.
     */
    t = STRDUP(to);
    es->value = STRDUP(t);
    util_uri_unescape(t);
    pp->value = t;

    if( (t = pblock_findval("name", pb)) )
        pblock_nvinsert("name", t, rq->vars);
      
    /* access control for pac is off by default */
    if (!pauth || (strcmp(pauth, "on")  && strcmp(pauth, "yes"))) 
        pblock_nvinsert("disable-pauth", "1", rq->vars);

    return REQ_PROCEED;
}
#endif // FEAT_PROXY


/* ------------------------- NameTrans::pat-map ---------------------------- */

#ifdef FEAT_PROXY
int ntrans_pat_map(pblock *pb, Session *sn, Request *rq)
{
    char *t = NULL;
    char *from   = pblock_findval("from", pb);
    char *to     = pblock_findval("to", pb);
    pb_param *pp = pblock_find("ppath", rq->vars);
    pb_param *es = pblock_find("escaped", rq->vars);
    char *pauth  = pblock_findval("pauth", pb);

    if (!es)
        es = pblock_find("uri", rq->reqpb);

    CACHETRACE(("*** PAT MAPPING es->value %s pp->value %s from %s to %s\n", es->value
, pp->value, from, to));

        /* >> cacheable unless mapping is necessary */
        rq->directive_is_cacheable = 1;

    if( !pp || strcmp( es->value, from ) )
        return REQ_NOACTION;

    if ((!from) || (!to)) {
        log_error(LOG_MISCONFIG, "pat-map", sn, rq,
                  "missing parameter for \"pat-map\" (need \"from\" and \"to\")");
        return REQ_ABORTED;
    }

    /*
     * Free both the escaped and unescaped versions
     */
    FREE(pp->value);
    FREE(es->value);

    CACHETRACE(("*** MAPPING \"%s\" TO \"%s\"\n", from, to));
    /*
     * t is the escaped version, and from it we unescape the other
     * one that will be used in further translations.
     */
    t = STRDUP(to);
    es->value = STRDUP(t);
    util_uri_unescape(t);
    pp->value = t;

    if( (t = pblock_findval("name", pb)) )
        pblock_nvinsert("name", t, rq->vars);

    /* access control for pat is off by default */
    if (!pauth || (strcmp(pauth, "on")  && strcmp(pauth, "yes"))) 
        pblock_nvinsert("disable-pauth", "1", rq->vars);

    return REQ_PROCEED;
}
#endif // FEAT_PROXY


/* ------------------------- NameTrans::regexp-map ---------------------------- */
/*
    Like pac-map, which looks for an exact match, but this does a
    regular expression match. Could be used for masking off sections of
    a server.
 */
int ntrans_regexp_map(pblock *pb, Session *sn, Request *rq)
{
    char *t = NULL;
    char *from   = pblock_findval("from", pb);
    char *to     = pblock_findval("to", pb);
    pb_param *pp = pblock_find("ppath", rq->vars);
    pb_param *es = pblock_find("escaped", rq->vars);

    if(!pp) 
	return REQ_NOACTION;

    if (!es)
        es = pblock_nvinsert("escaped", util_uri_escape(NULL, pp->value), rq->vars);


    if ((!from) || (!to)) {
	log_error(LOG_MISCONFIG, "regexp-map", sn, rq,
		  "missing parameter for \"regexp-map\" (need \"from\" and \"to\")");
	return REQ_ABORTED;
    }

    CACHETRACE(("*** REG MAPPING es->value %s pp->value %s from %s to %s\n", es->value, pp->value, from, to));

    if (!strncmp(from, "connect://", 10))
        from += 10;


    if( WILDPAT_CMP(es->value, from) )
	return REQ_NOACTION;


    /*
     * Free both the escaped and unescaped versions
     */
    FREE(pp->value);
    FREE(es->value);

    CACHETRACE(("*** MAPPING \"%s\" TO \"%s\"\n", from, to));


    /*
     * t is the escaped version, and from it we unescape the other
     * one that will be used in further translations.
     */
    t = STRDUP(to);

    /*
     * Fix the Host: header to point to the new map destination.
     *
     */
    if (util_getboolean(pblock_findval("rewrite-host", pb), PR_TRUE))
        general_set_host_header_from_url(to, sn, rq);

    es->value = STRDUP(t);
    util_uri_unescape(t);
    pp->value = t;

    if( (t = pblock_findval("name", pb)) )
        pblock_nvinsert("name", t, rq->vars);

    return REQ_PROCEED;
}



/* ------------------------- NameTrans::map ---------------------------- */


/*
 * This behaves somewhat like ntrans_pfx2dir() except that:
 *  * actual mapping is done in the escaped version, and a new
 *    unescaped version is derived from it for furher checking;
 *    in the end, netlib will use the escaped version
 *  * the return value is REQ_NOACTION instead of REQ_PROCEED to
 *    allow multiple name mappings
 *
 */
int ntrans_map(pblock *pb, Session *sn, Request *rq)
{
    register int x, d;
    register char *r;
    char *t, *u;
    int fl;
    char *from   = pblock_findval("from", pb);
    char *to     = pblock_findval("to", pb);
    pb_param *pp = pblock_find("ppath", rq->vars);
    pb_param *es = pblock_find("escaped", rq->vars);
    int is_connect_remap = 0;

    if (!pp)
        return REQ_NOACTION;

    if (!es)
        es = pblock_nvinsert("escaped", util_uri_escape(NULL, pp->value), rq->vars);


    if ((!from) || (!to)) {
	log_error(LOG_MISCONFIG, "map", sn, rq,
		  "missing parameter for \"map\" (need \"from\" and \"to\")");
	return REQ_ABORTED;
    }


    if (!strncmp(from, "connect:", 8))
	is_connect_remap = 1;

    fl = strlen(from);
    u = pp->value;

    if (fl > 0 && from[fl-1] == '/') {
	from[--fl] = '\0';
    }

    /* The second or clause is to prevent partials from shadowing.
       That is, if you have a cgi-bin directory and cgi-bin-sucks.html, 
       cgi-bin-sucks.html is not hidden by the mapping. Side effect is that 
       trailing slashes on 'from' or 'to' causes problems.
       -- Hmmm, I tried fixing that side effect -- let's see if it works.
    */
    if (is_connect_remap)
      {
	  if ((fl > 0 && strncmp(from, u, fl)) ||
	      (u[fl] && u[fl] != ':' && u[fl] != ':'))
	    {
		return REQ_NOACTION;
	    }
      }
    else if ((fl > 0 && strncmp(from, u, fl)) || (u[fl] && u[fl] != '/'))
      {
	  return REQ_NOACTION;
      }

    /*
     * On exact match (where the matched pattern never has a trailing
     * slash!), cause an auto-redirect to the original URL, adding the
     * trailing slash.
     *
     */
    if (fl > 0 && !strcmp(from, u) && !is_connect_remap) {
	char *orig_url = pblock_findval("uri", rq->reqpb);
	char *new_url;
	int len;

	if (orig_url && (len = strlen(orig_url)) > 0 &&
	    orig_url[len-1] != '/' &&
#ifdef FEAT_PROXY
	    (new_url = make_self_ref_and_add_trail_slash(orig_url)))
#else
	    (new_url = protocol_uri2url_dynamic(orig_url, "/", sn, rq)))
#endif
        {
	    protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
	    pblock_nvinsert("url", new_url , rq->vars);

	    FREE(new_url);

	    return REQ_ABORTED;
	}
    }

    CACHETRACE(("*** MAPPING FROM \"%s\" TO \"%s\"; es->value: \"%s\"\n", from, to, es->value));

    /*
     * r is the remainder of the escaped string that will be appended
     * to the new prefix.  It is originally the entire escaped string,
     * and then we skip fl characters (the length of the *unescaped*
     * prefix that matched the *unescaped* version of this string).
     */
    for(d=0, r=es->value; d < fl && *r; d++, r++) {
        if (*r == '%' && r[1] && r[2]) {
            r += 2;
	}
    }

    /*
     * Allocate and construct new string
     */
    t = (char *) MALLOC(strlen(to) + strlen(r) + 2);
    for(x = 0; (t[x] = to[x]); x++);
    if (x > 0) {
	if (!is_connect_remap) {
	    if ( to[x-1] != '/') {
		/* make sure there's a separating slash... */
		if (*r != '/') {
		    t[x++] = '/';
		}
	    }
	    else {
		/* ...and that there aren't multiple slashes */
		if (*r == '/') {
		    r++;
		}
	    }
	}
    }
    for( ; (t[x] = *r); x++, r++);

    if (is_connect_remap) {
	while (t[--x] == '/')
	    t[x] = '\0';
    }

    /*
     * Free both the escaped and unescaped versions
     */
    FREE(u);
    FREE(es->value);

    CACHETRACE(("*** MAPPING AFTER APPEND \"%s\"\n", t));


    /*
     * Fix the Host: header to point to the new map destination.
     *
     */
    if (util_getboolean(pblock_findval("rewrite-host", pb), PR_TRUE))
        general_set_host_header_from_url(t, sn, rq);


    /*
     * t is the escaped version, and from it we unescape the other
     * one that will be used in further translations.
     */
    es->value = STRDUP(t);
    util_uri_unescape(t);
    pp->value = t;

    if( (t = pblock_findval("name", pb)) )
        pblock_nvinsert("name", t, rq->vars);

    if (util_getboolean(pblock_findkeyval(pb_key_cont, pb), PR_FALSE))
	return REQ_NOACTION;	/* Fool server into continuing translation */
    else
	return REQ_PROCEED;	/* Stop mappings on match */
}


/* ----------------- NameTrans fn=regexp-redirect -------------------- */


int ntrans_regexp_redirect(pblock *pb, Session *sn, Request *rq)
{
    char *ppath = pblock_findval("ppath", rq->vars);
    char *from = pblock_findval("from", pb);
    char *url = pblock_findval("url", pb);
    char *escape = pblock_findval("escape", pb);

    if (!from || !url) {
        log_error(LOG_MISCONFIG, "regexp-redirect", sn, rq, 
                  "missing parameter (need from, url)");
        return REQ_ABORTED;
    }

    if (!ppath || WILDPAT_CMP(ppath, from))
	return REQ_NOACTION;

    protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);

    if (util_getboolean(escape, PR_TRUE))
        url = util_url_escape(NULL, url);

    pblock_nvinsert("location", url, rq->srvhdrs);

    return REQ_ABORTED;
}


/* ------------------------ NameTrans::host-map ------------------------ */

/*
 * Map a URI to an URL based on the value of the Host: header
 */

#ifdef FEAT_PROXY
static PRBool prepend_host(pool_handle_t *pool, const char *prefix, int prefix_len, const char *host, int host_len, pb_param *pp)
{
    if (!pp || *pp->value != '/')
        return PR_FALSE;

    int pp_value_len = strlen(pp->value);
    int url_len = prefix_len + host_len + pp_value_len;
    char *url = (char *)pool_malloc(pool, url_len + 1);
    if (!url)
        return PR_FALSE;

    int pos = 0;
    memcpy(url + pos, prefix, prefix_len);
    pos += prefix_len;
    memcpy(url + pos, host, host_len);
    pos += host_len;
    memcpy(url + pos, pp->value, pp_value_len);
    pos += pp_value_len;
    url[pos] = '\0';

    pool_free(pool, pp->value);

    pp->value = url;

    return PR_TRUE;
}

int ntrans_host_map(pblock *pb, Session *sn, Request *rq)
{
    const char *host = pblock_findkeyval(pb_key_host, rq->headers);
    if (!host)
        return REQ_NOACTION;

    int host_len = strlen(host);

    const char *url_prefix = pblock_findkeyval(pb_key_url_prefix, pb);
    int url_prefix_len;
    if (url_prefix) {
        url_prefix_len = strlen(url_prefix);
    } else if (GetSecurity(sn)) {
        url_prefix = "https://";
        url_prefix_len = 8;
    } else {
        url_prefix = "http://";
        url_prefix_len = 7;
    }

    int changes = 0;

    if (pb_param *escaped = pblock_findkey(pb_key_escaped, rq->vars)) {
        changes += prepend_host(sn->pool,
                                url_prefix, url_prefix_len,
                                host, host_len,
                                escaped);
    }

    if (pb_param *ppath = pblock_findkey(pb_key_ppath, rq->vars)) {
        changes += prepend_host(sn->pool,
                                url_prefix, url_prefix_len,
                                host, host_len,
                                ppath);
    }

    if (util_getboolean(pblock_findkeyval(pb_key_cont, pb), PR_FALSE))
        return REQ_NOACTION;

    return changes ? REQ_PROCEED : REQ_NOACTION;
}
#endif // FEAT_PROXY


/* ---------------------- PathCheck::deny-service ---------------------- */

#ifdef FEAT_PROXY
int pcheck_deny_service(pblock *pb, Session *sn, Request *rq)
{
    pb_param *path = pblock_find("path", rq->vars);
    char *regexp = pblock_findval("path", pb);

    CACHETRACE(("*** DENY-SERVICE TEST \"%s\" <-> \"%s\"\n",
		path->value, regexp ? regexp : "-NONE-"));

    if((!regexp) || (!WILDPAT_CMP(path->value, regexp))) {
        log_error(LOG_SECURITY, "deny-service", sn, rq, 
                  "denying service of %s", path->value);
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN,
			"Proxy denies fulfilling the request");
        return REQ_ABORTED;
    }
    else {
	CACHETRACE(("*** PASSED DENY-SERVICE TEST\n"));
	return REQ_NOACTION;
    }
}
#endif // FEAT_PROXY


/* -------------------- PathCheck::url-check --------------------- */

#ifdef FEAT_PROXY
int pcheck_url_check(pblock *pb, Session *sn, Request *rq)
{
    char *url        = pblock_findval("path", rq->vars);
    char *method     = pblock_findval("method", rq->reqpb);

    if (ISMOPTIONS(rq) || ISMTRACE(rq))
        return REQ_PROCEED;

    if (util_uri_check(url))
	return REQ_PROCEED;
    else {
        log_error(LOG_SECURITY, "url-check", sn, rq, 
                  "malformed URL: %s", url ? url : "-");
        protocol_status(sn, rq, PROTOCOL_BAD_REQUEST,
			"Malformed URL");
        return REQ_ABORTED;
    }
}
#endif // FEAT_PROXY


/* -------------------- PathCheck::user-agent-check --------------------- */

#ifdef FEAT_PROXY
int pcheck_user_agent_check(pblock *pb, Session *sn, Request *rq)
{
    char *pat = pblock_findval("ua", pb);
    char *ua  = pblock_findval("user-agent", rq->headers);
    char *bu  = pblock_findval("batch-update", rq->vars);

    if (bu) {
#if 0
	ereport(LOG_INFORM, "user-agent-check overridden for batch update");
#endif
	return REQ_NOACTION;
    }

    if (!pat)
#ifdef USE_REGEX
	pat = "Mozilla/.*";
#else
	pat = "Mozilla/*";   /* don't delete this comment; fixes NEC compiler bug */
#endif
    if (!ua)
	ua = "unspecified";

    if (WILDPAT_CMP(ua, pat)) {
	log_error(LOG_SECURITY, "user-agent-check", sn, rq,
		  "client program not permitted (%s)", ua);
	protocol_status(sn, rq, PROTOCOL_FORBIDDEN,
			"Proxy does not allow the use of your client program");
	return REQ_ABORTED;
    }
    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::cache-enable -------------------- */

#ifdef FEAT_PROXY
int otype_cache_enable(pblock *pb, Session *sn, Request *rq)
{
    pb_param *auth  = pblock_find("cache-auth",   rq->vars);
    pb_param *local = pblock_find("cache-local",  rq->vars);
    pb_param *https = pblock_find("cache-https",  rq->vars);
    pb_param *qml   = pblock_find("query-maxlen", rq->vars);
    pb_param *mns   = pblock_find("min-size",     rq->vars);
    pb_param *mxs   = pblock_find("max-size",     rq->vars);
    pb_param *rep   = pblock_find("log-report",   rq->vars);
    char *v;

    if (!pblock_find("cache-enable", rq->vars))
	pblock_nvinsert("cache-enable", "1", rq->vars);

    if (!auth && (v = pblock_findval("cache-auth", pb)))
	pblock_nvinsert("cache-auth", v, rq->vars);

    if (!local && (v = pblock_findval("cache-local", pb)))
	pblock_nvinsert("cache-local", v, rq->vars);

    if (!https && (v = pblock_findval("cache-https", pb)))
	pblock_nvinsert("cache-https", v, rq->vars);

    if (!qml && (v = pblock_findval("query-maxlen", pb)))
	pblock_nvinsert("query-maxlen", v, rq->vars);

    /* min-size is in KB */
    if (!mns && (v = pblock_findval("min-size", pb)))
	pblock_nvinsert("min-size", v, rq->vars);

    /* max-size is in KB */
    if (!mxs && (v = pblock_findval("max-size", pb)))
	pblock_nvinsert("max-size", v, rq->vars);

    if (!rep && (v = pblock_findval("log-report", pb)))
	pblock_nvinsert("log-report", v, rq->vars);

    return REQ_PROCEED;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::cache-disable ------------------- */

#ifdef FEAT_PROXY
int otype_cache_disable(pblock *pb, Session *sn, Request *rq)
{
    if (!pblock_find("cache-enable", rq->vars))
	pblock_nvinsert("cache-enable", "0", rq->vars);

    return REQ_PROCEED;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::cache-setting ------------------- */

/*
 * Does not override anything, only fills in undefined values
 *
 */
#ifdef FEAT_PROXY
int otype_cache_setting(pblock *pb, Session *sn, Request *rq)
{
    pb_param *muc = pblock_find("max-uncheck",   rq->vars);
    pb_param *lmf = pblock_find("lm-factor",     rq->vars);
    pb_param *tp  = pblock_find("term-percent",  rq->vars);
    pb_param *ce  = pblock_find("cover-errors",  rq->vars);
    pb_param *cm  = pblock_find("connect-mode",  rq->vars);
    pb_param *ex  = pblock_find("cache-exclude", rq->vars);
    pb_param *rp  = pblock_find("cache-replace", rq->vars);
    pb_param *nm  = pblock_find("cache-no-merge",rq->vars);
    char *v;

    if (!muc && (v = pblock_findval("max-uncheck", pb)))
	pblock_nvinsert("max-uncheck", v, rq->vars);

    if (!lmf && (v = pblock_findval("lm-factor", pb)))
	pblock_nvinsert("lm-factor", v, rq->vars);

    if (!tp  && (v = pblock_findval("term-percent", pb)))
	pblock_nvinsert("term-percent", v, rq->vars);

    if (!ce  && (v = pblock_findval("cover-errors", pb)))
	pblock_nvinsert("cover-errors", v, rq->vars);

    if (!cm  && (v = pblock_findval("connect-mode", pb)))
	pblock_nvinsert("connect-mode", v, rq->vars);

    if (!ex  && (v = pblock_findval("exclude", pb)))
	pblock_nvinsert("cache-exclude", v, rq->vars);

    if (!rp  && (v = pblock_findval("replace", pb)))
	pblock_nvinsert("cache-replace", v, rq->vars);

    if (!nm  && (v = pblock_findval("no-merge", pb)))
	pblock_nvinsert("cache-no-merge", v, rq->vars);

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::ftp-config ------------------- */

#ifdef FEAT_PROXY
int otype_ftp_config(pblock *pb, Session *sn, Request *rq)
{
    pb_param *mode = pblock_find("ftp-mode", rq->vars);
    char *v;

    if (!mode && (v = pblock_findval("mode", pb)))
	pblock_nvinsert("ftp-mode", v, rq->vars);

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::http-config ------------------- */

#ifdef FEAT_PROXY
int otype_http_config(pblock *pb, Session *sn, Request *rq)
{
    pb_param *keep_alive_ok = pblock_find("keep-alive-ok", rq->vars);
    char *v;

    if (!keep_alive_ok &&
	(v = pblock_findval("keep-alive", pb)) &&
	(!strcmp(v, "on") || !strcmp(v, "1"))) {
	pblock_nvinsert("keep-alive-ok", "1", rq->vars);
    } else {
        KEEP_ALIVE(rq);
    }

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::forward-ip ------------------- */

#ifdef FEAT_PROXY
int otype_forward_ip(pblock *pb, Session *sn, Request *rq)
{
    pb_param *fwd_ip = pblock_find("fwd-ip", rq->vars);
    char *v;

    if (!fwd_ip) {
	pblock_nvinsert("fwd-ip", "1", rq->vars);
	if ((v = pblock_findval("hdr", pb)))
	    pblock_nvinsert("fwd-ip-hdr", v, rq->vars);
    }

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::block-ip ------------------- */

#ifdef FEAT_PROXY
int otype_block_ip(pblock *pb, Session *sn, Request *rq)
{
#if 0
    pb_param *fwd_ip = pblock_find("fwd-ip", rq->vars);
    char *v;

    if (!fwd_ip) {
	pblock_nvinsert("fwd-ip", "1", rq->vars);
	if ((v = pblock_findval("hdr", pb)))
	    pblock_nvinsert("fwd-ip-hdr", v, rq->vars);
    }
#else
    pblock_nvinsert("fwd-ip", "0", rq->vars);
#endif

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* --------------------- ObjectType::java-ip-check ------------------- */

#ifdef FEAT_PROXY
int otype_java_ip_check(pblock *pb, Session *sn, Request *rq)
{
    pb_param *jip = pblock_find("java-ip-check", rq->vars);
    const char *v;

    if (!jip) {
        v = pblock_findval("status", pb);
        if (!v)
            v = "1";
        pblock_nvinsert("java-ip-check", v, rq->vars);
        httpclient_set_dest_ip(sn, rq, (util_get_onoff_fuzzy(v) == 1));
    }

    return REQ_PROCEED;
}
#endif // FEAT_PROXY


/* -------------------- ObjectType::set-basic-auth ------------------- */

int otype_set_basic_auth(pblock *pb, Session *sn, Request *rq)
{
    const char *user = pblock_findkeyval(pb_key_user, pb);
    if (!user)
        user = "";

    const char *password = pblock_findkeyval(pb_key_password, pb);
    if (!password)
        password = "";

    char *user_colon_password = (char *)pool_malloc(sn->pool,
                                                    strlen(user) + 1 +
                                                    strlen(password) + 1);
    if (!user_colon_password)
        return REQ_ABORTED;

    strcpy(user_colon_password, user);
    strcat(user_colon_password, ":");
    strcat(user_colon_password, password);

    char *b64 = BTOA_DataToAscii((unsigned char *)user_colon_password,
                                 strlen(user_colon_password));
    if (b64) {
        // Remove CRs/LFs per RFC 2617
        char *in = b64;
        char *out = b64;
        while (*in) {
            if (*in != '\r' && *in != '\n') 
                *out++ = *in;
            in++;
        }
        *out = '\0';

        char *value = (char *)pool_malloc(sn->pool,
                                          strlen("Basic ") +
                                          strlen(b64) + 1);
        if (value) {
            strcpy(value, "Basic ");
            strcat(value, b64);

            const char *hdr = pblock_findkeyval(pb_key_hdr, pb);
            if (!hdr)
                hdr = "authorization";
            general_set_or_replace_header2(hdr, value, sn, rq);

            pool_free(sn->pool, value);
        }

        PORT_Free(b64);
    }

    pool_free(sn->pool, user_colon_password);

    return REQ_PROCEED;
}


/* ----------------------- Init fn=load-types ------------------------ */

#ifdef FEAT_PROXY
int proxy_otype_init(pblock *pb, Session *sn, Request *rq)
{
    char *smt = pblock_findval("mime-types", pb);
    char *lmt = pblock_findval("local-types", pb);
    char *err;

    cinfo_init();
    daemon_atrestart((void (*)(void *))cinfo_terminate, NULL);

    /*
     * Initialize also netlib formats; this is stupid, grrr...
     */
    NET_InitFileFormatTypes(lmt, smt);

    if((err = cinfo_merge(smt))) {
        pblock_nvinsert("errors", err, pb);
        return REQ_ABORTED;
    }
    if(lmt && (err = cinfo_merge(lmt))) {
        pblock_nvinsert("errors", err, pb);
        return REQ_ABORTED;
    }
    return REQ_PROCEED;
}
#endif // FEAT_PROXY

/* ----------------------- DNS fn=dns-config ------------------------- */

#ifdef FEAT_PROXY
int dns_config(pblock *pb, Session *sn, Request *rq)
{
    char *dots   = pblock_findval("local-domain-levels", pb);
    char *domain = pblock_findval("local-domain-name", pb);

    if (dots)
	pblock_nvinsert("local-domain-levels", dots, rq->vars);
    if (domain)
	pblock_nvinsert("local-domain-name", domain, rq->vars);

    return REQ_NOACTION;
}
#endif // FEAT_PROXY


/* ----------------- PathCheck fn=block-multipart-posts -------------- */

/*
 * PathCheck fn=block-multipart-posts
 *	blocks all requests with any multipart content-type
 *
 * PathCheck fn=block-multipart-hosts
 *	user-agent="<ua-regex>"
 *	content-type="<ct-regex>"
 *	method="<method-regex>"
 *
 * blocks requests with:
 *	method matching <method-regex>,
 *	content-type matching <ct-regex>, and
 *	user-agent matching <ua-regex> (or missing).
 *
 */
#ifdef FEAT_PROXY
int pcheck_block_multipart_posts(pblock *pb, Session *sn, Request *rq)
{
    char *ua = pblock_findval("user-agent", rq->headers);
    char *ct = pblock_findval("content-type", rq->headers);
    char *md = pblock_findval("method", rq->reqpb);

    char *ua_re = pblock_findval("user-agent", pb);
    char *ct_re = pblock_findval("content-type", pb);
    char *md_re = pblock_findval("method", pb);

    if (!ct)
	return REQ_NOACTION;

    if (ua && ua_re && WILDPAT_CMP(ua, ua_re))
	return REQ_NOACTION;

    if (md && md_re && WILDPAT_CMP(md, md_re))
	return REQ_NOACTION;

    if (ct_re) {
	if (!WILDPAT_CMP(ct, ct_re)) {
	    log_error(LOG_SECURITY, "block-multipart-posts", sn, rq,
		      "request body content-type %s blocked", ct);
	    protocol_status(sn, rq, PROTOCOL_FORBIDDEN,
			    "Proxy does not allow requests with that content-type!");
	    return REQ_ABORTED;
	}
    }
    else {
	if (!strncasecmp(ct, "multipart/", 10)) {
	    log_error(LOG_SECURITY, "block-multipart-posts", sn, rq,
		      "request body content-type %s blocked", ct);
	    protocol_status(sn, rq, PROTOCOL_FORBIDDEN,
			    "Proxy does not allow file uploads!");
	    return REQ_ABORTED;
	}
    }

    return REQ_PROCEED;
}
#endif // FEAT_PROXY

