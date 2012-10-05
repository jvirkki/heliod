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
 * index.c: Handle crufty old NCSA directory indexing
 * 
 * Rob McCool
 */


#include "netsite.h"

#include "safs/index.h"
#include "base/session.h"
#include "frame/req.h"
#include "base/pblock.h"
#include "frame/log.h"
#include "frame/protocol.h"
#include "frame/httpfilter.h"
#include "base/cinfo.h"
#include "base/util.h"
#include "base/shexp.h"

#include <errno.h>

#include <sys/stat.h>
#include "safs/dbtsafs.h"

/* The granularity of our buffer of directory entries. */
#define NUM_ENTRIES 64

#define PATH_BUF_MAX 4096


/* -------------------------- Support functions --------------------------- */


/* This was a macro */
void FREE_AR(char **ar)
{
    register int x;

    for(x = 0; ar[x]; x++)
        FREE(ar[x]);
    FREE(ar);
}


extern "C" int _dumbsort(const void *s1, const void *s2)
{
    return strcmp(*((char **)s1), *((char **)s2));
}


char **_dir_ls(char *path, Session *sn, Request *rq)
{
    char **ar;
    SYS_DIR ds;
    SYS_DIRENT *d;
    int n, p;

    n = NUM_ENTRIES;
    p = 0;

    ar = (char **) MALLOC(n * sizeof(char *));

    if(!(ds = dir_open(path))) {
        log_error(LOG_WARN, "index-simple", sn, rq, 
                  XP_GetAdminStr(DBT_indexerror1), path, system_errmsg());
        protocol_status(sn, rq, (file_notfound() ? PROTOCOL_NOT_FOUND : 
                                                   PROTOCOL_FORBIDDEN), NULL);
        return NULL;
    }
    while( (d = dir_read(ds)) ) {
        if(d->d_name[0] != '.') {
            if(p == (n-1)) {
                n += NUM_ENTRIES;
                ar = (char **) REALLOC(ar, n*sizeof(char *));
            }
            /* 2: Leave space to add a trailing slash later */
            int len = strlen(d->d_name);
            if (len > PATH_BUF_MAX - 2)
                len = PATH_BUF_MAX - 2;
            ar[p] = (char *) MALLOC(len + 2);
            memcpy(ar[p], d->d_name, len);
            ar[p][len] = '\0';
            p++;
        }
    }

    dir_close(ds);

    qsort((void *)ar, p, sizeof(char *), _dumbsort);
    ar[p] = NULL;

    return ar;
}


/* ----------------------------- index_simple ----------------------------- */


int index_simple(pblock *pb, Session *sn, Request *rq)
{
    char **ar;
    int x, l;
    char c, *path = pblock_findval("path", rq->vars);
    char *uri = pblock_findval("uri", rq->reqpb);
    char buf[PATH_BUF_MAX], buf2[3*PATH_BUF_MAX];
    int bytes;

    bytes = 0;


    char* query = pblock_findval("query", rq->reqpb);

    if(!(ar = _dir_ls(path, sn, rq)))
        return REQ_ABORTED;

    httpfilter_buffer_output(sn, rq, PR_TRUE);

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);

    switch(protocol_start_response(sn, rq)) {
      case REQ_NOACTION:
        FREE_AR(ar);
        return REQ_PROCEED;
      case REQ_EXIT:
        FREE_AR(ar);
        return REQ_EXIT;
    }

    l = util_snprintf(buf2, sizeof(buf2),
                      "<TITLE>Index of %.*s</TITLE>\n<H1>Index of %.*s</H1>\n\n<ul>",
                      sizeof(buf2) - sizeof("<TITLE>Index of </TITLE>\n<H1>Index of </H1>\n\n<ul>"), uri,
                      sizeof(buf2) - sizeof("<TITLE>Index of </TITLE>\n<H1>Index of </H1>\n\n<ul>"), uri);
    bytes += l;

    if(net_write(sn->csd, buf2, l) == IO_ERROR) {
        if(errno != EPIPE) {
            log_error(LOG_WARN, "index-simple", sn, rq, 
                      XP_GetAdminStr(DBT_indexerror2),
                      system_errmsg());
        }
        FREE_AR(ar);
        return REQ_EXIT;
    }

    /* Skip last slash and find next slash for parent */
    for(x=strlen(uri) - 2; (x != -1) && (uri[x] != '/'); --x);
    if(x != -1) {
        c = uri[x+1];
        uri[x+1] = '\0';
        char* escaped = util_uri_escape(NULL, uri);
        l = util_snprintf(buf2, sizeof(buf2),
            "<li> <A HREF=\"%s\" NAME=\"parent\">Parent Directory</A>\n", escaped);
        FREE(escaped);
        bytes += l;
        if(net_write(sn->csd, buf2, l) == IO_ERROR) {
            if(errno != EPIPE) {
                log_error(LOG_WARN, "index-simple", sn, rq, 
                          XP_GetAdminStr(DBT_indexerror2),
                          system_errmsg());
            }
            FREE_AR(ar);
            return REQ_EXIT;
        }

        uri[x+1] = c;
    }

    for(x=0; ar[x]; x++) {
        util_uri_escape(buf2, ar[x]);
        l = util_snprintf(buf, sizeof(buf), "<li> <A NAME=\"%s\" HREF=\"%s\">%s</A>\n",
                    buf2, buf2, ar[x]);
        bytes += l;
        FREE(ar[x]);
        if(net_write(sn->csd, buf, l) == IO_ERROR) {
            if(errno != EPIPE) {
                log_error(LOG_WARN, "index-simple", sn, rq, 
                          XP_GetAdminStr(DBT_indexerror2),
                          system_errmsg());
            }
            return REQ_EXIT;
        }
    }
    l = util_sprintf(buf, "</ul>\n");
    bytes += l;

    FREE(ar);
    if(net_write(sn->csd, buf, l) == IO_ERROR) {
        if(errno != EPIPE) {
            log_error(LOG_WARN, "index-simple", sn, rq, 
                      XP_GetAdminStr(DBT_indexerror2),
                      system_errmsg());
        }
        return REQ_EXIT;
    }

    util_itoa(bytes, buf);
    pblock_nvinsert("content-length", buf, rq->vars);

    return REQ_PROCEED;
}


