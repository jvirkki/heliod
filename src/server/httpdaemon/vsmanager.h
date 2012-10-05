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

#ifndef VSMANAGER_H
#define VSMANAGER_H

class Configuration;
class VirtualServer;
class CriticalSection;
class GenericVector;

//-----------------------------------------------------------------------------
// VSListener
//-----------------------------------------------------------------------------

class VSListener {
public:
    virtual PRStatus initVS(VirtualServer* incoming, const VirtualServer* current) { return PR_SUCCESS; }
    virtual void destroyVS(VirtualServer* outgoing) { }
};

//-----------------------------------------------------------------------------
// VSManager
//-----------------------------------------------------------------------------

class VSManager {
public:
    static void init();

    // Register an object interested in tracking VSManager initVS() and
    // destroyVS() processing.  Listeners' initVS() methods are called in the
    // same order the listeners were registered in.  Listeners' destroyVS()
    // methods are called in reverse order (that is, the first listener to
    // register is the last listener called).
    static void addListener(VSListener* listener);

    // Management of per-VirtualServer* user data
    static int allocSlot();
    static void* setData(const VirtualServer* vs, int slot, void* data);
    static void* getData(const VirtualServer* vs, int slot);

    // Allow outsiders to know what we're doing
    static PRBool isInInitVS();
    static PRBool isInDestroyVS();

private:
    static void initEarly();
    static void setConfiguration(Configuration* incoming);
    static void copyUserData(VirtualServer* vsIncoming, const VirtualServer* vsCurrent);

    static int countConfigured; // Number of setConfiguration()s
    static int countSlots; // Number of per-VirtualServer* slots
    static Configuration* configuration; // Current configuration
    static CriticalSection* lock; // Serialize access to certain members
    static PRBool inInitVS; // Set when we're doing initVS() processing
    static PRBool inDestroyVS; // Set when we're doing destroyVS() processing

friend class VSConfigurationListener;
friend class VSUserDataListener;
};

#endif // VSMANAGER_H
