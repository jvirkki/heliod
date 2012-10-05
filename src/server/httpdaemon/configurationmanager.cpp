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

#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <nspr.h>

#include "httpdaemon/libdaemon.h"
#include "httpdaemon/WebServer.h"
#include "NsprWrap/CriticalSection.h"
#include "support/GenericVector.h"
#include "support/PSQueue.h"
#include "frame/conf.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"

//-----------------------------------------------------------------------------
// Definitions of static member variables
//-----------------------------------------------------------------------------

CriticalSection* ConfigurationManager::lockConfiguration = 0;
CriticalSection* ConfigurationManager::lockSetConfiguration = 0;
Configuration* ConfigurationManager::configuration = 0;
ReadWriteLock* ConfigurationManager::lockVectorListeners = 0;
GenericVector* ConfigurationManager::vectorListeners = 0;
CriticalSection* ConfigurationManager::lockCallListeners = 0;
ConfigurationManager* ConfigurationManager::releaserThread = 0;
PSQueue<ConfigurationManagerListener*>* ConfigurationManager::releasedConfigs = 0;
PRUint32 ConfigurationManager::countConfigurations = 0;

//-----------------------------------------------------------------------------
// ConfigurationManagerListener
//-----------------------------------------------------------------------------

class ConfigurationManagerListener : public ConfigurationListener {
public:
    ConfigurationManagerListener(Configuration* configuration, int countListenersCalled)
    : configuration(configuration),
      countListenersCalled(countListenersCalled)
    {
    }

    void releaseConfiguration(Configuration* outgoing)
    {
        ConfigurationManager::releaseConfiguration(this);
    }

    Configuration* configuration;
    int countListenersCalled;
};

//-----------------------------------------------------------------------------
// ConfigurationManager::init
//-----------------------------------------------------------------------------

void ConfigurationManager::init()
{
    initEarly();

    const PRInt32 DEFAULT_BACKLOG = 10;
    PRInt32 backlog = conf_getboundedinteger("ReconfigBacklog", 1, 1000, DEFAULT_BACKLOG);
    releasedConfigs = new PSQueue<ConfigurationManagerListener*>(backlog);

    releaserThread = new ConfigurationManager();
    if (releaserThread->start(PR_GLOBAL_BOUND_THREAD, PR_UNJOINABLE_THREAD) != PR_SUCCESS) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ConfigurationManager_UnableToStartThread));
    }
}

//-----------------------------------------------------------------------------
// ConfigurationManager::initEarly
//-----------------------------------------------------------------------------

void ConfigurationManager::initEarly()
{
    if (!lockConfiguration) {
        // XXX These are never delete'd
        lockConfiguration = new CriticalSection();
        lockSetConfiguration = new CriticalSection();
        lockVectorListeners = new ReadWriteLock();
        vectorListeners = new GenericVector();
        lockCallListeners = new CriticalSection();
    }
}

//-----------------------------------------------------------------------------
// ConfigurationManager::getConfiguration
//-----------------------------------------------------------------------------

Configuration* ConfigurationManager::getConfiguration()
{
    Configuration* current;

    // Get the current configuration and increase its reference count
    lockConfiguration->acquire();
    current = configuration;
    if (current) current->ref();
    lockConfiguration->release();

    return current;
}

//-----------------------------------------------------------------------------
// ConfigurationManager::setConfiguration
//-----------------------------------------------------------------------------

