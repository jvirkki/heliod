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

#include "definesEnterprise.h"
#include "httpdaemon/WebServer.h"
#ifdef XP_UNIX
#include "UnixSignals.h"
#endif

#include "base/systhr.h"
#include "base/daemon.h"
#include "base/date.h"
#include "base/vs.h"
#include "nspr.h"
#include "private/pprio.h"
#include "private/pprthred.h"
#include "support/EreportableException.h"
#include "generated/ServerXMLSchema/Server.h"
#include "libserverxml/ServerXML.h"
#include "safs/favicon.h"
#include "safs/ntrans.h"	                // for ntrans_init_crits prototype
#include "safs/auth.h"		                // for auth_init_crits prototype
#include "safs/reqlimit.h"                      // reqlimit_init_crits
#include "base/util.h"		                // for util_* prototypes
#include "base/ereport.h"	                // for ereport* prototypes
#include "base/dns_cache.h"	                // for dns_cache_init prototype
#include "frame/log.h"		                // for log_ereport* definition
#include "safs/acl.h"		                // for ACL_InitHttp* prototypes
#include "frame/clauth.h"
#include "frame/conf.h"		                // for conf_* prototypes
#include "../libsi18n/gshttpd.h"                // for DATABASE_NAME
#include "httpdaemon/dbthttpdaemon.h"	        // for XP_InitStringDatabase
#include "frame/filter.h"
#include "frame/httpfilter.h"
#include "frame/http.h"
#include "frame/error.h"
#include "frame/req.h"
#include "frame/accel.h"
#include "base/servnss.h"	                // for servssl_*
#include "httpdaemon/HttpMethodRegistry.h"      // for HttpMethodRegistry class
#include "time/nstime.h"	                // for nstime_init
#include "safs/nsfcsafs.h"	                // for nsfc_cache_init
#include "safs/cgi.h"		                // for cgiwatch_internal_init
#include "safs/flexlog.h"
#ifdef MCC_PROXY
#include "libproxy/filter.h"		    // for prefilter_subsystem_init
#include "libproxy/host_dns_cache.h"	// for host_dns_cache_hostent_init
#endif
#ifdef FEAT_SOCKS
#include "libproxy/sockslayer.h"        // sockslayer_init
#endif
#include "safs/filtersafs.h"
#include "safs/headerfooter.h"
#include "safs/httpcompression.h"       // http_compression_init
#include "safs/sed.h"
#include "safs/var.h"
#include "safs/control.h"
#include "safs/child.h"
#include "ares/arapi.h"		           // for PR_AR_* prototypes
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/configuration.h"           // Configuration class
#include "httpdaemon/configurationmanager.h"    // Configurationmanager class
#include "httpdaemon/vsmanager.h"               // VSManager class
#include "httpdaemon/statsmanager.h"            // StatsManager class
#include "httpdaemon/logmanager.h"              // LogManager class
#include "httpdaemon/scheduler.h"
#include "httpdaemon/JavaConfig.h"
#include "httpdaemon/updatecrl.h"
#include "smartheapinit.h"
#include "stdhandles.h"
#include "lognsprdescriptor.h"
#include "xercesc/util/PlatformUtils.hpp"
#include "xalanc/XPath/XPathEvaluator.hpp"
#include "httpdaemon/ListenSocket.h"            // ListenSocket class
#include "httpdaemon/ListenSockets.h"           // ListenSockets class
#include "httpdaemon/throttling.h"
#ifdef XP_UNIX
#include "base/unix_utils.h"                    // max_fdset/redirectStdStreams
#include "httpdaemon/WatchdogClient.h"          // WatchdogClient class
#include "httpdaemon/ParentAdmin.h"             // ParentAdmin class
#endif
#include "httpdaemon/ParentStats.h"             // ParentStats class

#include "libaccess/digest.h"                   // Digest auth functions
#include "libaccess/gssapi.h"
#include "libproxy/route.h"
#include "libproxy/channel.h"
#include "libproxy/reverse.h"
#include "libproxy/httpclient.h"
#include "shtml/ShtmlSaf.h"
#ifdef FEAT_SECRULE
#include "libsecrule/sec_filter.h"
#endif

#ifdef XP_WIN32
#include "base/eventhandler.h"
#include "nt/regparms.h"
#include "nt/ntwdog.h"
#include "wingetopt.h"
#include "uniquename.h"
#endif

using namespace XERCES_CPP_NAMESPACE;
using namespace XALAN_CPP_NAMESPACE;

extern struct FuncStruct func_standard[];

#ifdef XP_UNIX
#include <grp.h>		// for initgroups prototype
#include <sys/wait.h>		// for wait stuff
#include <limits.h>
#else
#include <process.h>
#define PATH_MAX MAX_PATH
#endif

conf_global_vars_s *   WebServer::globals = NULL;
PRBool                 WebServer::daemon_dead;
PRLock *               WebServer::daemon_exit_cvar_lock;
PRCondVar *            WebServer::daemon_exit_cvar;
char *                 WebServer::serverConfigDirectory;
char *                 WebServer::serverInstallDirectory;
char *                 WebServer::serverTemporaryDirectory;
char *                 WebServer::serverUser;
ServerXML *            WebServer::serverXML;
unsigned char          WebServer::magnusConfFingerprint[MD5_LENGTH];
unsigned char          WebServer::certmapConfFingerprint[MD5_LENGTH];
unsigned char          WebServer::certDbFingerprint[MD5_LENGTH];
unsigned char          WebServer::keyDbFingerprint[MD5_LENGTH];
unsigned char          WebServer::secmodDbFingerprint[MD5_LENGTH];
PRInt32                WebServer::terminateTimeOut_ = 0; 
PRBool                 WebServer::fForceCallRestartFns_ = PR_FALSE; 
PRBool                 WebServer::fCallRestartFns_ = PR_TRUE; 
PRBool                 WebServer::fDetach_ = PR_FALSE;
PRFileDesc *           WebServer::fdConsole_ = NULL;
LogStdHandle *         WebServer::logStdout_ = NULL;
LogStdHandle *         WebServer::logStderr_ = NULL;
PRBool                 WebServer::fRespawned_ = PR_FALSE;
PRBool                 WebServer::fFirstBorn_ = PR_FALSE;
PRBool 		       WebServer::fLogToSyslog_ = PR_FALSE;
PRBool 		       WebServer::fLogStdout_ = PR_FALSE;
PRBool 		       WebServer::fLogStderr_ = PR_FALSE;
PRBool 		       WebServer::fLogToConsole_ = PR_FALSE;
PRBool 		       WebServer::fCreateConsole_ = PR_FALSE;
int                    WebServer::nCPUs_ = 0;

#ifdef XP_UNIX
ChildAdminThread *WebServer::childAdminThread = NULL;
#else
HANDLE                 WebServer::hinst = NULL;
#endif

WebServer::ServerState WebServer::state_ = WebServer::WS_INITIALIZING;
CriticalSection* WebServer::serverLock_ = NULL;
ConditionVar* WebServer::server_ = NULL;

#define DEFAULT_TERMINATE_TIMEOUT 30

// uncomment to be able to run the server without uxwdog and without having it fork
// this is essential for debugging on Linux.
// This is how it works:
// uncomment the #define a couple of lines down.
// recompile and install
// cp start start.debug
// replace
//  ./$PRODUCT_BIN -d $PRODUCT_SUBDIR/config $@
// with
//  echo ./$PRODUCT_BIN -d $PRODUCT_SUBDIR/config $@
//  gdb ./ns-httpd
// then call "./start.debug". set your breakpoints, then
// enter "run -d <$PRODUCT_SUBDIR/config>", where <$PRODUCT_SUBDIR/config>
// is cut&pasted from the "echo" above.
//
// #define DEBUG_SINGLEPROCESS

static int ereport_console_callback(const VirtualServer* vs, int degree, const char *formatted, int formattedlen, const char *raw, int rawlen, void *data)
{
    PRFileDesc *fd = *(PRFileDesc **)data;
    if (fd != NULL) {
        // Isolate message and degree (i.e. don't log date and pid)
        const char *message = strchr(raw, ':');
        int degreelen = 0;
        const char *p = strchr(raw, ' ');
        if (p && p < message)
            degreelen = p - raw;

        // Log the abbreviated message to the console (don't use PR_fprintf()
        // as it can call malloc())
        if (message && degreelen) {
            PR_Write(fd, raw, degreelen);
            PR_Write(fd, message, strlen(message));
        } else {
            PR_Write(fd, raw, rawlen);
        }
    }

    // Log this message to the log file as well
    return 0;
}

static const char *default_language_callback(void *data)
{
    static char *language = NULL;

    Configuration *configuration = ConfigurationManager::getConfiguration();
    if (configuration) {
        if (language == NULL || strcmp(language, configuration->localization.defaultLanguage))
            language = PERM_STRDUP(configuration->localization.defaultLanguage);
        configuration->unref();
    }

    return language;
}

static inline int bitcount(PRUint32 mask)
{
    int c = 0;

    while (mask) {
        if (mask & 1)
            ++c;
        mask >>= 1;
    }

    return c;
}

static unsigned hashstring(const char *string)
{
    unsigned hash = 0;
    while (*string) {
        hash = (hash << 5) ^ hash ^ *(const unsigned char *)string;
        string++;
    }
    return hash;
}

static void hashfile(const char *filename, unsigned char *md5, size_t size)
{
    PR_ASSERT(size == MD5_LENGTH);

    memset(md5, 0, size);

    void *hctx = nsapi_md5hash_create();
    if (hctx != NULL) {
        nsapi_md5hash_begin(hctx);

        PRFileDesc *fd = PR_Open(filename, PR_RDONLY, 0);
        if (fd != NULL) {
            for (;;) {
                unsigned char buf[4096];
                PRInt32 rv = PR_Read(fd, buf, sizeof(buf));
                if (rv < 1)
                    break;

                nsapi_md5hash_update(hctx, buf, rv);
            }
            PR_Close(fd);
        }

        nsapi_md5hash_end(hctx, md5);
        nsapi_md5hash_destroy(hctx);
    }
}

static int cmpfilehash(const char *filename, const unsigned char *oldmd5, size_t size)
{
    PR_ASSERT(size == MD5_LENGTH);

    unsigned char newmd5[MD5_LENGTH];
    hashfile(filename, newmd5, sizeof(newmd5));

    return memcmp(newmd5, oldmd5, size);
}

static int getdescriptors(const Integer *descriptors)
{
    if (descriptors)
        return *descriptors;

    return -1;
}

static int round2(int n)
{
    int powerof2 = 1;

    // Chose a "pretty" number less than or equal to n that's a power of 2
    // or halfway between two powers of 2
    for (;;) {
        if (n < powerof2 + powerof2 / 2)
            return powerof2;
        if (n < 2 * powerof2)
            return powerof2 + powerof2 / 2;
        powerof2 *= 2;
    }
}

#ifdef XP_UNIX
static void clearenv(const char *name)
{
    const char *current = getenv(name);
    const char *desired = conf_getstring(name, NULL);
    if (current || desired) {
        NSString nv;
        nv.append(name);
        nv.append("=");
        if (desired)
            nv.append(desired);
        putenv(strdup(nv));
    }
}
#endif

