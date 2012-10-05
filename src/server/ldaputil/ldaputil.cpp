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

#include <ldap.h>
#include <stdlib.h>
#include <string.h>
#include <plstr.h>
#include <prlink.h>
#include "base/file.h"
#include <ldaputil/ldaputil.h>
#include "ldaputil/certmap.h"
#include "ldaputil/errors.h"
#include <ldaputil/list.h>
#include <ldaputili.h>

#ifdef XP_WIN32
#define DLL_SUFFIX ".dll"
#ifndef FILE_PATHSEP
#define FILE_PATHSEP '\\'
#endif
#else
#ifndef FILE_PATHSEP
#define FILE_PATHSEP '/'
#endif
#ifdef HPUX
#define DLL_SUFFIX ".sl"
#else
#define DLL_SUFFIX ".so"
#endif
#endif

static int load_server_libs (const char *dir)
{
    int rv = LDAPU_SUCCESS;
    SYS_DIR ds;
    int suffix_len = strlen(DLL_SUFFIX);

    if ((ds = dir_open((char *)dir)) != NULL) {
	SYS_DIRENT *d;

	/* Dir exists */
        while( (d = dir_read(ds)) )  {
	    PRLibrary *lib = 0;
            char *libname = d->d_name;
	    int len = strlen(libname);
	    int is_lib;

	    is_lib = (len > suffix_len && !strcmp(libname+len-suffix_len, DLL_SUFFIX));
            if (is_lib) {
		char path[1024];

		sprintf(path, "%s%c%s", dir, FILE_PATHSEP, libname);
		lib = PR_LoadLibrary(path);
		if (!lib) rv = LDAPU_ERR_UNABLE_TO_LOAD_PLUGIN;
	    }
	}
    }
    else {
	/* It's ok if dir doesn't exists */
    }
    
    return rv;
}

NSAPI_PUBLIC int ldaputil_init (const char *config_file,
				const char *dllname,
				const char *serv_root,
				const char *serv_type,
				const char *serv_id)
{
    int rv = LDAPU_SUCCESS;
    static int initialized = 0;

    /* If already initialized, cleanup the old structures */
    if (initialized) ldaputil_exit();

    if (config_file && *config_file) {
	char dir[1024];

	LDAPUCertMapListInfo_t *certmap_list;
	LDAPUCertMapInfo_t *certmap_default;

	if (serv_root && *serv_root) {
	    /* Load common libraries */
	    sprintf(dir, "%s%clib%c%s", serv_root, FILE_PATHSEP,
		    FILE_PATHSEP, "common");
	    rv = load_server_libs(dir);

	    if (rv != LDAPU_SUCCESS) return rv;

	    if (serv_type && *serv_type) {
		/* Load server type specific libraries */
		sprintf(dir, "%s%clib%c%s", serv_root, FILE_PATHSEP,
			FILE_PATHSEP, serv_type);
		rv = load_server_libs(dir);

		if (rv != LDAPU_SUCCESS) return rv;

		if (serv_id && *serv_id) {
		    /* Load server instance specific libraries */
		    sprintf(dir, "%s%clib%c%s", serv_root, FILE_PATHSEP,
			    FILE_PATHSEP, serv_id);
		    rv = load_server_libs(dir);

		    if (rv != LDAPU_SUCCESS) return rv;
		}
	    }
	}

	rv = ldapu_certmap_init (config_file, dllname, &certmap_list,
				 &certmap_default);
    }

    initialized = 1;

    if (rv != LDAPU_SUCCESS) return rv;

    return rv;
}


NSAPI_PUBLIC int ldaputil_exit ()
{
    ldapu_certmap_exit();

    return LDAPU_SUCCESS;
}


