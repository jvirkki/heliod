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
 * service.c: Handle Service-related things (including directive and user-
 * callable functions
 * 
 * Rob McCool
 */

#include "netsite.h"

#include "safs/service.h"
#include "safs/favicon.h"
#include "safs/dbtsafs.h"
#include "frame/log.h"      /* log_error */
#include "frame/protocol.h" /* protocol_start_response */
#include "frame/req.h"      /* request_handle_processed */
#include "base/pblock.h"    /* pblock_findval */
#include "base/file.h"      /* system_errmsg */
#include "base/util.h"      /* util_getline, util_is_url */
#include "frame/httpact.h"  /* servact_child_request */
#include "frame/http.h"
#include "frame/httpfilter.h"
#include "plstr.h"
#include "safs/nsfcsafs.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/ListenSocketConfig.h"

#include <errno.h>
#include <limits.h>

#define TRAILER_MAX_LEN 512
#define TRAILER_LASTMOD ":LASTMOD:"
#define TRAILER_LASTMOD_LEN 9

int SendHead(pblock *param, Session *sn, Request *rq);


/* ------------------------------- get_path ------------------------------- */

static inline char *get_path(const char *fn, Session *sn, Request *rq)
{
    char *path = pblock_findkeyval(pb_key_path, rq->vars);
    if (!path)
        return NULL;

    const char *path_info = pblock_findkeyval(pb_key_path_info, rq->vars);
    if (path_info) {
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        log_error(LOG_WARN, fn, sn, rq, 
                  XP_GetAdminStr(DBT_serviceError1),
                  path, path_info);
        return NULL;
    }

    return path;
}


/* ------------------------- service_plain_range -------------------------- */

typedef struct _range_element_t {
    _range_element_t *next;
    PROffset64       start;
    PROffset64       end;
} _range_element;


static int
_range_validate(_range_element *a_range, const PRFileInfo64 *finfo, int *status)
{

    PR_ASSERT(a_range != NULL && finfo != NULL && status != NULL);

    *status = PROTOCOL_OK;

           /* Range: bytes=-500 (last 500 bytes) */
    if (a_range->start == -1) {
        a_range->start = finfo->size - a_range->end;
        if (a_range->start < 0 && finfo->size != 0)
            a_range->start = 0;
        a_range->end = finfo->size - 1;
    }

    if (a_range->end == -1 || a_range->end >= finfo->size)
        a_range->end = finfo->size -1;

    if (a_range->start >= finfo->size) {
        *status = PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE;
        return -1;
    }
    if (a_range->start < 0 || a_range->end < 0 
        || (a_range->end - a_range->start) < 0)
        return -1; 

    return 0; 
}

static void 
_ranges_free(_range_element *head)
{
    _range_element *ptr;

    while (head != NULL) {
        ptr = head;
        head = ptr->next;
        FREE(ptr);
    }

}

/*
 * check status when _ranges_parse() return <= 0
 * in any case, caller need to free the linked-list pointed by *head
 */
static int 
_ranges_parse(_range_element **head, char *range_hdr, PRFileInfo64 *finfo, int *status)  
{

    int     num_ranges = 0, a_status = PROTOCOL_OK;
    PRBool  more = PR_TRUE;
    char   *range = range_hdr;
    _range_element *ranges_tail = NULL, *ranges_head = NULL;

    PR_ASSERT(head != NULL && status != NULL);

    *head = NULL;
    *status = PROTOCOL_OK;

    if (range && strchr(range, ';')) {
              /* According to draft... */
        *status = PROTOCOL_NOT_IMPLEMENTED; 
        return -1;
    }
    while (range) {
        char *ptr = NULL;
        char *next = NULL;
        _range_element *a_range;

        if (more == PR_FALSE)
            break;

        *status = PROTOCOL_OK;
        ptr = strchr(range, ',');
        next = ptr;
        if (ptr) {
            next++;
            *ptr = '\0'; 
        }

        if (!(next && *next))     /* no more ranges */
            more = PR_FALSE;

        ptr = strchr(range, '-'); 
        if (! (ptr && *ptr))
            return -1;           /* syntax error in Range request header */

        if (*(ptr+1) && strchr(ptr+1, '-') != NULL)
            return -1;

        while (isspace(*range))
            range++;

        a_range = (_range_element *)MALLOC(sizeof(_range_element));
        if (a_range == NULL)
            return -1;
        
        a_range->next = NULL;
        a_range->start = -1;
        a_range->end = -1;

        if (*range == '-') {
            a_range->start = -1;
        }
        else if (isdigit(*range)) {
            a_range->start = util_atoi64(range);
        }
        else {
            FREE(a_range);
            return -1;
        }

        range = ++ptr;
        while (isspace(*range))
            range++;
        if (!(*range)) {
            if (a_range->start == -1) { /* bytes= - syntax error */
                FREE(a_range);
                return -1;
            }
            a_range->end = finfo->size - 1;
        }
        else if (isdigit(*range)) {
            a_range->end = util_atoi64(range);
        }
        else {
            FREE(a_range);
            return -1;
        }

        if (_range_validate(a_range, finfo, &a_status) != 0) {
            FREE(a_range);
            if (a_status == PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE) {
                *status = PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE;
                a_status = PROTOCOL_OK;
                range = next;
                continue;
            }
            else {
                return -1;
            }
        }

        if (ranges_tail == NULL && ranges_head == NULL) {
            ranges_head=ranges_tail=a_range;
        }
        else {
            PR_ASSERT(ranges_tail != NULL && ranges_head != NULL);
            ranges_tail->next = a_range;
            ranges_tail = a_range;
        }
        num_ranges++;
        range = next;
    }
    *head = ranges_head;
    PR_ASSERT(*head == NULL || num_ranges > 0);

    return num_ranges;
}


static int _range_send(SYS_FILE fd, SYS_NETFD sd, PROffset64 rstart, PROffset64 rend, PROffset64 rlen)
{
  int n = -1;
  PROffset64 o = 0;
  PROffset64 l = rlen;
  char buf[FILE_BUFFERSIZE];
  
  /* We have no guarantee where the fd is at this point */
  PR_Seek64(fd, rstart, PR_SEEK_SET);
  
  // VB: n != 0 ensures that we do not loop infinitely on a EOF
  for(o = 0; (o != l) && (n != 0); o += n) {
    if( (n = l - o) > sizeof(buf))
      n = sizeof(buf);
    n = system_fread(fd, buf, n);
    if (n < 0)
      return IO_ERROR;
    else if (n > 0) {
      if (net_write(sd, buf, n) == IO_ERROR) 
        return IO_ERROR;
    }
  }
  return IO_OKAY;
}

static int _range_service(Session *sn, Request *rq, const char *path, PRFileInfo64 *finfo, const char *range)
{
    char sep[128];
    char numstr[UTIL_I64TOA_SIZE];
    char hdr[256];
    char *ums;
    int l, seplen = 0, num_ranges, parse_status=0;
    pb_param *doc_type;
    SYS_FILE fd;
    _range_element *head = NULL, *curr_head = NULL;

    if (strncmp(range, "bytes=", 6))
        return REQ_NOACTION;

    char *byterange = pool_strdup(sn->pool, range + 6);

    num_ranges = _ranges_parse(&head, byterange, finfo, &parse_status);
    if (num_ranges <= 0) {
        _ranges_free(head);
        if (parse_status == PROTOCOL_NOT_IMPLEMENTED) {
            protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
            return REQ_ABORTED;
        }
        if (rq->protv_num >= 101 &&
            parse_status == PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE) {
            if (!pblock_findval("if-range", rq->headers)) {
                protocol_status(sn, rq, PROTOCOL_REQUESTED_RANGE_NOT_SATISFIABLE, NULL);
                util_sprintf(hdr, "bytes */%lld", finfo->size);
                pblock_nvinsert("content-range", hdr, rq->srvhdrs);
                return REQ_ABORTED;
            }
        }
        return REQ_NOACTION;
    }

    if(!(pblock_find("status", rq->srvhdrs)))
        protocol_status(sn, rq, PROTOCOL_PARTIAL_CONTENT, NULL);

    /* Set content-length only if there's one range. */
    param_free(pblock_remove("content-length", rq->srvhdrs));

    PR_ASSERT(num_ranges >= 1);
    PR_ASSERT(head != NULL);
    if (num_ranges > 1) {
        doc_type = pblock_remove("content-type", rq->srvhdrs);
        seplen = util_mime_separator(sep);
        if (rq->protv_num >= 101) {
            util_sprintf(hdr, "multipart/byteranges; boundary=%s", &sep[4]);
        }
        else {
            util_sprintf(hdr, "multipart/x-byteranges; boundary=%s", &sep[4]);
        }
        pblock_nvinsert("content-type", hdr, rq->srvhdrs);
        util_sprintf(&sep[seplen], "%c%c", CR, LF);
        seplen += 2;
    }
    else {
        util_i64toa(head->end - head->start + 1, numstr);
        pblock_nvinsert("content-length", numstr, rq->srvhdrs);
        util_sprintf(hdr, "bytes %lld-%lld/%lld", head->start, head->end, finfo->size);
        pblock_nvinsert("content-range", hdr, rq->srvhdrs);
        doc_type = NULL;
    }
    if((fd = system_fopenRO(path)) == SYS_ERROR_FD) {
        log_error(LOG_WARN, "send-file", sn, rq, XP_GetAdminStr(DBT_serviceError3), 
                  path, system_errmsg());
        protocol_status(sn, rq, (rtfile_notfound() ? PROTOCOL_NOT_FOUND : 
                                 PROTOCOL_FORBIDDEN), NULL);
        _ranges_free(head);
        return REQ_ABORTED;
    }
    if(protocol_start_response(sn,rq) != REQ_NOACTION) {
        curr_head = head;
        while(curr_head != NULL) {
            if(num_ranges > 1) {
                net_write(sn->csd, sep, seplen);
                l = 0;
                if (doc_type) {
                    l += util_snprintf(hdr + l, sizeof(hdr) - l,
                                       "Content-type: %s\n",
                                       doc_type->value);
                }
                if (rq->protv_num >= 101) {
                    l += util_snprintf(hdr + l, sizeof(hdr) - l,
                                       "Content-range: bytes %lld-%lld/%lld\n\n",
                                       curr_head->start, 
                                       curr_head->end, 
                                       finfo->size);
                }
                else {
                    l += util_snprintf(hdr + l, sizeof(hdr) - l,
                                       "Range: bytes %lld-%lld/%lld\n\n",
                                       curr_head->start, 
                                       curr_head->end, 
                                       finfo->size);
                }
                net_write(sn->csd, hdr, l);
            }
            if(_range_send(fd, sn->csd, curr_head->start, curr_head->end,
                           (curr_head->end - curr_head->start + 1)) == IO_ERROR) {
                system_fclose(fd);
                param_free(doc_type);
                _ranges_free(head);
                return REQ_EXIT;
            }
            curr_head = curr_head->next;
        }
    }
    if(num_ranges > 1) {
        util_sprintf(&sep[seplen-2], "--%c%c", CR, LF);
        seplen += 2;
        net_write(sn->csd, sep, seplen);
    }
    system_fclose(fd);
    param_free(doc_type);
    _ranges_free(head);
    return REQ_PROCEED;
}

int service_plain_range(pblock *param, Session *sn, Request *rq)
{
    const char *range = pblock_findkeyval(pb_key_range, rq->headers);
    if (!range)
        return REQ_NOACTION;

    const char *path = get_path("send-range", sn, rq);
    if (!path)
        return REQ_ABORTED;

    PRFileInfo64 finfo;
    if (PR_GetFileInfo64(path, &finfo) != PR_SUCCESS) {
        int code;
        if (PR_GetError() == PR_FILE_NOT_FOUND_ERROR) {
            code = PROTOCOL_NOT_FOUND;
        } else {
            code = PROTOCOL_FORBIDDEN;
        }
        log_error(LOG_WARN, "send-file", sn, rq,
                  XP_GetAdminStr(DBT_serviceError2),
                  path, system_errmsg());
        protocol_status(sn, rq, code, NULL);
        return REQ_ABORTED;
    }

    return _range_service(sn, rq, path, &finfo, range);
}


/* -------------------------- service_plain_file -------------------------- */


int service_plain_file(pblock *param, Session *sn, Request *rq) 
{
    PRInt64 xlen;
    NSFCFileInfo *finfo = NULL;
    int ret;

    const char *path = get_path("send-file", sn, rq);
    if (!path)
        return REQ_ABORTED;

    NSFCCache nsfcCache = GetServerFileCache();
    NSFCEntry entry = NSFCENTRY_INIT;
    NSFCFileInfo myfinfo;
    NSFCStatusInfo statusInfo;
    PRStatus statrv;
    NSFCStatus rfc;
    int cache_transmit = 0;

    finfo = NULL;
    statrv = PR_SUCCESS;
    myfinfo.prerr = 0;
    NSFCSTATUSINFO_INIT(&statusInfo);

    PRBool doNotCache = (pblock_findkey(pb_key_nocache, param) == NULL ? 
                         PR_FALSE : PR_TRUE);
    if (doNotCache == PR_FALSE) {
        rfc = NSFC_AccessFilename(path, &entry, &myfinfo, nsfcCache,
                                  &statusInfo);
        if (rfc == NSFC_OK) {
            rfc = NSFC_GetEntryFileInfo(entry, &myfinfo, nsfcCache);
            if (rfc == NSFC_OK) {
                finfo = &myfinfo;
                cache_transmit = 1;
            }
            else {
                NSFC_ReleaseEntry(nsfcCache, &entry);
            }
        }
        if (rfc == NSFC_STATFAIL) {
            PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
            finfo = &myfinfo;    
            statrv = PR_FAILURE;
        }
    }
    if (finfo == NULL) {
        PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
        finfo = &myfinfo;
        if (doNotCache) {
            statrv = NSFC_GetNonCacheFileInfo(path, finfo); 
        } else {
            statrv = INTrequest_info_path(path, rq, &finfo);
        }
    }
    if (statrv == PR_FAILURE) {
        // <HACK>
        // ruslan: check for IE icon file
        char * lUri = pblock_findkeyval (pb_key_uri, rq->reqpb);
        if (lUri != NULL && !PL_strcasecmp (lUri, "/favicon.ico"))
        {
            ret = service_favicon(param, sn, rq);
            if (ret != REQ_NOACTION)
                return ret;
        }
        // </HACK>

        PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
        protocol_status(sn, rq, (finfo->prerr == PR_FILE_NOT_FOUND_ERROR) ?
                        PROTOCOL_NOT_FOUND : PROTOCOL_FORBIDDEN, NULL);
        PR_SetError(finfo->prerr, finfo->oserr);
        log_error(LOG_WARN, "send-file", sn, rq, XP_GetAdminStr(DBT_serviceError2),
                  path, system_errmsg());
        return REQ_ABORTED;
    }
    if(http_set_nsfc_finfo(sn, rq, entry, finfo) == REQ_ABORTED) {
        if (NSFCENTRY_ISVALID(&entry)) {
            PR_ASSERT(cache_transmit);
            if (rq->request_is_cacheable) {
                NSAPIRequest *nrq = (NSAPIRequest *) rq;
                nrq->accel_nsfc_entry = entry;
                rq->directive_is_cacheable = 1;
            } else {
                NSFC_ReleaseEntry(nsfcCache, &entry);
            }
        }
        return REQ_ABORTED;
    }

    const char *range = pblock_findkeyval(pb_key_range, rq->headers);
    if (range) {
        ret = _range_service(sn, rq, path, &finfo->pr, range);
        if (ret != REQ_NOACTION) {
            if (NSFCENTRY_ISVALID(&entry))
                NSFC_ReleaseEntry(nsfcCache, &entry);
            return ret;
        }
    }

    pblock_kvinsert(pb_key_accept_ranges, "bytes", 5, rq->srvhdrs);
    if(!(pblock_findkey(pb_key_status, rq->srvhdrs)))
        protocol_status(sn, rq, PROTOCOL_OK, NULL);

    if (ISMHEAD(rq))
    {
        if (NSFCENTRY_ISVALID(&entry))
        {
            PR_ASSERT(cache_transmit);
            NSFC_ReleaseEntry(nsfcCache, &entry);
        }
        return SendHead(param, sn, rq);
    }

    if (!ISMHEAD(rq)) {
        NSFCSTATUSINFO_INIT(&statusInfo);
            /* Send the headers, followed by the file */
        if (cache_transmit) {
            PR_ASSERT(NSFCENTRY_ISVALID(&entry));
            xlen = NSFC_TransmitEntryFile(sn->csd, entry,
                                          NULL, 0, NULL, 0,
                                          PR_INTERVAL_NO_TIMEOUT,
                                          nsfcCache, &statusInfo);
        }
        else {
            PR_ASSERT(!NSFCENTRY_ISVALID(&entry));
            xlen = NSFC_TransmitFileNonCached(sn->csd, path, finfo,
                                              NULL, 0, NULL, 0,
                                              PR_INTERVAL_NO_TIMEOUT,
                                              nsfcCache, &statusInfo);
        }

        /* Check for error on the transmit */
        if (xlen < 0) {
            PRErrorCode prerr = PR_GetError();
            if (prerr != PR_CONNECT_RESET_ERROR) {
                log_error(LOG_WARN,
                          "send-file", sn, rq, XP_GetAdminStr(DBT_serviceError4),
                          path, system_errmsg());
                if (prerr == PR_FILE_IS_BUSY_ERROR ||
                    statusInfo == NSFC_STATUSINFO_FILESIZE)
                    protocol_status(sn, rq, PROTOCOL_SERVER_ERROR, NULL);
                else
                    protocol_status(sn, rq,
                                    (file_notfound() ? PROTOCOL_NOT_FOUND
                                                     : PROTOCOL_FORBIDDEN), NULL);
            }
            // If we failed transmitting the file, we need to give up our reference
            if (NSFCENTRY_ISVALID(&entry)) {
                NSFC_ReleaseEntry(nsfcCache, &entry);
            }
            return REQ_ABORTED;
        }

        if (statusInfo == NSFC_STATUSINFO_FILESIZE) {
            if (NSFCENTRY_ISVALID(&entry)) 
                NSFC_ReleaseEntry(nsfcCache, &entry);
            log_error(LOG_VERBOSE, "send-file", sn, rq, 
                      "sending %s (File size changed on transmit). end session.", path);
            return REQ_EXIT;
        }
    }

    if (NSFCENTRY_ISVALID(&entry)) {
        if (rq->request_is_cacheable) {
            NSAPIRequest *nrq = (NSAPIRequest *) rq;
            nrq->accel_nsfc_entry = entry;
            rq->directive_is_cacheable = 1;
        } else {
            NSFC_ReleaseEntry(nsfcCache, &entry);
        }
    }

    return REQ_PROCEED;
}

/* -------------------------- service_send_error -------------------------- */


int service_send_error(pblock *pb, Session *sn, Request *rq)
{
    // First try to redirect to the specified uri...
    char *new_uri = pblock_findval("uri", pb);
    if (new_uri) {
        if (servact_include_virtual(sn, rq, new_uri, NULL) == REQ_PROCEED)
            return REQ_PROCEED;
    }

    char *new_path = pblock_findval("path", pb);
    SYS_FILE fd;
    filebuf_t *buf;
    pb_param *pptr;
    
    if(!new_path) {
        log_error(LOG_MISCONFIG, "send-error", sn, rq, 
                  XP_GetAdminStr(DBT_serviceError5));
        return REQ_ABORTED;
    }

    /* Fix content-type */
    pptr = pblock_remove("content-type", rq->srvhdrs);
    if (pptr)
        param_free(pptr);
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

    if((fd = system_fopenRO(new_path)) == SYS_ERROR_FD) {
        log_error(LOG_WARN, "send-error", sn, rq, XP_GetAdminStr(DBT_serviceError3),
                  new_path, system_errmsg());
        protocol_status(sn, rq, (file_notfound() ? PROTOCOL_NOT_FOUND :
                                 PROTOCOL_FORBIDDEN), NULL);
        return REQ_ABORTED;
    }

    if(!(buf = filebuf_open(fd, FILE_BUFFERSIZE))) {
        log_error(LOG_WARN,"send-error", sn, rq,
                  XP_GetAdminStr(DBT_serviceError6), new_path,
              system_errmsg());
        system_fclose(fd);
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    PRFileInfo statInfo;
    if (PR_GetFileInfo(new_path, &statInfo) == PR_SUCCESS) {
        char buff[24];
        sprintf(buff, "%d", statInfo.size);
        pblock_nvinsert("content-length", buff, rq->srvhdrs);
        // Something's really wrong (status isn't 401, 402, 403, or 404), so we
        // want to get rid of this connection

        if (rq->status_num < 401 || rq->status_num > 404) 
        {
            KEEP_ALIVE(rq) = PR_FALSE;
        }

    }

    if(protocol_start_response(sn,rq) != REQ_NOACTION) {
        if(filebuf_buf2sd(buf, sn->csd) == IO_ERROR) {
            filebuf_close(buf);
            return REQ_EXIT;
        }
    }
    filebuf_close(buf);

    return REQ_PROCEED;
}


/* --------------------------- service_trailer ---------------------------- */


int service_trailer(pblock *pb, Session *sn, Request *rq)
{
    char *query = pblock_findval("query", rq->reqpb);
    char *trail = pblock_findval("trailer", pb), *trailer = 0;
    char *timefmt = pblock_findval("timefmt", pb);
    char *path, timebuf[256], tbuf[TRAILER_MAX_LEN], *t, *u;
    struct stat *finfo;
    struct tm *tms, tmss;
    pb_param *pp;
    SYS_FILE fd;
    filebuf_t *buf;
    int l, cl;

    if(!trail)
	{
        log_error (LOG_MISCONFIG, "append-trailer", sn, rq, XP_GetAdminStr(DBT_serviceError7));
        goto punt;
    }
    trailer = STRDUP(trail);
    util_uri_unescape(trailer);
    path = get_path("append-trailer", sn, rq);
    if (!path)
        goto punt;
    if((fd = system_fopenRO(path)) == SYS_ERROR_FD) {
        log_error(LOG_WARN, "append-trailer", sn, rq, XP_GetAdminStr(DBT_serviceError3),
                  path, system_errmsg());
        protocol_status(sn, rq, (file_notfound() ? PROTOCOL_NOT_FOUND : 
                                 PROTOCOL_FORBIDDEN), NULL);
        goto punt;
    }
    if(!(finfo = request_stat_path(path, rq))) {
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        log_error(LOG_WARN,"append-trailer", sn, rq, XP_GetAdminStr(DBT_serviceError8), 
                  path, rq->staterr);
        goto punt;
    }

    if(!(buf = filebuf_open_nostat(fd, FILE_BUFFERSIZE, finfo))) {
        log_error(LOG_WARN, "append-trailer", sn, rq,
                  XP_GetAdminStr(DBT_serviceError6),path, system_errmsg());
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        goto punt;
    }
    if(!(pblock_find("status", rq->srvhdrs)))
        protocol_status(sn, rq, PROTOCOL_OK, NULL);
    if(protocol_set_finfo(sn, rq, finfo) == REQ_ABORTED) {
        filebuf_close(buf);
        goto punt;
    }

    if(timefmt) {
        tms = system_localtime(&finfo->st_mtime, &tmss);
        (void)util_strftime(timebuf, timefmt, tms);
        l = strlen(timebuf);
        t = trailer;
        u = tbuf;

        while(*t) {
            if(!strncmp(t, TRAILER_LASTMOD, TRAILER_LASTMOD_LEN)) {
                strcpy(u, timebuf);
                t += TRAILER_LASTMOD_LEN;
                u += l;
            }
            else *u++ = *t++;

            if((u - tbuf) > (TRAILER_MAX_LEN - l))
                break;
        }
        *u = '\0';
        l = u - tbuf;
        t = tbuf;
    }
    else {
        t = trailer;
        l = strlen(t);
    }
    if( (pp = pblock_find("content-length", rq->srvhdrs)) ) {
        cl = l + atoi(pp->value);
        FREE(pp->value);
        util_itoa(cl, timebuf);
        pp->value = STRDUP(timebuf);
    }
    if(protocol_start_response(sn, rq) != REQ_NOACTION) {
        if(filebuf_buf2sd(buf, sn->csd) == IO_ERROR) {
            FREE(trailer);
            filebuf_close(buf);
            return REQ_EXIT;
        }
        net_write(sn->csd, t, l);
    }
    filebuf_close(buf);
    FREE(trailer);
    return REQ_PROCEED;
  punt:
    FREE(trailer);
    return REQ_ABORTED;
}


/* --------------------------- service_imagemap --------------------------- */


#define MAXLINE 500
#define MAXVERTS 100
#define X 0
#define Y 1

#define isname(c) (!isspace((c)))

#define IMAP_BADREQ \
"<TITLE>Imagemap Error</TITLE><H1>Imagemap Error</H1>Your client did not send any coordinates. Either your client doesn't support imagemap or the map file was not accessed as a map."


void imap_die(char *msg, Session *sn, Request *rq)
{
    int ml = strlen(msg);
    char buf[16];

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    util_itoa(ml, buf);
    pblock_nvinsert("content-length", buf, rq->srvhdrs);
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    protocol_start_response(sn, rq);
    net_write(sn->csd, msg, ml);
}

int imap_handle(char *url, Session *sn, Request *rq, char *path, 
                int ncsa_compat)
{
    char *uri, *fullurl, *t, c = 0, *fulluri;

    if(util_is_url(url)) {
        pblock_nvinsert("url", url, rq->vars);
        protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);
        return REQ_ABORTED;
    }  else  {
	if(!ncsa_compat)  {
            uri = pblock_findval("uri", rq->reqpb);

            if( (t = strrchr(uri, '/')) ) {
                c = *(++t);
                *t = '\0';
            }

	    fulluri = (char *) MALLOC(strlen(uri)+strlen(url)+2);
	    if(url[0] != '/')  
	        util_sprintf(fulluri, "%s%s", uri, url);
	    else 
		util_sprintf(fulluri, "%s", url);
	    util_uri_parse(fulluri);	         /* strip /../, /./, etc */

            fullurl = protocol_uri2url_dynamic(fulluri, "", sn, rq);

            if(t) 
                *t = c;
            pblock_nvinsert("url", fullurl, rq->vars);
            protocol_status(sn, rq, PROTOCOL_REDIRECT, NULL);

	    FREE(fulluri);
            FREE(fullurl);

            return REQ_ABORTED;
        }  else  {
            /* Emulate old httpd behavior. Bad? */
            return request_restart(sn, rq, NULL, url, NULL);
        }
    }
}


int pointinpoly(double where[2], double target[MAXVERTS][2])
{
    int totalv = 0;
    int intersects = 0;
    double wherex = where[X];
    double wherey = where[Y];
    double xval, yval;
    double *pointer;
    double *end;

    while((target[totalv][X] != -1) && (totalv < MAXVERTS))
	totalv++;
    xval = target[totalv - 1][X]; 
    yval = target[totalv - 1][Y];  
    end = target[totalv];

    pointer = (double *) target+1;

    if ((yval >= wherey) != (*pointer >= wherey)) 
        if ((xval >= wherex) == (*(double *) target >= wherex)) 
	    intersects += (xval >= wherex);
        else 
            intersects += (xval - (yval - wherey) * 
                          (*(double *) target - xval) /
                          (*pointer - yval)) >= wherex;

    while(pointer < end)  {
        yval = *pointer;
        pointer += 2;

        if(yval >= wherey)  {
	   while((pointer < end) && (*pointer >= wherey))
		pointer+=2;
	   if (pointer >= end)
		break;
           if( (*(pointer-3) >= wherex) == (*(pointer-1) >= wherex) )
	        intersects += (*(pointer-3) >= wherex);
 	    else
		intersects += (*(pointer-3) - (*(pointer-2) - wherey) *
                              (*(pointer-1) - *(pointer-3)) /
                              (*pointer - *(pointer - 2))) >= wherex;

        }  else  {
	   while((pointer < end) && (*pointer < wherey))
		pointer+=2;
	   if (pointer >= end)
		break;
           if( (*(pointer-3) >= wherex) == (*(pointer-1) >= wherex) )
		intersects += (*(pointer-3) >= wherex);
 	   else
	        intersects += (*(pointer-3) - (*(pointer-2) - wherey) *
                              (*(pointer-1) - *(pointer-3)) /
                              (*pointer - *(pointer - 2))) >= wherex;
        }  
    }
    return (intersects & 1);
}


int service_imagemap(pblock *pb, Session *sn, Request *rq)
{
    char *coord = pblock_findval("query", rq->reqpb), *t;
    char *path = pblock_findval("path", rq->vars);
    int ncsa_compat = pblock_findval("ncsa-compat", pb) ? 1 : 0;
    double testpoint[2], pointarray[MAXVERTS][2], closepoint, len, ftmp;
    char input[MAXLINE], type[MAXLINE], url[MAXLINE], def[MAXLINE], num[16];
    filebuf_t *buf;
    SYS_FILE fd;
    int ln, i, j, k, eof;

    param_free(pblock_remove("content-type", rq->srvhdrs));

    if(!coord) {
        imap_die(IMAP_BADREQ, sn, rq);
        return REQ_PROCEED;
    }
    for(t = coord; *t != ','; ++t) {
        if((!*t) || (!isdigit(*t))) {
            imap_die(IMAP_BADREQ, sn, rq);
            return REQ_PROCEED;
        }
    }
    *t++ = '\0';
    testpoint[X] = atoi(coord);

    coord = t;
    while(*t) {
        if(!isdigit(*t)) {
            imap_die(IMAP_BADREQ, sn, rq);
            return REQ_PROCEED;
        }
        ++t;
    }
    testpoint[Y] = atoi(coord);

    if((fd = system_fopenRO(path)) == SYS_ERROR_FD) {
        log_error(LOG_WARN, "imagemap", sn, rq, XP_GetAdminStr(DBT_serviceError3), path,
                  system_errmsg());
        protocol_status(sn, rq, 
                    file_notfound() ? PROTOCOL_NOT_FOUND : PROTOCOL_FORBIDDEN,
                    NULL);
        return REQ_ABORTED;
    }
    if(!(buf = filebuf_open(fd, FILE_BUFFERSIZE))) {
        log_error(LOG_WARN,"imagemap", sn, rq, 
                  XP_GetAdminStr(DBT_serviceError6), path, 
                  system_errmsg());
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }
    ln = 1;
    def[0] = '\0';
    closepoint = -1.0;

    for(ln = 1, eof = 0; (!eof); ++ln) {
        if( (eof = util_getline(buf, ln, MAXLINE, input)) == -1) {
            filebuf_close(buf);
            log_error(LOG_WARN, "imagemap", sn, rq, 
                      XP_GetAdminStr(DBT_serviceError9), path, input);
            return REQ_ABORTED;
        }
        if((!input[0]) || (input[0] == '#'))
            continue;

        type[0] = '\0'; url[0] = '\0';

        for(i=0;isname(input[i]) && (input[i]);i++)
            type[i] = input[i];
        type[i] = '\0';

        while(isspace(input[i])) ++i;
        for(j=0;input[i] && isname(input[i]);++i,++j)
            url[j] = input[i];
        url[j] = '\0';

        if((closepoint < 0) && (!strcmp(type,"default"))) {
            strcpy(def,url);
            continue;
        }
        k=0;
        while (input[i]) {
            while (isspace(input[i]) || input[i] == ',')
                i++;
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];
            num[j] = '\0';
            if (num[0] != '\0')
                pointarray[k][X] = (double) atoi(num);
            else
                break;
            while (isspace(input[i]) || input[i] == ',')
                i++;
            j = 0;
            while (isdigit(input[i]))
                num[j++] = input[i++];
            num[j] = '\0';
            if (num[0] != '\0')
                pointarray[k++][Y] = (double) atoi(num);
            else {
                filebuf_close(buf);
                imap_die("Missing y value.", sn, rq);
                return REQ_PROCEED;
            }
        }
        pointarray[k][X] = -1;
        if(!strcasecmp(type,"poly")) {
            if(pointinpoly(testpoint,pointarray)) {
                filebuf_close(buf);
                return imap_handle(url, sn, rq, path, ncsa_compat);
            }
        }
        else if(!strcasecmp(type,"circle")) {
            double radius1, radius2, t;

            t = pointarray[0][Y] - pointarray[1][Y];
            radius1 = t * t;
            t = pointarray[0][X] - pointarray[1][X];
            radius1 += t * t;

            t = (pointarray[0][Y] - testpoint[Y]);
            radius2 = t * t;
            t = (pointarray[0][X] - testpoint[X]);
            radius2 += t * t;

            if(radius2 <= radius1) {
                filebuf_close(buf);
                return imap_handle(url, sn, rq, path, ncsa_compat);
            }
        }
        else if(!strcasecmp(type,"rect")) {
            if((testpoint[X] >= pointarray[0][X]) && 
               (testpoint[X] <= pointarray[1][X]) &&
               (testpoint[Y] >= pointarray[0][Y]) && 
               (testpoint[Y] <= pointarray[1][Y]))
            {
                filebuf_close(buf);
                return imap_handle(url, sn, rq, path, ncsa_compat);
            }
        }
        else if(!strcasecmp(type, "point")) {
            ftmp = (testpoint[X] - pointarray[0][X]);
            len = ftmp*ftmp;
            ftmp = (testpoint[Y] - pointarray[0][Y]);
            len += ftmp*ftmp;
            if((closepoint < 0) || (len < closepoint)) {
                closepoint = len;
                strcpy(def, url);
            }
        }
    }
    filebuf_close(buf);
    if(def[0])
        return imap_handle(def, sn, rq, path, ncsa_compat);
#if 0
    imap_die("No default URL specified in map file. Sorry.", sn, rq);
#else
    protocol_status(sn, rq, PROTOCOL_NO_RESPONSE, NULL);
    protocol_start_response(sn, rq);
#endif
    return REQ_PROCEED;
}



