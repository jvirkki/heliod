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
 * http_register.c: htaccess module registration routines.
 *
 */

#include "frame/log.h"
#include "frame/func.h"
#include "base/file.h"
#include "base/buffer.h"
#include "htaccess.h"

struct t_command * root;

#ifdef __cplusplus
extern "C"
#endif
NSAPI_PUBLIC int 
htaccess_register(pblock *pb, Session *sn, Request *rq)
{
    char *directive = pblock_findval("directive", pb);
    char *function = pblock_findval("function", pb);
    char *check_fn = pblock_findval("check_fn", pb);
    int argtype = atoi(pblock_findval("argtype", pb));
    int authtype = atoi(pblock_findval("authtype", pb));
    command * item;

    if (!(item = (command *)MALLOC(sizeof(command))))
    {
        return REQ_ABORTED;
    }
    item->directive = STRDUP(directive);
    item->function = STRDUP(function);
    item->argtype = argtype;
    item->authtype = authtype;
    item->check_fn = STRDUP(check_fn);

    /* prepend the new item, order doesn't matter */
    item->next = root;
    root = item;

    return REQ_PROCEED;
}

int
htaccess_call_external_function(char * directive, char * function, int args, char * arg1, char * arg2, pblock *pb, Session *sn, Request *rq)
{
    FuncPtr pFunc = NULL;
    Request *nrq = rq;
    pblock *npb;
    int rv;

    if ((pFunc = func_find(function)) == NULL)
    {
        log_error(LOG_FAILURE, "htaccess-find", sn, rq, "function %s not found.", function);
        return(0);
    }

    /* Insert the variables into the pblock */
    npb = pblock_create(2);
    pblock_nvinsert("fn", function, npb);
    pblock_nvinsert("arg1", arg1, npb);

    if (args == 2)
        pblock_nvinsert("arg2", arg2, npb);

    rv = func_exec(npb, sn, nrq);

    if (rv !=  REQ_PROCEED) {
        pblock_free(npb);
        return (0);
    }

    pblock_free(npb);

    return(1);
}
