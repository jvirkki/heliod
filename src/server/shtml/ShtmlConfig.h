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

#ifndef _SHTML_CONFIG_H_
#define _SHTML_CONFIG_H_

#include <base/pblock.h>

class ShtmlConfig {
  public:
    enum ShtmlFileType { PARSE_SHTML_ONLY, PARSE_HTML_FILES_ALSO, 
                         PARSE_FILES_WITH_EXEC_BIT};
    static void Init(pblock* pb)
    {
      if (!_gShtmlConfig)
        _gShtmlConfig = new ShtmlConfig(pb);
    }
    static ShtmlConfig* GetShtmlConfig()
    {
      return _gShtmlConfig;
    }
    ShtmlConfig(pblock* pb);
    ~ShtmlConfig();
    PRBool IsShtmlEnabled() { return _shtmlState; }
    void ActivateShtml(PRBool on) { _shtmlState = on;}
    ShtmlFileType GetShtmlFileExtension() {return _shtmlExt;}
    void SetShtmlFileExtension(ShtmlFileType shtmlExt) {_shtmlExt = shtmlExt;};
    PRBool AllowExecTag() {return _allowExecTag;}
    void AllowExecTag(PRBool allowExecTag) 
    {_allowExecTag = allowExecTag;};

  private:
    PRBool _shtmlState;
    PRBool _allowExecTag;
    ShtmlFileType _shtmlExt;
    static ShtmlConfig* _gShtmlConfig;
};

#endif /* _SHTML_CONFIG_H_ */
