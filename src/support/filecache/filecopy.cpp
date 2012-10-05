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

/* 
 *           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
 *              NETSCAPE COMMUNICATIONS CORPORATION
 * Copyright © 1998, 1999 Netscape Communications Corporation.  All Rights
 * Reserved.  Use of this Source Code is subject to the terms of the
 * applicable license agreement from Netscape Communications Corporation.
 * The copyright notice(s) in this Source Code does not indicate actual or
 * intended publication of this Source Code.
 */

#include "nsfc_pvt.h"
#include <sys/stat.h>
#ifdef XP_WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#include <sys/types.h>
#endif

#define COPY_BUFFER_SIZE 32768

PR_IMPLEMENT(PRStatus)
NSFC_MakeDirectory(char *dirname, NSFCCache cip)
{
    NSFCFileInfo finfo;
    char *cp;
    PRStatus rv;

    /* Use the cache to remember which directories exist */
    rv = NSFC_GetFileInfo(dirname, &finfo, cip);
    if (rv == PR_SUCCESS) {

        /*
         * dirname exists - so succeed or fail depending on whether
         * it is a directory or not.
         */
        return (finfo.pr.type == PR_FILE_DIRECTORY) ? PR_SUCCESS : PR_FAILURE; 
    }

    /* Otherwise call NSFC_MakeDirectory() on the parent directory */
    cp = strrchr(dirname, '/');
    if (cp) {
        *cp = 0;
        rv = NSFC_MakeDirectory(dirname, cip);
        *cp = '/';
        if (rv == PR_FAILURE) {

            /* Failed to create the parent directory */
            return rv;
        }
    }

    /* Finally create the desired directory */
    rv = PR_MkDir(dirname, 0770);

    /* another thread may alreay created the directory if two
     * threads trying to MakeDirectory dirname at same time for
     * different files, e.g. dirname/file1, dirname/file2
     */
    if (rv == PR_FAILURE && PR_GetError() == PR_FILE_EXISTS_ERROR)
        return PR_SUCCESS;

    return rv;
}

PR_IMPLEMENT(PRStatus)
NSFC_MakeTempCopy(const char *infile, char *outfile, 
                  NSFCCache cip, PRInt64 *size)
{
    PRFileDesc *infd;
    PRFileDesc *outfd;
    char *cp;
    PRStatus rv = PR_FAILURE;

    infd = NSFC_PR_Open(infile, PR_RDONLY, 0);
    if (infd == NULL) {
        return rv;
    }

    /* Create the output directory if necessary */
    cp = strrchr(outfile, '/');
    if (cp) {

        *cp = 0;
        rv = NSFC_MakeDirectory(outfile, cip);
        *cp = '/';
        if (rv == PR_FAILURE) {
            PR_Close(infd);
            return rv;
        }
    }

    outfd = NSFC_PR_Open(outfile, PR_CREATE_FILE|PR_TRUNCATE|PR_WRONLY, 0660);
    if (outfd != NULL) {

        PRInt32 inlen = -1;
        PRInt32 outlen;
        PRInt64 written = 0;
        char *buffer = (char *)PR_Malloc(COPY_BUFFER_SIZE);

        if (buffer) {
            while (1) {
                inlen = PR_Read(infd, buffer, COPY_BUFFER_SIZE);
                if (inlen <= 0) {
                    break;
                }
                outlen = PR_Write(outfd, buffer, inlen);
                if (outlen != inlen) {
                    break;
                }
                written += outlen;
            }

            PR_Free(buffer);
        }

        PR_Close(outfd);

        if (inlen == 0) {
            rv = PR_SUCCESS;
            struct stat    tempStat;

            *size = written;

            /* VB: bug# 360312
             * Whenever we make a temporary copy of a file, we need to 
             * set the modification/access time of the copy to be that
             * of the original.
             * This is required to avoid the following scenario.
             * 1. Copy file1 preserving timestamp.
             * 2. Edit file1 and access it. This would cause the edited verion
             *    to be cached and a temp copy of the edited verion to be 
             *    created. 
             * 3. Now replace the edited file1 with the original copy preserving
             *    the timestamp. So file 1 has the timestamp at the beginning 
             *    of our test.
             * 4. We would never end up serving this file since the timestamp
             *    of the temp copy would be ahead of that of file1.
             * 5. With this fix, we aviod the case since we can now compare
             *    for inequality (LL_NE) rather than which is newer. (LL_CMP).
             * This is of relevance in raltion to the doc dir being backed up
             * and restored.
             */
            if (stat(infile, &tempStat) == 0) {
                struct utimbuf infileTime;
                infileTime.actime = tempStat.st_atime;
                infileTime.modtime = tempStat.st_mtime;
                if (utime(outfile, &infileTime) == -1) {
                    /* VB: This can happen if the file is owned by someone else
                     * This should not happen unless the server user was changed
                     * Should we be failing in this case ?
                     */
                    int whatDoIdonow = 1;
                }
            }
            else {
                // stat failed. This should happen if infile got deleted.
                // So what - we serve the temp file until the filecache
                // realizes to the contrary.
                int shouldNotMatter = 0;
            }
        } else {
            rv = PR_FAILURE;
            PR_Delete(outfile);
        }
    }

    PR_Close(infd);

    return rv;
}

