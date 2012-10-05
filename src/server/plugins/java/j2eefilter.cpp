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

/**
 * J2EE-NSAPI filter
 *
 * Implements an NSAPI filter that calls OutputStream.write in response to
 * net_write, etc. calls. Additionally, the filter can optionally call
 * InputStream.read in response to net_read calls and migrate the response
 * status and headers from a subrequest to a HttpServletResponse.
 */


#include "netsite.h"
#include "frame/filter.h"
#include "frame/httpfilter.h"
#include "NsprWrap/NsprError.h"
#include "JVMControl.h"
#include "NSJavaUtil.h"
#include "j2eefilter.h"


// Default (minimum) size of Java byte arrays
#define J2EEFILTER_DEFAULT_ARRAY_LENGTH 1024


/**
 * Stores context associated with individual J2EE-NSAPI filter instances.
 */
struct J2EEFilterContext {
    pool_handle_t* pool;  // NSAPI memory pool

    Session* parent_sn;   // Session of the parent Request
    Request* parent_rq;   // Parent Request

    JNIEnv* env;          // JNI environment

    jobject response;     // Caller-managed reference to Java HttpServletResponse
    jobject inputstream;  // Caller-managed reference to Java InputStream
    jobject outputstream; // Caller-managed reference to Java OutputStream
    PRBool forward;       // Set if response status and headers should be migrated to the HttpServletResponse
    PRBool dirty;         // Set if the HttpServletResponse buffer should be flushed
    int bytearray_length; // length of bytearray
    jbyteArray bytearray; // Global reference to Java byte array

    jthrowable throwable; // an exception previously thrown from Java
};


// The J2EE-NSAPI filter
static const Filter* _j2eefilter = NULL;

// Java method ID for HttpServletResponse.setStatus(int)
static jmethodID _response_setStatus = NULL;

// Java method ID for HttpServletResponse.setHeader(String, String)
static jmethodID _response_setHeader = NULL;

// Java method ID for HttpServletResponse.addHeader(String, String)
static jmethodID _response_addHeader = NULL;

// Java method ID for HttpServletResponse.flushBuffer()
static jmethodID _response_flushBuffer = NULL;

// Java method ID for InputStream.read(byte[], int, int)
static jmethodID _inputstream_read = NULL;

// Java method ID for OutputStream.write(byte[], int, int)
static jmethodID _outputstream_write = NULL;

// Java method ID for OutputStream.flush()
static jmethodID _outputstream_flush = NULL;


/**
 * Handle a Java exception if one occurred. First, the exception is retained
 * for later retrieval by storing the exception as context->throwable and
 * acquiring a global reference to it. Second, the JNI exception state is
 * cleared so that further JNI calls may be made.
 */
static inline void j2eefilter_store_throwable(J2EEFilterContext* context)
{
    jthrowable throwable = context->env->ExceptionOccurred();
    if (throwable) {
        context->env->ExceptionClear();
        if (!context->throwable)
            context->throwable = (jthrowable) context->env->NewGlobalRef(throwable);
    }
}


/**
 * Retrieve a previously stored Java exception. Returns NULL if no exception
 * has occurred.
 */
jthrowable j2eefilter_retrieve_throwable(J2EEFilterContext* context)
{
    return context->throwable;
}


/**
 * Release our global reference to a previously-thrown Java exception.
 */
static inline void j2eefilter_destroy_throwable(J2EEFilterContext* context)
{
    if (context->throwable) {
        context->env->DeleteGlobalRef(context->throwable);
        context->throwable = NULL;
    }
}


/**
 * Create a new Java byte array that is at least size bytes long and associate
 * it with the specified J2EEFilterContext.
 */
static inline void j2eefilter_create_bytearray(J2EEFilterContext* context, int size)
{
    PR_ASSERT(context->bytearray_length == 0);
    PR_ASSERT(!context->bytearray);

    if (!context->throwable) {
        // Figure out how big to make the new Java byte array
        if (size < J2EEFILTER_DEFAULT_ARRAY_LENGTH)
            size = J2EEFILTER_DEFAULT_ARRAY_LENGTH;
        context->bytearray_length = size;

        // Create a new Java byte array
        context->bytearray = context->env->NewByteArray(context->bytearray_length);

        // Remember any NewByteArray exception
        j2eefilter_store_throwable(context);

        // Maintain a global reference to the Java byte array, allowing the
        // thread to return control to the JVM between net_read/net_write calls
        // (e.g. when there's a J2EE-NSAPI filter sitting between two Servlets)
        if (context->bytearray) {
            context->bytearray = (jbyteArray) context->env->NewGlobalRef(context->bytearray);
            j2eefilter_store_throwable(context);
         }
    }
}


