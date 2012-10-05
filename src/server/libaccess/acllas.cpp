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

#include <netsite.h>
#include <base/nsassert.h>
#include <libaccess/acl.h>              // generic ACL definitions
#include <libaccess/aclproto.h>         // internal prototypes
#include <libaccess/aclglobal.h>        // global data
#include <libaccess/aclerror.h>         // error codes
#include "aclpriv.h"                    // internal data structure definitions
#include <libaccess/dbtlibaccess.h>     // strings
#include "plhash.h"

// -----------------------------------------------------------------------------
// ACL_LasHash
// -----------------------------------------------------------------------------
void
ACL_LasHashInit()
{
    int	i;

    ACLLasEvalHash = PR_NewHashTable(0,
				     PR_HashString,
				     PR_CompareStrings,
				     PR_CompareValues,
				     &ACLPermAllocOps,
				     NULL);
    NS_ASSERT(ACLLasEvalHash);

    ACLLasFlushHash = PR_NewHashTable(0,
				     PR_HashString,
				     PR_CompareStrings,
				     PR_CompareValues,
				     &ACLPermAllocOps,
				     NULL);
    NS_ASSERT(ACLLasFlushHash);
}

void
ACL_LasHashDestroy()
{
    if (ACLLasEvalHash) {
        PR_HashTableDestroy(ACLLasEvalHash);
        ACLLasEvalHash=NULL;
    }
    if (ACLLasFlushHash) {
        PR_HashTableDestroy(ACLLasFlushHash);
        ACLLasFlushHash=NULL;
    }
}

/*  ACL_LasRegister
 *  INPUT
 *	errp		NSError structure
 *	attr_name	E.g. "ip" or "dns" etc.
 *	eval_func	E.g. LASIpEval
 *	flush_func	Optional - E.g. LASIpFlush or NULL
 *  OUTPUT
 *	0 on success, non-zero on failure
 */
NSAPI_PUBLIC int
ACL_LasRegister(NSErr_t *errp, char *attr_name, LASEvalFunc_t eval_func, LASFlushFunc_t flush_func)
{

    if ((!attr_name) || (!eval_func)) return -1;

    ACL_CritEnter();

    /*  See if the function is already registered.  If so, report and
     *  error, but go ahead and replace it.
     */
    if (PR_HashTableLookup(ACLLasEvalHash, attr_name) != NULL) {
	nserrGenerate(errp, ACLERRDUPSYM, ACLERR3900, ACL_Program, 1,
                      attr_name);
    }

    /*  Put it in the hash tables  */
    PR_HashTableAdd(ACLLasEvalHash, attr_name, (void*)eval_func);
    PR_HashTableAdd(ACLLasFlushHash, attr_name, (void*)flush_func);

    ACL_CritExit();
    return 0;
}

/*  ACL_LasFindEval
 *  INPUT
 *      errp            NSError pointer
 *      attr_name       E.g. "ip" or "user" etc.
 *      eval_funcp      Where the function pointer is returned.  NULL if the
 *                      function isn't registered.
 *  OUTPUT
 *      0 on success, non-zero on failure
 */
NSAPI_PUBLIC int
ACL_LasFindEval(NSErr_t *errp, char *attr_name, LASEvalFunc_t *eval_funcp)
{
 
    NS_ASSERT(attr_name);
    if (!attr_name) return -1;
 
    *eval_funcp = (LASEvalFunc_t)PR_HashTableLookup(ACLLasEvalHash, attr_name);
    return 0;
}
 
 
/*  ACL_LasFindFlush
 *  INPUT
 *      errp            NSError pointer
 *      attr_name       E.g. "ip" or "user" etc.
 *      eval_funcp      Where the function pointer is returned.  NULL if the
 *                      function isn't registered.
 *  OUTPUT
 *      0 on success, non-zero on failure
 */
NSAPI_PUBLIC int 
ACL_LasFindFlush(NSErr_t *errp, char *attr_name, LASFlushFunc_t *flush_funcp)
{
 
    NS_ASSERT(attr_name);
    if (!attr_name) return -1;
 
    *flush_funcp = (LASFlushFunc_t)PR_HashTableLookup(ACLLasFlushHash, attr_name);
    return 0;
}