#ifdef XP_WIN32
static PRBool server_set_libpath(char *inputPath) 
{
    PR_ASSERT(inputPath);

    NSString nsPath;
    nsPath.append("PATH=");
    nsPath.append(inputPath);
    const char *originalPath = getenv("PATH");
    if (originalPath) {
        nsPath.append(LIBPATH_SEPARATOR);
        nsPath.append(originalPath);
    }
    putenv(PERM_STRDUP(nsPath));
    return true;
}
#endif

static void usage()
{
    PR_fprintf(PR_STDERR, "Usage: "PRODUCT_DAEMON_BIN" -d configdir -r installdir [-t tempdir] [-u user] [-c]\n");
    PR_fprintf(PR_STDERR, "       "PRODUCT_DAEMON_BIN" -v\n");
}

PRBool
WebServer::ElementsAreEqual(const LIBXSD2CPP_NAMESPACE::Element *e1, const LIBXSD2CPP_NAMESPACE::Element *e2)
{
    if (e1 != NULL && e2 != NULL)
        return (*e1 == *e2);
    return (e1 == NULL && e2 == NULL);
}

void
WebServer::ChangesNotApplied(int reason)
{
    // Warn that the Configuration was installed, but that a particular setting
    // was ignored
    ereport(LOG_WARN,
            XP_GetAdminStr(DBT_Reconfigure_ChangesNotApplied),
            XP_GetAdminStr(reason));
}

void
WebServer::ChangesIncompatible(int reason)
{
    // Report that the Configuration cannot be installed because it is
    // incompatible with the current configuration
    ereport(LOG_FAILURE,
            XP_GetAdminStr(DBT_Reconfigure_ChangesIncompatible),
            XP_GetAdminStr(reason));
}

void
WebServer::CheckFileFingerprint(const char *filename, const unsigned char *oldfingerprint)
{
    // Warn that changes to a particular configuration file were ignored
    if (cmpfilehash(filename, oldfingerprint, MD5_LENGTH)) {
        NSString reason;
        reason.printf(XP_GetAdminStr(DBT_Reconfigure_FileXRestart), filename);
        ereport(LOG_WARN,
                XP_GetAdminStr(DBT_Reconfigure_ChangesNotApplied),
                reason.data());
    }
}

void
WebServer::CheckObsoleteFile(const char *filename)
{
    // Warn that an obsolete configuration file was ignored
    char *path = file_canonicalize_path(filename);
    if (path != NULL) {
        PRFileInfo64 finfo;
        if (PR_GetFileInfo64(path, &finfo) == PR_SUCCESS) {
            ereport(LOG_WARN,
                    XP_GetAdminStr(DBT_Configuration_ObsoleteFile),
                    path);
        }
        FREE(path);
    }
}

PRBool
WebServer::CheckNSS(const ServerXML *incomingServerXML)
{
    // NSS cannot be dynamically initialized.  Since attempts to instantiate an
    // SSLSocketConfiguration during Configuration::create() will fail if NSS
    // is disabled, reject ServerXMLs that attempt to enable NSS after startup.
    if (incomingServerXML->server.pkcs11.enabled && !security_active) {
        ChangesIncompatible(DBT_Reconfigure_EnablingSSLRestart);
        return PR_FALSE; // do not attempt to instantiate the Configuration
    }

    // Warn about changes to NSS databases.  Do this before we instantiate the
    // Configuration in case an SSLSocketConfiguration references a newly added
    // certificate.
    if (cmpfilehash(CERTN_DB, certDbFingerprint, sizeof(certDbFingerprint)) ||
        cmpfilehash(KEYN_DB, keyDbFingerprint, sizeof(keyDbFingerprint)))
        ChangesNotApplied(DBT_Reconfigure_TrustDbRestart);
    if (cmpfilehash(SECMOD_DB, secmodDbFingerprint, sizeof(secmodDbFingerprint)))
        ChangesNotApplied(DBT_Reconfigure_SecmodDbRestart);

    return PR_TRUE; // attempt to instantiate the Configuration
}

void
WebServer::ProcessConfiguration(const Configuration *incoming, const Configuration *outgoing)
{
    ServerXMLSchema::Server& orig = serverXML->server;

    // Warn about changes to magnus.conf and certmap.conf
    CheckFileFingerprint(PRODUCT_MAGNUS_CONF, magnusConfFingerprint);
    CheckFileFingerprint(CERTMAP_CONF, certmapConfFingerprint);

    // Warn about logging settings that cannot be dynamically reconfigured
    if (incoming->log.logStdout != orig.log.logStdout ||
        incoming->log.logStderr != orig.log.logStderr ||
        incoming->log.logToConsole != orig.log.logToConsole ||
        incoming->log.createConsole != orig.log.createConsole ||
        incoming->log.logToSyslog != orig.log.logToSyslog ||
        incoming->log.dateFormat != orig.log.dateFormat ||
        incoming->log.logFile != orig.log.logFile)
        ChangesNotApplied(DBT_Reconfigure_LoggingRestart);

    // Some logging settings can be dynamically reconfigured
    ReconfigureLogging(incoming->log);

    // Warn about platform changes
    if (incoming->platform != orig.platform)
        ChangesNotApplied(DBT_Reconfigure_PlatformRestart);

    // Warn about server user changes
    if (!ElementsAreEqual(incoming->getUser(), orig.getUser()))
        ChangesNotApplied(DBT_Reconfigure_UserRestart);

    // Warn about temporary directory changes
    if (!ElementsAreEqual(incoming->getTempPath(), orig.getTempPath()))
        ChangesNotApplied(DBT_Reconfigure_TempPathRestart);

    // Warn about HTTP protocol changes
    if (incoming->http != orig.http)
        ChangesNotApplied(DBT_Reconfigure_HTTPRestart);

    // Warn about keep-alive changes
    if (incoming->keepAlive != orig.keepAlive)
        ChangesNotApplied(DBT_Reconfigure_KeepAliveRestart);

    // Warn about thread pool settings that cannot be dynamically reconfigured
    // XXX elving Some thread pool settings should be dynamically reconfigured
    if (incoming->threadPool != orig.threadPool)
        ChangesNotApplied(DBT_Reconfigure_ThreadPoolRestart);

    // Some thread pool settings can be dynamically reconfigured
    // XXX elving DaemonSession::Reconfigure(incoming->threadPool);

    // Warn about PKCS #11 changes (note that attempts to enable NSS/SSL are
    // caught and rejected earlier by WebServer::CheckCompatibleServerXML())
    if (incoming->pkcs11 != orig.pkcs11)
        ChangesNotApplied(DBT_Reconfigure_PKCS11Restart);

    // Warn about statistics collection changes
    if (incoming->stats != orig.stats)
        ChangesNotApplied(DBT_Reconfigure_StatsRestart);

    // Warn about CGI changes
    if (incoming->cgi != orig.cgi)
        ChangesNotApplied(DBT_Reconfigure_CGIRestart);

    // Warn about DNS changes
    if (incoming->dns != orig.dns || incoming->dnsCache != orig.dnsCache)
        ChangesNotApplied(DBT_Reconfigure_DNSRestart);

    // Warn about file cache changes
    if (incoming->fileCache != orig.fileCache)
        ChangesNotApplied(DBT_Reconfigure_FileCacheRestart);

    // Warn about ACL cache changes
    if (incoming->aclCache != orig.aclCache)
        ChangesNotApplied(DBT_Reconfigure_ACLCacheRestart);

    // Warn about SSL/TLS session cache changes
    if (incoming->sslSessionCache != orig.sslSessionCache)
        ChangesNotApplied(DBT_Reconfigure_SSLSessionCacheRestart);

    // Warn about access log buffering changes
    if (incoming->accessLogBuffer != orig.accessLogBuffer)
        ChangesNotApplied(DBT_Reconfigure_AccessLogBufferRestart);

    // Warn about JVM changes
    if (!ElementsAreEqual(incoming->getJvm(), orig.getJvm()))
        ChangesNotApplied(DBT_Reconfigure_JVMRestart);

    // Warn about session replication changes
    if (incoming->getCluster() || orig.getCluster()) {
        if (incoming->getCluster() && orig.getCluster()) {
            if (incoming->getCluster()->sessionReplication !=
                orig.getCluster()->sessionReplication)
                ChangesNotApplied(DBT_Reconfigure_SessionReplicationRestart);
        } else {
            ChangesNotApplied(DBT_Reconfigure_ClusterRestart);
        }
    }
}

