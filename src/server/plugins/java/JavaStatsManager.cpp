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

#include "JavaStatsManager.h"   // JavaStatsManager
#include "base/ereport.h"       // ereport
#include "JVMControl.h"         // JVMControl
#include "NSJavaUtil.h"         // NSJavaUtil
#include "com_sun_web_monitoring_VirtualServerStats.h"
#include "com_sun_web_monitoring_WebModuleStats.h"
#include "com_sun_web_monitoring_WebModuleCacheStats.h"
#include "com_sun_web_monitoring_ServletStats.h"
#include "com_sun_web_monitoring_JdbcConnectionPoolStats.h"
#include "com_sun_web_monitoring_JvmManagementStats.h"
#include "com_sun_web_monitoring_SessionReplicationStats.h"
#include "httpdaemon/statsmanager.h"  // StatsManager
#include "httpdaemon/WebServer.h"


#define CLASS_JVM_STATS_MANAGER  "com/sun/web/monitoring/JvmStatsManager"

// Redifining the Constants from Java for shorter variable name
#define WM_COUNT_JSP_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_JSP_INDEX
#define WM_COUNT_JSP_RELOAD_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_JSP_RELOAD_INDEX
#define WM_COUNT_SESSIONS_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_SESSIONS_INDEX
#define WM_COUNT_ACTIVE_SESSIONS_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_ACTIVE_SESSIONS_INDEX
#define WM_PEAK_ACTIVE_SESSIONS_INDEX \
        com_sun_web_monitoring_WebModuleStats_PEAK_ACTIVE_SESSIONS_INDEX
#define WM_COUNT_REJECTED_SESSIONS_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_REJECTED_SESSIONS_INDEX
#define WM_COUNT_EXPIRED_SESSIONS_INDEX \
        com_sun_web_monitoring_WebModuleStats_COUNT_EXPIRED_SESSIONS_INDEX
#define WM_SESSION_MAXALIVETIME_INDEX \
        com_sun_web_monitoring_WebModuleStats_SESSION_MAXALIVETIME_INDEX
#define WM_SESSION_AVGALIVETIME_INDEX \
        com_sun_web_monitoring_WebModuleStats_SESSION_AVGALIVETIME_INDEX
#define WM_WEBMODULE_MODE_INDEX \
        com_sun_web_monitoring_WebModuleStats_WEBMODULE_MODE_INDEX
#define WEBMOD_INTDATA_LAST_INDEX \
        com_sun_web_monitoring_WebModuleStats_WEBMOD_INTDATA_LAST_INDEX

// Web module modes
#define WEBMODULE_MODE_DISABLED \
        com_sun_web_monitoring_WebModuleStats_WEBMODULE_MODE_DISABLED
#define WEBMODULE_MODE_ENABLED \
        com_sun_web_monitoring_WebModuleStats_WEBMODULE_MODE_ENABLED
#define WEBMODULE_MODE_UNKNOWN \
        com_sun_web_monitoring_WebModuleStats_WEBMODULE_MODE_UNKNOWN

// Constants for web module cache
#define WC_CACHE_TYPE_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_INDEX
#define WC_MAX_ENTRIES_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_MAX_ENTRIES_INDEX
#define WC_THRESHOLD_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_THRESHOLD_INDEX
#define WC_TABLE_SIZE_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_TABLE_SIZE_INDEX
#define WC_ENTRY_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_ENTRY_COUNT_INDEX
#define WC_HIT_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_HIT_COUNT_INDEX
#define WC_MISS_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_MISS_COUNT_INDEX
#define WC_REMOVAL_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_REMOVAL_COUNT_INDEX
#define WC_REFRESH_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_REFRESH_COUNT_INDEX
#define WC_OVERFLOW_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_OVERFLOW_COUNT_INDEX
#define WC_ADD_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_ADD_COUNT_INDEX
#define WC_LRU_LIST_LENGTH_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_LRU_LIST_LENGTH_INDEX
#define WC_TRIM_COUNT_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_TRIM_COUNT_INDEX
#define WC_SEGMENT_SIZE_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_SEGMENT_SIZE_INDEX
#define WC_WEBMOD_CACHE_INTDATA_LAST_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_WEBMOD_CACHE_INTDATA_LAST_INDEX
#define WC_CURRENT_SIZE_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_CURRENT_SIZE_INDEX
#define WC_MAX_SIZE_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_MAX_SIZE_INDEX
#define WC_WEBMOD_CACHE_LONGDATA_LAST_INDEX \
        com_sun_web_monitoring_WebModuleCacheStats_WEBMOD_CACHE_LONGDATA_LAST_INDEX
#define WC_CACHE_TYPE_UNKNOWN \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_UNKNOWN
#define WC_CACHE_TYPE_BASE_CACHE \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_BASE_CACHE
#define WC_CACHE_TYPE_LRU_CACHE \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_LRU_CACHE
#define WC_CACHE_TYPE_MULTI_LRU_CACHE \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_MULTI_LRU_CACHE
#define WC_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE \
        com_sun_web_monitoring_WebModuleCacheStats_CACHE_TYPE_BOUNDED_MULTI_LRU_CACHE

#define SERVLET_COUNT_REQUESTS \
        com_sun_web_monitoring_ServletStats_SERVLET_COUNT_REQUESTS
#define SERVLET_COUNT_ERRORS \
        com_sun_web_monitoring_ServletStats_SERVLET_COUNT_ERRORS
#define SERVLET_INTDATA_LAST_INDEX \
        com_sun_web_monitoring_ServletStats_SERVLET_INTDATA_LAST_INDEX
#define SERVLET_MAXTIME_IN_MILLIS \
        com_sun_web_monitoring_ServletStats_SERVLET_MAXTIME_IN_MILLIS
#define SERVLET_PROCESS_TIME_IN_MILLIS \
        com_sun_web_monitoring_ServletStats_SERVLET_PROCESS_TIME_IN_MILLIS
#define SERVLET_LONGDATA_LAST_INDEX \
        com_sun_web_monitoring_ServletStats_SERVLET_LONGDATA_LAST_INDEX

// Redifining constants for Jdbc connection pool stats.
#define JDBC_MAX_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_MAX_CONNECTIONS_INDEX
#define JDBC_CURRENT_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_CURRENT_CONNECTIONS_INDEX
#define JDBC_PEAK_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_PEAK_CONNECTIONS_INDEX
#define JDBC_FREE_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_FREE_CONNECTIONS_INDEX
#define JDBC_LEASED_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_LEASED_CONNECTIONS_INDEX
#define JDBC_TOTAL_FAILED_VALIDATION_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_TOTAL_FAILED_VALIDATION_INDEX
#define JDBC_TOTAL_RECREATED_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_TOTAL_RECREATED_CONNECTIONS_INDEX
#define JDBC_QUEUE_SIZE_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_QUEUE_SIZE_INDEX
#define JDBC_PEAK_QUEUE_SIZE_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_PEAK_QUEUE_SIZE_INDEX
#define JDBC_TOTAL_RESIZED_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_TOTAL_RESIZED_INDEX
#define JDBC_TOTAL_TIMEDOUT_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_TOTAL_TIMEDOUT_INDEX
#define JDBC_CONNPOOL_INT_LAST_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_CONNPOOL_INT_LAST_INDEX

