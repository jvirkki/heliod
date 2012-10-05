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

#include <string.h>

#include "frame/req.h"
#include "base/pblock.h"
#include "frame/log.h"
#include "frame/protocol.h"

#include "safs/preencrypted.h"
#include "safs/dbtsafs.h"

extern "C" {
#include "preenc.h"
}

//-----------------------------------------------------------------------------
// service_preencrypted
//-----------------------------------------------------------------------------

int service_preencrypted(pblock *param, Session *sn, Request *rq)
{
    PEHeader *header;
    char *path, *t;
    SYS_FILE fd;
    filebuf_t *buf;
    struct stat *finfo;
    char tmpbuf[128];
    int peheader;

    path = pblock_findval("path", rq->vars);
    if( (t = pblock_findval("path-info", rq->vars)) ) {
        protocol_status(sn, rq, PROTOCOL_NOT_FOUND, NULL);
        log_error(LOG_WARN, "send-file", sn, rq,
                  XP_GetAdminStr(DBT_preencError1), path, t);
        return REQ_ABORTED;
    }
    if(!(finfo = request_stat_path(path, rq))) {
        protocol_status(sn, rq, (rtfile_notfound() ? PROTOCOL_NOT_FOUND :
                                 PROTOCOL_FORBIDDEN), NULL);
        log_error(LOG_WARN, "send-file", sn, rq, 
                  XP_GetAdminStr(DBT_preencError2),
                  path, rq->staterr);
        return REQ_ABORTED;
    }

    if(protocol_set_finfo(sn, rq, finfo) == REQ_ABORTED)
        return REQ_ABORTED;

    if((fd = system_fopenRO(path)) == SYS_ERROR_FD) {
        log_error(LOG_WARN, "send-file", sn, rq, 
                  XP_GetAdminStr(DBT_preencError3),
                  path, system_errmsg());
        protocol_status(sn, rq, (file_notfound() ? PROTOCOL_NOT_FOUND :
                                 PROTOCOL_FORBIDDEN), NULL);
        return REQ_ABORTED;
    }

    if(!(buf = filebuf_open_nostat(fd, FILE_BUFFERSIZE, finfo))) {
        log_error(LOG_WARN,"send-file", sn, rq,
                  XP_GetAdminStr(DBT_preencError4), path,
              system_errmsg());
        system_fclose(fd);
        protocol_status(sn, rq, PROTOCOL_FORBIDDEN, NULL);
        return REQ_ABORTED;
    }

    pblock_nvinsert("accept-ranges", "bytes", rq->srvhdrs);
    if(!(pblock_find("status", rq->srvhdrs)))
        protocol_status(sn, rq, PROTOCOL_OK, NULL);

    if(protocol_start_response(sn,rq) != REQ_NOACTION) {

        if (filebuf_grab(buf,sizeof(tmpbuf)) == IO_EOF) {
            filebuf_close(buf);
            return REQ_EXIT;
        }
        memcpy(tmpbuf,(char *)buf->inbuf,buf->cursize);
        /*
         * this is broken, but it works because the only version of
         * SSL_PreencryptedFileToStream will actually return tmpbuf..
         * We need to fix this later...... */
        header = SSL_PreencryptedFileToStream(sn->csd, (PEHeaderStr *)tmpbuf, &peheader);
        if (header == NULL) {
            filebuf_close(buf);
            return REQ_EXIT;
        }
        if (net_write(sn->csd, (char *)header, buf->cursize) == IO_ERROR) {
            filebuf_close(buf);
            return REQ_EXIT;
        }
        while (filebuf_grab(buf,NET_BUFFERSIZE) != IO_EOF) {
            if (net_write(sn->csd,(char *)buf->inbuf,buf->cursize)==IO_ERROR) {
                filebuf_close(buf);
                return REQ_EXIT;
            }
        }
    }
    filebuf_close(buf);

    return REQ_PROCEED;
}
