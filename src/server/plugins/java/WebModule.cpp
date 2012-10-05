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

#include "WebModule.h"
#include "netsite.h"
#include "base/ereport.h"
#include "JVMControl.h"

// This has to be the same value as is in com.iplanet.ias.web.Constants.java
static const char* const DEFAULT_WEB_MODULE_NAME = "default-webapp";
static const char* const J_SECURITY_CHECK = "/j_security_check";
static const int J_SECURITY_CHECK_LEN = strlen(J_SECURITY_CHECK);
static ServletResource* warResource = new ServletResource("war_file", NULL, PR_TRUE);
static ServletResource* jSecurityResource = new ServletResource(J_SECURITY_CHECK, NULL, PR_TRUE);


WebModule::WebModule(const char* name, const char* contextPath, long jContextObj,
                     const char* location) :
    _isWarFile(PR_FALSE), _defaultServletResource(NULL), _numWelcomeFiles(0),
    _welcomeFileList(NULL), _longestWelcomeFileLen(0), _isAvailable(PR_TRUE),
    _defaultServletHasContent(PR_FALSE), _useRequestEncforHeaders(PR_FALSE)
{
    _name = PERM_STRDUP(name);
    _contextPath = PERM_STRDUP(contextPath);
    _contextPathLen = strlen(_contextPath);
    char* normalized = PERM_STRDUP(location);
    _location = normalized;
#ifdef XP_WIN32
    // Normalize backslashes to forward slashes so that the file cache
    // directories are created correctly on Windows
    while (*normalized) {
        if (*normalized == '\\')
            *normalized = '/';
        normalized++;
    }
#endif
    _locationLen = strlen(_location);

    _jContext = jContextObj;

    // Only the internal default context triggers special behaviour
    _isSystemDefault = ((strcmp(_name, DEFAULT_WEB_MODULE_NAME) == 0) &&
                        (*_contextPath == '\0'));

    // if the location is a file, assume that it is a war file
    PRFileInfo64 fInfo;
    if (PR_GetFileInfo64(_location, &fInfo) != PR_SUCCESS ||
        fInfo.type == PR_FILE_FILE)
        _isWarFile = PR_TRUE;

    _exactPathMap = new WResourceHash(20);
    _wildcardMap = new WResourceHash(20);
    _extensionMap = new WResourceHash(20);
#ifdef XP_WIN32
    // Use case insensitive lookups on Windows so that we dont go
    // into web core for a case-variant of the request.
    _exactPathMapInsensitive = new WResourceHash(20);
    _wildcardMapInsensitive = new WResourceHash(20);
    _extensionMapInsensitive = new WResourceHash(20);
    _exactPathMapInsensitive->setMixCase();
    _wildcardMapInsensitive->setMixCase();
    _extensionMapInsensitive->setMixCase();
#endif
    _mimeMap = new WStringHash(10);
    _errorMap = new WIntHash(10);
}