/* --------------------------- Bloated Indexing --------------------------- */


#define ICONS_ARE_LINKS 1
#define SCAN_HTML_TITLES 2

#define INDEX_MAXLEN 256


/* ----------------------------- cindex_init ------------------------------ */

#define NW_INIT 22
#define LW_INIT 18
#define SW_INIT 8
#define DW_INIT 33

#define MAX_FORMAT 256

static char *title_name = "Name";
static char *title_modified = "Last modified";
/* Have the extra space in here for nicer columns */
static char *title_size = " Size";
static char *title_description = "Description";

static char *blank_icon = "blank.png", *back_icon = "back.png";
static char *dir_icon = "menu.png", *default_icon = "unknown.png";
static char *icon_uri = "/mc-icons/", *ignore = NULL;
static int optf = 0, nw = NW_INIT, lw = LW_INIT, sw = SW_INIT, dw = DW_INIT;

static char *gmtformat="%d-%b-%Y %H:%M GMT";
static char *format="%d-%b-%Y %H:%M";
static int  gmtflag=0;


int cindex_init(pblock *pb, Session *sn, Request *rq)
{
    char *opts = pblock_findval("opts", pb);
    char *wid = pblock_findval("widths", pb);
    char *t;

    ignore = pblock_findval("ignore", pb);

    if ((t = pblock_findval("timezone", pb))) {
	if (!strcasecmp(t, "GMT")) {
		gmtflag = 1;
		lw += 4;    /* Provide extra space for the trailing GMT */
	}
    }

    if((!wid) || (sscanf(wid, "%d,%d,%d,%d", &nw, &lw, &sw, &dw) != 4)) {
        nw = NW_INIT; lw = gmtflag ? LW_INIT + 4 : LW_INIT; sw = SW_INIT; 
	    dw = DW_INIT;
    }

    
    /* Correct here for values that make no sense */
    if (nw <= (int)strlen(title_name)) 
	nw = strlen(title_name) + 1;
    if (lw != 0 && lw <= (int)strlen(title_modified)) 
	lw = strlen(title_modified) + 1;
    if (sw != 0 && sw <= (int)strlen(title_size)) 
	sw = strlen(title_size) + 1;
    if (dw != 0 && dw <= (int)strlen(title_description)) 
	dw = strlen(title_description) + 1;

    if(opts) {
        for(t = opts; *t; ++t) {
            if(tolower(*t) == 'i')
                optf |= ICONS_ARE_LINKS;
            else if(tolower(*t) == 's')
                optf |= SCAN_HTML_TITLES;
        }
    }
    if( (t = pblock_findval("icon-uri", pb)) )
        icon_uri = t;

    if ((t = pblock_findval("format", pb))) {
	/* This should prevent possible problems with long strings */
	if (strlen(t) < MAX_FORMAT)
		format = t;
	else
            log_error(LOG_WARN, "cindex_init", sn, rq, 
                  XP_GetAdminStr(DBT_indexerror3), MAX_FORMAT);
    }
    else if (gmtflag)
	format=gmtformat;

    return REQ_PROCEED;
}