PRStatus
WebServer::ConfigureLogging(const ServerXMLSchema::Log& log)
{
    char *err = NULL;

    ereport_set_servername(globals->Vserver_id);

    fdConsole_ = PR_STDERR;

    if (WebServer::state_ == WS_CONFIG_TEST) {
        // For configuration tests, we don't log anything
        err = ereport_init("-", NULL, NULL, BUILD_NUM, PR_FALSE);
        ereport_set_timefmt(NULL);

    } else {
        // Get logging options from server.xml
        const char *logfile;
        if (log.logToSyslog) {
            fLogToSyslog_ = PR_TRUE;
            logfile = EREPORT_SYSLOG;
        } else {
            fLogToSyslog_ = PR_FALSE;
            logfile = log.logFile;
        }
        fLogStdout_ = log.logStdout;
        fLogStderr_ = log.logStderr;
        fLogToConsole_ = log.logToConsole;
        fCreateConsole_ = log.createConsole;
        ReconfigureLogging(log);

#ifdef XP_WIN32
        // Should we explicitly create a console?
        if (fCreateConsole_) {
            // Create the console
            StdHandles::console();

            // Give the console window a meaningful title
            char tmp[256];
            util_snprintf(tmp, sizeof(tmp), PRODUCT_BRAND_NAME ": %s", server_id);
            SetConsoleTitle(tmp);
        }
#endif

        // Open the errors log
        err = ereport_init(logfile, NULL,globals->Vuserpw, BUILD_NUM,
                           globals->restarted_by_watchdog);
        ereport_set_timefmt(log.dateFormat);

        // Send ereport() write to the console as well as the log?
        if (fLogToConsole_)
            ereport_register_cb(&ereport_console_callback, &fdConsole_);
    }

    if (err) {
        ereport(LOG_FAILURE, "logging initialization failed: %s", err);
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

void
WebServer::ReconfigureLogging(const ServerXMLSchema::Log& log)
{
    int degree;
    switch (log.logLevel) {
    case ServerXMLSchema::LogLevel::LOGLEVEL_CATASTROPHE: degree = LOG_CATASTROPHE; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_SECURITY: degree = LOG_SECURITY; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_CONFIG: degree = LOG_MISCONFIG; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_FAILURE: degree = LOG_FAILURE; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_WARNING: degree = LOG_WARN; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_INFO: degree = LOG_INFORM; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_FINE: degree = LOG_VERBOSE; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_FINER: degree = LOG_FINER; break;
    case ServerXMLSchema::LogLevel::LOGLEVEL_FINEST: degree = LOG_FINEST; break;
    default: PR_ASSERT(0); degree = LOG_INFORM; break;
    }

    ereport_set_degree(degree);

    ereport_set_logvsid(log.logVirtualServerName);
}

void
WebServer::StartLoggingThreads()
{
    // If we want to log to the console...
    if (fLogToConsole_) {
        if (fLogStdout_) {
            // Redirect stdout to /dev/null
            if (PRFileDesc *fdStdout = StdHandles::out.null())
                PR_Close(fdStdout);
        }
        if (fLogStderr_) {
            // Redirect stderr to /dev/null but keep a copy of the original
            // around to use as the console
            if (PRFileDesc *fdStderr = StdHandles::err.null()) {
                if (fdConsole_ == PR_STDERR) {
                    fdConsole_ = fdStderr;
                } else {
                    PR_Close(fdStderr);
                }
            }
        }
    }

    PR_ASSERT(logStdout_ == NULL);
    PR_ASSERT(logStderr_ == NULL);

    // Create threads to log stdout/stderr to ereport()
    if (fLogStdout_)
        logStdout_ = new LogStdHandle(StdHandles::out, LOG_INFORM, XP_GetAdminStr(DBT_stdoutPrefix));
    if (fLogStderr_)
        logStderr_ = new LogStdHandle(StdHandles::err, LOG_WARN, XP_GetAdminStr(DBT_stderrPrefix));
}

void
WebServer::StopLoggingThreads()
{
    if (logStdout_ != NULL) {
        delete logStdout_;
        logStdout_ = NULL;
    }

    if (logStderr_ != NULL) {
        delete logStderr_;
        logStderr_ = NULL;
    }
}

PRStatus
WebServer::ConfigureFileDescriptorLimits(const Configuration *configuration)
{
    int i;

    int nFileCacheMaxOpenFiles = getdescriptors(configuration->fileCache.getMaxOpenFiles());
    int nKeepAliveMaxConnections = getdescriptors(configuration->keepAlive.getMaxConnections());
    int nThreadPoolQueueSize = getdescriptors(configuration->threadPool.getQueueSize());

#ifdef XP_WIN32
    // Windows doesn't have a priori file descriptor limits, so invent some
    if (nFileCacheMaxOpenFiles == -1)
        nFileCacheMaxOpenFiles = configuration->fileCache.maxEntries;
    if (nKeepAliveMaxConnections == -1)
        nKeepAliveMaxConnections = 65536;
    if (nThreadPoolQueueSize == -1)
        nThreadPoolQueueSize = 16384;
#else
    // Figure out how many file descriptors are available to us
    int nMaxDescriptors = maxfd_get();
    int nAvailableDescriptors = nMaxDescriptors - maxfd_getopen();

    // Because administrators may increase the operating system file descriptor
    // limit without adjusting the Web Server configuration when they encounter
    // "too many open file" errors, we need to leave some additional slack
    // that's proportional to the operating system file descriptor limit.  If
    // we didn't, Web Server would gobble up all the new file descriptors and
    // the "too many open file" error would reappear.  20% sounds okay.
    nAvailableDescriptors -= (nMaxDescriptors / 5);

    // Assume each request-processing thread can use 4 file descriptors
    // (e.g. the client socket descriptor, the requested file, an included
    // file, and a backend connection).  Note also that our HTTP client will
    // maintain up to pool_maxthreads * 2 idle connections.
    nAvailableDescriptors -= (pool_maxthreads * 4);

    // Leave room for listen sockets
    nAvailableDescriptors -= configuration->getHttpListenerCount();

    // Leave room for JDBC connections
    for (i = 0; i < configuration->getJdbcResourceCount(); i++)
        nAvailableDescriptors -= configuration->getJdbcResource(i)->maxConnections;

    // Leave room for access logs (we assume that the error logs have already
    // been opened)
    nAvailableDescriptors -= configuration->getAccessLogCount();
    for (i = 0; i < configuration->getVirtualServerCount(); i++)
        nAvailableDescriptors -= configuration->getVirtualServer(i)->getAccessLogCount();

    // For those file descriptor limits the administrator didn't specify,
    // figure out how to divvy up the available file descriptors
    double weightFileCacheMaxOpenFiles = 0.0;
    double weightKeepAliveMaxConnections = 0.0;
    double weightThreadPoolQueueSize = 0.0;
    double weightSum = 0.0;
    if (configuration->fileCache.enabled) {
        if (nFileCacheMaxOpenFiles != -1) {
            nAvailableDescriptors -= nFileCacheMaxOpenFiles;
        } else {
            weightFileCacheMaxOpenFiles = 1.0;
            weightSum += weightFileCacheMaxOpenFiles;
        }
    }
    if (configuration->keepAlive.enabled) {
        if (nKeepAliveMaxConnections != -1) {
            nAvailableDescriptors -= nKeepAliveMaxConnections;
        } else {
            weightKeepAliveMaxConnections = 16.0;
            weightSum += weightKeepAliveMaxConnections;
        }
    }
    if (configuration->threadPool.enabled) {
        if (nThreadPoolQueueSize != -1) {
            nAvailableDescriptors -= nThreadPoolQueueSize;
        } else {
            if (nAvailableDescriptors * 8.0 / (weightSum + 8.0) >= 1024) {
                // There are lots of available file descriptors, so we can afford
                // to give more to keep-alive and file cache
                weightThreadPoolQueueSize = 8.0;
            } else {
                // There aren't many available file descriptors, so keep lots for
                // the connection queue to avoid overflows
                weightThreadPoolQueueSize = 16.0;
            }
            weightSum += weightThreadPoolQueueSize;
        }
    }

    // Dole out the available file descriptors    
    if (weightSum) {
        PRBool lackOfFildes = PR_FALSE;
        if (nFileCacheMaxOpenFiles == -1) {
            nFileCacheMaxOpenFiles = round2(nAvailableDescriptors * weightFileCacheMaxOpenFiles / weightSum);
            if (nFileCacheMaxOpenFiles > configuration->fileCache.maxEntries) {
                // We ended up with more than we needed, so decrease our weight
                weightSum -= weightFileCacheMaxOpenFiles;
                weightFileCacheMaxOpenFiles *= (double) configuration->fileCache.maxEntries / (double) nFileCacheMaxOpenFiles;
                weightSum += weightFileCacheMaxOpenFiles;            
                nFileCacheMaxOpenFiles = configuration->fileCache.maxEntries;
            }
        }
        if (nKeepAliveMaxConnections == -1) {
            nKeepAliveMaxConnections = round2(nAvailableDescriptors * weightKeepAliveMaxConnections / weightSum);
            if (nKeepAliveMaxConnections < pool_maxthreads)
                lackOfFildes = PR_TRUE;
        }
        if (nThreadPoolQueueSize == -1) {
            nThreadPoolQueueSize = round2(nAvailableDescriptors * weightThreadPoolQueueSize / weightSum);
            if (nThreadPoolQueueSize < pool_maxthreads)
                lackOfFildes = PR_TRUE;
        }
        if (lackOfFildes == PR_TRUE)
            ereport(LOG_MISCONFIG, XP_GetAdminStr(DBT_LackOfAvailableFileDescriptors));
    }

    // Update the maxopen in filecache if necessary
    if (configuration->fileCache.enabled && getdescriptors(configuration->fileCache.getMaxOpenFiles()) == -1) {
        if (nsfc_cache_update_maxopen(nFileCacheMaxOpenFiles) == PR_FAILURE)
            return PR_FAILURE;
    }

    ereport(LOG_VERBOSE, "operating system file descriptor limit is %d", nMaxDescriptors);
    ereport(LOG_VERBOSE, "allocating %d file descriptors to the connection queue, %d file descriptors to keep-alive connections, and %d file descriptors to the file cache", nThreadPoolQueueSize, nKeepAliveMaxConnections, nFileCacheMaxOpenFiles);
#endif

    // Configure DaemonSession connection-related settings
    DaemonSession::ConfigureLate(configuration->keepAlive, nKeepAliveMaxConnections, nThreadPoolQueueSize);

    return PR_SUCCESS;
}

PRStatus
WebServer::Init(int argc, char *argv[])
{
    WebServer::state_ = WebServer::WS_INITIALIZING;

    // init NSPR
    systhread_init("netsite");

    // Get a pointer to the conf_global_vars_s
    globals = conf_get_true_globals();

    // parse options
    int c;
    while ((c = getopt(argc, argv, "d:r:t:u:s:cvi")) != -1) {
        switch(c) {
        case 'd':
            serverConfigDirectory = STRDUP(optarg);
            util_uri_normalize_slashes(serverConfigDirectory);
            break;
        case 'r':
            serverInstallDirectory = STRDUP(optarg);
            util_uri_normalize_slashes(serverInstallDirectory);
            break;
        case 't':
            serverTemporaryDirectory = STRDUP(optarg);
            util_uri_normalize_slashes(serverTemporaryDirectory);
            break;
        case 'u':
            if (*optarg)
                serverUser = STRDUP(optarg);
            break;
        case 's':
            break; // ignore (watchdog option)
        case 'c':
            WebServer::state_ = WS_CONFIG_TEST;
            break;
        case 'v':
            WebServer::state_ = WS_VERSION_QUERY;
            break;
        case 'i':
            // ignore (watchdog option)
            break;
        case '?':
            usage();
            return PR_FAILURE;
        }
    }

    // we're done if all we're doing is querying the version
    if (WebServer::state_ == WS_VERSION_QUERY)
        return PR_SUCCESS;

    // We need to know the config and install directories to start the server
    if (!serverConfigDirectory || !serverInstallDirectory) {
        usage();
        return PR_FAILURE;
    }

    // Get the canonical form of serverConfigDirectory
    if (util_chdir(serverConfigDirectory)) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ErrorChangingToDirXBecauseY),
                serverConfigDirectory,
                system_errmsg());
        return PR_FAILURE;
    }
    FREE(serverConfigDirectory);
    serverConfigDirectory = util_getcwd();
    if (!serverConfigDirectory) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_CannotDetermineCwdBecauseX),
                system_errmsg());
        return PR_FAILURE;
    }
    util_uri_normalize_slashes(serverConfigDirectory);

    // serverID is the name of the instance subdirectory
    util_chdir("..");
    char *serverID = util_getcwd();
    if (!serverID) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_CannotDetermineCwdBecauseX),
                system_errmsg());
        return PR_FAILURE;
    }
    util_uri_normalize_slashes(serverID);
    if (strrchr(serverID, '/'))
        serverID = strrchr(serverID, '/') + 1;

    // serverRootDirectory contains the instance directories (this particular
    // instance's instance directory is serverRootDirectory + "/" + serverID)
    util_chdir("..");
    char *serverRootDirectory = util_getcwd();
    if (!serverRootDirectory) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_CannotDetermineCwdBecauseX),
                system_errmsg());
        return PR_FAILURE;
    }
    util_uri_normalize_slashes(serverRootDirectory);

    // the server must run with its CWD set to the config directory for 
    // compatibility with existing plugins
    if (util_chdir(serverConfigDirectory)) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_ErrorChangingToDirXBecauseY),
                serverConfigDirectory,
                system_errmsg());
        return PR_FAILURE;
    }

    // Populate the conf_global_vars_s with the instance ID and server
    // directories
    globals->Vserver_id = serverID;
    globals->Vserver_root = serverRootDirectory;
    globals->Vnetsite_root = serverInstallDirectory;

    // Were we started by the watchdog?
