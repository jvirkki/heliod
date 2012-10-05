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
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <stdlib.h>

#include <base/ereport.h> // for ereport* prototypes
#include "httpdaemon/dbthttpdaemon.h"      // DBT_*

#include "httpdaemon/linuxprocessstats.h"
#include "httpdaemon/fileutility.h"

#define	 MEMFILE	"/proc/meminfo"

// Contstructor
LinuxProcessStats::LinuxProcessStats(void)
{
}

// Destructor
LinuxProcessStats::~LinuxProcessStats(void)
{
}

PRStatus LinuxProcessStats::init()
{
    // We don't want to get the process statistics
    // read comments for LinuxProcessStats::getMemUsageInfo(...) below
    return PR_FAILURE;
    //return initProcFileDesc();
}

//-----------------------------------------------------------------------------
// LinuxProcessStats::destroy
//-----------------------------------------------------------------------------

PRStatus LinuxProcessStats::destroy()
{
    /*close(procFileDesc);*/
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxProcessStats::initProcFileDesc
//-----------------------------------------------------------------------------

PRStatus LinuxProcessStats::initProcFileDesc()
{
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxProcessStats::getMemUsageInfo
// traverse /proc file for each process (linux threads are processes too)
// match if we found the same groupid as ourselves and sum the memory statistics for 
// all the processes that are in the same group. major pain!
//
// need to reimplement for linux kernel 2.6+ with the introduction of NPTL
// and praying they have a better interface
//
// Ok lets see how this solution works (or fails to)!!.
// PROBLEM: all forks(...), threads are processes. To get the meory used by a process, we
// need to sum up the memory taken by the whole process group *except the threads*.
//
// Simple enuff!!.. not quiet. How to determine if the process is a thread or not?
// See IsThread(...) implementation for details.
//
// Yippie!! it werks... err not quiet.. after much pain (loss of hair.. sleep..
// and being eligible for compensatory benefits) figured... the heuristics work fine.. 
// almost perfect except....
//
// That the statistics gatherer uses the /proc filesystem. If a process gathers the
// statistics for itself, the proc filesystem for the current process is also being updated.
//
// The proc filesystem doesn't gather atomicity of the contents being read. so the process 
// gets different data and messes up the heuristics.
//
// Even from a fairly lightweight process it is hard to gather information of the memory useage.
// Hopefully later kernels will provide other means of getting the same.
//
//-----------------------------------------------------------------------------

PRStatus LinuxProcessStats::getMemUsage(MemUsageInfo *info)
{
	DIR *root;
	dirent *dir;
	int pgid = getpgrp();
	FileUtility *file,*parent;
	int pagesize=getpagesize();
	
	long long  processsize = 0;
	long long residentsize = 0;
	long long totalmem = 0;
	
	char procname[30];
	char statfilename[]="/stat";	//actually /proc/<pid>/stat 
	long num;
	
	memset(procname,'\0',30);
	strcpy(procname,"/proc");
	
	root = opendir(procname);
	if (root != NULL)
	{
		while ( dir = readdir(root) )
		{
			if ((num = new_atol(dir->d_name))>1)
			{
				procname[5]='/';
				procname[6]='\0';
				strcat(procname,dir->d_name);
				strcat(procname,statfilename);
				
				file = new FileUtility(procname);
				if ( !file->IsEof() )
				{
					file->ReadLine();
					//printf("%s %s \n", file->GetToken(2),file->GetToken(23));
					
					procname[5]='/';
					procname[6]='\0';
					
					if ((new_atol(file->GetToken(4))>0)&&(new_atol(file->GetToken(5))==pgid))
					{
						strcat(procname,file->GetToken(4));
						strcat(procname,statfilename);
					
						parent = new FileUtility(procname);
						parent->ReadLine();
					
						if ( !IsThread(parent, file) )
						{
							//get the Virtual mem size
							processsize += new_atol( file->GetToken(23) );
						
							//get the resident mem size
							residentsize += new_atol( file->GetToken(24) ) * pagesize;
						}
					
					delete parent;
					}
				}
				
				delete file;
				
			}
		}
		
		closedir(root);
		
		file = new FileUtility(MEMFILE);
				
		if ( !file->IsEof() )
		{
   		 	do
			{
				file->ReadLine();
				if (( !file->IsInvalid() ) && (file->StartsWith("MemTotal")))
				{
					totalmem = new_atol( file->GetToken(2) );
					break;
				}
			} while ( !file->IsEof() );
		}
		
		delete file;
		
		info->process_size_in_Kbytes = processsize / 1024;
		info->process_resident_size_in_Kbytes = residentsize /1024;

		// If totalmem is non-Zero then only do the math, otherwise reset data
		//
		if(0 == totalmem) {
			info->fraction_system_memory_usage = 0;
		}
		else {
			info->fraction_system_memory_usage = processsize / (totalmem * 1024);
		}
	}
	else
	{
		return PR_FAILURE;
	}
	
    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------
// LinuxProcessStats::IsThread
// major complication.. how to distinguish between a process, fork(..) and a thread??
// no exposed interfaces to query hence we use heuristics.
//
// On linux either a fork(..) or pthread_create(...) calls clone(...) with 
// different arguements.
//
// The flags for the child reflect if the process is fork(...)ed or pthread_created(...)ed.
// To distunguish between a fork(..) and a thread we compare the resident size,
// the virtual size and the commandline of the program.
//
// If any of them do not match then it is a fork. If all of then match then it is a thread.
//
// Yes! there could be exceptions to this rule but well there is no other way!!!!!
//-----------------------------------------------------------------------------
int LinuxProcessStats::IsThread(FileUtility *parent , FileUtility *sibling)
{
	// compare the resident memory sizes
	// if match continue
	if ( new_atol(sibling->GetToken(24)) != new_atol(parent->GetToken(24)) )
	{
		return 0;
	}
	
	// compare the virtual memory sizes
	// if match continue
	if ( new_atol(sibling->GetToken(23)) != new_atol(parent->GetToken(23)) )
	{
		return 0;
	}
	
	// compare the command lines of the parent and the child
	// same command lines then continue
	if ( (strcmp( sibling->GetToken(2), parent->GetToken(2) )) != 0 )
	{
		return 0;
	}
	
	
	return 1;
}

int LinuxProcessStats::new_atoi(const char *nptr)
{
  if(nptr) return atoi(nptr);
  else return 0;
}

long LinuxProcessStats::new_atol(const char *nptr)
{
  if(nptr) return atol(nptr); 
  else return 0;
}
