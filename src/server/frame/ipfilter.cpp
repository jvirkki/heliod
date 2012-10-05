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
 * Description (ipfilter.c)
 *
 *	This module supports access to a file containing IP host and
 *	network specifications.  These specifications are used to
 *	accept or reject clients based on their IP address.
 */

#include "base/systems.h"
#include "netsite.h"
#include "nspr.h"
#include "base/pblock.h"
#include "base/objndx.h"
#include "base/lexer.h"
#include "frame/ipfilter.h"
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#ifndef XP_WIN32
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "base/daemon.h"

#ifdef NOACL

/* extern unsigned long inet_addr(char * ipastr);  CKA: already defined somewhere else */

#define MYREADSIZE	1024		/* buffer size for file reads */

/*
 * Description (IPNode_t)
 *
 * This type describes an internal node in the radix tree.  An internal
 * node has a link up the tree to its parent, and up to three links
 * down the tree to its descendants.  Each internal node is used to
 * test a particular bit in a given IP address, and traverse down the
 * tree in a direction which depends on whether the bit is set, clear,
 * or masked out.  The descendants of an internal node may be internal
 * nodes or leaf nodes (IPLeaf_t).
 */

/* Define indices of links in an IPNode_t */
#define IPN_CLEAR	0	/* link to node with ipn_bit clear */
#define IPN_SET		1	/* link to node with ipn_bit set */
#define IPN_MASKED	2	/* link to node with ipn_bit masked out */
#define IPN_NLINKS	3	/* number of links */

typedef struct IPNode_s IPNode_t;
struct IPNode_s {
    char ipn_type;		/* node type */
#define IPN_LEAF	0	/* leaf node */
#define IPN_NODE	1	/* internal node */

    char ipn_bit;		/* bit number (31-0) to test */
    IPNode_t * ipn_parent;	/* link to parent node */
    IPNode_t * ipn_links[IPN_NLINKS];	
};

/* Helper definitions */
#define ipn_clear	ipn_links[IPN_CLEAR]
#define ipn_set		ipn_links[IPN_SET]
#define ipn_masked	ipn_links[IPN_MASKED]

/*
 * Description (IPLeaf_t)
 *
 * This type describes a leaf node in the radix tree.  A leaf node
 * contains an IP host or network address, and a network mask.  A
 * given IP address matches a leaf node if the IP address, when masked
 * by ipl_netmask, equals ipl_ipaddr.
 */

typedef struct IPLeaf_s IPLeaf_t;
struct IPLeaf_s {
    char ipl_type;		/* see ipn_type in IPNode_t */
    char ipl_disp;		/* disposition of matching IP addresses */
#define IPL_ACCEPT	0	/* accept matching IP addresses */
#define IPL_REJECT	1	/* reject matching IP addresses */

    IPAddr_t ipl_netmask;	/* IP network mask */
    IPAddr_t ipl_ipaddr;	/* IP address of host or network */
};

typedef struct IPFilter_s IPFilter_t;
struct IPFilter_s {
    char ipf_anchor[4];		/* "IPF" - ipfilter parameter value points here */
    IPFilter_t * ipf_next;	/* link to next filter */
    char * ipf_acceptfile;	/* name of ipaccept filter file */
    char * ipf_rejectfile;	/* name of ipreject filter file */
    IPNode_t * ipf_tree;	/* pointer to radix tree structure */
};

static IPFilter_t * filters = NULL;

/* Handle for IP filter object index */
void * ipf_objndx = NULL;

static char * classv[] = {
    " \t\r\f\013",		/* class 0 - whitespace */
    "\n",			/* class 1 - newline */
    ",",			/* class 2 - comma */
    ".",			/* class 3 - period */
    "0123456789",		/* class 4 - digits */
				/* class 5 - letters */
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
};

static int classc = sizeof(classv)/sizeof(char *);

#define CCM_WS		0x1	/* whitespace */
#define CCM_NL		0x2	/* newline */
#define CCM_COMMA	0x4	/* comma */
#define CCM_PERIOD	0x8	/* period */
#define CCM_DIGIT	0x10	/* digits */
#define CCM_LETTER	0x20	/* letters */