#ifdef XP_UNIX
    globals->started_by_watchdog = (getenv("WD_STARTED") != NULL);
#else
    // generate a unique name from the server config directory and use it
    // for all server to watchdog communication
    set_uniquename(serverConfigDirectory);
    HANDLE hwnd = FindWindow(WC_WATCHDOG, get_uniquename());
    if (hwnd)
        globals->started_by_watchdog = 1;
#endif
    globals->restarted_by_watchdog = (getenv("WD_RESTARTED") != NULL);

#if defined(PUMPKIN_HOUR)
    // check pumpkin hour
    if (time(NULL) > PUMPKIN_HOUR + 651) {
        ereport(LOG_FAILURE, "This beta version has expired");
        return PR_FAILURE;
    }
#endif

    // initialize global synchronization stuff
    auth_init_crits();
#ifdef XP_UNIX
    ntrans_init_crits();
#endif
    reqlimit_init_crits();

    // init ACLs (part 1)
    if (ACL_InitHttp()) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ACLInitHttpFailed));
        return PR_FAILURE;
    }

    // initialize the string database
    NSString stringDatabaseDirectory;
    stringDatabaseDirectory.append(serverInstallDirectory);
    stringDatabaseDirectory.append("/" PRODUCT_RES_SUBDIR);
    XP_InitStringDatabase(stringDatabaseDirectory, DATABASE_NAME);

    // Initialize the XML parser
    XMLPlatformUtils::Initialize();
    XPathEvaluator::initialize();

    // Parse server.xml
    try {
        serverXML = ServerXML::parse(globals->vs_config_file);
    } catch (const EreportableException& e) {
        ereport_exception(e);
        return PR_FAILURE;
    }

    // Set user (can be overriden on command line)
    if (!serverUser) {
        if (serverXML->server.getUser())
            serverUser = STRDUP(*serverXML->server.getUser());
    }
    if (serverUser) {
#ifdef XP_UNIX
        if (geteuid() == 0) {
            // we're root
            char *pwline = (char *)MALLOC(DEF_PWBUF);
            globals->Vuserpw = (struct passwd *)PERM_MALLOC(sizeof(struct passwd));
            setpwent();
            /* getpwnam_r fills in the pw struct with pointers into pw line */
            if (!util_getpwnam(serverUser, globals->Vuserpw, pwline, DEF_PWBUF)) {
                ereport(LOG_MISCONFIG,
                        XP_GetAdminStr(DBT_CannotFindUserX),
                        serverUser);
                FREE(globals->Vuserpw);
                FREE(pwline);
                return PR_FAILURE;
            }
        }
#endif
    }

    // Set temporary directory (can be overriden on command line)
    if (!serverTemporaryDirectory)
        serverTemporaryDirectory = STRDUP(serverXML->getTempPath());
    if (serverTemporaryDirectory)
        system_set_temp_dir(serverTemporaryDirectory);

    // Set pid file path
    NSString pidPath;
    pidPath.printf("%s/pid", system_get_temp_dir());
    globals->Vpidfn = PERM_STRDUP(pidPath);

    // Initialize various conf_global_vars_s members
    conf_init_true_globals(serverXML->server);

    // Set default UI language
    XP_SetLanguageRanges(XP_LANGUAGE_SCOPE_PROCESS,
                         XP_LANGUAGE_AUDIENCE_DEFAULT,
                         serverXML->server.localization.defaultLanguage);

    // Configure error logging
    if (ConfigureLogging(serverXML->server.log) != PR_SUCCESS)
        return PR_FAILURE;

    // Check whether we're a 32-bit or 64-bit process
    int actualBits = sizeof(void *) * 8;
    ereport(LOG_VERBOSE, "running as a %d-bit process", actualBits);

    // Check whether the user wanted a 32-bit or 64-bit process
    int configuredBits;
    if (serverXML->server.platform == ServerXMLSchema::Platform::PLATFORM_64) {
        configuredBits = 64;
    } else {
        configuredBits = 32;
    }
    if (configuredBits != actualBits) {
        ereport(LOG_MISCONFIG,
                XP_GetAdminStr(DBT_Platform_UnsupportedBitiness),
                configuredBits, actualBits);
    }

    nCPUs_ = PR_GetNumberOfProcessors();
    ereport(LOG_VERBOSE, "detected %d processor(s)", nCPUs_);
 
    // Ensure we have access to the temporary directory
    if (CheckTempDir() != PR_SUCCESS)
        return PR_FAILURE;

    const ServerXMLSchema::Jvm *jvm = serverXML->server.getJvm();
    PRBool jvm_enabled = (jvm && jvm->enabled);
    // Parse magnus.conf
    if (conf_parse(PRODUCT_MAGNUS_CONF, jvm_enabled) != PR_SUCCESS)
        return PR_FAILURE;

#ifdef XP_UNIX
    // Clear LD_PRELOAD to avoid problems exec()'ing a setuid Cgistub
    clearenv("LD_PRELOAD");
    clearenv("LD_PRELOAD_32");
    clearenv("LD_PRELOAD_64");
#endif

    // Set the PATH based on the server.xml jvm element (on Unix, the start
    // script sets up the environment and this call is a noop)
    if (jvm_enabled)
        jvm_set_libpath(serverXML->server);

    // On Windows, update the PATH environment variable based on the 
    // user specified server root.  This will set the environment for 
    // NSAPI plugins to be dynamically loaded by specifying the relative 
    // path in the magnus.conf.
#ifdef XP_WIN32
    char serverLibPath[MAX_PATH];
    util_sprintf(serverLibPath,"%s/%s", globals->Vnetsite_root,PRODUCT_LIB_SUBDIR);
    if (!server_set_libpath(serverLibPath)) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CannotExportServerPathX), serverLibPath);
    }
#endif

    // Initialize SmartHeap.  This must be done before we create any threads.
#ifdef USE_SMARTHEAP
    SmartHeapInit();
#endif

    // Install a process-wide handler to detect failed calls to operator new
    if (conf_getboolean("SetNewHandler", PR_TRUE))
        system_setnewhandler();

    // Find the default CPU affinity mask for this process. This currently only
    // works on Windows.
    PRUint32 affinity = 0;
    PR_GetThreadAffinityMask(PR_CurrentThread(), &affinity);
    if (affinity != 0) {
        // Users can specify CPU(s) we shouldn't use (e.g. the CPU that handles
        // IO) by specifying an IoProcessorAffinity mask or 0-based IoProcessor
        // index
        PRUint32 maskIoProcessor = conf_getinteger("IoProcessorAffinity", 0);
        PRUint32 iIoProcessor = conf_getboundedinteger("IoProcessor", -1, 31, -1);
        if (iIoProcessor != -1)
            maskIoProcessor |= (1 << iIoProcessor);
        maskIoProcessor &= affinity;

        // If there are CPU(s) we aren't supposed to use...
        if (maskIoProcessor) {
            // Never disable all available CPUs
            PRUint32 affinityNew = affinity & ~maskIoProcessor;
            if (affinityNew == 0)
                affinityNew = affinity;

            ereport(LOG_VERBOSE, "setting processor affinity to %d of %d",
                    bitcount(affinityNew),
                    bitcount(affinity));

            // Adjust our affinity to avoid using the IO processor CPU(s)
            affinity = affinityNew;
            PR_SetThreadAffinityMask(PR_CurrentThread(), affinity);
        }

        // On Windows, Concurrency configures the number of Windows threads
        // used to execute NSPR user threads
        int cConcurrency = bitcount(affinity);
        if (cConcurrency < 2)
            cConcurrency = 2;
        cConcurrency = conf_getboundedinteger("Concurrency", 1, 512, cConcurrency);
        PR_SetConcurrency(cConcurrency);
        ereport(LOG_VERBOSE, "Concurrency set to %d", cConcurrency);
    }

#ifdef XP_UNIX
    // Should we close stdin/stdout/stderr?
    fDetach_ = (getenv("WD_DETACH") != NULL);
#endif

#ifndef Linux
    WebServer::StartLoggingThreads(); 
#endif
    
    // Configure statistics collection
    if (serverXML->server.stats.enabled) {
        StatsManager::setUpdateInterval(serverXML->server.stats.interval.getSecondsValue());
        if (serverXML->server.stats.profiling)
            StatsManager::enableProfiling();
        StatsManager::enable();
    }

    // DNS enabled?  Note that any asynchronous resolver configuration is
    // delayed until after the fork as it is threaded.
    net_enabledns = serverXML->server.dns.enabled;

    // Configure ACL cache
    if (serverXML->server.aclCache.enabled) {
        ACL_SetUserCacheMaxAge(serverXML->server.aclCache.maxAge.getSecondsValue());
        ACL_SetUserCacheMaxUsers(serverXML->server.aclCache.maxUsers);
        ACL_SetUserCacheMaxGroupsPerUser(serverXML->server.aclCache.maxGroupsPerUser);
    } else {
        ACL_SetUserCacheMaxAge(0);
    }

    // Use the specified <thread-pool> stack size for all Thread objects. 
    Thread::setDefaultStackSize(serverXML->server.threadPool.stackSize);

    // Configure DaemonSession threading and timeout settings
    DaemonSession::ConfigureEarly(serverXML->server);

	const char* timoutStr = conf_findGlobal("TerminateTimeOut");
	WebServer::terminateTimeOut_ = PR_SecondsToInterval(DEFAULT_TERMINATE_TIMEOUT);
	if (timoutStr) {
		int timeout = atoi(timoutStr);
		if (timeout > -1) {
		    WebServer::terminateTimeOut_ = PR_SecondsToInterval(timeout);
        }
    }

    //
    // ChildRestartCallback on  ==> ALWAYS invoke restart functions
    //                                fForceCallRestartFns = PR_TRUE
    //                                fCallRestartFns = PR_TRUE
    //                                existingSessions = DONT_CARE
    // ChildRestartCallback off ==> NEVER invoke restart functions
    //                                fForceCallRestartFns = PR_FALSE
    //                                fCallRestartFns = PR_FALSE
    //                                existingSessions = DONT_CARE
    // <default behaviour>      ==> Invoke restart functions only if 
    // (no directive in magnus)     existingSessions == 0
    //                                fForceCallRestartFns = PR_FALSE
    //                                fCallRestartFns = PR_TRUE
    //                                existingSessions >= 0
    //
    WebServer::fForceCallRestartFns_ = conf_getboolean("ChildRestartCallback",
                                                       PR_FALSE);
    WebServer::fCallRestartFns_ = conf_getboolean("ChildRestartCallback",
                                                  PR_TRUE);
 
    // init the NSAPI function execution environment
    // needs to be run after configuration is read
    func_init(func_standard);

    // Pre-fork NSS initialization
    if (servssl_init_early(serverXML->server.pkcs11, serverXML->server.sslSessionCache))
        return PR_FAILURE;

    // This needs to be done before the early init funcs get run
    // since http-method-registry can get called as an early init
    if (HttpMethodRegistry::Init() == NULL) {
	ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_method_registry));
	return PR_FAILURE;
    }

    HttpRequest::SetStrictHttpHeaders(serverXML->server.http.strictRequestHeaders);
    HttpRequest::SetDiscardMisquotedCookies(serverXML->server.http.discardMisquotedCookies);
    HttpHeader::SetMaxRqHeaders(serverXML->server.http.maxRequestHeaders);

    LogManager::initEarly();
    LogManager::setParams(serverXML->server.accessLogBuffer);

