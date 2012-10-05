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

#include "base/cinfo.h"
#include "base/ereport.h"

#include "NSJavaUtil.h"
#include "NSAPIVirtualServer.h"
#include "com_sun_webserver_connector_nsapi_NSAPIVirtualServer.h"

NSAPIVirtualServer::NSAPIVirtualServer(VirtualServer* vs) :
    _vs(vs), _uriMapper(NULL), _jVirtualServer(0L)
{
}

NSAPIVirtualServer::~NSAPIVirtualServer(void)
{
    _vs = NULL;
    _jVirtualServer = NULL;

    for (int i = 0; i < _webModules.length(); i++)
        delete (WebModule*) _webModules[i];

    if (_uriMapper != NULL) {
        delete _uriMapper;
        _uriMapper = NULL;
    }
}

char* NSAPIVirtualServer::getDocumentRoot()
{
    if (_vs)
        return vs_get_doc_root(_vs);
    return NULL;
}

char* NSAPIVirtualServer::translateURI(const char* uri) const
{
    if (_vs)
        return vs_translate_uri(_vs, uri);
    return NULL;
}

// Return the underlying vs. Needed by native realm support.
VirtualServer* NSAPIVirtualServer::getVirtualServer()
{
    return _vs;
}

//
// Save the corresponding Java VirtualServer object 
//
void NSAPIVirtualServer::setJavaObject(long jVirtualServer)
{
    _jVirtualServer = jVirtualServer;
}

//
// Return the corresponding Java VirtualServer object 
//
long NSAPIVirtualServer::getJavaObject(void)
{
    return _jVirtualServer;
}

// XXX : addWebModule assumes that contextPath starts with a '/' and
// does not have a trailing '/'. It also assumes that location exists.
// jcontext is a reference to the java context object
WebModule* NSAPIVirtualServer::addWebModule(const char* contextName,
                                           const char* contextPath,
                                           long jcontext,
                                           const char* location)
{
    WebModule* wm = new WebModule(contextName, contextPath, jcontext, 
                                  location);
    if (wm) {
        _webModules.append((void*) wm);
        _uriMap.addUriSpace(contextPath, wm);
        ereport(LOG_VERBOSE,
                (char*)"Adding web module : context = %s, location = %s",
                contextPath, location);
    }
    return wm;
}

void NSAPIVirtualServer::createUriMapper(void)
{
    _uriMapper = new UriMapper<WebModule*>(_uriMap);
}


// matchWebModule checks if a request uri matches a web application
// context path using the following rule from the servlet 2.3 spec.
// SRV. 11.1 Use of URL Paths 
//   Upon receipt of a client request, the web container determines the web
//   application to which to forward it. The web application selected must
//   have the the longest context path that matches the start of the request
//   URL. The matched part of the URL is the context path when mapping to
//   servlets. 

WebModule* NSAPIVirtualServer::matchWebModule(char* uri)
{
    WebModule* wm = NULL;
    if (_uriMapper)
        wm = _uriMapper->map(uri);
    return wm;
}

JNIEXPORT jstring JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIVirtualServer_jniFindMimeMapping(JNIEnv* env, jobject obj, jlong jniVS, jstring jextension)
{
    NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer*)jniVS;

    const char* extension = NULL;
    if (jextension != NULL)
        extension = env->GetStringUTFChars(jextension , NULL);    

    const char* type = vs_find_ext_type(nsapiVS->getVirtualServer(), extension);

    if (jextension != NULL)
        env->ReleaseStringUTFChars(jextension, extension);

    if (type == NULL)
        return NULL;

    return env->NewStringUTF(type);
}

