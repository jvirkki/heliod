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

#ifndef HTTPDAEMON_MIME_H
#define HTTPDAEMON_MIME_H

#include "base/cinfo.h"
#include "httpdaemon/configuration.h"
#include "generated/ServerXMLSchema/Server.h"
#include "support/GenericVector.h"
#include "support/LinkedList.hh"
#include "support/SimpleHash.h"

//-----------------------------------------------------------------------------
// MimeType
//-----------------------------------------------------------------------------

class MimeType : public ConfigurationObject {
public:
    MimeType(const char* type, const char* enc, const char* lang, char* exts, ConfigurationObject* parent);
    ~MimeType();

private:
    void init(const char* type, const char* enc, const char* lang, char* exts);

    cinfo ci;
    CList<char> extList;

friend class MimeFile;
};

//-----------------------------------------------------------------------------
// MimeFile
//-----------------------------------------------------------------------------

class MimeFile : public ConfigurationObject {
public:
    MimeFile(ServerXMLSchema::String& mimeFile, ConfigurationObject* parent);

    const cinfo* findExtCaseSensitive(const char* ext) const;
    const cinfo* findExtCaseInsensitive(const char* ext) const;

private:
    void parseFile(const char* filename);
    void parseFile(const char* filename, filebuf_t* buf, char* line);
    MimeType* addType(MimeType* type);
    MimeType* addType(MimeType* type, const char* filename, int line, int column);

    SimplePtrStringHash extHashExact;
    SimplePtrStringHash extHashMixed;

    static const int sizeExtHash;
};

//-----------------------------------------------------------------------------
// Mime
//-----------------------------------------------------------------------------

class Mime : public ConfigurationObject {
public:
    Mime(ConfigurationObject* parent);

    void addMimeFile(MimeFile *file);
    const cinfo* findExt(const char* ext) const;
    cinfo* getContentInfo(pool_handle_t* pool, char* uri) const;
    cinfo* getContentInfo(pool_handle_t* pool, const char* uri) const;

private:
    GenericVector mimeFilesVector;
};

#endif // HTTPDAEMON_MIME_H
