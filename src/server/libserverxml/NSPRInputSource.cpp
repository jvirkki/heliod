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

#include <string.h>
#include "xercesc/util/BinInputStream.hpp"
#include "xercesc/util/XMLString.hpp"
#include "base/ereport.h"
#include "i18n.h"
#include "libserverxml/dbtlibserverxml.h"
#include "NSPRInputSource.h"

using namespace XERCES_CPP_NAMESPACE;

//-----------------------------------------------------------------------------
// NSPRBinInputStream
//-----------------------------------------------------------------------------

class NSPRBinInputStream : public BinInputStream {
public:
    NSPRBinInputStream(PRFileDesc *fd)
    : fd(fd)
    { }

    ~NSPRBinInputStream()
    {
        PR_Close(fd);
    }

    unsigned int curPos() const
    {
        PRInt32 rv = PR_Seek(fd, 0, PR_SEEK_CUR);
        if (rv == -1)
            rv = 0;
        return rv;
    }

    unsigned int readBytes(XMLByte *const toFill, const unsigned int maxToRead)
    {
        PRInt32 rv = PR_Read(fd, toFill, maxToRead);
        if (rv == -1)
            rv = 0;
        return rv;
    }

private:
    PRFileDesc *fd;
};

//-----------------------------------------------------------------------------
// NSPRFileInputSource::NSPRFileInputSource
//-----------------------------------------------------------------------------

NSPRFileInputSource::NSPRFileInputSource(const char *path)
: path(path)
{
    systemId = XMLString::transcode(path);
}

//-----------------------------------------------------------------------------
// NSPRFileInputSource::~NSPRFileInputSource
//-----------------------------------------------------------------------------

NSPRFileInputSource::~NSPRFileInputSource()
{
    XMLString::release(&systemId);
}

//-----------------------------------------------------------------------------
// NSPRFileInputSource::getSystemId
//-----------------------------------------------------------------------------

const XMLCh *NSPRFileInputSource::getSystemId() const
{
    return systemId;
}

//-----------------------------------------------------------------------------
// NSPRFileInputSource::makeStream
//-----------------------------------------------------------------------------

BinInputStream *NSPRFileInputSource::makeStream() const
{
    PRFileDesc *fd = PR_Open(path, PR_RDONLY, 0);
    if (!fd) {
        NSString error;
        error.printf(XP_GetAdminStr(DBT_CONF1115_error_opening_X_error_Y), path.data(), system_errmsg());
        throw EreportableException(LOG_FAILURE, error);
    }

    return new NSPRBinInputStream(fd);
}
