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
 * ListenSockets class implementation.
 *
 * @author  celving
 * @since   iWS5.0
 */

#ifdef DEBUG
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
#include <iomanip>
using namespace std;
#else
#include <iostream.h>                        // cout
#include <iomanip.h>                         // setw()
#endif
#endif

#include "base/ereport.h"                    // LOG_*
#include "frame/conf.h"                      // conf_getboolean
#include "httpdaemon/dbthttpdaemon.h"        // DBT_LS* message strings
#include "httpdaemon/statsmanager.h"         // StatsManager class
#include "httpdaemon/configuration.h"        // Configuration class
#include "httpdaemon/daemonsession.h"        // DaemonSession::GetConnQueue()
#include "httpdaemon/ListenSocket.h"         // ListenSocket class
#include "httpdaemon/ListenSockets.h"        // ListenSockets class

// Linux is broken; it won't let us listen on a given port on both INADDR_ANY
// and a specific IP simultaneously
#ifdef Linux
#define BIND_ANY_AND_SPECIFIC PR_FALSE
#else
#define BIND_ANY_AND_SPECIFIC PR_TRUE
#endif

// a pointer to the only instance of this class
ListenSockets* ListenSockets::instance_ = NULL;

// PR_Lock used for double-check during creation of instance_
CriticalSection ListenSockets::lock_;

ListenSockets::ListenSockets(void) : currentConfig_(NULL)
{
    this->connQueue_ = DaemonSession::GetConnQueue();
}

ListenSockets*
ListenSockets::getInstance(void)
{
    if (ListenSockets::instance_ == NULL)
    {
        SafeLock guard(ListenSockets::lock_);
        if (ListenSockets::instance_ == NULL)
        {
            ListenSockets::instance_ = new ListenSockets;
            if (ListenSockets::instance_ == NULL)
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ListenSockets_malloc));
        }
    }
    return ListenSockets::instance_;
}

ListenSockets::~ListenSockets(void)
{
}

void
ListenSockets::setConnectionQueue(ConnectionQueue* connQueue)
{
    SafeLock guard(ListenSockets::lock_);
    this->connQueue_ = connQueue;
}

PRStatus
ListenSockets::setConfiguration(Configuration* newConfig,
                                const Configuration* currentConfig)
{
    if (newConfig == NULL)
    {
        if (this->currentConfig_)
            this->currentConfig_->unref();
        this->currentConfig_ = NULL;
        return this->closeAll();
    }

    SafeLock guard(ListenSockets::lock_);

    // Set new configuration
    PRStatus status = setConfiguration(*newConfig);
    if (status == PR_SUCCESS)
    {
        // Success.  Remember the configuration we're using.
        newConfig->ref();
        if (this->currentConfig_)
            this->currentConfig_->unref();
        this->currentConfig_ = newConfig;
    }
    else
    {
        // Error setting new configuration, rollback to old configuration
        if (this->currentConfig_ != NULL)
            setConfiguration(*this->currentConfig_);
    }

    return status;
}

PRStatus
ListenSockets::setConfiguration(Configuration& newConfig)
{
    if (!canBindAnyAndSpecific())
    {
        // Broken OS; specific IP LSs will conflict with INADRR_ANY LSs
        closeConflictingSockets(newConfig);
    }

    // For every existing listen socket that exists in the new configuration,
    // the corresponding position in the lsExists array is marked PR_TRUE.
    // All other entries in the array will contain PR_FALSE
    //
    // After adding the new sockets to lsList_, we examine each index of
    // lsExists and if the value is PR_FALSE then we close the corresponding
    // listen socket in lsList_. The PR_FALSE entries denote those listen
    // sockets that exist in the current configuration but not in the new one.

    int nCurrentLSs = this->lsList_.length();
    PRBool* lsExists = NULL;
    
    if (nCurrentLSs > 0)
    {
        lsExists = new PRBool[nCurrentLSs];
        if (lsExists == NULL)
        {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ListenSockets_malloc));
            return PR_FAILURE;
        }

        for (int i = 0; i < nCurrentLSs; i++)
            lsExists[i] = PR_FALSE;
    }

    PRStatus status = this->addNewSockets(newConfig, lsExists, nCurrentLSs);
    if (status == PR_SUCCESS)
        status = this->closeOldSockets(lsExists, nCurrentLSs);

    if (status == PR_SUCCESS)
    {
        if (this->currentConfig_)
        {
            // Dispose of unneeded StatsManager slots
            for (int i = 0; i < this->currentConfig_->getLscCount(); i++)
            {
                ListenSocketConfig* currentLSC = this->currentConfig_->getLsc(i);
                if (!newConfig.getLsc(currentLSC->name))
                    StatsManager::freeListenSlot(currentLSC->name);
            }
        }
    }

    if (lsExists != NULL)
        delete [] lsExists;

    return status;
}