/* ----------------------------- cindex_send ------------------------------ */


void _set_icon(cinfo *ci, char **icn, char **alt)
{
    if(ci && ci->type) {
        if(!strncasecmp(ci->type, "text/", 5)) {
            *icn = "text.png"; *alt = "TXT";
            return;
        }
        else if(!strncasecmp(ci->type, "image/", 6)) {
            *icn = "image.png"; *alt = "IMG";
            return;
        }
        else if(!strncasecmp(ci->type, "audio/", 6)) {
            *icn = "sound.png"; *alt = "SND";
            return;
        }
        else if(!strncasecmp(ci->type, "video/", 6)) {
            *icn = "movie.png"; *alt = "VID";
            return;
        }
        else if(!strcasecmp(ci->type, "application/octet-stream")) {
            *icn = "binary.png"; *alt = "BIN";
            return;
        }
    }
    *icn = default_icon;
    *alt = "   ";
}


int _scan_title(char *fn, struct stat *finfo, char *str, int size)
{
    char buf[257], *find = "<TITLE>";
    int x, y, p;
    SYS_FILE fd;

    if (!size) return 0;

    if( (fd = system_fopenRO(fn)) == SYS_ERROR_FD)
        return 0;

    x = system_fread(fd, buf, 256);

    system_fclose(fd);

    if((x == IO_ERROR) || (x == IO_EOF)) {
        return 0;
    }
    buf[x] = '\0';

    for(x=0, p=0; buf[x]; x++) {
        if(toupper(buf[x]) == find[p]) {
            if(!find[++p]) {
                ++x;
                while(buf[x] && isspace(buf[x])) ++x;
                if(!buf[x]) {
                    return 0;
                }
                y = x;
                while(buf[y] && (buf[y] != '<')) ++y;
                if(buf[y])
                    buf[y] = '\0';

                p = y - x;
                /* Scan for line breaks for Tanmoy's secretary */
                for(y = x; buf[y]; y++)
                    if((buf[y] == CR) || (buf[y] == LF))
                        buf[y] = ' ';

                if (p > size - 1)
                    p = size - 1;
                memcpy(str, &buf[x], p);
                str[p] = '\0';

                return p;
            }
        }
        else p=0;
    }

    return 0;
}


#include <limits.h>

int _insert_readme(char *path, char *fname, int rule, SYS_NETFD sd)
{
    char fn[PATH_BUF_MAX];
    int plaintext = 0, l, bytes;
    filebuf_t *fb;
    SYS_FILE fd;

    bytes = 0;
    l = util_snprintf(fn, sizeof(fn), "%s%s.html", path, fname);

    if((fd = system_fopenRO(fn)) == SYS_ERROR_FD) {
        fn[l - 5] = '\0';
        if((fd = system_fopenRO(fn)) == SYS_ERROR_FD)
            return 0;
        plaintext = 1;
        bytes = util_sprintf(fn, "%s<PRE>\n", (rule ? "<HR>" : ""));
        if(net_write(sd, fn, bytes) == IO_ERROR) {
            system_fclose(fd);
            return -1;
        }
    }
    else {
        if(rule) {
            bytes = 4;
            if(net_write(sd, "<HR>", 4) == IO_ERROR) {
                system_fclose(fd);
                return -1;
            }
        }
    }
    if(!(fb = filebuf_open(fd, FILE_BUFFERSIZE))) {
        system_fclose(fd);
        return 0;
    }

    l = filebuf_buf2sd(fb, sd);
    filebuf_close(fb);

    if(l == IO_ERROR)
        return -1;
    else 
        bytes += l;

    if(plaintext) {
        bytes += 6;
        if(net_write(sd, "</PRE>", 6) == IO_ERROR)
            return -1;
    }
    return bytes;
}


void _html_escape(char *d, int size, char *s)
{
    size--;
    while(*s && size > 0) {
        if((*s == '<') || (*s == '>')) {
            if (size < 5)
                break;

            int l = util_snprintf(d, size, "&%ct;", (*s == '<' ? 'l' : 'g'));
            ++s;
            d += l;
            size -= l;
        }
        else {
            *d++ = *s++;
            size--;
        }
    }
    *d = '\0';
}


