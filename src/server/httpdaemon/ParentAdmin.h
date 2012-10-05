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

#ifndef _ParentAdmin_h_
#define _ParentAdmin_h_

#ifdef XP_UNIX

#include <limits.h>                     // PATH_MAX
#include "nspr.h"

// Pre declaration of class.
class StatsServerMessage;
#define PARENT_ADMIN_RETRY_COUNT 30

/**
 * Admin message handler in the primordial process.
 *
 * Creates a Unix domain server socket that the child processes connect to in
 * order to inform the primordial process that they are done with their 
 * initialization.  When the parent process receives a Reconfigure message, it
 * uses these connections to notify the child processes to Reconfigure.
 * Converts int file descriptors to PRFileDesc* for use in 
 * <code>PR_Poll</code>.
 *
 * @author  $Author: bk139067 $
 * @version $Revision: 1.1.2.1.4.1.2.1.34.7 $
 * @since   iWS5.0
 */
class ParentAdmin
{
    public:
        /**
         * Creates the arrays used to poll the active descriptors etc.
         */
        ParentAdmin(const PRInt32 nChildren);

        /**
         * Removes the admin channel.
         */
        virtual ~ParentAdmin(void);

        /**
         * Creates the admin channel.
         */
        PRStatus init(void);

        /**
         * Do the late initialization ( after all the childs are forked)
         */
        virtual void initLate(void);

        /**
         * Unlinks the admin channel Unix domain socket.
         */
        void unlinkAdminChannel(void);

        /**
         * process the death of child with process id pid.
         */
        virtual void processChildDeath(int pid);

        /**
         * process the death of child with process id pid.
         */
        virtual void processChildStart(int pid);

        /**
         * process closing file descriptior
         */
        virtual void processCloseFd(PRFileDesc* fd);

        /**
         * process the start of reconfigure.
         */
        virtual void processReconfigure();

        /**
         * Polls all the active descriptors waiting for incoming requests.
         *
         * Takes appropriate action depending on the originator of the
         * message (i.e watchdog or child process)
         */
        void poll(PRIntervalTime timeout);

        /**
         * Instruct all child processes to terminate.
         */
        PRStatus terminateChildren(void);

        /**
         * Returns an admin channel connection that can be used from a
         * child process.
         */
        static wdServerMessage* connect(void);

        /** 
         * Returns an stats admin channel connection that can be used from
         * inside or outside of child processes e.g outside of the instance.
         * Unlike connect, it needs to pass the channel name as an argument. If
         * channelName is null then it connects to the current instance. It
         * should only be used for stats communication purposes.
         */
        static StatsServerMessage* connectStatsChannel(
                                        const char* channelName,
                                        int retries = PARENT_ADMIN_RETRY_COUNT);
        /**
         * Indicates whether or not a child process has signalled that
         * it has successfully initialized itself. This is used by
         * the parent process signal handler that handles child deaths,
         * to determine whether to restart them or not.
         */
        static PRBool isChildInitDone(void);

        /**
         * Sets the connection path name from connectionPathDir into outputPath
         * and returns the number of bytes written.
         */
        static int buildConnectionPath(const char* connectionPathDir,
                                       char* outputPath,
                                       int sizeOutputPath);

        /**
         * Returns the number of Children. It must be equal to MaxProcs setting.
         */
        static int getChildrenCount(void);
 

    private:

        /**
         * The socket descriptor of the Unix server socket
         */
        PRInt32 admHandle_;

        /**
         * <code>PRFileDesc*</code> equivalent of <code>admHandle_</code>
         * (for use in <code>PR_Poll()</code>).
         */
        PRFileDesc* admFD_;

        /**
         * <code>PRFileDesc*</code> equivalent of the handle corresponding
         * to the socket connection to the Watchdog (for use in 
         * <code>PR_Poll()</code>).
         */
        PRFileDesc* wdFD_;

        /**
         * Array used for <code>PR_Poll</code>
         */
        PRPollDesc* pollArray_;

        /**
         * Array whose elements indicate whether messages should be sent
         * to a given <code>pollArray_</code> index.
         */
        PRBool* pollIndexWritable_;