JNIEXPORT void  JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIVirtualServer_jniSetContextMapping(JNIEnv* env,
    jobject obj, jlong jniVS, jobject jcontext, jstring jctxname,
    jstring jctxpath,
    jstring jctxlocation, jint servlet_size, jintArray servlet_length_array,
    jbyteArray servlet_mapping, jobjectArray jwrappers,
    jint mime_size, jintArray mime_length_array,
    jbyteArray mimes, jint welcome_size,
    jintArray welcome_length_array, jbyteArray welcome_files,
    jint error_size, jintArray error_codes,
    jintArray error_length_array, jbyteArray error_pages, jint sec_size,
    jintArray url_pattern_length,jintArray sec_len_buffer,jbyteArray sec_byte_buffer,
    jint count, jintArray filter_len_buffer,jbyteArray filter_byte_buffer,
    jboolean form_login, jboolean default_servlet_has_content, 
    jboolean useRequestEncforHeaders)
{
     int i, j;

     NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer*)jniVS;

     char key[1024];
     char mapping[1024];
     const char* ctx_name = NULL;
     const char* ctx_path  = NULL;
     const char* ctx_location = NULL;

     
     if (jctxname != NULL) {
        ctx_name = env->GetStringUTFChars(jctxname , NULL);    
     }
     if (jctxpath != NULL) {
        ctx_path = env->GetStringUTFChars(jctxpath , NULL);    
     }
     if (jctxlocation != NULL) {
        ctx_location = env->GetStringUTFChars(jctxlocation , NULL);    
     }

     // Add a webmodule for this ctx
     if (jcontext)
         jcontext = env->NewGlobalRef(jcontext);

     WebModule* web_module = nsapiVS->addWebModule(ctx_name, ctx_path, 
                                                  (long)jcontext,
                                                  ctx_location);
     if (web_module == NULL) {
         if (jctxname != NULL) {
             env->ReleaseStringUTFChars(jctxname , ctx_name);
         }      

         if (jctxpath != NULL) {
             env->ReleaseStringUTFChars(jctxpath , ctx_path);
         }      

         if (jctxlocation != NULL) {
             env->ReleaseStringUTFChars(jctxlocation , ctx_location);
         }
         if (jcontext)
             env->DeleteGlobalRef(jcontext);
         return;
     }

     web_module->setDefaultServletHasContent(default_servlet_has_content);

     // Handle servlet mapping first before filter mapping and sec constraints
     // since we use the same hash table for all mappings and we don't want servlet
     // mapping to be overwritten for a equivalent uri
     jint* string_length = (jint*)env->GetIntArrayElements(servlet_length_array, NULL);
     jbyte* cbuf = (jbyte*)env->GetByteArrayElements(servlet_mapping, NULL);
     jbyte* pbuf = cbuf;

     int obj_index = 0;

     // Add servlet mapping first 
     for (i = 0; i < servlet_size; i+=2, obj_index++) {
         memcpy(key, (const char*)pbuf, string_length[i]);
         key[string_length[i]] = '\0';
         pbuf+= string_length[i];
         memcpy(mapping, (const char*)pbuf, string_length[i+1]);
         mapping[string_length[i+1]] = '\0';
         pbuf += string_length[i+1];
         jobject wrapper = env->GetObjectArrayElement(jwrappers, obj_index);
         if (wrapper)
             wrapper= env->NewGlobalRef(wrapper);
         if (web_module->addServletMapping(key, mapping, (long)wrapper) == PR_FALSE) {
             if (wrapper) {
                 env->DeleteGlobalRef(wrapper);
             }
         }
     }
     env->ReleaseByteArrayElements(servlet_mapping, cbuf, 0);
     env->ReleaseIntArrayElements(servlet_length_array, string_length, 0);


     // Handle mime mapping
     string_length = (jint*)env->GetIntArrayElements(mime_length_array, NULL);
     cbuf = (jbyte*)env->GetByteArrayElements(mimes, NULL);
     pbuf = cbuf;

     for (i = 0; i < mime_size; i+=2) {
         memcpy(key, (const char*)pbuf, string_length[i]);
         key[string_length[i]] = '\0';
         pbuf+= string_length[i];
         memcpy(mapping, (const char*)pbuf, string_length[i+1]);
         mapping[string_length[i+1]] = '\0';
         pbuf += string_length[i+1];
         web_module->addMimeMapping(key, mapping);
     }   
     env->ReleaseByteArrayElements(mimes, cbuf, 0);
     env->ReleaseIntArrayElements(mime_length_array, string_length, 0);

   
     // Handle welcome files
     if (welcome_size > 0) {
         string_length = (jint*)env->GetIntArrayElements(welcome_length_array, NULL);
         cbuf = (jbyte*)env->GetByteArrayElements(welcome_files, NULL);
         pbuf = cbuf;

         char ** welcomeFileList = (char**)PERM_MALLOC(welcome_size * sizeof(char**));
         for (i = 0; i < welcome_size; i++) {
             welcomeFileList[i] = (char*)PERM_MALLOC(string_length[i]+1);
             memcpy(welcomeFileList[i], (const char*)pbuf, string_length[i]);
             welcomeFileList[i][string_length[i]] = '\0';
             pbuf+= string_length[i];
         }
         web_module->setWelcomeFileList(welcome_size, welcomeFileList);
         env->ReleaseByteArrayElements(welcome_files, cbuf, 0);
         env->ReleaseIntArrayElements(welcome_length_array, string_length, 0);
     }
   
     // Handle error files
     string_length = (jint*)env->GetIntArrayElements(error_length_array, NULL);
     jint* errCodes = (jint*)env->GetIntArrayElements(error_codes, NULL);
     cbuf = (jbyte*)env->GetByteArrayElements(error_pages, NULL);
     pbuf = cbuf;

     for (i = 0; i < error_size; i++) {
         memcpy(key, (const char*)pbuf, string_length[i]);
         key[string_length[i]] = '\0';
         pbuf+= string_length[i];
         web_module->addErrorPage(errCodes[i],key);
     }   
     env->ReleaseByteArrayElements(error_pages, cbuf, 0);
     env->ReleaseIntArrayElements(error_length_array, string_length, 0);
     env->ReleaseIntArrayElements(error_codes, errCodes, 0);


     // Handle sec constraints
     string_length = (jint*)env->GetIntArrayElements(sec_len_buffer, NULL);
     cbuf = (jbyte*)env->GetByteArrayElements(sec_byte_buffer, NULL);
     jint* url_length = (jint*)env->GetIntArrayElements(url_pattern_length, NULL);
     pbuf = cbuf;

     int index = 0;
     for (i = 0; i < sec_size; i++) {
         memcpy(key, (const char*)pbuf, string_length[index]);
         key[string_length[index]] = '\0';
         pbuf+= string_length[index++];
         for (j = 0; j < url_length[i]; j++) {
             memcpy(mapping, (const char*)pbuf, string_length[index]);
             mapping[string_length[index]] = '\0';
             pbuf+= string_length[index++];
             web_module->addSecurityMapping(key,mapping);
         }
     }

     env->ReleaseIntArrayElements(url_pattern_length,url_length, 0);
     env->ReleaseByteArrayElements(sec_byte_buffer, cbuf, 0);
     env->ReleaseIntArrayElements(sec_len_buffer, string_length, 0);

     // Handle Filter Mappings
     string_length = (jint*)env->GetIntArrayElements(filter_len_buffer, NULL);
     cbuf = (jbyte*)env->GetByteArrayElements(filter_byte_buffer, NULL);
     pbuf = cbuf;

     for (i = 0; i < count; i+=2) {
         memcpy(key, (const char*)pbuf, string_length[i]);
         key[string_length[i]] = '\0';
         pbuf+= string_length[i];
         memcpy(mapping, (const char*)pbuf, string_length[i+1]);
         mapping[string_length[i+1]] = '\0';
         pbuf += string_length[i+1];
         web_module->addFilterMapping(key,mapping);
     }
     
     
     env->ReleaseByteArrayElements(filter_byte_buffer, cbuf, 0);
     env->ReleaseIntArrayElements(filter_len_buffer, string_length, 0);

     //     ereport(LOG_FAILURE, "form Login present for %s = %d",ctx_path, form_login);
     
     web_module->setUseRequestEncforHeaders(useRequestEncforHeaders);
 
     if (jctxname != NULL) {
        env->ReleaseStringUTFChars(jctxname , ctx_name);
     }

     if (jctxpath != NULL) {
        env->ReleaseStringUTFChars(jctxpath , ctx_path);
     }

     if (jctxlocation != NULL) {
        env->ReleaseStringUTFChars(jctxlocation , ctx_location);
     }
     
}

