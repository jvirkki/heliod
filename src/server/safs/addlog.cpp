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

#include "netsite.h"
#include "safs/addlog.h"
#include "safs/dbtsafs.h"
#include "base/vs.h"
#include "base/date.h"
#include "frame/req.h"
#include "frame/object.h"
#include "frame/conf.h"
#include "frame/log.h"
#include "frame/http_ext.h"
#include "base/util.h"
#include "httpdaemon/logmanager.h"

struct clog {
    char *logname;
    char *filename;

    struct clog *next;
};

struct ClfVsLog {
    ClfVsLog* next;
    char* filename;
    struct clog* c;
    LogFile* file;
};

static struct clog *cl = NULL;
static int slotClfVsLog = -1;


//-----------------------------------------------------------------------------
// clf_init_vs
//-----------------------------------------------------------------------------

static int clf_init_vs(VirtualServer* incoming, const VirtualServer* current)
{
    // For simplicity, start out with a fresh list of ClfVsLog*s each time
    vs_set_data(incoming, &slotClfVsLog, 0);

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// clf_init_vs_directive
//-----------------------------------------------------------------------------

static int clf_init_vs_directive(const directive* dir, VirtualServer* incoming, const VirtualServer* current)
{
    // Get this directive's log name
    char* logname = pblock_findkeyval(pb_key_name, dir->param.pb);
    if (!logname) logname = "global";

    // Get the clog* for this log name
    struct clog* c;
    for (c = cl; c; c = c->next) {
	if (!strcmp(c->logname, logname)) break;
    }
    if (!c) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_addlogEreport1), logname);
        return REQ_ABORTED;
    }

    // Get the filename for this clog* and VS
    char* filename = vs_substitute_vars(incoming, c->filename);
    if (!filename) {
        ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_addlogEreport2), c->logname, c->filename, vs_get_id(incoming));
        return REQ_ABORTED;
    }

    // Create a new ClfVsLog* for this filename and clog*
    ClfVsLog* vslog = (ClfVsLog*)PERM_MALLOC(sizeof(*vslog) + strlen(filename) + 1);
    if (vslog) {
        vslog->filename = (char*)(vslog + 1);
        strcpy(vslog->filename, filename);
        vslog->c = c;
    }
    FREE(filename);
    if (!vslog) return REQ_ABORTED;

    // Open the log file
    vslog->file = LogManager::getFile(vslog->filename);
    LogManager::openFile(vslog->file);

    // Add this ClfVsLog* to the VS's list
    vslog->next = (ClfVsLog*)vs_get_data(incoming, slotClfVsLog);
    vs_set_data(incoming, &slotClfVsLog, vslog);

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// clf_destroy_vs
//-----------------------------------------------------------------------------

static void clf_destroy_vs(VirtualServer* outgoing)
{
    ClfVsLog* vslog = (ClfVsLog*)vs_get_data(outgoing, slotClfVsLog);
    while (vslog) {
        ClfVsLog* next = vslog->next;
        LogManager::unref(vslog->file);
        PERM_FREE(vslog);
        vslog = next;
    }
    vs_set_data(outgoing, &slotClfVsLog, 0);
}

//-----------------------------------------------------------------------------
// clf_get_log
//-----------------------------------------------------------------------------

static ClfVsLog* clf_get_log(Request* rq, pblock* pb)
{
    // Find the ClfVsLog* list for this VS
    ClfVsLog* vslog = (ClfVsLog*)vs_get_data(request_get_vs(rq), slotClfVsLog);
    if (vslog && vslog->next) {
        // There's more than one log; look for one named logname
        char *logname = pblock_findkeyval(pb_key_name, pb);
        if (!logname) logname = "global";
        while (vslog) {
            if (!strcmp(vslog->c->logname, logname)) break;
            vslog = vslog->next;
        }
    }
    PR_ASSERT(vslog);
    return vslog;
}

//-----------------------------------------------------------------------------
// clf_terminate
//-----------------------------------------------------------------------------

static void clf_terminate(void *data)
{
    struct clog *p;

    while(cl) {
        PERM_FREE(cl->logname);
        PERM_FREE(cl->filename);

        p = cl;
        cl = cl->next;
        PERM_FREE(p);
    }
}

//-----------------------------------------------------------------------------
// clf_init
//-----------------------------------------------------------------------------

