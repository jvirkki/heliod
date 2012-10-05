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

/*
 * Description (objndx.c)
 *
 *	This module provides a way to associate numbers, encoded as
 *	strings, with pointers to dynamically created objects.  This
 *	allows these objects to be referenced in parameter block
 *	values.  Pointers to objects of the same type are registered
 *	in an object index structure.  The creator of an object index
 *	may specify a destructor function to be called to destroy
 *	objects in the index at server restart.  The creator should
 *	make arrangements for objndx_destroy() to be called at restart.
 */

#include "base/systems.h"
#include "netsite.h"
#include "util.h"
#include "objndx.h"
#include <assert.h>

#ifdef NOACL

#define DEFINDEXINC	16	/* number of entries to grow index by */

/*
 * Description (ObjNdx_t)
 *
 *	This type describes an object index header.
 */

typedef struct ObjNdx_s ObjNdx_t;
struct ObjNdx_s {
    int onx_avail;		/* list of available indices in the array */
    int onx_nentries;		/* current size of the array */
    void (*onx_free)(void *);	/* pointer to destructor for object */
    void **onx_array;		/* pointer to array of object pointers */
    ObjNdx_t * onx_next;	/* pointer to next object index */
};

/* Pointer to list of all object indices */
static ObjNdx_t * onx_list = NULL;

/*
 * Description (objndx_create)
 *
 *	This function creates an object index and returns a handle
 *	that can be used to reference it in other objndx_xxxx() calls.
 *
 * Arguments:
 *
 *	size		- initial number of index entries (0 is valid)
 *	freefunc	- pointer to a destructor function for objects
 *			  registered in this index
 *
 * Returns:
 *
 *	A handle to the object index is returned if it is successfully
 *	created.  Otherwise NULL is returned.
 */

NSAPI_PUBLIC void * objndx_create(int size, void (*freefunc)(void *))
{
    ObjNdx_t * onx;		/* pointer to new object index */
    int i;

    assert(size >= 0);

    /* Allocate the object index header */
    onx = (ObjNdx_t *)MALLOC(sizeof(ObjNdx_t));
    if (onx != NULL) {

	/* Initialize index empty */
	onx->onx_avail = -1;
	onx->onx_nentries = 0;
	onx->onx_free = freefunc;
	onx->onx_array = NULL;
	onx->onx_next = onx_list;
	onx_list = onx;

	/* Allocate space for object pointers if indicated */
	if (size > 0) {
	    onx->onx_array = (void **)MALLOC(size * sizeof(void *));
	    if (onx->onx_array) {

		/* Create a list of available entries */
		onx->onx_nentries = size;
		for (i = 1; i < size; ++i) {
		    onx->onx_array[i-1] = (void *)i;
		}
		onx->onx_array[size-1] = (void *)-1;
		onx->onx_avail = 0;
	    }
	}
    }

    return (void *)onx;
}

/*
 * Description (objndx_register)
 *
 *	This function registers an object in a specified object index,
 *	returning a pointer to a string that names the object relative
 *	to the index.
 *
 * Arguments:
 *
 *	objndx		- handle for the object index (from objndx_create)
 *	objptr		- pointer to the object to be registered
 *	namebuf		- pointer to buffer to contain object name
 *			  (should be char namebuf[OBJNDXNAMLEN])
 *
 * Returns:
 *
 *	A pointer to a string containing the name assigned to the object
 *	is returned.  NULL is returned if there is an error.
 */

