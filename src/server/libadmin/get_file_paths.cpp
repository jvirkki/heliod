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
	This is a file that contains various functions to get various paths on the 
	file system. These functions should be looked upon as a way to not hardcode 
	the names of various folders elsewhere in the code. e.g.
	getting the server.xml path for an instance with id server1 should be as 
	easy as getServerXmlPath(String instanceid)
	rather than sprintf(xmlPath, "%s/%s/%s", id, "config", "server.xml").
	This way of getting the paths will be used throughout the admin cgi scripts 
    and elsewhere.
	Note that all the functions are based on the instance id and location where 
	the configuration of all instances is stored. 
	The Paths returned are always absolute paths. If the path pertains to a 
    directory,
	the trailing path separator character is NOT appended.
	All blames to Kedar Mhaswade, Muralidhar Vempaty.
	03/27/2002
*/

/* include files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include "get_file_paths.h"

/* variables with scope of this file - will be used in many functions only in this file */
static const char  	FILE_SEP_CHAR		='/';
static const char* 	FILE_SEP_STRING		= "/";


/*
 * A function to initialize the config root for an instance. 
 * Note that his function is a provision for that. 
 * The static variable instanceConfigurationRoot is set to the
 * value of an instance's config Root. (The parent directory where
 * the instance directory is created).
 * It depends upon the env var ADMSERV_ROOT invariably which
 * is set by the startup code.
 */

static char *instanceConfigurationRoot = NULL;

char* getInstanceCfgRoot() {

	 char* admCfgRoot = getenv("ADMSERV_ROOT");
	 char buffer[PATH_MAX];
     char* loc;
	 char* tmpStr;

	 if (instanceConfigurationRoot != NULL)
		 return ( instanceConfigurationRoot );
	 makePathAbsolute(admCfgRoot, buffer);
     instanceConfigurationRoot = strdup(buffer); //initialize once
     
     /* parse the ADMSERV_ROOT to remove the admin-server/config substring */
	 /* first make it uniform */
	 tmpStr = instanceConfigurationRoot;

	 while (*tmpStr) {
		 if (*tmpStr =='\\') {
			 *tmpStr = '/';
		 }
		 tmpStr++;
	 }
	 
	 /* get rid of "config" and "admin-server" */

     if(loc = strrchr(instanceConfigurationRoot, '/'))
        *loc = '\0';

     if(loc = strrchr(instanceConfigurationRoot, '/'))
        *loc = '\0';

	 return ( instanceConfigurationRoot );
}

/* 
 * returns the absolute path of the server.xml depending 
 * upon value of backup. backup=1 gives the path for 
 * backup file. 
 */

int getServerXmlPath(char* instanceName, int backup, char* buffer, int length) {
	int 	status 		= GENERAL_FAILURE;
	if (buffer != NULL) {
		char *instanceCfgRoot = getInstanceCfgRoot();
		int requiredLength;
		int configLength = strlen(instanceCfgRoot) +
				strlen(FILE_SEP_STRING) + strlen(instanceName) + 1;
		if (backup) { /* get the backup(conf_bk) server.xml path */
			requiredLength = configLength + strlen(FILE_SEP_STRING) + 
				strlen(S1WS_BACKUP_CONFIG_DIR_NAME);
		}
		else { /* get the config server.xml path */
			requiredLength = configLength + strlen(FILE_SEP_STRING) + 
				strlen(S1WS_CONFIG_DIR_NAME);
		}
		requiredLength = requiredLength + strlen(FILE_SEP_STRING) +
			strlen(S1WS_SERVER_XML_FILE_NAME);
		if (requiredLength > length) {
			status = INSUFFICIENT_BUFFER_ERROR;
		}
		else {
			if (backup) {
				sprintf(buffer, "%s%c%s%c%s%c%s",
					instanceCfgRoot, FILE_SEP_CHAR,
					instanceName, FILE_SEP_CHAR,
					S1WS_BACKUP_CONFIG_DIR_NAME, FILE_SEP_CHAR,
					S1WS_SERVER_XML_FILE_NAME);
			}
			else {
				sprintf(buffer, "%s%c%s%c%s%c%s",
					instanceCfgRoot, FILE_SEP_CHAR,
					instanceName, FILE_SEP_CHAR,
					S1WS_CONFIG_DIR_NAME, FILE_SEP_CHAR,
					S1WS_SERVER_XML_FILE_NAME);
			}
			status = S1WS_SUCCESS;

		}
	}
	
	return ( status );
}

