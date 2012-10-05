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
 * Description (dnfilter.c)
 *
 *	This module supports access to a file containing DNS name suffix
 *	specifications.  These specifications are used to accept or
 *	reject clients based on their DNS names.
 */

#include "netsite.h"
#include "nspr.h"
#ifdef NS_OLDES3X
#include "prhash.h"
#endif /* NS_OLDES3X */
#include "base/pblock.h"
#include "base/objndx.h"
#include "frame/dnfilter.h"
#include "base/util.h"
#include <fcntl.h>
#include <ctype.h>
#include "base/daemon.h"

#ifdef NOACL

#define MYINITHSIZE	64	/* initial hash table size */
#define MYREADSIZE	1024	/* size of each file read */

/* Define states for file parsing */
#define BEGIN_ST	0	/* looking for beginning of something */
#define NLFLUSH_ST	1	/* flushing to newline */
#define LETTER_ST	2	/* looking for a letter in a suffix label */
#define LETDIGHY_ST	3	/* looking for letter, digit, or hyphen */
#define ENDSUF_ST	4	/* after a suffix */
#define WANTDOT_ST	5	/* looking for '.' or end-of-suffix */
#define NEWSUF_ST	6	/* process suffix */

typedef struct DNSToken_s DNSToken_t;
struct DNSToken_s {
    char * dnt_string;		/* pointer to string buffer */
    int dnt_bufsize;		/* current size of buffer */
    int dnt_length;		/* current length of string */
};

typedef struct DNSFilter_s DNSFilter_t;
struct DNSFilter_s {
    char dnf_anchor[4];		/* "DNF" - dnsfilter parameter value points here */
    DNSFilter_t * dnf_next;	/* link to next filter */
    char * dnf_acceptfile;	/* name of dnsaccept filter file */
    char * dnf_rejectfile;	/* name of dnsreject filter file */
    PRHashTable * dnf_hash;	/* pointer to constructed hash table */
};

static DNSFilter_t * filters = NULL;

/* Handle for DNS filter object index */
void * dnf_objndx = NULL;

static char * dns_errstr[] = {
    "insufficient memory",		/* DNFERR_MALLOC	-1 */
    "file open error",			/* DNFERR_FOPEN		-2 */
    "file I/O error",			/* DNFERR_FILEIO	-3 */
    "duplicate filter specification",	/* DNFERR_DUPSPEC	-4 */
    "internal error (bug)",		/* DNFERR_INTERR	-5 */
    "syntax error in filter file",	/* DNFERR_SYNTAX	-6 */
};

/* Hash a key - from mocha mo_atom.c */
static PRHashNumber dns_filter_keyhash(const void * key)
{
    PRHashNumber h;
    unsigned char * s;

    h = 0;
    for (s = (unsigned char *)key; *s; ++s) {
	h = (h >> 28) ^ (h << 4) ^ tolower(*s);
    }

    return h;
}

/* Compare hash table keys - case is ignored */
static int dns_filter_cmpstr(const void * key1, const void * key2)
{
    return !strcasecmp((char *)key1, (char *)key2);
}

/* Compare hash table values */
static int dns_filter_cmpval(const void * value1, const void * value2)
{
    return value1 == value2;
}

/* Return error information in a DNSFilterErr_t structure */
static void dns_filter_error(DNSFilterErr_t * reterr,
			     int errcode, int lineno,
			     char * filename, char * errstr)
{
    if (reterr != NULL) {
	reterr->errNo = errcode;
	reterr->lineno = lineno;
	reterr->filename = (filename) ? STRDUP(filename) : "";
	if (errstr == NULL) {
	    /* If no error string provided, try to supply one */
	    if ((errcode >= DNFERR_MIN) && (errcode <= DNFERR_MAX)) {
		errstr = dns_errstr[DNFERR_MAX-errcode];
	    }
	    else errstr = "unknown error";
	}
	reterr->errstr = errstr;
    }
}

