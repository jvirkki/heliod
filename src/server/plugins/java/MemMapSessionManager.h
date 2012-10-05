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

#ifndef __MemMapControlFile__ 
#define __MemMapControlFile__ 
 
#include <limits.h>
#include "NSJavaUtil.h"
#include "MemMapFile.h" 
#include "NsprWrap/ReadWriteLock.h" 
#include "time.h" 
#include "base/sem.h" 
#include "LockManager.h"

//FORMAT 
// 
// [RECORD] 
//    timestamp[4],id[32],name[64],cookie[4]  
#define MAX_ENTRIES_PER_SESSION = 64 
 
class _NS_SERVLET_EXPORT MemMapSessionManager  
{ 
private: 
  PRUint32  _lockId;
  SEMAPHORE	_sem; 
  PRBool	_initialization_success;
  time_t	_initialization_time;
  
  const PRUintn _controlValidator; 
  typedef struct  
  { 
    PRUintn validator; // always set to 0xabcd 
    char name[IWS_MAX_SESSIONID_LEN]; 
    PRUintn cookie; 
    PRUintn back; 
    PRUintn forward; 
  } _controlblock; 
 
  const PRUintn _sessionValidator; 
  typedef struct  
  { 
    PRBool invalidated;
    PRUintn validator; // always set to 0xdcba 
    PRUintn inactivitytime; 
    time_t createtimestamp; 
    time_t lastaccesstime; 
    time_t prevreqendtime; 
    time_t timestamp; 
    PRUintn numitems; 
    char id[IWS_MAX_SESSIONID_LEN]; 
    PRUintn cookie; 
  } _sessionblock; 
 
  char *_tempdir; 
   
  MemMapFile *_sessionFile; 
  MemMapFile *_controlFile; 
  MemMapFile *_dataFile; 

  static const char *_sessionfilename; 
  static const char *_controlfilename; 
  static const char *_datafilename; 
  static PRBool _isSingleProcess; 
 
  LockManager *_lockMgr;
  PRUint32 _maxLocks;

  PRUintn _blocksize;
  PRUintn _maxSessions;
  PRUintn _maxValuesPerSession;
  PRUintn _defaultinactivitytime; 

  PRBool acquirePrimordialLock(SEMAPHORE sem);
  void releasePrimordialLock(SEMAPHORE sem);

  PRUintn _getControlBlock (const _sessionblock &sessionentry,  
                            const char *name, _controlblock &entry); 
  PRUintn _getSessionBlock (const char *id, _sessionblock &entry); 
  PRUintn _getSessionBlockAtLocation (PRUintn location, _sessionblock &entry); 
 
public: 
  MemMapSessionManager (const char *filelocation, PRUintn blocksize,  
                        PRUintn maxsessions, PRUintn maxValues,
                        PRUintn defaultinactivitytime,
                        const char *snContext, PRUint32 maxLocks);
  ~MemMapSessionManager (); 
 
public: 
  PRBool isSessionManagerConfigured();
  PRBool createSession (const char* id, PRUintn timeout, PRUintn &location, 
                        time_t &lastAccessTime, time_t &creationTime);
  PRBool getSession (const char* id, PRUintn &location);

  PRBool deleteSessionAtLocation ( PRUintn location); 
  PRBool deleteSession (const char* id); 
  PRBool invalidate (const char* id); 
  PRBool _isSessionEntryValid(PRUintn sessionLocation, 
                                    _sessionblock &sessionEntry);
  PRBool isSessionValid (const char* id); 
 
  PRBool setSessionItem (const char *id, const char *name, const void *data,  
                         PRUintn size); 
  PRBool getSessionItem (const char *id, const char *name, void *&data,  
                         PRUintn &size); 
  PRBool setSessionItemAtLocation (const char *name, const void *data,  
                         PRUintn size, PRUintn location); 
  PRBool getSessionItemAtLocation (const char *name, void *&data,  
                         PRUintn &size, PRUintn location);

  PRBool _setSessionItemByEntry ( const char *name,
                                  void *&data,  
                                  PRUintn &size,
                                  PRUintn sessionlocation,
                                  _sessionblock &sessionentry,
                                  PRBool reserveBlock);
  PRBool _getSessionItemByEntry ( const char *name,
                                  void *&data,  
                                  PRUintn &size,
                                  PRUintn sessionlocation,
                                  _sessionblock &sessionentry,
                                  PRBool locateOnly);

  PRBool setSessionDataAtLocation (const char *name, char *&data,
                         PRUintn &size, PRUintn location, PRUintn maxInactiveInterval, PRUintn prevReqEndTime);

  PRBool getSessionDataAtLocation (const char *name, char *&data,  
                         PRUintn &size, PRUintn location, 
                         time_t &lastAccessedTime, time_t &prevReqEndTime,
                         long &maxInactiveInterval, time_t &creationTime,
                         time_t &currentAccessTime, PRBool &isValid);

  void clearSessionItem (const char *id, const char *name); 
 
 
  PRBool getSessionItemNames (const char* id, char** buf, int& num); 
  int getNumSessionItems (const char* id); 
 
  time_t getCreationTime (const char* id); 
  time_t getTimeStamp (const char* id); 
  PRBool updateTimeStamp (const char* id); 
  PRUintn getSessionTimeOut (const char* id); 
  PRUintn setSessionTimeOut (const char* id, PRUintn timeout);
  PRUintn getBlockSize(void);
  int reaper();

  PRBool lockSession(const char *id, PRUintn location);
  PRBool unlockSession(const char *id, PRUintn location);
  
private:
  void _deleteSessionFromLocation(PRUintn location, _sessionblock &sessionEntry);
  int _reapExpiredSessions();
  void updateTimeStampAtLocation(PRUintn location);
}; 
 
#endif 
