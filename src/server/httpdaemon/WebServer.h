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

#ifndef _WebServer_h
#define _WebServer_h

#include <stdarg.h>
#include "netsite.h"
#include "prtypes.h"
#include "prthread.h"
#include "prlock.h"
#include "prcvar.h"
#include "hasht.h"
#include "generated/ServerXMLSchema/Log.h"
#include "httpdaemon/internalstats.h"
#include "httpdaemon/ListenSocketConfig.h"
#include "httpdaemon/ListenSockets.h"
#include "NsprWrap/CriticalSection.h"        // CriticalSection class
#include "NsprWrap/ConditionVar.h"           // ConditionVar class

#define DEFAULT_GLOBAL_CONFIG_FILE PRODUCT_MAGNUS_CONF

#if defined(XP_UNIX)
#include "httpdaemon/ChildAdminThread.h"     // ChildAdminThread class
#endif // XP_UNIX

class NSAPI_PUBLIC WebServer {
public:
    //
    // METHODS
    //
    static PRStatus Init(int argc, char *argv[]);
    static PRStatus Run(void);
    static void Cleanup(void);

#ifdef XP_WIN32
    static void SetInstance(const HANDLE handle);
    static const HANDLE GetInstance();
#endif


    /**
     * Dispatch a reconfiguration request to all server processes.
     */
    static void RequestReconfiguration(void);

    /**
     * Dispatch a log reopen request to all server processes.
     */
    static void RequestReopenLogs(void);

    /**
     * Dispatch a restart request to all server processes.
     */
    static void RequestRestart(void);

    /**
     * Dynamically reconfigures the server by loading the new configuration
     * from the server.xml file.  Only a single thread within the server should
     * call this function.
     */
    static PRStatus Reconfigure(void);

    /**
     * Reopens logs files.
     */
    static void ReopenLogs(void);

    /**
     * Sets the terminate flag to <code>PR_TRUE</code> indicating that 
     * the Webserver is terminating.
     */
    static void Terminate(void);

    /**
     * Returns the value of the passed Integer if it's non-NULL, otherwise
     * returns the number of CPUs detected at startup.
     */
    static int GetConcurrency(const Integer *threads = NULL);

    /**
     * The various values of the <tt>state_</code> variable
     */
    enum ServerState
    {
        WS_INITIALIZING  = 0,
        WS_RUNNING       = 1,
        WS_RECONFIGURING = 2,
        WS_TERMINATING   = 3,
        WS_VERSION_QUERY = 4,
        WS_CONFIG_TEST   = 5
    };

    /**
     * Returns PR_TRUE when the webserver is in the initialization stage
     */
    static PRBool isInitializing(void);

    /**
     * Returns PR_TRUE when the webserver is ready (to accept requests).
     */
    static PRBool isReady(void);

    /**
     * Returns PR_TRUE when the webserver is shutting down.
     */
    static PRBool isTerminating(void);

    /**
     * Returns PR_TRUE when the webserver is reconfiguring (upon receipt
     * of SIGUSR2 on Unix systems and XXX on NT systems)
     */
    static PRBool isReconfiguring(void);

	static PRInt32 getTerminateTimeout();

private:

    // server global data, not accessible from outside WebServer class

    // the name of the global configuration file
    static char *globalConfigFilename;

    // the server config directory
    static char *serverConfigDirectory;

    // the parent of the bin and lib directories
    static char *serverInstallDirectory;

    // the server temporary directory
    static char *serverTemporaryDirectory;

    // the server user (may be NULL)
    static char *serverUser;

    // shortcut to all the usual NSAPI configuration global parameters
    // storage for those is kept in frame/conf.cpp
    static conf_global_vars_s *globals;

    // parsed server.xml contents before the first Configuration is created
    static ServerXML *serverXML;

