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

#ifndef _ChildAdminThread_h_
#define _ChildAdminThread_h_

#include "nspr.h"                              // NSPR declarations
#include "NsprWrap/Thread.h"                   // Thread class declarations
#include "base/wdservermessage.h"              // wdServerMessage class


/**
 * Admin message handler thread in each of the child processes.
 *
 * Establishes a connection on the primordial process's Admin
 * channel (<code>WebServer::ADMIN_CHANNEL</code>) and uses this
 * channel to inform the primordial process when the child process
 * completes initialization. The primordial process notifies the 
 * child process to Reconfigure via a message on this channel.
 *
 * @author  $Author: bk139067 $
 * @version $Revision: 1.1.2.2.10.1.34.2 $
 * @since   iWS5.0
 */


class ChildAdminThread : public Thread
{
    public:

        /**
         * Constructor
         */
        ChildAdminThread(void);

        /**
         * Terminates the thread
         */
        ~ChildAdminThread(void);

        /**
         * Establishes a connection with the admin channel created
         * by the primordial (parent) process.
         */
        PRStatus init(void);

        /**
         * Waits for Admin commands to come over the admin channel from
         * the primordial process.
         *
         * Currently, the only Admin command that this thread can handle
         * is the <code>wdmsgReconfigure</code> command.
         */
        void run(void);

        /**
         * Broadcast a reconfiguration request via the parent to the admin
         * threads of all its children.
         */
        static void reconfigure(void);

        /**
         * Broadcast a request to reopen log files via the parent to the
         * admin threads of all its children.
         */
        static void reopenLogs(void);

        /**
         * Broadcast a restart request via the parent to the admin threads
         * of all its children.
         */
        static void restart(void);

    private:

        /**
         * The communication channel to the parent process over which
         * admin messages are sent and responses received.
         */
        wdServerMessage* msgChannel_;

        /**
         * Pollable event used to wake the thread.
         */
        PRFileDesc* wakeupFD_;

        /**
         * Flag that indicates another thread has requested we reload the
         * server configuration.
         */
        PRBool fReconfigure_;

        /**
         * Sends a message to the parent indicating that the server process
         * has completed its initialization.
         *
         * Sends its PID as the parameter to the end-init message.
         */
        PRStatus sendEndInit(void);

        /**
         * process the stats request messages.
         */
        void processStatsMessage();

};

#endif /* _ChildAdminThread_h_ */
