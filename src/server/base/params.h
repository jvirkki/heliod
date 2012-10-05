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

#if !defined (BASE_PARAMS_HXX)
#define BASE_PARAMS_HXX

/**
 * Parameter parsing routines
 *
 * @author Ruslan Belkin <ruslan@netscape.com>
 * @version 1.0
 */

#include "base/plist.h"

/**
 * parameter value list
 */
typedef	
struct	_ParamList	{
			struct	_ParamList	*next;	// next element in the list
			struct	_ParamList	*tail;	// the last element in the list (only valid for the head elem)
			unsigned size;				// th size of the list (only valid for the first element)
			char value[1];				// value
		}	ParamList;


NSPR_BEGIN_EXTERN_C

/**
 * param_Parse - parse query String. It does not change the queryString
 *
 * @param pool	pointer to a memory pool
 * @param queryString	string to parse (must end with '\0')
 *
 * @return PList_t
 */
NSAPI_PUBLIC PList_t	param_Parse (pool_handle_t *pool, const char *queryString, PList_t pList);

/**
 * param_GetElem - get values given the parameter name
 *
 * @param pList	PList_t which contains parameters list
 * @param name	the name of the parameter
 *
 * @return	ParamList containing values
 */
NSAPI_PUBLIC ParamList *param_GetElem (PList_t pList, const char *name);

/**
 * param_GetSize - get values given the parameter name
 *
 * @param pList	PList_t which contains parameters list
 * @param name	the name of the parameter
 *
 * @return	the number of value elements
 */
NSAPI_PUBLIC unsigned   param_GetSize (PList_t pList, const char *name);

/**
 * param_GetNames - get all parameter names
 *
 * @param pList	PList_t which contains parameters list
 * @param names	pointer to an array to receive names (can be NULL)
 * @param maxsize maximum size of the array (0 if unbounded)
 *
 * @return	the number of name elements
 */
NSAPI_PUBLIC unsigned	param_GetNames(PList_t pList, char **names, unsigned maxsize);

/**
 * param_Add_PBlock - Adds the contents of pblock to a PList
 *                    The contents of pblock are not altered
 *
 * @param pool  pointer to a memory pool
 * @param pb    pblock
 *
 * @return PList_t
 */
NSAPI_PUBLIC PList_t
param_Add_PBlock(pool_handle_t *pool, PList_t pList, pblock *pb);

NSPR_END_EXTERN_C

#endif