NSAPI_PUBLIC char * objndx_register(void * objndx, void * objptr, 
                                    char * namebuf)
{
    ObjNdx_t * onx = (ObjNdx_t *)objndx;
    int ndx;					/* new object index */
    int i;

    if (!onx) {
	/* Invalid object index handle */
	return NULL;
    }

    ndx = onx->onx_avail;
    if (ndx < 0) {
	int newsize;
	void **newspace;

	/* No more free indices - time to grow the object index */
	newsize = DEFINDEXINC + onx->onx_nentries;

	/* Grow the object pointer space */
	newspace = (void **)REALLOC(onx->onx_array, newsize * sizeof(void **));
	if (!newspace) {
	    return NULL;
	}

	onx->onx_array = newspace;

	/* Add entries to list of available indices */
	for (i = onx->onx_nentries + 1; i < newsize; ++i) {
	    onx->onx_array[i-1] = (void *)i;
	}
	onx->onx_array[newsize-1] = (void **)-1;
	onx->onx_avail = onx->onx_nentries;
	ndx = onx->onx_nentries;
	onx->onx_nentries = newsize;
    }

    /* Allocate index position ndx */
    onx->onx_avail = (int)(onx->onx_array[ndx]);

    /* Store the object pointer there */
    onx->onx_array[ndx] = objptr;

    /* Create the name string */
    util_sprintf(namebuf, "%d", ndx);

    return namebuf;
}

/*
 * Description (objndx_lookup)
 *
 *	This function looks up an object in an object index, given its
 *	string name as returned by objndx_register().
 *
 * Arguments:
 *
 *	objndx		- handle for the object index (from objndx_create)
 *	objname		- pointer to object name string (from objndx_register)
 *
 * Returns:
 *
 *	The pointer to the corresponding object, as passed to objndx_register,
 *	is returned.  NULL is returned if the object index is invalid or the
 *	name is not found.
 */

NSAPI_PUBLIC void * objndx_lookup(void * objndx, char * objname)
{
    ObjNdx_t * onx = (ObjNdx_t *)objndx;
    int ndx = atoi(objname);

    if (!onx || (ndx < 0) || (ndx >= onx->onx_nentries)) {
	return NULL;
    }

    return onx->onx_array[ndx];
}

/*
 * Description (objndx_remove)
 *
 *	This function removes an object from an object index, given its
 *	string name as returned by objndx_register().  It returns a
 *	pointer to the object.  The caller is responsible for disposing
 *	of the object.
 *
 * Arguments:
 *
 *	objndx		- handle for the object index (from objndx_create)
 *	objname		- pointer to object name string (from objndx_register)
 *
 * Returns:
 *
 *	The pointer to the corresponding object, as passed to objndx_register,
 *	is returned.  NULL is returned if the object index is invalid or the
 *	name is not found.
 */

NSAPI_PUBLIC void * objndx_remove(void * objndx, char * objname)
{
    ObjNdx_t * onx = (ObjNdx_t *)objndx;
    int ndx = atoi(objname);
    void * ret;

    if (!onx || (ndx < 0) || (ndx >= onx->onx_nentries)) {
	return NULL;
    }

    ret = onx->onx_array[ndx];
    onx->onx_array[ndx] = (void *)(onx->onx_avail);
    onx->onx_avail = ndx;

    return ret;
}

/*
 * Description (objndx_destroy)
 *
 *	This function destroys an object index, after destroying each
 *	of the objects in it, using the specified object destructor
 *	function.
 *
 * Arguments:
 *
 *	objndx		- handle for the object index (from objndx_create)
 *
 */

NSAPI_PUBLIC void objndx_destroy(void * objndx)
{
    ObjNdx_t * onx = (ObjNdx_t *)objndx;
    int ndx, nextndx;

    if (onx) {

	/* First set all of the available indices to NULL */
	for (ndx = onx->onx_avail; ndx >= 0; ndx = nextndx) {
	    nextndx = (int)(onx->onx_array[ndx]);
	    onx->onx_array[ndx] = NULL;
	}

	/* Now scan the entire array for object pointers */
	for (ndx = 0; ndx < onx->onx_nentries; ++ndx) {
	    if (onx->onx_array[ndx]) {
		/* Call the object destructor function */
		(*onx->onx_free)(onx->onx_array[ndx]);
	    }
	}

	/* Free the object index space */
	FREE(onx->onx_array);
	FREE(onx);
    }
}

#endif /* NOACL */