/* Deallocate a DNSFilter_t structure */
NSAPI_PUBLIC void dns_filter_destroy(void * dnfptr)
{
    DNSFilter_t * dnf = (DNSFilter_t *)dnfptr;
    DNSFilter_t **dnfp;

    if (dnf != NULL) {

	/* Remove this filter from the list if it's there */
	for (dnfp = &filters; *dnfp != NULL; dnfp = &(*dnfp)->dnf_next) {
	    if (*dnfp == dnf) {
		*dnfp = dnf->dnf_next;
		break;
	    }
	}

	if (dnf->dnf_acceptfile) {
	    FREE((void *)dnf->dnf_acceptfile);
	}
	if (dnf->dnf_rejectfile) {
	    FREE((void *)dnf->dnf_rejectfile);
	}
	if (dnf->dnf_hash) {
	    PR_HashTableDestroy(dnf->dnf_hash);
	}
	FREE((void *)dnf);
    }
}

/* Variation of dns_filter_destroy() called by objndx at restart */
NSAPI_PUBLIC void dns_filter_decimate(void * dnfptr)
{
    dns_filter_destroy(dnfptr);

    if (filters == NULL) {
	/*
	 * The filter object index is about to go away.  Reset
	 * dnf_objndx so that we recreate it.
	 */
	dnf_objndx = NULL;
    }
}

NSAPI_PUBLIC DNSFilter_t * dns_filter_new(char * acceptname, char * rejectname)
{
    DNSFilter_t * dnf;		/* pointer to returned filter structure */

    dnf = (DNSFilter_t *)MALLOC(sizeof(DNSFilter_t));
    if (dnf) {
	strcpy(dnf->dnf_anchor, "DNF");
	dnf->dnf_acceptfile = (acceptname) ? STRDUP(acceptname) : NULL;
	dnf->dnf_rejectfile = (rejectname) ? STRDUP(rejectname) : NULL;
	dnf->dnf_hash = PR_NewHashTable(MYINITHSIZE, dns_filter_keyhash,
					dns_filter_cmpstr, dns_filter_cmpval,
					NULL, NULL);
	if (!dnf->dnf_hash) {
	    dns_filter_destroy(dnf);
	    dnf = NULL;
	}
    }

    return dnf;
}

/* Append a character to a token */
static char * tokenAppend(DNSToken_t * dnt, char appch)
{
    char * token = dnt->dnt_string;
    int bufsize = dnt->dnt_bufsize;
    int length = dnt->dnt_length;

    length += 1;

    if (token == NULL) {
	/* Assume most of these tokens will fit in 24 bytes */
	token = (char *)MALLOC(24);
	bufsize = 24;
	length = 1;
    }
    else if (length >= bufsize) {
	/* Grow buffer by 8 bytes */
	bufsize += 8;
	token = (char *)REALLOC((void *)token, bufsize);
    }

    if (token) {
	token[length-1] = appch;
	token[length] = 0;
	dnt->dnt_string = token;
	dnt->dnt_bufsize = bufsize;
	dnt->dnt_length = length;
    }

    return token;
}

/*
 * Description (dns_filter_read)
 *
 *	This function reads and parses a DNS filter file.  Entries in
 *	the file specify either complete DNS names of clients, or
 *	suffixes of DNS names.  Entries found in the file are entered
 *	into a filter structure, with a value specified by the caller.
 *
 * Arguments:
 *
 *	dnf		- pointer to filter structure to receive info
 *	filename	- name of filter file to read
 *	hval		- value to assign to hash table entries created
 *	reterr		- error information return pointer, or NULL
 */