JNIEXPORT void  JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIVirtualServer_jniSetContextUnavailable(JNIEnv* env,
    jobject obj, jlong jniVS, jobject jcontext, jstring jctxname,
    jstring jctxpath,
    jstring jctxlocation)
{
     const char* ctx_name = NULL;
     const char* ctx_path  = NULL;
     const char* ctx_location = NULL;

     if (jctxname != NULL) {
        ctx_name = env->GetStringUTFChars(jctxname , NULL);    
     }
     if (jctxpath != NULL) {
        ctx_path = env->GetStringUTFChars(jctxpath , NULL);    
     }
     if (jctxlocation != NULL) {
        ctx_location = env->GetStringUTFChars(jctxlocation , NULL);    
     }

     // Add a webmodule for this ctx
     if (jcontext)
         jcontext = env->NewGlobalRef(jcontext);

     NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer*)jniVS;
     WebModule* web_module = nsapiVS->addWebModule(ctx_name, ctx_path, 
                                                  (long)jcontext,
                                                  ctx_location);
     if (web_module == NULL) {
         if (jctxname != NULL) {
             env->ReleaseStringUTFChars(jctxname , ctx_name);
         }      

         if (jctxpath != NULL) {
             env->ReleaseStringUTFChars(jctxpath , ctx_path);
         }      

         if (jctxlocation != NULL) {
             env->ReleaseStringUTFChars(jctxlocation , ctx_location);
         }
         if (jcontext)
             env->DeleteGlobalRef(jcontext);
         return;
     }

     web_module->setUnavailable();

     if (jctxname != NULL) {
        env->ReleaseStringUTFChars(jctxname , ctx_name);
     }

     if (jctxpath != NULL) {
        env->ReleaseStringUTFChars(jctxpath , ctx_path);
     }

     if (jctxlocation != NULL) {
        env->ReleaseStringUTFChars(jctxlocation , ctx_location);
     }
}

JNIEXPORT jstring JNICALL 
Java_com_sun_webserver_connector_nsapi_NSAPIVirtualServer_jniGetRealPath
(JNIEnv *env, jobject obj, jlong jniVS, jstring juri)
{
    NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer*)jniVS;

    jstring jRealPath = NULL;

    const char* uri = NULL;
    if (juri != NULL)
        uri = env->GetStringUTFChars(juri, NULL);

    char* realPath = nsapiVS->translateURI(uri);
    jRealPath = NSJavaUtil::createString(env, realPath);
    FREE(realPath);

    if (juri != NULL)
        env->ReleaseStringUTFChars(juri, uri);

    return jRealPath;
}

JNIEXPORT void  JNICALL
Java_com_sun_webserver_connector_nsapi_NSAPIVirtualServer_jniCreateContextUriMapper(JNIEnv* env,
    jobject obj, jlong jniVS)
{
    NSAPIVirtualServer* nsapiVS = (NSAPIVirtualServer*)jniVS;
    nsapiVS->createUriMapper();
}

