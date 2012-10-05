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
 * password.c: Interfaces with the UNIX DES password encryption libs
 * 
 * Rob McCool
 *
 * sha1 routines picked up from LDAP server tree. Originally written by mcs.  _atulb */

#include "libadmin/libadmin.h"

#ifdef ENCRYPT_PASSWORDS

#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "httpcode.h"
#ifdef CRYPT_H
#include <crypt.h>
#else
extern "C" char *crypt(const char *key, const char *salt);
#endif

#ifdef NS_OLDES3X
#include "sec.h"
#else
#include "sechash.h"
#define DSSuccess SECSuccess
#endif /* NS_OLDES3X */

#ifdef _WIN32
#include <windows.h>
#elif !defined( macintosh )
#include <sys/socket.h>
#endif
#include "pw.h"

#define RIGHT2                  0x03
#define RIGHT4                  0x0f
#define LDAP_DEBUG_ANY          0x4000          /* from ldaplog.h */
#define LDIF_BASE64_LEN(vlen)   (((vlen) * 4 / 3 ) + 3) /* from ldif.h */
#define SHA1_LENGTH		20

static unsigned char b642nib[0x80] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
        0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};



/* line64.c - routines for dealing with the slapd line format */

/*
 * ldif_base64_decode - take the BASE64-encoded characters in "src"
 * (a zero-terminated string) and decode them into the the buffer "dst".
 * "src" and "dst" can be the same if in-place decoding is desired.
 * "dst" must be large enough to hold the decoded octets.  No more than
 *      3 * strlen( src ) / 4 bytes will be produced.
 * "dst" may contain zero octets anywhere within it, but it is not
 *      zero-terminated by this function.
 *
 * The number of bytes copied to "dst" is returned if all goes well.
 * -1 is returned if the BASE64 encoding in "src" is invalid.
 */

static int
ldif_base64_decode( char *src, unsigned char *dst )
{
        char            *p, *stop;
        unsigned char   nib, *byte;
        int             i, len;

        stop = strchr( src, '\0' );
        byte = dst;
        for ( p = src, len = 0; p < stop; p += 4, len += 3 ) {
                for ( i = 0; i < 3; i++ ) {
                        if ( p[i] != '=' && (p[i] & 0x80 ||
                            b642nib[ p[i] & 0x7f ] > 0x3f) ) {
                                return( -1 );
                        }
                }

                /* first digit */
                nib = b642nib[ p[0] & 0x7f ];
                byte[0] = nib << 2;

                /* second digit */
                nib = b642nib[ p[1] & 0x7f ];
                byte[0] |= nib >> 4;

                /* third digit */
                if ( p[2] == '=' ) {
                        len += 1;
                        break;
                }
                byte[1] = (nib & RIGHT4) << 4;
                nib = b642nib[ p[2] & 0x7f ];
                byte[1] |= nib >> 2;

                /* fourth digit */
                if ( p[3] == '=' ) {
                        len += 2;
                        break;
                }
                byte[2] = (nib & RIGHT2) << 6;
                nib = b642nib[ p[3] & 0x7f ];
                byte[2] |= nib;

                byte += 3;
        }

        return( len );
}

NSAPI_PUBLIC int
https_sha1_pw_cmp( char *userpwd, char *dbpwd )
{
/*
 * SHA1 passwords are stored in the database as SHA1_LENGTH bytes of
 * BASE64 encoded data.
 */
        unsigned char   userhash[ SHA1_LENGTH ], dbhash[ SHA1_LENGTH ];

        if ( strlen( dbpwd ) > LDIF_BASE64_LEN( SHA1_LENGTH )) {
/*              LDAPDebug( LDAP_DEBUG_ANY, hasherrmsg, SHA1_SCHEME_NAME, dbpwd,
0 );
*/
                return( 1 );    /* failure */
        }


        /* SHA1 hash the user's key */
        if ( https_SHA1_Hash( userhash, userpwd ) != DSSuccess ) {
/*
                LDAPDebug( LDAP_DEBUG_ANY, "sha1_pw_cmp: SHA1_Hash() failed\n",
                    0, 0, 0 );
*/
                return( 1 );    /* failure */
        }

        /* decode hash stored in database */
        if ( ldif_base64_decode( dbpwd, dbhash ) != SHA1_LENGTH ) {
/*
                LDAPDebug( LDAP_DEBUG_ANY, hasherrmsg, "SHA1", dbpwd, 0 );
*/
                return( 1 );    /* failure */
        }

        /* the proof is in the comparison... */
        return( memcmp( userhash, dbhash, SHA1_LENGTH ));
}

#else

NSAPI_PUBLIC char *pw_enc(char *pw)
{
    return pw;
}

NSAPI_PUBLIC int pw_cmp(char *pw, char *enc)
{
    return strcmp(pw, enc);
}

#endif
