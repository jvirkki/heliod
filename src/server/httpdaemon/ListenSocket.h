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

#ifndef _ListenSocket_h_
#define _ListenSocket_h_

#include "nspr.h"                              // NSPR declarations
#include "NsprWrap/CriticalSection.h"          // CriticalSection class
#include "NsprWrap/Thread.h"                   // Thread class
#include "httpdaemon/ListenSocketConfig.h"     // ListenSocketConfig class

// forward declarations rather than #include to prevent circular inclusion
// of header files
class Acceptor;
class ConnectionQueue;

/**
 * This class represents a server/listen socket that listens for incoming
 * connection requests on a specific port number on the machine.
 *
 * @author  $Author: us14435 $
 * @version $Revision: 1.1.2.14.8.1.32.5.16.2 $ $Date: 2007/11/12 20:44:43 $
 * @since   iWS5.0
 */


class ListenSocket
{
    public:

        /**
         * Default constructor.
         *
         * Since we are not using exceptions, this method does not create
         * the socket. Invoking the <code>create</code> method will create
         * the socket and return a success/failure status.
         */
        ListenSocket(ListenSocketConfig* config = NULL);

        /**
         * Destructor
         *
         * Closes the socket connection.
         */
        ~ListenSocket(void);

        /**
         * Creates a listen socket binding it to the address specified in
         * the configuration.
         *
         * @returns      <code>PR_SUCCESS</code> if the initialization/creation 
         *               of the socket was successful. <code>PR_FAILURE</code> 
         *               if there was an error.
         */
        PRStatus create(void);

        /**
         * Accepts a connection on this listen socket.
         *
         * @param remoteAddress On return from this method, this will contain
         *                      the address of the connecting socket. 
         * @param timeout       Specifies the timeout value for 
         *                      <code>PR_Accept</code>.
         *                      (default = <code>PR_INTERVAL_NO_TIMEOUT</code>)
         * @returns             A pointer to a new <code>PRFileDesc</code>
         *                      structure that represents the newly accepted
         *                      connection. A value of NULL, indicates that
         *                      there was an error. Error information can be
         *                      obtained via <code>PR_GetError</code>.
         */
        PRFileDesc* accept(PRNetAddr& remoteAddress,
                           const PRIntervalTime timeout = 
                               PR_INTERVAL_NO_TIMEOUT);

        /**
         * Sets the terminating flag in each of the <code>Acceptor</code>
         * threads and sends an interrupt to each thread causing the
         * thread to exit its <code>run</code> method.
         *
         * When closing multiple sockets, the performance of the close
         * process can be improved by first invoking this method 
         * (<code>stopAcceptors</code>) on all the <tt>ListenSocket</tt> 
         * objects that are to be closed and then invoking the <tt>close</tt>
         * method on those objects. Invoking this method first, gives
         * the <tt>Acceptor</tt> threads time to determine that they have
         * been terminated.
         */
        PRStatus stopAcceptors(void);

        /**
         * Closes the socket in the current process (and in the watchdog).
         * Waits for all the Acceptor threads (associated with this socket)
         * to terminate.
         *
         * @returns      <code>PR_SUCCESS</code> if the socket is successfully
         *               closed. <code>PR_FAILURE</code> if there was an error.
         */
        PRStatus close(void);

        /**
         * Creates and starts threads that accept connections on this
         * listen socket.
         *
         * The number of <code>Acceptor</code> threads that are created is 
         * determined by the concurrency parameter of this socket's 
         * configuration.
         *
         * @param connQueue The queue on which the <code>Acceptor</code>
         *                  enqueue new connections for servicing by the
         *                  <code>DaemonSession</code> threads or NULL if
         *                  <code>DaemonSession</code> threads should be
         *                  used both to accept and service connections.
         * @returns         <code>PR_SUCCESS</code> if all the Acceptor 
         *                  threads were started successfully.
         * 
         */
        PRStatus startAcceptors(ConnectionQueue *connQueue);