#define JDBC_AVERAGE_QUEUE_TIME_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_AVERAGE_QUEUE_TIME_INDEX
#define JDBC_CONNPOOL_FLOAT_LAST_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_CONNPOOL_FLOAT_LAST_INDEX

#define JDBC_TOTAL_LEASED_CONNECTIONS_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_TOTAL_LEASED_CONNECTIONS_INDEX
#define JDBC_PEAK_WAIT_TIME_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_PEAK_WAIT_TIME_INDEX
#define JDBC_CONNPOOL_LONG_LAST_INDEX \
        com_sun_web_monitoring_JdbcConnectionPoolStats_JDBC_CONNPOOL_LONG_LAST_INDEX

#define MAX_UPDATE_SIZE_CONN_POOL \
        com_sun_web_monitoring_JdbcConnectionPoolStats_MAX_UPDATE_SIZE_CONN_POOL

#define TOTAL_LOADED_CLASS_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_TOTAL_LOADED_CLASS_COUNT_INDEX
#define UNLOADED_CLASS_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_UNLOADED_CLASS_COUNT_INDEX
#define HEAP_MEMORY_USED_INDEX \
        com_sun_web_monitoring_JvmManagementStats_HEAP_MEMORY_USED_INDEX
#define TOTAL_STARTED_THR_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_TOTAL_STARTED_THR_COUNT_INDEX
#define GARBAGE_COLLECTION_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_GARBAGE_COLLECTION_COUNT_INDEX
#define GARBAGE_COLLECTION_TIME_INDEX \
        com_sun_web_monitoring_JvmManagementStats_GARBAGE_COLLECTION_TIME_INDEX
#define JVM_MGMT_LONG_LAST_INDEX \
        com_sun_web_monitoring_JvmManagementStats_JVM_MGMT_LONG_LAST_INDEX
#define LOADED_CLASS_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_LOADED_CLASS_COUNT_INDEX
#define PEAK_THREAD_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_PEAK_THREAD_COUNT_INDEX
#define THREAD_COUNT_INDEX \
        com_sun_web_monitoring_JvmManagementStats_THREAD_COUNT_INDEX
#define JVM_MGMT_INT_LAST_INDEX \
        com_sun_web_monitoring_JvmManagementStats_JVM_MGMT_INT_LAST_INDEX
#define VM_VERSION_INDEX \
        com_sun_web_monitoring_JvmManagementStats_VM_VERSION_INDEX
#define VM_NAME_INDEX \
        com_sun_web_monitoring_JvmManagementStats_VM_NAME_INDEX
#define VM_VENDOR_INDEX \
        com_sun_web_monitoring_JvmManagementStats_VM_VENDOR_INDEX
#define JVM_MGMT_STRING_LAST_INDEX \
        com_sun_web_monitoring_JvmManagementStats_JVM_MGMT_STRING_LAST_INDEX

// defining the session replication index constants with smaller names.
#define SR_COUNT_SELF_RECOVERY_ATTEMPTS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_SELF_RECOVERY_ATTEMPTS_INDEX
#define SR_COUNT_SELF_RECOVERY_FAILURES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_SELF_RECOVERY_FAILURES_INDEX
#define SR_COUNT_FAILOVER_ATTEMPTS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_FAILOVER_ATTEMPTS_INDEX
#define SR_COUNT_FAILOVER_FAILURES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_FAILOVER_FAILURES_INDEX
#define SR_COUNT_BACKUP_CONN_FAILURES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_BACKUP_CONN_FAILURES_INDEX
#define SR_COUNT_BACKUP_CONN_FAILOVER_SUCC_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_BACKUP_CONN_FAILOVER_SUCC_INDEX
#define SR_COUNT_SENT_PUTS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_SENT_PUTS_INDEX
#define SR_COUNT_SENT_GETS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_SENT_GETS_INDEX
#define SR_COUNT_SENT_REMOVES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_SENT_REMOVES_INDEX
#define SR_COUNT_RECEIVED_PUTS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_RECEIVED_PUTS_INDEX
#define SR_COUNT_RECEIVED_GETS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_RECEIVED_GETS_INDEX
#define SR_COUNT_RECEIVED_REMOVES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_RECEIVED_REMOVES_INDEX
#define SR_COUNT_ASYNC_QUEUE_ENABLED_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_ASYNC_QUEUE_ENABLED_INDEX
#define SR_COUNT_ASYNC_QUEUE_ENTRIES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_ASYNC_QUEUE_ENTRIES_INDEX
#define SR_PEAK_ASYNC_QUEUE_ENTRIES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_PEAK_ASYNC_QUEUE_ENTRIES_INDEX
#define SR_COUNT_LOCK_FAILURES_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_COUNT_LOCK_FAILURES_INDEX
#define SESSION_REPLICATION_INT_LAST_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SESSION_REPLICATION_INT_LAST_INDEX
#define SR_STATE_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_STATE_INDEX
#define SR_LIST_CLUSTER_MEMBERS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_LIST_CLUSTER_MEMBERS_INDEX
#define SR_CURRENT_BACKUP_INSTANCE_ID_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SR_CURRENT_BACKUP_INSTANCE_ID_INDEX
#define SESSION_REPLICATION_STRING_LAST_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_SESSION_REPLICATION_STRING_LAST_INDEX
#define WEBAPP_STORE_ID_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_ID_INDEX
#define WEBAPP_STORE_VSID_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_VSID_INDEX
#define WEBAPP_STORE_URI_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_URI_INDEX
#define WEBAPP_STORE_STRATTR_LAST_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_STRATTR_LAST_INDEX
#define WEBAPP_STORE_COUNT_SESSIONS_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_COUNT_SESSIONS_INDEX
#define WEBAPP_STORE_INTATTR_LAST_INDEX \
        com_sun_web_monitoring_SessionReplicationStats_WEBAPP_STORE_INTATTR_LAST_INDEX


////////////////////////////////////////////////////////////////

// JavaStatsManager Class members

////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------------
// JavaStatsManager::JavaStatsManager
//-----------------------------------------------------------------------------

