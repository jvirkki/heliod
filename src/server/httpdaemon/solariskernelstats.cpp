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

#include <kstat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sysinfo.h> // cpu_stat_t
#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*

#include "httpdaemon/solariskernelstats.h"

// Contstructor
SolarisKernelStats::SolarisKernelStats(void)
{
    oldrbytes = 0;
    oldobytes = 0;
    oldhrtime = 0;
    noofCpus = 1;
}

// Destructor
SolarisKernelStats::~SolarisKernelStats(void)
{
}

PRStatus SolarisKernelStats::init()
{
    // Initialize kstat control structures
    kstatControlPtr = kstat_open();
    if ( !kstatControlPtr )
    {
        // Error
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_OpenFailed), system_errmsg());
        return PR_FAILURE;
    }

    // get the number of CPUs
    noofCpus = sysconf(_SC_NPROCESSORS_CONF);

    // Allocate and Initialize CPU counters
    oldCpuIdleTimeCounter = (uint_t *) malloc(noofCpus*sizeof(uint_t));
    oldCpuUserTimeCounter = (uint_t *) malloc(noofCpus*sizeof(uint_t));
    oldCpuKernelTimeCounter = (uint_t *) malloc(noofCpus*sizeof(uint_t));
    oldCpuWaitTimeCounter = (uint_t *) malloc(noofCpus*sizeof(uint_t));

    return PR_SUCCESS;
}

int SolarisKernelStats::getNumberOfCpus()
{
    return noofCpus;
}

//-----------------------------------------------------------------------------
// SolarisKernelStats::destroy
//-----------------------------------------------------------------------------

PRStatus SolarisKernelStats::destroy()
{
    // Free kstat resources
    if ( kstat_close(kstatControlPtr) == -1 )
    {
        // Error
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_CloseFailed), system_errmsg());
        return PR_FAILURE;
    }

    // Free memory allocated
    free(oldCpuIdleTimeCounter);
    free(oldCpuUserTimeCounter);
    free(oldCpuKernelTimeCounter);
    free(oldCpuWaitTimeCounter);

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisKernelStats::update
//-----------------------------------------------------------------------------

PRStatus SolarisKernelStats::update()
{
    kid_t kstatChainId = 0;
    kid_t newKstatChainId = 0;

    kstatChainId = kstatControlPtr->kc_chain_id;
    newKstatChainId = kstat_chain_update(kstatControlPtr);
    while ( newKstatChainId != 0 )
    {
        if ( newKstatChainId <= 0 )
        {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_Inconsistency), system_errmsg());
            return PR_FAILURE;
        }
        newKstatChainId = kstat_chain_update(kstatControlPtr);
    }
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisKernelStats::getLoadAverages
//-----------------------------------------------------------------------------

PRStatus SolarisKernelStats::getLoadAverages(LoadAveragesInfo *info)
{
    kstat_t *ksp = NULL;
    kstat_named_t *ksname = NULL;

    /* Scan the kstat chain to find the system_misc kstat in the module
     * unix
     */
    ksp = kstat_lookup(kstatControlPtr, "unix", 0, "system_misc");

    /* Read data for the kstat pointed to by ksp */
    if ( kstat_read(kstatControlPtr, ksp, 0) == -1 )
    {
        // Error message
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_ReadFailed), system_errmsg());
        return PR_FAILURE;
    }

    /* Get the system load averages for 1, 5, 15 min */
    ksname = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_1min");
    if ( ksname != NULL )
        info->load_avg_for_1min = (double) ksname->value.l/FSCALE;
    ksname = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_5min");
    if ( ksname != NULL )
        info->load_avg_for_5min = (double) ksname->value.l/FSCALE;
    ksname = (kstat_named_t *) kstat_data_lookup(ksp, "avenrun_15min");
    if ( ksname != NULL )
        info->load_avg_for_15min = (double) ksname->value.l/FSCALE;
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisKernelStats::getNetworkThroughPut
//-----------------------------------------------------------------------------