static LDAPUDispatchVector_t __ldapu_vector = {
    ldapu_cert_to_ldap_entry,
    ldapu_set_cert_mapfn,
    ldapu_get_cert_mapfn,
    ldapu_set_cert_searchfn,
    ldapu_get_cert_searchfn,
    ldapu_set_cert_verifyfn,
    ldapu_get_cert_verifyfn,
    ldapu_get_cert_subject_dn,
    ldapu_get_cert_issuer_dn,
    ldapu_get_cert_ava_val,
    ldapu_free_cert_ava_val,
    ldapu_get_cert_der,
    ldapu_issuer_certinfo,
    ldapu_certmap_info_attrval,
    ldapu_err2string,
    ldapu_free_old,
    ldapu_malloc,
    ldapu_strdup,
    ldapu_free
};

#ifdef XP_UNIX
LDAPUDispatchVector_t *__ldapu_table = &__ldapu_vector;
#endif

#if 0
NSAPI_PUBLIC int CertMapDLLInitFn(LDAPUDispatchVector_t **table)
{
    *table = &__ldapu_vector;
}
#endif

NSAPI_PUBLIC int CertMapDLLInitFn(LDAPUDispatchVector_t **table)
{
    *table = (LDAPUDispatchVector_t *)malloc(sizeof(LDAPUDispatchVector_t));

    if (!*table) return LDAPU_ERR_OUT_OF_MEMORY;

    (*table)->f_ldapu_cert_to_ldap_entry = ldapu_cert_to_ldap_entry;
    (*table)->f_ldapu_set_cert_mapfn = ldapu_set_cert_mapfn;
    (*table)->f_ldapu_get_cert_mapfn = ldapu_get_cert_mapfn;
    (*table)->f_ldapu_set_cert_searchfn = ldapu_set_cert_searchfn;
    (*table)->f_ldapu_get_cert_searchfn = ldapu_get_cert_searchfn;
    (*table)->f_ldapu_set_cert_verifyfn = ldapu_set_cert_verifyfn;
    (*table)->f_ldapu_get_cert_verifyfn = ldapu_get_cert_verifyfn;
    (*table)->f_ldapu_get_cert_subject_dn = ldapu_get_cert_subject_dn;
    (*table)->f_ldapu_get_cert_issuer_dn = ldapu_get_cert_issuer_dn;
    (*table)->f_ldapu_get_cert_ava_val = ldapu_get_cert_ava_val;
    (*table)->f_ldapu_free_cert_ava_val = ldapu_free_cert_ava_val;
    (*table)->f_ldapu_get_cert_der = ldapu_get_cert_der;
    (*table)->f_ldapu_issuer_certinfo = ldapu_issuer_certinfo;
    (*table)->f_ldapu_certmap_info_attrval = ldapu_certmap_info_attrval;
    (*table)->f_ldapu_err2string = ldapu_err2string;
    (*table)->f_ldapu_free_old = ldapu_free_old;
    (*table)->f_ldapu_malloc = ldapu_malloc;
    (*table)->f_ldapu_strdup = ldapu_strdup;
    (*table)->f_ldapu_free = ldapu_free;
    return LDAPU_SUCCESS;
}

//
// this stuff is obsolete - the only reason why we still have it
// is that we have not adapted the code in cert*.cpp to use the routines
// in LdapSession.cpp
//

/*
 * ldapu_find
 *   Description:
 *	Caller should free res if it is not NULL.
 *   Arguments:
 *	ld		Pointer to LDAP (assumes connection has been
 *	    		established and the client has called the
 *	    		appropriate bind routine)
 *	base		basedn (where to start the search)
 *	scope		scope for the search.  One of
 *	    		LDAP_SCOPE_SUBTREE, LDAP_SCOPE_ONELEVEL, and
 *	    		LDAP_SCOPE_BASE
 *	filter		LDAP filter
 *	attrs		A NULL-terminated array of strings indicating which
 *	    		attributes to return for each matching entry.  Passing
 *	    		NULL for this parameter causes all available
 *	    		attributes to be retrieved.
 *	attrsonly	A boolean value that should be zero if both attribute
 *	    		types and values are to be returned, non-zero if only
 *	    		types are wanted.
 *	res		A result parameter which will contain the results of
 *			the search upon completion of the call.
 *   Return Values:
 *	LDAPU_SUCCESS	if entry is found
 *	LDAPU_FAILED	if entry is not found
 *	<rv>		if error, where <rv> can be passed to
 *			ldap_err2string to get an error string.
 */