WebModule::~WebModule()
{
    PERM_FREE(_name);
    _name = NULL;

    PERM_FREE(_contextPath);
    _contextPath = NULL;

    PERM_FREE(_location);
    _location = NULL;

    JvmCall jvm_call;
    JNIEnv *env = jvm_call.getEnv();

    if (_jContext) {
        if (env != NULL)
            env->DeleteGlobalRef((jobject)_jContext); 
        _jContext = NULL;
    }

    if (_defaultServletResource) {
        if (env != NULL)
            env->DeleteGlobalRef((jobject)_defaultServletResource->getServletWrapper());
        delete _defaultServletResource;
        _defaultServletResource = NULL;
    }

    for (int i=0; i < _numWelcomeFiles; i++)
        PERM_FREE(_welcomeFileList[i]);
    PERM_FREE(_welcomeFileList);

    _welcomeFileList = NULL;
    _numWelcomeFiles = 0;
    _longestWelcomeFileLen = 0;

    if (_exactPathMap) {
        removeResourceHashElements(_exactPathMap, env);
        delete _exactPathMap;
        _exactPathMap = NULL;
    }

    if (_wildcardMap) {
        removeResourceHashElements(_wildcardMap, env);
        delete _wildcardMap;
        _wildcardMap = NULL;
    }

    if (_extensionMap) {
        removeResourceHashElements(_extensionMap, env);
        delete _extensionMap;
        _extensionMap = NULL;
    }

#ifdef XP_WIN32
    if (_exactPathMapInsensitive) {
        // The resource is already  deleted by _exactPathMap (above)
        delete _exactPathMapInsensitive;
        _exactPathMapInsensitive = NULL;
    }

    if (_wildcardMapInsensitive) {
        // The resource is already  deleted by _wildcardMap (above)
        delete _wildcardMapInsensitive;
        _wildcardMapInsensitive = NULL;
    }

    if (_extensionMapInsensitive) {
        // The resource is already  deleted by _extensionMap (above)
        delete _extensionMapInsensitive;
        _extensionMapInsensitive = NULL;
    }
#endif

    if (_mimeMap) {
        _mimeMap->removeAll();
        delete _mimeMap;
        _mimeMap = NULL;
    }

    if (_errorMap) {
        _errorMap->removeAll();
        delete _errorMap;
        _errorMap = NULL;
    }
}

const char* WebModule::getName()
{
    return _name;
}

const char* WebModule::getContextPath()
{
    return _contextPath;
}

int WebModule::getContextPathLen()
{
    return _contextPathLen;
}

const char* WebModule::getLocation()
{
    return _location;
}

int WebModule::getLocationLen()
{
    return _locationLen;
}

long WebModule::getJavaContextObject()
{
    return _jContext;

}

const char* WebModule::getErrorPage(int errorCode)
{
    return _errorMap->lookup(errorCode);
}

const char* WebModule::getMimeMapping(const char *extension)
{
    return _mimeMap->lookup(extension);
}

PRBool WebModule::addServletMapping(const char* servletName,
                                  const char* urlPattern,
                                  long jWrapperObj)
{
     return addUrlPattern(urlPattern, servletName, jWrapperObj, PR_TRUE);
}

void WebModule::addFilterMapping(const char* filterName, 
                                 const char* urlPattern)
{
     addUrlPattern(urlPattern, filterName, NULL, PR_FALSE);
}

void WebModule::addSecurityMapping(const char* resourceName,
                                   const char* urlPattern)
{
     addUrlPattern(urlPattern, resourceName, NULL, PR_FALSE);
}

