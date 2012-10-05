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
** Watchdog Listen Socket Manager
**
** This module manages the list of sockets which the server
** listens on for requests.
**
**/

#ifndef _WDLSMANAGER_
#define _WDLSMANAGER_

#include <assert.h>
#include <definesEnterprise.h>
#ifdef Linux
/* Needed for extra defines */
#include <sys/poll.h>
#else
#include <poll.h>
#endif
#include "base/wdservermessage.h"

#ifdef  FEAT_NOLIMITS
#define INITIAL_LS_SIZE 200
#else
/* This is a hard limit for FastTrack that won't be exceeded in the code */
#define INITIAL_LS_SIZE 5
#endif

#define INITIAL_PA_SIZE 200

typedef struct _wdLS_entry {
        char *  ls_name;
        char *  ipaddress;
        int     port;
        int     family;
        int     fd;
        int     listen_queue_size;
        int     send_buff_size;
        int     recv_buff_size;
} wdLS_entry;

typedef struct _msg_info {
        wdServerMessage * wdSM;
        int     _waiting;       /* message is waiting to be read                */
} msg_info;

class wdLSmanager {
  public:
        wdLSmanager();
        ~wdLSmanager();
        char *  InitializeLSmanager     (char * UDS_Name);
        int     getNewLS                (char * new_ls_name, char * new_IP,
                                         int new_port, int family, 
                                         int ls_Qsize, int SendBSize, 
                                         int RecvBSize);
        int     removeLS                (char * new_ls_name, char * new_IP,
                                         int new_port, int family, 
                                         int ls_Qsize, int SendBSize, 
                                         int RecvBSize);
        wdLS_entry * ls_table;          /* Table of Listen Sockets      */

        int     Reset_Poll_Entry        (int index);
        int     Add_Poll_Entry          (int socket);
        int     Wait_for_Message        ();
        struct pollfd * pa_table;       /* Table of Poll fds            */
        int *   _heard_restart;         /* heard restart on this fd     */
        msg_info * msg_table;           /* Table of active messages     */

        void    unbind_all              (void);
        int     get_table_size          (void);

  private:
        void    Initialize_new_ls_table (int table_start, int table_size);
        int     lookupLS                (char * new_ls_name, char * new_IP,
                                         int new_port, int new_family,
                                         int ls_Qsize, int SendBSize, 
                                         int RecvBSize);
        int     addLS                   (char * ls_name, char * ip, 
                                         int port, int family, int new_fd, 
                                         int ls_Qsize, int SendBSize, 
                                         int RecvBSize);
        int     create_new_LS           (char * UDS_Name, char * new_IP, 
                                         int new_port, int family, 
                                         int ls_Qsize, int SendBSize, 
                                         int RecvBSize);

        int     ls_count;               /* Number of entries entered    */
        int     ls_table_size;          /* Number of entries allocated  */
        int     msg_listener_fd;        /* socket for talking to server */

        int     pa_count;               /* Number of entries used       */
        int     pa_table_size;          /* Number of entries allocated  */

        /* Default values for entry fields */
        char *  default_ipaddress;
        int     default_port;
};

#endif /*       _WDLSMANAGER_   */
