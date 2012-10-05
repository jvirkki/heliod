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

#ifndef FCGIPROCESS_H
#define FCGIPROCESS_H

#include "support/GenericList.h"

typedef struct ChildProcessData {
    PidType pid;
    PRIntervalTime epoch;   //process start time
} ChildProcessData;

class FcgiProcess {
public:
    FcgiProcess(FileDescPtr connFd, FILE *logFd, PRBool verbose);
    ~FcgiProcess();
    void initProcess();
    int getNumChildren() { return (children ? children->length() : 0); }
    static FcgiProcess *lookupChildPid(GenericList procList, PidType p);
    int operator==(const FcgiProcess& right) const;
    int operator!=(const FcgiProcess& right) const;
    PRStatus addChild(PidType cPid);
    void removeChild(PidType cPid);
    FileDescPtr getClientFd() { return (clientFd ? clientFd : NULL); }
    void addVsId(char *vsid);
    void removeVsId(char *vsid);
    PRBool containsVsId(char *vsid);
    int getNumVS() { return vsIdList.length(); }
    int getIntParameter(const char *val);
    ChildProcessData *getChild(int index) { return (children ? (ChildProcessData *)(*children)[index] : NULL); }
    void terminateChilds();
    PluginError execFcgiProgram(PRBool lifecycleCall = PR_FALSE);
    ChildProcessData *childExists(PidType p);

    int maxProcs;
    int minProcs;
    int currentNumFails;
    int totalNumFails;
    ProcessInfo procInfo;
    //PRFileDesc *serverFd;
    int serverFd;
#ifdef LINUX    
    PRBool createRequest;
#endif    

private:
    void initChildren();
    void printMsg(const char *msg);
    void sendErrorStatus(PluginError errorType);
    char *formWinArgs();
    char *getWindowsEnvironmentVars();
    PluginError hasAccess();

//    PidType *children; //stores child pids
    GenericList *children;   //stores ChildProcessData
    GenericList vsIdList; //stores VS references
    FileDescPtr clientFd;
    FILE *logFd;
    PRBool verboseSet;
    int procPriority;
};

#endif // FCGIPROCESS_H
