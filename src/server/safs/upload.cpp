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
 * upload.c: File upload for livewire
 * 
 * Rob McCool
 */


#include "safs/upload.h"
#include "base/systems.h"

#include "base/pblock.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/servact.h"

#include "base/util.h"
#include "base/file.h"
#include "frame/log.h"
#include "frame/httpfilter.h"
#include "frame/protocol.h"

#include "base/cinfo.h"
#include "safs/dbtsafs.h"

#include "safs/nsfcsafs.h"

#define UPLOAD_METHOD "PUT"
#define RENAME_METHOD "MOVE"
#define INDEX_METHOD "INDEX"
#define DELETE_METHOD "DELETE"
#define MKDIR_METHOD "MKDIR"
#define RMDIR_METHOD "RMDIR"


#ifndef FILENAME_MAX
#define FILENAME_MAX 1024
#endif


#define REMOVED_MESSAGE \
    "<title>File removed</title>\n" \
    "<h1>File removed</h1>\n" \
    "Your file has been scheduled for removal."

#define RMDIR_MESSAGE \
    "<title>Directory removed</title>\n" \
    "<h1>Directory removed</h1>\n" \
    "Your directory has been removed."

#define RENAMED_MESSAGE \
    "<title>File renamed</title>\n" \
    "<h1>File renamed</h1>\n" \
    "Your file has been renamed."

static void _insertContentLength(Request& rq, int len);

/* This is netbuf_buf2sd with net_write replaced with system_fwrite.
   It also is designed to swallow any remaining data if the writes fail.
 */
int 
_netbuf_buf2fd(netbuf *buf, SYS_FILE fd, int len)
{
    register int n = len, t, ns;
    int write_error = 0;
    int bytes_written;

    ns = 0;

    /* First, flush the current buffer */
    t = buf->cursize - buf->pos;

    if((n != -1) && (t > n))
        t = n;
    if((t) && (fd != SYS_ERROR_FD)) {
        if(system_fwrite(fd, (char *)&buf->inbuf[buf->pos], t) == IO_ERROR) {
            buf->errmsg = system_errmsg();
            write_error = 1;
            fd = SYS_ERROR_FD;
        }
        ns += t;
    }
    buf->pos += t;

    if(n != -1) {
        n -= t;
        if(!n)
            return (write_error ? IO_ERROR : ns);
    }
    else 
        t = buf->maxsize;

    /* Now, keep blasting until done */

    while(1) {
        if(n != -1)
            t = (n < buf->maxsize ? n : buf->maxsize);

        switch(netbuf_grab(buf, t)) {
          case IO_ERROR:
            return IO_ERROR;
          case IO_EOF:
            if(n == -1)
                return ns;
            else {
                buf->errmsg = "premature EOF";
                return IO_ERROR;
            }
          default:
            if(fd != SYS_ERROR_FD) {
                if(system_fwrite(fd, (char *)(buf->inbuf), buf->cursize) == IO_ERROR) {
                    buf->errmsg = system_errmsg();
                    write_error = 1;
                    fd = SYS_ERROR_FD;
                }
                bytes_written = buf->cursize;
                ns += bytes_written;
                buf->cursize = 0;  // since we wrote out the data, it is consumed
            }
            if(n != -1) {
                n -= bytes_written;
                if(!n)
                    return (write_error ? IO_ERROR : ns);
            }
            break;
        }
    }
}


/* ----------------------------- upload_file ------------------------------ */


