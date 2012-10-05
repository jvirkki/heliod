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

// Control File Structure 
// 
//  Record Structure 
//    time stamp[4],cookie[32],value name[32],data address[4]   

#include <stdio.h> 

#include "NSJavaUtil.h" 
#include "MemMapSessionManager.h" 
#include "prlog.h" 
#include "nstime/nstime.h" 
#include "base/SemPool.h"
#include "base/util.h"
#include <private/pprio.h>               // PR_LockFile

#ifdef XP_UNIX
#include <limits.h>                      // PATH_MAX
#endif


#ifndef XP_PC 
#define MAX_PATH 4048 
#endif 

#ifdef DEBUG
static int IWS_LOCK_DEBUG = 0;    // set to 1 from dbx to enable dbg msgs
#endif

const char *MemMapSessionManager::_sessionfilename = "IWSSession.dat"; 
const char *MemMapSessionManager::_controlfilename = "IWSSessionControl.dat"; 
const char *MemMapSessionManager::_datafilename = "IWSSessionData.dat"; 

// Names for files used for PR_LockFile = "$(IWS_LOCK_PREFIX)$(SessionID)"
static const char* IWS_LOCK_PREFIX = "/iwslock.";

// local util class 
class SemLock 
{ 
private : 
    LockManager *_lockMgr;
    PRUint32 _lockId;
public : 
    SemLock (LockManager *lockMgr, PRUint32 lockId); 
    ~SemLock (); 
}; 

int MemMapSessionManager::_isSingleProcess; 

MemMapSessionManager::MemMapSessionManager 
( 
 const char *filelocation,  
 PRUintn blocksize,  
 PRUintn maxsessions,  
 PRUintn maxValues,
 PRUintn defaultinactivitytime,
 const char *snContext, 
 PRUint32 maxLocks
 ) :
_initialization_success(PR_FALSE),
_controlFile(NULL), 
_dataFile(NULL), 
_sessionFile(NULL), 
_blocksize(blocksize),
_maxSessions (maxsessions),
_maxValuesPerSession (maxValues),
_controlValidator (0xabcd), 
_sessionValidator (0xdcba),
_maxLocks (maxLocks)
{ 
     _tempdir = strdup(system_get_temp_dir());

     if (_tempdir == NULL)
     {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_MEMORY_ALLOCATION_ERROR");
        NSJavaUtil::log(LOG_CATASTROPHE, logMsg);
        FREE(logMsg);
        return;
     }

	_sem = SemPool::get ("SessionSem"); 
	_isSingleProcess = NSJavaUtil::isSingleProcess (); 
	if (acquirePrimordialLock(_sem) == PR_FALSE)
    {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_CANNOT_ACQUIRE_PRIMORDIAL");
        NSJavaUtil::log(LOG_CATASTROPHE, logMsg);
        FREE(logMsg);
        return;
    }
	
	char filename[MAX_PATH]; 
	PRBool	done = PR_FALSE;

	_defaultinactivitytime = defaultinactivitytime; 
	
    // we use one additional lock for protecting the MMap'ed memory.
    // rest of the locks are used to protect individual session locks;
    // the id for the global lock is the _maxLocks value
    _lockMgr = new LockManager(snContext, (_maxLocks+1));
    if (_lockMgr == NULL)
    {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_CANNOT_CREATE_LOCK_MANAGER");
        NSJavaUtil::log(LOG_CATASTROPHE, logMsg);
        FREE(logMsg);
        return;
    }

    _lockId = maxLocks;

	_initialization_time = ft_time();
	
	util_snprintf (filename, sizeof(filename), "%s/%s", filelocation, _sessionfilename);     
	_sessionFile = new MemMapFile (filename, sizeof (_sessionblock), maxsessions, done);
	if (done != PR_TRUE)
		goto bail;
	
	util_snprintf (filename, sizeof(filename), "%s/%s", filelocation, _controlfilename);     
	_controlFile = new MemMapFile(filename, sizeof (_controlblock), maxsessions * maxValues, done); 
	if (done != PR_TRUE)
		goto bail;
	
	util_snprintf (filename, sizeof(filename), "%s/%s", filelocation, _datafilename);     
	_dataFile = new MemMapFile (filename, blocksize, maxsessions * maxValues, done); 
	if (done != PR_TRUE)
		goto bail;

	_initialization_success = PR_TRUE;

	/*
	 * Session at location 0 is special, it's a bootstrap session.
	 * Just update the time so that it doesn't expire
	 */
	updateTimeStampAtLocation(0);

    // reap the expired sessions if any at startup
    _reapExpiredSessions();

bail:
    releasePrimordialLock(_sem);
}

