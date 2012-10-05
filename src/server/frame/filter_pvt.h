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

#ifndef FILTER_PVT_H
#define FILTER_PVT_H

#ifndef FRAME_HTTPDIR_H
#include "frame/httpdir.h"
#endif

/*
 * filter_generic_callback sets up the the filter stack for the currently
 * executing request.
 */
inline void filter_generic_callback(Session *sn)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    // Setup the filter stack for the currently executing request
    if (nsn->filter_rq != nsn->exec_rq)
        filter_set_request(&nsn->sn, &nsn->exec_rq->rq);
}


/*
 * filter_read_callback sets up the the filter stack for the currently
 * executing request and indicates that no more Input directives should be
 * run for this Session.
 */
inline void filter_read_callback(Session *sn)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    // Setup the filter stack for the currently executing request
    if (nsn->filter_rq != nsn->exec_rq)
        filter_set_request(&nsn->sn, &nsn->exec_rq->rq);

    // Since some request body has been read, no more Input directives should
    // be run
    if (!nsn->input_done) {
        nsn->input_rv = REQ_PROCEED;
        nsn->input_done = PR_TRUE;
    }
}


/*
 * filter_output_callback sets up the filter stack for the currently executing
 * request and calls servact_output() as necessary.  Returns REQ_NOACTION if
 * the filter stack was not modified or REQ_PROCEED if all configured filters
 * were successfully inserted. Any other return value is an error returned by
 * an Output directive.
 */
inline int filter_output_callback(Session *sn)
{
    NSAPISession *nsn = (NSAPISession *)sn;

    // Setup the filter stack for the currently executing request
    if (nsn->filter_rq != nsn->exec_rq)
        filter_set_request(&nsn->sn, &nsn->exec_rq->rq);

    Request *rq = &nsn->filter_rq->rq;

    // Fast path out if the Output stage has already been run
    if (request_output_done(rq)) {
        int rv = request_output_rv(rq);

        // If the stage returned REQ_PROCEED, REQ_NOACTION should have been
        // stored.  REQ_PROCEED indicates that new filters were added on this
        // particular invocation.
        PR_ASSERT(rv != REQ_PROCEED);

        // Set an error if the previous attempt to run the Output stage failed
        if (rv != REQ_NOACTION) {
            NsprError::setErrorf(PR_INVALID_STATE_ERROR,
                                 XP_GetAdminStr(DBT_StageXError),
                                 directive_num2name(NSAPIOutput));
        }

        return rv;
    }

    // Run the Output stage for this Request
    int rv = servact_output(sn, rq);

    // Set an error if the Output stage failed
    if (rv != REQ_NOACTION && rv != REQ_PROCEED) {
        NsprError::setErrorf(PR_INVALID_STATE_ERROR,
                             XP_GetAdminStr(DBT_StageXError),
                             directive_num2name(NSAPIOutput));
    }

    return rv;
}

#endif /* FILTER_PVT_H */