int
upload_file(pblock *pb, Session *sn, Request *rq)
{
    char *t, *path;
    struct stat fi;
    SYS_FILE fd;
    int cl, existed, wrote, rv;

    if ( !ISMPUT(rq))
        return REQ_NOACTION;

    t = pblock_findval("content-length", rq->headers);
    if(t) {
        cl = atoi(t);
        if(cl < 0) {
            protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
            log_error(LOG_WARN, "upload-file", sn, rq, 
                      "bad client: invalid content-length header");
            return REQ_ABORTED;
        }
    } else {
        cl = -1;
    }

    path = pblock_findval("path", rq->vars);
    existed = (system_stat(path, &fi) == -1 ? 0 : 1);

    /* Make any parents first */
    if(!existed) {
        t = strrchr(path, FILE_PATHSEP);
        if(t && (t != path)) {
            *t = '\0';
            rv = dir_create_all(path);
            *t = FILE_PATHSEP;
            if(rv == -1) {
                log_error(LOG_FAILURE, "upload-file", sn, rq,
                          "can't create directory for %s (%s)", path, 
                          system_errmsg());
                protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
                return REQ_ABORTED;
            }
        }
    }

    /* Backup??? */
    if( (fd = system_fopenWT(path)) == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, "upload-file", sn, rq,
                  "can't write to %s (%s)", path, system_errmsg());
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    /*
     * If request is HTTP/1.1 or greater and "If-Match" or "If-None-Match"
     * is present, then make use of the Etag.  Usefull in detecting race
     * condition.
     */	
    if ( existed && rq->protv_num >= PROTOCOL_VERSION_HTTP11 ) {
        /* Format the Etag for the resource we're replacing */
        char etag[MAX_ETAG];
        http_format_etag(sn, rq, etag, sizeof(etag), fi.st_size, fi.st_mtime);

        /* Check If-match, etc. */
        struct tm mtms;
        struct tm *mtm = system_gmtime(&fi.st_mtime, &mtms);
        rv = http_check_preconditions(sn, rq, mtm, etag);
        if (rv != REQ_PROCEED)
            return rv;
    }

    wrote = _netbuf_buf2fd(sn->inbuf, fd, cl);

    system_fclose(fd);

    NSFCCache nsfcCache = GetServerFileCache();
    NSFC_RefreshFilename(path, nsfcCache);

    /* Indicate that we've already taken the data */
    pblock_nvinsert("data-removed", "1", rq->vars);

    if(wrote == IO_ERROR) {
        log_error(LOG_FAILURE, "upload-file", sn, rq,
                  "can't write to %s (%s)", path, sn->inbuf->errmsg);
        return REQ_ABORTED;
    }
    if((cl == -1) || (wrote == cl)) {
        int protocolStatus = PROTOCOL_CREATED;
        if (existed) {
            protocolStatus = PROTOCOL_NO_RESPONSE;
        }
        else {
            protocolStatus = PROTOCOL_CREATED;
            pblock_nvinsert("content-length", "0", rq->srvhdrs);
        }
        protocol_status(sn, rq, 
                        protocolStatus,
                        NULL);
        
        param_free(pblock_remove("content-type", rq->srvhdrs));
        /* Hmm. Catch REQ_ABORTED? */
        protocol_start_response(sn, rq);
        return REQ_PROCEED;
    }
    log_error(LOG_FAILURE, "upload-file", sn, rq,
              "can't write to %s (%s)", path, sn->inbuf->errmsg);
    return REQ_ABORTED;
}


/* ----------------------------- remove_file ------------------------------ */


int
remove_file(pblock *pb, Session *sn, Request *rq)
{
    char *path = pblock_findval("path", rq->vars);
    struct stat fi;

	if ( !ISMDELETE(rq) )
        return REQ_NOACTION;
    if(system_stat(path, &fi) == -1) {
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        log_error(LOG_FAILURE, "delete-file", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror1), path);
        return REQ_ABORTED;
    }
    if(!S_ISREG(fi.st_mode)) {
        protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
        log_error(LOG_FAILURE, "delete-file", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror2), path);
        return REQ_ABORTED;
    }
    if(system_unlink(path) == -1) {
        PRErrorCode err = PR_GetError();
        PRInt32     osErr = PR_GetOSError();
        if (err == PR_FILE_NOT_FOUND_ERROR) {
            protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        }
        else {
            protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        }
        log_error(LOG_FAILURE, "delete-file", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror3), path, system_errmsg());
        return REQ_ABORTED;
    }

    NSFCCache nsfcCache = GetServerFileCache();
    NSFC_RefreshFilename(path, nsfcCache);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    int len = strlen(RENAMED_MESSAGE);
    _insertContentLength(*rq, len);
    if(protocol_start_response(sn, rq) != REQ_ABORTED) {
        net_write(sn->csd, REMOVED_MESSAGE, len);
    }
    return REQ_PROCEED;
}


