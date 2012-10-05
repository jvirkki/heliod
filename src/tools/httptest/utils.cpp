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


#include "utils.h"
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <support/NSString.h>
#include <prerror.h>
#include "log.h"
#include "engine.h"
#include "utils.h"
#include <ctype.h>

PRTime origin = PR_Now(); // program start time

PRFd :: PRFd(const PRFileDesc* desc)
{
    fd = (PRFileDesc*) desc;
};

PRFd :: PRFd()
{
    fd = NULL;
};

PRFile :: PRFile(const char *name, PRIntn flags, PRIntn mode)
{
    fname.append(name);
    fd = PR_Open(name, flags, mode);
    if (!fd && name && (flags & PR_CREATE_FILE))
    {
        NSString newname;
        newname.append(name);
        int i=0;
        // replace backslashes with forward slashes
        newname.replace('\\', '/');

        char* dirname = strdup(newname.data());
        char* slash = strchr(dirname, '/');

        while (slash)
        {
            char* nextslash = NULL;
            
            if (slash)
            {
                *slash='\0'; // terminate string
                nextslash = strchr(slash+1, '/');
            };

            PRDir* dir = PR_OpenDir(dirname);
            if (dir)
            {
                // the directory already existed, skip to next level
                PR_CloseDir(dir);
                *slash='/';
                slash = nextslash;
                continue;
            };

            PRStatus status = PR_MkDir(dirname, mode);
            
            if (PR_SUCCESS == status && !nextslash)
                fd = PR_Open(name, flags, mode);
        };
        if (dirname)
            free(dirname);
    };
};

PRFile :: ~PRFile()
{
    if (fd)
        PR_Close(fd);
};

PRFileInfo PRFile :: GetFileInfo() const
{
    PRFileInfo info;
    memset(&info, 0, sizeof(info));
    PRStatus status = PR_GetFileInfo(fname.data(), &info);
    return info;
};

#define READBUF 1024

PRBool PRFd :: read(void*& buf, PRInt32& len)
{
    PRErrorCode code = 0;

    buf = NULL;
    len = 0;

    if (!fd)
        return PR_FALSE;

    len=0;
    buf = malloc(1);

    if (!buf)
        return PR_FALSE;

    PRInt32 bytes_read = 0;
    do
    {
        len+=READBUF;
        buf=realloc(buf, len +1 ); // hack : always allocate one extra byte for NULL termination ...
        if (!buf)
        {
            len = 0;
            return PR_FALSE;
        }
        else
        {
            * ((char*) buf + len-READBUF) = '\0';
            * ((char*) buf + len) = '\0';
        };

        bytes_read = PR_Read(fd, (void*) ((char*)buf+ len-READBUF), READBUF);
        if (bytes_read>=0)
        {
            *((char*)buf+len-READBUF+bytes_read) = '\0'; // hack : NULL termination
            len -= (READBUF-bytes_read);
        }
        else
        {
            code = PR_GetError();
            len -= READBUF;
        };
    } while (bytes_read>0);

    return PR_TRUE;
};

PRBool PRFd :: write(const void* buf, PRInt32 len)
{
    if ( (!fd) || (!buf) || (!len))
        return PR_FALSE;

    PRInt32 bytes_written = 0;
    do
    {
        PRInt32 bwritten = PR_Write(fd, (void*) ((char*)buf+ bytes_written), len-bytes_written);
        if (bwritten>=0)
           bytes_written += bwritten;
        else
          return PR_FALSE;
    } while (bytes_written<len);

    return PR_TRUE;
};

PRSocket :: PRSocket(const PRFileDesc* desc, PRIntervalTime tm)
{
    fd = (PRFileDesc*) desc;
    timeout = tm;
};

PRBool PRSocket :: read(void*& buf, PRInt32& len)
{
    PRErrorCode code = 0;

    buf = NULL;
    len = 0;

    if (!fd)
        return PR_FALSE;

    len=0;
    buf = malloc(1);

    if (!buf)
        return PR_FALSE;

    PRInt32 bytes_read = 0;
    do
    {
        len+=READBUF;
        buf=realloc(buf, len +1 ); // hack : always allocate one extra byte for NULL termination ...
        if (!buf)
        {
            len = 0;
            return PR_FALSE;
        }
        else
        {
            * ((char*) buf + len-READBUF) = '\0';
            * ((char*) buf + len) = '\0';
        };

        bytes_read = PR_Recv(fd, (void*) ((char*)buf+ len-READBUF), READBUF, 0, timeout);
        if (bytes_read>=0)
        {
            *((char*)buf+len-READBUF+bytes_read) = '\0'; // hack : NULL termination
            len -= (READBUF-bytes_read);
        }
        else
        {
#ifdef DEBUG
            printerror("PR_Recv");
#endif
            code = PR_GetError();
            len -= READBUF;
            Logger::logError(LOGTRACE, "Error while receiving response : %s.\n", nscperror_lookup(code));
        };
    } while (bytes_read>0);

    return PR_TRUE;
};

#define BUFSIZE 4096

PRBool PRSocket :: empty(PRInt32& len)
{
    PRErrorCode code = 0;

    void * buf = NULL;
    len = 0;

    if (!fd)
        return PR_FALSE;

    buf = malloc(BUFSIZE);

    if (!buf)
        return PR_FALSE;

    PRInt32 bytes_read;

    do
    {
        bytes_read = PR_Recv(fd, (void*) ((char*)buf), BUFSIZE, 0, timeout);
        if (bytes_read>=0)
        {            
            len += bytes_read;
        }
        else
        {
#ifdef DEBUG
            printerror("PR_Recv");
#endif
            code = PR_GetError();
            Logger::logError(LOGTRACE, "Error while receiving response : %s.\n", nscperror_lookup(code));
        };
    } while (bytes_read>0);

    free(buf);

    return PR_TRUE;
};

void printerror(char* funcname)
{
        PRInt32 ecode = PR_GetError();
        PRInt32 oscode = PR_GetOSError();

        char *buffer = NULL;
        PRInt32 len = PR_GetErrorTextLength();
        if (len)
            buffer = (char *) malloc(len+1);

        PRInt32 status = PR_FAILURE;
        if (buffer)
        {
            status = PR_GetErrorText(buffer);
            *(buffer+len) = '\0';
        };

        if (PR_SUCCESS != status)
        {
            if (buffer)
                free(buffer);
            buffer = (char*) nscperror_lookup(ecode);
        };

        Logger::logError(LOGERROR, "%s failure - NSPR rc = %d , OS rc = %d\n       %s", 
            funcname?funcname:"", ecode, oscode, buffer?buffer:"");

        if (buffer && PR_SUCCESS == status)
            free(buffer);
};

char* uppercase(char* string)
{
    size_t len = strlen(string);
    size_t index;
    for (index=0;index<len;index++)
        string[index] = toupper(string[index]);
    return string;
};