        /** Array whose elements indicate whether whether connection is from a
         * child to a given <code>pollArray_</code> index. The other end of the
         * connection is handled by ChildAdminThread  It is the only connection
         * at which child sends a wdmsgEndInit message.
         */
        PRBool* pollIndexStatsChannel_;

        /**
         * Size (maximum number of items) of <code>pollArray_</code> and
         * <code>pollIndexWritable_</code>.
         */
        PRInt32 maxPollItems_;

        /**
         * Number of descriptors to be polled
         */
        PRInt32 nPollItems_;

        /**
         * Number of child processes (sent as a parameter of the 
         * wdmsgEndInit message)
         *
         * This can be used to determine when to the send the EndInit
         * message to the Watchdog. Currently, the message is sent to the
         * Watchdog as soon as the any one child finishes initialization.
         * This counter can be used to send the EndInit message to the
         * watchdog only after ALL child processes have finished 
         * initialization.
         */
        static PRInt32 nChildren_;

        /**
         * Indicates whether the primordial process has received an
         * End-Init message from its children
         */
        static PRInt32 nChildInitDone_;

        /**
         * The pathname of the Unix domain socket that child processes
         * use for sending admin message.
         */
        static char channelName_[PATH_MAX];

        /**
         * Creates a Unix domain server socket
         */
        PRStatus createAdminChannel(void);

        /**
         * Compacts the poll array by removing all entries that are marked
         * invalid.
         */
        PRStatus removeInvalidItems(void);

        /**
         * Determines whether the incoming message is from the Watchdog
         * or a child process and processes it accordingly.
         */
        void processIncomingMessage(int i);

        /**
         * Accepts a connection request (from a child process) and 
         * adds the descriptor to the poll array.
         */
        void acceptChildConnection(void);

        /**
         * Reads an incoming message from the Watchdog and passes the
         * message down to the child processes.
         *
         * Currently, the only messages that come from the Watchdog are
         * <code>wdmsgReconfigure</code> and <code>wdmsgTerminate</code>.
         */
        void processWatchdogMessage(int i);

        /**
         * Reads an incoming message from a child process and processes
         * it. Child processes inform the primordial process that they are
         * done with initialization and the primordial process notifies
         * the watchdog so that the watchdog can release the terminal.
         *
         * Once a child process sends <code>wdmsgEndInit</code> on a
         * connection, we use that connection for relaying messages from
         * its siblings and/or the watchdog.
         */
        void processChildMessage(int i);

        /**
         * Send a message to all initialized child processes.
         */
        void sendMessageToChildren(WDMessages msgType);

        /**
         * Returns a file descriptor to the connection specified by channelName
         * (Unix domain socket).
         */
        static int connectInternal(const char* channelName, int retries);

      protected:
  
        /**
        * Adds the specified descriptor to the poll array
        */
        PRStatus addPollItem(PRFileDesc* fd);

        /**
        * Marks the specified descriptor as inactive (in the poll array)
        * so that it can be removed.
        */
        PRStatus invalidatePollItem(int i);

        /**
        * Removes the descriptor on which the error occurred from
        * the poll array
        */
        void processError(int i);

        /**
         * When child initialization is done, it sends a message.
         * handleChildInit gives the derive classes to do it's post
         * child init initialization
         */
        virtual void handleChildInit(PRFileDesc* fd);

        /**
         * process Stats messages. The request should not arrive for this
         * class. It should arrive only if Stats are enabled. In that case
         * derive class e.g. ParentStats will handle the request.
         */
        virtual void processStatsMessage(int i);

        /**
         * Returns the FileDesc for index i
         */
        PRFileDesc* getPollFileDesc(int i ) const;

        /**
         * Does the reverse of getPollFileDesc. It returns the index
         * of Poll file descriptor which matches fd.
         */
        PRInt32 findFileDescIndex(PRFileDesc* fd);
  
};

inline
int 
ParentAdmin::getChildrenCount(void)
{
  return nChildren_;
}
  
inline
PRFileDesc*
ParentAdmin::getPollFileDesc(int i) const
{
    return ( pollArray_ ? pollArray_[i].fd : NULL);
}

#endif

#endif /* _ParentAdmin_h_ */
