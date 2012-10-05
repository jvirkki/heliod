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

#ifdef XP_WIN32
#define _MBCS
#include <windows.h>
#include <mbctype.h>
#endif

#include "base/util.h"
#include "base/pool.h"
#include "frame/conf_api.h"
#include "support/stringvalue.h"

#ifdef XP_WIN32
static PRBool _getfullpathname = -1;
#endif /* XP_WIN32 */

/* --------------------------- util_uri_is_evil --------------------------- */

static inline int allow_dbcs_uri()
{
    static int flagDbcsUri = -1;
    if (flagDbcsUri == -1) {
        flagDbcsUri = StringValue::getBoolean(conf_findGlobal("DbcsUri"));
    }
    return flagDbcsUri;
}

#ifdef XP_WIN32
void set_fullpathname(PRBool b)
{
    _getfullpathname = b;
}
#endif  /*XP_WIN32*/

NSAPI_PUBLIC int util_uri_is_evil_internal(const char *t, int allow_tilde, int allow_dot_dir)
{
#ifdef XP_WIN32
    int flagDbcsUri = allow_dbcs_uri();
#endif // XP_WIN32
    PRBool flagEmptySegment = PR_FALSE;
    register int x;

    for (x = 0; t[x]; ++x) {
        if (t[x] == '/') {
            if (flagEmptySegment)
                return 1; // "/;a/b"
#ifdef XP_WIN32
            if (t[x+1] == '/' && x != 0)
#else
            if (t[x+1] == '/')
#endif
                return 1;
            if (t[x+1] == ';')
                flagEmptySegment = PR_TRUE; // "/;a/b" is evil, "/a/;b" is not
            if (t[x+1] == '.') {
                /* "." at end of line is always prohibited */
                if (t[x+2] == '\0')
                    return 1;

                /* "." as a path segment is prohibited conditionally */
                if (!allow_dot_dir && (t[x+2] == '/' || t[x+2] == ';'))
                    return 1;

                /* ".." as a path segment is always prohibited */
                if (t[x+2] == '.' && (t[x+3] == '/' || t[x+3] == ';' || t[x+3] == '\0'))
                    return 1;
            }
        }
#ifdef XP_WIN32
        // Don't allow '~' in the filename.  On some filesystems a long name
        // (e.g. longfilename.htm) can be accessed using '~' bypassing any ACL
        // checks (e.g. longfi~1.htm).
        if (!allow_tilde && (t[x] == '~')) {
            return 1;
        }

        // Do not allow ':' apart from drive letter. Windows filestream 
        // will treat filename::$DATA as a plain file & display content.
        // So block it to prevent source viewing vulnerability.
        if ((t[x] == ':') && x > 1) {
            return 1;
        } 

        // On NT, the directory "abc...." is the same as "abc"
        // The only cheap way to catch this globally is to disallow
        // names with the trailing "."s.  Hopefully this is not over
        // restrictive.
        // Also trailing spaces in names can wreak havoc on ACL checks
        // and name resolution.  Therefore, ban them on the end of a
        // name.
        if (((t[x] == '.') || (t[x] == ' ')) && 
            ((t[x+1] == ';') || (t[x+1] == '/') || (t[x+1] == '\0')))
        {
            return 1;
        }

        // Skip past the second byte of two byte DBCS characters.  Bug 353999
        if (flagDbcsUri && t[x+1] && IsDBCSLeadByte(t[x])) x++;
#endif // XP_WIN32
    }
    return 0;
}

NSAPI_PUBLIC int util_uri_is_evil(const char *t)
{
    return util_uri_is_evil_internal(t, 0, 0);
}


/* -------------------- util_uri_unescape_and_normalize -------------------- */

#ifdef XP_WIN32
/* The server calls this function to unescape the URI and also normalize 
 * the uri.  Normalizing the uri converts all "\" characters in the URI
 * and pathinfo portion to "/".  Does not touch "\" in query strings.
 */