static char * ip_errstr[] = {
    "insufficient memory",		/* IPFERR_MALLOC	-1 */
    "file open error",			/* IPFERR_FOPEN		-2 */
    "file I/O error",			/* IPFERR_FILEIO	-3 */
    "duplicate filter specification",	/* IPFERR_DUPSPEC	-4 */
    "internal error (bug)",		/* IPFERR_INTERR	-5 */
    "syntax error in filter file",	/* IPFERR_SYNTAX	-6 */
    "conflicting filter specification",	/* IPFERR_CNFLICT	-7 */
};

/*
 * Description (ip_filter_search)
 *
 *	This function searches a specified subtree of the radix tree,
 *	looking for the best match for a given IP address.
 *
 * Arguments:
 *
 *	ipaddr		- the IP host or network address value
 *	root		- the root node of the subtree to be searched
 *
 * Returns:
 *
 *	A pointer to a matching leaf node is returned, if one is found.
 *	Otherwise NULL is returned.
 */

NSAPI_PUBLIC IPLeaf_t * ip_filter_search(IPAddr_t ipaddr, IPNode_t * root)
{
    IPLeaf_t * leaf;		/* leaf node pointer */
    IPAddr_t bitmask;		/* bit mask for current node */
    IPNode_t * ipn;		/* current internal node */
    IPNode_t * lastipn;		/* last internal node seen in search */
    IPNode_t * mipn;		/* ipn_masked subtree root pointer */

    lastipn = NULL;
    ipn = root;

    /*
     * The tree traversal first works down the tree, under the assumption
     * that all of the bits in the given IP address may be significant.
     * The internal nodes of the tree will cause particular bits of the
     * IP address to be tested, and the ipn_clear or ipn_set link to
     * a descendant followed accordingly.  The internal nodes are arranged
     * in such a way that high-order bits are tested before low-order bits.
     * Usually some bits are skipped, as they are not needed to distinguish
     * the entries in the tree.
     *
     * At the bottom of the tree, a leaf node may be found, or the last
     * descendant link may be NULL.  If a leaf node is found, it is
     * tested for a match against the given IP address.  If it doesn't
     * match, or the link was NULL, backtracking begins, as described
     * below.
     *
     * Backtracking follows the ipn_parent links back up the tree from
     * the last internal node, looking for internal nodes with ipn_masked
     * descendants.  The subtrees attached to these links are traversed
     * downward, as before, with the same processing at the bottom as
     * the first downward traversal.  Following the ipn_masked links is
     * essentially examining the possibility that the IP address bit
     * associated with the internal node may be masked out by the
     * ipl_netmask in a leaf at the bottom of such a subtree.  Since
     * the ipn_masked links are examined from the bottom of the tree
     * to the top, this looks at the low-order bits first.
     */

    while (ipn != NULL) {

	/*
	 * Work down the tree testing bits in the IP address indicated
	 * by the internal nodes.  Exit the loop when there are no more
	 * internal nodes.
	 */
	while ((ipn != NULL) && (ipn->ipn_type == IPN_NODE)) {

	    /* Save pointer to internal node */
	    lastipn = ipn;

	    /* Get a mask for the bit this node tests */
	    bitmask = 1<<ipn->ipn_bit;

	    /* Select link to follow for this IP address */
	    ipn = (bitmask & ipaddr) ? ipn->ipn_set : ipn->ipn_clear;
	}

	/* Did we end up with a non-NULL node pointer? */
	if (ipn != NULL) {

	    /* It must be a leaf node */
	    assert(ipn->ipn_type == IPN_LEAF);
	    leaf = (IPLeaf_t *)ipn;

	    /* Is it a matching leaf? */
	    if (leaf->ipl_ipaddr == (ipaddr & leaf->ipl_netmask)) {

		/* Yes, we are done */
		return leaf;
	    }
	}

	/*
	 * Backtrack, starting at lastipn. Search each subtree
	 * emanating from an ipn_masked link.  Step up the tree
	 * until the ipn_masked link of the node referenced by
	 * "root" has been considered.
	 */

	for (ipn = lastipn; ipn != NULL; ipn = ipn->ipn_parent) {

	    /*
	     * Look for a node with a non-NULL masked link, but don't
	     * go back to the node we just came from.
	     */

	    if ((ipn->ipn_masked != NULL) && (ipn->ipn_masked != lastipn)) {

		/* Get the root of this subtree */
		mipn = ipn->ipn_masked;

		/* If this is an internal node, start downward traversal */
		if (mipn->ipn_type == IPN_NODE) {
		    ipn = mipn;
		    break;
		}

		/* Otherwise it's a leaf */
		assert(mipn->ipn_type == IPN_LEAF);
		leaf = (IPLeaf_t *)mipn;

		/* Is it a matching leaf? */
		if (leaf->ipl_ipaddr == (ipaddr & leaf->ipl_netmask)) {

		    /* Yes, we are done */
		    return leaf;
		}
	    }

	    /* Don't consider nodes above the given root */
	    if (ipn == root) {

		/* No matching entry found */
		return NULL;
	    }

	    lastipn = ipn;
	}
    }

    /* No matching entry found */
    return NULL;
}