MemMapSessionManager::~MemMapSessionManager() 
{ 
 	if (_lockMgr != NULL) 
    {
        delete _lockMgr;
        _lockMgr = NULL;
    }
   
	if (_sessionFile != NULL) 
		delete _sessionFile; 
	
	if (_controlFile != NULL) 
		delete _controlFile; 
	
	if (_dataFile != NULL) 
		delete _dataFile; 

 	if (_tempdir != NULL) 
		free(_tempdir); 
} 

PRBool
MemMapSessionManager::acquirePrimordialLock(SEMAPHORE sem)
{
    PRBool ret = PR_TRUE;

	if (_isSingleProcess == PR_FALSE) 
	{ 
		int rval = Sem_grab (sem);

		if (rval == SEM_ERROR) {
		  char* logMsg =  get_message(j2eeResourceBundle,
                                              "j2ee.MemMapSessionManager.ERR_SEM_GRAB_FAILED");
		  NSJavaUtil::log(LOG_FAILURE, logMsg, rval, errno);
		  FREE(logMsg);
		  ret = PR_FALSE;
		}
		NS_JAVA_ASSERT (rval != SEM_ERROR); 
	} 

    return ret;
}

void 
MemMapSessionManager::releasePrimordialLock(SEMAPHORE sem)
{
	if (_isSingleProcess == PR_FALSE) 
	{ 
		Sem_release (sem); 
	} 
}

PRBool
MemMapSessionManager::isSessionManagerConfigured()
{
	return (_initialization_success);
}

PRBool
MemMapSessionManager::createSession 
( 
 const char *id,
 PRUintn timeout,
 PRUintn &location,
 time_t &lastAccessTime,
 time_t &creationTime
 ) 
{
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionentry; 
	PRUintn sessionLocation;

	//prepare a session entry template and insert into the session file.
    sessionentry.invalidated = PR_FALSE;
    sessionentry.prevreqendtime = 0;
	sessionentry.lastaccesstime = sessionentry.timestamp = 
                                     sessionentry.createtimestamp = ft_time(); 
	sessionentry.inactivitytime = timeout; 
	sessionentry.validator = _sessionValidator; 
	sessionentry.numitems = 0; 
	strcpy (sessionentry.id, id);

	sessionentry.cookie = _controlFile->getMaxBlocks (); 
	sessionLocation = _sessionFile->setEntry (&sessionentry, 
		sizeof (_sessionblock)); 

	if (sessionLocation >= _sessionFile->getMaxBlocks ())  
	{ 
		//cleanup data 
		//failed 
		sessionentry.validator = 0; 
		return PR_FALSE; 
	} 

    location = sessionLocation;
    lastAccessTime = sessionentry.lastaccesstime;
    creationTime = sessionentry.createtimestamp;

	return PR_TRUE; 
}

PRBool
MemMapSessionManager::lockSession(const char *id, PRUintn location)
{
#ifdef XP_UNIX
    // session locking is disabled when maxLocks is 0.
    if (_maxLocks == 0)
        return PR_TRUE;

    PRStatus status = _lockMgr -> lock(location % _maxLocks);

    if (status == PR_SUCCESS)
    {
#ifdef DEBUG
      if (IWS_LOCK_DEBUG) {
          char* logMsg =  get_message(j2eeResourceBundle,
                                      "j2ee.MemMapSessionManager.INFO_ACQUIRED_LOCK_FOR_SESSION");
          NSJavaUtil::log(LOG_INFORM, logMsg, id);
          FREE(logMsg);
      }
#endif
        return PR_TRUE;
    }
    else
    {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_ACQUIRING_LOCK_FOR_SESSION");
        NSJavaUtil::log(LOG_FAILURE, logMsg, errno, id);
        FREE(logMsg);
        return PR_FALSE;
    }
#endif
    return PR_FALSE;
}