JavaStatsManager::JavaStatsManager(void)
{
    jenv_                             = NULL;
    fInitializedVM_                   = PR_FALSE;
    fVMStatsInit_                     = PR_FALSE;
    fWebModInitialize_                = PR_FALSE;
    vmStatsManager_updateStats_       = 0;
    vmStatsManagerObj_                = 0;
}

//-----------------------------------------------------------------------------
// JavaStatsManager::init
//-----------------------------------------------------------------------------

void JavaStatsManager::init(void)
{
    StatsManager::lockStatsData();
    jenv_ = JVMControl::attach();
    if (jenv_)
        fInitializedVM_ = PR_TRUE;
    PRBool fSuccess = initJavaClasses();
    if (fSuccess != PR_TRUE)
    {
        ereport(LOG_FAILURE, "Java stats initialization failed");
    }
    StatsManager::unlockStatsData();
}


//-----------------------------------------------------------------------------
// JavaStatsManager::poll
//
// Right now this function needs to be call by a single thread. If it is called
// by multiple threads even at different times, it will crash as this class is
// not storing thread specific JNIEnv pointer.
//-----------------------------------------------------------------------------

void JavaStatsManager::poll(void)
{
    jboolean fUpdateJvmMgmtStats = PR_FALSE;
    if (fInitializedVM_ == PR_FALSE && (!jenv_))
    {
        init();
        // For the first time during initialization update the management stats
        // too. From then onward jvm management stats will be updated on demand
        // only.
        fUpdateJvmMgmtStats = PR_TRUE;
    }
    if (!jenv_)
        return;
    if (fVMStatsInit_ != PR_TRUE)
        return;
    jboolean retVal = JNI_FALSE;
    // Lock stats data.
    StatsManager::lockStatsData();
    retVal = jenv_->CallBooleanMethod(vmStatsManagerObj_,
                                      vmStatsManager_updateStats_,
                                      JNI_TRUE,  // webmodule stats
                                      JNI_TRUE,  // servlet stats
                                      JNI_TRUE,  // jdbc pools
                                      JNI_TRUE,  // session failover
                                      fUpdateJvmMgmtStats, // management
                                      JNI_TRUE   // unused
                                      );
    // Immediately unlock it.
    StatsManager::unlockStatsData();
    if (retVal != JNI_TRUE)
    {
        // Don't log the message if webserver is termniating. During
        // termination, it could fail for any reason.
        if (WebServer::isTerminating() != PR_TRUE)
        {
            NSJavaUtil::reportException(jenv_);
        }
        // Fall down.
    }
    if (fWebModInitialize_ != PR_TRUE)
    {
        StatsManager::markNotificationFlag(STATS_NOTICE_WEB_MODULE_INIT_DONE);
        fWebModInitialize_ = PR_TRUE;
    }
}

//-----------------------------------------------------------------------------
// JavaStatsManager::initJavaClasses
//-----------------------------------------------------------------------------