/*
 * Description (ip_filter_add)
 *
 *	This function adds a new [IP address, netmask] entry to a
 *	specified IP filter, along with an indication of whether matching
 *	IP addresses are to be accepted or rejected.  Duplicate entries,
 *	that is entries with the same IP address and netmask, are not
 *	permitted.
 *
 * Arguments:
 *
 *	ipf		- pointer to IPFilter_t structure
 *	ipaddr		- the IP host or network address value
 *	netmask		- the netmask associated with this IP address
 *	disp		- value for ipl_disp field of leaf
 *
 * Returns:
 *
 *	Zero if successful.  Otherwise an IPFERR_xxxx error code.
 */

NSAPI_PUBLIC int ip_filter_add(IPFilter_t * ipf,
                               IPAddr_t ipaddr, IPAddr_t netmask, int disp)
{
    IPNode_t * ipn;		/* current node pointer */
    IPNode_t * lastipn;		/* last (lower) node pointer */
    IPLeaf_t * leaf;		/* leaf node pointer */
    IPAddr_t bitmask;		/* bit mask for current node */
    int lastbit;		/* number of last bit set in netmask */
    int i;			/* loop index */

    lastipn = NULL;

    for (ipn = ipf->ipf_tree; (ipn != NULL) && (ipn->ipn_type == IPN_NODE); ) {

	/* Get a mask for the bit this node tests */
	bitmask = 1<<ipn->ipn_bit;

	/* Save pointer to last internal node */
	lastipn = ipn;

	/* Is this a bit we care about? */
	if (bitmask & netmask) {

	    /* Yes, get address of set or clear descendant pointer */
	    ipn = (bitmask & ipaddr) ? ipn->ipn_set : ipn->ipn_clear;
	}
	else {
	    /* No, get the address of the masked descendant pointer */
	    ipn = ipn->ipn_masked;
	}
    }

    /* Did we end up at a leaf node? */
    if (ipn == NULL) {

	/*
         * No, well, we need to find a leaf node if possible.  The
         * reason is that we need an IP address and netmask to compare
         * to the IP address and netmask we're inserting.  We know that
         * they're the same up to the bit tested by the lastipn node,
         * but we need to know the *highest* order bit that's different.
         * Any leaf node below lastipn will do.
         */

	leaf = NULL;
        ipn = lastipn;

        while (ipn != NULL) {

            /* Look for any non-null child link of the current node */
            for (i = 0; i < IPN_NLINKS; ++i) {
                if (ipn->ipn_links[i]) break;
            }

            /*
             * Fail search for leaf if no non-null child link found.
             * This should only happen on the root node of the tree
             * when the tree is empty.
             */
            if (i >= IPN_NLINKS) {
                assert(ipn == ipf->ipf_tree);
                break;
            }

            /* Step to the child node */
            ipn = ipn->ipn_links[i];

            /* Is it a leaf? */
            if (ipn->ipn_type == IPN_LEAF) {

                /* Yes, search is over */
                leaf = (IPLeaf_t *)ipn;
                ipn = NULL;
                break;
	    }
	}
    }
    else {

	/* Yes, loop terminated on a leaf node */
	assert(ipn->ipn_type == IPN_LEAF);
	leaf = (IPLeaf_t *)ipn;

	/* Same IP address and netmask? */
	if ((leaf->ipl_ipaddr == ipaddr) && (leaf->ipl_netmask == netmask)) {

	    /* Yes, error if not same disp.  Otherwise done. */
	    return (leaf->ipl_disp == disp) ? 0 : IPFERR_CNFLICT;
	}
    }

    /* Got a leaf yet? */
    if (leaf != NULL) {

	/* Combine the IP address and netmask differences */
	bitmask = (leaf->ipl_ipaddr ^ ipaddr) | (leaf->ipl_netmask ^ netmask);

	assert(bitmask != 0);

	/* Find the bit number of the first different bit */
	for (lastbit = 31;
	     (bitmask & 0x80000000) == 0; --lastbit, bitmask <<= 1) ;

	/* Generate a bit mask with just that bit */
	bitmask = 1 << lastbit;

	/*
	 * Go up the tree from lastipn, looking for an internal node
	 * that tests lastbit.  Stop if we get to a node that tests
	 * a higher bit number first.
	 */
	for (ipn = lastipn, lastipn = (IPNode_t *)leaf;
	     ipn != NULL; ipn = ipn->ipn_parent) {

	    if (ipn->ipn_bit >= lastbit) {
		if (ipn->ipn_bit == lastbit) {
		    /* Need to add a leaf off ipn node */
		    lastipn = NULL;
		}
		break;
	    }
	    lastipn = ipn;
	}

	assert(ipn != NULL);
    }
    else {

	/* Just hang a leaf off the lastipn node if no leaf */
	ipn = lastipn;
	lastipn = NULL;
	lastbit = ipn->ipn_bit;
    }

    /*
     * If lastipn is not NULL at this point, the new leaf will hang
     * off an internal node inserted between the upper node, referenced
     * by ipn, and the lower node, referenced by lastipn.  The lower
     * node may be an internal node or a leaf.
     */
    if (lastipn != NULL) {
	IPNode_t * parent = ipn;	/* parent of the new node */

	assert((lastipn->ipn_type == IPN_LEAF) ||
	       (ipn == lastipn->ipn_parent));

	/* Allocate space for the internal node */
	ipn = (IPNode_t *)MALLOC(sizeof(IPNode_t));
	if (ipn == NULL) {
	    return IPFERR_MALLOC;
	}

	ipn->ipn_type = IPN_NODE;
	ipn->ipn_bit = lastbit;
	ipn->ipn_parent = parent;
	ipn->ipn_clear = NULL;
	ipn->ipn_set = NULL;
	ipn->ipn_masked = NULL;

	bitmask = 1 << lastbit;

	/*
	 * The values in the leaf we found above determine which
	 * descendant link of the new internal node will reference
	 * the subtree that we just ascended.
	 */
	if (leaf->ipl_netmask & bitmask) {
	    if (leaf->ipl_ipaddr & bitmask) {
		ipn->ipn_set = lastipn;
	    }
	    else {
		ipn->ipn_clear = lastipn;
	    }
	}
	else {
	    ipn->ipn_masked = lastipn;
	}

	/* Allocate space for the new leaf */
	leaf = (IPLeaf_t *)MALLOC(sizeof(IPLeaf_t));
	if (leaf == NULL) {
	    FREE((void *)ipn);
	    return IPFERR_MALLOC;
	}

	/* Insert internal node in tree */

	/* First the downward link from the parent to the new node */
	for (i = 0; i < IPN_NLINKS; ++i) {
	    if (parent->ipn_links[i] == lastipn) {
		parent->ipn_links[i] = ipn;
		break;
	    }
	}

	/* Then the upward link from the child (if it's not a leaf) */
	if (lastipn->ipn_type == IPN_NODE) {
	    lastipn->ipn_parent = ipn;
	}
    }
    else {
	/* Allocate space for a leaf node only */
	leaf = (IPLeaf_t *)MALLOC(sizeof(IPLeaf_t));
	if (leaf == NULL) {
	    return IPFERR_MALLOC;
	}
    }

    /* Initialize the new leaf */
    leaf->ipl_type = IPN_LEAF;
    leaf->ipl_disp = disp;
    leaf->ipl_ipaddr = ipaddr;
    leaf->ipl_netmask = netmask;

    /*
     * Select the appropriate descendant link of the internal node
     * and point it at the new leaf.
     */
    bitmask = 1 << ipn->ipn_bit;
    if (bitmask & netmask) {
	if (bitmask & ipaddr) {
	    assert(ipn->ipn_set == NULL);
	    ipn->ipn_set = (IPNode_t *)leaf;
	}
	else {
	    assert(ipn->ipn_clear == NULL);
	    ipn->ipn_clear = (IPNode_t *)leaf;
	}
    }
    else {
	assert(ipn->ipn_masked == NULL);
	ipn->ipn_masked = (IPNode_t *)leaf;
    }

    /* Successful completion */
    return 0;
}

