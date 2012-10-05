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
 * This is the extension to get the platform specific stats for Linux.
 * On Linux, the stats are stored on the proc filesystem (/proc)
 * These are normal files that are read and then the parameters got from the files.
 * We need to parse the file into tokens which is handled but the FileUtility.
 *
 * The files that are used to gather the statistics are 'defined' after the #includes.
 */

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/sysinfo.h> // cpu_stat_t
#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*

#include "httpdaemon/linuxkernelstats.h"
#include "httpdaemon/fileutility.h"

#define	 CPUFILE	"/proc/cpuinfo"
#define	 UPTIMEFILE	"/proc/uptime"
#define  LOADFILE	"/proc/loadavg"
#define  NETWORKFILE	"/proc/net/dev"
#define	 CPUSTATSFILE	"/proc/stat"

#define  COMPAREWRD	"processor"

/*
 * Initialize the member variables
 */
LinuxKernelStats::LinuxKernelStats(void)
{
    oldrbytes = 0;
    oldobytes = 0;
    /*oldhrtime = 0;*/
    noofCpus = 1;
}

// Destructor
LinuxKernelStats::~LinuxKernelStats(void)
{
/* nothing to do die a graceful death */
}



//-----------------------------------------------------------------------------
// LinuxKernelStats::init
// store the number of CPUs: open the /proc/cpuinfo file and find the last processor number.
// Increment by one to get the number of system processors.
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::init()
{
    FileUtility *file=new FileUtility(CPUFILE);
    
    if ( !file->IsEof() )
    {
    	do
	{
		file->ReadLine();
		if (( !file->IsInvalid() ) && (file->StartsWith("processor")))
		{
			noofCpus = new_atoi( file->GetToken(2) ) + 1;
		}
	} while ( !file->IsEof() );
    }
    else
    {
    	return PR_FAILURE;
    }
    
    delete file;
    
    return PR_SUCCESS;
}

int LinuxKernelStats::getNumberOfCpus()
{
    return noofCpus;
}

//-----------------------------------------------------------------------------
// LinuxKernelStats::destroy
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::destroy()
{
    // we generate no error reports
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxKernelStats::update
// update the uptime 
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::update()
{
    
    FileUtility *ufile=new FileUtility(UPTIMEFILE);
    
    if ( !ufile->IsEof() )
    {
	ufile->ReadLine();
	if ( !ufile->IsInvalid() )
	{
		uptime = new_atol(ufile->GetToken(1));
	}
    }
    else
    {
    	return PR_FAILURE;
    }
    delete ufile;
    
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxKernelStats::getLoadAverages
// open the /proc/loadavg file the first three entried contains the load averages
// for 1, 5 and 15 mins respectively
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::getLoadAverages(LoadAveragesInfo *info)
{
    FileUtility *file=new FileUtility(LOADFILE);
    
    if ( !file->IsEof() )
    {
	file->ReadLine();
	if ( !file->IsInvalid() )
	{
		info->load_avg_for_1min = strtod(file->GetToken(1),NULL);
		info->load_avg_for_5min = strtod(file->GetToken(2),NULL);
		info->load_avg_for_15min = strtod(file->GetToken(3),NULL);
	}
    }
    else
    {
    	return PR_FAILURE;
    }
    
    delete file;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxKernelStats::getNetworkThroughPut
// open /proc/dev/net file. ignore the first two lines which are formattign lines
// the next lines contains the network device information in the format
// device: <received informartion> <sent information>
// we need to ignore the localloopback device 'lo' and the other 'dummy' devices
// a summation of the rest would would us the bytes transterred across the network interfaces
//
// (bytes transferred / uptime) = rate of transfer / second
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::getNetworkThroughPut(NetworkStatsInfo *info)
{   
    long long BytesReceived=0;
    long long BytesSent=0;
    
    FILE *f;
    
    FileUtility *file=new FileUtility(NETWORKFILE); 
    file->ReadLine();
    
    if ( !file->IsEof() )
    {
    	do
	{
		if (( !file->IsInvalid() ) && ( file->GetLineNum() > 2 ))
		{
			if (( strncmp("lo",file->GetToken(1),2) != 0 )&& \
			( strncmp("dummy",file->GetToken(1),5) != 0 ))
			{
				BytesReceived += new_atol(file->GetToken(2));
				BytesSent += new_atol(file->GetToken(10));
			}
		}
		file->ReadLine();
	} while ( !file->IsEof() );
	
	update();
    }
    else
    {
    	return PR_FAILURE;
    }
    
    delete file;
    
    // If uptime is non-Zero then only do the math, otherwise reset data
    //
    if(0 == uptime) {
        info->in_bytes_per_sec = 0;
        info->out_bytes_per_sec = 0;
    }
    else {
        info->in_bytes_per_sec = BytesReceived / uptime;
        info->out_bytes_per_sec = BytesSent / uptime;
    }
    
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxKernelStats::getProcessorInfo
// open the /proc/stat file. All cpu stats being with 'cpu'.
// summation of all the processor stats for the CPUs
// the times are in jiffies (1/100 of second) and per-processor
// we don't need convert that to seconds and take the percentage as is
//-----------------------------------------------------------------------------

PRStatus LinuxKernelStats::getProcessorInfo(ProcessorInfo *info)
{
    //--------------------------------------------------------------------------------
    // we don't need to worry about making sure numSPUs is set on linux
    // as the statmanager::initEarly() calls us for information on linux
    // unlike SOLARIS where a system call is made
    //--------------------------------------------------------------------------------
    
    FileUtility *file=new FileUtility(CPUSTATSFILE);
    
    double usermode,sysmode,idle = 0;
    double totaljiffies;
    int numcpu=getNumberOfCpus();
    int count=0;
    
    if ( !file->IsEof() )
    {
	file->ReadLine();
    	do
	{
		file->ReadLine();		// file line number > 1
		
		if (( !file->IsInvalid() ) && (file->StartsWith("cpu")) && \
			 ( count < numcpu ))
		{
			// all values are in jiffies (100th of a second)
			usermode	= ( atof(file->GetToken(2)) + atof(file->GetToken(3)));
			sysmode 	= atof(file->GetToken(4));
			idle	 	= atof(file->GetToken(5));
			totaljiffies	= (usermode + sysmode + idle);
			
			info[count].percent_user_time	= (usermode / totaljiffies) * 100;
			info[count].percent_kernel_time	= (sysmode / totaljiffies) * 100;
			info[count].percent_idle_time	= (idle / totaljiffies) * 100;
			count++;
		}
	} while ( !file->IsEof() );
    }
    else
    {
    	return PR_FAILURE;
    }
    
    delete file;
    
    return PR_SUCCESS;
}

int LinuxKernelStats::new_atoi(const char *nptr)
{
  if(nptr) return atoi(nptr);
  else return 0;
}

long LinuxKernelStats::new_atol(const char *nptr)
{
  if(nptr) return atol(nptr);
  else return 0;
}
