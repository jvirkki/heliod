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

#include <unistd.h>
#include <sys/pstat.h>
#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*
#include "httpdaemon/hpuxprocessstats.h"

// Contstructor
HpuxProcessStats::HpuxProcessStats(void)
{
}

// Destructor
HpuxProcessStats::~HpuxProcessStats(void)
{
}

//-----------------------------------------------------------------------------
// HpuxProcessStats::init
//-----------------------------------------------------------------------------
PRStatus HpuxProcessStats::init()
{
    struct pst_static s_pst_static;
    memset(&s_pst_static,sizeof(s_pst_static),0);
    if(pstat_getstatic(&s_pst_static, sizeof(s_pst_static),1,0)==-1){
       ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ProcessStats_OpenFailed), system_errmsg());
       return PR_FAILURE;
    }

    long page_size = s_pst_static.page_size;
    page_size_kb = page_size/1024;

    mem_total_kb = s_pst_static.physical_memory * page_size_kb;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxProcessStats::destroy
//-----------------------------------------------------------------------------

PRStatus HpuxProcessStats::destroy()
{
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// HpuxProcessStats::getMemUsageInfo
//-----------------------------------------------------------------------------

PRStatus HpuxProcessStats::getMemUsage(MemUsageInfo *info)
{
    struct pst_status pst_status_buf;
    if(pstat_getproc(&pst_status_buf,sizeof(pst_status_buf),0,getpid())== -1){
       ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ProcessStats_ReadFailed), system_errmsg());
       return PR_FAILURE;
    }
    /*
     vtsize--text,vdsize--data, vssize--stack, vshmsize--shared memory,
     vmmsize--mem-maped files, vusize--U area, viosize-- IO dev mapping
    */
    info->process_size_in_Kbytes = (pst_status_buf.pst_vtsize+pst_status_buf.pst_vdsize 
        +pst_status_buf.pst_vssize+pst_status_buf.pst_vshmsize+pst_status_buf.pst_vmmsize 
        +pst_status_buf.pst_vusize+pst_status_buf.pst_viosize)*page_size_kb;
    info->process_resident_size_in_Kbytes =(pst_status_buf.pst_rssize)*page_size_kb;
    /*
       multiply 0x8000 to convert the format to high byte to conforming the definition
       of fraction_sys_memory_usage as Solaris. 
    */
    info->fraction_system_memory_usage = info->process_resident_size_in_Kbytes*0x8000/mem_total_kb;
    return PR_SUCCESS;
}