/* Return error information in a IPFilterErr_t structure */
static void ip_filter_error(IPFilterErr_t * reterr,
			    int errcode, int lineno,
			    char * filename, char * errstr)
{
    if (reterr != NULL) {
	reterr->errNo = errcode;
	reterr->lineno = lineno;
	reterr->filename = (filename) ? STRDUP(filename) : "";
	if (errstr == NULL) {
	    /* If no error string provided, try to supply one */
	    if ((errcode >= IPFERR_MIN) && (errcode <= IPFERR_MAX)) {
		errstr = ip_errstr[IPFERR_MAX-errcode];
	    }
	    else errstr = "unknown error";
	}
	reterr->errstr = errstr;
    }
}

/* Deallocate a IPFilter_t structure */
NSAPI_PUBLIC void ip_filter_destroy(void * ipfptr)
{
    IPFilter_t * ipf = (IPFilter_t *)ipfptr;
    IPFilter_t **ipfp;
    IPNode_t * ipn;			/* current node pointer */
    IPNode_t * parent;			/* parent node pointer */
    int i;

    if (ipf != NULL) {

	/* Remove this filter from the list if it's there */
	for (ipfp = &filters; *ipfp != NULL; ipfp = &(*ipfp)->ipf_next) {
	    if (*ipfp == ipf) {
		*ipfp = ipf->ipf_next;
		break;
	    }
	}

	if (ipf->ipf_acceptfile) {
	    FREE((void *)ipf->ipf_acceptfile);
	}
	if (ipf->ipf_rejectfile) {
	    FREE((void *)ipf->ipf_rejectfile);
	}

	/* Traverse tree, freeing nodes, except root */
	for (parent = ipf->ipf_tree; parent != NULL; ) {

	    /* Look for a link to a child node */
	    for (i = 0; i < IPN_NLINKS; ++i) {
		ipn = parent->ipn_links[i];
		if (ipn != NULL) break;
	    }

	    /* Any children for the parent node? */
	    if (ipn == NULL) {

		/* No, if it's the root, we're done */
		if (parent == ipf->ipf_tree) break;

		/* Otherwise back up the tree */
		ipn = parent;
		parent = ipn->ipn_parent;

		/* Free the lower node */
		FREE(ipn);
		continue;
	    }

	    /*
	     * Found a child node for the current parent.
	     * NULL out the downward link and check it out.
	     */
	    parent->ipn_links[i] = NULL;

	    /* Is it a leaf? */
	    if (ipn->ipn_type == IPN_LEAF) {
		/* Yes, free it */
		FREE(ipn);
		continue;
	    }

	    /* No, step down the tree */
	    parent = ipn;
	}

	/* Free the IPFilter_t structure and the root IPNode_t */
	FREE((void *)ipf);
    }
}

