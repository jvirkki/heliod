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

#ifndef CM_STAT_H
#define CM_STAT_H


#define CM_SERVICES_NS      "PageServices"
#define CM_PROPERTY_STR     "properties"
#define CM_TOC_NS           "wp-toc"
#define CM_SYS_PROP_NS      "wp-sys-prop"
#define CM_CUSTOM_FEILD_NS  "wp-custom-info"
#define CM_USR_PROP_NS      "wp-usr-prop"
#define CM_HTML_REND_NS     "wp-html-rend"
#define CM_VER_INFO_NS      "wp-ver-info"
#define CM_VER_DIFF_NS      "wp-ver-diff"
#define CM_START_VERSION_NS "wp-start-ver"
#define CM_STOP_VERSION_NS  "wp-stop-ver"
#define CM_UNCHECKOUT_NS    "wp-uncheckout"
#define CM_LINK_INFO_NS     "wp-link-info"
#define CM_VER_LINKS_NS     "wp-verify-link"
#define CM_ADMIN_DUMP       "wp-cs-dump"


typedef  int (*CMTriggerProc)( pblock *pb, Session *sn, Request *rq, short message);
struct CMVtbl {
  CMTriggerProc CMTrigger;
};

  
typedef struct CMVtbl CMVtbl;


NSAPI_PUBLIC void CM_enable_cm(void *CMTrigger);
NSAPI_PUBLIC void CM_disable_cm(void);
NSAPI_PUBLIC int CM_get_status(void);
NSAPI_PUBLIC void CM_set_link_header(Request *rq);
NSAPI_PUBLIC int CM_get_link_header(char *uri, char *buf, int size);
NSAPI_PUBLIC int CM_is_cm_query( char *query );


#endif  /* CM_STAT_H */
