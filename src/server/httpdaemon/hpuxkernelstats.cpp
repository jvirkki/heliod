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

#include <sys/resource.h>
#include <sys/dk.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*
#include "httpdaemon/hpuxkernelstats.h"
#include <sys/pstat.h>

// Contstructor
HpuxKernelStats::HpuxKernelStats(void)
{
    noofCpus = 1;
}

// Destructor
HpuxKernelStats::~HpuxKernelStats(void)
{
    /* nothing to do die a graceful death */
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::init
//-----------------------------------------------------------------------------
PRStatus HpuxKernelStats::init()
{
    struct pst_dynamic psd;
    if(pstat_getdynamic(&psd, sizeof( psd ), (size_t)1,0)==-1){
        /* Error */
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_OpenFailed), system_errmsg());
        return PR_FAILURE;
    }
    /* get the active number of CPUs */
    noofCpus = psd.psd_proc_cnt;

    /* Allocate and Initialize CPU counters */
    oldCpuIdleTimeCounter = (unsigned int *) malloc(noofCpus*sizeof(unsigned int));
    oldCpuUserTimeCounter = (unsigned int *) malloc(noofCpus*sizeof(unsigned int));
    oldCpuKernelTimeCounter = (unsigned int *) malloc(noofCpus*sizeof(unsigned int));
    oldCpuWaitTimeCounter = (unsigned int *) malloc(noofCpus*sizeof(unsigned int));

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::getNumberOfCpu
//-----------------------------------------------------------------------------
int HpuxKernelStats::getNumberOfCpus()
{
    return noofCpus;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::destroy
//-----------------------------------------------------------------------------

PRStatus HpuxKernelStats::destroy()
{
    /* Free memory allocated */
    free(oldCpuIdleTimeCounter);
    free(oldCpuUserTimeCounter);
    free(oldCpuKernelTimeCounter);
    free(oldCpuWaitTimeCounter);

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::update
//-----------------------------------------------------------------------------

PRStatus HpuxKernelStats::update()
{
    /*uptime */
    struct pst_static pst;
    memset(&pst,sizeof(pst),0);
    if(pstat_getstatic(&pst, sizeof(pst),1,0)==-1){
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_Inconsistency), system_errmsg());
        return PR_FAILURE;
    }
    /* reserved for future usage */
    uptime = time(0) - pst.boot_time;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::getLoadAverages
//-----------------------------------------------------------------------------

PRStatus HpuxKernelStats::getLoadAverages(LoadAveragesInfo *info)
{
    struct pst_dynamic s_pst_dynamic;
    if(pstat_getdynamic( &s_pst_dynamic, sizeof ( s_pst_dynamic ),(size_t)1,0)==-1){
        /* Error message */
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_ReadFailed), system_errmsg());
        return PR_FAILURE;
    }

    /* Get the system load averages for 1, 5, 15 min */
    info->load_avg_for_1min =  s_pst_dynamic.psd_avg_1_min;
    info->load_avg_for_5min = s_pst_dynamic.psd_avg_5_min;
    info->load_avg_for_15min = s_pst_dynamic.psd_avg_15_min;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::getNetworkThroughPut
//-----------------------------------------------------------------------------