PRBool JavaStatsManager::initJavaClasses(void)
{
    if (!jenv_)
        return PR_FALSE;
    jclass vmStatsMgrClass = jenv_->FindClass(CLASS_JVM_STATS_MANAGER);
    if (vmStatsMgrClass == 0)
        return PR_FALSE;
    jmethodID vmStatsMgrClass_init = jenv_->GetMethodID(vmStatsMgrClass,
                                                        "init", "()Z");
    if (!vmStatsMgrClass_init)
        return PR_FALSE;
    vmStatsManager_updateStats_ = jenv_->GetMethodID(vmStatsMgrClass,
                                                     "updateStats",
                                                     "(ZZZZZZ)Z");
    if (!vmStatsManager_updateStats_)
        return PR_FALSE;
    jmethodID claszConstructor = jenv_->GetMethodID(vmStatsMgrClass,
                                                    "<init>",
                                                    "()V");
    if (!claszConstructor)
        return PR_FALSE;
    jobject objTemp = jenv_->NewObject(vmStatsMgrClass, claszConstructor);
    if (! objTemp)
    {
        // Check and Handle Exception
        NSJavaUtil::reportException(jenv_);
        return PR_FALSE;
    }
    vmStatsManagerObj_ = jenv_->NewGlobalRef(objTemp);
    jenv_->DeleteLocalRef(objTemp);
    if (vmStatsManagerObj_ && vmStatsMgrClass_init)
    {
        jboolean retVal = jenv_->CallBooleanMethod(vmStatsManagerObj_,
                                                   vmStatsMgrClass_init);
        if (retVal == JNI_TRUE)
        {
            fVMStatsInit_ = PR_TRUE;
        }
        else
        {
            NSJavaUtil::reportException(jenv_);
        }
    }
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// JavaStatsManager::processReconfigure
//-----------------------------------------------------------------------------

void JavaStatsManager::processReconfigure(void)
{
    // Set the webmodule initialization to FALSE so that during next poll it
    // will update the webmodule list. If during reconfigure new webapps are
    // deployed then webmodule list is re-initialized.
    fWebModInitialize_ = PR_FALSE;
    StatsManager::lockStatsData();
    StatsHeaderNode* header = StatsManager::getHeader();
    StatsVirtualServerNode* vss = header->vss;
    // Set all web app cache state to disabled. Those which are enabled will
    // be marked as enabled in next poll stats.
    while (vss)
    {
        StatsWebModuleNode* wms = vss->wms;
        while (wms)
        {
            wms->wmCacheStats.fEnabled = PR_FALSE;
            wms = wms->next;
        }
        vss = vss->next;
    }
    StatsManager::unlockStatsData();
}

//-----------------------------------------------------------------------------
// JavaStatsManager::updateJvmMgmtStats
//-----------------------------------------------------------------------------

void JavaStatsManager::updateJvmMgmtStats(void)
{
    if (fVMStatsInit_ != PR_TRUE)
        return; // initialization is not yet done so ignore it.
    // Since it could be called by any thread so we can't use jenv_ here. Get
    // caller's thread env from jvm.
    JNIEnv* jenv = JVMControl::attach();
    if (!jenv)
        return;

    jboolean retVal = JNI_FALSE;
    // This lock is useful to acess both stats data structure as well as
    // to protect vmStatsManagerObj_ (Optionally we can use synchronized inside
    // java method).
    StatsManager::lockStatsData();
    retVal = jenv->CallBooleanMethod(vmStatsManagerObj_,
                                      vmStatsManager_updateStats_,
                                      JNI_FALSE,  // webmodule stats
                                      JNI_FALSE,  // servlet stats
                                      JNI_FALSE,  // jdbc pools
                                      JNI_FALSE,  // session failover
                                      JNI_TRUE,   // management
                                      JNI_FALSE   // unused
                                      );
    // Immediately unlock it.
    StatsManager::unlockStatsData();
    if (retVal != JNI_TRUE)
    {
        NSJavaUtil::reportException(jenv);
    }
    return;
}


//-----------------------------------------------------------------------------
// static (to the file) functions
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// updateWebmoduleStats
//-----------------------------------------------------------------------------

static void updateWebmoduleStats(StatsWebModuleSlot* wms,
                                 jint* webmodData)
{
    wms->countJSP = (PRInt32) webmodData[WM_COUNT_JSP_INDEX];
    wms->countJSPReload = (PRInt32) webmodData[WM_COUNT_JSP_RELOAD_INDEX];
    wms->countSessions = (PRInt32) webmodData[WM_COUNT_SESSIONS_INDEX];
    wms->countActiveSessions =
                    (PRInt32) webmodData[WM_COUNT_ACTIVE_SESSIONS_INDEX];
    wms->peakActiveSessions =
                    (PRInt32) webmodData[WM_PEAK_ACTIVE_SESSIONS_INDEX];
    wms->countRejectedSessions =
                    (PRInt32) webmodData[WM_COUNT_REJECTED_SESSIONS_INDEX];
    wms->countExpiredSessions =
                    (PRInt32) webmodData[WM_COUNT_EXPIRED_SESSIONS_INDEX];
    wms->sessionMaxAliveTime =
                    (PRInt32) webmodData[WM_SESSION_MAXALIVETIME_INDEX];
    wms->sessionAvgAliveTime =
                    (PRInt32) webmodData[WM_SESSION_AVGALIVETIME_INDEX];
    PRInt32 webModuleMode = (PRInt32) webmodData[WM_WEBMODULE_MODE_INDEX];
    PRInt32 mode = 0;
    switch (webModuleMode) {
        case WEBMODULE_MODE_DISABLED:
            mode = STATS_WEBMODULE_MODE_DISABLED;
            break;
        case WEBMODULE_MODE_ENABLED:
            mode = STATS_WEBMODULE_MODE_ENABLED;
            break;

        case WEBMODULE_MODE_UNKNOWN:
        default:
            mode = STATS_WEBMODULE_MODE_UNKNOWN;
            break;
    }
    int prevMode = wms->mode;
    // Right now java land does not know web module is disabled or deleted as
    // web container doesn't delete the MBeans associated with them after
    // reconfig.
    if ((prevMode != STATS_WEBMODULE_MODE_EMPTY) &&
                     (prevMode != STATS_WEBMODULE_MODE_DISABLED))
    {
        wms->mode = mode;
    }
}

//-----------------------------------------------------------------------------
// updateWMCacheStats
//-----------------------------------------------------------------------------

static void
updateWMCacheStats(StatsWebModuleCacheSlot* wmCacheSlot,
                   jint* wmCacheIntData,
                   jlong* wmCacheLongData)
{
    wmCacheSlot->fEnabled     = (PRInt32) PR_TRUE;
    wmCacheSlot->cacheType    = (PRInt32) wmCacheIntData[WC_CACHE_TYPE_INDEX];
    wmCacheSlot->maxEntries   = (PRInt32) wmCacheIntData[WC_MAX_ENTRIES_INDEX];
    wmCacheSlot->threshold    = (PRInt32) wmCacheIntData[WC_THRESHOLD_INDEX];
    wmCacheSlot->tableSize    = (PRInt32) wmCacheIntData[WC_TABLE_SIZE_INDEX];
    wmCacheSlot->entryCount   = (PRInt32) wmCacheIntData[WC_ENTRY_COUNT_INDEX];
    wmCacheSlot->hitCount     = (PRInt32) wmCacheIntData[WC_HIT_COUNT_INDEX];
    wmCacheSlot->missCount    = (PRInt32) wmCacheIntData[WC_MISS_COUNT_INDEX];
    wmCacheSlot->removalCount =
                        (PRInt32) wmCacheIntData[WC_REMOVAL_COUNT_INDEX];
    wmCacheSlot->refreshCount =
                        (PRInt32) wmCacheIntData[WC_REFRESH_COUNT_INDEX];
    wmCacheSlot->overflowCount =
                        (PRInt32) wmCacheIntData[WC_OVERFLOW_COUNT_INDEX];
    wmCacheSlot->addCount = (PRInt32) wmCacheIntData[WC_ADD_COUNT_INDEX];
    wmCacheSlot->lruListLength =
                        (PRInt32) wmCacheIntData[WC_LRU_LIST_LENGTH_INDEX];
    wmCacheSlot->trimCount   = (PRInt32) wmCacheIntData[WC_TRIM_COUNT_INDEX];
    wmCacheSlot->segmentSize = (PRInt32) wmCacheIntData[WC_SEGMENT_SIZE_INDEX];
    wmCacheSlot->currentSize = (PRInt32) wmCacheLongData[WC_CURRENT_SIZE_INDEX];
    wmCacheSlot->maxSize     = (PRInt32) wmCacheLongData[WC_MAX_SIZE_INDEX];
}

//-----------------------------------------------------------------------------
// updateServletStats
//
// This updates the servlet slot.
// This copies 64 bit varibles too which may have issues in simulatenous
// access of this slot by multiple threads so acquiring some lock
// to prohibit multiple access is preferred.
//-----------------------------------------------------------------------------
static void
updateServletStats(StatsServletJSPSlot* sjs,
                   jint* servletIntData,
                   jlong* servletLongData)
{
    sjs->countRequest = (PRInt32) servletIntData[SERVLET_COUNT_REQUESTS];
    sjs->countError = (PRInt32) servletIntData[SERVLET_COUNT_ERRORS];
    sjs->millisecProcessing =
                     (PRInt32) servletLongData[SERVLET_MAXTIME_IN_MILLIS];
    sjs->millisecPeakProcessing =
                     (PRInt32) servletLongData[SERVLET_PROCESS_TIME_IN_MILLIS];
}

//-----------------------------------------------------------------------------
// updateJdbcConnPoolStats
//-----------------------------------------------------------------------------

static void
updateJdbcConnPoolStats(StatsJdbcConnPoolNode* poolNode,
                        jint* poolIntData,
                        jlong* poolLongData,
                        jfloat* poolFloatData)
{
    StatsJdbcConnPoolSlot* poolSlot = &poolNode->jdbcStats;
    poolSlot->maxConnections =
                     (PRInt32) poolIntData[JDBC_MAX_CONNECTIONS_INDEX];
    poolSlot->currentConnections =
                     (PRInt32) poolIntData[JDBC_CURRENT_CONNECTIONS_INDEX];
    poolSlot->peakConnections =
                     (PRInt32) poolIntData[JDBC_PEAK_CONNECTIONS_INDEX];
    poolSlot->freeConnections =
                     (PRInt32) poolIntData[JDBC_FREE_CONNECTIONS_INDEX];
    poolSlot->leasedConnections =
                     (PRInt32) poolIntData[JDBC_LEASED_CONNECTIONS_INDEX];
    poolSlot->totalFailedValidation =
                     (PRInt32) poolIntData[JDBC_TOTAL_FAILED_VALIDATION_INDEX];
    poolSlot->totalRecreatedConnections =
                  (PRInt32) poolIntData[JDBC_TOTAL_RECREATED_CONNECTIONS_INDEX];
    poolSlot->queueSize = (PRInt32) poolIntData[JDBC_QUEUE_SIZE_INDEX];
    poolSlot->peakQueueSize =
                     (PRInt32) poolIntData[JDBC_PEAK_QUEUE_SIZE_INDEX];
    poolSlot->totalResized =
                     (PRInt32) poolIntData[JDBC_TOTAL_RESIZED_INDEX];
    poolSlot->totalTimedout =
                     (PRInt32) poolIntData[JDBC_TOTAL_TIMEDOUT_INDEX];

    poolSlot->totalLeasedConnections =
                  (PRUint64) poolLongData[JDBC_TOTAL_LEASED_CONNECTIONS_INDEX];
    poolSlot->peakWaitTime =
                     (PRUint64) poolLongData[JDBC_PEAK_WAIT_TIME_INDEX];
    poolSlot->averageQueueTime =
                     (PRFloat64) poolFloatData[JDBC_AVERAGE_QUEUE_TIME_INDEX];
}



//-----------------------------------------------------------------------------
// Java Native APIs.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_VirtualServerStats_updateWebModuleStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_VirtualServerStats_updateWebModuleStats(
                                              JNIEnv* penv,
                                              jclass clasz,
                                              jobjectArray jobjWebModNameArray,
                                              jintArray jobjWebModuleData)
{
    int nCount = penv->GetArrayLength(jobjWebModNameArray);
    int nDataElem = (penv->GetArrayLength(jobjWebModuleData));
    PR_ASSERT((nCount * WEBMOD_INTDATA_LAST_INDEX) == nDataElem);

    NSJIntArrayPtr webmodDataPtr(penv, jobjWebModuleData);
    jint* webmodData = (jint*) webmodDataPtr;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCount; ++nIndex)
    {
        jstring jobjWebModuleName = (jstring)
            penv->GetObjectArrayElement(jobjWebModNameArray,
                                        nIndex);
        if (jobjWebModuleName == NULL)
            break;
        NSJStringLocalRef webModuleName(penv,
                                        jobjWebModuleName);
        // First find vss where the webModule belongs to.
        StatsVirtualServerNode* vss =
            StatsManager::getVSSFromWebModuleName(webModuleName);
        if (!vss)
        {
            PR_ASSERT(0);
            continue;
        }
        // Find webModule slot and fill the data. Create if it doesn't exist
        StatsWebModuleNode* wms =
            StatsManagerUtil::getWebModuleSlot(vss, webModuleName);
        if (wms)
        {
            int nCurIndex = nIndex * WEBMOD_INTDATA_LAST_INDEX;
            updateWebmoduleStats(&wms->wmsStats, webmodData + nCurIndex);
        }
        else
        {
            // Error
            PR_ASSERT(0);
        }
    }
}


