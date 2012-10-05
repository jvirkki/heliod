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

#ifdef XP_UNIX

#ifndef _WatchdogClient_h_
#define _WatchdogClient_h_

#include "nspr.h"                              // NSPR declarations
#include "base/wdservermessage.h"              // wdServerMessage class

/**
 * This class encapsulates the client-side communication of the watchdog
 * process that runs on Unix platforms.
 *
 * @author  $Author: elving $
 * @version $Revision: 1.1.2.13.8.1.46.2 $ $Date: 2005/07/30 08:18:20 $
 * @since   iWS5.0
 */


class WatchdogClient
{
    public:
        /**
         * Creates a listen socket binding it to the address specified in
         * the configuration. The watchdog process creates the listen sockets
         * on behalf of this method and passes the file descriptors to this
         * process.
         *
         * @returns      <code>PR_SUCCESS</code> if the initialization/creation 
         *               of the socket was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        static PRStatus init(void);

        /**
         * Closes the watchdog connection.
         *
         * @returns <code>PR_SUCCESS</code> if the close
         *          of the socket was successful. <code>PR_FAILURE</code> 
         *          if there was an error.
         */
        static PRStatus close(void);

        /**
         * Reconnects to the watchdog (in a child process).
         *
         * @returns <code>PR_SUCCESS</code> if the reconnect was successful.
         *          <code>PR_FAILURE</code> if there was an error.
         */
        static PRStatus reconnect(void);

        /**
         * Asks the watchdog to create a listen socket on the specified IP
         * and port and return the file descriptor for the newly created
         * socket.
         *
         * @param lsName The ID of the listen socket in the configuration
         * @param ip     The address to which the listen socket should bind to
         *               or <code>NULL</code> if an <code>INADDR_ANY</code>
         *               socket is to be created
         * @param port   The port on which the newly created socket must listen
         *               for requests on.
         * @param family The address family, e.g. PR_AF_INET
         * @param qsize  The listen backlog
         * @param sendbuffersize   The value for socket option SO_SNDBUF
         * @param recvbuffersize   The value for socket option SO_RCVBUF
         * @returns      A valid file descriptor corresponding to the listen
         *               socket. A negative value indicates that there was
         *               an error.
         */
        static PRInt32 getLS(const char* lsName, const char* ip,
                             const PRUint16 port, const PRUint16 family,
                             const PRUint32 qsize,
                             const PRUint32 sendbuffersize,
                             const PRUint32 recvbuffersize);

        /**
         * Asks the watchdog to close the listen socket on the specified IP
         * and port.
         *
         * @param lsName The ID of the listen socket in the configuration
         * @param ip     The address to which the listen socket was bound to
         *               or <code>NULL</code> if an <code>INADDR_ANY</code>
         *               socket was created
         * @param port   The port on which the socket was listening for
         *               requests on.
         * @param family The address family, e.g. PR_AF_INET
         * @param qsize  The listen backlog
         * @param sendbuffersize   The value for socket option SO_SNDBUF
         * @param recvbuffersize   The value for socket option SO_RCVBUF
         * @returns      <code>PR_SUCCESS</code> if the socket could be
         *               closed. <code>PR_FAILURE</code> indicates that there
         *               was an error.
         */
        static PRStatus closeLS(const char* lsName, const char* ip,
                             const PRUint16 port, const PRUint16 family,
                             const PRUint32 qsize,
                             const PRUint32 sendbuffersize,
                             const PRUint32 recvbuffersize);

        /**
         * Sends a message to the watchdog indicating that the server process
         * has completed its initialization.
         */
        static PRStatus sendEndInit(PRInt32 numprocs);

        /**
         * Requests a SSL module password from the watchdog.
         *
         * @param prompt   The prompt to use when asking for the password. Also
         *                 serves as an index in case the same password must be
         *                 retrieved multiple times (multiprocess mode/restart).
         * @param serial   Serial number of the password - the watchdog will 
         *                 reprompt if serial is greater than the one from the 
         *                 last one it stored
         * @param password Pointer to a string containing the password.
         * @returns        <code>PR_SUCCESS</code> if the password was received.
         *                 <code>PR_FAILURE</code> indicates that there was an 
         *                 error.
         */
        static PRStatus getPassword(const char *prompt, const PRInt32 serial,
                                    char **password);

        /**
         * Tell the watchdog that the server is finished with its business.
         *
         * @returns <code>PR_SUCCESS</code> if that watchdog acknowledged the
         *          terminate message. <code>PR_FAILURE</code> indicates that 
         *          there was an error.
         */
        static PRStatus sendTerminate(void);

        /**
         * Returns whether or not the watchdog process is running.
         */
        static PRBool isWDRunning(void);

        /**
         * Sends status from the reconfigure process to admin server via 
         * watchdog
         */
        static PRStatus sendReconfigureStatus(const char *statusmsg);

        /**
         * Indicates server has finished reconfigure process to watchdog
         */
        static PRStatus sendReconfigureStatusDone();

        /**
         * Receive a message from the watchdog and return its message
         * type.
         */
        static WDMessages getAdminMessage(void);

#ifdef DEBUG
        /**
         * Prints the contents of this object to cout.
         */
        static void printInfo(void);
#endif

        /**
         * Return the native file descriptor of the channel
         */
        static int getFD(void);

    private:

        /**
         * The communication channel to the watchdog process over which
         * commands are sent and responses received.
         */
        static wdServerMessage* wdMsg_;

        /**
         * Indicates whether the watchdog process is running or not.
         */
        static PRBool bIsWDRunning_;

        /**
         * Indicates whether the watchdog process is running or not.
         */
        static PRInt32 wdPID_;

        /**
         * Connects to the watchdog process via the Unix domain socket.
         *
         * This method assumes that the watchdog process is listening on
         * the Unix socket specified by <code>WDSOCKETNAME</code>.
         *
         * @returns <code>PR_SUCCESS</code> if the creation was successful.
         *          <code>PR_FAILURE</code> if an error occurred while
         *          connecting to the watchdog.
         */
        static PRStatus connectToWD(const PRInt32 pid);

	// Closes the specified connection to the watchdog

	static PRStatus closeWDConnection(wdServerMessage* wdConnection);

};

inline
PRBool
WatchdogClient::isWDRunning(void)
{
    return WatchdogClient::bIsWDRunning_;
}

#endif /* _WatchdogClient_h_ */

#endif