/**
 * Discard any Java byte array associated with the specified J2EEFilterContext.
 */
static inline void j2eefilter_destroy_bytearray(J2EEFilterContext* context)
{
    context->bytearray_length = 0;
    if (context->bytearray) {
        context->env->DeleteGlobalRef(context->bytearray);
        context->bytearray = NULL;
    }
}


/**
 * Create a new J2EEFilterContext. The specified JNIEnv* must remain valid
 * (i.e. the thread must not return control to Java) until after the returned
 * J2EEFilterContext destroyed.
 */
J2EEFilterContext* j2eefilter_create_context(Session* parent_sn, Request* parent_rq, JNIEnv* env, jobject response, jobject inputstream, jobject outputstream, PRBool forward)
{
    J2EEFilterContext* context = (J2EEFilterContext*) pool_malloc(parent_sn->pool, sizeof(J2EEFilterContext));
    if (context) {
        context->pool = parent_sn->pool;
        context->parent_sn = parent_sn;
        context->parent_rq = parent_rq;
        context->env = env;
        context->response = response;
        context->inputstream = inputstream;
        context->outputstream = outputstream;
        context->forward = forward;
        context->dirty = (response && !outputstream);
        context->bytearray_length = 0;
        context->bytearray = NULL;
        context->throwable = NULL;
    }

    return context;
}


/**
 * Discard a J2EEFilterContext.
 */
void j2eefilter_destroy_context(J2EEFilterContext* context)
{
    j2eefilter_destroy_bytearray(context);

    j2eefilter_destroy_throwable(context);

    pool_free(context->pool, context);
}


/**
 * Migrate response status and headers from the subrequest to the
 * HttpServletResponse.
 */
static inline void j2eefilter_migrate_response(Request* sub_rq, J2EEFilterContext* context)
{
    if (context->forward) {
        if (sub_rq != context->parent_rq && !context->parent_rq->senthdrs) {
            // Pass status_num to HttpServletResponse.setStatus
            // XXX In NSAPI, SAFs always set the status code, even on success.
            // In Java land, however, Servlets only explicitly set the status
            // code for non-200 responses. To preserve the original error code
            // on <error-page> RequestDispatcher.forward calls, never call
            // HttpServletResponse.setStatus(200).
            if (sub_rq->status_num != 0 && sub_rq->status_num != 200) {
                context->env->CallVoidMethod(context->response, _response_setStatus, sub_rq->status_num);
                j2eefilter_store_throwable(context);
            }

            // Pass the contents of srvhdrs to the HttpServletResponse
            pblock *seen = pblock_create(PARAMETER_HASH_SIZE);
            for (int i = 0; i < sub_rq->srvhdrs->hsize; i++) {
                for (pb_entry *p = sub_rq->srvhdrs->ht[i]; p; p = p->next) {
                    // The NSAPI status code is stored in rq->srvhdrs but
                    // shouldn't be passed to the HttpServletResponse
                    if (!strcmp(p->param->name, "status"))
                        continue;

                    jstring jname = context->env->NewStringUTF(p->param->name);
                    j2eefilter_store_throwable(context);
                    jstring jvalue = context->env->NewStringUTF(p->param->value);
                    j2eefilter_store_throwable(context);

                    // Call setHeader for the first occurrence of a given name
                    // and addHeader for each subsequent occurrence
                    if (pblock_findval(p->param->name, seen)) {
                        context->env->CallVoidMethod(context->response, _response_addHeader, jname, jvalue);
                    } else {
                        pblock_nvinsert(p->param->name, "1", seen);
                        context->env->CallVoidMethod(context->response, _response_setHeader, jname, jvalue);
                    }
                    j2eefilter_store_throwable(context);

                    if (jname != NULL)
                        context->env->DeleteLocalRef(jname);
                    if (jvalue != NULL)
                        context->env->DeleteLocalRef(jvalue);
                }
            }
            pblock_free(seen);
        }

        context->forward = PR_FALSE;
    }
}


/**
 * NSAPI insert filter method.
 */
