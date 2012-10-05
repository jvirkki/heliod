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

#include "base/buffer.h"
#include "base/file.h"
#include "base/util.h"
#include "base/pool.h"
#include "base/ereport.h"
#include "httpdaemon/mime.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/configuration.h"

//-----------------------------------------------------------------------------
// MimeFile::MimeFile
//-----------------------------------------------------------------------------

MimeFile::MimeFile(ServerXMLSchema::String& mimeFile, ConfigurationObject* parent)
: ConfigurationObject(parent), 
  extHashExact(3), 
  extHashMixed(3)
{
    extHashMixed.setMixCase();

    try {
        parseFile(mimeFile);
    }
    catch (const EreportableException& e) {
        throw ConfigurationServerXMLException(mimeFile, e);
    }
}

//-----------------------------------------------------------------------------
// MimeFile::parseFile
//-----------------------------------------------------------------------------

void MimeFile::parseFile(const char *filename)
{
    char line[CINFO_MAX_LEN];

    // Open the file
    SYS_FILE fd;
    fd = system_fopenRO((char *)filename);
    if (fd == SYS_ERROR_FD) {
        NSString error;
        error.setGrowthSize(NSString::MEDIUM_STRING);
        error.printf(XP_GetAdminStr(DBT_Configuration_ErrorOpeningFile), filename, system_errmsg());
        throw EreportableException(LOG_FAILURE, error);
    }

    // Create a file_buf for the file
    filebuf_t* buf;
    buf = filebuf_open(fd, FILE_BUFFERSIZE);
    if (!buf) {
        NSString error;
        error.setGrowthSize(NSString::MEDIUM_STRING);
        error.printf(XP_GetAdminStr(DBT_Configuration_ErrorOpeningFile), filename, system_errmsg());
        system_fclose(fd);
        throw EreportableException(LOG_FAILURE, error);
    }

    try {
        // Get a single line from the file
        switch (util_getline(buf, 1, CINFO_MAX_LEN, line)) {
        case 0: // Valid line
        case 1: // EOF
            // Make sure this is an MCC/NES/iWS MIME types file
            if (line[0] != '#' || !strstr(line, "MIME")) {
                throw ConfigurationFileFormatException(filename, 1, 1, XP_GetAdminStr(DBT_Configuration_InvalidFileFormat));
            }
            break;

        case -1: // Error
        default:
            throw EreportableException(LOG_FAILURE, XP_GetAdminStr(DBT_Configuration_ErrorReadingFile));
        }

        // Parse the file
        parseFile(filename, buf, line);
    }
    catch (const EreportableException& e) {
        // Error
        filebuf_close(buf);
        throw;
    }

    filebuf_close(buf);
}

void MimeFile::parseFile(const char* filename, filebuf_t* buf, char* line)
{
    int eof = 0;
    int ln = 1;

    // Parse the file
    while (!eof)  {
        // Get another line
        eof = util_getline(buf, ln, CINFO_MAX_LEN, line);
        ln++;

        // Skip this line if it's empty or a comment
        char* pos = line;
        if (!*pos || (*pos == '#')) continue;

        // Parse this line
        char* type = 0;
        char* encoding = 0;
        char* language = 0;
        char* exts = 0;
        for (;;) {
            // Find the beginning of the name
            while (*pos && isspace(*pos)) ++pos;
            if (!*pos) break;
            char* name = pos;

            // Find the end of the name
            while (*pos && !isspace(*pos) && (*pos != '=')) ++pos;

            // Find the '=' preceding the value
            char* value = pos;
            while (*value && isspace(*value)) ++value;
            if (*value != '=') {
                throw ConfigurationFileFormatException(filename,
                                                       ln, 
                                                       (int)(value - line) + 1,
                                                       XP_GetAdminStr(DBT_Configuration_ExpectedEquals));
            }
            value++;

            // Null terminate the name
            *pos = '\0';

            // Find the beginning of the value
            while (*value && isspace(*value)) ++value;
            if (!*value) {
                throw ConfigurationFileFormatException(filename,
                                                       ln,
                                                       (int)(value - line) + 1,
                                                       XP_GetAdminStr(DBT_Configuration_ExpectedString));
            }

            // Null terminate the value
            if (*value == '"') {
                value++;
                pos = value;
                while (*pos && (*pos != '"')) ++pos;
                if (!*pos) {
                    throw ConfigurationFileFormatException(filename,
                                                           ln,
                                                           (int)(pos - line) + 1,
                                                           XP_GetAdminStr(DBT_Configuration_MissingClosingQuote));
                }
            } else {
                pos = value;
                while (*pos && !isspace(*pos)) ++pos;
            }
            if (*pos) {
                *pos = '\0';
                pos++;
            }

            // Decide where to stick the value
            char** dest = 0;
            if (!strcasecmp(name, "type")) dest = &type;
            else if (!strcasecmp(name, "enc")) dest = &encoding;
            else if (!strcasecmp(name, "lang")) dest = &language;
            else if (!strcasecmp(name, "exts")) dest = &exts;
            else {
                throw ConfigurationFileFormatException(filename,
                                                       ln,
                                                       (int)(name - line) + 1,
                                                       name,
                                                       XP_GetAdminStr(DBT_Configuration_ExpectedTypeEncLangExts));
            }
            if (*dest) {
                throw ConfigurationFileFormatException(filename, ln,
                                                       (int)(name - line) + 1,
                                                       name,
                                                       XP_GetAdminStr(DBT_Configuration_AttrMultiplyDefined));
            }
            *dest = value;
        }

        // If there were extensions given...
        if (exts) {
            // Make sure there was a type, encoding, and/or language
            if (!type && !encoding && !language) {
                throw ConfigurationFileFormatException(filename,
                                                       ln,
                                                       XP_GetAdminStr(DBT_Configuration_ExpectedTypeEncLang));
            }

            // Add this entry
            addType(new MimeType(type, encoding, language, exts, this),
                    filename, ln, (int)(exts - line) + 1);

        } else if (type || encoding || language) {
            // Gave a type, encoding, and/or language without any extensions
            throw ConfigurationFileFormatException(filename,
                                                   ln,
                                                   XP_GetAdminStr(DBT_Configuration_ExpectedExts));
        }
    }
}

