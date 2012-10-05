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
 * httpcompression.cpp:
 * 
 * Implements find-compressed SAF to support serving precompressed content.
 * Implements http-compression filter for compressing dynamic content in
 * gzip (RFC 1952 ) format.Uses zlib to support gzip standard.
 *
 * Kirankumar Arnepalli
 */

#include "base/net.h"
#include "base/util.h"
#include "base/pool.h"
#include "frame/log.h"
#include "frame/http.h"
#include "frame/filter.h"
#include "frame/httpfilter.h"
#include "safs/httpcompression.h"
#include "safs/dbtsafs.h"
#include "zlib.h"
#include "safs/nsfcsafs.h" /* GetServerFileCache */
#include "support/SimpleHash.h" 
#include <limits.h> /* INT_MAX */ 

/* find-compressed default values */
const PRBool FIND_COMPRESSED_DEFAULT_CHECK_AGE = PR_TRUE; // chck-age
const PRBool FIND_COMPRESSED_DEFAULT_VARY = PR_TRUE; // vary header

/* http-compression default values */
const PRBool HTTP_COMPRESSION_DEFAULT_VARY = PR_TRUE; // vary header
const int HTTP_COMPRESSION_DEFAULT_FRAGMENT_SIZE = 8192; // fragment-size
const int HTTP_COMPRESSION_DEFAULT_COMPRESSION_LEVEL = 6; // compression-level
const int HTTP_COMPRESSION_DEFAULT_WINDOW_SIZE = 15; // window-size
const int HTTP_COMPRESSION_DEFAULT_MEMORY_LEVEL = 8; // memory-level

/* compress-file default values */
const int COMPRESS_FILE_DEFAULT_MIN_SIZE = 256; // min-size
const int COMPRESS_FILE_DEFAULT_MAX_SIZE = 1048576; // max-size
const int COMPRESS_FILE_DEFAULT_COMPRESSION_LEVEL = 6; // compression-level
const PRBool COMPRESS_FILE_DEFAULT_VARY = PR_TRUE; // vary header
const PRBool COMPRESS_FILE_DEFAULT_CHECK_AGE = PR_TRUE; // check-age
const int COMPRESS_FILE_TIME_OUT = 10;
static PRLock *_cvLock = NULL;
static PRCondVar *_cvar = NULL; 
static SimpleStringHash *_hash = NULL;

/* defines for initializing various header fields for each gzip compression
 * data set we're going to generate.
 */
#define GZIP_ID1 31
#define GZIP_ID2 139
#ifdef XP_WIN32
#define GZIP_OS_TYPE 0x0b
#else
#define GZIP_OS_TYPE 0x03 /* Unix */
#endif

/* HttpCompressionData : used for mainitaining information across
 * filter method calls because filter methods are called in callback mode.
 */

typedef struct HttpCompressionData {

    /* output buffer */
    unsigned char *outbuf;

    /* output buffer size (fragment size) */
    int outbufSize;

    /* zstream used by zlib */
    z_stream *zstream;

    /* crc */
    unsigned long crc;

} HttpCompressionData;

/* RFC 2616 section 14.3, Parsing Accept-Encoding header stuff */
static PRBool parseAcceptEncodingHdr(char *acceptencoding, const char **codingtoset)
{
    char *token, *lasts;
    char *contentcoding = NULL;
    int gzipqvalue = -1;
    int identityqvalue = -1;
    int starqvalue = -1;

    /* An example header looks like follows
     * Accept-Encoding: gzip;q=1.0, identity; q=0.5, *;q=0
     */
    token = util_strtok(acceptencoding, ",", &lasts);
    while (token) {
        /* ignore leading white space in token if any */
        while ( isspace(*token) )
            token++;

        const char *ptr = token;
        while ( *ptr && !isspace(*ptr) && *ptr != ';' )
            ptr++;

        int len = ptr - token;
        if ((len==4 && !strncasecmp(token, "gzip", 4)) ||
            (len==6 && !strncasecmp(token, "x-gzip", 6)) ||
            (len==8 && !strncasecmp(token, "identity", 8)) ||
            (len==1 && !strncasecmp(token, "*", 1))) {

            int qvalue = util_qtoi(ptr, &ptr);

            if ( len == 1 ) /* token == '*' */
                starqvalue = qvalue;
            else if ( len == 4 ) /* token == gzip */
                gzipqvalue = qvalue;
            else if ( len == 6 ) { /* token == x-gzip */
                if (qvalue >= gzipqvalue)
                    *codingtoset = "x-gzip";
                gzipqvalue = qvalue;
            }
            else if ( len == 8 ) /* token == identity */
                identityqvalue = qvalue;
        }

        token = util_strtok(NULL, ",", &lasts);
    }

    /* Now make the decision based on q values */
    if (gzipqvalue > 0) {
        /* gzip is explicitly accepted, but is it the most preferred? */
        if (gzipqvalue >= starqvalue && gzipqvalue >= identityqvalue)
            return PR_TRUE;
    }
    if (gzipqvalue == -1 && starqvalue > 0) {
        /* gzip wasn't mentioned, so check if * is preferred over identity */
        if (starqvalue >= identityqvalue)
            return PR_TRUE;
    }

    return PR_FALSE;
}

