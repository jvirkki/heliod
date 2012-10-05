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
 * Parameter parsing routines
 *
 * @author Ruslan Belkin <ruslan@netscape.com>
 * @version 1.0
 */

# include "netsite.h"
# include "base/util.h"
# include "base/params.h"

# include "pblock.h"
# include "plist_pvt.h"

/**
 * param_Add - add parameter to a PList
 *
 * @param pList	parameters list
 * @param np	pointer to a name substring
 * @param vp	pointer to a value substring
 * @Param nl	size of the name substring
 * @param vl	size of the value substring
 */
static void
param_Add (PList_t pList, const char *np, const char *vp, int nl, int vl)
{
	pool_handle_t *pool = PListGetPool (pList);

	char *name = (char *)pool_malloc (pool, nl + 1);

	if (name  == NULL)
		return;

	util_uri_unescape_plus   (np, name, nl);

	ParamList * new_p = (ParamList *)pool_malloc (pool, sizeof (ParamList) + vl);

	if (new_p == NULL)
	{
		pool_free (pool, name);
		return;
	}

	util_uri_unescape_plus   (vp, new_p -> value, vl);

	new_p -> next =  NULL;
	new_p -> size =  1;
	new_p -> tail = new_p;

	ParamList *p = NULL;
    PListFindValue (pList, name, (void **)(&p), NULL);

	if (p != NULL)
	{
		p -> size++;
		p -> tail -> next = new_p;
		p -> tail = new_p;
	}
	else
	{
	    PListInitProp (pList, 0, name, new_p, 0);
	}
	
	// name is no longer needed
	pool_free (pool, name);
}

/**
 * param_Parse - parse query String. It does not change the queryString
 *
 * @param pool	pointer to a memory pool
 * @param queryString	string to parse (must end with '\0')
 *
 * @return PList_t
 */
NSAPI_PUBLIC PList_t
param_Parse (pool_handle_t *pool, const char *queryString, PList_t pList)
{
	int loopFlag = 1;
	int nl = 0;	// size of the name substring
	int vl = 0;	// size of the value substring
	int state = 0;
	const char *np = queryString;
	const char *vp = NULL;

	if (pList == NULL)
		pList = PListNew (pool);

	if (pList == NULL)
		return NULL;

	while (loopFlag)
	{
		char delim = *queryString++;

		switch (delim)
		{
			case '&' :
			case '\0':

				if (!delim)	
					loopFlag = 0;
				
				state = 0;

				if (nl > 0)
					param_Add (pList, np, vp, nl, vl);
				
				nl = 0;
				vl = 0;
				vp = NULL;
				np = queryString;
				break;

			case '=' :
				state = 1;
				vp = queryString;
				break;

			default:
				if (state)
					vl++;
				else
					nl++;
		} /* switch */
	} /* while */
	return pList;
}

/**
 * param_GetElem - get values given the parameter name
 *
 * @param pList	PList_t which contains parameters list
 * @param name	the name of the parameter
 *
 * @return	ParamList containing values
 */
NSAPI_PUBLIC ParamList * 
param_GetElem (PList_t pList, const char *name)
{
	ParamList *p = NULL;
    PListFindValue (pList, name, (void **)(&p), NULL);

	return p;
}

/**
 * param_GetSize - get values given the parameter name
 *
 * @param pList	PList_t which contains parameters list
 * @param name	the name of the parameter
 *
 * @return	the number of value elements
 */
NSAPI_PUBLIC unsigned
param_GetSize (PList_t pList, const char *name)
{
	ParamList *p = NULL;
    PListFindValue (pList, name, (void **)(&p), NULL);

	return p != NULL ? p -> size : 0;
}

/**
 * param_GetNames - get all parameter names
 *
 * @param pList	PList_t which contains parameters list
 * @param names	pointer to an array to receive names (can be NULL)
 * @param maxsize maximum size of the array (0 if unbounded)
 *
 * @return	the number of name elements
 */
NSAPI_PUBLIC unsigned
param_GetNames (PList_t pList, char **names, unsigned maxsize)
{
	PListStruct_t *pl = (PListStruct_t *)pList;

    unsigned count = pl -> pl_initpi;

	if (maxsize > 0 && count > maxsize)
		count = maxsize;
	
	unsigned rvcnt = count;

    if (count > 0 && names != NULL)
	{
	    PLValueStruct_t ** ppval = (PLValueStruct_t **)(pl -> pl_ppval);
		PLValueStruct_t *  pv;

		/* Loop over the initialized property indices */
		for (int i = 0; i < count; ++i)
		{
			/* Got a property here? */
			pv = ppval[i];
			if (pv)
				names[i] = (char*)pv -> pv_name;
			else
				rvcnt--;
		}
    }
	return rvcnt;
}

/**
 * add_Param_to_PList - Adds a param to PList using param_Add
 *						This function is used as a parameter to
 *						PList iterator.
 *
 */
static void
add_Param_to_PList(char *name, const void *value, void *pl)
{
	param_Add((PList_t) pl, (const char *) name, (const char *) value, strlen(name), strlen((char *) value));
}		
	
/**
 * param_Add_PBlock - Adds the contents of pblock to a PList 
 *					  The contents of pblock are not altered
 *
 * @param pool	pointer to a memory pool
 * @param pb	pblock
 *
 * @return PList_t
 */
NSAPI_PUBLIC PList_t
param_Add_PBlock(pool_handle_t *pool, PList_t pList, pblock *pb)
{
	if (pList == NULL) 
		pList = PListNew (pool);

	if (pList != NULL)
		PListEnumerate(PBTOPL(pb), &add_Param_to_PList, (void *) pList);

	return pList;
}