PRBool
MemMapSessionManager::unlockSession(const char *id, PRUintn location)
{
#ifdef XP_UNIX
    // session locking is disabled when maxLocks is 0.
    if (_maxLocks == 0)
        return PR_TRUE;

    PRStatus status = _lockMgr -> unlock(location % _maxLocks);

    if (status == PR_SUCCESS)
    {
#ifdef DEBUG
      if (IWS_LOCK_DEBUG) {
          char* logMsg =  get_message(j2eeResourceBundle,
                                      "j2ee.MemMapSessionManager.INFO_RELEASED_LOCK_FOR_SESSION");
          NSJavaUtil::log(LOG_INFORM, logMsg, id);
          FREE(logMsg);
      }
#endif
        return PR_TRUE;
    }
    else
    {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_RELEASING_LOCK_FOR_SESSION");
        NSJavaUtil::log(LOG_FAILURE, logMsg, errno, id);
        FREE(logMsg);
        return PR_FALSE;
    }
#else
    return PR_TRUE;
#endif
}

void
MemMapSessionManager::_deleteSessionFromLocation(
	PRUintn		location,
	_sessionblock	&sessionEntry)
{
	/* No locking done here, should be done by the calle */

	int		num = sessionEntry.numitems;
	PRUintn		ignore; 
	_controlblock	c_entry; 
	void		*temp = (void *) &c_entry;
	
	PRUintn pointer = sessionEntry.cookie; 
	for (int i = 0; (i < num) && (pointer < _controlFile->getMaxBlocks()); i++) { 
			
		if (_controlFile->getEntry(pointer, temp, ignore) == PR_TRUE) { 
			// clear the data block associated with this ctrl blk
			
			_dataFile->clearEntry(c_entry.cookie); 
			_controlFile->clearEntry(pointer); 

			pointer = c_entry.forward; 
		} else { 
			//something wrong 
			break; 
		} 
	} 
		
	sessionEntry.validator = 0;
	
	_sessionFile->setEntry(location, &sessionEntry, sizeof(_sessionblock)); 
	_sessionFile->clearEntry(location);
}

PRBool  
MemMapSessionManager::deleteSession 
( 
 const char *id 
 ) 
{ 
	SemLock semLock(_lockMgr, _lockId); 
	
	_sessionblock	sessionEntry; 
	
	PRUintn sessionlocation = _getSessionBlock(id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks()) { 
		// found. continue 
		// run thru all session items and delete them.

		_deleteSessionFromLocation(sessionlocation, sessionEntry);
		return PR_TRUE;
	}
	
	return PR_FALSE; 
}

PRBool  
MemMapSessionManager::deleteSessionAtLocation 
( 
    PRUintn location
 ) 
{ 
	SemLock semLock(_lockMgr, _lockId); 
	
	_sessionblock	sessionEntry; 

    // get the session block at the specified location.
    PRUintn sessionLocation = _getSessionBlockAtLocation (location,sessionEntry);

    // has this been cleaned up already? if so, nothing to do.
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;

    // Cannot delete a valid session!
    if (_isSessionEntryValid(sessionLocation, sessionEntry))
        return PR_FALSE;
    
	_deleteSessionFromLocation(sessionLocation, sessionEntry);
	return PR_TRUE;
}

int
MemMapSessionManager::_reapExpiredSessions()
{
	// Locking should be done by the caller

	_sessionblock	s_entry;
	void		*temp = (void *) &s_entry;
	
	PRUintn		ignore;
	PRBool		delete_it = PR_FALSE;

	time_t		timeNow;
	time_t		timeSlack = 300;	// 300 seconds
	PRUintn		max = _sessionFile->getMaxBlocks();
    int         count = 0, nActive = 0;

	for (PRUintn n = 1; n < max; n++) {

		if (_sessionFile->getEntry(n, temp, ignore) != PR_TRUE)
			continue;
		
		timeNow = ft_time();
		delete_it = PR_FALSE;
		
		// Decide if this session needs to be reaped
		if (s_entry.validator != _sessionValidator) {
			delete_it = PR_TRUE;
		} else {
			if (s_entry.invalidated == PR_TRUE ||
                (s_entry.prevreqendtime > 0 &&
                ( (timeNow - s_entry.prevreqendtime) >
			      (((time_t) s_entry.inactivitytime) + timeSlack))) )
				delete_it = PR_TRUE;
            else
                nActive++;
		}

		// Delete this session
		if (delete_it == PR_TRUE) {
			_deleteSessionFromLocation(n, s_entry);
            count++;
        }
	}

    if (count > 0) {
        char* logMsg =  get_message(j2eeResourceBundle,
                                    "j2ee.MemMapSessionManager.ERR_REAPER_SESSION_EXPIRED");
        NSJavaUtil::log(LOG_INFORM, logMsg, count, nActive);
        FREE(logMsg);
    }

    return count;
}