/* Variation of dns_filter_destroy() called by objndx at restart */
NSAPI_PUBLIC void ip_filter_decimate(void * ipfptr)
{
    ip_filter_destroy(ipfptr);

    if (filters == NULL) {
	/*
	 * The filter object index is about to go away.  Reset
	 * ipf_objndx so that we recreate it.
	 */
	ipf_objndx = NULL;
    }
}

NSAPI_PUBLIC IPFilter_t * ip_filter_new(char * acceptname, char * rejectname)
{
    IPFilter_t * ipf;		/* pointer to returned filter structure */
    IPNode_t * ipn;		/* pointer to initial node */

    assert(sizeof(IPAddr_t) >= 4);

    ipf = (IPFilter_t *)MALLOC(sizeof(IPFilter_t) + sizeof(IPNode_t));
    if (ipf) {
	strcpy(ipf->ipf_anchor, "IPF");
	ipf->ipf_acceptfile = (acceptname) ? STRDUP(acceptname) : NULL;
	ipf->ipf_rejectfile = (rejectname) ? STRDUP(rejectname) : NULL;

	/*
	 * Initialize a radix tree to filter IP addresses.  The initial
	 * tree contains only one internal node, for bit 31, with no
	 * descendants.
	 */

	ipn = (IPNode_t *)(ipf + 1);
	ipn->ipn_type = IPN_NODE;
	ipn->ipn_bit = 31;
	ipn->ipn_parent = NULL;
	ipn->ipn_clear = NULL;
	ipn->ipn_set = NULL;
	ipn->ipn_masked = NULL;

	ipf->ipf_tree = ipn;
    }

    return ipf;
}