// url patterns are stored in the hash tables using the following rules
// from the servlet 2.3 spec :
// SRV. 11.2 Specification of Mappings 
//   In the web application deployment descriptor, the following syntax is
//   used to define mappings: 
// 1.A string beginning with a '/' character and ending with a '/*' postfix
//   is used for path mapping. 
// 2.A string beginning with a '*.' prefix is used as an extension mapping.
// 3.A string containing only the '/' character indicates the "default"
//   servlet of the application.
// 4.All other strings are used for exact matches only. 
PRBool WebModule::addUrlPattern(const char* urlPattern, 
                                const char* resourceName, long jWrapperObj,
                                PRBool hasContent)
{
    PRBool status  = PR_TRUE;

    if (resourceName == NULL)
         resourceName = "noname";

     ereport(LOG_VERBOSE, (char*)"WebModule[%s]: adding pattern \"%s\" for resource \"%s\"",
             (_contextPathLen ? _contextPath : "/"), urlPattern, resourceName);

     // rule 3 : default servlet
     if (strcmp(urlPattern, "/") == 0) {
         _defaultServletResource = new ServletResource(resourceName,
                                                       jWrapperObj,
                                                       _defaultServletHasContent);
         return status;
     }

     ServletResource* sr = new ServletResource(resourceName, jWrapperObj,
                                               hasContent);

     // rule 2 : extension mapping
     if (urlPattern[0] == '*' && urlPattern[1] == '.' && 
         urlPattern[2] != '\0') {
         if ((status = _extensionMap->insert(&urlPattern[2], sr)) == PR_FALSE) 
             delete sr;

#ifdef XP_WIN32
         if (status == PR_TRUE)
             _extensionMapInsensitive->insert(&urlPattern[2], sr);
#endif
         return status;
     }

     // it must be a path map or exact match. Ensure that the pattern
     // begins with a '/'
     int len = strlen(urlPattern);
     char* pattern = (char*)MALLOC(len + 2);
     if (len && urlPattern[0] != '/') {
        sprintf(pattern, "/%s", urlPattern);
        len += 1;
     } else
        strcpy(pattern, urlPattern);
        
     // rule 1 : path mapping, begin with / and end with /*
     if (len >= 2 && pattern[len-2] == '/' && pattern[len-1] == '*') {

         // remove the trailing /* from the pattern
         pattern[len-2] = '\0';

         // If a wildcard pattern maps to the JspServlet, then we need to
         // do some special processing to handle welcome files etc as the
         // JspServlet doesn't know to do that. Also, JspServlet's dispatcher
         // expects servlet path to be set as if it were an extension match. 
         // Mark the resource accordingly so that even a wildcard match is
         // treated like an extension match when figuring out the servlet path.
         if (!strcmp(resourceName, "jsp"))
            sr->setSpecial(PR_TRUE);

         if ((status = _wildcardMap->insert(pattern, sr)) == PR_FALSE) 
             delete sr;
#ifdef XP_WIN32
         if (status == PR_TRUE)
             _wildcardMapInsensitive->insert(pattern, sr);
#endif

         FREE(pattern);
         return status;
     }

     // if path ends with /, remove it
     if (len > 2 && pattern[len-1] == '/')
         pattern[len-1] = '\0';

     // rule 4 : exact match for all other strings
     if ((status = _exactPathMap->insert(pattern, sr)) == PR_FALSE) 
         delete sr;
#ifdef XP_WIN32
     if (status == PR_TRUE)
         _exactPathMapInsensitive->insert(pattern, sr);
#endif


     FREE(pattern);
     return status;
}

void WebModule::addMimeMapping(const char* ext, const char* mimeType)
{
    char *mime = PERM_STRDUP(mimeType);
    if (_mimeMap->insert(ext, mime) == PR_FALSE)
        PERM_FREE(mime);
}

// XXX : assumes that location starts with a '/'
void WebModule::addErrorPage(int errCode, const char* location)
{
    char* fullPath = (char*)PERM_MALLOC(strlen(_contextPath) + strlen(location) + 1);
    strcpy(fullPath, _contextPath);
    strcat(fullPath, location);

    if (_errorMap->insert(errCode, fullPath) == PR_FALSE)
        PERM_FREE(fullPath);
}

void WebModule::setWelcomeFileList(int numFiles, char* fileList[])
{
    if (_welcomeFileList == NULL) {
        _numWelcomeFiles = numFiles;
        _welcomeFileList = fileList;
        for (int i = 0; i < _numWelcomeFiles; i++) {
            int len = strlen(_welcomeFileList[i]);
            if (len > _longestWelcomeFileLen)
                _longestWelcomeFileLen = len;
        }
    }
}

int WebModule::getNumWelcomeFiles() const
{
    return _numWelcomeFiles;
}

const char* WebModule::getWelcomeFile(int i) const
{
    if (i < _numWelcomeFiles)
        return _welcomeFileList[i];
    return NULL;
}

int WebModule::getLongestWelcomeFileLen() const
{
    return _longestWelcomeFileLen;
}