#ifdef PUMPKIN_HOUR
    // Warn user of upcoming expiry date
    int daysRemaining = (PUMPKIN_HOUR - time(NULL)) / 86400;
    if (daysRemaining < 0) daysRemaining = 0;
    ereport(LOG_WARN, XP_GetAdminStr(DBT_ExpiresIn), daysRemaining);
#endif

    // initialize NSAPI subsystems
    http_set_protocol(serverXML->server.http.version);
    if (strlen(serverXML->server.http.serverHeader))
        http_set_server_header(serverXML->server.http.serverHeader);
    http_enable_etag(serverXML->server.http.etag);
    httpfilter_set_default_output_buffer_size(serverXML->server.http.outputBufferSize);
    http_set_max_unchunk_size(serverXML->server.http.maxUnchunkSize);
    http_set_unchunk_timeout(serverXML->server.http.unchunkTimeout.getPRIntervalTimeValue());
    filter_init();
    httpfilter_init();
    headerfooter_init();
    http_compression_init();
#ifdef FEAT_SECRULE
    secrule_filters_init();
#endif
    sed_init();
    var_init();
    control_init();
    error_init();
    route_init();
    channel_init();
    reverse_init();
    httpclient_init();
    shtml_init_early();
    favicon_init(serverXML->server.http.favicon);
#ifdef FEAT_GSS
    gssapi_init();
#endif

    // we're done if all we're doing is testing the configuration
    if (WebServer::state_ == WS_CONFIG_TEST)
        return PR_SUCCESS;

    // Set CGI defaults prior to running Init SAFs
#ifdef XP_UNIX
    if (serverXML->server.cgi.getCgistubPath())
        cgistub_set_path(*serverXML->server.cgi.getCgistubPath());
    cgistub_set_idle_timeout(serverXML->server.cgi.cgistubIdleTimeout.getPRIntervalTimeValue());
    cgistub_set_min_children(serverXML->server.cgi.minCgistubs);
    cgistub_set_max_children(serverXML->server.cgi.maxCgistubs);
#endif
    cgi_set_timeout(serverXML->server.cgi.timeout.getPRIntervalTimeValue());
    cgi_set_idle_timeout(serverXML->server.cgi.idleTimeout.getPRIntervalTimeValue());
    for (int i = 0; i < serverXML->server.cgi.getEnvVariableCount(); i++) {
        const ServerXMLSchema::EnvVariable *var = serverXML->server.cgi.getEnvVariable(i);
        cgi_add_var(var->name, var->value);
    }

#if defined(XP_UNIX)

    // Set the FD limit to the max
    maxfd_set(maxfd_getmax());
    
    // Initialize the UNIX signals subsystem
    // (needs to be done before the first thread starts & possibly before the 
    // init funcs)
    // this just sets up the infrastructure and blocks all the interesting 
    // signals
    UnixSignals::Init();
    if (conf_getboolean("LogCrashes", PR_TRUE)) {
        UnixSignals::CatchCrash(SIGSEGV);
        UnixSignals::CatchCrash(SIGBUS);
        UnixSignals::CatchCrash(SIGILL);
        UnixSignals::CatchCrash(SIGFPE);
    }

    //
    // open connection to watchdog
    //
    if (WatchdogClient::init() == PR_FAILURE) {
	ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_watchdog_init));
	return PR_FAILURE;
    }

    // If the watchdog isn't tracking our pid...
    if (!globals->started_by_watchdog) {
        // create pid file myself
        CreatePidFile();
    }
#endif

    //
    // Controls whether SO_REUSEADDR/SO_EXCLUSIVEADDRUSE are used for LSs
    //
    ListenSocket::setReuseAddr(conf_getstring("ReuseAddr", "true"));

    //
    // If the KernelThreads property has been set to a non-zero value in
    // magnus.conf then force all Thread instances to be created as
    // PR_GLOBAL_BOUND threads
    //
    PRBool bGlobalThreadsOnly = conf_getboolean("KernelThreads", PR_FALSE);
    Thread::forceGlobalThreadCreation(bGlobalThreadsOnly);

    return PR_SUCCESS;
}

PRStatus
WebServer::Run()
{
    char *err;
#ifdef XP_UNIX
    uid_t uid;
#endif

    // we're done if all we're doing is querying the version
    if (WebServer::state_ == WS_VERSION_QUERY) {
        printf(PRODUCT_ID" "PRODUCT_FULL_VERSION_ID" B"BUILD_NUM"\n");
        return PR_SUCCESS;
    }

    // on NT, from this point on, things would run in a thread
    // (daemon_run used to have pretty much this functionality)

    InitializeExitVariable();

    net_init(globals->Vsecurity_active);

    // alloc & set thread malloc key (should be done locally!! XXX)
    InitThreadMallocKey();

    // run early NSAPI init functions
    //
    // these always run once per server instance, in a single process,
    // in a non-threaded environment
    //
    if (conf_run_early_init_functions() != PR_SUCCESS)
        return PR_FAILURE;

    if (WebServer::state_ != WS_CONFIG_TEST) {
        // Initialize the statistics subsystem. Must be done after calling
        // WebServer::ProcessServerXML. WebServer::init call ProcessServerXML
        // which initializes the stats configuration variables. If stats are
        // not enabled StatsManager::initEarly does nothing and simply return.
        // Previous versions of webserver used stats-init saf to enable stats.
        StatsManager::initEarly(globals->Vpool_max, DaemonSession::GetMaxSessions());

#if defined(XP_WIN32) || defined(DEBUG_SINGLEPROCESS)
        // We're the lone process
        WebServer::fFirstBorn_ = PR_TRUE;
        StatsManager::findProcessSlot();
        StatsManager::activateProcessSlot(getpid());
        new StatsRunningThread;
#endif
    }

#ifdef XP_UNIX
    // ----------------------------------------------------------------------
    // lookee mom, we're forking! whee!!!
    // ----------------------------------------------------------------------

    // signal setup for primordial process
    UnixSignals::Catch(SIGCHLD);
    UnixSignals::Catch(SIGTERM);
    UnixSignals::Catch(SIGINT);
    UnixSignals::Catch(SIGHUP);

#if !defined(DEBUG_SINGLEPROCESS)
    if (WebServer::state_ != WS_CONFIG_TEST) {
        StopLoggingThreads();

        // Always reopen the error log before writing in the primordial as
        // someone may have rotated it
        ereport_set_alwaysreopen(PR_TRUE);

	(void)ForkChildren(globals->Vpool_max);         // XXX error handling


	// now we are in multiple child processes
	//  so we can create threads etc.

        // Keep the error log fd open
        ereport_set_alwaysreopen(PR_FALSE);

        StartLoggingThreads();
    }
#endif

    //
    // signal setup for worker processes
    //
    // this is the same for single and multi process mode
    // every worker process gets its own signal watching thread
    // which will handle things nicely in a multithreaded
    // environment

    UnixSignals::Ignore(SIGUSR1);       // JVM uses USR1 to suspend threads
    UnixSignals::Ignore(SIGUSR2);
    UnixSignals::Ignore(SIGPIPE);
    UnixSignals::Catch(SIGHUP);
    UnixSignals::Catch(SIGINT);
    UnixSignals::Catch(SIGTERM);
    UnixSignals::Default(SIGCHLD);

    if (WebServer::state_ != WS_CONFIG_TEST) {
        if (globals->started_by_watchdog) {
            // close old WD connection && reopen a new one for every child 
            // process
            if (WatchdogClient::reconnect() == PR_FAILURE) {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_reopen_wd));
                return PR_FAILURE;
            }
        }
    }
#endif // XP_UNIX

    serverLock_ = new CriticalSection();
    if (!serverLock_)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_mutex_lock));
        return PR_FAILURE;
    }

    server_ = new ConditionVar(*serverLock_);
    if (!server_)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_cond_var));
        return PR_FAILURE;
    }

    // Handle post-fork NSS initialization
    // must be done in the children because some hardware SSL vendors
    // don't support forking after their stuff has been set up
    // this will cause the WD to prompt for the passwords,
    // or, in case we have no WD, the server prompts for the passwords 
    // itself (which should prove to be lots of fun in multiprocess mode...)
    if (servssl_init_late(fdConsole_, serverXML->server.pkcs11))
        return PR_FAILURE;

    // Generate the private key used by Digest Authentication
    GenerateDigestPrivatekey();

    // ACL post-magnus init
    // XXX need to delay the opening of the databases until after the fork
    if (ACL_InitHttpPostMagnus()) {
	ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_acl_init));
	return PR_FAILURE;
    }

    // Store MD5 fingerprints for those configuration files whose contents we
    // don't process once the server is up and running
    hashfile(PRODUCT_MAGNUS_CONF, magnusConfFingerprint, sizeof(magnusConfFingerprint));
    hashfile(CERTMAP_CONF, certmapConfFingerprint, sizeof(certmapConfFingerprint));
    hashfile(CERTN_DB, certDbFingerprint, sizeof(certDbFingerprint));
    hashfile(KEYN_DB, keyDbFingerprint, sizeof(keyDbFingerprint));
    hashfile(SECMOD_DB, secmodDbFingerprint, sizeof(secmodDbFingerprint));

    // Get the initial configuration object.  Do this before we setuid() so
    // the server is free to drop privileges required to access its own config.
    Configuration *configuration = NULL;
    try {
        if (fRespawned_) {
            // Child was respawned, so primordial serverXML may be outdated
            configuration = Configuration::parse();
        } else {
            // Use the primordial serverXML object
            configuration = Configuration::create(serverXML, PR_FALSE);
        }
    } catch (const EreportableException& e) {
        ereport_exception(e);
        return PR_FAILURE;
    }

    // Initialize the configuration subsystems
    ConfigurationManager::init();
    VSManager::init();
    StatsManager::initLate();

    // Allow default UI language to be dynamically reconfigured
    XP_RegisterGetLanguageRangesCallback(XP_LANGUAGE_SCOPE_PROCESS,
                                         XP_LANGUAGE_AUDIENCE_DEFAULT,
                                         &default_language_callback,
                                         NULL);

    // Installing the initial configuration is a little different from
    // dynamic reconfiguring due to the possibility of ConfigurationListeners
    // being installed when the late init functions are run. Hence the
    // need to invoke setConfiguration twice

    // This configuration is ready, install it
    if (ConfigurationManager::setConfiguration(configuration) != PR_SUCCESS) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_set_conf));
        return PR_FAILURE;
    }

    // Warn about obsolete configuration files
    CheckObsoleteFile("nsfc.conf");
    CheckObsoleteFile("password.conf");
    CheckObsoleteFile("dbswitch.conf");
    CheckObsoleteFile("../userdb/dbswitch.conf");
    CheckObsoleteFile("../userdb/certmap.conf");
    CheckObsoleteFile("cert7.db");