//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_VirtualServerStats_updateWMCacheStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_VirtualServerStats_updateWMCacheStats(
                                            JNIEnv       *penv,
                                            jclass       clasz,
                                            jobjectArray jobjWebModNameArray,
                                            jintArray    jobjWMCacheIntData,
                                            jlongArray   jobjWMCacheLongData)
{
    int nCount = penv->GetArrayLength(jobjWebModNameArray);
    int nIntDataElem = (penv->GetArrayLength(jobjWMCacheIntData));
    int nLongDataElem = (penv->GetArrayLength(jobjWMCacheLongData));
    PR_ASSERT((nCount * WC_WEBMOD_CACHE_INTDATA_LAST_INDEX) == nIntDataElem);
    PR_ASSERT((nCount * WC_WEBMOD_CACHE_LONGDATA_LAST_INDEX) == nLongDataElem);

    NSJIntArrayPtr wmCacheIntDataPtr(penv, jobjWMCacheIntData);
    jint* wmCacheIntData = (jint*) wmCacheIntDataPtr;
    NSJLongArrayPtr wmCacheLongDataPtr(penv, jobjWMCacheLongData);
    jlong* wmCacheLongData = (jlong*) wmCacheLongDataPtr;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCount; ++nIndex)
    {
        jstring jobjWebModuleName = (jstring)
            penv->GetObjectArrayElement(jobjWebModNameArray,
                                        nIndex);
        if (jobjWebModuleName == NULL)
            break;
        NSJStringLocalRef webModuleName(penv,
                                        jobjWebModuleName);
        // First find vss where the webModule belongs to.
        StatsVirtualServerNode* vss =
            StatsManager::getVSSFromWebModuleName(webModuleName);
        if (!vss)
        {
            PR_ASSERT(0);
            continue;
        }
        // Find webModule slot and fill the data. Create if it doesn't exist
        StatsWebModuleNode* wms =
            StatsManagerUtil::getWebModuleSlot(vss, webModuleName);
        if (wms)
        {
            int nCurIndex = nIndex * WC_WEBMOD_CACHE_INTDATA_LAST_INDEX;
            int nCurLongIndex = nIndex * WC_WEBMOD_CACHE_LONGDATA_LAST_INDEX;
            updateWMCacheStats(&wms->wmCacheStats,
                               wmCacheIntData + nCurIndex,
                               wmCacheLongData + nCurLongIndex);
        }
        else
        {
            // Error
            PR_ASSERT(0);
        }
    }
}