ServletResource* WebModule::matchUrlPattern(char* uri, char*& servletPath, 
                                            char*& pathInfo)
{
    ServletResource* sr = matchUrlPattern(uri, servletPath, pathInfo, 
                                          _exactPathMap, 
                                          _wildcardMap,
                                          _extensionMap);
#ifdef XP_WIN32
    // On windows, 
    // If the first case sensitive lookup did not match a servlet,
    // do a case insensitive lookup to get the wrapper that
    // should serve the request.
    if (sr == NULL) {
        char* sp = NULL;
        char* pi = NULL;
        ServletResource* newRes =  matchUrlPattern(uri, sp, pi, 
                                                   _exactPathMapInsensitive, 
                                                   _wildcardMapInsensitive,
                                                   _extensionMapInsensitive);
        if (newRes != NULL) {
            FREE(servletPath);
            FREE(pathInfo);
            servletPath = sp;
            pathInfo = pi;
            sr = newRes;
        }
    }
#endif
    return sr;
}

// This function checks if the given uri maps to a java resource within the
// context.
// Firstly, if the application is deployed as a war file, then every
// uri within the context is mapped to java.
// It then checks if it ends with j_security_check which is an implied
// servlet used for form login, if so it again maps it to java.
// If not, it uses the matching rules specified in the servlet 2.3 spec : 
//
// SRV. 11.1 Use of URL Paths 
//   The path used for mapping to a servlet is the request URL from the
//   request object minus the context path. The URL path mapping rules below
//   are used in order. The first successful match is used with no further
//   matches attempted: 
// 1.The container will try to find an exact match of the path of the
//   request to the path of the servlet. A successful match selects the
//   servlet. 
// 2.The container will recursively try to match the longest path-prefix:
//   This is done by stepping down the path tree a directory at a time,
//   using the '/' character as a path separator. The longest match
//   determines the servlet selected. 
// 3.If the last segment in the URL path contains an extension (e.g. .jsp),
//   the servlet container will try to match a servlet that handles requests
//   for the extension. An extension is defined as the part of the last
//   segment after the last '.' character. 
// 4.If neither of the previous three rules result in a servlet match, the
//   container will attempt to serve content appropriate for the resource
//   requested. If a "default" servlet is defined for the application, it
//   will be used. 
// The container must use case-sensitive string comparisons for matching. 
//
// The function will search until a match to a non null servletWrapper
// is found. If a non null servletWrapper is found then the servletPath 
// and pathInfo will be set. If a matching servlet is not found, that 
// is if only a ServletResource match is found to a url pattern (as in 
// filter mapping, security constaint -- the servletWrapper is NULL 
// in this case),  then the servletResource matching the uri will 
// be returned, but the servletPath and and pathInfo will be NULL
ServletResource* WebModule::matchUrlPattern(char* uri, char*& servletPath, 
                                            char*& pathInfo,
                                            WResourceHash* exactPathMap,
                                            WResourceHash* wildcardMap,
                                            WResourceHash* extensionMap)
{
    servletPath = NULL;
    pathInfo = NULL;
    ServletResource* servletResource = NULL;
    // Save any javaResource if a resource with a null servlet wrapper
    // object is found
    ServletResource* javaResource = NULL;

    int uriLen = strlen(uri);
    if (uriLen >= J_SECURITY_CHECK_LEN) {
        // get the last part of the uri
        char* suffix = uri + (uriLen - J_SECURITY_CHECK_LEN);

        // check if it matches /j_security_check, if so this is a form-login
        // authentication request, return /j_security_check as resource name
        if (strcmp(suffix, J_SECURITY_CHECK) == 0)
            return jSecurityResource;
    }

    // rule 1, check if there is an exact match 
    // If it matches to a non null servlet object, return rightaway
    if (exactPathMap->numEntries()) {
        servletResource = exactPathMap->lookup(uri);
        if (servletResource) {
            if (servletResource->hasContent()) {
                servletPath = STRDUP(uri);
                return servletResource;
            }
            javaResource = servletResource;
        }
    }

    // rule 2, step down the path tree a directory at a time, using the '/'
    // character as a path separator
    // If it matches to a non null servlet object, return rightaway
    if (wildcardMap->numEntries()) {
        servletResource = wildcardMap->lookup(uri);
        if (servletResource) {
            if (servletResource->hasContent()) {
                servletPath = STRDUP(uri);
                return servletResource;
            }
            javaResource = servletResource;
        }

        // Continuation of rule 2 traverse up and see if there is a match
        int index = strlen(uri);
        while (index) {
            index--;
            if (uri[index] == '/') {
                uri[index] = '\0';
                servletResource = wildcardMap->lookup(uri);
                // If the servletesource matches a servlet, then we are done
                // o/w do a exhaustive search until we find one 
                if (servletResource) {
                    if (servletResource->hasContent()) {
                        // Assign the servletPath and pathInfo
                        if (servletResource->isSpecial()) {
                            // This wildcard pattern is served by JspServlet,
                            // so set the servlet path as if it were an 
                            // extension match.
                            // Restore the "/"
                            uri[index] = '/';
                            servletPath = STRDUP(uri);
                        } else {
                            servletPath = STRDUP(uri);
                            // Restore the "/"
                            uri[index] = '/';
                            pathInfo = STRDUP(&uri[index]);
                        }
                        // It mapped to a servlet, then return o/w keep looking
                        return servletResource;
                    }
                    javaResource = servletResource;
                }
                // Restore the "/" unconditionally
                uri[index] = '/';
            }
        }
    }

    // rule 3, check if the extension maps to a servlet
    char *extension = strrchr(uri, '.');
    if (extension) {
        servletResource = extensionMap->lookup(extension + 1);

        if (servletResource) {
            if (servletResource->hasContent()) {
                servletPath = STRDUP(uri);
                return servletResource;
            }
            javaResource = servletResource;            
        }
    }

    // rule 4, return "default" servlet if the user configured a custom
    // default servlet or if the requested resource matched filters or
    // security constraints but not any path/extension mappings.
    if (javaResource || 
        (_defaultServletResource && _defaultServletResource->hasContent())) {
        servletPath = STRDUP(uri);
        return _defaultServletResource;
    }

    // If its a war file, then we should be handling it in java land
    if (_isWarFile == PR_TRUE)
        return warResource;

    // If nothing matches, return javaResource
    return javaResource;
}