/*
 * returns the absolute path of the config directory depending 
 * upon value of backup. backup=1 gives the path for config/backup 
 * directory. 
 */

int getServerConfigPath(char* instanceName, int backup, char* buffer, int length) {
	int 	status 		= GENERAL_FAILURE;
	if (buffer != NULL) {
		char *instanceCfgRoot = getInstanceCfgRoot();
		int requiredLength;
		int configLength = strlen(instanceCfgRoot) +
				strlen(FILE_SEP_STRING) + strlen(instanceName) + 1;
		if (backup) { /* get the backup(conf_bk) dir path */
			requiredLength = configLength + strlen(FILE_SEP_STRING) + 
				strlen(S1WS_BACKUP_CONFIG_DIR_NAME);
		}
		else { /* get the config dir path */
			requiredLength = configLength + strlen(FILE_SEP_STRING) + 
				strlen(S1WS_CONFIG_DIR_NAME);
		}
		if (requiredLength > length) {
			status = INSUFFICIENT_BUFFER_ERROR;
		}
		else {
			if (backup) {
				sprintf(buffer, "%s%c%s%c%s",
					instanceCfgRoot, FILE_SEP_CHAR,
					instanceName, FILE_SEP_CHAR,
					S1WS_BACKUP_CONFIG_DIR_NAME);
					
			}
			else {
				sprintf(buffer, "%s%c%s%c%s",
					instanceCfgRoot, FILE_SEP_CHAR,
					instanceName, FILE_SEP_CHAR,
					S1WS_CONFIG_DIR_NAME);
			}
			status = S1WS_SUCCESS;
		}
	}
	
	return ( status );
}

/* returns the absolute path of the log directory */

int getServerLogDirPath(char* instanceName, char* buffer, int length) {
	int 	status 		= GENERAL_FAILURE;
	if (buffer != NULL) {
		char *instanceCfgRoot = getInstanceCfgRoot();
		int configLength = strlen(instanceCfgRoot) +
				strlen(FILE_SEP_STRING) + strlen(instanceName)
				+ strlen(FILE_SEP_STRING) +
				strlen(S1WS_DEFAULT_LOGS_DIR_NAME) + 1;
		
		if (configLength > length) {
			status = INSUFFICIENT_BUFFER_ERROR;
		}
		else {
			sprintf(buffer, "%s%c%s%c%s",
			instanceCfgRoot, FILE_SEP_CHAR,
			instanceName, FILE_SEP_CHAR,
			S1WS_DEFAULT_LOGS_DIR_NAME);
			status = S1WS_SUCCESS;
		}
	}
	return ( status );
}

/* Returns a 0 on success and 1 otherwise.
   Makes the passed path absolute. This routine should be
   able to change the present directory to the given path. Caller's
   current working directory is NOT changed.
   Arguments path and absPath should not be null.
*/
int makePathAbsolute(char *path, char *absPath)
{
	int resultOk		= 0;
	int resultFailed	= 1;
	int result			= resultFailed;
	char currentPath[PATH_MAX];
	int chDirStat;

#ifdef XP_WIN32
#define getcwd _getcwd
#define chdir _chdir
#endif

	if (path && absPath)
	{
		getcwd(currentPath, PATH_MAX); /*should never return NULL/failure */
		chDirStat = chdir(path);
		if (!chDirStat)
		{
			getcwd(absPath, PATH_MAX);
			chdir(currentPath);			/* this should never fail */
			result = resultOk;
		}
	}
	return result;
}