/* ----------------------------- rename_file ------------------------------ */


int
rename_file(pblock *pb, Session *sn, Request *rq)
{
    char *uri = pblock_findval("uri", rq->reqpb);
    char *newuri = pblock_findval("new-uri", rq->headers);
    char *path = pblock_findval("path", rq->vars);
    char *newpath;
    char *t;
    int len;

	if ( !ISMMOVE(rq) )
        return REQ_NOACTION;
    if(!newuri) {
        protocol_status(sn, rq, PROTOCOL_BAD_REQUEST, NULL);
        log_error(LOG_INFORM, "rename-file", sn, rq,
                  XP_GetAdminStr(DBT_uploaderror4),
                  path);
        return REQ_ABORTED;
    }
    t = strrchr(uri, FILE_PATHSEP);
    len = t - uri;
#if 0  /* XXXrobm this check does not work properly for foo/ rename to bar/ */
    if(strncmp(uri, newuri, len) || strchr(newuri+len+1, '/')) {
        protocol_status(sn, rq, PROTOCOL_NOT_IMPLEMENTED, NULL);
        log_error(LOG_INFORM, "rename-file", sn, rq,
                  "client asked to rename %s but this revision of the server "
                  "does not allow renaming into new directories", path);
        return REQ_ABORTED;
    }
#endif
    if(!(newpath = servact_translate_uri(newuri, sn))) {
        log_error(LOG_FAILURE, "rename-file", sn, rq,
                  XP_GetAdminStr(DBT_uploaderror5), newuri);
        return REQ_ABORTED;
    }
    if(system_rename(path, newpath) == -1) {
        PRErrorCode err = PR_GetError();
        PRInt32     osErr = PR_GetOSError();
        if (err == PR_FILE_NOT_FOUND_ERROR) {
            protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        }
        log_error(LOG_FAILURE, "rename-file", sn, rq,
                  XP_GetAdminStr(DBT_uploaderror6), uri, newuri,
                  system_errmsg());
        return REQ_ABORTED;
    }

    NSFCCache nsfcCache = GetServerFileCache();
    NSFC_RefreshFilename(path, nsfcCache);
    NSFC_RefreshFilename(newpath, nsfcCache);

    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    int msglen = strlen(RENAMED_MESSAGE);
    _insertContentLength(*rq, msglen);
    if(protocol_start_response(sn, rq) != REQ_ABORTED) {
        net_write(sn->csd, RENAMED_MESSAGE, msglen);
    }
    return REQ_PROCEED;
}


/* ------------------------------- make_dir ------------------------------- */


int
make_dir(pblock *pb, Session *sn, Request *rq)
{
    char *path = pblock_findval("path", rq->vars);
    struct stat fi;

    if(system_stat(path, &fi) != -1) {
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        log_error(LOG_WARN, "make-dir", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror7), path);
        return REQ_ABORTED;
    }
    if(dir_create_all(path) == -1) {
        /* Check errno? */
        log_error(LOG_WARN, "make-dir", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror8), path, system_errmsg());
        return REQ_ABORTED;
    }
    protocol_status(sn, rq, PROTOCOL_CREATED, NULL);
    pblock_nvinsert("content-length", "0", rq->srvhdrs);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    protocol_start_response(sn, rq);
    return REQ_PROCEED;
}


/* ------------------------------ remove_dir ------------------------------ */


