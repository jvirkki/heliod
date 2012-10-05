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

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include "definesEnterprise.h"
#include "wdlog.h"


#if defined(AIX)
/*
 * AIX is completely lame and unable to
 * handle thread safety in a transparent 
 * way.
 */
struct syslog_data _watchdog_syslogdata = SYSLOG_DATA_INIT;
#endif

void
watchdog_openlog(void)
{
#if defined(AIX)
  openlog_r(PRODUCT_WATCHDOG_BIN, LOG_PID|LOG_CONS|LOG_NOWAIT, LOG_DAEMON, &_watchdog_syslogdata);
  setlogmask_r(LOG_UPTO(LOG_ERR), &_watchdog_syslogdata);
#else
  openlog(PRODUCT_WATCHDOG_BIN, LOG_PID|LOG_CONS|LOG_NOWAIT, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_ERR));
#endif
  watchdog_log(LOG_INFO,
	       "logging initialized info");
}

void
watchdog_closelog(void)
{
#if defined(AIX)
  closelog_r(&_watchdog_syslogdata);
#else
  closelog();
#endif
}

void
watchdog_syslog(int priority, const char *fmt, ...)
{
  va_list args;
#if !defined(IRIX) && !defined(SOLARIS)
  char msgbuffer[8192]; /* Big enough? */
#endif

  va_start(args, fmt);

#if defined(AIX)
  vsprintf(msgbuffer, fmt, args);
  syslog_r(priority, &_watchdog_syslogdata, msgbuffer);
#elif defined(IRIX) || defined(SOLARIS)
  vsyslog(priority, fmt, args);
#else
  /* HPUX, DEC, everyone else */
  vsprintf(msgbuffer, fmt, args);
  syslog(priority, msgbuffer);
#endif
}