int ldapu_find (LDAP *ld, const char *base, int scope,
		const char *filter, const char **attrs,
		int attrsonly, LDAPMessage **res)
{
    int retval;
#ifdef USE_THIS_CODE /* ASYNCHRONOUS */
    int msgid;
#endif
    int numEntries;

    *res = 0;

    /* If base is NULL set it to null string */
    if (!base) {
	DBG_PRINT1("ldapu_find: basedn is missing -- assuming null string\n");
	base = "";
    }

    if (!filter || !*filter) {
	DBG_PRINT1("ldapu_find: filter is missing -- assuming objectclass=*\n");
	filter = "objectclass=*";
    }
    
    DBG_PRINT2("\tbase:\t\"%s\"\n", base);
    DBG_PRINT2("\tfilter:\t\"%s\"\n", filter ? filter : "<NULL>");
    DBG_PRINT2("\tscope:\t\"%s\"\n",
	       (scope == LDAP_SCOPE_SUBTREE ? "LDAP_SCOPE_SUBTREE"
		: (scope == LDAP_SCOPE_ONELEVEL ? "LDAP_SCOPE_ONELEVEL"
		   : "LDAP_SCOPE_BASE")));

    retval = ldapu_search_s(ld, base, scope, filter, (char **)attrs,
			   attrsonly, res);

    if (retval != LDAP_SUCCESS)
    {
	/* retval = ldap_result2error(ld, *res, 0); */
	DBG_PRINT2("ldapu_search_s: %s\n", ldapu_err2string(retval));
	return(retval);
    }

    numEntries = ldapu_count_entries(ld, *res);

    if (numEntries == 1) {
	/* success */
	return LDAPU_SUCCESS;
    }
    else if (numEntries == 0) {
	/* not found -- but not an error */
	DBG_PRINT1("ldapu_search_s: Entry not found\n");
	return LDAPU_FAILED;
    }
    else if (numEntries > 0) {
	/* Found more than one entry! */
	DBG_PRINT1("ldapu_search_s: Found more than one entry\n");
	return LDAPU_ERR_MULTIPLE_MATCHES;
    }
    else {
	/* should never get here */
	DBG_PRINT1("ldapu_search_s: should never reach here\n");
	ldapu_msgfree(ld, *res);
	return LDAP_OPERATIONS_ERROR;
    }
}

/*
 * Support routines
 */

#define DNSEPARATOR(c)  (c == ',' || c == ';')
#define SEPARATOR(c)    (c == ',' || c == ';' || c == '+')
#define SPACE(c)    (c == ' ' || c == '\n')
#define NEEDSESCAPE(c)  (c == '\\' || c == '"')
#define B4TYPE      0
#define INTYPE      1
#define B4EQUAL     2
#define B4VALUE     3
#define INVALUE     4
#define INQUOTEDVALUE   5
#define B4SEPARATOR 6
 
#define DNNORM_INITIAL_RDN_AVS    10
#define DNNORM_SMALL_RDN_AV   512

static int
hexchar2int( char c )
{
    if ( '0' <= c && c <= '9' ) {
    return( c - '0' );
    }
    if ( 'a' <= c && c <= 'f' ) {
    return( c - 'a' + 10 );
    }
    if ( 'A' <= c && c <= 'F' ) {
    return( c - 'A' + 10 );
    }
    return( -1 );
}

/*
 * Append previous AV to the attribute value array if multivalued RDN.
 * We use a stack based array at first and if we overflow that, we
 * allocate a larger one from the heap, copy the stack based data in,
 * and continue to grow the heap based one as needed.
 */