/* ----------------------- find_compressed --------------------- */
int find_compressed(pblock * pb, Session * sn, Request * rq)
{
    const char *checkage;
    const char *vary;
    PRBool needToCheckAge = FIND_COMPRESSED_DEFAULT_CHECK_AGE;
    PRBool isVary = FIND_COMPRESSED_DEFAULT_VARY;
    char *acceptencoding;
    const char *newcontentcoding = "gzip";

    /* Read parameters check-age and vary from pblock and validate */
    checkage = pblock_findval("check-age", pb);
    if (checkage) {
        int ret = util_getboolean(checkage, -1);
        if ( ret == -1 ) {
            log_error(LOG_MISCONFIG, "find-compressed", sn, rq, 
                      XP_GetAdminStr(DBT_invalidBooleanValue),
                      "check-age");
            return REQ_ABORTED;
        }
        else
            needToCheckAge = ret;
    }
    else
        needToCheckAge = FIND_COMPRESSED_DEFAULT_CHECK_AGE;

    vary = pblock_findval("vary", pb);
    if (vary) {
        int ret = util_getboolean(vary, -1);
        if ( ret == -1 ) {
            log_error(LOG_MISCONFIG, "find-compressed", sn, rq, 
                      XP_GetAdminStr(DBT_invalidBooleanValue),
                      "vary");
            return REQ_ABORTED;
        }
        else
            isVary = ret;
    }
    else
        isVary = FIND_COMPRESSED_DEFAULT_VARY;

    /* Read request headers to find out whether client is able to receive
     * compressed content
     */
    acceptencoding = pblock_findval("accept-encoding", rq->headers);

    if ( !acceptencoding )
        return REQ_NOACTION;

    acceptencoding = STRDUP(acceptencoding);
    int retval = parseAcceptEncodingHdr(acceptencoding, &newcontentcoding);
    FREE(acceptencoding);

    if ( retval == PR_FALSE ) {
        /* Client won't be able to receive encoded content. So return
         * from here.
         */
        return REQ_NOACTION;
    }

    /* Get the requested file's information */
    pb_param *pp;
    char *file_path;
    struct stat finfo, cfinfo;

    /* if path_info present, return from here */
    if ( pblock_findkeyval(pb_key_path_info, rq->vars) )
        return REQ_NOACTION;

    pp = pblock_findkey(pb_key_path, rq->vars);
    file_path = pp->value;

    if (!file_path)
        return REQ_NOACTION;
 
    if ( system_stat(file_path, &finfo) < 0 )
        return REQ_NOACTION;

    /* Get the compressed file's information */
    char *compressed_file_path = (char *) MALLOC(strlen(file_path) + 4);
    util_sprintf(compressed_file_path, "%s.gz", file_path);

    if ( system_stat(compressed_file_path, &cfinfo) >= 0 ) {
        /* If check-age is true and compressed version is atleast as recent
         * as original file, change the path to point to new file.
         * i.e originalfilename.gz
         * Otherwise, do nothing and return.
         */
        if ( needToCheckAge == PR_TRUE ) {
            /* The server caches the stat() of the current path. Update it. */
            request_stat_path(NULL, rq);
            if ( cfinfo.st_mtime < finfo.st_mtime ) {
                return REQ_NOACTION;
            }
        }

        /* Now change the path to point to compressed version of the file */
        FREE(file_path);
        pp->value = compressed_file_path;

        /* Set up new uri to restart the request */
        char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
        char *newUri = (char *) MALLOC(strlen(uri) + 4);
        util_sprintf(newUri, "%s.gz", uri);

        int rv = request_restart(sn, rq, NULL, newUri, NULL);

        FREE(newUri);

        /* Also see whether you have to insert Vary Header */
        if ( isVary == PR_TRUE ) {
            /* set HTTP Vary tag, which notify client that this document
             * is subject to change based on Accept-Encoding header
             * from client
             */
            pblock_nvinsert("vary", "accept-encoding", rq->srvhdrs);
        }

        /* Set Content-Encoding header to gzip ( or x-gzip ) */
        pblock_nvinsert("content-encoding", newcontentcoding, rq->srvhdrs);

        return rv;
    }

    return REQ_NOACTION;
}