#if defined(XP_UNIX)
    // we're still root here
    // chroot if required, then setuid to server user
    uid = geteuid();

    if (uid || globals->Vuserpw == NULL) {
        setpwent();
        if(!(globals->Vuserpw = getpwuid(uid))) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_couldNotDetermineCurrentUserName_));
            return PR_FAILURE;
        }
	endpwent();
    }

    //
    // If Chroot is set in magnus.conf, we do that here, just before
    // changing to the server uid.
    //
    if (globals->Vchr) {
        if (chroot(globals->Vchr) < 0) {
            char *oserr = system_errmsg();
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_errorChrootToSFailedSN_),
                         globals->Vchr, oserr);
            return PR_FAILURE;
        }
    }

    if (geteuid() == 0) {
	// we're still root. so change to server user now.
#if defined(Linux)
        StopLoggingThreads();
#endif

        if(setgid((gid_t)globals->Vuserpw->pw_gid) == -1)
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_warningCouldNotSetGroupIdToDSN_),
                    (int)globals->Vuserpw->pw_gid, system_errmsg());
        else
            initgroups(globals->Vuserpw->pw_name, globals->Vuserpw->pw_gid);

        if(globals->Vuserpw->pw_uid) {
            if(setuid((uid_t)globals->Vuserpw->pw_uid) == -1)
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_warningCouldNotSetUserIdToDSN_),
                        (int)globals->Vuserpw->pw_uid, system_errmsg());
        }
        else
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_warningDaemonIsRunningAsSuperUse_));

#if defined(Linux)
        StartLoggingThreads();
#endif
    }

    // Ensure we have access to the temporary directory
    if (CheckTempDir() != PR_SUCCESS) {
        return PR_FAILURE;
    }

#endif // defined(XP_UNIX)

    date_init();

    // Determine the number of file descriptors to use for filecache
    int nFileCacheMaxOpenFiles = getdescriptors(configuration->fileCache.getMaxOpenFiles());

    // If one is not defined in server.xml, then create one with maxEntries
    // This will later be set to the correct value after the plugins have been
    // initialized
    if (nFileCacheMaxOpenFiles == -1) {
        nFileCacheMaxOpenFiles = configuration->fileCache.maxEntries;
    }

    // Configure the file cache
    if (nsfc_cache_init(configuration->fileCache, nFileCacheMaxOpenFiles) != PR_SUCCESS) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_nsfc_init));
        return PR_FAILURE;
    }

    if (configuration->dnsCache.enabled) {
        int entries = configuration->dnsCache.maxEntries;
        int timeout = configuration->dnsCache.maxAge.getSecondsValue();
        if (dns_cache_init(entries, timeout)) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_dns_cache_init));
            return PR_FAILURE;
        }
    }

    http_init();
    ntrans_init();
    filtersafs_init();
    child_init();
    flex_init_late();
#ifdef FEAT_SECRULE
    secrule_filters_init_late();
#endif

    if (!HttpHeader::Initialize()) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_httpheader_init));
        return PR_FAILURE;
    }

    if (!HttpRequest::Initialize()) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_httprequest_init));
        return PR_FAILURE;
    }

#ifdef MCC_PROXY
#if defined(XP_UNIX)
    // initialize the proxy filter subsystem locks
    prefilter_subsystem_init();
#endif
#endif /* MCC_PROXY */
#ifdef FEAT_SOCKS
    sockslayer_init();
#endif

#if defined(XP_UNIX)
    // Start the first Cgistub process
    if (configuration->cgi.minCgistubs > 0) {
        if (cgistub_init() != PR_SUCCESS)
            return PR_FAILURE;
    }

    // for NT, this is done as part of func_init - should we do it the same as 
    // on UNIX?
    if (func_native_pool_init() != 0) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_func_init));
	return PR_FAILURE;
    }
#endif

    // initialize the clock thread
    // (was in ntmain.cpp for NT)
    nstime_init();

    /* Initialize the asynchronous DNS thread */
    if (configuration->dns.async) {
#ifndef XP_WIN32
        PR_AR_Init(configuration->dns.timeout.getPRIntervalTimeValue());
#else
        // jpierre - the async DNS library doesn't work on NT so don't initialize it ...
        ereport(LOG_WARN, XP_GetAdminStr(DBT_WebServer_asyncdns_notsupp));
#endif
    }

    // we're done if all we're doing is testing the configuration
    if (WebServer::state_ == WS_CONFIG_TEST) {
        return PR_SUCCESS;
    }

#if defined(XP_WIN32)
    // Call scheduler_rotate to lotate log files when EventName is signalled
    char EventName[1024];
    util_snprintf(EventName, sizeof(EventName), "%s.MoveLog", get_uniquename());
    if ((err = add_rotation_handler(EventName, &scheduler_rotate, NULL))) {
	FREE(err);
	return PR_FAILURE;
    }
#endif

    BandwidthManager::init();


    // now run the late init functions

    // The late init functions have a place on Unix where they are called
    // after the fork (ie in the child only)
    // On NT since there is no forking, they are called just after the init
    // functions have been run

    // VB: to get hold of the filecache it needs to be first created
    // Call the internal init routines before calling the late init routines

    if (conf_run_late_init_functions(configuration) != PR_SUCCESS)
        return PR_FAILURE;

    // install the same configuration again so that any ConfigurationListeners
    // registered during late init are called (e.g. via vs_register_cb())
    if (ConfigurationManager::setConfiguration(configuration) != PR_SUCCESS) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_set_conf));
        return PR_FAILURE;
    }

    // Now that all plugins have been loaded and their configuration callbacks
    // have been invoked, figure out how many file descriptors the server
    // should use for the connection queue, keep-alive, and NSFC based on how
    // many are left
    if (ConfigureFileDescriptorLimits(configuration) == PR_FAILURE)
        return PR_FAILURE;

    // Initialize the various NSAPI subsystems that rely on the file cache
    http_init_late();
    shtml_init_late();
    accel_init_late();

    // Initialize the DaemonSession infrastructure including the keep-alive
    // subsystem and the connection queue
    if (DaemonSession::Initialize() == PR_FAILURE) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_ds_init));
        return PR_FAILURE;
    }

    // Register the ListenSockets instance with the ConfigurationManager so
    // that it is notified when new Configurations are installed.  We want to
    // be registered last to ensure we don't close a listen socket only to have
    // another ConfigurationListener reject that configuration.
    ListenSockets *ls = ListenSockets::getInstance();
    ConfigurationManager::addListener(ls);

    // Begin accepting connections on the listen sockets
    if (ls->setConfiguration(configuration, NULL) == PR_FAILURE)
        return PR_FAILURE;

    // ConfigurationManager now has the only reference to configuration
    configuration->unref();
    configuration = 0;

    LogManager::initLate();
    route_init_late();

    // Warn about unaccessed and duplicate magnus.conf directives
    conf_warnunaccessed();
    conf_warnduplicate();

    // ========================================================================
    // ========================================================================
    // ========================================================================

    // We're done with initialization and ready to start the processing threads

    // Tell the request subsystem we're going threaded now
    request_server_active();

    // Start the thread that collects stats
    StatsManager::startPollThread();

    // Start up the intial set of DaemonSession threads
    PRUint32 nThreads = DaemonSession::PostThreads();
    ereport(LOG_VERBOSE, "Started %d request processing threads", nThreads);

    // Log successful server startup
    ereport(LOG_INFORM, XP_GetAdminStr(DBT_successfulServerStartup_));

#ifdef XP_UNIX
    // Close stdin, stdout, stderr only if started by watchdog
    if (fDetach_) {
        redirectStream(0);
        if (!logStdout_)
            redirectStream(1);
        if (!logStderr_)
            redirectStream(2);
        if (fdConsole_)
            redirectStream(PR_FileDesc2NativeHandle(fdConsole_));
        fdConsole_ = NULL;
    }
#endif

    WebServer::state_ = WebServer::WS_RUNNING;

#if defined(XP_UNIX) && !defined(DEBUG_SINGLEPROCESS)
    // Start the thread that processes Admin messages (from the primordial
    // process). The init() method sends a message to the 
    // the primordial process that this child has finished initializing
    childAdminThread = new ChildAdminThread();
    if (childAdminThread->init() == PR_FAILURE)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_admin_thread));
        return PR_FAILURE;
    }
    childAdminThread->start(PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD);
#endif

#ifdef XP_WIN32
    // Tell the watchdog we're up and running
    HWND hwnd = FindWindow(WC_WATCHDOG, get_uniquename());
    if (hwnd)
        SetWindowLong(hwnd, GWL_SERVER_STATUS, SERVER_STATUS_RUNNING);
    StatsManager::setChildInitialized();
#endif

    // The event scheduler runs in the first child process
    if (WebServer::fFirstBorn_)
        ft_register_cb(&scheduler_clock, NULL);

    //
    // Wait for termination
    //
    // The only ways to get us out of this are
    // a) a crash
    // b) a signal
    // c) WebServer::Terminate()
    //
    // we prefer the latter, of course :-)
    //
    // WebServer::Terminate is supposed to make sure that all sessions are
    // dead
    //
    {
        SafeLock guard(*serverLock_);

#ifdef XP_UNIX
        PRIntervalTime child_poll_interval = PR_MillisecondsToInterval(100);
        while (WebServer::isTerminating() == PR_FALSE) {
            server_->timedWait(child_poll_interval);

            // Check for pending signals
            for (;;) {
                int signum = UnixSignals::Get();
                if (signum == -1)
                    break;

                ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXSigHndlrThrdRecvSignal), signum);

                switch (signum) {
                case SIGHUP:
                case SIGINT:
                case SIGTERM:
                    WebServer::state_ = WebServer::WS_TERMINATING;
                    break;
                }
            }
        }
#else
        while (WebServer::isTerminating() == PR_FALSE)
            server_->wait();
