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

#include "frame/log.h"
#include "frame/servact.h"

#include "FileInfoHandler.h"
#include "ShtmlHelperFuncs.h"
#include "dbtShtml.h"

FileInfoHandler::FileInfoHandler(const char* tag, pblock* pb)
: _valid(PR_TRUE), _badFilePath(PR_FALSE), _path(0)
{
  if (strcmp(tag, "flastmod") == 0) {
    _type = FLASTMOD;
  }
  else if (strcmp(tag, "fsize") == 0) {
    _type = FSIZE;
  }
  else {
    _type = UNKNOWN;
    _valid = FALSE;
  }
  const char* file = pblock_findval("virtual", pb);
  if (file) {
    _fType = VIRTUAL;
  }
  else {
    file = pblock_findval("file", pb);
    if (file)
      _fType = ACTUAL;
  }
  if (file)
    _path = PERM_STRDUP(file);
  else 
    _valid = PR_FALSE;

  if (IsValid() == PR_TRUE) {
    _badFilePath = FilePathIsBad(_path);
  }
  
}

FileInfoHandler::~FileInfoHandler()
{
  if (_path)
    PERM_FREE(_path);
}
    
PRBool
FileInfoHandler::Execute(pblock* pb, Session* sn, Request* rq, 
                                 PageStateHandler& pg)
{
  PRBool res = IsValid();
  if (res == PR_TRUE) {
    char* vpath = 0;
    if (_fType == VIRTUAL)
      vpath = _path;
    else {
      const char* virtPath = pg.GetVirtualPath();
      vpath = (char*) MALLOC(strlen(_path) +
                                 strlen(virtPath) + 1);
      if (vpath) {
        util_sprintf(vpath, "%s", virtPath);
        /* Don't use FILE_PATHSEP here. The pathname has not been
         converted to an NT pathname... */
        char* t = 0;
        if(!(t = strrchr(vpath, '/')))
          t = vpath;
        else
          ++t;
        strcpy(t, _path);
      }
    }
  
    char* transPath = servact_translate_uri(vpath, sn);
    if (transPath) {
      struct stat buff;
      if (stat(transPath, &buff) == 0) {
        if (_type == FLASTMOD) {
          OutputLastModifiedDate(pg, buff);
        }
        else if (_type == FSIZE) {
          OutputSize(pg, buff);
        }
      }
      else {
        res = PR_FALSE;
        log_error(LOG_FAILURE, "parse-html", sn, rq,
                  XP_GetAdminStr(DBT_fileInfoHandlerError1),
                  transPath, system_errmsg());      
      }
      FREE(transPath);

    }
    else {
      res = PR_FALSE;
    }

    if (_fType == ACTUAL)
      FREE(vpath);
  }

  if (res == PR_FALSE) {
    pg.WriteErrMsg();

  }

  return res;

}

void 
FileInfoHandler::OutputLastModifiedDate(PageStateHandler& pg, struct stat &buff)
{
  struct tm modTime;
  char out[256];
  
  util_localtime(&buff.st_mtime, &modTime);
  util_strftime(out, pg.GetLastModifiedTimeFormat(), &modTime);
  int l = strlen(out);
  pg.Write(out, l);
}


void 
FileInfoHandler::OutputSize(PageStateHandler& pg, struct stat &buff)
{
  size_t sz = buff.st_size;
  int l = 0;
  char in[256], out[256];
  if (pg.GetSizeFormat() == SIZE_KMG) {
    if(!sz)
      l = util_sprintf(out, "0K");
    else if(sz < 1024)
      l = util_sprintf(out, "1K");
    else if(sz < 1048576)
      l = util_sprintf(out, "%dK", sz / 1024);
    else
      l = util_sprintf(out, "%dM", sz / 1048576);
  }
  else {
    util_itoa(sz, in);
    out[0] = in[0];
    l = 1;
    for(char* t = in+1, *u = &in[strlen(in)]; *t; ++t, ++l) {
      if(!((u - t) % 3)) {
        out[l++] = ',';
      }
      out[l] = *t;
    }
    out[l] = '\0';
  }       

  l = strlen(out);
  pg.Write(out, l);
}