/* ----------------------- httpcompression_insert --------------------- */
int http_compression_insert(FilterLayer *layer, pblock *pb)
{
    const char *vary;
    const char *fragmentSize;
    const char *compressionLevel;
    const char *windowSize;
    const char *memoryLevel;
    int intCompressionLevel;
    int intWindowSize;
    int intMemoryLevel;
    HttpCompressionData *data;
    char *acceptencoding;
    const char *newcontentcoding = "gzip";
    PRBool isVary = HTTP_COMPRESSION_DEFAULT_VARY;

    /* Allocate HttpCompressionData to maintain state information across
     * filter method calls.
     */
    data = (HttpCompressionData *) MALLOC(sizeof(HttpCompressionData));

    /* Get the parameters passed in insert-filter and validate them */
    vary = pblock_findval("vary", pb);
    if (vary) {
        int ret = util_getboolean(vary, -1);
        if ( ret == -1 ) {
            log_error(LOG_MISCONFIG, "http-compression-insert",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_invalidBooleanValue),
                      "vary");
            return REQ_ABORTED;
        }
        else
            isVary = ret;
    }
    else
        isVary = HTTP_COMPRESSION_DEFAULT_VARY;

    fragmentSize = pblock_findval("fragment-size", pb);
    if (fragmentSize) {
        int fsz = atoi(fragmentSize);
        if (fsz >= 1024) /* minimum 1024 bytes should be acceptable */
            data->outbufSize = fsz;
        else {
            log_error(LOG_MISCONFIG, "http-compression-insert",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_invalidFragmentSize));
            return REQ_ABORTED;
        }
    }
    else
        data->outbufSize = HTTP_COMPRESSION_DEFAULT_FRAGMENT_SIZE;

    compressionLevel = pblock_findval("compression-level", pb);
    if (compressionLevel) {
        int clv = atoi(compressionLevel);
        if (( clv >= 1) && (clv <= 9))
            intCompressionLevel = clv;
        else {
            log_error(LOG_MISCONFIG, "http-compression-insert",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_invalidCompressionLevel));
            return REQ_ABORTED;
        }
    }
    else
        intCompressionLevel = HTTP_COMPRESSION_DEFAULT_COMPRESSION_LEVEL;

    windowSize = pblock_findval("window-size", pb);
    if (windowSize) {
        int wsz = atoi(windowSize);
        if (( wsz >= 9) && (wsz <= 15))
            intWindowSize = -wsz; // zlib needs a negative value
        else {
            log_error(LOG_MISCONFIG, "http-compression-insert",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_invalidWindowSize));
            return REQ_ABORTED;
        }
    }
    else
        intWindowSize = -HTTP_COMPRESSION_DEFAULT_WINDOW_SIZE;

    memoryLevel = pblock_findval("memory-level", pb);
    if (memoryLevel) {
        int mlvl = atoi(memoryLevel);
        if (( mlvl >= 1) && (mlvl <= 9))
            intMemoryLevel = mlvl;
        else {
            log_error(LOG_MISCONFIG, "http-compression-insert",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_invalidMemoryLevel));
            return REQ_ABORTED;
        }
    }
    else
        intMemoryLevel = HTTP_COMPRESSION_DEFAULT_MEMORY_LEVEL;

    /* Store a pointer to HttpCompressionData in FilterLayer */
    layer->context->data = data;

    /* We can't handle ranges: gzip("123")[1] != gzip("2") */
    if (layer->context->rq->status_num == PROTOCOL_PARTIAL_CONTENT) {
        return REQ_NOACTION;
    }
    param_free(pblock_remove("accept-ranges", layer->context->rq->srvhdrs));

    /* A compressed response is semantically equivalent to the non-compressed
     * resource, but they differ on an octet-by-octet basis
     */
    http_weaken_etag(layer->context->sn, layer->context->rq);

    /* Read request headers to find out whether client is able to receive
     * compressed content
     */
    acceptencoding = pblock_findval("accept-encoding", layer->context->rq->headers);

    if (!acceptencoding) {
        /* Client won't accept this encoding. So don't insert filter */
        return REQ_NOACTION;
    }

    acceptencoding = STRDUP(acceptencoding);
    int retval = parseAcceptEncodingHdr(acceptencoding, &newcontentcoding);
    FREE(acceptencoding);

    if ( retval == PR_FALSE ) {
        /* Client won't be able to receive encoded content.
         * So don't insert filter.
         */
        return REQ_NOACTION;
    }

    const char *contentcoding = pblock_findval("content-encoding", layer->context->rq->srvhdrs);
    if (contentcoding) {
        if ( !strcasecmp(contentcoding, "identity") ) {
            pblock_nvreplace("content-encoding", newcontentcoding, layer->context->rq->srvhdrs);
        }
        else {
            /* Its already encoded content; So Don't insert filter  */
            return REQ_NOACTION;
        }
    }
    else {
        pblock_nvinsert("content-encoding", newcontentcoding, layer->context->rq->srvhdrs);
    }

    if ( isVary == PR_TRUE ) {
        /* set HTTP Vary tag, which notify client that this document
         * is subject to change based on Accept-Encoding header
         * from client
         */
        pblock_nvinsert("vary", "accept-encoding", layer->context->rq->srvhdrs);
    }

    /* From here we're on for gzipping the content. So remove content-length */
    pb_param *pp;
    pp = pblock_remove("content-length", layer->context->rq->srvhdrs);
    if (pp)
        param_free(pp);

    /* Allocate memory for the output buffer */
    data->outbuf = (unsigned char *) MALLOC(data->outbufSize*sizeof(unsigned char));

    /* Initialize zlib */
    int zrv;
    data->zstream = (z_stream *) MALLOC(sizeof(z_stream));
    z_stream *zstream = data->zstream;
    memset(zstream, 0, sizeof(z_stream));
    zrv = deflateInit2(zstream, intCompressionLevel,
                       Z_DEFLATED, intWindowSize,
                       intMemoryLevel,
                       Z_DEFAULT_STRATEGY);

    if ( zrv != Z_OK ) {
        log_error(LOG_FAILURE, "http-compression-insert",
                  layer->context->sn, layer->context->rq, 
                  XP_GetAdminStr(DBT_zlibInitFailure),
                  zrv);
        return REQ_ABORTED;
    }

    /* Initialize crc32 */
    data->crc = crc32(0, Z_NULL, 0);

    unsigned char *outbuf = data->outbuf;

    outbuf[0] = GZIP_ID1;
    outbuf[1] = GZIP_ID2;
    outbuf[2] = Z_DEFLATED;
    outbuf[3] = Z_BINARY;
    outbuf[4] = outbuf[5] = outbuf[6] = outbuf[7] = 0;
    outbuf[8] = 0;
    outbuf[9] = GZIP_OS_TYPE;

    /* specify output stream initially. Note that gzip header consumes
     * first 10 bytes
     */
    zstream->next_out = data->outbuf + 10;
    zstream->avail_out = data->outbufSize - 10;

    return REQ_PROCEED;
}

