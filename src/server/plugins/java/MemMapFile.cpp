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

// 
//	Header Bit Map of data records in use 
//		header[ 
// 
//	Record Structure 
//		 
//		data[CONFIGURABLE]  
 
#include "MemMapFile.h"
#include "NSJavaUtil.h"
#include "prlog.h"
#include "frame/log.h"

MemMapFile::MemMapFile(const char *name, PRUintn blocksize, PRUintn maxsize, PRBool &done) : 
	_BlockSize(blocksize + sizeof(PRUintn)), 
	_MaxBlocks(maxsize), 
	_headersizeasinteger(0), 
	_filedesc(NULL), 
	_mapfile(NULL) 
{ 
	done = PR_FALSE;
	
    _filename.setGrowthSize(NSString::SMALL_STRING);
	_filename.append(name); 
		 
	//calculate out the size as integer 32 
	_headersizeasinteger = (maxsize + (32 - 1)) / 32; 
 
	_filedesc = PR_Open (name, PR_CREATE_FILE | PR_RDWR | PR_SYNC, 0xffff); 
	NS_JAVA_ASSERT (_filedesc != NULL); 

	_filesize = (_headersizeasinteger * 4) +
			((_BlockSize + sizeof(PRUintn)) * _MaxBlocks);

	PRFileInfo pr_info;
	if (PR_GetOpenFileInfo(_filedesc, &pr_info) == -1) {
		ereport(LOG_FAILURE, "MMapSessionManager (native): cannot create memoary-mapped file because server is unable to access file %s in SessionData directory; error code = %d", name, PR_GetError());
		return;
	} else if ((pr_info.size > 0) && (pr_info.size != _filesize)) {
		ereport(LOG_FAILURE, "MMapSessionManager (native): cannot create memory-mapped file because of configuration changes; please delete the files under SessionData directory and start the server.");
		return;
	}		
	
	_mapfile = PR_CreateFileMap(
		_filedesc,
		(_headersizeasinteger * 4) + ((_BlockSize + sizeof(PRUintn)) * _MaxBlocks),
		PR_PROT_READWRITE);

	if (_mapfile == NULL) {
		ereport(LOG_FAILURE, "MMapSessionManager (native): cannot create memory-mapped file map for the file %s of size %d bytes; error code = %d", name, _filesize, PR_GetError());
		return;
	}		
 
	_memory = PR_MemMap(
		_mapfile,
		0,
		(_headersizeasinteger * 4) + ((_BlockSize + sizeof(PRUintn)) * _MaxBlocks));
	if (_memory == NULL) {
		ereport(LOG_FAILURE, "MMapSessionManager (native): cannot create memory-mapped file map; failed to memory map the file %s of size %d bytes; error code = %d", name, _filesize, PR_GetError());
		return;
	}		

	done = PR_TRUE;

	//format memory 
	//memset(_memory,0,(_headersizeasinteger * 4) + ((_BlockSize + sizeof(PRUintn)) * _MaxBlocks)); 
} 
 
MemMapFile::~MemMapFile() 
{ 
	PRStatus status; 
	 
	status = PR_MemUnmap(_memory,(_headersizeasinteger * 4) + ((_BlockSize + sizeof(PRUintn)) * _MaxBlocks)); 
	NS_JAVA_ASSERT (status == PR_SUCCESS); 
 
	status = PR_CloseFileMap(_mapfile); 
	NS_JAVA_ASSERT (status == PR_SUCCESS); 
 
	status = PR_Close(_filedesc); 
	NS_JAVA_ASSERT (status == PR_SUCCESS); 
}
 
PRUintn MemMapFile::setEntry(const void *data, PRUintn size) 
{ 
	NS_JAVA_ASSERT (size <= _BlockSize); 
 
	//find a place in the header 
	PRUintn location = _findunusedblock(); 
 
	if (location < _MaxBlocks) { 
		//write data 
		_setblock(location,data,size); 
	} 
 
	//return address 
	return location; 
} 
 
PRBool MemMapFile::setEntry(PRUintn location, const void *data, PRUintn size) 
{ 
	//check args 
	NS_JAVA_ASSERT (location < _MaxBlocks); 
	NS_JAVA_ASSERT (size <= _BlockSize); 
 
	//check the place in the header 
	if (_checkblock(location) != PR_TRUE) { 
		return PR_FALSE; 
	} 
 
	_setblock(location,data,size); 
 
	return PR_TRUE; 
}
 
PRUintn MemMapFile::reserveEntry(void *&data, PRUintn &size) 
{ 
	NS_JAVA_ASSERT (size <= _BlockSize); 
 
	//find a place in the header 
	PRUintn location = _findunusedblock(); 

	if (location < _MaxBlocks) { 
        // 'reserve' the block for write later.
	    _reserveblock(location, data, size);
    }

	//return address 
	return location; 
}
 
PRUintn MemMapFile::reserveEntry(PRUintn location, void *&data, PRUintn &size) 
{ 
	NS_JAVA_ASSERT (location < _MaxBlocks); 
	NS_JAVA_ASSERT (size <= _BlockSize); 

	// 'reserve' the block for write later.
	_reserveblock(location, data, size);
            
	return PR_TRUE; 
}