static void
slapi_add_rdn_av( char *avstart, char *avend, int *rdn_av_countp,
    struct berval **rdn_avsp, struct berval *avstack )
{
    if ( *rdn_av_countp == 0 ) {
    *rdn_avsp = avstack;
    } else if ( *rdn_av_countp == DNNORM_INITIAL_RDN_AVS ) {
    struct berval   *tmpavs;
 
    tmpavs = (struct berval *)calloc(
        DNNORM_INITIAL_RDN_AVS * 2, sizeof( struct berval ));
    memcpy( tmpavs, *rdn_avsp,
        DNNORM_INITIAL_RDN_AVS * sizeof( struct berval ));
    *rdn_avsp = tmpavs;
    } else if (( *rdn_av_countp % DNNORM_INITIAL_RDN_AVS ) == 0 ) {
    *rdn_avsp = (struct berval *)realloc( (char *)*rdn_avsp,
        *rdn_av_countp + DNNORM_INITIAL_RDN_AVS );
    }
 
    /*
     * Note: The bv_val's are just pointers into the dn itself.  Also,
     * we DO NOT zero-terminate the bv_val's.  The sorting code in
     * slapi_sort_rdn_avs() takes all of this into account.
     */
    (*rdn_avsp)[ *rdn_av_countp ].bv_val = avstart;
    (*rdn_avsp)[ *rdn_av_countp ].bv_len = avend - avstart;
    ++(*rdn_av_countp);
}
 
 
/*
 * Reset RDN attribute value array, freeing memory if any was allocated.
 */
static void
slapi_reset_rdn_avs( struct berval **rdn_avsp, int *rdn_av_countp )
{
    if ( *rdn_av_countp > DNNORM_INITIAL_RDN_AVS ) {
        free( *rdn_avsp );
    }
    *rdn_avsp = NULL;
    *rdn_av_countp = 0;
}
 

/*
 * Swap two adjacent attribute=value pieces within an (R)DN.
 * Avoid allocating any heap memory for reasonably small AVs.
 */
static void
slapi_rdn_av_swap( struct berval *av1, struct berval *av2 )
{
    char    *buf1, *buf2;
    char    stackbuf1[ DNNORM_SMALL_RDN_AV ];
    char    stackbuf2[ DNNORM_SMALL_RDN_AV ];
    int     len1, len2;
 
    /*
     * Copy the two avs into temporary buffers.  We use stack-based buffers
     * if the avs are small and allocate buffers from the heap to hold
     * large values.
     */
    if (( len1 = av1->bv_len ) <= DNNORM_SMALL_RDN_AV ) {
        buf1 = stackbuf1;
    } else {
        buf1 = (char *)ldapu_malloc( len1 ); /* slapi_ch_malloc */
    }
    memcpy( buf1, av1->bv_val, len1 );
 
    if (( len2 = av2->bv_len ) <= DNNORM_SMALL_RDN_AV ) {
        buf2 = stackbuf2;
    } else {
        buf2 = (char *)ldapu_malloc( len2 );
    }
    memcpy( buf2, av2->bv_val, len2 );
 
    /*
     * Copy av2 over av1 and reset length of av1.
     */
    memcpy( av1->bv_val, buf2, av2->bv_len );
    av1->bv_len = len2;
 
    /*
     * Add separator character (+) and copy av1 into place.
     * Also reset av2 pointer and length.
     */
    av2->bv_val = av1->bv_val + len2;
    *(av2->bv_val)++ = '+';
    memcpy( av2->bv_val, buf1, len1 );
    av2->bv_len = len1;

    /*
     * Clean up.
     */
    if ( len1 > DNNORM_SMALL_RDN_AV ) {
        ldapu_free( buf1 ); /* slapi_ch_free */
    }
    if ( len2 > DNNORM_SMALL_RDN_AV ) {
        ldapu_free( buf2 );
    }
}
 
/*
 * strcasecmp()-like function for RDN attribute values.
 */