int http_compression_write(FilterLayer *layer, const void *buf, int amount)
{
    int zrv;
    HttpCompressionData *data;
    z_stream *zstream;

    data = (HttpCompressionData *) layer->context->data;
    zstream = data->zstream;

    /* dangerous, but no other way */
    unsigned char *inbuf = (unsigned char *)buf;

    if ( amount > 0 ) {
        /* recompute the crc for this input buffer */
        data->crc = crc32(data->crc, inbuf, amount);

        /* specify input stream */
        zstream->next_in = inbuf;
        zstream->avail_in = amount;

        while ( zstream->avail_in != 0 ) {
            zrv = deflate(zstream, Z_NO_FLUSH);
            if ( zrv != Z_OK && zrv != Z_BUF_ERROR ) {
                log_error(LOG_FAILURE, "http-compression-write",
                          layer->context->sn, layer->context->rq, 
                          XP_GetAdminStr(DBT_zlibInternalError),
                          zrv);
                return IO_ERROR;
            }

            if ( zstream->avail_out == 0 ) {
                int len = data->outbufSize - zstream->avail_out;
                int rv = net_write(layer->lower, data->outbuf, len);
                if ( rv != len)
                    return IO_ERROR;

                /* reset output stream */
                zstream->next_out = data->outbuf;
                zstream->avail_out = data->outbufSize;
            }
            else
                return amount;
        }
    }

    return amount;
}

int http_compression_flush(FilterLayer *layer)
{
    HttpCompressionData *data;
    int zrv;
    z_stream *zstream;
    int len;

    data = (HttpCompressionData *) layer->context->data;
    zstream = data->zstream;
    /* Do the flush atleast once */
    do {
        zrv = deflate(zstream, Z_SYNC_FLUSH);
        if ( zrv != Z_OK && zrv != Z_BUF_ERROR ) {
            log_error(LOG_FAILURE, "http-compression-flush",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_zlibInternalError),
                      zrv);
            return IO_ERROR;
        }
        len = data->outbufSize - zstream->avail_out;
        int rv = net_write(layer->lower, data->outbuf, len);
        if ( rv != len)
            return IO_ERROR;

        /* reset output stream */
        zstream->next_out = data->outbuf;
        zstream->avail_out = data->outbufSize;
    } while ( zstream->avail_in != 0 || len == data->outbufSize );

    return net_flush(layer->lower);
}

/* ----------------------- httpcompression_remove --------------------- */
void http_compression_remove(FilterLayer *layer)
{
    HttpCompressionData *data;
    int zrv;
    z_stream *zstream;
    PRBool io_error = PR_FALSE;

    data = (HttpCompressionData *) layer->context->data;
    zstream = data->zstream;
    unsigned char *outbuf = data->outbuf;

    for (;;) {
        zrv = deflate(zstream, Z_FINISH);
        if ( zrv == Z_STREAM_END ) {
            int len = data->outbufSize - zstream->avail_out;

            /* Add crc and write remaining bytes */
            if ( (len + 8) >= data->outbufSize ) {
                /* Rare case but we need to hanlde it */
                data->outbuf = (unsigned char *) REALLOC( data->outbuf, data->outbufSize + 8);
                data->outbufSize += 8;
                outbuf = data->outbuf;
            }

            unsigned long crc = data->crc;

            outbuf[len] = crc & 0xff;
            outbuf[len+1] = (crc >> 8) & 0xff;
            outbuf[len+2] = (crc >> 16) & 0xff;
            outbuf[len+3] = (crc >> 24) & 0xff;

            outbuf[len+4] = zstream->total_in & 0xff;
            outbuf[len+5] = (zstream->total_in >> 8) & 0xff;
            outbuf[len+6] = (zstream->total_in >> 16) & 0xff;
            outbuf[len+7] = (zstream->total_in >> 24) & 0xff;

            /* write them out */
            int rv = net_write(layer->lower, outbuf, len+8);
            if ( rv != (len + 8) )
                io_error = PR_TRUE;

            break;
        }
        else if ( zrv != Z_OK && zrv != Z_BUF_ERROR && !io_error) {
            log_error(LOG_FAILURE, "http-compression-remove",
                      layer->context->sn, layer->context->rq, 
                      XP_GetAdminStr(DBT_zlibInternalError),
                      zrv);
            break;
        }

        /* Got Z_OK or Z_BUF_ERROR
         * we have compressed data available to write
         * Note that Z_BUF_ERROR is not fatal and just proceed
         * by making "avail_out > 0"
         */
        int len = data->outbufSize - zstream->avail_out;

        /* If we get a Z_BUF_ERROR and there is nothing
         * to be written, then break
         */
        if ( len == 0 && zrv == Z_BUF_ERROR )
            break;

        int rv = net_write(layer->lower, outbuf, len);

        /* IO_ERROR */
        if ( rv != len ) {
            io_error = PR_TRUE;
            break;
        }

        /* Reinitialize avail_out and next_out */
        zstream->avail_out = data->outbufSize;
        zstream->next_out = data->outbuf;
    }

    /* We're done */
    zrv = deflateEnd(zstream);
    if (zrv != Z_OK && !io_error) {
        log_error(LOG_FAILURE, "http-compression-remove",
                  layer->context->sn, layer->context->rq, 
                  XP_GetAdminStr(DBT_zlibInternalError),
                  zrv);
    }

    /* Destroy the objects */
    FREE(data->outbuf);
    FREE(data->zstream);
    FREE(data);
}