/* Helper routine for ip_filter_read, called from lexer */
static int read_more(LEXStream_t * lst)
{
    PRFileDesc *fd = (PRFileDesc *)(lst->lst_strmid);
    int rlen;

    rlen = PR_Read(fd, lst->lst_buf, lst->lst_buflen);
    if (rlen < 0) {
	/* File I/O error */
	return IPFERR_FILEIO;
    }

    lst->lst_len = rlen;
    return rlen;
}

/*
 * Description (ip_filter_read)
 *
 *	This function reads and parses a IP filter file.  Entries in
 *	the file specify IP host or network addresses of clients.
 *	Entries found in the file are entered into a filter structure,
 *	with a value specified by the caller.
 *
 * Arguments:
 *
 *	ipf		- pointer to filter structure to receive info
 *	filename	- name of filter file to read
 *	disp		- value to be associated with filter entries
 *	reterr		- error information return pointer, or NULL
 */

NSAPI_PUBLIC int ip_filter_read(IPFilter_t * ipf, char * filename,
                                int disp, IPFilterErr_t * reterr)
{
    LEXStream_t * lst;		/* input stream pointer */
    void * chtab;		/* character class table reference */
    void * token;		/* current token reference */
    char * tokenstr;		/* token string pointer */
    IPAddr_t ipaddr;		/* IP host or network address */
    IPAddr_t netmask;		/* IP network mask */
    PRFileDesc *fd;
    int lineno = 0;
    int rv;

    fd = (PRFileDesc *)0;
    lst = NULL;
    chtab = NULL;
    token = NULL;

    /* XXX handle relative filename - set default directory */

    /* Open the filter file */
    fd = PR_Open(filename, O_RDONLY, 0);
    if (fd == 0) {
	ip_filter_error(reterr, IPFERR_FOPEN, 0, filename, NULL);
	return IPFERR_FOPEN;
    }

    /* Initialize a lexer stream for the file */
    lst = lex_stream_create(read_more, (void *)fd, NULL, MYREADSIZE);
    if (lst == NULL) {
	ip_filter_error(reterr, IPFERR_MALLOC, 0, filename, NULL);
	rv = IPFERR_MALLOC;
	goto error_ret;
    }

    /* Initialize character classes for lexer processing */
    rv = lex_class_create(classc, classv, &chtab);
    if (rv < 0) {
	goto error_ret;
    }

    rv = lex_token_new((pool_handle_t *)0, 24, 8, &token);
    if (rv < 0) {
	goto error_ret;
    }

    lineno = 1;

    /* Loop to read file */
    for (;;) {

	/* Skip whitespace and commas, but not newline */
	rv = lex_skip_over(lst, chtab, CCM_WS|CCM_COMMA);
	if (rv < 0) goto error_ret;

	/* Exit loop if EOF */
	if (rv == 0) break;

	if (rv == '\n') {
	    /* Keep count of lines as we're skipping whitespace */
	    ++lineno;
	    (void)lex_next_char(lst, chtab, CCM_NL);
	    continue;
	}

	/* Check for beginning of comment */
	if (rv == '#') {
	    /* Skip to a newline if so */
	    rv = lex_skip_to(lst, chtab, CCM_NL);
	    if (rv < 0) break;
	    continue;
	}

	/* Assume no netmask */
	netmask = 0xffffffff;

	/* Initialize token for IP address */
	rv = lex_token_start(token);

	/* Collect token including digits, letters, and periods */
	rv = lex_scan_over(lst, chtab, (CCM_DIGIT|CCM_LETTER|CCM_PERIOD),
			   token);
	if (rv < 0) goto error_ret;

	/* Get a pointer to the token string */
	tokenstr = lex_token(token);

	/* A NULL pointer or an empty string is an error */
	if (!tokenstr || !*tokenstr) {
	    rv = IPFERR_SYNTAX;
	    goto error_ret;
	}

	/* Convert IP address to binary */
	ipaddr = inet_addr(tokenstr);
	if (ipaddr == (unsigned long)-1) {
	    rv = IPFERR_SYNTAX;
	    goto error_ret;
	}

	/* Skip whitespace */
	rv = lex_skip_over(lst, chtab, CCM_WS);
	if (rv < 0) goto error_ret;

	/* If no digit, must not be a netmask */
	if (!isdigit(rv)) goto add_entry;

	/* Initialize token for network mask */
	rv = lex_token_start(token);

	/* Collect token including digits, letters, and periods */
	rv = lex_scan_over(lst, chtab, (CCM_DIGIT|CCM_LETTER|CCM_PERIOD),
			   token);
	if (rv < 0) goto error_ret;

	/* Get a pointer to the token string */
	tokenstr = lex_token(token);

	/* A NULL pointer or an empty string is an error */
	if (!tokenstr || !*tokenstr) {
	    rv = IPFERR_SYNTAX;
	    goto error_ret;
	}

	/*
	 * Convert netmask to binary.  Note 255.255.255.255 is not a
	 * valid netmask.
	 */
	netmask = inet_addr(tokenstr);
	if (netmask == (unsigned long)-1) {
	    rv = IPFERR_SYNTAX;
	    goto error_ret;
	}

      add_entry:
	rv = ip_filter_add(ipf, ipaddr, netmask, disp);
	if (rv < 0) goto error_ret;
    }

  error_ret:
    if (fd >= 0) {
	PR_Close(fd);
    }
    if (lst) lex_stream_destroy(lst);
    if (chtab) lex_class_destroy(chtab);

    if (rv < 0) {
	ip_filter_error(reterr, rv, lineno, filename, NULL);
    }
    return rv;
}