static int
slapi_rdn_av_cmp( struct berval *av1, struct berval *av2 )
{
    int     rc;
 
    rc = PL_strncasecmp( av1->bv_val, av2->bv_val,
        ( av1->bv_len < av2->bv_len ) ? av1->bv_len : av2->bv_len );
 
    if ( rc == 0 ) {
	return( av1->bv_len - av2->bv_len );    /* longer is greater */
    } else {
	return( rc );
    }
}
 
/*
 * Perform an in-place, case-insensitive sort of RDN attribute=value pieces.
 * This function is always called with more than one element in "avs".
 *
 * Note that this is used by the DN normalization code, so if any changes
 * are made to the comparison function used for sorting customers will need
 * to rebuild their database/index files.
 *
 * Also note that the bv_val's in the "avas" array are not zero-terminated.
 */
static void
slapi_sort_rdn_avs( struct berval *avs, int count )
{
    int     i, j, swaps;
 
    /*
     * Since we expect there to be a small number of AVs, we use a
     * simple bubble sort.  slapi_rdn_av_swap() only works correctly on
     * adjacent values anyway.
     */
    for ( i = 0; i < count - 1; ++i ) {
    swaps = 0;
    for ( j = 0; j < count - 1; ++j ) {
        if ( slapi_rdn_av_cmp( &avs[j], &avs[j+1] ) > 0 ) {
        slapi_rdn_av_swap( &avs[j], &avs[j+1] );
        ++swaps;
        }
    }
    if ( swaps == 0 ) {
        break;  /* stop early if no swaps made during the last pass */
    }
    }
}
 
 
/*
 * ldapdn_substr_normalize
 */