        /**
         * Initializes and creates the listen socket, binding it to the
         * address specified in the configuration.
         *
         * The refcount of the <code>config</code> is incremented and that
         * of the previous <code>ListenSocketConfig</code> (if any) is
         * decremented.
         * 
         * @param config Specifies the configuration/properties that are
         *               associated with this listen socket.
         */
        void setConfig(ListenSocketConfig* config);

        /**
         * Returns the configuration information using which this listen 
         * socket was created.
         *
         * The refcount of the <code>ListenSocketConfig*</code> that is
         * returned is incremented by this method and it is the users
         * responsibility to decrement the refcount by calling 
         * <code>unref()</code> on the returned object.
         */
        ListenSocketConfig* getConfig(void);

        /**
         * Indicates whether the listen socket is listening on an address
         * other than INADDR_ANY.
         *
         * @returns <code>PR_FALSE</code> if the listen socket is listening on
         *          INADDR_ANY. <code>PR_TRUE</code> if the listen socket is
         *          listening on an explicit IP.
         */
        PRBool hasExplicitIP(void) const;

        /**
         * Returns the network address (IP and port) the listen socket is
         * listening on.
         *
         * @returns The network address the listen socket is listening on
         */
        PRNetAddr getAddress(void) const;

        /**
         * Set whether address reuse should be allowed.
         *
         * @param reuse "true" if SO_REUSEADDR should be set, "false" if
         *              SO_REUSEADDR should be cleared, or "exclusive" if
         *              Windows' SO_EXCLUSIVEADDRUSE should be set.
         */
        static void setReuseAddr(const char *reuse);

        /**
         * Logs the specified error message to the error log file.
         * Context information such as the name, port of the listen socket 
         * are also logged.
         *
         * This method assumes that the error log file has already been
         * opened (i.e <code>ereport_init</code> has been invoked).
         *
         * @param msg A textual description of the error
         */
        void logError(const char* msg) const;

        /**
         * Logs the specified message to the error log file using
         * <code>ereport(LOG_INFORM...).</code>  Context information such as 
         * the name, port of the listen socket are also logged.
         *
         * This method assumes that the error log file has already been
         * opened (i.e <code>ereport_init</code> has been invoked).
         *
         * @param msg The text of the message
         */
        void logInfo(const char* msg) const;

#ifdef DEBUG
        /**
         * Prints the contents of this object to cout.
         */
        void printInfo(void) const;
#endif

        /**
         * Represents the open listen socket on which the server is 
         * willing to accept new connections.
         */
        PRFileDesc* fd_;

        /**
         * The IP address and port the socket listens for requests on.
         */
        PRNetAddr address_;

        /**
         * Set if we're listening on something other than INADDR_ANY.
         */
        PRBool bExplicitIP_;

        /**
         * A reference to the configuration information using which this
         * listen object was created.
         *
         * The memory for this pointer is managed by the Server class and
         * is allocated when the XML configuration file is parsed. The
         * memory is released when the "old" Configuration object is
         * discarded.
         */
        ListenSocketConfig* config_;

        /**
         * Lock that allows <code>config_</code> to be updated in a
         * thread-safe manner during reconfiguration.
         */
        CriticalSection configLock_;

        /**
         * An array of pointers to each of the threads running accept
         * on this listen socket.
         */
        Thread** acceptors_;

        /**
         * The size of the <code>acceptors_</code> array.
         *
         * This value is the same as <code>config_->getConcurrency()</code>
         * and is "cached" when the <code>acceptors_</code> array is created.
         */
        int nAcceptors_;

        /**
         * Address of client socket used to wakeup acceptors.
         */
        PRNetAddr wakeupAddress_;

        /**
         * Set when the acceptors are being stopped.
         */
        PRBool bStoppingAcceptors_;

        /**
         * The socket reuse option name, SO_REUSEADDR or SO_EXCLUSIVEADDRUSE.
         */
        static int reuseOptname_;

        /**
         * The socket reuse option value, 0 (don't reuse) or 1 (reuse).
         */
        static int reuseOptval_;