/*
 * Description (ip_filter_setup)
 *
 *	This function checks for "ipaccept" and "ipreject" parameter
 *	definitions in a client parameter block.  If one or both of
 *	these are present, a IPFilter_t structure is created, and
 *	through some questionable magic, a "ipfilter" parameter is
 *	created to point it.  This structure will subsequently be used
 *	by ip_filter_check() to see if a client IP address matches any
 *	of the filter specifications in the ipaccept and/or ipreject
 *	files.
 *
 * Arguments:
 *
 *	client		- client parameter block pointer
 *	reterr		- pointer to structure for error info, or NULL
 *
 * Returns:
 *
 *	If an error occurs, a negative error code (IPFERR_xxxxx) is
 *	returned, and information about the error is stored in the
 *	structure referenced by reterr, if any.  If there is no error,
 *	the return value is either one or zero, depending on whether
 *	a filter is created or not, respectively.
 */

NSAPI_PUBLIC int ip_filter_setup(pblock * client, IPFilterErr_t * reterr)
{
    char * acceptname;		/* name of ipaccept file */
    char * rejectname;		/* name of ipreject file */
    IPFilter_t * ipf;		/* pointer to filter structure */
    char * fname;		/* name assigned to this filter */
    pb_param * pp;		/* "ipfilter" parameter pointer */
    int rv;			/* result value */
    char namebuf[OBJNDXNAMLEN];	/* buffer for filter name */

    /* "ipfilter" must not be defined by the user in any case */
    pblock_remove("ipfilter", client);

    /* Get names of ipaccept and ipreject files, if any */
    acceptname = pblock_findval("ipaccept", client);
    rejectname = pblock_findval("ipreject", client);

    /* If neither are specified, there's nothing to do */
    if (!(acceptname || rejectname)) {
	return 0;
    }

    /* Initialize NSPR (assumes multiple PR_Init() calls ok) */
    /* XXXMB can we get rid of this? */
    PR_Init(PR_USER_THREAD, 1, 0);

    ipf = ip_filter_new(acceptname, rejectname);
    if (!ipf) {
	ip_filter_error(reterr, IPFERR_MALLOC, 0,
			(acceptname) ? acceptname : rejectname, NULL);
	rv = IPFERR_MALLOC;
	goto error_ret;
    }

    /* Is there a ipaccept file? */
    if (ipf->ipf_acceptfile) {
	/*
	 * Yes, parse the file, creating hash table entries for
	 * the filter patterns.
	 */
	rv = ip_filter_read(ipf, ipf->ipf_acceptfile, IPL_ACCEPT, reterr);
	if (rv < 0) {
	    ip_filter_destroy(ipf);
	    goto error_ret;
	}
    }

    /* Is there a ipreject file? */
    if (ipf->ipf_rejectfile) {
	/*
	 * Yes, parse the file, creating hash table entries for
	 * the filter patterns.
	 */
	rv = ip_filter_read(ipf, ipf->ipf_rejectfile, IPL_REJECT, reterr);
	if (rv < 0) {
	    ip_filter_destroy(ipf);
	    goto error_ret;
	}
    }

    /* Create the object index for IP filters if necessary */
    if (ipf_objndx == NULL) {

	ipf_objndx = objndx_create(8, ip_filter_decimate);

	/*
	 * Arrange for the object index and all the filters in it to
	 * be cleaned up at restart.
	 */
#if 0
	daemon_atrestart(objndx_destroy, ipf_objndx);
#endif
    }

    /* Register the filter in the object index */
    fname = objndx_register(ipf_objndx, (void *)ipf, namebuf);
    if (fname == NULL) {
	ip_filter_destroy(ipf);
	ip_filter_error(reterr, IPFERR_MALLOC, 0,
			(acceptname) ? acceptname : rejectname, NULL);
	rv = IPFERR_MALLOC;
	goto error_ret;
    }
	    
    /* Create a parameter for the client, ipfilter=<filter-name> */
    pp = pblock_nvinsert("ipfilter", fname, client);
    if (pp == NULL) {
	ip_filter_destroy(ipf);
	ip_filter_error(reterr, IPFERR_MALLOC, 0,
			(acceptname) ? acceptname : rejectname, NULL);
	rv = IPFERR_MALLOC;
	goto error_ret;
    }

    /* Add this to the list of filters */
    ipf->ipf_next = filters;
    filters = ipf;

    /* Indicate filter created */
    return 1;

  error_ret:
    /*
     * Our assumption here is that our caller is just going to log our
     * error and keep going.  Our caller may in fact get far enough
     * to make calls to dns_filter_check() for the current client,
     * in which case, we want ip_filter_check() to return an error
     * code to indicate that a filter was specified but not applied,
     * due to the current error condition.  So we add a special
     * "ipfilter=?" parameter to the client, which ip_filter_check()
     * can recognize as a broken filter.
     */
    pp = pblock_nvinsert("ipfilter", "?", client);
    return rv;
}