void
ListenSockets::closeConflictingSockets(Configuration& newConfig)
{
    ListenSocketConfig* newLSC = NULL;
    ListenSocketConfig* currentLSC = NULL;
    ListenSocket* currentLS = NULL;
    int nNewLSCs = newConfig.getLscCount();

    // Close specific IP LSs that would get in the way of new INADDR_ANY LSs
    for (int m = 0; m < nNewLSCs; m++)
    {
        newLSC = newConfig.getLsc(m);
        if (!newLSC->enabled)
            continue;

        int nSockets = this->lsList_.length();

        for (int i = (nSockets - 1); i >= 0; i--)
        {
            currentLS = (ListenSocket*)(this->lsList_[i]);
            currentLSC = currentLS->getConfig();  // refcount incremented

            if (currentLSC->hasExplicitIP() &&
                !newLSC->hasExplicitIP() &&
                newLSC->getFamily() == currentLSC->getFamily() &&
                newLSC->port == currentLSC->port)
            {
                // newLSC conflicts with currentLSC, so close currentLSC
                this->lsList_.removeAt(i);
                currentLS->close();
                delete currentLS;
            }

            currentLSC->unref();                  // decrement the refcount
        }
    }

    // Close INADDR_ANY LSs that could get in the way of new specific IP LSs
    int nSockets = this->lsList_.length();
    for (int i = (nSockets - 1); i >= 0; i--)
    {
        currentLS = (ListenSocket*)(this->lsList_[i]);
        currentLSC = currentLS->getConfig();  // refcount incremented

        if (!currentLSC->hasExplicitIP())
        {
            PRBool found = PR_FALSE;

            for (int m = 0; m < nNewLSCs; m++)
            {
                newLSC = newConfig.getLsc(m);
                if (!newLSC->enabled)
                    continue;

                if (*newLSC == *currentLSC)
                {
                    found = PR_TRUE;
                    break;
                }
            }

            if (!found)
            {
                // currentLSC doesn't exist in the new configuration
                this->lsList_.removeAt(i);
                currentLS->close();
                delete currentLS;
            }
        }

        currentLSC->unref();                  // decrement the refcount
    }
}

PRStatus
ListenSockets::addNewSockets(Configuration& newConfig, PRBool* lsExists, int nExistingLSs)
{
    int nErrors = 0;

    if (!canBindAnyAndSpecific())
    {
        // Since specific IP LSs can be attached to a parent INADDR_ANY LS,
        // add INADDR_ANY LSs before specific IP LSs
        nErrors += addNewSockets(newConfig, lsExists, nExistingLSs, PR_FALSE);
        nErrors += addNewSockets(newConfig, lsExists, nExistingLSs, PR_TRUE);
    }
    else
    {
        // To avoid INADDR_ANY LSs accepting connections intended for a
        // specific IP, add specific IP LSs before INADDR_ANY LSs
        nErrors += addNewSockets(newConfig, lsExists, nExistingLSs, PR_TRUE);
        nErrors += addNewSockets(newConfig, lsExists, nExistingLSs, PR_FALSE);
    }

    PRStatus status = PR_SUCCESS;
    if (nErrors)
    {
        status = PR_FAILURE;
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LSNotCreated_), nErrors);
    }

    return status;
}

int
ListenSockets::addNewSockets(Configuration& newConfig, PRBool* lsExists,
                             int nSockets, PRBool bExplicitIP)
{
    ListenSocketConfig* newLSC = NULL;
    ListenSocketConfig* currentLSC = NULL;
    ListenSocket* currentLS = NULL;

    int nNewLSCs = newConfig.getLscCount();

    int nErrors = 0;
 
    for (int m = 0; m < nNewLSCs; m++)
    {
        newLSC = newConfig.getLsc(m);
        if (!newLSC->enabled)
            continue;

        // Our caller will add new LSs in two passes. Skip this LSC if its LS
        // shouldn't be added in this pass.
        if (newLSC->hasExplicitIP() != bExplicitIP)
            continue;

        // Update stats for this LSC
        StatsListenNode* statsLSSNode;
        statsLSSNode = StatsManager::allocListenSlot(newLSC->name);
        if (statsLSSNode)
        {
            StatsListenSlot* statsLSS = &statsLSSNode->lsStats;
            PRNetAddr addr = newLSC->getAddress();
            memcpy(&statsLSS->address, &addr, sizeof(statsLSS->address));
            statsLSS->countAcceptors = newLSC->getConcurrency();
            statsLSS->flagSecure = newLSC->ssl.enabled;
            const int nVsIdSize = sizeof(statsLSS->defaultVsId);
            memset(statsLSS->defaultVsId, 0, nVsIdSize);
            strncpy(statsLSS->defaultVsId,
                    newLSC->defaultVirtualServerName.getStringValue(),
                    nVsIdSize -1);
            statsLSS->mode = STATS_LISTEN_ACTIVE;
        }

        PRBool found = PR_FALSE;
        for (int n = 0; n < lsList_.length(); n++)
        {
            currentLS = (ListenSocket*)(this->lsList_[n]);
            currentLSC = currentLS->getConfig();  // refcount incremented

            if (*newLSC == *currentLSC)
            {
                currentLS->setConfig(newLSC);
                found = PR_TRUE;
                if (n < nSockets)
                    lsExists[n] = PR_TRUE;
                currentLSC->unref();              // decrement the refcount
                break;
            }
            else if (currentLSC->hasIPSpecificConfig(newLSC))
            {
                found = PR_TRUE;
                if (n < nSockets)
                    lsExists[n] = PR_TRUE;
                currentLSC->unref();              // decrement the refcount
                break;
            }

            currentLSC->unref();                  // decrement the refcount
        }
        if (found == PR_FALSE)
        {
            if (this->activateNewLS(newLSC) == PR_FAILURE)
                nErrors++;
        }
    }

    return nErrors;
}