//-----------------------------------------------------------------------------
// MimeFile::addType
//-----------------------------------------------------------------------------

MimeType* MimeFile::addType(MimeType* type)
{
    return addType(type, 0, 0, 0);
}

MimeType* MimeFile::addType(MimeType* type, const char* filename, int ln, int cn)
{
    // For every extension...
    CListIterator<char> iterator(&type->extList);
    const char* ext;
    while (ext = (++iterator)) {
        // If the extension has been added already (with the same case)...
        if (extHashExact.lookup((void*)ext)) {
            // Duplicate extension.  This is a configuration error, but we
            // allow it to ease migration from 4.x servers.
            ereport(LOG_MISCONFIG,
                    XP_GetAdminStr(DBT_Configuration_ErrorParsingFileLineColTokenDesc),
                    filename,
                    ln,
                    cn,
                    ext,
                    XP_GetAdminStr(DBT_Configuration_ExtensionMultiplyDefined));
        }

        // If this is the first occurrence of this extension in any case, we
        // will use type for case insensitive matches
        if (!extHashMixed.lookup((void*)ext)) {
            extHashMixed.insert((void*)ext, (void*)type);
        }

        // Use type for case sensitive matches
        extHashExact.insert((void*)ext, (void*)type);
    }

    // Caller is responsible for delete'ing type when we're destroyed.  This is
    // achieved by returning type from newObject() or calling addObject().
    return type;
}

//-----------------------------------------------------------------------------
// MimeFile::findExtCaseSensitive
//-----------------------------------------------------------------------------

const cinfo* MimeFile::findExtCaseSensitive(const char* ext) const
{
    // XXX SimpleHash has its constness wrong
    SimplePtrStringHash* hash = (SimplePtrStringHash*)&extHashExact;
    const MimeType* mimeType = (const MimeType*)hash->lookup((void*)ext);
    if (mimeType)
        return &mimeType->ci;
    return NULL;
}

//-----------------------------------------------------------------------------
// MimeFile::findExtCaseInsensitive
//-----------------------------------------------------------------------------

const cinfo* MimeFile::findExtCaseInsensitive(const char* ext) const
{
    // XXX SimpleHash has its constness wrong
    SimplePtrStringHash* hash = (SimplePtrStringHash*)&extHashMixed;
    const MimeType* mimeType = (const MimeType*)hash->lookup((void*)ext);
    if (mimeType)
        return &mimeType->ci;
    return NULL;
}

//-----------------------------------------------------------------------------
// Mime::Mime
//-----------------------------------------------------------------------------

Mime::Mime(ConfigurationObject* parent)
: ConfigurationObject(parent)
{ }

//-----------------------------------------------------------------------------
// Mime::addMimeFile
//-----------------------------------------------------------------------------