/*
 * Description (ip_filter_check)
 *
 *	This function checks a client parameter block for a "ipfilter"
 *	parameter.  If present, its value will point to a IPFilter_t
 *	structure, and the client's IP address will be checked against this
 *	filter.
 *
 * Arguments:
 *
 *	client		- client parameter block pointer
 *	cip		- client IP address value
 *
 * Returns:
 *
 *	-2	- there was a broken filter indication
 *		  (see "error_ret:" comments above)
 *	-1	- there was a reject filter and the client was rejected,
 *		  or there was only an accept filter and it did not
 *		  accept the client
 *	 0	- there was no filter present, or both filters were present
 *		  and neither matched the client
 *	 1	- there was an accept filter and the client was accepted,
 *		  or there was only a reject filter and it did not
 *		  reject the client
 */

NSAPI_PUBLIC int ip_filter_check(pblock * client, unsigned long cip)
{
    IPFilter_t * ipf;		/* IP filter structure pointer */
    char * fname;		/* filter name */
    IPLeaf_t * leaf;		/* radix tree leaf pointer */
    int disp = 0;		/* return value */

    fname = pblock_findval("ipfilter", client);
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
    ipf = (IPFilter_t *)objndx_lookup(ipf_objndx, fname);
    if (ipf != NULL) {

	/* Yes, look for a match on the client IP address */
	leaf = ip_filter_search(cip, ipf->ipf_tree);
	if (leaf == NULL) {
	    /*
	     * There was no information for the client IP in the table.
	     * If there is only a ipaccept file, but no ipreject file,
	     * figure that the client should be rejected.  On the other
	     * hand, if there is only a ipreject file, but no ipaccept
	     * file, figure that the client should be accepted.
	     */
	    if (ipf->ipf_acceptfile != NULL) {

		/* Only an accept file, no reject file? */
		if (ipf->ipf_rejectfile == NULL) {

		    /* Reject client */
		    disp = -1;
		}
	    }
	    else if (ipf->ipf_rejectfile != NULL) {

		/* Accept client - no accept file, but reject file present */
		disp = 1;
	    }
	}
	else {

	    disp = (leaf->ipl_disp == IPL_ACCEPT) ? 1 : -1;
	}
    }

    return disp;
}
#endif /* NOACL */
