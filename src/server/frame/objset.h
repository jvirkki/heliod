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

#ifndef FRAME_OBJSET_H
#define FRAME_OBJSET_H

/*
 * objset.h: Handles object sets
 * 
 * Each object is produced by reading a config file of some form. See the
 * server documentation for descriptions of the directives that are 
 * recognized, what they do, and how they are parsed.
 * 
 * This module requires the pblock and buffer modules from the base library.
 *
 * Rob McCool
 */

#ifndef NETSITE_H
#include "../netsite.h"
#endif /* !NETSITE_H */

#ifndef BASE_PBLOCK_H
#include "../base/pblock.h"
#endif /* !BASE_PBLOCK_H */

#ifndef BASE_BUFFER_H
#include "../base/buffer.h"
#endif /* !BASE_BUFFER_H */

/* ---------------------- NSAPI Function Prototypes ----------------------- */

PR_BEGIN_EXTERN_C

/*
 * INTobjset_scan_buffer will scan through buffer, looking for object 
 * configuration information, and adding them to the object set os if it 
 * finds any. If os is NULL it will allocate a new object set.
 *
 * If any error occurs (syntax error, premature EOF) this function will
 * free os, print an error message into errstr, and return NULL.
 * This is because a config. file error is viewed as a catastrophic error
 * from which httpd should not try to recover. If httpd were to continue
 * after an error, it would not behave as the admin. expected and he/she
 * may not notice until it's too late.
 *
 * Upon EOF the file will not be closed.
 */
NSAPI_PUBLIC httpd_objset *INTobjset_scan_buffer(filebuffer *buf,
                                                 char *errstr, 
                                                 httpd_objset *os);

/*
 * INTobjset_create creates a new object set and returns a pointer to it.
 */
NSAPI_PUBLIC httpd_objset *INTobjset_create(void);

/*
 * INTobjset_free will free an object set, any associated objects, Init
 * functions, etc.
 */
NSAPI_PUBLIC void INTobjset_free(httpd_objset *os);

/*
 * INTobjset_free_setonly frees only the object set and not the associated
 * objects, Init functions, etc.
 */
NSAPI_PUBLIC void INTobjset_free_setonly(httpd_objset *os);

/*
 * INTobjset_new_object will add a new object to objset with the specified
 * name. It returns a pointer to the new object (which may be anywhere in 
 * the objset).
 */
NSAPI_PUBLIC httpd_object *INTobjset_new_object(pblock *name, httpd_objset *os);

/*
 * INTobjset_add_object will add the existing object to os.
 */
NSAPI_PUBLIC void INTobjset_add_object(httpd_object *obj, httpd_objset *os);


/*
 * INTobjset_add_init will add the initialization function specified by 
 * initfn to the given object set. Modifies os->initfns.
 */
NSAPI_PUBLIC void INTobjset_add_init(pblock *initfn, httpd_objset *os);

/*
 * INTobjset_findbyname will find the object in objset having the given name,
 * and return the object if found, and NULL otherwise.
 * ign is a set of objects to ignore.
 */
NSAPI_PUBLIC httpd_object *INTobjset_findbyname(const char *name, httpd_objset *ign, 
                                                httpd_objset *os);

/*
 * INTobjset_findbyppath will find the object in objset having the given 
 * partial path entry. Returns object if found, NULL otherwise.
 * ign is a set of objects to ignore.
 */
NSAPI_PUBLIC httpd_object *INTobjset_findbyppath(char *ppath,
                                                 httpd_objset *ign, 
                                                 httpd_objset *os);

PR_END_EXTERN_C

#define objset_scan_buffer INTobjset_scan_buffer
#define objset_create INTobjset_create
#define objset_free INTobjset_free
#define objset_free_setonly INTobjset_free_setonly
#define objset_new_object INTobjset_new_object
#define objset_add_object INTobjset_add_object
#define objset_add_init INTobjset_add_init
#define objset_findbyname INTobjset_findbyname
#define objset_findbyppath INTobjset_findbyppath

/* ---------------------------- Internal Stuff ---------------------------- */

/*
 * The size of the hash tables that store a directive's parameters
 */
#define PARAMETER_HASH_SIZE 3

/*
 * httpd_objset is a container for a bunch of objects. obj is a 
 * NULL-terminated array of objects. pos points to the entry after the last
 * one in the array. You should not mess with pos, but can read it to find
 * the last entry.
 *
 * The initfns array is a NULL-terminated array of the Init functions 
 * associated with this object set. If there are no Init functions associated
 * with this object set, initfns can be NULL. Each pblock specifies the
 * parameters which are passed to the function when it's executed.
 */
struct httpd_objset {
    int pos;
    httpd_object **obj;

    pblock **initfns;
};

/*
 * objset_load adds object configuration information from the file named
 * filename to the object set os. If os is NULL, a new object set is created.
 * Returns a pointer to object set on success. Logs an error and returns NULL
 * on failure.
 */
NSAPI_PUBLIC httpd_objset *objset_load(const char *filename, httpd_objset *os);

/*
 * objset_save writes an object set to disk.
 */
NSAPI_PUBLIC PRStatus objset_save(const char *filename, httpd_objset *os);

NSAPI_PUBLIC httpd_objset *objset_dup(const httpd_objset *src);

httpd_objset *objset_create_pool(pool_handle_t *pool);

/*
 * objset_substitute_vs_vars resolves $variable references in object names.
 */
PRStatus objset_substitute_vs_vars(const VirtualServer *vs, httpd_objset *os);

/*
 * objset_interpolative marks an object set and its contained objects as
 * interpolative, constructing pblock models for any pblocks that contain
 * $fragments.
 */
PRStatus objset_interpolative(httpd_objset *os);

#endif /* FRAME_OBJSET_H */