    // fingerprints of files at the time of the first Configuration
    static unsigned char magnusConfFingerprint[MD5_LENGTH];
    static unsigned char certmapConfFingerprint[MD5_LENGTH];
    static unsigned char certDbFingerprint[MD5_LENGTH];
    static unsigned char keyDbFingerprint[MD5_LENGTH];
    static unsigned char secmodDbFingerprint[MD5_LENGTH];

#ifdef XP_WIN32
    static HANDLE hinst;
#endif

    // ExitVariable stuff
    static PRBool     daemon_dead;
    static PRLock *   daemon_exit_cvar_lock;
    static PRCondVar *daemon_exit_cvar;

    /**
     * Indicates the current state of the webserver process.
     *
     * @see #ServerState
     */
    static ServerState state_;

    /**
     * Class-wide lock that is associated with the condition variable
     * <code>server_</code>.
     */
    static CriticalSection* serverLock_;

    /**
     * The condition variable on which the server waits until termination.
     *
     * After the initial configuration has been completed, the
     * Acceptor-Worker threads take over the processing of requests and
     * the <code>Run</code> method waits on this condition variable to
     * detect when the server has been terminated.
     */
    static ConditionVar* server_;

    /**
	 * Terminate TimeOut in seconds
	 * Time for which server will wait fro a clean shutdown to occur
	 */
	static PRInt32 terminateTimeOut_; 

    /**
     * Determines whether the restart callback functions must be invoked
     * regardless of whether there are active DaemonSession threads.
     *
     * Controlled by the ChildRestartCallback directive in magnus.conf
     */
    static PRBool fForceCallRestartFns_;

    /**
     * Determines whether the restart callback functions must be invoked
     * when there are no DaemonSession threads that are processing requests.
     *
     * Controlled by the ChildRestartCallback directive in magnus.conf
     */
    static PRBool fCallRestartFns_;

    /**
     * Determines whether to close stdin/stdout/stderr after initialization.
     */
    static PRBool fDetach_;

    /**
     * Options for Logging. 
     */
    static PRBool fLogToSyslog_;
    static PRBool fLogStdout_;
    static PRBool fLogStderr_;
    static PRBool fLogToConsole_;
    static PRBool fCreateConsole_;

    /**
     * File descriptor to echo the error log to.
     */
    static PRFileDesc *fdConsole_;

    /**
     * Logs data written to stdout to the error log.
     */
    static class LogStdHandle *logStdout_;

    /**
     * Logs data written to stderr to the error log.
     */
    static class LogStdHandle *logStderr_;

    /**
     * Set if this child process was respawned by the primordial following a
     * crash.
     */
    static PRBool fRespawned_;

    /**
     * Set if this is the only process (Windows) or the first child process
     * (Unix).
     */
    static PRBool fFirstBorn_;

    /**
     * The number of CPUs that were detected on startup.
     */
    static int nCPUs_;

    static PRStatus CheckTempDir();

#if defined(XP_UNIX)
    static ChildAdminThread *childAdminThread;
#endif

    static void InitializeExitVariable();
    static void WaitForExitVariable();
    static void NotifyExitVariable();

#if defined(XP_UNIX)
    static PRBool CreatePidFile();
    static PRBool RemovePidFile();
    static PRBool ForkChildren(int num_processes);
#endif
    static PRBool ElementsAreEqual(const LIBXSD2CPP_NAMESPACE::Element *e1, const LIBXSD2CPP_NAMESPACE::Element *e2);
    static void ChangesNotApplied(int reason);
    static void ChangesIncompatible(int reason);
    static void CheckFileFingerprint(const char *filename, const unsigned char *oldfingerprint);
    static void CheckObsoleteFile(const char *filename);
    static PRBool CheckNSS(const ServerXML *incomingServerXML);
    static void ProcessConfiguration(const Configuration *incoming, const Configuration *outgoing);
    static PRStatus ConfigureLogging(const ServerXMLSchema::Log& log);
    static void ReconfigureLogging(const ServerXMLSchema::Log& log);
    static void StartLoggingThreads();
    static void StopLoggingThreads();
    static PRStatus ConfigureFileDescriptorLimits(const Configuration *configuration);
};

#endif // _WebServer_h