#endif
    }

    // ========================================================================
    // ========================================================================
    // ========================================================================

    // TERMINATION TIME
    // we're on our way out now....

    DaemonSession::Terminate();

    // For historical reasons, we allow NSAPI plugins to have restart
    // callback routines, but not shutdown callbacks.  Since the restart 
    // handling was changed to do a stop/start, we'll be nice guys and call
    // the restart handlers here, since they primarily just shut things 
    // down nicely.
    // VB: if exitingSessions is not 0, then there are still requestor threads
    // doing active stuff and calling restart functions is not correct
    // This is because historically they have been called in a single threaded
    // environment and do not do ref counting
    // The user can force us to do this by setting ChildRestartCallback yes
    // in magnus
    // 
    //
    // ChildRestartCallback on  ==> ALWAYS invoke restart functions
    //                                fForceCallRestartFns = PR_TRUE
    //                                fCallRestartFns = PR_TRUE
    //                                existingSessions = DONT_CARE
    // ChildRestartCallback off ==> NEVER invoke restart functions
    //                                fForceCallRestartFns = PR_FALSE
    //                                fCallRestartFns = PR_FALSE
    //                                existingSessions = DONT_CARE
    // <default behaviour>      ==> Invoke restart functions only if 
    // (no directive in magnus)     existingSessions == 0
    //                                fForceCallRestartFns = PR_FALSE
    //                                fCallRestartFns = PR_TRUE
    //                                existingSessions >= 0
    //
    PRInt32 existingSessions = DaemonSession::GetTotalSessions();
    if (WebServer::fForceCallRestartFns_ || existingSessions == 0) {
        if (WebServer::fCallRestartFns_) {
            //
            // Either "ChildRestartCallback on" has been specified in 
            // magnus.conf or the directive is not specified at all in magnus
            //
            log_ereport(LOG_VERBOSE, "Before calling restart functions");
            daemon_dorestart();
            log_ereport(LOG_VERBOSE, "After calling restart functions");
        }
    } else {
        log_ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_researt_active), existingSessions);
    }

    StatsManager::terminateProcess();

#if defined(XP_UNIX) && !defined(DEBUG_SINGLEPROCESS)
    // stop the admin thread in the child process
    if (childAdminThread != NULL)
    {
        delete childAdminThread;
        childAdminThread = NULL;
    }
#else
    // On Unix the primordial process calls StatsManager::terminateServer()
    StatsManager::terminateServer();
#endif

    // Flush any outstanding log buffers
    LogManager::terminate();

    WebServer::Cleanup();

    // it's ok to exit now
    NotifyExitVariable();

    return PR_SUCCESS;
}

void
WebServer::Terminate(void)
{
    // wave the jolly roger. Every thread must break out of its processing
    // loop when WebServer::isTerminating() return PR_TRUE
    WebServer::state_ = WebServer::WS_TERMINATING;
    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxSetTerminateFlag));

    // this causes the Run logic to finish
    if ((serverLock_ != NULL) && (server_ != NULL))
    {
        SafeLock guard(*serverLock_);   // serverLock_ acquired
        server_->notify();
    }                                   // serverLock_ released

    // Wait for official signal that server is dead
    WaitForExitVariable();
}

#ifdef XP_UNIX
PRBool
WebServer::CreatePidFile()
{
    char buffer[64];
    int length;
    char *fn = globals->Vpidfn;

    if (fn == NULL || *fn == '\0')
	return PR_TRUE;

    SYS_FILE f = system_fopenRW(fn);
    if(!f || f == SYS_ERROR_FD) {
	ereport(LOG_FAILURE, XP_GetAdminStr(DBT_daemonCanTLogPidToSSN_), fn, 
                     system_errmsg());
	return PR_FALSE;
    }
    length = util_sprintf(buffer, "%d\n", (int)getpid());
    system_fwrite(f, buffer, length);
    system_fclose(f);

    return PR_TRUE;
}

PRBool
WebServer::RemovePidFile()
{
    char *fn = globals->Vpidfn;

    if (fn == NULL || *fn == '\0')
	return PR_TRUE;

    unlink(fn);
    return PR_TRUE;
}

PRBool
WebServer::ForkChildren(int num_processes)
{
    int	_num_forks = 0;
    int	_num_exits = num_processes;
    PRBool _main_terminating = PR_FALSE;
    PRBool _wait_for_children = PR_TRUE;
    PRIntervalTime admin_poll_interval = PR_MillisecondsToInterval(100);
    int signum;
    pid_t pid;
    pid_t *_process_array = 0;
    int status;
    int index;

    // Create ParentAdmin which processes Admin messages from both the 
    // watchdog (if there is one) and the server instance (child) processes
    ParentAdmin *admin = 0;
    if ( StatsManager::isEnabled() == PR_TRUE )
    {
        admin = new ParentStats(num_processes);
    }
    else
    {
        admin = new ParentAdmin(num_processes);
    }

    if (admin->init() != PR_SUCCESS) {
        exit(1);
    }

    if (! globals->started_by_watchdog)
    {
        // jpierre 1/5 : if we were NOT started by the watchdog, let's become the
        // process group leader. When shutdown problems occur, this will allow us to
        // radically kill everything spawned by the server
#if defined(OSF1)
        (void) setpgid(0,0);
#else
        (void) setpgrp();
#endif
    }

    // create the process array
    _process_array = (pid_t *)PERM_MALLOC(sizeof(pid_t) * num_processes);
    if (!_process_array)
    {
	log_ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_DaemonPxUXProcessArrayAllocFail));
	return PR_FALSE;
    }
    memset(_process_array, 0, sizeof(pid_t) * num_processes);

    // act like a rabbit:
    // keep forking off the kids until it's time to die
    while (_main_terminating == PR_FALSE) {
        while ((_main_terminating == PR_FALSE) && (_num_forks < _num_exits)) {
            StatsManager::findProcessSlot();

            int index;
            for (index = 0; index < num_processes; index++) {
                if (_process_array[index] == 0)
                    break;
            }
            if (index >= num_processes)
                log_ereport(LOG_FAILURE, XP_GetAdminStr(DBT_DaemonPxUXProcessArrayOverFlow));

            /* YES- we really do want fork rather than fork1.
             * fork1() on Solaris isn't thread safe.  If any
             * thread holds a lock when you call fork1() (even if that lock is
             * in libc!!) then the process can deadlock.  Sun claims that 
             * their libc is immune to this, but I've personally seen it 
             * hang very easily on 2.5.
             */
            switch ((pid = fork())) {
	    case 0:
		// THE CHILD 
                WebServer::state_ = WebServer::WS_INITIALIZING;
                WebServer::fRespawned_ = (_num_forks >= num_processes);
                WebServer::fFirstBorn_ = (index == 0);
                delete admin;
		//
		// return from this horrible function and go on with our merry little worker process ways
		return PR_FALSE;

	    case -1:
		ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_fork),  system_errmsg());
		break;

	    default:
		// THE PARENT

                WebServer::state_ = WebServer::WS_RUNNING;
                log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXSpawnedWorkerProcess), pid);

                admin->processChildStart(pid);
                StatsManager::activateProcessSlot(pid);

		// Record the pid in the process array
		_process_array[index] = pid;

		_num_forks++;
		break;
            }
        }

	// all children we needed to fork off are forked off

        // Close stdin, stdout, stderr only if started by watchdog
        if (fDetach_) {
            redirectStream(0);
            if (!logStdout_)
                redirectStream(1);
            if (!logStderr_)
                redirectStream(2);
            if (fdConsole_)
                redirectStream(PR_FileDesc2NativeHandle(fdConsole_));
            fdConsole_ = NULL;
        }

        // Do the late initialization of admin channel after all
        // the children are forked off.
        admin->initLate();
        //
	// now, just wait for a signal or message to arrive
        // (this is where the primordial process sits in a running server)
        //
        while (_main_terminating == PR_FALSE) {
            // Update server running time
            // StatsManager::updateSecondsRunning();
            StatsManager::updateStats();

            // Get out if there's a signal to process
            signum = UnixSignals::Get();
            if (signum != -1) break;

            // Process watchdog/child messages
            admin->poll(admin_poll_interval);
        }

        if (_main_terminating == PR_TRUE)
            break;

        // There's a signal to process
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXSigHndlrThrdRecvSignal), signum);

	switch (signum) {
	case SIGCHLD:
            //
            // a child died
            //
	    while ( (pid = waitpid((pid_t)-1, &status, WNOHANG)) > 0) {
                // Check if the child that died was one of ours; if
                // so, spawn a new one
                //
                int index;
                PRBool  isChild = PR_FALSE;

		log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXDetectedChildDeath), pid, status);
                // see if it's one of our worker children
                for (index = 0; index < num_processes; index++) {
                    if (_process_array[index] == pid) {
                        _process_array[index] = 0;
                        // tell the main loop we need to fork another one
                        _num_exits++;
                        isChild = PR_TRUE;
                        break;
                    }
                }
		if (isChild == PR_TRUE) {
                    // ok, so it was one of our direct children that died
                    if (!ParentAdmin::isChildInitDone()) {
                        //
                        // a worker child died before anybody could successfully finish initialization
                        // do not respawn, but kill off all others (they will probably die anyway, but we'll
                        // make sure the whole breed is dead)
                        //
                        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXChildDiedBeforeInitComplete));
                        // Do not pass the signal on to our children as they
                        // probably have a JVM and there are issues sending
                        // SIGTERM to Solaris JVMs.  Instead, send a message.
                        if (admin->terminateChildren() != PR_SUCCESS)
                            _wait_for_children = PR_FALSE;
                        _main_terminating = PR_TRUE;
                    } else {
                        //
                        // a worker child died after at least one child could successfully initialize
                        // so let's respawn it.
                        //
                        log_ereport(LOG_INFORM, XP_GetAdminStr(DBT_DaemonPxUXWillRespawn), pid);

                        // Reset pid slot for the dead child
                        StatsManager::freeProcessSlot(pid);
                        admin->processChildDeath(pid);
                    }
		} else
		    log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXWillNotRespawn));
	    }
	    break;

	case SIGHUP:
	    log_ereport(LOG_VERBOSE, 
			XP_GetAdminStr(DBT_DaemonPxUXPrimordialRecvHangup)); 
	    
	    // pass the signal on to all of our children
	    for (index=0; index < num_processes; index++) {
		if (_process_array[index]) {
		    log_ereport(LOG_VERBOSE, 
		    XP_GetAdminStr(DBT_DaemonPxUXPrimordialSendingHangup),
				index, _process_array[index]);
		    kill(_process_array[index], SIGHUP);
		}
	    }
	    break;

    case SIGINT:
    case SIGTERM:
        // Ignore successive/multiple terminate signals (such as the kill
        // of the process group that is initiated below)
        if (!WebServer::isTerminating())
        {
            _main_terminating = PR_TRUE;
            WebServer::state_ = WebServer::WS_TERMINATING;
            log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonPxUXPrimordialRecvTerminationSignal));

            // Do not send SIGTERM to our children as they probably have a JVM
            // and there are issues sending SIGTERM to Solaris JVMs.  Instead,
            // send a message.
            if (admin->terminateChildren() != PR_SUCCESS)
                _wait_for_children = PR_FALSE;
        }
        break;

	default:
	    log_ereport(LOG_VERBOSE, 
		   XP_GetAdminStr(DBT_DaemonPxUXPrimordialRecvUnexpectedSignal),
		   signum);
	    break;
	}
    }

    //
    // this is the primordial process on its way out - someone set _main_terminating to PR_TRUE
    //

    // _wait_for_children = PR_FALSE shouldn't happen
    PR_ASSERT(_wait_for_children);

    // since we still have the kids in tow, we must wait for them
    while (_wait_for_children && (_num_exits < _num_forks + num_processes)) {
        // Check for a dead child process
        pid = waitpid((pid_t)-1, &status, WNOHANG);
        if (pid == -1) {
            // Bail if the OS says there are no child processes left (i.e.
            // someone monkeyed with SIGCHLD handling)
            if (errno == ECHILD)
                break;
        } else if (pid == 0) {
            // Child processes are still running, so check for messages
            admin->poll(admin_poll_interval);
        } else {
            // Check if the child that died was one of ours
            int index;
            for (index = 0; index < num_processes; index++) {
                if (_process_array[index] == pid) {
                    // it's one of ours
                    _process_array[index] = 0;
                    _num_exits++;
                    break;
                }
            }
        }
    }

    // .... doing the last twitches ....
    // at this point, all children are reaped

    // if we weren't started by the watchdog, it's our turn to remove
    // the pidfile
    if (!globals->started_by_watchdog)
        RemovePidFile();

    // close admin channel
    admin->unlinkAdminChannel();
    delete admin;

    // get rid of the stats file
    StatsManager::terminateProcess();
    StatsManager::terminateServer();
    
    if (globals->started_by_watchdog) {
        WatchdogClient::close();
    }

    exit(0);
}