/* ----------------------- compress_file_init --------------------- */
void compress_file_init(void)
{
    _cvLock= PR_NewLock();
    PR_ASSERT(_cvLock != NULL);
    _cvar = PR_NewCondVar(_cvLock);
    PR_ASSERT(_cvar != NULL);
    if (_hash)
        delete _hash;
    _hash = new SimpleStringHash(10);
}

/* ------------------------- httpcompression_init ------------------------ */

PRStatus http_compression_init(void)
{
    FilterMethods methods = FILTER_METHODS_INITIALIZER;
    const Filter *filter;

    /*
     * Create the http-compression filter.This is a FILTER_CONTENT_CODING
     * filter.
     */
    methods.insert = &http_compression_insert;
    methods.remove = &http_compression_remove;
    methods.write = &http_compression_write;
    methods.flush = &http_compression_flush;
    filter = filter_create("http-compression",
                           FILTER_CONTENT_CODING,
                           &methods);
    if (!filter)
        return PR_FAILURE;

    compress_file_init();
    return PR_SUCCESS;
}

/* This method actually creates the compressed file. 
 * return values -1 if error, 0 if success 
 */
static int compressFile(Session *sn, Request *rq, 
                        const char *compressedFilePath, 
                        const char *originalFilePath, 
                        int compressionLevel)
{
    char *tempFile=NULL;
    char *temp = NULL;
    int tempfd=0;
    // Create and open temp file.
#ifdef XP_WIN32
    tempFile = STRDUP(compressedFilePath);
    char *prefix = strrchr(tempFile,'/');
    *prefix='\0'; 
    prefix++;

    char tempFilenameWin[MAX_PATH];
    tempFilenameWin[0]='\0';
    UINT retVal = GetTempFileName(tempFile, prefix, 0, tempFilenameWin);
    if (retVal == 0) {
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_tempfileFailure), 
                  compressedFilePath, system_errmsg());
        FREE(tempFile);
        return -1;
    }
    // hack from now refer it as
    temp = tempFilenameWin;
    // Use this temp file for compression 
    gzFile file = gzopen(tempFilenameWin, "wb");
#else
    int len = strlen(compressedFilePath) + 6;
    tempFile = (char *)MALLOC(len+1);
    util_sprintf(tempFile, "%sXXXXXX", compressedFilePath);
    tempFile[len]='\0';

    tempfd = mkstemp(tempFile);
    if (tempfd == -1) { /* Error no suitable file could be created. */
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_tempfileFailure), 
                  compressedFilePath, system_errmsg());
        if (tempFile) FREE(tempFile);
        return -1;
    }

    // hack from now refer it as
    temp = tempFile;
    /* Use this temp file for compression */
    gzFile file = gzdopen(tempfd, "wb");