int
MemMapSessionManager::reaper()
{
	SemLock semLock(_lockMgr, _lockId); 
	return _reapExpiredSessions();
}

void
MemMapSessionManager::updateTimeStampAtLocation(PRUintn location) {
	// Locking should be done by the caller

	_sessionblock	s_entry;
	void		*temp = (void *) &s_entry;
	
	PRUintn		ignore;
	
	if (_sessionFile->getEntry(location, temp, ignore) == PR_TRUE) {
		s_entry.timestamp = ft_time();

        // we set the first 'lastaccesstime' to be the creationtime
		s_entry.lastaccesstime = s_entry.timestamp;
		_sessionFile->setEntry(0, temp, sizeof(_sessionblock));
	}
}
	
PRBool  
MemMapSessionManager::setSessionItem 
( 
 const char *id,  
 const char *name,  
 const void *data,  
 PRUintn size
 ) 
{
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionentry; 
	
	PRUintn sessionLocation = _getSessionBlock (id,sessionentry);
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;
    
    void *buf = (void *)data;
    return _setSessionItemByEntry(name, buf, size, 
                                  sessionLocation, sessionentry, PR_FALSE);
}

PRBool  
MemMapSessionManager::setSessionItemAtLocation
( 
 const char *name,  
 const void *data,  
 PRUintn size,
 PRUintn location
 ) 
{
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionentry; 
	PRUintn sessionLocation;
    
    // get the session block at the specified location.
    sessionLocation = _getSessionBlockAtLocation (location,sessionentry);
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;
    
    void *buf = (void *)data;
    return _setSessionItemByEntry(name, buf, size, location, sessionentry, PR_FALSE);
}

PRBool
MemMapSessionManager::_setSessionItemByEntry
(
 const char *name,
 void *&data,
 PRUintn &size,
 PRUintn sessionlocation,
 _sessionblock &sessionentry,
 PRBool reserveBlock
 ) 
{
	_controlblock controlentry; 

	// caller has the session lock, and the right location.
	// try insert the item in the control/data files.
	
	PRUintn controllocation = _getControlBlock (sessionentry, 
		name, controlentry); 
	if (controllocation < _controlFile->getMaxBlocks ())  
	{ 
		//cerr << "\tfound control black" << endl; 
		
		// 
		//just change entry 
		// 
        if (reserveBlock)
		    _dataFile->reserveEntry (controlentry.cookie, data, size); 
        else
		    _dataFile->setEntry (controlentry.cookie, data, size); 
		
		_sessionFile->setEntry (sessionlocation, 
			&sessionentry, sizeof (_sessionblock));       
	}  
	else  
	{ 
		if (sessionentry.numitems == _maxValuesPerSession) // limit reached!
		{
			char* logMsg =  get_message(j2eeResourceBundle,
                                                    "j2ee.MemMapSessionManager.ERR_CURRENT_LIMIT_EXCEEDED");
			NSJavaUtil::log (LOG_INFORM, logMsg, _maxValuesPerSession);
			FREE(logMsg);
			return PR_FALSE;
		}
		//cerr << "\t\tno control block" << endl; 
		// 
		//add data 
		// 
		PRUintn datalocation;
        
        if (reserveBlock)
            datalocation = _dataFile->reserveEntry (data, size); 
        else
            datalocation = _dataFile->setEntry (data, size); 

		if (datalocation >= _dataFile->getMaxBlocks ())  
		{ 
			//failed 
			return PR_FALSE; 
		} 
		// 
		//create a control entry for that name 
		// 
		strcpy (controlentry.name, name); 

		controlentry.validator = _controlValidator; 
		controlentry.cookie = datalocation; 
		controlentry.forward = sessionentry.cookie; 
		controlentry.back = _controlFile->getMaxBlocks (); 
		controllocation = _controlFile->setEntry (&controlentry, 
			sizeof (_controlblock)); 
		if (controllocation >= _controlFile->getMaxBlocks ())  
		{ 
			//cleanup data 
			controlentry.validator = 0;  
			//failed 
			return PR_FALSE; 
		} 
		_controlblock existingcontrolentry; 
		void *temp = (void *) &existingcontrolentry; 
		PRUintn dummy; 
		if ((sessionentry.cookie < _controlFile->getMaxBlocks ()) && 
			(_controlFile->getEntry (sessionentry.cookie, temp, dummy) == PR_TRUE)) 
		{ 
			existingcontrolentry.back = controllocation; 
			//        existingcontrolentry.forward = _controlFile->getMaxBlocks (); 
			
			_controlFile->setEntry (sessionentry.cookie, 
				&existingcontrolentry,  
				sizeof (_controlblock)); 
		} 
		//set the session entry 
		sessionentry.cookie = controllocation; 
		// inc num items in current session 
		sessionentry.numitems++; 
		_sessionFile->setEntry (sessionlocation, &sessionentry,  
			sizeof (_sessionblock)); 
	} 

    return PR_TRUE; 
}