PRStatus SolarisKernelStats::getNetworkThroughPut(NetworkStatsInfo *info)
{
    kstat_t *ksp = NULL;
    kstat_named_t *ksname = NULL;
    PRUint64 newrbytes=0;
    PRUint64 newobytes=0;
    PRUint64 diffrbytes=0;
    PRUint64 diffobytes=0;
    PRUint64 rbytespersec;
    PRUint64 obytespersec;
    hrtime_t newhrtime = 0;
    hrtime_t diffhrtime = 0;

    /* Scan the kstat chain to find the network interface kstat
     * Loop through each interface and accumulate the stats
     */
    for ( ksp = kstatControlPtr->kc_chain; ksp; ksp = ksp->ks_next ) {
        if (strcmp(ksp->ks_class, "net") == 0) {
            /* Read data for the kstat pointed to by ksp */
            if ( kstat_read(kstatControlPtr, ksp, 0) != -1 )
            {
                newhrtime = ksp->ks_snaptime;
                ksname = (kstat_named_t *) kstat_data_lookup(ksp, "rbytes");
                if ( ksname != NULL )
                    newrbytes += ksname->value.ui32;
                ksname = (kstat_named_t *) kstat_data_lookup(ksp, "obytes");
                if ( ksname != NULL )
                    newobytes += ksname->value.ui32;
            }
        }
    }

    if ( newrbytes < oldrbytes )
    {
        diffrbytes = oldrbytes - newrbytes;
        diffobytes = oldobytes - newobytes;
        diffhrtime = oldhrtime - newhrtime;
    }
    else
    {
        diffrbytes = newrbytes - oldrbytes;
        diffobytes = newobytes - oldobytes;
        diffhrtime = newhrtime - oldhrtime;
    }

    if ( diffhrtime != 0 )
    {
        rbytespersec = diffrbytes * 1000000000 / diffhrtime;
        obytespersec = diffobytes * 1000000000 / diffhrtime;
    }

    // Now store the new values to old values
    oldrbytes = newrbytes;
    oldobytes = newobytes;
    oldhrtime = newhrtime;

    // Set the return values here.
    info->in_bytes_per_sec = rbytespersec;
    info->out_bytes_per_sec = obytespersec;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisKernelStats::getProcessorInfo
//-----------------------------------------------------------------------------

PRStatus SolarisKernelStats::getProcessorInfo(ProcessorInfo *info)
{
    kstat_t *ksp = NULL;
    kstat_named_t *ksname = NULL;
    cpu_stat_t *cpu_stats; /* cpu stats for each cpu */
    int ncpus = 0;
    /* Scan the kstat chain to find the system_misc kstat in the module
     * unix
     */
    ksp = kstat_lookup(kstatControlPtr, "unix", 0, "system_misc");

    /* Get number of cpus from kstat */
    ksname = (kstat_named_t *) kstat_data_lookup(ksp, "ncpus");

    ncpus = ksname->value.ui32;

    /* Check is this equal to our countCpuInfoSlots which got from sysconf */
    /* Throw an error if they don't match */
    /* ncpus = countCpuInfoSlots; */

    /* Allocate memory for cpustats */
    cpu_stats = (cpu_stat_t *) malloc(ncpus*sizeof(cpu_stat_t));

    int j=0;
    for ( ksp = kstatControlPtr->kc_chain; ksp; ksp = ksp->ks_next )
    {
        if ( strncmp(ksp->ks_name, "cpu_stat", 8 ) == 0 ) {
            if ( kstat_read(kstatControlPtr, ksp, NULL) == -1 )
            {
                // Error
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_ReadFailed), system_errmsg());
                free(cpu_stats);
                return PR_FAILURE;
            }
            memcpy(&cpu_stats[j], ksp->ks_data, sizeof(cpu_stats[j]));
            j++;
        }
        if ( j >= ncpus )
            break; /* some thing wrong */
    }

    uint_t *newCpuIdleTimeCounter;
    uint_t *newCpuUserTimeCounter;
    uint_t *newCpuKernelTimeCounter;
    uint_t *newCpuWaitTimeCounter;
    uint_t *cpuIdleTime;
    uint_t *cpuUserTime;
    uint_t *cpuKernelTime;
    uint_t *cpuWaitTime;

    newCpuIdleTimeCounter = (uint_t *) malloc(ncpus*sizeof(uint_t));
    newCpuUserTimeCounter = (uint_t *) malloc(ncpus*sizeof(uint_t));
    newCpuKernelTimeCounter = (uint_t *) malloc(ncpus*sizeof(uint_t));
    newCpuWaitTimeCounter = (uint_t *) malloc(ncpus*sizeof(uint_t));
    cpuIdleTime = (uint_t *) malloc(ncpus*sizeof(uint_t));
    cpuUserTime = (uint_t *) malloc(ncpus*sizeof(uint_t));
    cpuKernelTime = (uint_t *) malloc(ncpus*sizeof(uint_t));
    cpuWaitTime = (uint_t *) malloc(ncpus*sizeof(uint_t));

    /* Calculate CPU load distribution in idle, user and system states */
    for ( int i = 0; i < ncpus && i < noofCpus; i++ )
    {

        newCpuIdleTimeCounter[i] = cpu_stats[i].cpu_sysinfo.cpu[CPU_IDLE];
        newCpuUserTimeCounter[i] = cpu_stats[i].cpu_sysinfo.cpu[CPU_USER];
        newCpuKernelTimeCounter[i] = cpu_stats[i].cpu_sysinfo.cpu[CPU_KERNEL];
        newCpuWaitTimeCounter[i] = cpu_stats[i].cpu_sysinfo.cpu[CPU_WAIT];

        if ( newCpuIdleTimeCounter[i] >= oldCpuIdleTimeCounter[i] )
            cpuIdleTime[i] = newCpuIdleTimeCounter[i] - oldCpuIdleTimeCounter[i];
        else if ( newCpuIdleTimeCounter[i] < oldCpuIdleTimeCounter[i] )
            cpuIdleTime[i] = oldCpuIdleTimeCounter[i] - newCpuIdleTimeCounter[i];
        if ( newCpuUserTimeCounter[i] >= oldCpuUserTimeCounter[i] )
            cpuUserTime[i] = newCpuUserTimeCounter[i] - oldCpuUserTimeCounter[i];
        else if ( newCpuUserTimeCounter[i] < oldCpuUserTimeCounter[i] )
            cpuUserTime[i] = oldCpuUserTimeCounter[i] - newCpuUserTimeCounter[i];
        if ( newCpuKernelTimeCounter[i] >= oldCpuKernelTimeCounter[i] )
            cpuKernelTime[i] = newCpuKernelTimeCounter[i] - oldCpuKernelTimeCounter[i];
        else if ( newCpuKernelTimeCounter[i] < oldCpuKernelTimeCounter[i] )
            cpuKernelTime[i] = oldCpuKernelTimeCounter[i] - newCpuKernelTimeCounter[i];
        if ( newCpuWaitTimeCounter[i] >= oldCpuWaitTimeCounter[i] )
            cpuWaitTime[i] = newCpuWaitTimeCounter[i] - oldCpuWaitTimeCounter[i];
        else if ( newCpuWaitTimeCounter[i] < oldCpuWaitTimeCounter[i] )
            cpuWaitTime[i] = oldCpuWaitTimeCounter[i] - newCpuWaitTimeCounter[i];

        // Count the toal time
        uint_t totalTime = 0;
        totalTime = cpuIdleTime[i] + cpuUserTime[i] + cpuKernelTime[i] + cpuWaitTime[i];
        if ( totalTime != 0 )
        {
            info[i].percent_idle_time = 100 * ((double) cpuIdleTime[i])/((double) totalTime);
            info[i].percent_user_time = 100 * ((double) cpuUserTime[i])/((double) totalTime);
            info[i].percent_kernel_time = 100 * ((double) cpuKernelTime[i])/((double) totalTime);
        }

        // Now replace the old values with the new ones
        oldCpuIdleTimeCounter[i] = newCpuIdleTimeCounter[i];
        oldCpuUserTimeCounter[i] = newCpuUserTimeCounter[i];
        oldCpuKernelTimeCounter[i] = newCpuKernelTimeCounter[i];
        oldCpuWaitTimeCounter[i] = newCpuWaitTimeCounter[i];
    }

    // Free cpu stats
    free(cpu_stats);

    free(newCpuIdleTimeCounter);
    free(newCpuUserTimeCounter);
    free(newCpuKernelTimeCounter);
    free(newCpuWaitTimeCounter);
    free(cpuIdleTime);
    free(cpuUserTime);
    free(cpuKernelTime);
    free(cpuWaitTime);

    return PR_SUCCESS;
}