        /**
         * Copy constructor. Objects of this class cannot be
         * copy constructed.
         */
        ListenSocket(const ListenSocket& source);

        /**
         * operator=. Objects of this class cannot be cloned.
         */
        ListenSocket& operator=(const ListenSocket& source);

        /**
         * Creates a new TCP socket
         *
         * @returns <code>PR_SUCCESS</code> if the creation was successful.
         *          <code>PR_FAILURE</code> if an error occurred either while
         *          closing the currently open socket or in creating the
         *          new one.
         */
        PRStatus createSocket(void);

        /**
         * Enables SSL on the listen socket if the configuration specifies
         * that security is on.
         */
        PRStatus enableSSL(void);

        /**
         * Enables Blocking mode on listen socket if the configuration specifed
         * that blocking semantics be enabled.
         */
        PRStatus enableBlocking(void);

        /**
         * Log a message for each IP address present in <code>config1</code>
         * but not present in <code>config2</code>.
         */
        static void logIPSpecificConfigChange(ListenSocketConfig* config1,
                                              ListenSocketConfig* config2,
                                              const char* msg);

        /**
         * Attempt to wake up acceptors by connecting to the listen socket.
         */
        PRStatus wakeup(void);

        /**
         * Allocates memory for the <code>acceptors_</code> array and 
         * creates as many <code>Acceptor</code> objects as have been
         * configured in <code>config_</code>.
         *
         * @returns <code>PR_SUCCESS</code> if all the acceptors were
         *          created successfully. <code>PR_FAILURE</code> indicates
         *          that there was either a memory allocation error or that
         *          the acceptor array was already created.
         */
        PRStatus createAcceptors(ConnectionQueue* connQueue);

        /**
         * Waits for the <code>Acceptor</code> threads to stop running
         * and then releases the memory associated with the 
         * <code>acceptors_</code> array.
         */
        PRStatus deleteAcceptors(void);

        /**
         * Sets the terminating flag in the specified <code>Acceptor</code>
         * thread and sends an interrupt to the thread causing it
         * to exit its <code>run</code> method.
         */
        PRStatus stopAcceptor(Thread* acceptorThread);

        /**
         * Waits for the specified <code>Acceptor</code> thread to stop running
         * and then releases the memory associated with the thread.
         */
        PRStatus deleteAcceptor(Thread* acceptorThread);

        /**
         * Adjust (increase/decrease) the number of <code>Acceptor</code>
         * threads based on the new configuration.
         */
        PRStatus reconfigureAcceptors(ListenSocketConfig& newConfig);
        
        /**
         * Increment the number of <code>Acceptor</code> threads by the 
         * amount specified.
         */
        PRStatus addAcceptors(int nMore);

        /**
         * Reduce the number of <code>Acceptor</code> threads by the 
         * amount specified.
         */
        PRStatus removeAcceptors(int nLess);

        /**
         * Performs a sanity check on the attributes of this object.
         */
        PRBool isValid(void) const;

        /**
         * Sets properties of the socket such as setting TCP_NODELAY to TRUE,
         * and preventing the socket from being inherited across forks.
         */
        void setProperties(void);

        /**
         * Invokes PR_Listen if it has not already been invoked.
         */
        PRStatus listen(void);

        /**
         * Log an error.
         *
         * @param config The configuration to log the error for
         * @param msg    A textual description of the error
         */
        static void logError(ListenSocketConfig* config, const char* msg);

        /**
         * Log an informational message.
         *
         * @param config The configuration to log the message for
         * @param msg    The text of the message
         */
        static void logInfo(ListenSocketConfig* config, const char* msg);

        /**
         * Return the hostname to be used in URLs in log messages.
         *
         * @param config The configuration the log message is for
         * @param buffer Buffer to store the hostname in
         * @param size   Size of the buffer
         */
        static void getLogHostname(ListenSocketConfig *config, char *buffer,
                                   int size);

};

#endif /* _ListenSocket_h_ */
