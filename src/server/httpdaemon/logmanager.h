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

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include "generated/ServerXMLSchema/AccessLogBuffer.h"

struct LogFile;
struct LogBuffer;

class LogManager {
public:
    static PRBool isParam(const char* name);
    static void setParams(const ServerXMLSchema::AccessLogBuffer& config);
    static void setParams(pblock* pb);
    static PRStatus initEarly();
    static PRStatus initLate();
    static char* getAbsoluteFilename(const char* filename);
    static LogFile* getFile(const char* filename);
    static void setHeader(LogFile* file, const char* header);
    static PRStatus openFile(LogFile* file);
    static LogFile* ref(LogFile* file);
    static LogFile* unref(LogFile* file);
    static char* lockBuffer(LogFile* file, LogBuffer*& handle, PRUint32 size);
    static void unlockBuffer(LogBuffer* handle, PRUint32 size);
    static PRInt32 logf(LogFile* file, PRUint32 size, const char* fmt, ...);
    static PRInt32 log(LogFile* file, const char* string);
    static PRInt32 log(LogFile* file, const char* string, PRUint32 size);
    static void reopen();
    static void rotate(const char* ext);
    static void setRotateCallback(void (*fn)(const char* filenameNew, const char* filenameOld));
    static void terminate();
    static int getLogCount();
    static char* getBuffer(LogBuffer *buffer);
    static PRBool isSpaceAvailable(LogBuffer *buffer, PRUint32 size);
    static void updatePosition(LogBuffer *buffer, PRUint32 size);

    static const PRUint32 sizeMaxLogLine;
};

#endif // LOGMANAGER_H