// set the session-related data, i.e. HttpSession object blob along with the 
// inactivity interval that can be set by the servlet.
PRBool MemMapSessionManager::setSessionDataAtLocation (const char *name,
                                                       char *&data,
                                                       PRUintn &size,
                                                       PRUintn location, 
                                                       PRUintn maxInactiveInterval,
                                                       PRUintn prevReqEndTime)
{
    PRBool ret = PR_TRUE;

	SemLock semLock (_lockMgr, _lockId); 

	_sessionblock sessionEntry;
	PRUintn sessionLocation;
    
    // get the session block at the specified location
    sessionLocation = _getSessionBlockAtLocation (location, sessionEntry);
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;

	sessionEntry.inactivitytime = maxInactiveInterval;
	sessionEntry.prevreqendtime = prevReqEndTime;
	ret = _sessionFile->setEntry (sessionLocation, 
				      &sessionEntry, sizeof (_sessionblock)); 

    if (ret == PR_TRUE) {
        void *ptr;
        ret = _setSessionItemByEntry(name, ptr, size, 
                                     sessionLocation, sessionEntry, PR_TRUE);
        if (ret == PR_TRUE)
            data = (char *)ptr;
    }

    return ret;
}

PRBool MemMapSessionManager::getSessionDataAtLocation (const char *name, 
                                                       char *&data,  
                                                       PRUintn &size, 
                                                       PRUintn location, 
                                                       time_t &lastAccessedTime, 
                                                       time_t &prevReqEndTime,
                                                       long &maxInactiveInterval, 
                                                       time_t &creationTime,
                                                       time_t &currentAccessTime,
                                                       PRBool &isValid)
{
    PRBool ret = PR_TRUE;
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionEntry; 
	PRUintn sessionLocation;

    // first get the session block at the specified location
    sessionLocation = _getSessionBlockAtLocation (location, sessionEntry); 
    if (sessionLocation >= _sessionFile->getMaxBlocks()) {
        isValid = PR_FALSE;
        return PR_FALSE;
    }

    if ((sessionEntry.validator == _sessionValidator) &&
        sessionEntry.invalidated == PR_TRUE) {
        isValid = PR_FALSE;
        return PR_FALSE;
    }

    // check if this session is still valid?
	time_t timeNow = ft_time (); 
    time_t reqend = sessionEntry.prevreqendtime;
	if ((sessionEntry.validator == _sessionValidator) && 
        sessionEntry.prevreqendtime > 0 &&
		((timeNow - reqend) > (time_t) sessionEntry.inactivitytime))
	{
        isValid = PR_FALSE;
        return PR_FALSE;
    }

    // update the lastaccesstime and current timestamps in tandem.
	sessionEntry.lastaccesstime = sessionEntry.timestamp; 
	sessionEntry.timestamp = ft_time();
	ret = _sessionFile->setEntry (sessionLocation, &sessionEntry, 
                                            sizeof (_sessionblock)); 

    // get the session item (or the attribute) by the given name
    void *ptr;
    if (ret == PR_TRUE && 
       (ret = _getSessionItemByEntry(name, ptr, size, 
                  sessionLocation, sessionEntry, PR_TRUE)) == PR_TRUE) {
        lastAccessedTime = sessionEntry.lastaccesstime;
        prevReqEndTime = sessionEntry.prevreqendtime;
        maxInactiveInterval = sessionEntry.inactivitytime;
        creationTime = sessionEntry.createtimestamp;
        currentAccessTime = sessionEntry.timestamp;
        data = (char *)ptr;
    }

    return ret;
}

