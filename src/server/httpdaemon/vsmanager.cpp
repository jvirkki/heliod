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

#include <nspr.h>

#include "base/ereport.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/vsmanager.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/statsmanager.h"
#include "NsprWrap/CriticalSection.h"
#include "support/GenericVector.h"

//-----------------------------------------------------------------------------
// Definitions of static member variables
//-----------------------------------------------------------------------------

int VSManager::countConfigured = 0;
int VSManager::countSlots = 0;
Configuration* VSManager::configuration = 0;
CriticalSection* VSManager::lock = 0;
PRBool VSManager::inInitVS = PR_FALSE;
PRBool VSManager::inDestroyVS = PR_FALSE;

//-----------------------------------------------------------------------------
// VSConfigurationListener
//-----------------------------------------------------------------------------

class VSConfigurationListener : public ConfigurationListener {
public:
    VSConfigurationListener(VSListener* listener)
    : listener(listener)
    {
    }

    PRStatus setConfiguration(Configuration* incoming, const Configuration* current)
    {
        PRStatus rv = PR_SUCCESS;

        // Let VSManager know about this (new?) Configuration
        VSManager::setConfiguration(incoming);

        // For every VirtualServer* in the new configuration...
        if (incoming) {
            for (int iVS = 0; (iVS < incoming->getVSCount()) && (rv == PR_SUCCESS); iVS++) {
                VirtualServer* vsIncoming = incoming->getVS(iVS);
                if (!vsIncoming->enabled)
                    continue;

                const VirtualServer* vsCurrent = current ? current->getVS(vsIncoming->name) : 0;
                if (vsCurrent && !vsCurrent->enabled)
                    vsCurrent = 0;

                // Call the VSListener
                VSManager::inInitVS = PR_TRUE;
                rv = listener->initVS(vsIncoming, vsCurrent);
                VSManager::inInitVS = PR_FALSE;
            }
        }

        return (rv == PR_SUCCESS) ? PR_SUCCESS : PR_FAILURE;
    }

    void releaseConfiguration(Configuration* outgoing)
    {
        // For every VirtualServer* in the outgoing configuration...
        for (int iVS = 0; iVS < outgoing->getVSCount(); iVS++) {
            // Get the outgoing VirtualServer*
            VirtualServer* vsOutgoing = outgoing->getVS(iVS);
            if (!vsOutgoing->enabled)
                continue;

            // Call the VSListener
            VSManager::inDestroyVS = PR_TRUE;
            listener->destroyVS(vsOutgoing);
            VSManager::inDestroyVS = PR_FALSE;
        }
    }

private:
    VSListener* listener;
};

//-----------------------------------------------------------------------------
// VSUserDataListener
//-----------------------------------------------------------------------------

class VSUserDataListener : public ConfigurationListener {
public:
    PRStatus setConfiguration(Configuration* incoming, const Configuration* current)
    {
        // For every VirtualServer* in the new configuration...
        if (incoming) {
            for (int iVS = 0; iVS < incoming->getVSCount(); iVS++) {
                VirtualServer* vsIncoming = incoming->getVS(iVS);
                const VirtualServer* vsCurrent = current ? current->getVS(vsIncoming->name) : 0;
                VSManager::copyUserData(vsIncoming, vsCurrent);
            }
        }

        return PR_SUCCESS;
    }
};

//-----------------------------------------------------------------------------
// VSManager::init
//-----------------------------------------------------------------------------

void VSManager::init()
{
    initEarly();
}

//-----------------------------------------------------------------------------
// VSManager::isInInitVS()
//-----------------------------------------------------------------------------

PRBool VSManager::isInInitVS()
{
    return inInitVS;
}

//-----------------------------------------------------------------------------
// VSManager::isInDestroyVS()
//-----------------------------------------------------------------------------

PRBool VSManager::isInDestroyVS()
{
    return inDestroyVS;
}

//-----------------------------------------------------------------------------
// VSManager:allocSlot
//-----------------------------------------------------------------------------

int VSManager::allocSlot()
{
    if (!lock) initEarly();

    // We should only be called during late init (i.e. first configuration) or
    // initVS processing
    if ((countConfigured > 1) && !inInitVS) {
        ereport(LOG_WARN, "Attempt to allocate per-VirtualServer* slot outside of init processing");
        return -1;
    }

    int slot = -1;

    lock->acquire();

    VirtualServer* vs = 0;
    if (configuration) {
        // Add another slot to every VS.  The default value at each slot is 0.
        int count = configuration->getVSCount();
        int i;
        for (i = 0; i < count; i++) {
            vs = configuration->getVS(i);
            if (vs->enabled) {
                PR_ASSERT(countSlots == vs->getUserVector().length());
                vs->getUserVector().append(0);
            }
        }
    }

    // Track the number of slots allocated
    slot = countSlots;
    countSlots++;

    lock->release();

    return slot;
}

//-----------------------------------------------------------------------------
// VSManager::setData
//-----------------------------------------------------------------------------

void* VSManager::setData(const VirtualServer* vs, int slot, void* data)
{
    void* set = 0;

    if (slot >= 0 && slot < countSlots) {
        vs->setUserData(slot, data);
        set = data;
    } else {
        ereport(LOG_WARN, "Attempt to set per-VirtualServer* data for unknown slot %d", slot);
    }

    return set;
}

//-----------------------------------------------------------------------------
// VSManager:getData
//-----------------------------------------------------------------------------

void* VSManager::getData(const VirtualServer* vs, int slot)
{
    void* data = 0;

    if (slot >= 0 && slot < countSlots) {
        data = vs->getUserData(slot);
    } else {
        ereport(LOG_WARN, "Attempt to get per-VirtualServer* data for unknown slot %d", slot);
    }

    return data;
}

//-----------------------------------------------------------------------------
// VSManager::addListener
//-----------------------------------------------------------------------------

void VSManager::addListener(VSListener* listener)
{
    if (!lock) initEarly();

    if (listener)
        ConfigurationManager::addListener(new VSConfigurationListener(listener));
}

//-----------------------------------------------------------------------------
// VSManager::initEarly
//-----------------------------------------------------------------------------

void VSManager::initEarly(void)
{
    if (!lock) {
        lock = new CriticalSection();

        // Add a ConfigurationListener that copies per-VS user data from old
        // VSs to new VSs.  This should be registered before any VSListeners.
        ConfigurationManager::addListener(new VSUserDataListener);
    }
}

//-----------------------------------------------------------------------------
// VSManager::setConfiguration
//-----------------------------------------------------------------------------

void VSManager::setConfiguration(Configuration* incoming)
{
    // Is this a new Configuration?
    if (incoming && incoming != configuration)
        VSManager::countConfigured++;

    // Remember which Configuration we're working on
    configuration = incoming;
}

//-----------------------------------------------------------------------------
// VSManager::copyUserData
//-----------------------------------------------------------------------------

void VSManager::copyUserData(VirtualServer* vsIncoming, const VirtualServer* vsCurrent)
{
    if (vsIncoming && vsCurrent) {
        // This VirtualServer was in the old configuration, too
        vsIncoming->getUserVector() = vsCurrent->getUserVector();
    } else if (vsIncoming) {
        // This is a new VirtualServer
        int count = countSlots;
        while (count--) vsIncoming->getUserVector().append(0);
    }
}
