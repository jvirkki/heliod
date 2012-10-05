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

#include "ShtmlConfig.h"
#include "plstr.h"

static const char* SHTML_ENABLE_TAG = "enable";
static const char* SHTML_ALLOW_EXEC = "opts";
static const char* SHTML_PARSE_HTML_FILES = "parseHtmlFiles";

ShtmlConfig* ShtmlConfig::_gShtmlConfig = 0;

ShtmlConfig::ShtmlConfig(pblock* pb)
:_shtmlState(PR_FALSE), _allowExecTag(PR_TRUE), _shtmlExt(PARSE_SHTML_ONLY)

{
  if (!pb) {
    return;
  }

  const char* enable = pblock_findval(SHTML_ENABLE_TAG, pb);
  const char* allowExec = pblock_findval(SHTML_ALLOW_EXEC, pb);
  const char* fileExt = pblock_findval(SHTML_PARSE_HTML_FILES, pb);

  if (enable) {
    PRBool flag = (PL_strcasecmp(enable, "yes") == 0 ?
                            PR_TRUE : PR_FALSE);
    ActivateShtml(flag);
  }
  if (allowExec) {
    PRBool flag = (PL_strcasecmp(allowExec, "noexec") == 0 ?
                            PR_FALSE : PR_TRUE);
    AllowExecTag(flag);
  }
  if (fileExt) {
    if (PL_strcasecmp(fileExt, "yes") == 0) {
      SetShtmlFileExtension(PARSE_HTML_FILES_ALSO); 
    }
    else
      SetShtmlFileExtension(PARSE_SHTML_ONLY);
  }
}

ShtmlConfig::~ShtmlConfig()
{

}

