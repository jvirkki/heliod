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

#ifdef XP_UNIX
#include <signal.h>
#endif

#include "frame/servact.h"        /* servact_translate_uri */
#include "frame/log.h"        /* log_error */
#include "frame/conf.h"        /* log_error */
#include "base/util.h"          /* util_waitpid */
#include "safs/child.h"


#include "ExecHandler.h"
#include "ShtmlConfig.h"
#include "ShtmlHelperFuncs.h"
#include "dbtShtml.h"



PRBool
ExecHandler::Execute(pblock* pb, Session* sn, Request* rq, PageStateHandler& pg)
{
  ShtmlConfig cfg(pb);
  PRBool res = IsValid();
  if (cfg.AllowExecTag() == PR_FALSE) {
    log_error(LOG_MISCONFIG, "shtml-exec", sn, rq,
              XP_GetAdminStr(DBT_execHandlerError1));
    res = PR_FALSE;
  }
  else {
    if (res == PR_TRUE) {
      if ((res = pg.SendHeaders()) == PR_TRUE) {
        if (ISMHEAD(rq)) {
          return res;
        }
        if (_type == CGI) {
          res = ExecuteCgi(pb, sn, rq, pg);
        }
        else if (_type == CMD) {
          res = ExecuteCmd(pb, sn, rq, pg);
        }
      }
    }
  }

  if (res == PR_FALSE) {
    pg.WriteErrMsg();
  }
  return res;
}


PRBool 
ExecHandler::ExecuteCgi(pblock* pb, 
                        Session* sn, Request* origRq, PageStateHandler& pg)
{
  PRBool res;

  pblock* param = pblock_dup(pg.GetNonEnvVars());

  // Remove variables that would conflict with those set by cgi_specific_vars()
  param_free(pblock_remove("PATH_INFO", param));
  param_free(pblock_remove("QUERY_STRING", param));
  param_free(pblock_remove("QUERY_STRING_UNESCAPED", param));

  switch (servact_include_virtual(sn, origRq, _cgi, param)) {
  case REQ_PROCEED:
    res = PR_TRUE;
    break;

  case REQ_NOACTION:
    log_error(LOG_MISCONFIG, "shtml-include", sn, origRq,
              XP_GetAdminStr(DBT_execHandlerError3), _cgi);
    res = PR_FALSE;
    break;

  default:
    res = PR_FALSE;
    break;
  }

  pblock_free(param);

  return res;
}

PRBool 
ExecHandler::ExecuteCmd(pblock* pb, Session* sn, Request* rq, 
                        PageStateHandler& pg)
{
  PRBool res = PR_FALSE;

  Child* child = child_create(sn, rq, _cgi);
  PRIntervalTime ioTimeout = cgi_get_idle_timeout();
  PRFileDesc* c2s = child_pipe(child, PR_StandardOutput, ioTimeout);

  if (child_shell(child, pg.GetEnv(), NULL, PR_INTERVAL_NO_TIMEOUT) ==
      PR_SUCCESS)
  {
    netbuf* b = netbuf_open(c2s, NET_BUFFERSIZE);
    int bytesSent = netbuf_buf2sd(b, sn->csd, -1);
    if (bytesSent != IO_ERROR) {
      res = PR_TRUE;
    }
    else {
      log_error(LOG_FAILURE, "parse-html", sn, rq,
                XP_GetAdminStr(DBT_execHandlerError6), system_errmsg());
    }
    netbuf_close(b);
    child_done(child);
  }
  else {
    log_error(LOG_FAILURE, "parse-html", sn, rq,
              XP_GetAdminStr(DBT_execHandlerError7),
              _cgi, system_errmsg);
  }

  return res;
}
