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

#include <procfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*

#include "httpdaemon/solarisprocessstats.h"

// Contstructor
SolarisProcessStats::SolarisProcessStats(void)
{
}

// Destructor
SolarisProcessStats::~SolarisProcessStats(void)
{
}

PRStatus SolarisProcessStats::init()
{
    return initProcFileDesc();
}

//-----------------------------------------------------------------------------
// SolarisProcessStats::destroy
//-----------------------------------------------------------------------------

PRStatus SolarisProcessStats::destroy()
{
    close(procFileDesc);
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisProcessStats::initProcFileDesc
//-----------------------------------------------------------------------------

PRStatus SolarisProcessStats::initProcFileDesc()
{
    char procfile[64];
    sprintf(procfile, "/proc/%d/psinfo", getpid());
    if ( (procFileDesc = open(procfile, O_RDONLY)) == -1 )
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ProcessStats_OpenFailed), system_errmsg());
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SolarisProcessStats::getMemUsageInfo
//-----------------------------------------------------------------------------

PRStatus SolarisProcessStats::getMemUsage(MemUsageInfo *info)
{
    psinfo_t processInfo;

    if (procFileDesc == -1)
        return PR_FAILURE;

    // lseek to the beginning of the file before reading
    lseek(procFileDesc, 0L, SEEK_SET);
    if ( read(procFileDesc, &processInfo, sizeof(psinfo_t)) != sizeof(psinfo_t) )
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ProcessStats_ReadFailed), system_errmsg());
        return PR_FAILURE;
    }
    info->process_size_in_Kbytes = processInfo.pr_size;
    info->process_resident_size_in_Kbytes = processInfo.pr_rssize;
    info->fraction_system_memory_usage = processInfo.pr_pctmem;
    return PR_SUCCESS;
}