int _strpad(char *t, int size, char *s, int wid)
{
    return util_snprintf(t, size, "%-*s", wid, s);
}

/* Faster than dividing */
int _numpad(char *t, int size, int n, char c)
{
    if(n < 10)
        return util_snprintf(t, size, "   %d%c  ", n, c);
    else if(n < 100)
        return util_snprintf(t, size, "  %d%c  ", n, c);
    else if(n < 1000)
        return util_snprintf(t, size, " %d%c  ", n, c);
    else
        return util_snprintf(t, size, "%d%c  ", n, c);
}


#define IDX_WRITE(buf, len) \
    if(net_write(sn->csd, (buf), (len)) == IO_ERROR) { \
        if(errno != EPIPE) { \
            log_error(LOG_WARN, "index-common", sn, rq, \
                     XP_GetAdminStr(DBT_indexerror2), \
                     system_errmsg()); \
        } \
        return REQ_ABORTED; \
    }


int cindex_send(pblock *pb, Session *sn, Request *rq)
{
    char **ar, *t;
    register int x, y, stat_good, l;
    char c = 0, *path = pblock_findval("path", rq->vars);
    char *uri = pblock_findval("uri", rq->reqpb);
    char *head = pblock_findval("header", pb);
    char *readme = pblock_findval("readme", pb);
    char *str = pblock_findval("urlencoding", pb);
    PRBool urlencode = PR_TRUE;
	
    char buf[PATH_BUF_MAX], buf2[3*PATH_BUF_MAX];
    int bytes;
    struct stat finfo;
    cinfo *ci = 0;
    char *query;

    /* Check if the urlencoding for file names is disabled */
    if ((str != NULL) && !(strcmp(str, "off")))
	urlencode = PR_FALSE;

    bytes = 0;

    if(!(ar = _dir_ls(path, sn, rq)))
        return REQ_ABORTED;

    httpfilter_buffer_output(sn, rq, PR_TRUE);

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);

    switch(protocol_start_response(sn, rq)) {
      case REQ_NOACTION:
        FREE_AR(ar);
        return REQ_PROCEED;
      case REQ_EXIT:
        FREE_AR(ar);
        return REQ_EXIT;
    }

    l = util_snprintf(buf2, sizeof(buf2), "<TITLE>Index of %s</TITLE>\n", uri);
    IDX_WRITE(buf2, l);
    bytes += l;

    l = 0;

    if(head) {
        if((l = _insert_readme(path, head, 0, sn->csd)) == -1) {
            FREE_AR(ar);
            return REQ_EXIT;
        }
    }

    if(!l) {
        l = util_snprintf(buf2, sizeof(buf2), "<h1>Index of %.*s</h1>\n",
                          sizeof(buf2) - sizeof("<h1>Index of </h1>\n"), uri);
        IDX_WRITE(buf2, l);
    }
    bytes += l;

    l = util_snprintf(buf, sizeof(buf), "<PRE><IMG SRC=\"%s%s\" ALT=\"     \">  ", 
                      icon_uri, blank_icon);
    IDX_WRITE(buf, l);
    bytes += l;

    l = _strpad(buf2, sizeof(buf2), title_name, nw + 1);
    if(lw)
        l += _strpad(&buf2[l], sizeof(buf2) - l, title_modified, lw + 1);
    if(sw)
        l += _strpad(&buf2[l], sizeof(buf2) - l, title_size, sw + 1);
    if(dw > 0)
        l += _strpad(&buf2[l], sizeof(buf2) - l, title_description, dw + 1);

    l += util_snprintf(&buf2[l], sizeof(buf2) - l, "\n<HR>\n");

    t = pblock_findval("uri", rq->reqpb);
    if((t[0] != '/') || (t[1] != '\0')) {
        x = strlen(t) - 1;
        if(x) {
            for(--x; x && (t[x] != '/'); --x);
            c = t[x+1];
            t[x+1] ='\0';
        }

        char *escaped = util_uri_escape(NULL, t);
        l += util_snprintf(&buf2[l], sizeof(buf2) - l, "<A HREF=\"%s\" NAME=\"%s\"><IMG SRC=\"%s%s\" ALT=\"[%s]\" BORDER=0>  Parent Directory</A>\n", escaped, escaped, icon_uri, back_icon, "DIR");
        FREE(escaped);

        bytes += l;
        t[x+1] = c;
    }
    IDX_WRITE(buf2, l);

    for(x=0; ar[x]; x++) {
        char *alt, *icn, fn[PATH_BUF_MAX + sizeof("</A>")];

        if(ignore && (!shexp_cmp(ar[x], ignore)))
            continue;

        alt = NULL; icn = NULL;

        util_snprintf(fn, sizeof(fn), "%s%s", path, ar[x]);
        stat_good = (stat(fn, &finfo) == -1 ? 0 : 1);

        if(stat_good && S_ISDIR(finfo.st_mode)) {
            alt = "DIR";
            icn = dir_icon;
            l = strlen(ar[x]) - 1;
            if(ar[x][l] != '/') {
                ar[x][l+1] = '/'; ar[x][l+2] = '\0';
            }
        }
        else {
            ci = cinfo_find(ar[x]);
            _set_icon(ci, &icn, &alt);
            if (ci)
                FREE(ci);
        }
        util_uri_escape(buf2, ar[x]);

	if (urlencode == PR_TRUE) {
        	l = util_snprintf(buf, sizeof(buf), "<A HREF=\"%s\" NAME=\"%s\"><IMG SRC=\"%s%s\" ALT=\"[%s]\" BORDER=0>  ", buf2, buf2, (icn[0] == '/' ? "" : icon_uri), icn, alt);
	}
	else {
        	l = util_snprintf(buf, sizeof(buf), "<A HREF=\"%s\" NAME=\"%s\"><IMG SRC=\"%s%s\" ALT=\"[%s]\" BORDER=0>  ", ar[x], ar[x], (icn[0] == '/' ? "" : icon_uri), icn, alt);
	}
        for(y = 0; ar[x][y]; ++y) {
            if(y == (nw - 1)) {
                fn[y++] = '+';
                break;
            }
            fn[y] = ar[x][y];
        }
        strcpy(&fn[y], "</A>");
        _strpad(buf2, sizeof(buf2), fn, nw+4);

        l += util_snprintf(&buf[l], sizeof(buf) - l, "%s ", buf2);

        if(lw) {
            if(stat_good) {
                struct tm ts;
		if (gmtflag)
                    util_gmtime(&(finfo.st_mtime), &ts);
		else
                    util_localtime(&(finfo.st_mtime), &ts);
                util_strftime(buf2, format, &ts);
                l += _strpad(&buf[l], sizeof(buf) - l, buf2, lw + 1);
            }
            else
                l += _strpad(&buf[l], sizeof(buf) - l, "", lw + 1);
        }
        if(sw) {
            if(stat_good) {
                size_t size = finfo.st_size;
		int bytes;

                if(!size)
                    bytes = util_snprintf(&buf[l], sizeof(buf) - l, "%s", "   0K");
                else if(size < 1024)
                    bytes = util_snprintf(&buf[l], sizeof(buf) - l, "%s", "   1K");
                else if(size < 1048576)
                    bytes = _numpad(&buf[l], sizeof(buf) - l, size / 1024, 'K');
                else
                    bytes = _numpad(&buf[l], sizeof(buf) - l, size / 1048576, 'M');

		l += bytes;
		l += _strpad(&buf[l], sizeof(buf) - l, "", (sw-bytes > 0) ? (sw-bytes+1):(1));
            }
            else
                l += _strpad(&buf[l], sizeof(buf) - l, "-", sw + 1);
        }
        if(dw) {
            if( (optf & SCAN_HTML_TITLES) ) {
                if(ci && ci->type && (!strcmp(ci->type, "text/html"))) {
		    int bytes;
                    util_sprintf(fn, "%s%s", path, ar[x]);
                    bytes = _scan_title(fn, &finfo, &buf[l], sizeof(buf) - l);
		    l += bytes;
		    _strpad(&buf[l], sizeof(buf) - l, "", (dw-bytes > 0) ? (dw-bytes+1):(1));
                }
            }
        }

        if (l < sizeof(buf)) {
            buf[l++] = '\n';
        }
    
        bytes += l;
        IDX_WRITE(buf, l);

        FREE(ar[x]);
    }
    bytes += 6;
    IDX_WRITE("</PRE>", 6);

    FREE(ar);

    if(readme) {
        if( (l = _insert_readme(path, readme, 1, sn->csd)) == -1)
            return REQ_EXIT;
        else 
            bytes += l;
    }

    util_itoa(bytes, buf);
    pblock_nvinsert("content-length", buf, rq->vars);

    return REQ_PROCEED;
}
