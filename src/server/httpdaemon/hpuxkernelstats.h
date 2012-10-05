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

#ifndef _HpuxKernelStats_h_
#define _HpuxKernelStats_h_
/* enable the compatibility */
#define _ICOD_BASE_INFO

#include <time.h>
#include <ctype.h>
#include <sys/scall_define.h>
#include "httpdaemon/kernelstatsbase.h"  // kernel stats base class

/**
 * This class represents Kenel Related Statistics.
 * It derives from KernelStatsBase class for the interface declarations
 *
 * @author  $Author: svbld $
 * @version $Revision: 1.1.2.1 $ $Date: 2006/07/26 23:58:35 $
 * @since   iWS6.1sp6
 */

class HpuxKernelStats
{
    public:

        /**
         * Default constructor.
         */
        HpuxKernelStats();

        /**
         * Destructor
         */
        ~HpuxKernelStats(void);

        /**
         * Initialize the kernel statistics subsystem
         *
         * @returns      <code>PR_SUCCESS</code> if the initialization
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus init(void);

        /**
         * destroy the kernel statistics subsystem
         *
         * @returns      <code>PR_SUCCESS</code> if the destroy
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus destroy(void);

        /**
         * update the kernel statistics
         *
         * @returns      <code>PR_SUCCESS</code> if the update
         *               of the stats was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus update(void);

        /**
         * Get the load averages of the system for 1min, 5min and 15min
         *
         * @returns      <code>PR_SUCCESS</code> if this call is successful
         *               <code>PR_FAILURE</code> if error
         *               if there was an error.
         */
        PRStatus getLoadAverages(LoadAveragesInfo *);

        /**
         * Get the network statistics like input, output bytes/sec
         *
         * @returns      <code>PR_SUCCESS</code> if this call is successful
         *               <code>PR_FAILURE</code> if error
         *               if there was an error.
         */
        PRStatus getNetworkThroughPut(NetworkStatsInfo *);

        /**
         * Get the cpu related statistics
         *
         * @returns      <code>PR_SUCCESS</code> if this call is successful
         *               <code>PR_FAILURE</code> if error
         *               if there was an error.
         */
        PRStatus getProcessorInfo(ProcessorInfo *);

       /**
        * Get the number of CPUs
        */
        int getNumberOfCpus(void);

    private:

        int noofCpus;
        time_t grab_time;
        time_t uptime;
        unsigned int *oldCpuIdleTimeCounter;
        unsigned int *oldCpuUserTimeCounter;
        unsigned int *oldCpuKernelTimeCounter;
        unsigned int *oldCpuWaitTimeCounter;

};

#endif /* _HpuxKernelStats_h_ */
