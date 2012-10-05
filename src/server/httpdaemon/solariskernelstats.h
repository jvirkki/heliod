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

#ifndef _SolarisKernelStats_h_
#define _SolarisKernelStats_h_

#include <kstat.h>
#include "httpdaemon/kernelstatsbase.h"  // kernel stats base class

/**
 * This class represents Kenel Related Statistics.
 * It derives from KernelStatsBase class for the interface declarations
 *
 * @author  $Author: celving $
 * @version $Revision: 1.1.4.2 $ $Date: 2001/07/22 00:43:52 $
 * @since   iWS6.0
 */


class SolarisKernelStats
{
    public:

        /**
         * Default constructor.
         */
        SolarisKernelStats();

        /**
         * Destructor
         */
        ~SolarisKernelStats(void);

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
        kstat_ctl_t *kstatControlPtr;
        PRUint64 oldrbytes;
        PRUint64 oldobytes;
        hrtime_t oldhrtime;
        int noofCpus;
        uint_t *oldCpuIdleTimeCounter;
        uint_t *oldCpuUserTimeCounter;
        uint_t *oldCpuKernelTimeCounter;
        uint_t *oldCpuWaitTimeCounter;
};

#endif /* _SolarisKernelStats_h_ */
