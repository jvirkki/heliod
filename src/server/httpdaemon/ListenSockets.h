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

#ifndef _ListenSockets_h_
#define _ListenSockets_h_

#include "nspr.h"                              // NSPR declarations
#include "NsprWrap/CriticalSection.h"          // CriticalSection/SafeLock
#include "support/GenericList.h"               // GenericList class
#include "httpdaemon/configurationmanager.h"   // ConfigurationListener class
#ifdef XP_UNIX
#include "httpdaemon/WatchdogClient.h"         // WatchdogClient class
#endif

class ConnectionQueue;

/**
 * This class manages all the <code>ListenSocket</code> instances in the
 * webserver.
 * 
 * This class is implemented as a Singleton in order to ensure that
 * there is exactly one instance of this class during the lifetime of the
 * process. All the public methods of this class are synchronized.
 *
 * On program exit, the user should delete the memory returned by the
 * <code>getInstance()</code> method in order to prevent memory leaks.
 *
 * @since   iWS5.0
 */


class ListenSockets : public ConfigurationListener
{
    public:

        /**
         * Singleton pseudo-constructor
         *
         * A factory method that returns a pointer to an instance of
         * this class. If an instance of this class doesn't exist, then
         * one gets created and a pointer to it is returned.
         * This static method is the only means by which one may obtain
         * access to an instance of the this class. This ensures that
         * exactly one instance of this class exists during the lifetime
         * of the application.
         */
        static ListenSockets* getInstance(void);

        /**
         * Destructor
         *
         * Shuts down all the listen sockets and deletes the memory
         * associated with them (invokes <code>closeAll()</code>)
         */
        ~ListenSockets(void);

        /**
         * Sets the <code>ConnectionQueue</code> object that is used
         * by every Acceptor, DaemonSession and KAPollThread.
         *
         * This is a synchronized method.
         */
        void setConnectionQueue(ConnectionQueue* connQueue);

        /**
         * Reconfigures the list of listen sockets based upon the new
         * configuration.
         *
         * This is a synchronized method.
         *
         * @param newConfig     The attributes of the new configuration.
         * @param currentConfig The attributes of the current configuration.
         * @returns             <code>PR_SUCCESS</code> if the list of listen
         *                      sockets could be reconfigured successfully. If
         *                      an error occurs either will creating a new 
         *                      listen socket or while shutting down and old
         *                      one, then <code>PR_FAILURE</code> is returned.
         */
        PRStatus setConfiguration(Configuration* newConfig,
                                  const Configuration* currentConfig);

        /**
         * Closes all the listen sockets that are currently open.
         *
         * This is a synchronized method.
         */
        PRStatus closeAll(void);

        /**
         * Indicate whether the server can listen on a given port on both
         * INADDR_ANY and a specific IP simultaneously.
         *
         * @returns <code>PR_TRUE</code> if the server can listen on a given
         *          port on both INADDR_ANY and a specific IP simultaneously,
         *          otherwise <code>PR_FALSE</code>.
         */
        static PRBool canBindAnyAndSpecific(void);

#ifdef DEBUG
        /**
         * Prints the contents of this object to cout.
         */
        void printInfo(void) const;
#endif

        /**
         * A pointer to the <i>only</i> instance of this class.
         */
        static ListenSockets* instance_;

        /**
         * Ensures that only 1 <code>instance_</code> is created even if
         * <code>getInstance()</code> is invoked by multiple threads.
         */
        static CriticalSection lock_;

        /**
         * Represents the open listen socket on which the server is 
         * willing to accept new connections.
         */
        GenericList lsList_;

        /**
         * The threadsafe queue over which Acceptor, DaemonSession and
         * KAPollThread enqueue and deqeue Connection instances.
         */
        ConnectionQueue* connQueue_;

        /**
         * The configuration we're using.
         */
        Configuration* currentConfig_;

        /**
         * Void constructor
         *
         * This class is a Singleton and hence the void constructor is
         * not accessible to users.
         */
        ListenSockets(void);

        /**
         * Copy constructor. Objects of this class cannot be
         * copy constructed.
         */
        ListenSockets(const ListenSockets& source);

        /**
         * operator=. Objects of this class cannot be cloned.
         */
        ListenSockets& operator=(const ListenSockets& source);

        /**
         * Reconfigures the list of listen sockets based upon the specified
         * configuration.
         *
         * Existing listen sockets that are <i>not</i> specified in the
         * new configuration are closed. Listen sockets that are specified
         * in the new configuration but are not part of the existing list
         * of listen sockets, get created.
         *
         * @param newConfig     The attributes of the new configuration.
         * @returns             <code>PR_SUCCESS</code> if the list of listen
         *                      sockets could be reconfigured successfully. If
         *                      an error occurs either will creating a new 
         *                      listen socket or while shutting down and old
         *                      one, then <code>PR_FAILURE</code> is returned.
         */
        PRStatus setConfiguration(Configuration& newConfig);

