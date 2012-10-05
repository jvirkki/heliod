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

#ifndef _LinuxProcessStats_h_
#define _LinuxProcessStats_h_

#include "httpdaemon/processstatsbase.h"  // stats base class declarations
#include "httpdaemon/fileutility.h"

/**
 * This class represents the Linux Process Related Statistics.
 * It derives from ProcessStatsBase class for the interface declarations
 *
 * @author  $Author: bk139067 $
 * @version $Revision: 1.1.6.2 $ $Date: 2006/02/26 19:22:46 $
 * @since   S1AS 7
 */


class LinuxProcessStats
{
    public:

        /**
         * Default constructor.
         */
        LinuxProcessStats();

        /**
         * Destructor
         */
        ~LinuxProcessStats(void);

        /**
         * Initialize the Process statistics subsystem
         *
         * @returns      <code>PR_SUCCESS</code> if the initialization
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus init(void);

        /**
         * destroy the Process statistics subsystem
         *
         * @returns      <code>PR_SUCCESS</code> if the destroy
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus destroy(void);

        /**
         * update the Process statistics
         *
         * @returns      <code>PR_SUCCESS</code> if the update
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus update(void);

        /**
         * Get the memory usage statistics of the process
         *
         * @returns      <code>PR_SUCCESS</code> if this call is successful
         *               <code>PR_FAILURE</code> if error
         *               if there was an error.
         */
        PRStatus getMemUsage(MemUsageInfo *);

        int new_atoi(const char *nptr);
        long new_atol(const char *nptr);

    private:
        int procFileDesc;
        PRStatus initProcFileDesc(void);
	int IsThread(FileUtility *parent , FileUtility *sibling);
};

#endif /* _LinuxProcessStats_h_ */