/* ------------------------- service_disable_type ------------------------- */


int service_disable_type(pblock *pb, Session *sn, Request *rq)
{
    char *type = pblock_findval("content-type", rq->srvhdrs);

    log_error(LOG_WARN, "disable-type", sn, rq,
              XP_GetAdminStr(DBT_serviceError10), type ? type : "(none)");
    return REQ_ABORTED;
}
/* ------------------------- service_keytoosmall -------------------------- */



#define CRYPT_HEADER "Insufficient encryption"

int service_keytoosmall(pblock *pb, Session *sn, Request *rq)
{
    char buf[MAGNUS_ERROR_LEN];
    int l;

    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);

    protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
    protocol_start_response(sn, rq);

    l = util_snprintf(buf, MAGNUS_ERROR_LEN, "<title>%s</title><h1>%s</h1>\n", CRYPT_HEADER,
                     CRYPT_HEADER);
    l += util_snprintf(&buf[l], MAGNUS_ERROR_LEN - l, "This document requires a larger secret key size for encryption than your browser is capable of supporting.\n");

    net_write(sn->csd, buf, l);
    return REQ_PROCEED;
}


int
service_toobusy(pblock *pb, Session *sn, Request *rq)
{
    protocol_status(sn, rq, PROTOCOL_SERVICE_UNAVAILABLE, NULL);

    char *fname;

    if(!(fname = pblock_findval("fn", pb)))
        fname = "unknown";

    log_error(LOG_VERBOSE, "service-toobusy", sn, rq,
              XP_GetAdminStr(DBT_serviceError11), fname);

    return REQ_ABORTED;
}


int 
SendHead(pblock *param, Session *sn, Request *rq) 
{
    int res = REQ_PROCEED;
    if (protocol_start_response(sn, rq) == REQ_ABORTED)
        res = REQ_ABORTED;

    return res;
}