        /**
         * Close any sockets from the existing configuration that would
         * conflict with sockets in the new configuration. For example, close
         * 1.2.3.4:80 if 0.0.0.0:80 is being added. This function should only
         * be callsed when <code>canBindAnyAndSpecific() == PR_FALSE</code>.
         */
        void closeConflictingSockets(Configuration& newConfig);

        /**
         * Compares the list of sockets in the new configuration against
         * the current configuration and creates new <code>ListenSocket</code>
         * instances for sockets that do not exist in the current configuration
         * but do in the new configuration. Listen sockets instances are
         * created in 2 passes. First, listen sockets that listen on explicit
         * IP addresses are instantiated. Second, listen sockets that listen on
         * INADDR_ANY are instantiated. This ensures that a newly instantiated
         * INADDR_ANY listen socket does not accidentally accept a connection
         * intended for a listen socket that will listen on an explicit IP.
         *
         * @param newConfig The new configuration as loaded from the updated
         *                  XML file.
         * @param lsExists  An array (of booleans) whose length must be the
         *                  same as the number of listen sockets in the current
         *                  configuration. Upon return from this method, 
         *                  for every listen socket that exists in both the
         *                  current as well as the new configuration, the 
         *                  corresponding entry in this array will hold the
         *                  value <code>PR_TRUE</code>.
         * @param nSockets  The number of elements in the lsExists array.
         * @returns         <code>PR_SUCCESS</code> if the creation of new 
         *                  sockets was successful and <code>PR_FAILURE</code>
         *                  if errors occurred.
         */
        PRStatus addNewSockets(Configuration& newConfig, PRBool* lsExists,
                               int nSockets);

        /**
         * Compares the list of sockets in the new configuration against
         * the current configuration and creates new <code>ListenSocket</code>
         * instances for sockets that do not exist in the current configuration
         * but do in the new configuration.
         *
         * @param newConfig   The new configuration as loaded from the updated
         *                    XML file.
         * @param lsExists    An array (of booleans) whose length must be the
         *                    same as the number of listen sockets in the 
         *                    current configuration. Upon return from this
         *                    method, for every listen socket that exists in
         *                    both the current as well as the new
         *                    configuration, the corresponding entry in this
         *                    array will hold the value <code>PR_TRUE</code>.
         * @param nSockets    The number of elements in the lsExists array.
         * @param bExplicitIP <code>PR_TRUE</code> if only listen sockets that
         *                    listen on explicit IPs should be instaniated,
         *                    <code>PR_FALSE</code> if only listen sockets that
         *                    listen on INADDR_ANY should be instantiated.
         * @returns           The number of errors that occurred.
         */
        int addNewSockets(Configuration& newConfig, PRBool* lsExists,
                          int nSockets, PRBool bExplicitIP);

        /**
         * Closes listen sockets corresponding to <code>PR_FALSE</code>
         * entries in <code>lsExists</code> and removes them from the
         * list of active listen sockets. Listen sockets are closed in 2
         * passes. First, listen sockets that listen on INADDR_ANY are closed.
         * Second, listen sockets that listen on explicit IP addresses are
         * closed. This ensures that a listen socket that listens on INADDR_ANY
         * does not accidentally accept a connection intended for a listen
         * socket that was just closed.
         *
         * This method must be invoked <i>after</i> <code>addNewSockets</code>.
         *
         * @param lsExists An array of booleans in which <code>PR_TRUE</code>
         *                 entries correspond to listen sockets that exist
         *                 in both the current as well as the new configuration.
         *                 <code>PR_FALSE</code> entries denote listen sockets
         *                 that exist in the current configuration but not in
         *                 the new one and hence must be closed.
         * @param nSockets The number of entries in <code>lsExists</code>.
         *                 As this method is meant to be invoked <i>after</i>
         *                 <code>addNewSockets</code> and hence new listen
         *                 sockets could possibly have been added to the list
         *                 of active listen sockets. This parameter must
         *                 contain the value of the number of sockets in
         *                 the "old" configuration, prior to the addition of
         *                 the new sockets.
         * @returns        <code>PR_SUCCESS</code> if all the inactive 
         *                  sockets could be closed successfully. If errors
         *                  occurred, then <code>PR_FAILURE</code> is returned.
         */
        PRStatus closeOldSockets(PRBool* lsExists, int nSockets);

        /**
         * Creates a new <code>ListenSocket</code> instance with the
         * specified configuration, adds it to the list of active
         * listen sockets and starts its <code>Acceptor</code> threads.
         *
         * @param config The properties for the new socket
         * @returns      <code>PR_SUCCESS</code> upon success and 
         *               <code>PR_FAILURE</code> if errors occur.
         */
        PRStatus activateNewLS(ListenSocketConfig* config);

};

#endif /* _ListenSockets_h_ */