PRStatus HpuxKernelStats::getNetworkThroughPut(NetworkStatsInfo *info)
{
    /* not impletmented */
    /* Set the return values here. */
    info->in_bytes_per_sec = 0;
    info->out_bytes_per_sec =0;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxKernelStats::getProcessorInfo
//-----------------------------------------------------------------------------

PRStatus HpuxKernelStats::getProcessorInfo(ProcessorInfo *info)
{
    struct pst_dynamic s_pst_dynamic;
    int ncpus = 0;

    if(pstat_getdynamic(&s_pst_dynamic, sizeof( s_pst_dynamic ), (size_t)1,0)==-1){
         /* Error */
         ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_ReadFailed), system_errmsg());
         return PR_FAILURE;
    }
    /* Get active number of cpus */
    ncpus = s_pst_dynamic.psd_proc_cnt;


    /* Calculate CPU load distribution in idle, user and system states */
    for ( int i = 0; i < ncpus; i++ )
    {
        struct pst_processor sppr;
        if(pstat_getprocessor(&sppr,sizeof(sppr),1,i)==-1){
            /* Error */
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_KernelStats_ReadFailed), system_errmsg());
            return PR_FAILURE;
        }

        unsigned int newCpuIdleTimeCounter = sppr.psp_cpu_time[CP_IDLE];
        unsigned int newCpuUserTimeCounter = sppr.psp_cpu_time[CP_USER];
        unsigned int newCpuKernelTimeCounter = sppr.psp_cpu_time[CP_SYS]
            +sppr.psp_cpu_time[CP_NICE] +sppr.psp_cpu_time[CP_INTR]
            +sppr.psp_cpu_time[CP_SSYS];
        unsigned int newCpuWaitTimeCounter = sppr.psp_cpu_time[CP_WAIT]
            +sppr.psp_cpu_time[CP_SWAIT] +sppr.psp_cpu_time[CP_BLOCK];

        unsigned int cpuIdleTime;
        unsigned int cpuUserTime;
        unsigned int cpuKernelTime;
        unsigned int cpuWaitTime;

        if ( newCpuIdleTimeCounter >= oldCpuIdleTimeCounter[i] )
            cpuIdleTime = newCpuIdleTimeCounter - oldCpuIdleTimeCounter[i];
        else if ( newCpuIdleTimeCounter < oldCpuIdleTimeCounter[i] )
            cpuIdleTime = oldCpuIdleTimeCounter[i] - newCpuIdleTimeCounter;
        if ( newCpuUserTimeCounter >= oldCpuUserTimeCounter[i] )
            cpuUserTime = newCpuUserTimeCounter - oldCpuUserTimeCounter[i];
        else if ( newCpuUserTimeCounter < oldCpuUserTimeCounter[i] )
            cpuUserTime = oldCpuUserTimeCounter[i] - newCpuUserTimeCounter;
        if ( newCpuKernelTimeCounter >= oldCpuKernelTimeCounter[i] )
            cpuKernelTime = newCpuKernelTimeCounter - oldCpuKernelTimeCounter[i];
        else if ( newCpuKernelTimeCounter < oldCpuKernelTimeCounter[i] )
            cpuKernelTime = oldCpuKernelTimeCounter[i] - newCpuKernelTimeCounter;
        if ( newCpuWaitTimeCounter >= oldCpuWaitTimeCounter[i] )
            cpuWaitTime = newCpuWaitTimeCounter - oldCpuWaitTimeCounter[i];
        else if ( newCpuWaitTimeCounter < oldCpuWaitTimeCounter[i] )
            cpuWaitTime = oldCpuWaitTimeCounter[i] - newCpuWaitTimeCounter;

        /* Count the toal time */
        unsigned int totalTime = 0;
        totalTime = cpuIdleTime + cpuUserTime + cpuKernelTime + cpuWaitTime;
        if ( totalTime != 0 )
        {
            info[i].percent_idle_time = 100 * ((double)cpuIdleTime)/((double)totalTime);
            info[i].percent_user_time = 100 * ((double)cpuUserTime)/((double)totalTime);
            info[i].percent_kernel_time = 100 * ((double)cpuKernelTime)/((double)totalTime);
        }

        /* Now replace the old values with the new ones */
        oldCpuIdleTimeCounter[i] = newCpuIdleTimeCounter;
        oldCpuUserTimeCounter[i] = newCpuUserTimeCounter;
        oldCpuKernelTimeCounter[i] = newCpuKernelTimeCounter;
        oldCpuWaitTimeCounter[i] = newCpuWaitTimeCounter;
    }

    return PR_SUCCESS;
}