static char * 
_make_tempname(NSFCCache cip, const char *fname,
               char *tempname, int tempnamelen) 
{
    const char *sp = cip->cfg.tempDir;
    int i;

    /* Copy tempDir to tempname, keeping track of length */
    for (i = 0; i < tempnamelen; ++i) {
        tempname[i] = sp[i];
        if (!tempname[i]) {
            break;
        }
    }

    if (i >= tempnamelen) {
        return NULL;
    }

    /* Append filename to tempname */
    sp = fname;

    /* Ensure exactly one '/' at the end */
    if ((i > 0) && (tempname[i-1] != '/') && (*sp != '/')) {
        tempname[i++] = '/';
    }

#ifdef XP_WIN32
    /* Convert drive: notation to drive/ */
    if (isalpha(sp[0]) && (sp[1] == ':')) {
        if ((tempnamelen - i) < 3) {
            return NULL;
        }

        tempname[i++] = *sp;
        sp += 2;
        if (*sp != '/') {
            tempname[i++] = '/';
        }
    }
#endif /* XP_WIN32 */

    for ( ; i < tempnamelen; ++i) {
        tempname[i] = *sp++;
        if (!tempname[i]) {
            break;
        }
    }

    if (i >= tempnamelen) {
        return NULL;
    }
    return tempname;
}


PR_IMPLEMENT(PRFileDesc *)
NSFC_OpenTempCopy(NSFCCache cip, NSFCEntryImpl *nep, 
                  NSFCStatusInfo *statusInfo)
{
    const char *fname;
    int releaseWrite = 0;
    PRStatus rv;
    PRFileDesc *fd;
    char tempname[1024];

    NSFCSTATUSINFO_INIT(statusInfo);

    /* copy files for non-cache transmit is not supported */
    if (nep == NULL) {
        PR_ASSERT(0);
        return NULL;
    }

    fname = nep->filename;

    if (!(nep->flags & NSFCENTRY_HASINFO)) {
        PR_ASSERT(0);
        return NULL;
    } 
    if (nep->flags & NSFCENTRY_ERRORINFO)
        return NULL;

    /* Don't repeatedly try to copy a too-large file to a too-small tempDir */
    if (nep->flags & NSFCENTRY_CREATETMPFAIL)
        return NULL;

    if (_make_tempname(cip, fname, tempname, sizeof(tempname)) == NULL) 
        return NULL;

    /* Has this file been copied to a temporary file yet? */
    if (!(nep->flags & NSFCENTRY_TMPFILE)) {
        PRFileInfo64 tfinfo;
        PRFileInfo64 *nepfinfo = NULL;
        PRBool needCopy = PR_TRUE;

        nepfinfo = &(nep->finfo.pr);

        if (NSFC_AcquireEntryWriteLock(cip, nep) != NSFC_OK)
            goto error;

        /* See if temp file created while we acquired the lock */
        if (nep->flags & NSFCENTRY_TMPFILE) {
            NSFC_ReleaseEntryWriteLock(cip, nep);
            goto do_open;
        }        

        releaseWrite = 1;

        /* See if the temporary file is present */
        if (NSFC_PR_GetFileInfo(tempname, &tfinfo) == PR_SUCCESS) {

            /* Is the temporary file up to date? */
            if ((tfinfo.size == nep->finfo.pr.size) &&
                (tfinfo.creationTime == nep->finfo.pr.creationTime) &&
                (tfinfo.modifyTime == nep->finfo.pr.modifyTime)) {

                /* Yes, use the copy as is */
                needCopy = PR_FALSE;
            }
            else {

                /* No, delete the copy and create a new one */
                rv = PR_Delete(tempname);
                if (rv == PR_FAILURE) {
                    NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_DELETETMPFAIL);
                    fd = NULL;
                    goto error;
                }
            }
        }

        if (needCopy == PR_TRUE) {
            PRInt64 size = nepfinfo->size;
            rv = NSFC_MakeTempCopy(fname, tempname, cip, &size);
            if (rv == PR_FAILURE) {
                /* Failed to create temporary copy */
                NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_CREATETMPFAIL);
                nep->flags |= NSFCENTRY_CREATETMPFAIL;
                fd = NULL;
                goto error;
            }

            if (size != nepfinfo->size) {
                NSFCSTATUSINFO_SET(statusInfo, NSFC_STATUSINFO_FILESIZE);
                NSFC_DeleteEntry(cip, nep, PR_FALSE);
                fd = NULL;
                goto error;
            }
        }
        
        nep->flags |= NSFCENTRY_TMPFILE;

        NSFC_ReleaseEntryWriteLock(cip, nep);
        releaseWrite = 0;
    }

do_open:
    fd = NSFC_PR_Open(tempname, PR_RDONLY, 0);

    return fd;

error:
    /* We get here only on an error with a NULL fd */
    if (releaseWrite) {
        NSFC_ReleaseEntryWriteLock(cip, nep);
    }
    return NULL;
}