PRStatus ConfigurationManager::setConfiguration(Configuration* incoming)
{
    PRStatus rv = PR_SUCCESS;

    lockSetConfiguration->acquire();

    if (incoming) {
        if (incoming != configuration && countConfigurations > 0) {
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_ConfigurationManager_InstallingNewConfiguration));
        }

        lockCallListeners->acquire();

        // Let all the installed ConfigurationListeners know we're about to set
        // the configuration (call them in the same order they were added)
        lockVectorListeners->acquireRead();
        int i;
        for (i = 0; i < vectorListeners->length() && rv == PR_SUCCESS; i++) {
            ConfigurationListener* listener;
            listener = (ConfigurationListener*)(*vectorListeners)[i];
            lockVectorListeners->release();
            if (listener->setConfiguration(incoming, configuration) == PR_FAILURE) {
                // This ConfigurationListener rejected the configuration, so
                // don't call any further listeners
                rv = PR_FAILURE;
            }
            lockVectorListeners->acquireRead();
        }
        int countListenersCalled = i;
        lockVectorListeners->release();

        // We want to be notified when this configuration is to be deleted
        incoming->setListener(new ConfigurationManagerListener(incoming, countListenersCalled));

        lockCallListeners->release();
    }

    if (rv == PR_FAILURE) {
        if (configuration && configuration != incoming) {
            // We'll continue using the old configuration
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_ConfigurationManager_RolledBack));
        }

    } else {
        // Handle a new incoming configuration
        if (incoming != configuration) {
            if (countConfigurations > 0) {
                ereport(LOG_INFORM, XP_GetAdminStr(DBT_ConfigurationManager_SuccessfullyInstalled));
            }

            countConfigurations++;
        }

        // Decrease the reference count on the old configuration, mark the new
        // as current, and increase its ref count
        const void *released = NULL;
        const void *installed = NULL;
        PRInt32 idReleased;
        PRInt32 idInstalled;
        lockConfiguration->acquire();
        if (configuration) {
            released = configuration;
            idReleased = configuration->getID();
            configuration->unref();
        }
        configuration = incoming;
        if (configuration) {
            installed = configuration;
            idInstalled = configuration->getID();
            configuration->ref();
        }
        lockConfiguration->release();
        if (released) {
            ereport(LOG_VERBOSE, "Released configuration %d", idReleased);
        }
        if (installed) {
            ereport(LOG_VERBOSE, "Installed configuration %d", idInstalled);
        }
    }

    lockSetConfiguration->release();

    return rv;
}

//-----------------------------------------------------------------------------
// ConfigurationManager::releaseConfiguration
//-----------------------------------------------------------------------------

void ConfigurationManager::releaseConfiguration(ConfigurationManagerListener* outgoing)
{
    if (releasedConfigs->put(outgoing) == PR_FAILURE) {
        PRSize capacity = releasedConfigs->capacity();
        PRSize length = releasedConfigs->length();
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ConfigurationManager_ReleaseQueueFullNofN), length, capacity);
    }
}

//-----------------------------------------------------------------------------
// ConfigurationManager::addListener
//-----------------------------------------------------------------------------

void ConfigurationManager::addListener(ConfigurationListener* listener)
{
    if (!lockVectorListeners)
        initEarly();

    if (listener) {
        // Add the new listener to the end of the vector.  Order is important
        // as we identify listeners by index in ConfigurationManager::run.
        lockVectorListeners->acquireWrite();
        vectorListeners->append(listener);
        lockVectorListeners->release();
    }
}

//-----------------------------------------------------------------------------
// ConfigurationManager::run (releases configurations)
//-----------------------------------------------------------------------------

void ConfigurationManager::run(void)
{
    ConfigurationManagerListener* listenerOutgoing;

    while (releasedConfigs->get(listenerOutgoing) != PR_FAILURE) {
        Configuration* outgoing = listenerOutgoing->configuration;
        PRInt32 idOutgoing = outgoing->getID();

        ereport(LOG_VERBOSE, "Deleting configuration %d", idOutgoing);

        lockCallListeners->acquire();

        // Let all the ConfigurationListeners we told about this configuration
        // know that we're about to release it (call in reverse order)
        lockVectorListeners->acquireRead();
        int i = listenerOutgoing->countListenersCalled;
        while (i--) {
            ConfigurationListener* listener;
            listener = (ConfigurationListener*)(*vectorListeners)[i];
            lockVectorListeners->release();
            listener->releaseConfiguration(outgoing);
            lockVectorListeners->acquireRead();
        }
        lockVectorListeners->release();

        lockCallListeners->release();

        delete outgoing;
        delete listenerOutgoing;

        ereport(LOG_VERBOSE, "Deleted configuration %d", idOutgoing);
    }
}

//-----------------------------------------------------------------------------
// ConfigurationManager::ConfigurationManager
//-----------------------------------------------------------------------------

ConfigurationManager::ConfigurationManager(void) : Thread("ConfigurationManager")
{
}