NSAPI_PUBLIC int dns_filter_read(DNSFilter_t * dnf, char * filename,
                                 int hval, DNSFilterErr_t * reterr)
{
    PRHashTable * htab = dnf->dnf_hash;
    PRFileDesc *fd;
    int lineno;
    char * cp;
    char * buf;
    int rlen;
    int state;
    int rv;
    PRHashNumber keyhash;
    PRHashEntry * he;
    PRHashEntry **hep;
    DNSToken_t suffix;

    /* Allocate input buffer */
    buf = (char *)MALLOC(MYREADSIZE);
    if (buf == NULL) {
	dns_filter_error(reterr, DNFERR_MALLOC, 0, filename, NULL);
	return DNFERR_MALLOC;
    }

    /* XXX handle relative filename - set default directory */

    /* Open the filter file */
    fd = PR_Open(filename, O_RDONLY, 0);
    if (fd == 0) {
	FREE((void *)buf);
	dns_filter_error(reterr, DNFERR_FOPEN, 0, filename, NULL);
	return DNFERR_FOPEN;
    }

    /* Initialize suffix token */
    suffix.dnt_string = NULL;
    suffix.dnt_bufsize = 0;
    suffix.dnt_length = 0;

    state = BEGIN_ST;
    lineno = 1;

    /* Loop to read file */
    for (;;) {

	rlen = PR_Read(fd, buf, MYREADSIZE);
	if (rlen <= 0) {
	    if (rlen < 0) {
		/* File I/O error */
		rv = DNFERR_FILEIO;
		goto error_ret;
	    }

	    /* EOF - done reading */
	    break;
	}

	/* Point to start of buffer */
	cp = buf;

	/* Begin state-driven parse of buffer contents */
	while (rlen > 0) {

	    switch (state) {

	      case BEGIN_ST:	/* looking for beginning of something */

		/*
		 * We are looking for the beginning of a suffix, which
		 * should start with either "*" or a letter.  We'll
		 * skip whitespace and blank lines, and comments that
		 * begin with "#".
		 */

		/* Count newlines as they go by */
		if (*cp == '\n') {
		    ++lineno;
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Skip whitespace */
		while (isspace(*cp)) {
		    ++cp;
		    if (--rlen <= 0) continue;
		}

		/* Look for start of comments */
		if (*cp == '#') {
		    state = NLFLUSH_ST;
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Look for start of a suffix */
		if (*cp == '*') {
		    /* Want '.' or end-of-suffix next */
		    state = WANTDOT_ST;
		    tokenAppend(&suffix, *cp);
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Try for start of suffix label */
		state = LETTER_ST;
		break;

	      case WANTDOT_ST:	/* looking for a '.' to continue suffix */

		/* Is the next character a '.' ? */
		if (*cp == '.') {
		    /* Yes, keep going on suffix */
		    state = LETTER_ST;
		    tokenAppend(&suffix, *cp);
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Else process current suffix */
		state = NEWSUF_ST;
		break;

	      case ENDSUF_ST:	/* looking at suffix terminator */

		/*
		 * We want to see either a ',', indicating that there's
		 * another suffix coming, or a valid terminator
		 * for the current suffix.  Valid terminators are
		 * whitespace, newline, or "#" (start of comment).
		 */

		/* Newline is a reasonable ending */
		if (*cp == '\n') {
		    state = BEGIN_ST;
		    ++lineno;
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Whitespace is acceptable */
		if (isspace(*cp)) {
		    ++cp;
		    --rlen;
		    continue;
		}

		/* A comma indicates another suffix follows */
		if (*cp == ',') {
		    state = BEGIN_ST;
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Even the start of a comment will do */
		if (*cp == '#') {
		    state = NLFLUSH_ST;
		    ++cp;
		    --rlen;
		    continue;
		}

		rv = DNFERR_SYNTAX;
		goto error_ret;

	      case LETTER_ST:	/* looking for a letter in a suffix label */

		/*
		 * We want to see an upper or lower case letter or there
		 * will be trouble.
		 */

		if (isalpha(*cp)) {
		    state = LETDIGHY_ST;
		    tokenAppend(&suffix, *cp);
		    ++cp;
		    --rlen;
		    continue;
		}

		rv = DNFERR_SYNTAX;
		goto error_ret;

	      case LETDIGHY_ST:	/* looking for letter, digit, or hyphen */

		/*
		 * Letters, digits, and hyphens are added to the current
		 * suffix.  Anything else better be '.' or a valid suffix
		 * terminator.
		 */

		if (isalnum(*cp) || (*cp == '-')) {
		    tokenAppend(&suffix, *cp);
		    ++cp;
		    --rlen;
		    continue;
		}

		/* Try for '.' or a suffix terminator */
		state = WANTDOT_ST;
		break;

	      case NLFLUSH_ST:	/* flushing to newline */

		/* We're in a comment, flushing to end-of-line */
		while ((rlen > 0) && (*cp != '\n')) {
		    ++cp;
		    --rlen;
		}

		/* If no newline yet, read more */
		if (rlen <= 0) continue;

		/* Otherwise enter next state */
		state = BEGIN_ST;
		break;

	      case NEWSUF_ST:	/* process suffix */

		/*
		 * A suffix has terminated validly.  Now add it to the
		 * specified filter.
		 */

		keyhash = (*htab->keyHash)(suffix.dnt_string);
		hep = PR_HashTableRawLookup(htab, keyhash, suffix.dnt_string);

		/* Is there an entry for this suffix already? */
		if ((he = *hep) == 0) {

		    /* No, then add one */
		    he = PR_HashTableRawAdd(htab, hep, keyhash,
					    suffix.dnt_string, (void *)hval);
		    if (he == NULL) {
			rv = DNFERR_MALLOC;
			FREE((void *)suffix.dnt_string);
			goto error_ret;
		    }
		}
		else {

		    /* Suffix is already in table */

		    FREE((void *)suffix.dnt_string);

		    if (hval > (int)(he->value)) {

			/* Store largest value (reject by default) */
			he->value = (void *)hval;
		    }
		    else if (hval == (int)(he->value)) {

			/* Suffix must be duplicated in this file */
			rv = DNFERR_DUPSPEC;
			goto error_ret;
		    }
		}

		/* Reset the suffix token to be empty */
		suffix.dnt_string = NULL;
		suffix.dnt_bufsize = 0;
		suffix.dnt_length = 0;

		/* Go look at the terminator for this suffix */
		state = ENDSUF_ST;
		break;

	      default:
		/* This shouldn't happen ever */
		rv = DNFERR_INTERR;
		goto error_ret;
	    }
	}
    }

    /* Check for a valid ending state */
    if ((state != BEGIN_ST) &&
	(state != ENDSUF_ST) && (state != NLFLUSH_ST)) {
	rv = DNFERR_SYNTAX;
	goto error_ret;
    }

    if (suffix.dnt_string != NULL) {
	/* This shouldn't happen */
	FREE((void *)suffix.dnt_string);
    }

    PR_Close(fd);
    if (buf != NULL) {
	FREE((void *)buf);
    }

    return 0;

  error_ret:
    if (fd > 0) {
	PR_Close(fd);
    }
    if (buf != NULL) {
	FREE((void *)buf);
    }
    if (suffix.dnt_string != NULL) {
	FREE((void *)suffix.dnt_string);
    }
    dns_filter_error(reterr, rv, lineno, filename, NULL);
    return rv;
}

/*
 * Description (dns_filter_setup)
 *
 *	This function checks for "dnsaccept" and "dnsreject" parameter
 *	definitions in a client parameter block.  If one or both of
 *	these are present, a DNSFilter_t structure is created, and
 *	through some questionable magic, a "dnsfilter" parameter is
 *	created to point it.  This structure will subsequently be used
 *	by dns_filter_check() to see if a client DNS name matches any
 *	of the filter specifications in the dnsaccept and/or dnsreject
 *	files.
 *
 * Arguments:
 *
 *	client		- client parameter block pointer
 *	reterr		- pointer to structure for error info, or NULL
 *
 * Returns:
 *
 *	If an error occurs, a negative error code (DNFERR_xxxxx) is
 *	returned, and information about the error is stored in the
 *	structure referenced by reterr, if any.  If there is no error,
 *	the return value is either one or zero, depending on whether
 *	a filter is created or not, respectively.
 */

NSAPI_PUBLIC int dns_filter_setup(pblock * client, DNSFilterErr_t * reterr)
{
    char * acceptname;		/* name of dnsaccept file */
    char * rejectname;		/* name of dnsreject file */
    DNSFilter_t * dnf;		/* pointer to filter structure */
    char * fname;		/* name assigned to this filter */
    pb_param * pp;		/* "dnsfilter" parameter pointer */
    int rv;			/* result value */
    char namebuf[OBJNDXNAMLEN];	/* buffer for filter name */

    /* "dnsfilter" must not be defined by the user in any case */
    pblock_remove("dnsfilter", client);

    /* Get names of dnsaccept and dnsreject files, if any */
    acceptname = pblock_findval("dnsaccept", client);
    rejectname = pblock_findval("dnsreject", client);

    /* If neither are specified, there's nothing to do */
    if (!(acceptname || rejectname)) {
	return 0;
    }

    /* Initialize NSPR (assumes multiple PR_Init() calls ok) */
  /* XXXMB - can we remove this? */
    PR_Init(PR_USER_THREAD, 1, 0);

    dnf = dns_filter_new(acceptname, rejectname);
    if (!dnf) {
	dns_filter_error(reterr, DNFERR_MALLOC, 0,
			 (acceptname) ? acceptname : rejectname, NULL);
	rv = DNFERR_MALLOC;
	goto error_ret;
    }

    /* Is there a dnsaccept file? */
    if (dnf->dnf_acceptfile) {
	/*
	 * Yes, parse the file, creating hash table entries for
	 * the filter patterns.
	 */
	rv = dns_filter_read(dnf, dnf->dnf_acceptfile, 1, reterr);
	if (rv < 0) {
	    dns_filter_destroy(dnf);
	    goto error_ret;
	}
    }

    /* Is there a dnsreject file? */
    if (dnf->dnf_rejectfile) {
	/*
	 * Yes, parse the file, creating hash table entries for
	 * the filter patterns.
	 */
	rv = dns_filter_read(dnf, dnf->dnf_rejectfile, 2, reterr);
	if (rv < 0) {
	    dns_filter_destroy(dnf);
	    goto error_ret;
	}
    }

    /* Create the object index for DNS filters if necessary */
    if (dnf_objndx == NULL) {

	dnf_objndx = objndx_create(8, dns_filter_decimate);

	/*
	 * Arrange for the object index and all the filters in it to
	 * be cleaned up at restart.
	 */
#if 0
	daemon_atrestart(objndx_destroy, dnf_objndx);
#endif
    }

    /* Register the filter in the object index */
    fname = objndx_register(dnf_objndx, (void *)dnf, namebuf);
    if (fname == NULL) {
	dns_filter_destroy(dnf);
	dns_filter_error(reterr, DNFERR_MALLOC, 0,
			 (acceptname) ? acceptname : rejectname, NULL);
	rv = DNFERR_MALLOC;
	goto error_ret;
    }
	    
    /* Create a parameter for the client, dnsfilter=<filter-name> */
    pp = pblock_nvinsert("dnsfilter", fname, client);
    if (pp == NULL) {
	dns_filter_destroy(dnf);
	dns_filter_error(reterr, DNFERR_MALLOC, 0,
			 (acceptname) ? acceptname : rejectname, NULL);
	rv = DNFERR_MALLOC;
	goto error_ret;
    }

    /* Add this to the list of filters */
    dnf->dnf_next = filters;
    filters = dnf;

    /* Indicate filter created */
    return 1;

  error_ret:
    /*
     * Our assumption here is that our caller is just going to log our
     * error and keep going.  Our caller may in fact get far enough
     * to make calls to dns_filter_check() for the current client,
     * in which case, we want dns_filter_check() to return an error
     * code to indicate that a filter was specified but not applied,
     * due to the current error condition.  So we add a special
     * "dnsfilter=?" parameter to the client, which dns_filter_check()
     * can recognize as a broken filter.
     */
    pp = pblock_nvinsert("dnsfilter", "?", client);
    return rv;
}

/*
 * Description (dns_filter_check)
 *
 *	This function checks a client parameter block for a "dnsfilter"
 *	parameter.  If present, its value will point to a DNSFilter_t
 *	structure, and the client's DNS name will be checked against this
 *	filter.  If the specified pointer to the client's DNS name is
 *	NULL, the function assumes a DNS name of "unknown".
 *
 * Arguments:
 *
 *	client		- client parameter block pointer
 *	cdns		- client DNS name pointer, or NULL
 *
 * Returns:
 *
 *	-2	- there was a broken filter indication
 *		  (see "error_ret:" comments above)
 *	-1	- there was a reject filter and the client was rejected,
 *		  or there was only an accept filter and it did not
 *		  accept the client
 *	 0	- there was no filter present
 *	 1	- there was an accept filter and the client was accepted,
 *		  or there was only a reject filter and it did not
 *		  reject the client
 */

NSAPI_PUBLIC int dns_filter_check(pblock * client, char * cdns)
{
    DNSFilter_t * dnf;		/* DNS filter structure pointer */
    char * fname;		/* filter name */
    char * cdnscpy;		/* copy of client DNS name */
    char * subdns;		/* suffix of client DNS name */
    PRHashTable * htab;		/* hash table pointer */
    int hval;			/* hash entry value */

    /* Got the client's DNS name? */
    if (!cdns || !*cdns) {
	/* No, use special one */
	cdns = "unknown";
    }
    
    /* Is there a "dnsfilter" parameter for the client? */
    fname = pblock_findval("dnsfilter", client);
    if (fname == NULL) {
	/* No, nothing to do */
	return 0;
    }

    /* Check for broken filter */
    if (fname[0] == '?') {
	/* Yep, it's broke */
	return -2;
    }

    /* Look up pointer to filter, using filter name */
    dnf = (DNSFilter_t *)objndx_lookup(dnf_objndx, fname);
    if (dnf == NULL) {
	/* Not found, give up */
	return 0;
    }

    /* Get hash table pointer */
    htab = dnf->dnf_hash;

    /*
     * Look up each possible suffix for the client domain name,
     * starting with the entire string, and working toward the
     * last component.
     */
    cdnscpy = STRDUP(cdns);
    subdns = cdnscpy;

    for (;;) {

	/* Look up the domain name suffix in the hash table */
	hval = (int)PR_HashTableLookup(htab, (void *)subdns);
	if (hval != 0) break;

	/* Step to the next level */
	if ((subdns[0] == '*') && (subdns[1] == '.')) subdns += 2;
	subdns = strchr(subdns, '.');
	if (subdns == NULL) break;

	/* Prepend a '*' to match the pattern in the filter */
	*--subdns = '*';
    }

    FREE((void *)cdnscpy);

    /* One more possibility if nothing found yet... */
    if (!hval) {
	hval = (int)PR_HashTableLookup(htab, (void *)"*");
    }

    /*
     * The values of hval are interpreted as:
     *
     *		0 - no suffixes of the dns name appear in the table
     *		1 - a suffix was present with a 'accept' indication
     *		2 - a suffix was present with an 'reject' indication
     */
    if (hval > 1) {
	hval = -1;
    }
    else if (hval == 0) {

	/*
	 * There was no information for the client DNS in the table.
	 * If there is only a dnsaccept file, but no dnsreject file,
	 * figure that the client should be rejected.  On the other
	 * hand, if there is only a dnsreject file, but no dnsaccept
	 * file, figure that the client should be accepted.
	 */
	if (dnf->dnf_acceptfile != NULL) {

	    /* Only an accept file, no reject file? */
	    if (dnf->dnf_rejectfile == NULL) {

		/* Reject client */
		hval = -1;
	    }
	}
	else if (dnf->dnf_rejectfile != NULL) {

	    /* Accept client - no accept file, but reject file present */
	    hval = 1;
	}
    }

    return hval;
}
#endif /* NOACL */
