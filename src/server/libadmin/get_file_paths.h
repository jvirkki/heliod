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

/* some defines that are used throughout get_file_paths.c file */
#ifndef _LIBADMIN_GET_FILE_PATHS_
#define _LIBADMIN_GET_FILE_PATHS_
/* include the other required include files*/
#include "libadmin/libadmin.h"

#ifndef S1WS_SUCCESS
#define S1WS_SUCCESS 0
#endif

#define GENERAL_FAILURE 7

#ifndef S1WS_GENERAL_FAILURE
#define S1WS_GENERAL_FAILURE 1
#endif
#define INSUFFICIENT_BUFFER_ERROR 2

/* defining these as preprocessor directives for now as all of them are str */
#define S1WS_CONFIG_DIR_NAME					    "config"
#define S1WS_BACKUP_CONFIG_DIR_NAME				"conf_bk"
#define S1WS_DEFAULT_LOGS_DIR_NAME				"logs"	/*can be overridden */

#define S1WS_SERVER_XML_FILE_NAME				"server.xml"
#define S1WS_CRON_ERROR_FILE_NAME				"scheduler.error"
#define S1WS_CRON_LOG_FILE_NAME					"scheduler.log"
#define S1WS_CRON_DUMP_FILE_NAME					"scheduler.dump"
#define S1WS_CRON_PID_FILE_NAME					"scheduler.pid"

/*
 * ********* A General Rule Followed **********
 * All the functions take a preallocated buffer and length of that buffer. If
 * the length of the buffer is rather too small to accomodate the path
 * it calculates, then an error code is returned, 0 indicating 
 * success. If successful, the calculated path is put into the passed buffer.
 * ********* A General Rule Followed **********
*/
/* declaration of functions that are defined in get_file_paths.c. 
 * Note that the functions ALWAYS return the path with forward slashes,
 * as that is not a problem on either Unix or Windows
*/

/* returns the path to the directory containing all the instances */
char* getInstanceCfgRoot();

/* returns the instance directory absolute path */
int getInstanceHomePath(char* instanceName, char* buffer, int length);

/* returns the absolute path of the server.xml depending upon value of
 * backup */
int getServerXmlPath(char* instanceName, int backup, char* buffer, int length);

/* returns the absolute path of the config directory for the server depending on
 * the value of backup*/
int getServerConfigPath(char* instanceName, int backup, char* buffer, int length);

/* returns the absolute path of the logs directory for the server */ 
int getServerLogDirPath(char* instanceName, char* buffer, int length);

/* makes the given path absolute and returns it */

int makePathAbsolute(char *path, char *absPath);

#endif  /* _LIBADMIN_GET_FILE_PATHS_ */