static char *
ldapdn_substr_normalize( char *dn, char *end, int *nRDNs )
{
    /* \xx is changed to \c.
     * \c is changed to c, unless this would change its meaning.
     * All values that contain 2 or more separators are "enquoted";
     * all other values are not enquoted.
     */
    char        *value, *value_separator;
    char        *d, *s, *typestart;
    int     gotesc = 0;
    int     state = B4TYPE;
    int     rdn_av_count = 0;
    struct berval   *rdn_avs = NULL;
    struct berval   initial_rdn_av_stack[ DNNORM_INITIAL_RDN_AVS ];
    int nrdn = 0;
 
    for ( d = s = dn; s != end; s++ ) {
        switch ( state ) {
        case B4TYPE:
            if ( ! SPACE( *s ) ) {
                state = INTYPE;
                typestart = d;
		nrdn++;
                *d++ = *s;
            }
            break;
        case INTYPE:
            if ( *s == '=' ) {
                state = B4VALUE;
                *d++ = *s;
            } else if ( SPACE( *s ) ) {
                state = B4EQUAL;
            } else {
                *d++ = *s;
            }
            break;
        case B4EQUAL:
            if ( *s == '=' ) {
                state = B4VALUE;
                *d++ = *s;
            } else if ( ! SPACE( *s ) ) {
                /* not a valid dn - but what can we do here? */
                *d++ = *s;
            }
            break;
        case B4VALUE:
            if ( *s == '"' || ! SPACE( *s ) ) {
                value_separator = NULL;
                value = d;
                state = ( *s == '"' ) ? INQUOTEDVALUE : INVALUE;
                *d++ = *s;
            }
            break;
        case INVALUE:
            if ( gotesc ) {
                if ( SEPARATOR( *s ) ) {
		    if ( value_separator ) value_separator = dn;
		    else value_separator = d;
                } else if ( ! NEEDSESCAPE( *s ) ) {
		    --d; /* eliminate the \ */
                }
            } else if ( SEPARATOR( *s ) ) {
                while ( SPACE( *(d - 1) ) )
		    d--;
                if ( value_separator == dn ) { /* 2 or more separators */
		    /* convert to quoted value: */
		    auto char *L = NULL;
		    auto char *R;
		    for ( R = value; (R = strchr( R, '\\' )) && (R < d); L = ++R ) {
			if ( SEPARATOR( R[1] )) {
			    if ( L == NULL ) {
				auto const size_t len = R - value;
				if ( len > 0 ) memmove( value+1, value, len );
				*value = '"'; /* opening quote */
				value = R + 1;
			    } else {
				auto const size_t len = R - L;
				if ( len > 0 ) {
				memmove( value, L, len );
				value += len;
				}
				--d;
			    }
			}
		    }
		    memmove( value, L, d - L + 1 );
		    *d++ = '"'; /* closing quote */
                }
                state = B4TYPE;
 
                /*
                 * Track and sort attribute values within
                 * multivalued RDNs.
                 */
                if ( *s == '+' || rdn_av_count > 0 ) {
		    slapi_add_rdn_av( typestart, d, &rdn_av_count,
			&rdn_avs, initial_rdn_av_stack );
                }
                if ( *s != '+' ) {  /* at end of this RDN */
		    if ( rdn_av_count > 1 ) {
			slapi_sort_rdn_avs( rdn_avs, rdn_av_count );
		    }
		    if ( rdn_av_count > 0 ) {
			slapi_reset_rdn_avs( &rdn_avs, &rdn_av_count );
		    }
                }
 
                *d++ = (*s == '+') ? '+' : ',';
                break;
            }
            *d++ = *s;
            break;
        case INQUOTEDVALUE:
            if ( gotesc ) {
                if ( ! NEEDSESCAPE( *s ) ) {
                --d; /* eliminate the \ */
                }
            } else if ( *s == '"' ) {
                state = B4SEPARATOR;
                if ( value_separator == dn /* 2 or more separators */
                || SPACE( value[1] ) || SPACE( d[-1] ) ) {
                *d++ = *s;
                } else {
                /* convert to non-quoted value: */
                if ( value_separator == NULL ) { /* no separators */
                    memmove ( value, value+1, (d-value)-1 );
                    --d;
                } else { /* 1 separator */
                    memmove ( value, value+1, (value_separator-value)-1 );
                    *(value_separator - 1) = '\\';
                }
                }
                break;
            }
            if ( SEPARATOR( *s )) {
                if ( value_separator ) value_separator = dn;
                else value_separator = d;
            }
            *d++ = *s;
            break;
        case B4SEPARATOR:
            if ( SEPARATOR( *s ) ) {
                state = B4TYPE;
 
                /*
                 * Track and sort attribute values within
                 * multivalued RDNs.
                 */
                if ( *s == '+' || rdn_av_count > 0 ) {
                slapi_add_rdn_av( typestart, d, &rdn_av_count,
                    &rdn_avs, initial_rdn_av_stack );
                }
                if ( *s != '+' ) {  /* at end of this RDN */
                if ( rdn_av_count > 1 ) {
                    slapi_sort_rdn_avs( rdn_avs, rdn_av_count );
                }
                if ( rdn_av_count > 0 ) {
                    slapi_reset_rdn_avs( &rdn_avs, &rdn_av_count );
                }
                }
 
                *d++ = (*s == '+') ? '+' : ',';
            }
            break;
        default:
	    // unknown state
            break;
        }
        if ( *s != '\\' ) {
            gotesc = 0;
        } else {
            gotesc = 1;
            if ( s+2 < end ) {
                auto int n = hexchar2int( s[1] );
                if ( n >= 0 ) {
		    auto int n2 = hexchar2int( s[2] );
		    if ( n2 >= 0 ) {
			n = (n << 4) + n2;
			if (n == 0) { /* don't change \00 */
			*d++ = *++s;
			*d++ = *++s;
			gotesc = 0;
			} else { /* change \xx to a single char */
			++s;
			*(unsigned char*)(s+1) = n;
			}
		    }
                }
            }
        }
    }
 
    /*
     * Track and sort attribute values within multivalued RDNs.
     */
    if ( rdn_av_count > 0 ) {
        slapi_add_rdn_av( typestart, d, &rdn_av_count,
            &rdn_avs, initial_rdn_av_stack );
    }
    if ( rdn_av_count > 1 ) {
        slapi_sort_rdn_avs( rdn_avs, rdn_av_count );
    }
    if ( rdn_av_count > 0 ) {
        slapi_reset_rdn_avs( &rdn_avs, &rdn_av_count );
    }
 
    /* Trim trailing spaces */
    while ( d != dn && *(d - 1) == ' ' ) d--;
 
    *nRDNs = nrdn;

    return( d );
}

