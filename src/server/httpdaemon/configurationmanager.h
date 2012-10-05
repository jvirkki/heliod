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

#ifndef CONFIGURATIONMANAGER_H
#define CONFIGURATIONMANAGER_H

#include "nspr.h"                   // PRStatus
#include "NsprWrap/Thread.h"        // Thread class

class Configuration;
class ConfigurationManagerListener;
class ReadWriteLock;
class CriticalSection;
class GenericVector;
template<class T> class PSQueue;

//-----------------------------------------------------------------------------
// ConfigurationListener
//-----------------------------------------------------------------------------

class ConfigurationListener {
public:
    virtual PRStatus setConfiguration(Configuration* incoming, const Configuration* current) { return PR_SUCCESS; }
    virtual void releaseConfiguration(Configuration* outgoing) { }
};

//-----------------------------------------------------------------------------
// ConfigurationManager
//-----------------------------------------------------------------------------

class ConfigurationManager : public Thread {
public:
    static void init();

    // Obtain a reference to the current configuration.  The caller must call
    // the returned Configuration*'s unref() method when done with the
    // Configuration object.
    static Configuration* getConfiguration();

    // Make a new configuration active.  ConfigurationManager will call unref()
    // on any old configuration and ref() on the incoming configuration.
    // ConfigurationManager also installs a ConfigurationListener via
    // incoming->setListener().
    static PRStatus setConfiguration(Configuration* incoming);

    // Register an object interested in tracking ConfigurationManager
    // setConfiguration() and releaseConfiguration() processing.  Listeners'
    // setConfiguration() methods are called in the same order the listeners
    // were registered in.  Listeners' releaseConfiguration() methods are
    // called in reverse order (that is, the first listener to register is the
    // last listener called).
    static void addListener(ConfigurationListener* listener);

    // Number of configurations that have been set
    static PRUint32 getConfigurationCount() { return countConfigurations; }

    // Executes in a separate thread and implements the code for releasing a 
    // Configuration. It waits for Configuration objects to be put on the
    // "release" queue by the releaseConfiguration method
    // The memory associated with the Configuration object that is released
    // is deleted in this method. The releaseConfiguration method of this
    // class is called only when the ref count of the Configuration drops
    // to 0, so the memory for the Configuration can safely be deleted.
    void run(void);

private:
    // Early initialization (creation of static locks, etc.)
    static void initEarly();

    // Only allow methods of this class to create instances of itself
    // so that the <tt>run</tt> method that releases configurations can be
    // run in a separate thread (that is bound to an LWP)
    ConfigurationManager(void);

    // Called by a Configuration object when its reference count drops to 0.
    // Releasing a configuration can potentially be a time-consuming task
    // as every ConfigurationListener's releaseConfiguration method will
    // be invoked. Hence this method merely sets a condition variable
    // and the actual release is executed in a separate thread (in the
    // run method of this class)
    static void releaseConfiguration(ConfigurationManagerListener* outgoing);

    static CriticalSection* lockConfiguration;
    static CriticalSection* lockSetConfiguration;
    static Configuration* configuration;
    static ReadWriteLock* lockVectorListeners;
    static GenericVector* vectorListeners;
    static CriticalSection* lockCallListeners;
    static ConfigurationListener* listenerConfiguration;

    static ConfigurationManager* releaserThread;

    // A FIFO list onto which the releaseConfiguration method enqueues
    // Configuration objects that have been "released" by users.
    // The "releaser" thread (executing the run() method of this class) 
    // dequeues these objects in the order in which they were enqueued and
    // after invoking releaseConfiguration on each of the listeners it then
    // deletes the memory associated with the configuration object.
    static PSQueue<ConfigurationManagerListener*>* releasedConfigs;

    static PRUint32 countConfigurations;

friend class ConfigurationManagerListener;
};

#endif // CONFIGURATIONMANAGER_H