void Mime::addMimeFile(MimeFile *mimeFile)
{
    int i;

    for (i = 0; i < mimeFilesVector.length(); i++) 
        if (mimeFilesVector[i] == (void*)mimeFile) 
            return;

    // N.B. we don't assume ownership of the passed MimeFile *

    mimeFilesVector.append(mimeFile);
}

//-----------------------------------------------------------------------------
// Mime::findExt
//-----------------------------------------------------------------------------

const cinfo* Mime::findExt(const char* ext) const
{
    int i;

    // First try a case sensitive lookup across all MIME files
    for (i = 0; i < mimeFilesVector.length(); i++) {
        const cinfo* cinfo = ((MimeFile*)mimeFilesVector[i])->findExtCaseSensitive(ext);
        if (cinfo)
            return cinfo;
    }

    // The case sensitive lookup failed, try case insensitive
    for (i = 0; i < mimeFilesVector.length(); i++) {
        const cinfo* cinfo = ((MimeFile*)mimeFilesVector[i])->findExtCaseInsensitive(ext);
        if (cinfo)
            return cinfo;
    }

    return NULL;
}

//-----------------------------------------------------------------------------
// Mime::getContentInfo
//-----------------------------------------------------------------------------

cinfo* Mime::getContentInfo(pool_handle_t* pool, const char* uri) const
{
    char* temp = pool_strdup(pool, uri);
    cinfo* ci = getContentInfo(pool, temp);
    pool_free(pool, temp);
    return ci;
}

cinfo* Mime::getContentInfo(pool_handle_t* pool, char* uri) const
{
    const char* type = 0;
    const char* encoding = 0;
    const char* language = 0;
    char* exts;

    // Find the last path component of uri
    exts = strchr(uri, FILE_PATHSEP);
    if (!exts)
        exts = uri;
    else
        ++exts;

    // Find the first extension
    exts = strchr(exts, CINFO_SEPARATOR);
    if (!exts) return 0;
    ++exts;

    // For every extension, starting with the first...
    while (*exts) {
        // Find the end of this extension
        char* t = exts;
        while (*t && (*t != CINFO_SEPARATOR)) ++t;
        if (t == exts) {
            exts++;
            continue;
        }

        // Null terminate this extension
        char c = *t;
        *t = '\0';

        // exts is now the beginning of an extension.  See if there's a cinfo
        // associated with this extension.
        const cinfo* ci = findExt(exts);
        if (ci) {
            if (ci->type) type = ci->type;
            if (ci->encoding) encoding = ci->encoding;
            if (ci->language) language = ci->language;
        }

        // Restore the separator at the end of this extension
        if (c) {
            *t = c;
            ++t;
        }

        // Next extension...
        exts = t;
    }

    // Create a new cinfo, the caller is responsible for FREE()ing it
    cinfo* ci = 0;
    if (type || encoding || language) {
        ci = (cinfo*)pool_malloc(pool, sizeof(*ci));
        ci->type = type ? pool_strdup(pool, type) : 0;
        ci->encoding = encoding ? pool_strdup(pool, encoding) : 0;
        ci->language = language ? pool_strdup(pool, language) : 0;
    }

    return ci;
}

//-----------------------------------------------------------------------------
// MimeType::MimeType
//-----------------------------------------------------------------------------

MimeType::MimeType(const char* type, const char* encoding, const char* language, char* exts, ConfigurationObject* parent)
: ConfigurationObject(parent)
{
    init(type, encoding, language, exts);
}

//-----------------------------------------------------------------------------
// MimeType::~MimeType
//-----------------------------------------------------------------------------

MimeType::~MimeType()
{
    if (ci.type) free(ci.type);
    if (ci.encoding) free(ci.encoding);
    if (ci.language) free(ci.language);

    // Empty extList, freeing the strdup()'d strings
    char* string;
    while (string = extList.First()) {
        free(string);
        extList.Delete(string);
    }
}

//-----------------------------------------------------------------------------
// MimeType::init
//-----------------------------------------------------------------------------

void MimeType::init(const char* type, const char* encoding, const char* language, char* exts)
{
    ci.type = type ? strdup(type) : 0;
    ci.encoding = encoding ? strdup(encoding) : 0;
    ci.language = language ? strdup(language) : 0;

    // For every extension in the list...
    while (*exts) {
        // We can be separated by " ", ",", or ", "
        while (*exts && isspace(*exts)) ++exts;
        char* t = exts;
        while (*t && (*t != ',') && !isspace(*t)) ++t;
        if (*t) {
            *t = '\0';
            t++;
        }
        extList.Append(strdup(exts));
        exts = t;
    }
}
