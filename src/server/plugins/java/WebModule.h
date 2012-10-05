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

#ifndef _WebModule_h_
#define _WebModule_h_

#include "HashUtil.h"
#include "ServletResource.h"
#include <jni.h>

class WebModule
{
    public:

        WebModule(const char* name, const char* contextPath, 
                  long jContextObj, const char* location);

        ~WebModule(void);

        const char* getName();
        const char* getContextPath();
        int getContextPathLen();
        const char* getLocation();
        int getLocationLen();
        long getJavaContextObject();
        const char* getErrorPage(int errorCode);
        const char* getMimeMapping(const char *extension);

        PRBool addServletMapping(const char* servletName, 
                                 const char* urlPattern, long jWrapperObj);
        PRBool isSystemDefault();
        PRBool isWarFile();

        void addFilterMapping(const char* filterName, const char* urlPattern);
        void addSecurityMapping(const char* resourceName, 
                                const char* urlPattern);

        void addMimeMapping(const char* ext, const char* mimeType);
        void addErrorPage(int errCode, const char* location);
        void setWelcomeFileList(int numFiles, char* fileList[]);
        const char* getWelcomeFile(int i) const;
        int getNumWelcomeFiles() const;
        int getLongestWelcomeFileLen() const;


        /* Indicate that the context is unavailable. The context is
         * marked unavailable when there are errors starting it up
         */
        void setUnavailable(void);

        /* Return whether the context is available or not.
         */
        PRBool isAvailable(void) const;

        /* Set the value that indicates whether the default servlet 
         * is the one that dispatches to NSAPI (i.e. has no content) or not.
         */
        void setDefaultServletHasContent(PRBool hasContent);

        /* Returns the ServletResource for this uri along with the servlet
         * path and pathinfo for this uri if the servletWrapper is non-null
         */
        ServletResource* matchUrlPattern(char* uri, char*& servletPath, 
                                         char*& pathInfo);
#ifdef XP_WIN32
        ServletResource* matchUrlPatternForWindows(char* uri, 
                                                   char*& servletPath, 
                                                   char*& pathInfo);
#endif
        /* Returns DefaultServlet resource
         */
        ServletResource* getDefaultServletResource(void);

        void setUseRequestEncforHeaders(PRBool useRequestEncforHeaders);

        PRBool useRequestEncforHeaders(void);

    private:
        PRBool addUrlPattern(const char* urlPattern, const char* resourceName,
                             long jWrapperObj, PRBool hasContent);
        void removeResourceHashElements(WResourceHash* hash, JNIEnv* env);
        ServletResource* matchUrlPattern(char* uri, char*& servletPath, 
                                         char*& pathInfo,
                                         WResourceHash* exactPathMap,
                                         WResourceHash* wildcardMap,
                                         WResourceHash* extensionMap);
        char*               _name;
        char*               _contextPath;
        int                 _contextPathLen;
        char*               _location;
        int                 _locationLen;
        long                _jContext; // Java Context Object
        ServletResource*    _defaultServletResource;
        PRBool              _isWarFile;
        PRBool              _isSystemDefault;
        PRBool              _isAvailable;
        PRBool              _defaultServletHasContent;
        PRBool              _useRequestEncforHeaders;
        int                 _numWelcomeFiles;
        int                 _longestWelcomeFileLen;
        char**              _welcomeFileList;
        WResourceHash*      _exactPathMap;
        WResourceHash*      _wildcardMap;
        WResourceHash*      _extensionMap;
#ifdef XP_WIN32
        WResourceHash*      _exactPathMapInsensitive;
        WResourceHash*      _wildcardMapInsensitive;
        WResourceHash*      _extensionMapInsensitive;
#endif
        WStringHash*        _mimeMap;
        WIntHash*           _errorMap;
};

#endif /* _WebModule_h_ */