PRStatus
ListenSockets::closeOldSockets(PRBool* lsExists, int nSockets)
{
    int nErrors = 0;
    if (lsExists != NULL)
    {
        int i;

        // Send an interrupt to each of the Acceptor threads, thus giving
        // them an opportunity to detect that they have been terminated and
        // exit
        for (i = (nSockets - 1); i >= 0; i--)
        {
            if (lsExists[i] == PR_FALSE)
            {
                ListenSocket* currentLS = (ListenSocket*)(this->lsList_[i]);
                currentLS->stopAcceptors();
            }
        }

        // Wait for the Acceptor threads to terminate and then
        // close each of the socket descriptors
        for (i = (nSockets - 1); i >= 0; i--)
        {
            if (lsExists[i] == PR_FALSE)
            {
                ListenSocket* currentLS = (ListenSocket*)(this->lsList_[i]);
                ListenSocketConfig* currentLSC = currentLS->getConfig();

                if (this->lsList_.removeAt(i) == PR_FAILURE)
                    nErrors++;
                PRStatus status = currentLS->close();
                if (status == PR_FAILURE)
                    nErrors++;
                delete currentLS;

                currentLSC->unref();
            }
        }
    }

    PRStatus status = PR_SUCCESS;
    if (nErrors)
    {
        status = PR_FAILURE;
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LSNotClosed_), nErrors);
    }

    return status;
}

PRStatus
ListenSockets::activateNewLS(ListenSocketConfig* config)
{
    PRStatus status = PR_SUCCESS;
    if (config)
    {
        ListenSocket* newLS = new ListenSocket(config);

        status = newLS->create();
        if (status == PR_SUCCESS)
        {
            status = this->lsList_.append(newLS);
            if (status == PR_FAILURE)
            {
                ereport(LOG_FAILURE,
                        XP_GetAdminStr(DBT_ListenSockets_append));
            }
            else 
                status = newLS->startAcceptors(this->connQueue_);
        }
        else
        {
            delete newLS;
        }
    }
    else
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_ListenSocket_nullconfig));
        status = PR_FAILURE;
    }
    return status;
}

PRStatus
ListenSockets::closeAll(void)
{
    SafeLock guard(ListenSockets::lock_);

    PRStatus status = PR_SUCCESS;
    int nSockets = this->lsList_.length();
    int nErrors = 0;
    int i;

    for (i = 0; i < nSockets; i++)
    {
        ListenSocket* currentLS = (ListenSocket*)(this->lsList_[i]);
        status = currentLS->stopAcceptors();
    }

    for (i = 0; i < nSockets; i++)
    {
        ListenSocket* currentLS = (ListenSocket*)(this->lsList_[i]);

        status = currentLS->close();
        if (status == PR_FAILURE)
            nErrors++;

        delete currentLS;
    }

    if (nErrors > 0)
    {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_LSNotClosed_), nErrors);
        status = PR_FAILURE;
    }

    this->lsList_.flush();

    return status;
}

PRBool
ListenSockets::canBindAnyAndSpecific(void)
{
    static PRBool bindAnyAndSpecific = -1;

    if (bindAnyAndSpecific == -1)
    {
        bindAnyAndSpecific = conf_getboolean("BindAnyAndSpecific",
                                             BIND_ANY_AND_SPECIFIC);
    }

    return bindAnyAndSpecific;
}

#ifdef DEBUG
void
ListenSockets::printInfo(void) const
{
    cout << "<ListenSockets>" << endl;
    cout << "</ListenSockets>" << endl;
}
#endif
