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

#ifndef __SERVERCONTROL_H__
#define __SERVERCONTROL_H__

#include <stdio.h>
#include <string>
#include <sstream>
#include <stdarg.h>

#include <limits.h>
#ifdef XP_WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

#define NBUF_SIZE 1024
#define MAX_BUF_SIZE 4096
#define BIG_LINE    1024
#define NUM_ENTRIES 64

#define FAILURE "failure"
#define STARTUP_STATUS_LINE "startup: "
#define RECONFIG_SUCCESS "CORE3280: "
#define RECONFIG_FAILED "CORE3279: "
#define CHANGES_IGNORED "CORE3312: "
#define CHANGES_INCOMPATIBLE "CORE3313: "

using std::stringstream;
using std::string;

class RestartRequired : public std::exception {
    private:
        string _what;

    public:
        RestartRequired(const string& what_arg) : _what(what_arg) {}
        virtual const char* what() const throw() { return _what.data(); }
        ~RestartRequired () throw() {};
};

class ServerControl {
    protected:
        const char *installRoot;
        const char *instanceRoot;
        const char *instanceName;
        const char *serverTempDir;
        char *adminTempDir;
        char *cfgDir;
        bool secEnabled;
        stringstream strout;

        void bufferOutput(const char *fmt, ...);
       
    private:
        bool noBuffering;
        // XXX:
        // This is used as a temporary variable in getOutput()
        // so that we can extend the scope of strout.str()
        // Doing this will not mandate any clients to depend
        // on std::string
        string tmpStr;

    public:
        ServerControl(const char *installRoot,
                      const char *instanceRoot, const char *instanceName,
                      const char *tempDir);
        virtual ~ServerControl();

        inline void setBuffering(bool flag) { noBuffering = !flag; }

        virtual bool isRunning()=0;
        virtual bool reconfigServer()=0;

        inline const char *getOutput() {
            tmpStr = strout.str();
            return tmpStr.c_str();
        }
};

#endif //__SERVERCONTROL_H__