NSAPI_PUBLIC
int util_uri_unescape_and_normalize(pool_handle_t *pool, char *s, char *unnormalized)
{
    if(!(util_uri_unescape_strict(s)))
        return 0;

    if (unnormalized) strcpy(unnormalized, s);

    if (_getfullpathname == -1)
        _getfullpathname = (_getmbcp() != 0);

    /* Get canonical filename Bugid: 4672869 */
    if(_getfullpathname && strcmp(s, "*") && (*s == '/' ) ) {
        char *pzAbsPath = NULL;
        int pathlen = 0;
        int len = 0;
        int ret = 0;
        if(!(pzAbsPath = util_canonicalize_uri(pool, s, strlen(s), NULL))) {
            //Error canonicalizing; possibly pointing out of docroot
            return 0;
        }
        char *pzPath = (char *)MALLOC(MAX_PATH + 1); /* reserved byte for trailing slash */
        char *pzFilename = NULL;

        /* If required length of the buffer(pzPath) is more than the allocated one i.e. MAX_PATH(neglecting the reserved byte for trailing slash), return BAD REQUEST. This will happen if length of uri is more than the specified uri length(257) for MBCS windows */
        if(!(ret = GetFullPathName(pzAbsPath, MAX_PATH, pzPath, &pzFilename)) || ( ret > MAX_PATH)){
            FREE(pzAbsPath);
            FREE(pzPath);
            return 0;
        }
        len = strlen(pzAbsPath);
        pathlen = strlen( pzPath );

        /*  GetFullPathName behaves differently in case of WINNT and WIN2K */
        /* o/p string doesn't contain the trailing slash in case of WINNT */
        /* if i/p is /foo/, we get o/p as c:\foo instead of c:\foo\ */
        /* Checking if i/p has trailing slash and o/p doesn't have, then */
        /* adding slash */
        if ( pzAbsPath[len-1] == '/' && pzPath[pathlen-1] != '\\')
            strcat( pzPath, "\\");
        FREE(pzAbsPath);
        pzFilename = strchr(pzPath, '\\');
        if(!pzFilename) {
            FREE(pzPath);
            return 0;
        }
        strcpy(s, pzFilename);
        FREE(pzPath);
    }

    util_uri_normalize_slashes(s);

    return 1;
}
#endif /* XP_WIN32 */


/* ---------------------- util_uri_normalize_slashes ---------------------- */

void util_uri_normalize_slashes(char *s)
{
#ifdef XP_WIN32
    int flagDbcsUri = allow_dbcs_uri();

    while (*s) {
        if (*s == '\\') {
            // Normalize '\\' to '/'
            *s = '/';
        } else if (flagDbcsUri && s[1] && IsDBCSLeadByte(s[0])) {
            // Skip past two byte DBCS characters.  Bug 353999
            s++;
        }
        s++;
    }
#endif
}


/* --------------------------- util_uri_escape ---------------------------- */