//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_VirtualServerStats_updateServletStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_VirtualServerStats_updateServletStats(
                                            JNIEnv       *penv,
                                            jclass       clasz,
                                            jobjectArray jobjWebModNameArray,
                                            jobjectArray jobjServletNameArray,
                                            jintArray    jobjServletIntData,
                                            jlongArray   jobjServletLongData)
{
    int nCount = penv->GetArrayLength(jobjWebModNameArray);
    int nIntDataElem = (penv->GetArrayLength(jobjServletIntData));
    int nLongDataElem = (penv->GetArrayLength(jobjServletLongData));
    PR_ASSERT((nCount * SERVLET_INTDATA_LAST_INDEX) == nIntDataElem);
    PR_ASSERT((nCount * SERVLET_LONGDATA_LAST_INDEX) == nLongDataElem);

    NSJIntArrayPtr servletIntDataPtr(penv, jobjServletIntData);
    jint* servletIntData = (jint*) servletIntDataPtr;
    NSJLongArrayPtr servletLongDataPtr(penv, jobjServletLongData);
    jlong* servletLongData = (jlong*) servletLongDataPtr;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCount; ++nIndex)
    {
        jstring
        jobjWebModuleName = (jstring)
                             penv->GetObjectArrayElement(jobjWebModNameArray,
                                                         nIndex);
        if (jobjWebModuleName == NULL)
            break;              // No more servlets
        jstring jobjServletName = (jstring)
            penv->GetObjectArrayElement(jobjServletNameArray,
                                        nIndex);
        if (jobjServletName == NULL)
            break;              // no more servlets
        NSJStringLocalRef webModuleName(penv, jobjWebModuleName);
        NSJStringLocalRef servletName(penv, jobjServletName);
        // First find vss where the webModule belongs to.
        StatsVirtualServerNode*
        vss = StatsManager::getVSSFromWebModuleName(webModuleName);
        if (!vss)
        {
            // Error
            PR_ASSERT(0);
            continue;
        }
        // Find webModule slot
        StatsWebModuleNode*
        wms = StatsManager::findWebModuleSlot(vss, webModuleName);
        if (!wms)
        {
            // Error
            PR_ASSERT(0);
            continue;
        }
        StatsServletJSPNode*
        sjs = StatsManagerUtil::getServletSlot(wms, servletName);
        int nCurIntIndex = nIndex * SERVLET_INTDATA_LAST_INDEX;
        int nCurLongIndex = nIndex * SERVLET_LONGDATA_LAST_INDEX;
        updateServletStats(&sjs->sjsStats,
                           servletIntData + nCurIntIndex,
                           servletLongData + nCurLongIndex);
    }
}

//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_JdbcConnectionPoolStats_updateConnectionPoolStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_JdbcConnectionPoolStats_updateConnectionPoolStats(
                                            JNIEnv*      penv,
                                            jclass       clasz,
                                            jobjectArray jobjPoolNameArray,
                                            jintArray    jobjPoolIntData,
                                            jlongArray   jobjPoolLongData,
                                            jfloatArray  jobjPoolFloatData)
{
    int nCount = penv->GetArrayLength(jobjPoolNameArray);
    int nIntDataElem = (penv->GetArrayLength(jobjPoolIntData));
    PR_ASSERT((nCount * JDBC_CONNPOOL_INT_LAST_INDEX) == nIntDataElem);
    int nLongDataElem = (penv->GetArrayLength(jobjPoolLongData));
    PR_ASSERT((nCount * JDBC_CONNPOOL_LONG_LAST_INDEX) == nLongDataElem);
    int nFloatDataElem = (penv->GetArrayLength(jobjPoolFloatData));
    PR_ASSERT((nCount * JDBC_CONNPOOL_FLOAT_LAST_INDEX) == nFloatDataElem);

    NSJIntArrayPtr poolIntDataPtr(penv, jobjPoolIntData);
    jint* poolIntData = (jint*) poolIntDataPtr;
    NSJLongArrayPtr poolLongDataPtr(penv, jobjPoolLongData);
    jlong* poolLongData = (jlong*) poolLongDataPtr;
    NSJFloatArrayPtr poolFloatDataPtr(penv, jobjPoolFloatData);
    jfloat* poolFloatData = (jfloat*) poolFloatDataPtr;
    int nIndex = 0;
    for (nIndex = 0; nIndex < nCount; ++nIndex)
    {
        jstring jobjConnPoolName = (jstring)
            penv->GetObjectArrayElement(jobjPoolNameArray,
                                        nIndex);
        if (jobjConnPoolName == NULL)
            break;
        NSJStringLocalRef connPoolName(penv, jobjConnPoolName);
        // Find the pool Node for connPoolName
        StatsJdbcConnPoolNode* poolNode = 0;
        PRBool fNewNode = PR_FALSE;
        poolNode = StatsManager::getJdbcConnPoolNode(connPoolName, fNewNode);
        if (fNewNode == PR_TRUE)
        {
            StatsManager::markNotificationFlag(
                            STATS_NOTICE_JDBC_NODES_COUNT_CHANGED);
        }

        if (!poolNode)
        {
            PR_ASSERT(0);
            continue;
        }
        const int nCurIntIndex = nIndex * JDBC_CONNPOOL_INT_LAST_INDEX;
        const int nCurLongIndex = nIndex * JDBC_CONNPOOL_LONG_LAST_INDEX;
        const int nCurFloatIndex = nIndex * JDBC_CONNPOOL_FLOAT_LAST_INDEX;
        updateJdbcConnPoolStats(poolNode, (poolIntData + nCurIntIndex),
                                (poolLongData + nCurLongIndex),
                                (poolFloatData + nCurFloatIndex));
    }
    return;
}

//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_JvmManagementStats_updateJvmNativeStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_JvmManagementStats_updateJvmNativeStats(
                                            JNIEnv*      penv,
                                            jclass       clasz,
                                            jintArray    jobjJvmIntData,
                                            jlongArray   jobjJvmLongData,
                                            jobjectArray  jobjJvmStringData)
{
    int nIntDataElem = (penv->GetArrayLength(jobjJvmIntData));
    PR_ASSERT(JVM_MGMT_INT_LAST_INDEX == nIntDataElem);
    int nLongDataElem = (penv->GetArrayLength(jobjJvmLongData));
    PR_ASSERT(JVM_MGMT_LONG_LAST_INDEX == nLongDataElem);

    NSJIntArrayPtr jobjJvmIntDataPtr(penv, jobjJvmIntData);
    jint* jvmIntData = (jint*) jobjJvmIntDataPtr;
    NSJLongArrayPtr jobjJvmLongDataPtr(penv, jobjJvmLongData);
    jlong* jvmLongData = (jlong*) jobjJvmLongDataPtr;

    StatsProcessNode* procNode = StatsManager::getProcessSlot();
    if (!procNode)
        return;
    procNode->procStats.jvmManagentStats = STATS_STATUS_ENABLED;
    StatsJvmManagementNode* jvmMgmtNode = procNode->jvmMgmtNode;
    if (!procNode->jvmMgmtNode)
    {
        jvmMgmtNode = procNode->jvmMgmtNode = new StatsJvmManagementNode;
        // Assign the string constants only once as they are not expected to
        // change continuously.
        int nStringDataElem = (penv->GetArrayLength(jobjJvmStringData));
        PR_ASSERT(JVM_MGMT_STRING_LAST_INDEX == nStringDataElem);

        NSJavaUtil::getStringAtIndex(penv, jobjJvmStringData,
                                     VM_VERSION_INDEX, jvmMgmtNode->vmVersion);
        NSJavaUtil::getStringAtIndex(penv, jobjJvmStringData,
                                     VM_NAME_INDEX, jvmMgmtNode->vmName);
        NSJavaUtil::getStringAtIndex(penv, jobjJvmStringData,
                                     VM_VENDOR_INDEX, jvmMgmtNode->vmVendor);
    }
    PR_ASSERT(jvmMgmtNode);
    StatsJvmManagementSlot& jvmMgmtStats = jvmMgmtNode->jvmMgmtStats;

    // Copy integer and long data in native stats.
    jvmMgmtStats.loadedClassCount =
                 (PRInt32) jvmIntData[LOADED_CLASS_COUNT_INDEX];
    jvmMgmtStats.threadCount = (PRInt32) jvmIntData[THREAD_COUNT_INDEX];
    jvmMgmtStats.peakThreadCount =
                 (PRInt32) jvmIntData[PEAK_THREAD_COUNT_INDEX];

    jvmMgmtStats.totalLoadedClassCount =
                 (PRInt64) jvmLongData[TOTAL_LOADED_CLASS_COUNT_INDEX];
    jvmMgmtStats.unloadedClassCount =
                 (PRInt64) jvmLongData[UNLOADED_CLASS_COUNT_INDEX];
    jvmMgmtStats.totalStartedThreadCount =
                 (PRInt64) jvmLongData[TOTAL_STARTED_THR_COUNT_INDEX];

    jvmMgmtStats.sizeHeapUsed = (PRInt64) jvmLongData[HEAP_MEMORY_USED_INDEX];

    jvmMgmtStats.garbageCollectionCount =
                 (PRInt64) jvmLongData[GARBAGE_COLLECTION_COUNT_INDEX];
    jvmMgmtStats.garbageCollectionTime =
                 (PRInt64) jvmLongData[GARBAGE_COLLECTION_TIME_INDEX];

}