int
remove_dir(pblock *pb, Session *sn, Request *rq)
{
    char *path = pblock_findval("path", rq->vars);
    struct stat fi;

    if(system_stat(path, &fi) == -1) {
        log_error(LOG_WARN, "remove-dir", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror9), path, system_errmsg());
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        return REQ_ABORTED;
    }
    /* XXXrobm must do mkdir -p */
    if(dir_remove(path) == -1) {
        /* Check errno? */
        log_error(LOG_WARN, "remove-dir", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror9), path, system_errmsg());
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/html", rq->srvhdrs);
    int len = strlen(RMDIR_MESSAGE); 
    _insertContentLength(*rq, len);
    if(protocol_start_response(sn, rq) != REQ_ABORTED) {
        net_write(sn->csd, RMDIR_MESSAGE, len);
    }
    return REQ_PROCEED;
}


/* ------------------------------- list_dir ------------------------------- */


int
list_dir(pblock *pb, Session *sn, Request *rq)
{
    char *path = pblock_findval("path", rq->vars), *escname, *ct, *uri;
    char buf[3*FILENAME_MAX + 100];
    SYS_DIR ds;
    SYS_DIRENT *d;
    struct stat fi;
    cinfo *ci;

    if ( !ISMINDEX(rq) )
        return REQ_NOACTION;

    /*
       There might be an index file in this directory. Make sure they're 
       really asking for a directory, and then change the path to get it
       instead of the index file. 
     */
    if(path[strlen(path) - 1] != FILE_PATHSEP) {
        uri = pblock_findval("uri", rq->reqpb);
        if(uri[strlen(uri) - 1] != '/') {
            protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
            log_error(LOG_WARN, "list-dir", sn, rq, 
                      XP_GetAdminStr(DBT_uploaderror10), path);
            return REQ_ABORTED;
        }
        *(strrchr(path, FILE_PATHSEP) + 1) = '\0';
    }
    if(!(ds = dir_open(path))) {
        log_error(LOG_WARN, "list-dir", sn, rq, 
                  XP_GetAdminStr(DBT_uploaderror11), path, system_errmsg());
        protocol_status(sn, rq, (file_notfound() ? PROTOCOL_NOT_FOUND : 
                                 PROTOCOL_FORBIDDEN), NULL);
        return REQ_ABORTED;
    }

    httpfilter_buffer_output(sn, rq, PR_TRUE);
    param_free(pblock_remove("content-type", rq->srvhdrs));
    pblock_nvinsert("content-type", "text/plain", rq->srvhdrs);
    protocol_status(sn, rq, PROTOCOL_OK, NULL);
    while( (d = dir_read(ds)) ) {
        if(d->d_name[0] == '.')
            continue;
        util_snprintf(buf, sizeof(buf), "%s%s", path, d->d_name);
        if(system_stat(buf, &fi) == -1)
            continue;
        escname = util_uri_escape(NULL, d->d_name);
        ci = NULL;
        if(S_ISDIR(fi.st_mode))
            ct = "directory";
        else
            ct = (ci = cinfo_find(escname)) ? ci->type : (char *)"unknown";
        
        if (ci) 
            FREE(ci);
        if ((strlen(escname) + strlen(ct) + 50) < sizeof(buf)) {
            sprintf(buf, "%s %s %d %lu\n", escname, ct, 
                    (int)fi.st_size, (unsigned long) fi.st_mtime);
            net_write(sn->csd, buf, strlen(buf));
        }
        else {
            sprintf(buf, "%s %d %lu\n", ct, (int)fi.st_size, 
                    (unsigned long) fi.st_mtime);
            net_write(sn->csd, escname, strlen(escname));
            net_write(sn->csd, buf, strlen(buf));
        }
        FREE(escname);
    }
    dir_close(ds);
    return REQ_PROCEED;
}

void _insertContentLength(Request& rq, int len)
{
    char msgLen[24];
    sprintf(msgLen, "%d", len);
    pblock_nvinsert("content-length", msgLen, rq.srvhdrs);
}