PRBool
MemMapSessionManager::getSession
(
 const char *id,
 PRUintn &location)
{

	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionEntry; 
	PRUintn sessionLocation = _getSessionBlock (id, sessionEntry);
	if (sessionLocation >= _sessionFile->getMaxBlocks ())  
		return PR_FALSE; 
	
    location = sessionLocation;
    return PR_TRUE;
}

PRBool  
MemMapSessionManager::getSessionItem 
( 
 const char *id,  
 const char *name,  
 void *&data,  
 PRUintn &size
 )  
{
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionEntry; 
	PRUintn sessionLocation = _getSessionBlock (id, sessionEntry); 
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;

	return _getSessionItemByEntry(name, data, size,
                                  sessionLocation, sessionEntry, PR_FALSE);
}

PRBool  
MemMapSessionManager::getSessionItemAtLocation 
( 
 const char *name,  
 void *&data,  
 PRUintn &size,
 PRUintn location
 )  
{
	SemLock semLock (_lockMgr, _lockId); 
	
	_sessionblock sessionEntry; 
	PRUintn sessionLocation = _getSessionBlockAtLocation (location, sessionEntry); 
    if (sessionLocation >= _sessionFile->getMaxBlocks())
        return PR_FALSE;

	return _getSessionItemByEntry(name, data, size, 
                                  sessionLocation, sessionEntry, PR_FALSE);
}

PRBool
MemMapSessionManager::_getSessionItemByEntry
( 
 const char *name,  
 void *&data,  
 PRUintn &size,
 PRUintn sessionlocation,
 _sessionblock &sessionentry,
 PRBool locateOnly
 )  
{
	if (sessionlocation >= _sessionFile->getMaxBlocks ())  
	{ 
		data = NULL; 
		size = 0; 
		return PR_FALSE; 
	} 
	
	_controlblock controlentry; 
	PRUintn controllocation = _getControlBlock (sessionentry, name, controlentry); 
	if (controllocation >= _controlFile->getMaxBlocks ())  
	{ 
		data = NULL; 
		size = 0; 
		return PR_FALSE; 
	} 
	
    if (locateOnly)
	    return _dataFile->locateEntry (controlentry.cookie, data, size); 
    else
	    return _dataFile->getEntry (controlentry.cookie, data, size); 
}