// Remove the ServletResources 
void WebModule::removeResourceHashElements(WResourceHash* hashMap, JNIEnv* env) {
    SimpleHashUnlockedIterator iter(hashMap);
    while (iter.next()) {
        ServletResource* sr = (ServletResource*) (*iter.getValPtr());
        if (env) {
            jobject jobj = (jobject)sr->getServletWrapper();
            if (jobj)
                env->DeleteGlobalRef(jobj);
        }
        delete sr;
    }
}


//
// Returns whether this is a default context that was assigned/created by
// the server (because the user did not configure default web module for
// the virtual server)
//
// The default web application created by the server is ALWAYS named
// 'default-webapp'
//
PRBool WebModule::isSystemDefault()
{
    return _isSystemDefault;
}

PRBool WebModule::isWarFile()
{
    return _isWarFile;
}

//
// Set the value of the flag to indicate that there were errors when
// starting the web module and so it is unavailable
//
void WebModule::setUnavailable(void)
{
    _isAvailable = PR_FALSE;
}


//
// Get the value of the flag that indicates whether the web module was
// started successfully.
//
PRBool WebModule::isAvailable(void) const
{
    return _isAvailable;
}

//
// Set the value that indicates whether the default servlet 
// is the one that dispatches to NSAPI (i.e. has no content) or not.
//
void WebModule::setDefaultServletHasContent(PRBool hasContent)
{
    _defaultServletHasContent = hasContent;
}

//
// Returns DefaultServlet resource
//
ServletResource* WebModule::getDefaultServletResource(void)
{
    return _defaultServletResource;
}


void WebModule::setUseRequestEncforHeaders(PRBool useRequestEncforHeaders)
{
    _useRequestEncforHeaders = useRequestEncforHeaders;
}

PRBool WebModule::useRequestEncforHeaders(void)
{
    return _useRequestEncforHeaders;
}

