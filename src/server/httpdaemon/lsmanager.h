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
**
** Listen Socket Manager
**
** This module manages the list of sockets which the server
** listens on for requests.
**
**/

#ifndef _LSMANAGER_
#define _LSMANAGER_

#include <assert.h>
#include <definesEnterprise.h>

#define DEFAULT_WDOG_BASE_FD	260

#ifdef  FEAT_NOLIMITS
#define INITIAL_LS_SIZE 42
#else
/* This is a hard limit for FastTrack that won't be exceeded in the code */
#define INITIAL_LS_SIZE 5
#endif

/* Maximum length line allowed in LS.conf file */
#define MAX_LS_LINE	1024

/* Default LS name for primary port (the one in magnus.conf) */
#define DEFAULT_PRIMARY_LS_NAME	"<DEFAULT>"

typedef struct _LS_entry {
    char *	ls_name;
    char *	ipaddress;
    int		port;
    int		security;
	int     fd;
    int		numAcceptors;
    int		threadStartIndex; /* where in Acceptor array to find
				the first acceptor for this LS - only 
				used in deamon, not watchdog	*/
} LS_entry;

class LSmanager {
  public:
	LSmanager();
	~LSmanager();
	int	getNumberOfLS();
	char *	InitializeLSmanager(int do_opens, LS_entry * default_server);

	LS_entry * ls_table;	/* Table  of Listen Sockets	*/
	int	total_num_AcceptorThreads;

  private:

	char *	ParseLSconfigfile(char * filepathname);
	int	addLS(char * ls_name, char * ip, int port, 
			int numberAcceptors, int security);
	int	Parse_security(char * str);
	void unbind_all(void);
	
	int	ls_count;	/* Number of Listen Sockets entered 		*/
	int	ls_table_size;	/* Number of Listen Socket entries allocated	*/
	int	max_port_number;
	int	max_allowed_Acceptor_per_LS;

		/* Default values for entry fields */
	char *	default_ipaddress;
	int	default_port;
	int	default_numAcceptors;
	int	default_security;
	
	// PRBool getLSindex();
	// PRBool openLS();
	// PRBool closeLS();
	// LS * getLSarray();
	// LS * getLS(PRInt32 index);
};

#endif /*	_LSMANAGER_	*/