#endif
    if (file == NULL) {
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_zlibgzopenFailure), 
                  temp, system_errmsg());
        FREE(tempFile);
        return -1;
    }

    //  set compression level as read from obj.conf 
    int zrv = gzsetparams (file, compressionLevel, Z_DEFAULT_STRATEGY);
    if (zrv != Z_OK) {
        // Could get Z_STREAM_ERROR if the file was not opened for writing. 
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_zlibgzsetparamsFailure), zrv);
        gzclose (file);
        FREE(tempFile);
        return -1;
    }

    // Open the original requested file we want to compress 
    SYS_FILE origfd = system_fopenRO(originalFilePath);
    if (origfd == SYS_ERROR_FD) {
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_systemfopenROFailure),
                  originalFilePath, system_errmsg());
        gzclose(file);
        FREE(tempFile);
        return -1;  
    }

    // compress until end of file 
    PRInt32 buf_sz = 8192;
    char buf[8192];
    PRInt32 read = 0;
    PRInt32 wrote = 0;
    PRInt32 avail = 0;
    do {
        avail = system_fread(origfd, buf, buf_sz);
        if (avail == IO_ERROR) {
            log_error(LOG_FAILURE, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_systemfreadFailure),
                      originalFilePath, system_errmsg());
            gzclose (file);
            system_fclose(origfd);
            FREE(tempFile);
            return -1; 
        }
        read += avail;
        // if avail is 0,zrv is also 0 which also means error
        if (avail >0) {
            zrv = gzwrite (file, buf, avail);
            if (zrv == 0)  { // error 
                log_error(LOG_FAILURE, "compress-file", sn, rq, 
                          XP_GetAdminStr(DBT_zlibgzwriteFailure),
                          temp, system_errmsg());
                gzclose (file);
                system_fclose(origfd);
                FREE(tempFile);
                return -1;
            }
            wrote += avail;
        }
    } while (avail != IO_EOF);

    gzclose(file);
    system_fclose(origfd);

    // rename this temp file 
    /* ISSUE: if the destination file is open by other thread then 
     * rename will fail for windows. */
    int rv = rename(temp, compressedFilePath);
    if (rv < 0) { // Could be -1
        log_error(LOG_FAILURE, "compress-file", sn, rq, 
                  XP_GetAdminStr(DBT_renameFailure), 
                  temp, compressedFilePath, system_errmsg());
        // clean up
        system_unlink(temp);
        FREE(tempFile);
        return rv;
    }

    NSFCCache nsfcCache = GetServerFileCache();
    NSFC_RefreshFilename(compressedFilePath, nsfcCache); 

    FREE(tempFile);
    return rv;
}

static int filenameIsInHash(const char *filename) 
{
    if (_hash == NULL)
        return 0;
    const char* val = (const char *)_hash->lookup((void *)filename);
    if (val) 
        return 1;
    return 0;
}

static int addFilenameInHash(const char *filename) 
{
    if (_hash == NULL)
        return -1;
    const char* oldval = (const char *)_hash->lookup((void *)filename);
    if (oldval) 
        return -1;
    _hash->insert((void *)filename, (void *)filename);
    return 1;
}

static void removeFilenameFromHash(const char *filename) 
{
    if (_hash == NULL)
        return;
    _hash->remove((void *)filename);
}


/* 
 * Returns 1 if file is ready to serve as it is i.e.
 *     if compressed file exists AND if check-age is false. OR 
 *     if check-age is true AND if compressed-file is newer than original file.
 *
 * Returns 0
 *     if compressed file doesn't exist. OR
 *     if compressed file exists AND if check-age is true AND if compressed 
 *         file is older than the requested original file.
 *
 * Returns -1 
 *     if compressed file exists AND if check-age is true AND the original 
 *         file doesn't exist.
 */
static int compressedFileIsReadyToServe(Request *rq, 
                                        const char *compressedFilePath, 
                                        const char *originalFilePath, 
                                        PRBool needToCheckAge)
{
    /* Call stat(compressed-file) and verify that the file doesn't exist OR 
     * if check-age==true do a timestamp comparison if its newer than the 
     * original file
     */
    struct stat cfinfo;
    if ( system_stat(compressedFilePath, &cfinfo) >= 0 ) {
        /* If check-age is true and compressed version is atleast as recent
         * as original file, return 1. * Otherwise, return 0.
         */
        if ( needToCheckAge == PR_TRUE ) {
            struct stat finfo, cfinfo;
            if ( system_stat(originalFilePath, &finfo) < 0 )
                return -1;
            /* The server caches the stat() of the current path. Update it. */
            request_stat_path(NULL, rq);
            if ( cfinfo.st_mtime < finfo.st_mtime )
                return 0;
            else
                return 1;
        } else 
            return 1;
    } 
    else 
        return 0;
}

static int createCompressedFile(Session *sn, Request *rq, 
                                const char *compressedFilePath, 
                                const char *originalFilePath, 
                                int compressionLevel, PRBool needToCheckAge)
{
    /* Calling function has already stat(compressed-file) and 
     * verified that the file doesn't exist OR 
     * if check-age==true check if its older than the original file
     */
    int rv = 0;
    int added = 0;
    if (compressedFilePath == NULL || originalFilePath == NULL)
        return -1;

    /* While one thread is compressing a file, and then request(s) comes in 
     * for compressing the same file rather than compressing the file again 
     * and again, the first thread adds the (original) filename in a hashtable 
     * so the subsequent threads wait for the first thread to complete the 
     * compression 
     */

    // This lock will be the worst bottleneck 
    PR_Lock(_cvLock);
    if (filenameIsInHash(originalFilePath)) {
        while (filenameIsInHash(originalFilePath))
            PR_WaitCondVar(_cvar, PR_SecondsToInterval(COMPRESS_FILE_TIME_OUT));
        // File is by now compressed by the other thread but verify by stat().. 
        PR_Unlock(_cvLock);
        // Check if file creation by the other thread was successful
        rv = compressedFileIsReadyToServe(rq, compressedFilePath, 
                                          originalFilePath,
                                          needToCheckAge);
        // if (rv == -1) Return error
        // if (rv == 1) Do nothing, serve the file as it is
        if (rv == 0)  { // current thread should try to compress it 
            PR_Lock(_cvLock);
            added = addFilenameInHash(originalFilePath);
            PR_Unlock(_cvLock);
        }
    } else {
        // If current thread is the first thread, it will create compressed file
        added = addFilenameInHash(originalFilePath);
        PR_Unlock(_cvLock);
    }
    /* ISSUE: IF n (n>1) threads are trying to get the same file, 
     * do a stat(compressed-file) in the calling function, they get file 
     * not found, they come into this function. If the first thread adds 
     * the filename in hashtable, creats the compressed file and removed the 
     * filename from hashtable, while the current thread starved for the lock 
     * all this while and by the time the current thread called 
     * filenameIsInHash(), the first thread has removed filename from hashtable.
     * In that scenario, the current thread will create the compressed 
     * file again. Which is an overkill but can't be avoided.
     */
    if (added) {
        // File should be now compressed by this thread 
        rv = compressFile(sn, rq, compressedFilePath, originalFilePath, 
                          compressionLevel);
    }
    PR_Lock(_cvLock);
    if (added)
        removeFilenameFromHash(originalFilePath);
    // This wakes up all the threads that are waiting for other filenames too
    PR_NotifyAllCondVar(_cvar);
    PR_Unlock(_cvLock);
    return rv;
}