NSAPI_PUBLIC char *util_uri_escape(char *od, const char *s)
{
    int flagDbcsUri = allow_dbcs_uri();
    char *d;

    if (!od)
        od = (char *) MALLOC((strlen(s)*3) + 1);
    d = od;

    while (*s) {
        if (strchr("% ?#:+&*\"'<>\r\n", *s)) {
            util_sprintf(d, "%%%02x", (unsigned char)*s);
            ++s; d += 3;
        }
#ifdef XP_WIN32
        else if (flagDbcsUri && s[1] && IsDBCSLeadByte(s[0]))
#else
        // Treat any character with the high bit set as a DBCS lead byte
        else if (flagDbcsUri && s[1] && (s[0] & 0x80))
#endif
	{
            // Escape the second byte of DBCS characters.  The first byte will
            // have been escaped already.  IE translates all unescaped '\\'s
            // into '/'.
            // Bug 353999
            util_sprintf(d, "%%%02x%%%02x", (unsigned char)s[0], (unsigned char)s[1]);
            s += 2; d += 6;
        }
        else if (0x80 & *s) {
            util_sprintf(d, "%%%02x", (unsigned char)*s);
            ++s; d += 3;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0';
    return od;
}


/* --------------------------- util_url_escape ---------------------------- */

NSAPI_PUBLIC char *util_url_escape(char *od, const char *s)
{
    int flagDbcsUri = allow_dbcs_uri();
    char *d;

    if (!od)
        od = (char *) MALLOC((strlen(s)*3) + 1);
    d = od;

    while (*s) {
        if (strchr("% +*\"'<>\r\n", *s)) {
            util_sprintf(d, "%%%02x", (unsigned char)*s);
            ++s; d += 3;
        }
#ifdef XP_WIN32
        else if (flagDbcsUri && s[1] && IsDBCSLeadByte(s[0]))
#else
        // Treat any character with the high bit set as a DBCS lead byte
        else if (flagDbcsUri && s[1] && (s[0] & 0x80))
#endif
	{
            // Escape the second byte of DBCS characters.  The first byte will
            // have been escaped already.  IE translates all unescaped '\\'s
            // into '/'.
            // Bug 353999
            util_sprintf(d, "%%%02x%%%02x", (unsigned char)s[0], (unsigned char)s[1]);
            s += 2; d += 6;
        }
        else if (0x80 & *s) {
            util_sprintf(d, "%%%02x", (unsigned char)*s);
            ++s; d += 3;
        } else {
            *d++ = *s++;
        }
    }
    *d = '\0';
    return od;
}


/* ------------------------- util_uri_strip_params ------------------------- */

NSAPI_PUBLIC char* util_uri_strip_params(char *uri)
{
    // As per RFC2396, URI path segments can contain parameters beginning with
    // ';'.  These parameters must be removed from the ppath.  Bug 418271
    char* out;
    if (out = strchr(uri, ';')) {
        char* in = out;
        while (*in) {
            if (*in == ';') {
                // Skip past parameter
                do in++; while (*in && *in != '/');
            } else {
                // Copy non-parameter path data
                *out++ = *in++;
            }
        }
        *out = 0;
    }
    return uri;
}


/* ------------------------ util_canonicalize_uri ------------------------- */

/*
 * rewrite rules:
 *   //                       ->  '/'
 *   /./                      ->  '/'
 *   /.\0                     ->  '/'
 *   /foo/../                 ->  '/'
 *   /foo/..\0                ->  '/'
 *
 * Allocate a new string, as otherwise replacing in-line would impact the
 * RequestURI, i.e. original URI in the request.
 * Some guidelines in: http://www.ietf.org/rfc/rfc2396.txt 
 *      Uniform Resource Identifiers (URI): Generic Syntax
 */
NSAPI_PUBLIC char* util_canonicalize_uri(pool_handle_t *pool, const char *uri, int len, int *pcanonlen)
{
    PRBool success = PR_TRUE;
    const char *in_ptr = uri;
    int in = 0;
    int in_len = len;

    PR_ASSERT(uri != NULL); 

    char* canonPath = (char *)pool_malloc(pool, in_len+1);
    char* out_ptr = canonPath;

    if (!canonPath) {
        success = PR_FALSE;
        goto done;
    }


    /* in goes from 0 .. sURIPath.len-1; out_ptr points to
     * space where next char from input would be copied to
     */
    while (in < in_len) {

        /* If the character isn't '/' then copy it out and move on*/
        if (in_ptr[0] != '/') {
            *out_ptr++ = *in_ptr++;
            in++; 
            continue;
        }

        /* found '/' and reached end of sURIPath, done */
        if (in+1 >= in_len) {
            *out_ptr++ = *in_ptr++;
            in++; 
            break;
        }

        /* we have '/' and there are more chars in the string */
        switch(in_ptr[1]) {
        case '/':
            /*  '//' => '/'  */
            in_ptr++;
            in++;
            break;

        case '.':
            /* we have "/." so far */
            if (in+2 >= in_len) {
                /*  the string ends after this; basically ignore '.' 
                 *  make sure the ending / is transferred to output.
                 */
                *out_ptr++ = *in_ptr++; 
                goto done;
            } 

            /* more chars after "/."; see if it is a '/' */
            if (in_ptr[2] == '/') {
                /* in deed, compact "/./" => "/"; */
                in_ptr += 2;
                in += 2;
                break;
            } 
            
            if (in_ptr[2] != '.') {
                /* "/.x" where x is not '.'; copy as is */
                *out_ptr++ = *in_ptr++; 
                in++;
                break;
            } 

            /* we have "/.." so far. see if we have either string
             * ending after this or '/' following.
             */
            if (in+3 < in_len && in_ptr[3] != '/' && in_ptr[3] != ';') {
                /* we have "/..x" here; so copy as is */
                *out_ptr++ = *in_ptr++; 
                in++;
            }
            else {
                /* we have "foo/../" or "foo/.." at the end; */
                if (out_ptr == canonPath) {
                    /* oops, we found "/../" pointing out of docroot */
                    success = PR_FALSE;
                    goto done;
                }

                /* remove the previous segment in the output */
                for (out_ptr--; 
                     out_ptr != canonPath && out_ptr[0] != '/'; 
                     out_ptr--); /* Empty Loop */

                /* point to '/' if the last segment ended with .. then
                 * leave the '/' before the previous segment.
                 */
                if(in+3 == in_len)
                    out_ptr++;

                /* skip the input as well */
                in_ptr += 3;
                in += 3;
            } 
            break;

        default:
            /* If we already have '/' at out_ptr we donot need to copy */
            if (out_ptr == canonPath || *(out_ptr-1) != '/')
                *out_ptr++ = *in_ptr; 
            in_ptr++; in++;
            break;
        }
    }

done:
    int canonLen = 0;

    if (success) {
        /* the path looks fine; return the canonicalized form */
        canonLen = out_ptr - canonPath;
        canonPath[canonLen] = '\0';
    } else {
        /* error canonicalizing */
        pool_free(pool, canonPath);
        canonPath = NULL;
    }

    if (pcanonlen)
        *pcanonlen = canonLen;

    return canonPath;
}


/* ---------------------- util_canonicalize_redirect ---------------------- */

NSAPI_PUBLIC char* util_canonicalize_redirect(pool_handle_t *pool, const char *baseUri, const char *newUri)
{
    PR_ASSERT(baseUri != NULL);

    if (*newUri == '/')
        return util_canonicalize_uri(pool, newUri, strlen(newUri), NULL);

    int bLen = strlen(baseUri);
    if (bLen > 0 && baseUri[bLen - 1] != '/') {
        while (bLen > 0 && baseUri[bLen - 1] != '/')
            bLen--;
    }

    int pLen = strlen(newUri) + bLen + 1; // 1 for slash
    char *pUri = (char *)pool_malloc(pool, pLen + 1);
    if (!pUri)
        return PR_FALSE;

    memcpy(pUri, baseUri, bLen);
    pUri[bLen] = '/';
    strcpy(pUri + bLen + 1, newUri);

    char *rval = util_canonicalize_uri(pool, pUri, pLen, NULL);
    pool_free(pool, pUri);

    return rval;
}


/* ------------------------ util_host_port_suffix ------------------------- */

NSAPI_PUBLIC char *util_host_port_suffix(char *h)
{
    return (char *)util_host_port_suffix((const char *)h);
}

const char *util_host_port_suffix(const char *h)
{
    /* Return a pointer to the colon preceding the port number in a hostname.
     *
     * util_host_port_suffix("foo.com:80") = ":80"
     * util_host_port_suffix("foo.com") = NULL
     * util_host_port_suffix("[::]:80") = ":80"
     * util_host_port_suffix("[::]") = NULL
     */

    if (h == NULL)
        return h;

    for (;;) {
        /* Find end of host, beginning of ":port", or an IPv6 address */
        for (;;) {
            register char c = *h;

            if (c == '\0')
                return NULL; /* end of host, no port found */

            if (c == '/')
                return NULL; /* end of host, no port found */

            if (c == ':')
                return h; /* found port */

            if (c == '[')
                break; /* skip IPv6 address */

            h++;
        }

        /* Skip IPv6 address */
        while (*h != '\0' && *h != ']')
            h++;
    }
}