#endif

#if defined(XP_WIN32)

#include <windows.h>

//--------------------------------------------------------------------------//
// get server config path (c:\netscape\server\httpd-nickname) from registry //
//--------------------------------------------------------------------------//
BOOL
NT_GetServerConfigAndRoot(char *szServiceName,
			char *szServerConfig, LPDWORD lpdwServerConfig,
			char *szServerRoot, LPDWORD lpdwServerRoot)
{
    BOOL bReturn = FALSE;
    HKEY hHttpdKey = 0;
    char szHttpdKey[MAX_PATH];
    DWORD dwValueType;
    DWORD dwResult = 0;
    const char *ServerKeyRoot = SVR_KEY_ROOT;
    char *temp = NULL;

    // find server key root
    if (szServiceName && (strncmp(szServiceName, "admin", 5) == 0) &&
        (strlen(szServiceName) > strlen("adminXX-")))
    {
        if (isdigit(szServiceName[5]) && isdigit(szServiceName[6]))
        {
           temp = (char *) malloc(strlen("Administration\\X.X" ) + 1);
           sprintf(temp, "Administration\\%c.%c", szServiceName[5], szServiceName[6]);
           ServerKeyRoot = temp;
        }
    }

    // query registry key to figure out server root
    wsprintf(szHttpdKey, "%s\\%s", KEY_SOFTWARE_NETSCAPE, ServerKeyRoot);
    dwResult = RegOpenKey(HKEY_LOCAL_MACHINE, szHttpdKey, &hHttpdKey);
    if(dwResult != ERROR_SUCCESS) 
	goto done;
    dwResult = RegQueryValueEx(hHttpdKey, VALUE_ROOT_PATH, (LPDWORD)NULL, &dwValueType, (LPBYTE)szServerRoot, lpdwServerRoot);
    RegCloseKey(hHttpdKey);
    if (dwResult != ERROR_SUCCESS)
	goto done;

    // query registry key to figure out config directory
    wsprintf(szHttpdKey, "%s\\%s\\%s", KEY_SOFTWARE_NETSCAPE, ServerKeyRoot, szServiceName);
    dwResult = RegOpenKey(HKEY_LOCAL_MACHINE, szHttpdKey, &hHttpdKey);
    if(dwResult != ERROR_SUCCESS)
	goto done;
    dwResult = RegQueryValueEx(hHttpdKey, VALUE_CONFIG_PATH, (LPDWORD)NULL, &dwValueType, (LPBYTE)szServerConfig, lpdwServerConfig);
    RegCloseKey(hHttpdKey);
    if(dwResult != ERROR_SUCCESS)
	goto done;

    bReturn = TRUE;

done:
    if (temp)
       free(temp);
    return(bReturn);
}
#endif

void
WebServer::InitializeExitVariable()
{
    daemon_dead = PR_FALSE;
    daemon_exit_cvar_lock = PR_NewLock();
    daemon_exit_cvar = PR_NewCondVar(daemon_exit_cvar_lock);
}

void
WebServer::WaitForExitVariable()
{
    PR_Lock(daemon_exit_cvar_lock);

    if (daemon_dead == PR_FALSE) 
        PR_WaitCondVar(daemon_exit_cvar, WebServer::terminateTimeOut_);

    if ( daemon_dead == PR_FALSE )
        log_ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_DaemonNotDeadAfterTimeOut),
                    PR_IntervalToSeconds(WebServer::terminateTimeOut_));

    PR_Unlock(daemon_exit_cvar_lock);
}

void
WebServer::NotifyExitVariable()
{
    PR_Lock(daemon_exit_cvar_lock);
    daemon_dead = PR_TRUE;
    PR_NotifyCondVar(daemon_exit_cvar);
    PR_Unlock(daemon_exit_cvar_lock);
}

#ifdef XP_WIN32

void WebServer :: SetInstance(const HANDLE instance)
{
	hinst = instance;
};

const HANDLE WebServer :: GetInstance()
{
	return hinst;
};

#endif

void
WebServer::ReopenLogs(void)
{
    LogManager::reopen();
    ereport_reopen();
}

void
WebServer::Cleanup(void)
{
    StopLoggingThreads();

    if (server_ != NULL)
    {
        delete server_;
        server_ = NULL;
    }
    if (serverLock_ != NULL)
    {
        delete serverLock_;
        serverLock_ = NULL;
    }
}

PRBool
WebServer::isTerminating(void)
{
    return (WebServer::state_ == WebServer::WS_TERMINATING);
}

PRBool
WebServer::isReconfiguring(void)
{
    return (WebServer::state_ == WebServer::WS_RECONFIGURING);
}

PRBool
WebServer::isInitializing(void)
{
    return (WebServer::state_ == WebServer::WS_INITIALIZING);
}

PRBool
WebServer::isReady(void)
{
    return (WebServer::state_ == WebServer::WS_RUNNING);
}

PRStatus
WebServer::CheckTempDir()
{
    // Create the temporary directory if it doesn't exist
    const char* tempdir = system_get_temp_dir();
    if (PR_MkDir(tempdir, 0700) == PR_SUCCESS) {
#ifdef XP_UNIX
        // We just created the temporary directory, chown it to the right user
        if (globals->Vuserpw && geteuid() == 0) {
            chown(tempdir, globals->Vuserpw->pw_uid, globals->Vuserpw->pw_gid);
        }
#endif
    }

#ifdef XP_UNIX
    struct stat finfo;
    if (system_stat(tempdir, &finfo)) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_tmp_writable), tempdir, system_errmsg());
        return PR_FAILURE;
    }

    uid_t serveruid;
    if (globals->Vuserpw) {
        serveruid = globals->Vuserpw->pw_uid;
    } else {
        serveruid = geteuid();
    }

    if (finfo.st_uid != serveruid) {
        // Directory is not owned by the server user.  This exposes us to
        // bad things like symlink games.
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_WebServer_tmp_owner), tempdir, tempdir);
        return PR_FAILURE;
    }

    if (finfo.st_mode & (S_IWOTH | S_IXOTH)) {
        // We don't want anyone else to have x permissions on this dir
        // because it's where we keep our Unix domain sockets, and Solaris
        // (and any BSD < 4.4) doesn't respect permissions on sockets on
        // connect().  We don't want anyone else to have w permissions to
        // avoid symlink games.
        ereport(LOG_SECURITY, XP_GetAdminStr(DBT_WebServer_tmp_accessible), tempdir, tempdir);
        return PR_FAILURE;
    }
#endif

    // Ensure we can write to the temporary directory
    char* tempfile = (char*)malloc(strlen(tempdir) + sizeof("/test.") + 10);
    util_sprintf(tempfile, "%s/test.%d", tempdir, getpid());
    PRFileDesc* fd = PR_Open(tempfile, PR_RDWR | PR_CREATE_FILE, 0644);
    if (!fd) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_WebServer_tmp_writable), tempdir, system_errmsg());
        return PR_FAILURE;
    }
    PR_Close(fd);
    PR_Delete(tempfile);
    free(tempfile);

    return PR_SUCCESS;
}

PRInt32 WebServer::getTerminateTimeout()
{
	return WebServer::terminateTimeOut_;
}

PRStatus
WebServer::Reconfigure(void)
{
    // N.B. WebServer::Reconfigure() should only be called by the initial
    // thread, the ChildAdminThread (Unix), or the _WaitSignalThread (Windows).
    // If any other thread wishes to trigger dynamic reconfiguration, it must
    // use WebServer::RequestReconfiguration() instead.
    PR_ASSERT(HttpRequest::CurrentRequest() == NULL);
    PR_ASSERT(conf_get_vs() == NULL);

    WebServer::state_ = WebServer::WS_RECONFIGURING;

    // Attempt to parse a new configuration
    Configuration *incoming = NULL;
    try {
        ServerXML *incomingServerXML = ServerXML::parse(globals->vs_config_file);
        if (CheckNSS(incomingServerXML))
            incoming = Configuration::create(incomingServerXML, PR_TRUE);
    } catch (const EreportableException& e) {
        ereport_exception(e);
    }

    PRStatus rv = PR_FAILURE;

    // Attempt to install the new configuration
    if (incoming != NULL) {
        Configuration *outgoing = ConfigurationManager::getConfiguration();
        PR_ASSERT(outgoing != NULL);

        rv = ConfigurationManager::setConfiguration(incoming);
        if (rv == PR_SUCCESS)
            ProcessConfiguration(incoming, outgoing);

        outgoing->unref();
        incoming->unref();
    }

    if (rv == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ConfigurationManager_NotInstalled));

    WebServer::state_ = WebServer::WS_RUNNING;

    // Reload CRLs after reconfig
    crl_check_updates(PR_FALSE);


#ifdef XP_WIN32
    StatsManager::processReconfigure();
#endif

    return rv;
}

void
WebServer::RequestReconfiguration(void)
{
#ifdef XP_WIN32
    // Poke the _WaitSignalThread
    char szReconfigEvent[MAX_PATH];
    wsprintf(szReconfigEvent, "NR_%s", get_uniquename());
    HANDLE hReconfigEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, szReconfigEvent);
    SetEvent(hReconfigEvent);
    CloseHandle(hReconfigEvent);
#else
    // Poke each process's ChildAdminThread
    childAdminThread->reconfigure();
#endif
}

void
WebServer::RequestReopenLogs(void)
{
#ifdef XP_WIN32
    ReopenLogs();
#else
    // Poke each process's ChildAdminThread
    childAdminThread->reopenLogs();
#endif
}

void
WebServer::RequestRestart(void)
{
#ifdef XP_WIN32
    // Poke the _WaitSignalThread
    char szDoneEvent[MAX_PATH];
    wsprintf(szDoneEvent, "NS_%s", get_uniquename());
    HANDLE hDoneEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, szDoneEvent);
    SetEvent(hDoneEvent);
    CloseHandle(hDoneEvent);
#else
    // Poke each process's ChildAdminThread
    childAdminThread->restart();
#endif
}

int
WebServer::GetConcurrency(const Integer *threads)
{
    PR_ASSERT(nCPUs_ > 0);

    if (threads)
        return *threads;

    return nCPUs_;
}