//-----------------------------------------------------------------------------
// updateSessionReplicationAttributes
//-----------------------------------------------------------------------------

static void
updateSessionReplicationAttributes(StatsSessionReplicationNode* sessReplNode,
                                   const jint* intAttributes)
{
    StatsSessionReplicationSlot* sessReplStats = &sessReplNode->sessReplStats;
    sessReplStats->countSelfRecoveryAttempts =
            intAttributes[SR_COUNT_SELF_RECOVERY_ATTEMPTS_INDEX];
    sessReplStats->countSelfRecoveryFailures =
            intAttributes[SR_COUNT_SELF_RECOVERY_FAILURES_INDEX];
    sessReplStats->countFailoverAttempts =
            intAttributes[SR_COUNT_FAILOVER_ATTEMPTS_INDEX];
    sessReplStats->countFailoverFailures =
            intAttributes[SR_COUNT_FAILOVER_FAILURES_INDEX];
    sessReplStats->countBackupConnFailures =
            intAttributes[SR_COUNT_BACKUP_CONN_FAILURES_INDEX];
    sessReplStats->countBackupConnFailoverSucc =
            intAttributes[SR_COUNT_BACKUP_CONN_FAILOVER_SUCC_INDEX];
    sessReplStats->countSentPuts = intAttributes[SR_COUNT_SENT_PUTS_INDEX];
    sessReplStats->countSentGets = intAttributes[SR_COUNT_SENT_GETS_INDEX];
    sessReplStats->countSentRemoves =
            intAttributes[SR_COUNT_SENT_REMOVES_INDEX];
    sessReplStats->countReceivedPuts =
            intAttributes[SR_COUNT_RECEIVED_PUTS_INDEX];
    sessReplStats->countReceivedGets =
            intAttributes[SR_COUNT_RECEIVED_GETS_INDEX];
    sessReplStats->countReceivedRemoves =
            intAttributes[SR_COUNT_RECEIVED_REMOVES_INDEX];
    sessReplStats->flagAsyncQueueEnabled =
            intAttributes[SR_COUNT_ASYNC_QUEUE_ENABLED_INDEX];
    sessReplStats->countAsyncQueueEntries =
            intAttributes[SR_COUNT_ASYNC_QUEUE_ENTRIES_INDEX];
    sessReplStats->peakAsyncQueueEntries =
            intAttributes[SR_PEAK_ASYNC_QUEUE_ENTRIES_INDEX];
    sessReplStats->countLockFailures =
            intAttributes[SR_COUNT_LOCK_FAILURES_INDEX];
}

//-----------------------------------------------------------------------------
// updateSessionReplicationStoreData
//-----------------------------------------------------------------------------