/*
 * ldapdn_normalize
 */
int
ldapdn_normalize( char *dn )
{
    int nRDNs;

    *(ldapdn_substr_normalize( dn, dn + strlen( dn ), &nRDNs )) = '\0';

    return nRDNs;
}
 
/*
 *  ldapdn_issuffix:
 *	Description:
 *	    find out if suffix is a suffix of userdn (which means that userdn is in suffix)
 *	Arguments:
 *	    dn			a DN
 *	    suffix		a DN suffix, typically a baseDN
 *	Return Values:
 *	    -1			if suffix is NOT a suffix of dn
 *	    >= 0		suffix is a suffix of dn, and it starts at this position in dn
 *  thanx to Terry Hayes for this one -- chrisk 050599
 */
int
ldapdn_issuffix(const char *dn, const char *suffix)
{
    int dnlen, suffixlen;
 
    if (dn == NULL || suffix == NULL)
        return(-1);
 
    suffixlen = strlen(suffix);
    dnlen = strlen(dn);
 
    if (suffixlen > dnlen) {
	return(-1);
    }
 
    // if it's a suffix, return the index where it begins in dn
    return (PL_strcasecmp( dn + dnlen - suffixlen, suffix ) == 0) ? dnlen - suffixlen : -1;
}


// =============================================================================
// LDAPU lists
// =============================================================================

NSAPI_PUBLIC int
ldapu_list_add_node (LDAPUList_t *list, LDAPUListNode_t *node)
{
    if (list->head) {
	node->prev = list->tail;
	list->tail->next = node;
    }
    else {
	node->prev = 0;
	list->head = node;
    }

    node->next = 0;
    list->tail = node;
    return LDAPU_SUCCESS;
}

NSAPI_PUBLIC int
ldapu_list_remove_node (LDAPUList_t *list, LDAPUListNode_t *node)
{
    if (list->head == node) {
	list->head = node->next;
	if (list->tail == node) list->tail = 0;	/* removed the only node */
    }
    else if (list->tail == node) {
	list->tail = node->prev;
    }
    else {
	node->prev->next = node->next;
	node->next->prev = node->prev;
    }

    node->next = 0;
    node->prev = 0;
    return LDAPU_SUCCESS;
}

NSAPI_PUBLIC int
ldapu_list_alloc (LDAPUList_t **list)
{
    *list = (LDAPUList_t *)malloc(sizeof(LDAPUList_t));

    if (!*list) return LDAPU_ERR_OUT_OF_MEMORY;

    memset((void *)*list, 0, sizeof(LDAPUList_t));
    return LDAPU_SUCCESS;
}

NSAPI_PUBLIC void
ldapu_list_free (LDAPUList_t *list, LDAPUListNodeFn_t free_fn)
{
    if (list && ! ldapu_list_empty (list, free_fn, 0)) {
	free(list);
    }
}

NSAPI_PUBLIC int
ldapu_list_add_info (LDAPUList_t *list, void *info)
{
    LDAPUListNode_t *node;

    /* Allocate the list node and set info in the node. */
    node = (LDAPUListNode_t *)malloc(sizeof(LDAPUListNode_t));

    if (!node) {
	return LDAPU_ERR_OUT_OF_MEMORY;
    }

    memset((void *)node, 0, sizeof(LDAPUListNode_t));
    node->info = info;

    return ldapu_list_add_node(list, node);
}

NSAPI_PUBLIC void
ldapu_list_move (LDAPUList_t* from, LDAPUList_t* into)
{
    if (from && from->head) {
	if (into->head) {
	    from->head->prev = into->tail;
	    into->tail->next = from->head;
	} else {
	    into->head = from->head;
	}
	into->tail = from->tail;
	from->tail = 0;
	from->head = 0;
    }
}