void  
MemMapSessionManager::clearSessionItem 
( 
 const char *id,  
 const char *name 
 )  
{ 
	SemLock semLock (_lockMgr, _lockId); 

	_sessionblock sessionentry; 
	PRUintn sessionlocation = _getSessionBlock (id,sessionentry); 
	if (sessionlocation >= _sessionFile->getMaxBlocks ())  
	{ 
		return; 
	} 
	
	_controlblock controlentry; 
	PRUintn controllocation = _getControlBlock(sessionentry, name, controlentry); 
	if (controllocation >= _controlFile->getMaxBlocks())  
	{ 
		return; 
	} 
	
	//fix the control entries 
	if (controlentry.forward < _controlFile->getMaxBlocks())  
	{ 
		_controlblock forwardcontrolentry; 
		
		void *temp = (void *) &forwardcontrolentry; 
		PRUintn dummy; 
		if (_controlFile->getEntry (controlentry.forward,temp, dummy) == PR_TRUE)  
		{ 
			//something wrong 
		} 
		
		forwardcontrolentry.back = controlentry.back; 
		
		_controlFile->setEntry (controlentry.forward,  
			&forwardcontrolentry, sizeof(_controlblock)); 
	} 
	if (controlentry.back < _controlFile->getMaxBlocks ())  
	{ 
		_controlblock backcontrolentry; 
		
		void *temp = (void *) &backcontrolentry; 
		PRUintn dummy; 
		if (_controlFile->getEntry (controlentry.back,temp, dummy) == PR_TRUE)  
		{ 
			//something wrong 
		} 
		
		backcontrolentry.forward = controlentry.forward; 
		
		_controlFile->setEntry (controlentry.back, 
			&backcontrolentry, sizeof (_controlblock)); 
	} 
	
	//check if session pointer changes 
	if (sessionentry.cookie == controllocation)  
	{ 
		sessionentry.cookie = controlentry.forward; 
	} 
	
	//do session time stamp 
	sessionentry.timestamp = ft_time (); 
	sessionentry.numitems--;

	_sessionFile->setEntry (sessionlocation, &sessionentry,  
		sizeof(_sessionblock)); 
	
	//clear the control file 
	_controlFile->clearEntry (controllocation); 
	//clear the data file 
	_dataFile->clearEntry (controlentry.cookie); 
} 

PRUintn  
MemMapSessionManager::_getControlBlock 
( 
	const _sessionblock &sessionentry,  
	const char *name,  
	_controlblock &entry 
)  
{ 
	PRUintn pointer = sessionentry.cookie; 
	void *temp = (void *) &entry; 
	PRUintn dummy; 
	
	while(pointer < _controlFile->getMaxBlocks() )  
	{ 
		if (_controlFile -> getEntry (pointer, temp, dummy) == PR_TRUE)  
		{ 
			if (strcmp (entry.name, name) == 0)  
			{ 
				if (entry.validator != _controlValidator) 
					return _controlFile -> getMaxBlocks (); 

				return pointer; 
			}
		}
		else
			break;	// bail out

		pointer = entry.forward; 
	} 
	
	return _controlFile->getMaxBlocks (); 
} 

PRUintn MemMapSessionManager::_getSessionBlock (const char *id, _sessionblock &entry)  
{ 
	PRUintn	ignore;
	void	*temp = (void *) &entry;

	PRUintn	max = (_sessionFile->getMaxBlocks());
	for (PRUintn i = 0; i < max ; i++)
	{
		if (_sessionFile->getEntry(i, temp, ignore) == PR_TRUE)
		{
			if (strcmp (entry.id, id) == 0)
			{ 
				if (entry.validator != _sessionValidator) 
					return _sessionFile->getMaxBlocks();
				
				return i; 
			} 
		} 
	} 
	
	return _sessionFile->getMaxBlocks (); 
} 

PRUintn MemMapSessionManager::_getSessionBlockAtLocation (PRUintn location, _sessionblock &entry)  
{
	PRUintn	ignore;
	void	*temp = (void *) &entry;

	if (_sessionFile->getEntry(location, temp, ignore) == PR_TRUE) {
	    if (entry.validator != _sessionValidator) 
		    return _sessionFile->getMaxBlocks();
				
		return location; 
    }

	return _sessionFile->getMaxBlocks (); 
}

PRBool MemMapSessionManager::_isSessionEntryValid(PRUintn sessionLocation, 
                                    _sessionblock &sessionEntry)
{
	if (sessionLocation < _sessionFile->getMaxBlocks ()) 
	{ 
	    if (sessionEntry.validator != _sessionValidator)
		    return PR_FALSE;

		time_t timeNow = ft_time (); 
		if ((sessionEntry.validator == _sessionValidator) && 
            sessionEntry.invalidated == PR_FALSE &&
            sessionEntry.prevreqendtime > 0 &&
			((timeNow - sessionEntry.prevreqendtime ) < 
			(time_t) sessionEntry.inactivitytime)) 
		{
			return PR_TRUE;
		}
	}   

    return PR_FALSE;
}

PRBool MemMapSessionManager::isSessionValid (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 

	PRUintn sessionLocation = _getSessionBlock (id, sessionEntry); 
    return _isSessionEntryValid(sessionLocation, sessionEntry);
} 