extern "C" int j2eefilter_method_insert(FilterLayer* layer, pblock* pb)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    if (!context)
        return REQ_ABORTED; // error, don't insert filter

    return REQ_PROCEED; // insert filter
}


/**
 * NSAPI remove filter method.
 */
extern "C" void j2eefilter_method_remove(FilterLayer* layer)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    j2eefilter_migrate_response(layer->context->rq, context);
}


/**
 * NSAPI read filter method. Calls InputStream.read in response to a net_read
 * call.
 */
extern "C" int j2eefilter_method_read(FilterLayer* layer, void* buf, int amount, int timeout)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    if (!context->inputstream)
        return net_read(context->parent_sn->csd, buf, amount, timeout);

    if (!context->throwable) {
        // Ensure we have a Java byte array
        if (!context->bytearray)
            j2eefilter_create_bytearray(context, J2EEFILTER_DEFAULT_ARRAY_LENGTH);

        // Call InputStream.read
        if (amount > context->bytearray_length)
            amount = context->bytearray_length;
        amount = context->env->CallIntMethod(context->inputstream, _inputstream_read, context->bytearray, 0, amount);
        if (amount > 0)
            context->env->GetByteArrayRegion(context->bytearray, 0, amount, (jbyte*) buf);

        // On EOF, InputStream.read returns -1 but NSAPI filters return 0
        if (amount == -1)
            amount = 0;
           
        // Remember any InputStream.read exception
        j2eefilter_store_throwable(context);
    }

    if (context->throwable) {
        // Propagate exception information to NSAPI land
        JException jexc(context->env, context->throwable);
        NsprError::setErrorf(PR_IO_ERROR, "%s, %s", jexc.toString(), jexc.getMessage());
        return -1;
    }

    return amount;    
}


/**
 * Call HttpServletResponse.flushBuffer if necessary.
 */
static inline void j2eefilter_flush_dirty(J2EEFilterContext* context)
{
    if (context->dirty) {
        // Call HttpServletResponse.flushBuffer
        httpfilter_suppress_flush(context->parent_sn, context->parent_rq, PR_TRUE);
        context->env->CallVoidMethod(context->response, _response_flushBuffer);
        httpfilter_suppress_flush(context->parent_sn, context->parent_rq, PR_FALSE);

        // Remember any HttpServletResponse.flushBuffer exception
        j2eefilter_store_throwable(context);

        context->dirty = PR_FALSE;
    }
}


/**
 * NSAPI write filter method. Calls OutputStream.write or net_write in response
 * to a net_write, etc. call.
 */
extern "C" int j2eefilter_method_write(FilterLayer* layer, const void* buf, int amount)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    j2eefilter_migrate_response(layer->context->rq, context);

    if (!context->outputstream) {
        j2eefilter_flush_dirty(context);
        return net_write(context->parent_sn->csd, buf, amount);
    }

    if (!context->throwable) {
        // Ensure we have a Java byte array big enough to hold all of buf
        if (context->bytearray_length < amount) {
            j2eefilter_destroy_bytearray(context);
            j2eefilter_create_bytearray(context, amount);
        }
        PR_ASSERT(context->bytearray_length >= amount);
    }

    if (!context->throwable) {
        // Call OutputStream.write
        context->env->SetByteArrayRegion(context->bytearray, 0, amount, (jbyte*) buf);
        context->env->CallVoidMethod(context->outputstream, _outputstream_write, context->bytearray, 0, amount);

        // Remember any OutputStream.write exception
        j2eefilter_store_throwable(context);
    }

    if (context->throwable) {
        // Propagate exception information to NSAPI land
        JException jexc(context->env, context->throwable);
        NsprError::setErrorf(PR_IO_ERROR, "%s, %s", jexc.toString(), jexc.getMessage());
        return -1;
    }

    return amount;
}


/**
 * NSAPI writev filter method. Calls j2eefilter_method_write (via
 * filter_emulate_writev) or net_writev in response to a net_writev call.
 */
extern "C" int j2eefilter_method_writev(FilterLayer* layer, const NSAPIIOVec *iov, int iov_size)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    j2eefilter_migrate_response(layer->context->rq, context);

    if (!context->outputstream) {
        j2eefilter_flush_dirty(context);
        return net_writev(context->parent_sn->csd, iov, iov_size);
    }

    // Call OutputStream.write via j2eefilter_method_write
    return filter_emulate_writev(layer, iov, iov_size);
}