static void
updateSessionReplicationStoreData(JNIEnv* penv,
                                  int nCountInstances,
                                  StatsSessionReplicationNode* sessReplNode,
                                  const jint* countWebAppStoreArray,
                                  jobjectArray jobjInstanceIdArray,
                                  jintArray    jobjWebappStoreIntAttr,
                                  jobjectArray jobjWebappStoreStrAttr)
{
    int nIndex = 0;
    int nTotalCountWebappStores = 0; // Sum of all webpapp stores.
    for (nIndex = 0; nIndex < nCountInstances; ++nIndex)
    {
        nTotalCountWebappStores += countWebAppStoreArray[nIndex];
    }

    // There must be integer attributes for all webapp store in
    // jobjWebappStoreIntAttr.
    int intAttrCount = penv->GetArrayLength(jobjWebappStoreIntAttr);
    if (intAttrCount !=
        nTotalCountWebappStores * WEBAPP_STORE_INTATTR_LAST_INDEX)
    {
        PR_ASSERT(0);
        return;
    }

    // There must be string attributes for all webapp store in
    // jobjWebappStoreStrAttr.
    int strAttrCount = penv->GetArrayLength(jobjWebappStoreStrAttr);
    if (strAttrCount !=
        nTotalCountWebappStores * WEBAPP_STORE_STRATTR_LAST_INDEX)
    {
        PR_ASSERT(0);
        return;
    }
    NSJIntArrayPtr jWebAppStoreIntAttrArray(penv, jobjWebappStoreIntAttr);
    jint* webappStoreIntAttrArray = (jint*) jWebAppStoreIntAttrArray;

    int nStrAttrIndex = 0; // an index for string attribute.
    int nIntAttrIndex = 0; // an index for integer attribute.
    for (nIndex = 0; nIndex < nCountInstances; ++nIndex)
    {
        NSString instanceId;
        NSJavaUtil::getStringAtIndex(penv, jobjInstanceIdArray,
                                     nIndex, instanceId);
        StatsSessReplInstanceNode* instanceNode = NULL;
        PRBool fNewNode = PR_FALSE;
        instanceNode = StatsManagerUtil::getSessReplInstanceNode(sessReplNode,
                                                                 instanceId,
                                                                 fNewNode);
        PR_ASSERT(instanceNode != NULL);
        if (!instanceNode)
        {
            return;
        }
        if (fNewNode == PR_TRUE)
        {
            StatsManager::markNotificationFlag(
                                STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE);
        }

        // Now mark this instance node as active as stats are getting updated
        // for this instance node.
        instanceNode->markNode(STATS_SR_INST_NODE_ACTIVE);

        // Mark all the webapp store nodes underneath this instance node as
        // invalid. During updating the web app store node these will be
        // marked as active.
        instanceNode->markWebappStoreNodes(STATS_SR_WEBAPP_NODE_INVALID);

        int countWebAppStores = countWebAppStoreArray[nIndex];
        instanceNode->countWebappStores = countWebAppStores;
        int nStoreIndex = 0;
        for (nStoreIndex = 0; nStoreIndex < countWebAppStores; ++nStoreIndex)
        {
            StatsWebAppSessionStoreNode* wassNode = NULL;
            // Retrive the web app store id and retrieve the
            // StatsWebAppSessionStoreNode for instanceNode.
            int nIdIndex = nStrAttrIndex + WEBAPP_STORE_ID_INDEX;
            NSString storeId;
            NSJavaUtil::getStringAtIndex(penv, jobjWebappStoreStrAttr,
                                         nIdIndex, storeId);
            fNewNode = PR_FALSE;
            wassNode = StatsManagerUtil::getWebAppSessStoreNode(instanceNode,
                                                                storeId,
                                                                fNewNode);
            if (wassNode == NULL)
            {
                PR_ASSERT(0);
                return;
            }
            if (fNewNode == PR_TRUE)
            {
                StatsManager::markNotificationFlag(
                                STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE);
            }
            // Assign the string attributes in wassNode
            int nVsIdIndex = nStrAttrIndex + WEBAPP_STORE_VSID_INDEX;
            NSJavaUtil::getStringAtIndex(penv, jobjWebappStoreStrAttr,
                                         nVsIdIndex, wassNode->vsId);
            int nUriIndex = nStrAttrIndex + WEBAPP_STORE_URI_INDEX;
            NSJavaUtil::getStringAtIndex(penv, jobjWebappStoreStrAttr,
                                         nUriIndex, wassNode->uri);

            // Assign the string attributes.
            int nCountEntriesIndex = nIntAttrIndex +
                                     WEBAPP_STORE_COUNT_SESSIONS_INDEX;
            int nCountSessions = webappStoreIntAttrArray[nCountEntriesIndex];
            wassNode->countReplicatedSessions = nCountSessions;

            // As stats are available for this node so mark it as active node.
            wassNode->markNode(STATS_SR_WEBAPP_NODE_ACTIVE);

            // increment the array index counters.
            nStrAttrIndex += WEBAPP_STORE_STRATTR_LAST_INDEX;
            nIntAttrIndex += WEBAPP_STORE_INTATTR_LAST_INDEX;
        }
        // Delete the inactive webapp store nodes.
        if (instanceNode->deleteInactiveWebappStoreNodes() > 0)
        {
            StatsManager::markNotificationFlag(
                            STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE);
        }
    }
}

//-----------------------------------------------------------------------------
// Java_com_sun_web_monitoring_SessionReplicationStats_jniUpdateSessionReplicationStats
//-----------------------------------------------------------------------------

JNIEXPORT void JNICALL
Java_com_sun_web_monitoring_SessionReplicationStats_jniUpdateSessionReplicationStats(
                             JNIEnv      *penv,
                             jclass       clasz,
                             jintArray    jobjSessReplIntAttr,
                             jobjectArray jobjSessReplStringAttr,
                             jintArray    jobjCountWebappStoreArray,
                             jobjectArray jobjInstanceIdArray,
                             jintArray    jobjWebappStoreIntAttr,
                             jobjectArray jobjWebappStoreStrAttr)
{
    StatsProcessNode* procNode = StatsManager::getProcessSlot();
    if (!procNode)
        return;
    if (procNode->sessReplNode == NULL)
    {
        procNode->sessReplNode = new StatsSessionReplicationNode;
        StatsManager::markNotificationFlag(
                        STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE);
    }
    StatsSessionReplicationNode* sessReplNode = procNode->sessReplNode;

    int nIntDataElem = (penv->GetArrayLength(jobjSessReplIntAttr));
    PR_ASSERT(SESSION_REPLICATION_INT_LAST_INDEX == nIntDataElem);
    int nStringDataElem = (penv->GetArrayLength(jobjSessReplStringAttr));
    PR_ASSERT(SESSION_REPLICATION_STRING_LAST_INDEX == nStringDataElem);

    // Retrieve the string attributes from jobjSessReplStringAttr array.
    NSJavaUtil::getStringAtIndex(penv, jobjSessReplStringAttr, SR_STATE_INDEX,
                                 sessReplNode->state);
    NSJavaUtil::getStringAtIndex(penv, jobjSessReplStringAttr,
                                 SR_CURRENT_BACKUP_INSTANCE_ID_INDEX,
                                 sessReplNode->currentBackupInstanceId);
    NSJavaUtil::getStringAtIndex(penv, jobjSessReplStringAttr,
                                 SR_LIST_CLUSTER_MEMBERS_INDEX,
                                 sessReplNode->listClusterMembers);

    // Set the integer attributes.
    NSJIntArrayPtr intAttributes(penv, jobjSessReplIntAttr);
    updateSessionReplicationAttributes(sessReplNode, intAttributes);

    // Mark all the underneath instance nodes as invalid.
    sessReplNode->markInstanceNodes(STATS_SR_INST_NODE_INVALID);

    if ((jobjCountWebappStoreArray == NULL) || (jobjInstanceIdArray == NULL))
        return;
    int nCountInstances = (penv->GetArrayLength(jobjCountWebappStoreArray));
    int nCountInstanceIds = (penv->GetArrayLength(jobjInstanceIdArray));
    PR_ASSERT(nCountInstances == nCountInstanceIds);

    if (nCountInstances == 0)
    {
        return;
    }
    NSJIntArrayPtr countWebAppStoreArray(penv, jobjCountWebappStoreArray);
    updateSessionReplicationStoreData(penv, nCountInstances, sessReplNode,
                                      countWebAppStoreArray,
                                      jobjInstanceIdArray,
                                      jobjWebappStoreIntAttr,
                                      jobjWebappStoreStrAttr);

    if (sessReplNode->deleteInactiveInstanceNodes() > 0)
    {
        StatsManager::markNotificationFlag(
                        STATS_NOTICE_SESSION_REPL_NODE_COUNT_CHANGE);
    }
}