PRBool MemMapSessionManager::getSessionItemNames (const char* id, char** buf, 
                                                  int &num) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 

	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 

	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		num = sessionEntry.numitems; 

		PRUintn pointer = sessionEntry.cookie; 
		int i = 0;

		for ( ; (i < num) && (pointer < _controlFile->getMaxBlocks ()); i++) 
		{ 
			_controlblock controlEntry; 
			void *temp = (void *)&controlEntry; 
			PRUintn dummy; 
			
			if (_controlFile->getEntry (pointer, temp, dummy) == PR_TRUE)  
			{ 
				char* temp = new char [strlen (controlEntry.name) + 1];
				strcpy (temp, controlEntry.name);
				buf [i] = temp;
			} 
			else 
			{ 
				buf [i] = NULL; 
				//something wrong 
			} 
			pointer = controlEntry.forward; 
		} 
		num = i;

		_sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
		
		return PR_TRUE; 
	} 
	return PR_FALSE; 
} 

int MemMapSessionManager::getNumSessionItems (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		_sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
		return sessionEntry.numitems; 
	} 
	return 0;
} 

time_t MemMapSessionManager::getCreationTime (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		_sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
		return sessionEntry.createtimestamp; 
	} 
	return -1; 
} 

time_t MemMapSessionManager::getTimeStamp (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		return sessionEntry.lastaccesstime;
	} 
	return -1; 
} 


PRBool MemMapSessionManager::updateTimeStamp (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		sessionEntry.lastaccesstime = sessionEntry.timestamp;
		sessionEntry.timestamp = ft_time (); 
		return _sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
	} 
	return PR_FALSE; 
} 

PRUintn MemMapSessionManager::getSessionTimeOut (const char* id) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		_sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
		return sessionEntry.inactivitytime; 
	} 
	return 0; 
} 

PRBool MemMapSessionManager::invalidate (const char* id)
{
    SemLock semLock (_lockMgr, _lockId);
    _sessionblock sessionEntry;
    PRUintn sessionlocation = _getSessionBlock (id, sessionEntry);
    if (sessionlocation < _sessionFile->getMaxBlocks ())
    {
        sessionEntry.invalidated = PR_TRUE;

        return _sessionFile->setEntry (sessionlocation, &sessionEntry,
                    sizeof (_sessionblock));
    }
    return PR_FALSE;
}

PRUintn MemMapSessionManager::setSessionTimeOut (const char* id, PRUintn timeout) 
{ 
	SemLock semLock (_lockMgr, _lockId); 
	_sessionblock sessionEntry; 
	PRUintn oldTimeOut = 0;
	PRUintn sessionlocation = _getSessionBlock (id, sessionEntry); 
	if (sessionlocation < _sessionFile->getMaxBlocks ()) 
	{ 
		oldTimeOut = sessionEntry.inactivitytime;
		sessionEntry.inactivitytime = timeout; 
		
		_sessionFile->setEntry (sessionlocation, &sessionEntry,  
			sizeof (_sessionblock)); 
	} 
	return oldTimeOut; 
}

PRUintn
MemMapSessionManager::getBlockSize(void)
{
    return _blocksize;
}

SemLock::SemLock (LockManager *lockMgr, PRUint32 lockId) : _lockMgr(lockMgr), _lockId (lockId) 
{ 
	PRStatus rval = _lockMgr -> lock(_lockId);

	if (rval != PR_SUCCESS) { 
		char* logMsg =  get_message(j2eeResourceBundle,
                                            "j2ee.MemMapSessionManager.ERR_SEM_LOCK_FAILED");
		NSJavaUtil::log(LOG_FAILURE, logMsg, rval, errno);
		FREE(logMsg);
		NS_JAVA_ASSERT (rval != SEM_ERROR); 
	} 
} 

SemLock::~SemLock () 
{ 
	PRStatus rval = _lockMgr -> unlock(_lockId);

	if (rval != PR_SUCCESS) {
		char* logMsg =  get_message(j2eeResourceBundle,
                                            "j2ee.MemMapSessionManager.ERR_SEM_UNLOCK_FAILED"); 
		NSJavaUtil::log(LOG_FAILURE, logMsg, rval, errno);
		FREE(logMsg);
	}
} 
