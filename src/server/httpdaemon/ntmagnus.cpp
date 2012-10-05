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
 * ntmagnus.c: Startup processing etc.
 * 
 * Rob McCool
 * 
 */

#include "base/daemon.h"
#include "nt/magnus.h"

/*#include <winsock.h>
#include <process.h>

#include "definesEnterprise.h"
#include "dbthttpdsrc.h"
#include "prtypes.h"
#include "prthread.h"
#include "netsite.h"
#include "base/daemon.h"
#include "base/ereport.h"
#include "base/eventlog.h"
#include "base/util.h"
#include "base/cinfo.h"
#include "base/systems.h"
#include "base/eventhandler.h"
#include "base/time_cache.h"
#include "base/systhr.h"
#include "base/pool.h"
#include "base/servnss.h"

#include <libaccess/acl.h>
#include <libaccess/aclproto.h>

#include <frame/aclinit.h>
#include "frame/conf.h"
#include "frame/conf_api.h"
#include "frame/req.h"
#include "frame/func.h"
#include "frame/protocol.h"
#include "frame/accel_file_cache.h"

#include "httpdaemon/HttpMethodRegistry.h"

#include "nt/messages.h"
#include "nt/regparms.h"
// #include "nt/nsapi.h"
#include "nt/magnus.h" */

/* #include "httpdaemon/WebServer.h"

#define GetConfigParm(name, default) \
    (conf_findGlobal(name)?conf_findGlobal(name):default)

#include <safs/acl.h>
#include "safs/addlog.h"
#include "safs/flexlog.h"
#include "safs/nsfcsafs.h"

//#include <sec.h>

#include "httpd-fn.h" // func_standard table
#include "httpdaemon/httpdaemon.h"
#include "httpdaemon/multihome.h"

  */

NSAPI_PUBLIC void magnus_atrestart(void (*fn)(void *), void *data)  
{                                                                   
#if 0                                                               
    daemon_atrestart(fn, data);                                     
#endif                                                              
}                                                                   