int clf_init(pblock *pb, Session *sn, Request *rq)
{
    struct pb_entry *p;
    struct clog *c;
    register int x;
    register char *t;

    if (!conf_is_late_init(pb)) {
        // We want to run LateInit (when configuration is ready)
        pblock_nvinsert("LateInit", "yes", pb);
        return REQ_PROCEED;
    }

    param_free(pblock_remove("server-version", pb));
    param_free(pblock_remove("LateInit", pb));

    LogManager::setParams(pb);

    for(x = 0; x < pb->hsize; x++) {
        p = pb->ht[x];
        while(p) {
            t = p->param->name;

            if(strcmp(t, "fn") && (!LogManager::isParam(t))) {
                c = (struct clog *) PERM_MALLOC(sizeof(struct clog));
                c->logname = PERM_STRDUP(t);
                c->filename = PERM_STRDUP(p->param->value);
                c->next = cl;
                cl = c;

            }
            p = p->next;
        }
    }

    if (slotClfVsLog == -1) {
        // We use this slot to store a per-VS list of ClfVsLog*s
        slotClfVsLog = vs_alloc_slot();

        // Call clf_init_vs()/clf_destroy_vs() on VS init/destruction
        vs_register_cb(clf_init_vs, clf_destroy_vs);

        // Call clf_init_vs_directive() for each common-log, record-useragent,
        // and record-keysize directive
        vs_directive_register_cb(clf_record, clf_init_vs_directive, 0);
        vs_directive_register_cb(record_useragent, clf_init_vs_directive, 0);
        vs_directive_register_cb(record_keysize, clf_init_vs_directive, 0);
    }

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// clf_record
//-----------------------------------------------------------------------------

static int _clf_record(ClfVsLog* vslog, char *rql, char *user, char *bytes, 
                       char *status, char *rhost, char *cipher, char *user_dn)
{
    char tbuf[28];
    int len;

    if (!user) user = "-";
    if (!bytes) bytes = "-";
    if (!status) status = "-";
    if (!rhost) rhost = "-";
    if (!cipher) cipher = "-";
    if (!user_dn) user_dn = "-";

    date_current_formatted(date_format_clf, tbuf, sizeof(tbuf));

    len = strlen(rhost) + 1 + 
          2 + 
          strlen(user) + 1 + 
          28 + 1 +
          1 + strlen(rql) + 2 +
          3 + 1 +
          strlen(bytes) + 1 +
          strlen(cipher) + 1 +
          2 + strlen(user_dn) + 2 +
          strlen(ENDLINE) + 1;

    LogManager::logf(vslog->file, len, "%s - %s [%s] \"%s\" %-.3s %s %s { %s }" ENDLINE,
                     rhost, user, tbuf, 
                     rql, status, bytes, cipher, user_dn);

    return REQ_PROCEED;
}

int clf_record(pblock *pb, Session *sn, Request *rq)
{
    char *rql = pblock_findkeyval(pb_key_clf_request, rq->reqpb);
    char *user = pblock_findkeyval(pb_key_auth_user, rq->vars);
    char *bytes = pblock_findkeyval(pb_key_content_length, rq->srvhdrs);
    char *status = pblock_findkeyval(pb_key_status, rq->srvhdrs);
    char *iponly = pblock_findkeyval(pb_key_iponly, pb);
    char *host;
    char *cipher = pblock_findval("cipher", sn->client);
    char *user_dn = pblock_findval("user_dn", sn->client);

    if (slotClfVsLog == -1) return REQ_NOACTION;
    if (!rql) return REQ_NOACTION;

    if (iponly || (!(host = session_dns(sn))))
        host = pblock_findkeyval(pb_key_ip, sn->client);

    ClfVsLog* vslog = clf_get_log(rq, pb);
    if (!vslog) return REQ_ABORTED;

    return _clf_record(vslog, rql, user, bytes, status, host, cipher, user_dn);
}

//-----------------------------------------------------------------------------
// record_useragent
//-----------------------------------------------------------------------------

int record_useragent(pblock *pb, Session *sn, Request *rq)
{
    if (slotClfVsLog == -1) return REQ_NOACTION;

    char* rhost = pblock_findkeyval(pb_key_ip, sn->client);
    if (!rhost) rhost = "-";

    char* ua = pblock_findkeyval(pb_key_user_agent, rq->headers);
    if (!ua) ua = "-";

    ClfVsLog* vslog = clf_get_log(rq, pb);
    if (!vslog) return REQ_ABORTED;

    LogManager::logf(vslog->file, 513, "%-.255s %-.255s" ENDLINE, rhost, ua);

    return REQ_PROCEED;
}

//-----------------------------------------------------------------------------
// record_keysize
//-----------------------------------------------------------------------------

int record_keysize(pblock *pb, Session *sn, Request *rq)
{
    char tbuf[28];

    if (slotClfVsLog == -1) return REQ_NOACTION;
    if (!GetSecurity(sn)) return REQ_NOACTION;

    date_current_formatted(date_format_clf, tbuf, sizeof(tbuf));

    char* rhost = pblock_findval("ip", sn->client);
    char* keySize = pblock_findval("keysize", sn->client);
    if(keySize)  {
        ClfVsLog* vslog = clf_get_log(rq, pb);
        if (!vslog) return REQ_ABORTED;

        LogManager::logf(vslog->file, 256, "%-.200s: [%s] using keysize %s" ENDLINE, 
                         rhost ? rhost : "-", tbuf, keySize);
    }  else  {
        log_error(LOG_WARN, "record-keysize", sn, rq, 
                  XP_GetAdminStr(DBT_addlogError2));
    }

    return REQ_PROCEED;
}