/* ----------------------- compress_file --------------------- */
int compress_file(pblock * pb, Session * sn, Request * rq)
{
    const char *checkage;
    const char *vary;
    const char *compressionLevel;
    const char *subdir;
    const char *minSize;
    const char *maxSize;
    PRBool needToCheckAge = COMPRESS_FILE_DEFAULT_CHECK_AGE;
    PRBool isVary = COMPRESS_FILE_DEFAULT_VARY;
    int intCompressionLevel;
    PRInt32 int32MinSize = COMPRESS_FILE_DEFAULT_MIN_SIZE;
    PRInt32 int32MaxSize = COMPRESS_FILE_DEFAULT_MAX_SIZE;
    char *acceptEncoding;
    const char *newContentCoding = "gzip";
    char *filePath=NULL;
    struct stat finfo, cfinfo;
    pb_param *pp;

    /* Read parameters check-age and vary from pblock and validate */
    checkage = pblock_findval("check-age", pb);
    if (checkage) {
        int ret = util_getboolean(checkage, -1);
        if ( ret == -1 ) {
            log_error(LOG_MISCONFIG, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_invalidBooleanValue),
                      "check-age");
            return REQ_ABORTED;
        }
        else
            needToCheckAge = ret;
    }
    else
        needToCheckAge = COMPRESS_FILE_DEFAULT_CHECK_AGE;

    vary = pblock_findval("vary", pb);
    if (vary) {
        int ret = util_getboolean(vary, -1);
        if ( ret == -1 ) {
            log_error(LOG_MISCONFIG, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_invalidBooleanValue),
                      "vary");
            return REQ_ABORTED;
        }
        else
            isVary = ret;
    }
    else
        isVary = COMPRESS_FILE_DEFAULT_VARY;

    subdir = pblock_findval("subdir", pb);

    compressionLevel = pblock_findval("compression-level", pb);
    if (compressionLevel) {
        int clv = atoi(compressionLevel);
        if (( clv >= 1) && (clv <= 9))
            intCompressionLevel = clv;
        else {
            log_error(LOG_MISCONFIG, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_invalidCompressionLevel));
            return REQ_ABORTED;
        }
    }
    else
        intCompressionLevel = COMPRESS_FILE_DEFAULT_COMPRESSION_LEVEL;

    minSize = pblock_findval("min-size", pb);
    if (minSize) {
        PRInt32 minSz = atoi(minSize);
        if ((minSz >= 0) && (minSz <= INT_MAX))
            int32MinSize = minSz;
        else {
            log_error(LOG_MISCONFIG, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_invalidMinSize), 0, INT_MAX);
            return REQ_ABORTED;
        }
    }
    else
        int32MinSize = COMPRESS_FILE_DEFAULT_MIN_SIZE;

    maxSize = pblock_findval("max-size", pb);
    if (maxSize) {
        PRInt32 maxSz = atoi(maxSize);
        if ((maxSz >= int32MinSize) && (maxSz <= INT_MAX))
            int32MaxSize = maxSz;
        else {
            log_error(LOG_MISCONFIG, "compress-file", sn, rq, 
                      XP_GetAdminStr(DBT_invalidMaxSize), int32MinSize, INT_MAX);
            return REQ_ABORTED;
        }
    }
    else
        int32MaxSize = COMPRESS_FILE_DEFAULT_MAX_SIZE;

    /* Read request headers to find out whether client is able to receive
     * compressed content
     */
    acceptEncoding = pblock_findval("accept-encoding", rq->headers);

    if ( !acceptEncoding )
        return REQ_NOACTION;

    acceptEncoding = STRDUP(acceptEncoding);
    int retVal = parseAcceptEncodingHdr(acceptEncoding, &newContentCoding);
    FREE(acceptEncoding);

    if ( retVal == PR_FALSE ) {
        /* Client won't be able to receive encoded content. So return
         * from here.
         */
        return REQ_NOACTION;
    }

    /* if path_info present, return from here */
    if ( pblock_findkeyval(pb_key_path_info, rq->vars) )
        return REQ_NOACTION;

    /* Get the requested file's information */
    pp = pblock_findkey(pb_key_path, rq->vars);
    filePath = pp->value;

    if (!filePath)
        return REQ_NOACTION;

    /* If the file ends with an extension .gz, don't compress it again */
    int len = strlen(filePath);
    char *p = filePath + len - strlen(".gz");
    if (!strcasecmp(p, ".gz"))
        return REQ_NOACTION;

    const char *contentcoding = pblock_findval("content-encoding", rq->srvhdrs);
    if (contentcoding && (strcasecmp(contentcoding, "identity"))) {
        /* Its already encoded content; So do nothing */
        return REQ_NOACTION;
    }

    if ( system_stat(filePath, &finfo) < 0 )
        return REQ_NOACTION;

    /* The original file should be between max-size and min-size */
    if ((finfo.st_size < int32MinSize) || (finfo.st_size > int32MaxSize))
        return REQ_NOACTION;

    /* Get the compressed file's information */
    char *compressedFilePath = NULL;
    if (subdir && strcmp(subdir,".")) {
        len = strlen(filePath) + strlen("/") + strlen(subdir) + 
                  strlen(".gz");
        char *parentPath = STRDUP(filePath);
        char *fileName = strrchr(parentPath,'/');
        if (fileName)
            *fileName='\0';
        fileName++;
        compressedFilePath = (char *) MALLOC(len +1);
        util_sprintf(compressedFilePath, "%s/%s/%s.gz", parentPath,
                     subdir, fileName);
        compressedFilePath[len] = '\0';

        /* Make subdir if it doesn't exist */
        len = strlen(parentPath) + strlen("/") + strlen(subdir);
        char *subdirPath = (char *)MALLOC(len +1);
        util_sprintf(subdirPath, "%s/%s", parentPath, subdir);
        subdirPath[len] = '\0';
        
        struct stat pfinfo;
        if ( system_stat(subdirPath, &pfinfo) < 0) {
            // race condition between stat and dir_create_all
            retVal = dir_create_all(subdirPath);
            if (retVal == -1) {
                log_error(LOG_FAILURE, "compress-file", sn, rq, 
                          XP_GetAdminStr(DBT_dircreateallFailure), 
                          subdirPath, system_errmsg());
                FREE(compressedFilePath);
                FREE(parentPath);
                FREE(subdirPath);
                return REQ_NOACTION;
             }
        }
        FREE(subdirPath);
        FREE(parentPath);
    } else {
        len= strlen(filePath) + strlen(".gz");
        compressedFilePath = (char *) MALLOC(len + 1);
        util_sprintf(compressedFilePath, "%s.gz", filePath);
        compressedFilePath[len] = '\0';
    }

    /* If the compressed file doesn't exist,
     *     create the compressed file and restart the request with the new uri.
     * If the compressed file exists AND if check-age is true
     *     if the compressed file is older than the original file 
     *         create the compressed file and restart the request with new uri.
     *     if the compressed file is NOT older than the original file 
     *         restart the request with the new uri.
     * If compressed file exists AND if check-age is false
     *         restart the request with the new uri.
     */
    int rv = 0;
    if ( system_stat(compressedFilePath, &cfinfo) >= 0) {
        if ( needToCheckAge == PR_TRUE ) {
            /* The server caches the stat() of the current path. Update it. */
            request_stat_path(NULL, rq);
            if ( cfinfo.st_mtime < finfo.st_mtime ) {
                rv = createCompressedFile(sn, rq, compressedFilePath, filePath, 
                                          intCompressionLevel, needToCheckAge);
            }
        }
    } else
        rv = createCompressedFile(sn, rq, compressedFilePath, filePath,
                                  intCompressionLevel, needToCheckAge);

    /* If there was a problem creating compressed file, return */
    if (rv < 0) {
        FREE(compressedFilePath);
        return REQ_NOACTION;
    }

    /* Now change the path to point to compressed version of the file */
    FREE(filePath);
    pp->value = compressedFilePath;

    /* Set up new uri to restart the request */
    char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
    char *newUri = NULL;

    if (subdir && strcmp(subdir,".")) {
        len= strlen(uri) + strlen(subdir) + strlen("/") + strlen(".gz");
        newUri = (char *) MALLOC(len +1);
        char *parentUri = STRDUP(uri);
        char *fileName = strrchr(parentUri,'/');
        if (fileName) 
            *fileName='\0';
        fileName++;
        util_sprintf(newUri, "%s/%s/%s.gz", parentUri, subdir, fileName);
        newUri[len] = '\0';

        FREE(parentUri);
    } else {
        len=strlen(uri) + strlen(".gz");
        newUri = (char *) MALLOC(len + 1);
        util_sprintf(newUri, "%s.gz", uri);
        newUri[len]='\0';
    }

    rv = request_restart(sn, rq, NULL, newUri, NULL);

    FREE(newUri);

    /* Also see whether you have to insert Vary Header */
    if ( isVary == PR_TRUE ) {
        /* set HTTP Vary tag, which notify client that this document
         * is subject to change based on Accept-Encoding header
         * from client
         */
        pblock_nvinsert("vary", "accept-encoding", rq->srvhdrs);
    }

    return rv;
}