PRBool MemMapFile::locateEntry(PRUintn location, void *&data, PRUintn &size) 
{ 
	//check args 
	NS_JAVA_ASSERT (location < _MaxBlocks); 
	NS_JAVA_ASSERT (size <= _BlockSize); 
 
	//check the place in the header 
	if (_checkblock(location) != PR_TRUE) { 
		return PR_FALSE; 
	} 
 
	_locateblock(location,data,size); 
 
	return PR_TRUE; 
}
 
PRBool MemMapFile::getEntry(PRUintn location, void *&data, PRUintn &size)
{
	NS_JAVA_ASSERT (location < _MaxBlocks); 
    NS_JAVA_ASSERT (data != NULL);

	//verify block it still not used 
	if (_checkblock(location) != PR_TRUE) { 
		//	data = NULL; 
		size = 0; 
		return PR_FALSE; 
	} 
 
	//size of data 
	memcpy(&size, ((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), sizeof(PRUintn)); 
	 
	//copy data 
	memcpy(data, ((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))) + sizeof(PRUintn), size); 

	return PR_TRUE;	 
} 
 
void MemMapFile::clearEntry(PRUintn location)
{
	NS_JAVA_ASSERT (location < _MaxBlocks); 
 
	_setunusedblock(location); 
 
	//clear data 
	//memset(((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), 0, (_BlockSize + sizeof(PRUintn))); 
} 
 
	 
PRBool MemMapFile::_setblock(PRUintn location, const void *data, PRUintn size)  
{
	NS_JAVA_ASSERT (location < _MaxBlocks);
	NS_JAVA_ASSERT (size <= _BlockSize);
	 
	_setusedblock(location); 
 
	//size of data 
	memcpy(((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), &size, sizeof(PRUintn)); 
	 
	//copy data 
	memcpy(((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))) + sizeof(PRUintn), data, size); 
 
	return PR_TRUE; 
} 
 
PRBool MemMapFile::_reserveblock(PRUintn location, void *&data, PRUintn &size)
{
	NS_JAVA_ASSERT (location < _MaxBlocks);
	NS_JAVA_ASSERT (size <= _BlockSize);
	 
	_setusedblock(location); 
 
	//size of data 
	memcpy(((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), &size, sizeof(PRUintn)); 
	 
	//point to data
	data = ((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))) + sizeof(PRUintn);
 
	return PR_TRUE; 
} 

PRBool MemMapFile::_locateblock(PRUintn location, void *&data, PRUintn &size)  
{
	NS_JAVA_ASSERT (location < _MaxBlocks);
	NS_JAVA_ASSERT (size <= _BlockSize);
	 
	_setusedblock(location); 
 
	//size of data 
	memcpy(&size, ((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), sizeof(PRUintn)); 
	 
	//point to the location of the block where data for the entry actually resides.
	data = ((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))) + sizeof(PRUintn);
 
	return PR_TRUE; 
} 
 
void MemMapFile::_clearblock(PRUintn location) 
{ 
	NS_JAVA_ASSERT (location < _MaxBlocks);
 
	_setunusedblock(location); 
 
	//copy data 
	//memset(((unsigned char *)_memory) + (_headersizeasinteger * 4) + (location * (_BlockSize + sizeof(PRUintn))), 0, (_BlockSize + sizeof(PRUintn))); 
} 
 
PRUintn MemMapFile::_findunusedblock()  
{ 
	PRUintn location = _MaxBlocks; 
	 
	for(PRUintn i = 0; i <  _headersizeasinteger; i++) { 
		if (((PRUint32)(*(((PRUintn *)_memory) + i))) != 0xFFFFFFFF) {			 
			PRUint32 temploc = 1; 
			PRUint32 tempmask = ~ ((PRUint32)(*(((PRUintn *)_memory) + i))); 
			for(PRUintn j = 0; j <  32; j++) { 
				if (tempmask & temploc) { 
					location = i * 32 + j;		 
					return location; 
				} 
				temploc <<= 1; 
			} 
		 
		} 
	} 
 
	return location; 
} 
 
void MemMapFile::_setusedblock(PRUintn location)  
{	 
	NS_JAVA_ASSERT (location < _MaxBlocks);
 
	PRUintn intlocation = location / 32; 
	PRUintn bitlocation = location % 32; 
	PRUint32 temp = 1 << bitlocation; 
 
	*(((PRUintn *)_memory) + intlocation) = *(((PRUintn *)_memory) + intlocation) | temp; 
} 
 
 
void MemMapFile::_setunusedblock(PRUintn location)  
{	 
	NS_JAVA_ASSERT (location < _MaxBlocks);
 
	PRUintn intlocation = location / 32; 
	PRUintn bitlocation = location % 32; 
	PRUint32 temp = ~ (1 << bitlocation); 
 
	*(((PRUintn *)_memory) + intlocation) = *(((PRUintn *)_memory) + intlocation) & temp; 
} 
 
PRBool MemMapFile::_checkblock(PRUintn location)  
{ 
	NS_JAVA_ASSERT (location < _MaxBlocks);
 
	PRUintn intlocation = location / 32; 
	PRUintn bitlocation = location % 32; 
	PRUint32 temp = 1 << bitlocation; 
 
	return *(((PRUintn *)_memory) + intlocation) & temp ? PR_TRUE : PR_FALSE; 
} 