NSAPI_PUBLIC int
ldapu_list_copy (const LDAPUList_t *from, LDAPUList_t **to,
			    LDAPUListNodeFn_t copy_fn)
{
    LDAPUListNode_t *node = from->head;
    LDAPUListNode_t *newnode;
    LDAPUList_t *list;
    int rv;

    *to = 0;
    rv = ldapu_list_alloc(&list);
    if (rv != LDAPU_SUCCESS) return rv;

    while(node) {
	newnode = (LDAPUListNode_t *)(*copy_fn)(node->info, 0);
	if (!newnode) return LDAPU_ERR_OUT_OF_MEMORY;
	rv = ldapu_list_add_info(list, newnode);
	if (rv != LDAPU_SUCCESS) return rv;
	node = node->next;
    }

    *to = list;
    return LDAPU_SUCCESS;
}

NSAPI_PUBLIC int
ldapu_list_find_node (const LDAPUList_t *list,
				 LDAPUListNode_t **found,
				 LDAPUListNodeFn_t find_fn,
				 void *find_arg)
{
    LDAPUListNode_t *node = list->head;

    while(node) {
	if ((*find_fn)(node->info, find_arg) == LDAPU_SUCCESS) {
	    *found = node;
	    return LDAPU_SUCCESS;
	}
	node = node->next;
    }

    return LDAPU_ERR_CERTMAP_INFO_MISSING;
}

NSAPI_PUBLIC int
ldapu_list_print (LDAPUList_t *list, LDAPUListNodeFn_t print_fn,
			     LDAPUPrintInfo_t *pinfo)
{
    LDAPUListNode_t *node = list->head;
    int rv;

    while(node) {
	rv = (int)(size_t)(*print_fn)(node->info, pinfo);
	if (rv != LDAPU_SUCCESS) return rv;
	node = node->next;
    }

    return LDAPU_SUCCESS;
}


NSAPI_PUBLIC void *
ldapu_list_empty (LDAPUList_t *list, LDAPUListNodeFn_t free_fn, void *arg)
{
    if (list) {
	auto LDAPUListNode_t *node = list->head;
	while (node) {
	    auto LDAPUListNode_t *next = node->next;
	    if (free_fn) {
		auto void *rv = (*free_fn)(node->info, arg);
		if (rv) {
		    list->head = node;
		    node->prev = 0;
		    return rv;
		}
	    }
	    node->info = 0;
	    free(node);
	    node = next;
	}
	list->head = 0;
	list->tail = 0;
    }
    return 0;
}


// =============================================================================
// LDAPU strings
// =============================================================================

LDAPUStr_t *ldapu_str_alloc (const int size)
{
    LDAPUStr_t *lstr = (LDAPUStr_t *)ldapu_malloc(sizeof(LDAPUStr_t));

    if (!lstr) return 0;
    lstr->size = size < 0 ? 1024 : size;
    lstr->str = (char *)ldapu_malloc(lstr->size*sizeof(char));
    lstr->len = 0;
    lstr->str[lstr->len] = 0;

    return lstr;
}


void ldapu_str_free (LDAPUStr_t *lstr)
{
    if (lstr) {
	if (lstr->str) ldapu_free(lstr->str);
	ldapu_free((void *)lstr);
    }
}

int ldapu_str_append(LDAPUStr_t *lstr, const char *arg)
{
    int arglen = strlen(arg);
    int len = lstr->len + arglen;

    if (len >= lstr->size) {
	/* realloc some more */
	lstr->size += arglen > 4095 ? arglen+1 : 4096;
	lstr->str = (char *)ldapu_realloc(lstr->str, lstr->size);
	if (!lstr->str) return LDAPU_ERR_OUT_OF_MEMORY;
    }

    memcpy((void *)&(lstr->str[lstr->len]), (void *)arg, arglen);
    lstr->len += arglen;
    lstr->str[lstr->len] = 0;
    return LDAPU_SUCCESS;
}