/**
 * NSAPI sendfile filter method. Calls j2eefilter_method_write (via
 * filter_emulate_sendfile) or net_sendfile in response to a net_sendfile call.
 */
extern "C" int j2eefilter_method_sendfile(FilterLayer *layer, sendfiledata *sfd)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    j2eefilter_migrate_response(layer->context->rq, context);

    if (!context->outputstream) {
        j2eefilter_flush_dirty(context);
        return net_sendfile(context->parent_sn->csd, sfd);
    }

    // Call OutputStream.write via j2eefilter_method_write
    return filter_emulate_sendfile(layer, sfd);
}


/**
 * NSAPI flush filter method. Calls OutputStream.flush in response to a
 * net_flush call.
 */
extern "C" int j2eefilter_method_flush(FilterLayer* layer)
{
    J2EEFilterContext* context = (J2EEFilterContext*) layer->context->data;

    j2eefilter_migrate_response(layer->context->rq, context);

    if (!context->outputstream) {
        j2eefilter_flush_dirty(context);
        return net_flush(context->parent_sn->csd);
    }

    if (!context->throwable) {
        // Call OutputStream.flush
        context->env->CallVoidMethod(context->outputstream, _outputstream_flush);

        // Remember any OutputStream.flush exception
        j2eefilter_store_throwable(context);
    }

    if (context->throwable) {
        // Propagate exception information to NSAPI land
        JException jexc(context->env, context->throwable);
        NsprError::setErrorf(PR_IO_ERROR, "%s, %s", jexc.toString(), jexc.getMessage());
        return -1;
    }

    return 0;
}


/**
 * Retrieve the J2EE-NSAPI filter.
 */
const Filter* j2eefilter_get_filter()
{
    return _j2eefilter;
}


/**
 * Initialize the J2EE-NSAPI filter subsystem.
 */
PRStatus j2eefilter_init(JNIEnv* env)
{
    FilterMethods methods = FILTER_METHODS_INITIALIZER;
    methods.insert = j2eefilter_method_insert;
    methods.remove = j2eefilter_method_remove;
    methods.read = j2eefilter_method_read;
    methods.write = j2eefilter_method_write;
    methods.writev = j2eefilter_method_writev;
    methods.sendfile = j2eefilter_method_sendfile;
    methods.flush = j2eefilter_method_flush;

    // Create an NSAPI filter that will call OutputStream.write
    _j2eefilter = filter_create("j2ee-filter", FILTER_SUBREQUEST_BOUNDARY, &methods);
    if (!_j2eefilter)
        return PR_FAILURE;

    // Lookup HttpServletResponse methods
    jclass response_class = NSJavaUtil::findClassGlobal(env, "javax/servlet/http/HttpServletResponse");
    if (!response_class)
        return PR_FAILURE;
    _response_setStatus = env->GetMethodID(response_class, "setStatus", "(I)V");
    if (!_response_setStatus)
        return PR_FAILURE;
    _response_setHeader = env->GetMethodID(response_class, "setHeader", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!_response_setHeader)
        return PR_FAILURE;
    _response_addHeader = env->GetMethodID(response_class, "addHeader", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!_response_addHeader)
        return PR_FAILURE;
    _response_flushBuffer = env->GetMethodID(response_class, "flushBuffer", "()V");
    if (!_response_flushBuffer)
        return PR_FAILURE;

    // Lookup InputStream.read
    jclass inputstream_class = NSJavaUtil::findClassGlobal(env, "java/io/InputStream");
    if (!inputstream_class)
        return PR_FAILURE;
    _inputstream_read = env->GetMethodID(inputstream_class, "read", "([BII)I");
    if (!_inputstream_read)
        return PR_FAILURE;

    // Lookup OutputStream methods
    jclass outputstream_class = NSJavaUtil::findClassGlobal(env, "java/io/OutputStream");
    if (!outputstream_class)
        return PR_FAILURE;
    _outputstream_write = env->GetMethodID(outputstream_class, "write", "([BII)V");
    if (!_outputstream_write)
        return PR_FAILURE;
    _outputstream_flush = env->GetMethodID(outputstream_class, "flush", "()V");
    if (!_outputstream_flush)
        return PR_FAILURE;

    return PR_SUCCESS;
}
