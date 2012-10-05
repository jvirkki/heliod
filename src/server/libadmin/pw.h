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

#ifndef _SLAPD_PW_H_
#define _SLAPD_PW_H_


#define CRYPT_SCHEME_NAME	"crypt"
#define CRYPT_NAME_LEN		5
#define SHA1_SCHEME_NAME	"SHA"
#define SHA1_NAME_LEN		3
#define NS_MTA_MD5_SCHEME_NAME	"NS-MTA-MD5"
#define NS_MTA_MD5_NAME_LEN	10

#define PWD_MAX_NAME_LEN	NS_MTA_MD5_NAME_LEN

#define PWD_HASH_PREFIX_START	'{'
#define PWD_HASH_PREFIX_END	'}'


/* structure for password policies */
struct pw_policy {
	long maxage; 	/* in seconds */
	int never_exp;		/* 0 = no, 1=yes */
	int min_length;
	int no_pw_history;	/* 0 = no, 1=yes */
	int num_pw_in_history;	
	int must_chg;		/* 0 = no, 1=yes*/
	int allow_chg;		/* 0 = no, 1=yes */
	long warning_duration; 	/* duration in seconds */
	int no_lockout;		/* 0 = no, 1=yes */
	int max_retry;
	long rst_retry_cnt_duration;/* duration in seconds */
	int lockout_forever; 	/* 0 = no, 1=yes */
	long lockout_duration; 	/* in seconds */
};

/* structure for holding password scheme info. */
struct pw_scheme {
	/* case-insensitive name used in prefix of passwords that use scheme */
	char	*pws_name;

	/* length of pws_name */
	int	pws_len;

	/* thread-safe comparison function; returns 0 for positive matches */
	/* userpwd is value sent over LDAP bind; dbpwd is from the database */
	int	(*pws_cmp)( char *userpwd, char *dbpwd );

	/* thread-safe encoding function (returns pointer to malloc'd string) */
	char	*(*pws_enc)( char *pwd );
};


struct pw_scheme	*pw_name2scheme( char *name );
struct pw_scheme	*pw_val2scheme( char *val, char **valpwdp,
	int first_is_default );
struct pw_scheme *g_get_pwdhashscheme();
void g_set_pwdhashscheme(struct pw_scheme* val);


#if !defined(NET_SSL)
/******************************************/
/*
 * Some of the stuff below depends on a definition for uint32, so
 * we include one here.  Other definitions appear in nspr/prtypes.h,
 * at least.  All the platforms we support use 32-bit ints.
 */
typedef unsigned int uint32;


/******************************************/
/*
 * The following is from ds.h, which the libsec sec.h stuff depends on (see
 * comment below).
 */
/*
** A status code. Status's are used by procedures that return status
** values. Again the motivation is so that a compiler can generate
** warnings when return values are wrong. Correct testing of status codes:
**
**      DSStatus rv;
**      rv = some_function (some_argument);
**      if (rv != DSSuccess)
**              do_an_error_thing();
**
*/
typedef enum DSStatusEnum {
    DSWouldBlock = -2,
    DSFailure = -1,
    DSSuccess = 0
} DSStatus;


/******************************************/
/*
 * All of the SHA1-related defines are from libsec's "sec.h" -- including
 * it directly pulls in way too much stuff that we conflict with.  Ugh.
 */

/*
 * Number of bytes each hash algorithm produces
 */
#define SHA1_LENGTH		20

/******************************************/
/*
** SHA-1 secure hash function
*/
 
/*
** Hash a null terminated string "src" into "dest" using SHA-1
*/
extern DSStatus SHA1_Hash(unsigned char *dest, char *src);

#endif /* !defined(NET_SSL) */


/*
 * MD5 algorithm used by Netscape Mail Server
 */

/* MD5 code taken from reference implementation published in RFC 1321 */

#ifndef _RFC1321_MD5_H_
#define _RFC1321_MD5_H_

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD5 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD5 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef unsigned char      * POINTER;
typedef unsigned short int   UINT2;
typedef unsigned long  int   UINT4;

/* MD5 context. */
typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} mta_MD5_CTX;

void mta_MD5Init   (mta_MD5_CTX *);
void mta_MD5Update (mta_MD5_CTX *, const unsigned char *, unsigned int);
void mta_MD5Final  (unsigned char [16], mta_MD5_CTX *);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* end of _RFC1321_MD5_H_ */

#endif /* _SLAPD_PW_H_ */
