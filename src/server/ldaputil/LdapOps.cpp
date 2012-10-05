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

#include "LdapSession.h"
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include "libaccess/LdapRealm.h"
#include "LinkedList.hh"
#include "base/util.h"
#include <ldap.h>
#include <ldaputil/certmap.h>
#include <ldaputil/errors.h>
#include <ldaputili.h>
#include <plstr.h>
#include "definesEnterprise.h"
#include "support/NSString.h"
#include "support/NSTokenizer.h"
#include "base/ereport.h"

#define LDAP_METACHARS       "*&|!()=<>"

int ldapu_str_append(LDAPUStr_t *lstr, const char *arg);
LDAPUStr_t *ldapu_str_alloc (const int size);
void ldapu_str_free (LDAPUStr_t *lstr);

/* If we are not interested in the returned attributes, just ask for one
 * attribute in the call to ldap_search.  Also don't ask for the attribute
 * value -- just the attr.
 */
static const char *default_search_attrs[] = { "c" , 0 };
static int default_search_attrsonly = 1;

static const int MAX_BUFFER = 1024;

/*
 * Check if the attribute is a "common name"/grouptargetAttr name or a subtype thereof
 *	Arguments:
 *	    attr_desc		an LDAP attribute name
 *	Return Values:
 *	    LDAPU_SUCCESS	attribute is a "common name"
 *	    LDAPU_FAILED	attribute is NOT a "common name"
 *	    LDAPU_ERR_OUT_OF_MEMORY
 *				a memory allocation failed.
 */
static int
is_groupTargetAttr(char* attr_desc,const char* groupTargetAttr)
{
    auto int retval = LDAPU_FAILED;
    auto char* desc = strdup (attr_desc);
    if (desc == NULL) {
	retval = LDAPU_ERR_OUT_OF_MEMORY;
    } else {
	auto const char* brk  = " ;";
	auto char* next = NULL;
	auto char* attr = ldap_utf8strtok_r (desc, brk, &next);
	if (attr && (!ldapu_strcasecmp(attr, groupTargetAttr) ||
		     !ldapu_strcasecmp(attr, "commonName"))) {
	    auto char* option;
	    retval = LDAPU_SUCCESS; /* attribute type is correct */
	    while (option = ldap_utf8strtok_r (NULL, brk, &next)) {
		if (!ldapu_strcasecmp(option, "binary")) {
		    retval = LDAPU_FAILED; /* not a subtype */
		    break;
		}
	    }
	}
	free (desc);
    }
    return retval;
}


/*
 * Make sure only one group exists with the same CN
 *
 * to find out, we search for the CN of the group (while making sure we hit groups
 * only). If we get multiple matches, it's over.
 *
 *	Arguments:
 *	    entry		the winner candidate group's entry
 *	    baseDN		Search base DN for finding the group
 *	    group		the cn of the group we were looking for
 *	Return Values:
 *	    LDAPU_SUCCESS	the group is unique
 *	    LDAPU_FAILED	we could not find the group anymore
 *	    LDAPU_ERR_MULTIPLE_MATCHES
 *				there are multiple groups with cn="group"
 */
int
LdapSession::group_is_unique(LdapEntry * entry, const char* baseDN, const char* group)
{
    LdapSearchResult *res;
    int rv;
    char	filter[MAX_BUFFER];
    int		retval;

    // setup the filter
    // we'll try to find an object that has cn == groupid and any of the group
    // object classes
    PL_strcpy(filter, "(&");
    PL_strcatn(filter, sizeof(filter),"(");
    PL_strcatn(filter, sizeof(filter),ldapRealm->getGroupTargetAttr());
    PL_strcatn(filter, sizeof(filter),"=");
    PL_strcatn(filter, sizeof(filter), group);
    PL_strcatn(filter, sizeof(filter), ")"
                      "(|"
                      "(objectclass=groupofuniquenames)"
                      "(objectclass=groupofnames)"
                      "(objectclass=groupofurls)"
                      "(objectclass=groupofcertificates)"
                      "(objectclass=group)"
                      ")"
                      ")");

    rv = find(baseDN, LDAP_SCOPE_SUBTREE, filter, default_search_attrs, default_search_attrsonly, res);
    if (rv == LDAPU_SUCCESS) {
	// we have only one group with this name (otherwise LDAPU_ERR_MULTIPLE_MATCHES
        // would have been returned) -- but is it the same as one for which the membership
        // check succeeded?
	LdapEntry *newentry = res->next();
	char *newdn = newentry->DN();
	char *olddn = entry->DN();

	if (strcmp(newdn, olddn)) {
	    // The group for which we had success got deleted
	    // and we found another one with the same CN
	    rv = LDAPU_ERR_MULTIPLE_MATCHES;
	}
	ldap_memfree(olddn);
	ldap_memfree(newdn);

	// free up the object
	delete newentry;
    }
    delete res;
    return rv;
}

/*
 * Check if the argument group is one of the groups we're looking for
 *	Arguments:
 *	    entry		the candidate group's entry
 *	    groups		Some representation of a set of groups.
 *				E.g. a comma-separated list of names in a string,
 *				a hash table, a pattern, etc.
 *	    grpcmpfn		Function to decide whether a group is in the set.
 *	    baseDN		Search base DN.  This function can find groups in
 *				the directory subtree rooted at this entry, but
 *				not elsewhere in the directory.
 *	    cn			if not equal to 0 and successful, get set to pointer to the CN of
 *				the winning group
 *	    test_unique		do the uniqueness check.
 *	Return Values:
 *	    LDAPU_SUCCESS	entry is a group in the list of groups we're looking for
 *	    LDAPU_FAILED	entry is not a group in the list of groups we're looking for
 *	    LDAPU_ERR_MULTIPLE_MATCHES
 *	    			entry is a group in the list of groups we're looking for, but
 *				there are multiple groups with its cn...
 */
int
LdapSession::match_groups(LdapEntry *entry, void *groupids, LDAPU_GroupCmpFn_t groupcmpfn, const char *baseDN, char **cn, int test_unique)
{
    /* Iterate over all the attributes of the entry: */
    char* attr;
    int rv = LDAPU_FAILED;

    entry->reset();
    while (rv == LDAPU_FAILED  && (attr = entry->next_name()) != NULL) { 
	if (is_groupTargetAttr(attr,ldapRealm->getGroupTargetAttr()) != LDAP_SUCCESS) {
	    // no CN or one of its subtypes - skip it
	    ldap_memfree(attr);
	    continue;
	}

	// now see if we have this one in our list of groups

	// BTW: there may be multiple values. We need to check each one...
	LdapValues *vals = entry->values(attr);
	if (!vals) { // should not be null here
	    ldap_memfree(attr);
	    return rv;
	}
	for (int i = 0; rv == LDAPU_FAILED && (*vals)[i] != NULL; ++i) {
	    const size_t groupLen = (*vals)[i]->bv_len;
	    if ((rv = groupcmpfn(groupids, (*vals)[i]->bv_val, groupLen)) == LDAPU_SUCCESS) {
		// found it! YES!
		// However, we must check for the match being unique
		// if it's not, we fail the lookup with LDAPU_ERR_MULTIPLE_MATCHES
		if (test_unique)
		    rv = group_is_unique(entry, baseDN, (*vals)[i]->bv_val);
		if (cn && rv == LDAPU_SUCCESS)
		    *cn = strdup((*vals)[i]->bv_val);
		break;
	    }
	}
	delete vals;
	ldap_memfree(attr);
    }
    return rv;
}

/*
 * Helper function for constructing a filter for static group member search
 *	Arguments:
 *	    m1			list of DNs to include in filter
 *	    filter		if successful, points to filter string
 *	Return Values:
 *	    LDAPU_SUCCESS	filter constructed successfully
 *	    LDAPU_FAILED	filter construction failed
 *	    LDAPU_ERR_OUT_OF_MEMORY
 *	    			a memory allocation failed
 */
static int
_construct_group_filter (LdapRealm *pLdapRealm,LdapDNList &m1, char** filter)
{
    LDAPUStr_t* uniqueNames = ldapu_str_alloc (1024);
    LDAPUStr_t* names = ldapu_str_alloc (1024);
    int err = (uniqueNames && names) ? LDAPU_SUCCESS : LDAPU_ERR_OUT_OF_MEMORY;
    void *iter;
    const char *dn;
    char *buffer;
    size_t bufsize;

    if (err == LDAPU_SUCCESS) {
        // add group to filter
        for (iter = m1.first(); iter != NULL; iter = m1.next(iter)) {
            dn = m1.item(iter);
            // "3" because  the escaping mechanism replace one char by 3 
            // escaped character, this will ensure that we have enough space 
            // to escape all characters
            bufsize = 3 * (strlen(dn)) + 1;
            buffer = (char *)ldapu_malloc(bufsize);
            if (!buffer) {
                err = LDAPU_ERR_OUT_OF_MEMORY; 
                break;
            }
            int rc = ldap_create_filter(buffer, bufsize, "%e",NULL,NULL,NULL,(char *)dn,NULL);
            if (rc != LDAP_SUCCESS) {
                ereport(LOG_FAILURE, "ldap_create_filter for DN [%s]: failed(%s)\n", dn, ldap_err2string(rc));
                ldapu_str_free (uniqueNames);
                ldapu_str_free (names);
                ldapu_free (buffer);
                return rc;
            }
            /*
            if (((err = ldapu_str_append(uniqueNames, "(uniquemember=")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, buffer)) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, ")")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, "(member=")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, buffer)) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, ")")) != LDAPU_SUCCESS)) {
                ldapu_free (buffer); 
                break;
            }
            */
            const char* grpFilter=pLdapRealm->getGroupSearchFilter();
            if (((err = ldapu_str_append(uniqueNames, "(")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, grpFilter )) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, "=")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, buffer)) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(uniqueNames, ")")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, "(member=")) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, buffer)) != LDAPU_SUCCESS) ||
                ((err = ldapu_str_append(names, ")")) != LDAPU_SUCCESS)) {
                ldapu_free (buffer);
                break;
            }
            ldapu_free (buffer);
        }
    }

    if (err == LDAPU_SUCCESS) {
        auto const char* fmt =
          "(|(&(objectclass=groupofuniquenames)(|%s))"
              "(&(objectclass=group)(|%s))"
            "(&(objectclass=groupofnames)(|%s)))";
        size_t fmtlen = strlen(fmt);
        *filter = (char*)ldapu_malloc (uniqueNames->len + 2* names->len + fmtlen);
        if (!*filter)
            err = LDAPU_ERR_OUT_OF_MEMORY;
        else
            sprintf (*filter, fmt, uniqueNames->str, names->str,names->str);
    }

    ldapu_str_free (uniqueNames);
    ldapu_str_free (names);
    return err;
}

#if defined(FEAT_DYNAMIC_GROUPS)
/*
 * Helper function for constructing a filter for dynamic group member search
 */
static int
_construct_dynamic_group_filter(LdapRealm* ldapRealm,NSString& dynamicGrpFilter, const char* groups,dyngroupmode_t dyngroupmode)
{
    dynamicGrpFilter.append("&(objectClass=groupOfURLs)(memberURL=*)");
    if (dyngroupmode == DYNGROUPS_FAST) { 
            //to get DYNGROUPS_FAST, config "ldapdb: dyngroups fast"
        NSTokenizer tokens(groups,",");
        const char* t=NULL;
        NSString tmp;
        while ( (t=tokens.next())!=NULL) {
            //t.strip(NSString::BOTH,',');
            tmp.append("(");
            tmp.append(ldapRealm->getGroupTargetAttr());
            tmp.append("=");
            tmp.append(t);
            tmp.append(")");
        }
        if (tmp.length()>0) {
            dynamicGrpFilter.append("(|");
            dynamicGrpFilter.append(tmp);
            dynamicGrpFilter.append(")");
        }
    }
    dynamicGrpFilter.prepend("(");
    dynamicGrpFilter.append(")");
    return 0;
}


/*
 * Helper function: parse dynamic group's memberURL attribute
 *	Arguments:
 *	    url			pointer to a LDAP url
 *	    filter		if successful, points to the filter component of the LDAP url
 *				if no filter component was found, return "(objectclass=*)"
 *	    basedn		if successful, points to the basedn component of the LDAP url or NULL
 *	    scope		if successful, contains the scope component of the LDAP url
 *          (free filter and basedn with ldapu_free)
 *	Return Values:
 *	    LDAPU_SUCCESS	the parsing was successful
 *	    LDAPU_ERR_URL_PARSE_FAILED
 *				the parsing failed
 */
int
parse_memberURL(char *url, char **filter, char **basedn, int *scope)
{
    char *urlfilter;
    LDAPURLDesc *pLUD;

    if (ldap_url_parse(url, &pLUD) != LDAP_SUCCESS) {
	return LDAPU_ERR_URL_PARSE_FAILED;
    }
    // NOTE: we IGNORE <host> and <port> in the LDAP URL for now
    //       we will always ignore <attributes>
    // we should *fail* if any of those are present
    if (pLUD->lud_dn) {
	*basedn = (char *)ldapu_malloc(strlen(pLUD->lud_dn) + 1);
	strcpy(*basedn, pLUD->lud_dn);
    } else {
	*basedn = NULL;
    }
    *scope = pLUD->lud_scope;

    if (pLUD->lud_filter) {
	int l = strlen(pLUD->lud_filter);
	// we need to check if it's surrounded by parentheses
	// if not, add some
	if (pLUD->lud_filter[0] != '(' || pLUD->lud_filter[l-1] != ')') {
	    *filter = (char *)ldapu_malloc(strlen(pLUD->lud_filter) + 3);
	    strcpy(*filter + 1, pLUD->lud_filter);
	    (*filter)[0] = '(';
	    (*filter)[l+1] = ')';
	    (*filter)[l+2] = '\0';
	} else {
	    *filter = (char *)ldapu_malloc(strlen(pLUD->lud_filter) + 1);
	    strcpy(*filter, pLUD->lud_filter);
	}
    } else {
	// empty filter - ho hum.
	*filter = NULL;
    }
    ldap_free_urldesc(pLUD);
    return LDAPU_SUCCESS;
}

/*
 * Check if any of the DNs is member in a dynamic group
 *	Arguments:
 *	    DNs			A list of DNs
 *	    dyngroup		the dynamic group to check
 *	    baseDN		Search base DN.  This function can find groups in
 *				the directory subtree rooted at this entry, but
 *				not elsewhere in the directory.
 *	Return Values:
 *	    LDAPU_SUCCESS	one or more of the DNs are members of the dynamic group
 *	    LDAPU_FAILED	none of the DNs are member of the dynamic group
 *	    LDAPU_ERR_URL_PARSE_FAILED
 *				parsing one of the dynamic groups's URLs failed
 */
//
// checks if the dynamic group dyngroup matches any of the DNs
//
int
LdapSession::search_dyngroup(LdapDNList &DNs, LdapEntry *dyngroup, const char *basedn)
{
    LdapValues* vals;
    LdapSearchResult* nres;
    char *filter, *urlbasedn;
    int urlscope;
    const char *attrs[] = { "c", 0 };

    vals = dyngroup->values("memberURL");
    if (!vals || vals->length() == 0) {
	// no memberURL attribute? that means no matches for that group...
	delete vals;
	return LDAPU_FAILED;
    }

    // ok, we have some of those
    // try them until we have a match
    for (int i=0; (*vals)[i] != NULL; i++) {
	void *iter;
	int rv;
	int baseDNnRDNs;

	// 
	// parse memberURL value -> urlfilter, urlbasedn, urlscope
	if ((rv = parse_memberURL((*vals)[i]->bv_val, &filter, &urlbasedn, &urlscope)) != LDAPU_SUCCESS) {
	    // most likely LDAPU_ERR_URL_PARSE_FAILED
	    // don't let one malformed URL make all group lookups fail
	    if (rv == LDAPU_ERR_URL_PARSE_FAILED) {
		continue;
	    } else {
		delete vals;
		return rv;
	    }
	}

	if (filter == NULL)
	    // searching without a filter does not exactly make sense
	    continue;

	if (urlbasedn == NULL)
	    // the default value would be the base dn of the LDAP session, I suppose
	    urlbasedn = strdup(basedn);

	// normalize the URL baseDN to ignore whitespace, order in multivalued RDNs in the suffix check
	baseDNnRDNs = ldapdn_normalize(urlbasedn);

	// search only for DNs where memberURL has a chance to match (urlbasedn must be suffix of DN).
	for (iter = DNs.first(); iter != NULL; iter = DNs.next(iter)) {
	    int ind;
	    char *dn = strdup(DNs.item(iter)); // need to modify...
	    int objDNnRDNs = ldapdn_normalize(dn);

	    if ((ind = ldapdn_issuffix(dn, urlbasedn)) < 0) {
		// no suffix, so no match possible
		ldapu_free(dn);
		continue;
	    }
	    if (urlscope == LDAP_SCOPE_BASE && ind != 0) {
		// dn is not equal to urlbasedn, so a BASE search cannot match
		ldapu_free(dn);
		continue;
	    }
	    if (urlscope == LDAP_SCOPE_ONELEVEL && ((objDNnRDNs - 1) > baseDNnRDNs)) {
		// number of RDNs in the object's DN is greater than one more than
		// the number of RDNs in the search baseDN
		// so a ONELEVEL query cannot match
		ldapu_free(dn);
		continue;
	    }

	    // ok, now we know that dn is ok with the scope rules of memberURL
	    // see if it matches the filter
	    rv = find (dn, LDAP_SCOPE_BASE, filter, attrs, 1, nres);
	    delete nres;
	    ldapu_free(dn);

	    if (rv == LDAPU_SUCCESS) {
		// bingo
		// this group contains one of the DNs
		ldapu_free (filter);
		ldapu_free (urlbasedn); // might be NULL
		delete vals;
		return rv;
	    }
	}
	ldapu_free (filter);
	ldapu_free (urlbasedn);
    }
    delete vals;
    // no match, man!
    return LDAPU_FAILED;
}

#endif /* FEAT_DYNAMIC_GROUPS */

/*
 * Check if any of the memberDNs or certificate is a member of the given set of groups.
 *	Arguments:
 *	    userdn		A DN or NULL.
 *	    certificate		A SECCertificate*, or NULL.
 *	    groups		Some representation of a set of groups.
 *				E.g. a comma-separated list of names in a string,
 *				a hash table, a pattern, etc.
 *	    grpcmpfn		Function to decide whether a group is in the set.
 *	    baseDN		Search base DN.  This function can find groups in
 *				the directory subtree rooted at this entry, but
 *				not elsewhere in the directory.
 *	    recurse		0: Check only groups that contain a DN or cert directly.
 *				1: Also check groups that contain those groups.
 *				2: Also check groups that contain those groups.
 *				etc...
 *	    group_out		if successful, pointer to the CN of a group that
 *				contains either the userDN or certificate.
 *	Return Values: (same as ldapu_find)
 *	    LDAPU_SUCCESS	userdn or certificate is a member of a group
 *	    LDAPU_FAILED	userdn or certificate is not a member of any group
 *	    LDAPU_ERR_MULTIPLE_MATCHES
 *				userdn or certificate is a member of a group, but the
 *				group's cn is not unique
 *	    LDAPU_ERR_CIRCULAR_GROUPS
 *				the recursive search exceeded the allowed number of levels
 *	    LDAPU_ERR_OUT_OF_MEMORY
 *				a memory allocation failed.
 *	    LDAPU_ERR_URL_PARSE_FAILED
 *				parsing one of the dynamic groups's URLs failed
 *	    <rv>		Something went wrong. <rv> can be passed to
 *				ldap_err2string to get an error string.
 */
int
LdapSession::usercert_groupids(const char* userdn,
				void* certificate,
				void* groups,
				LDAPU_GroupCmpFn_t grpcmpfn,
				const char* baseDN,
				int recurse,
				char **group_out)
{
    LdapDNList candidates, newcandidates;
    auto int retval = LDAPU_FAILED;
    CList<LdapEntry> direct_hits;
    CList<LdapEntry> candidate_hits;
    LdapEntry *entry;
    int num_dyngroups = -1;
    LdapSearchResult *dyngroups;
    char *cn;
    dyngroupmode_t dyngroupmode = this->ldapRealm->getDyngroupmode();

    // no binding - will end up in LdapSession::search

    if (certificate != NULL) {
	// first, look through all groupOfCertificates for our cert
	const char *attrs[] = { ldapRealm->getGroupTargetAttr(), "memberCertificateDescription", 0 };
	LdapSearchResult *res;
	auto int err;

	err = find (baseDN, LDAP_SCOPE_SUBTREE,
			  "(&(objectClass=groupOfCertificates)(memberCertificateDescription=*))",
			  attrs, 0 /*attrsonly*/, res);

	if (err == LDAPU_SUCCESS || err == LDAPU_ERR_MULTIPLE_MATCHES) {
	    // ok, we have some groupOfCertificates

	    // look through all groupOfCertificates
	    while ((entry = res->next()) != NULL) {
		LdapValues *vals;
		vals = entry->values("memberCertificateDescription");
		if (!vals || vals->length() == 0) {
		    // no memberCertificateDescription attributes. shouldn't happen.
		    delete entry;
		    delete vals;
		    continue;
		}

		// look through all the certificates in this group to see if one matches
		for (int i=0; i < vals->length(); i++) {
		    err = ldapu_member_certificate_match (certificate, (*vals)[i]->bv_val);
		    if (err != LDAPU_FAILED) break;
		}
		delete vals;

		if (err == LDAPU_SUCCESS) {
		    char *dn;

		    // one of the certificates in this group matched!
		    // now check if we're looking for that group
		    // if we do, we're done.
		    // if not, we add it to the list of candidate groups
		    switch (err = match_groups (entry, groups, grpcmpfn, baseDN, &cn, 1)) {
		    case LDAPU_SUCCESS:
			// found one, and it's unique
			*group_out = cn;
			break;
		    case LDAPU_FAILED:
			// not found, add to candidates, then try next entry...
			dn = entry->DN();
			candidates.add(dn);
			ldap_memfree (dn);
			break;
		    case LDAPU_ERR_MULTIPLE_MATCHES:
			// found, but the group is not unique - this is a fatal error
		    default:
			// out with an error...
			break;
		    }
		}
		delete entry;
		if (err != LDAPU_FAILED) {
		    delete res;
		    candidates.clear();
		    return err; /* either SUCCESS or something went wrong */
		}
	    }
	}
	delete res;
	if (userdn == NULL) {
	    /* the certificate search was one recursion level */
	    --recurse; 
	}
    }

    // some IDEAS
    // it would be nice to have a way to specify the max. recursion
    // possibly, we could do async searches on the dynamic groups.
    // there should be a way to specify the "roleSpace" (baseDN of a tree containing dynamic
    // groups) so that we do not have to search through ALL dynamic groups in the baseDN
    // Do loop detection earlier by adding all candidates to a set of candidates. If we try
    // to add a group that's on the list already, we have a loop
    // this will also give us a list of groups the user is member of - might be useful for
    // ACL caching

    if (userdn)
	candidates.add(userdn);

    for (; recurse >= 0  && !candidates.is_empty(); --recurse)
    {
	// we're still recursing, and the list of candidates is not empty
	auto const char	*attrs[] = { ldapRealm->getGroupTargetAttr(), 0 };
	LdapSearchResult *res;
	auto char* filter;
	auto int rv;

	newcandidates.clear();
	// ############################################################################
	// first, look for STATIC groups who have members equal to one of the
	// candidate DNs.
	// ############################################################################
	rv = _construct_group_filter(ldapRealm,candidates, &filter);
	if (rv != LDAPU_SUCCESS) {
	    retval = rv; /* failed to construct filter */
	    goto clean_up_the_mess;
	}
	// do the search
	rv = find(baseDN, LDAP_SCOPE_SUBTREE, filter, attrs, 0 /*attrsonly*/, res);
	ldapu_free(filter);

	if (rv == LDAPU_SUCCESS || rv == LDAPU_ERR_MULTIPLE_MATCHES) {
	    // found some!
	    // now see if these groups have CNs that match the list of groups we're looking for
	    while ((entry = res->next()) != NULL) {
		char *dn;
		switch (rv = match_groups (entry, groups, grpcmpfn, baseDN, &cn, 1)) {
		case LDAPU_FAILED:
		    dn = entry->DN();
		    newcandidates.add(dn);
		    ldap_memfree (dn);
		    break;
		case LDAPU_SUCCESS:
		    // found one already
		    if (group_out)
			*group_out = cn;
		    /* FALL THROUGH */
		case LDAPU_ERR_MULTIPLE_MATCHES:
		default:
		    // oops - we've run into a problem
		    delete entry;
		    delete res;
		    retval = rv;
		    goto clean_up_the_mess;
		}
		delete entry;
	    }
	}
	delete res;

#ifdef FEAT_DYNAMIC_GROUPS

	// ################################################################
	// We're still here, so no success with the static groups this time
	// now check the dynamic groups
	// ################################################################

	if (num_dyngroups < 0 && dyngroupmode != DYNGROUPS_OFF) {
	    // have not enumerated the dynamic groups yet.
	    // do it now.
	    // we do this lazily because if we happen to have a hit with static groups
	    // we don't need to go through this.
	    const char *dyngroupattrs[] = { ldapRealm->getGroupTargetAttr(), "memberURL", 0 };
	    int err, rv;

	    // a dynamic group must have an objectClass attribute of "groupOfURLs"
	    // and some memberURL attributes as well
	    // BTW: we could limit our search for dynamic groups to a special DN
            NSString dynamicGrpFilter;
            _construct_dynamic_group_filter(ldapRealm,dynamicGrpFilter,(const char*)groups,dyngroupmode);
            err = find (baseDN, LDAP_SCOPE_SUBTREE,
			    dynamicGrpFilter.data(),
			    dyngroupattrs, 0, dyngroups);

	    switch (err) {
	    case LDAPU_SUCCESS:
	    case LDAPU_ERR_MULTIPLE_MATCHES:
		// found some dynamic groups
		// looks like we're in for some REAL work now.
		num_dyngroups = dyngroups->entries();

		// check each of the cn attributes of those groups against
		// the list of groups we're looking for.
		while (entry = dyngroups->next()) {
		    // do NOT test every dynamic group for uniqueness here...
		    switch (rv = match_groups(entry, groups, grpcmpfn, baseDN, NULL, 0)) {
		    case LDAPU_SUCCESS:
			// one of the groups we're looking for - try them first in subsequent searches
			direct_hits.Append(entry);
			break;
		    case LDAPU_FAILED:
			// not one of the groups we're looking for
			candidate_hits.Append(entry);
			break;
		    default:
			delete entry;
			retval = rv;
			goto clean_up_the_mess;
		    }
		    // do NOT delete the entries
		}
		break;
	    case LDAPU_FAILED:
		// no dynamic groups in baseDN
		num_dyngroups = 0;
		break;
	    default:
		// something went wrong
		retval = err;
		goto clean_up_the_mess;
	    }
	}

	if (num_dyngroups > 0) {
	    // first, search the direct hits list
	    // if we find one of the DNs we're looking for, it's celebration time
	    int rv;
	    char *dn;

	    CListIterator<LdapEntry> it1 (&direct_hits);
	    while (entry = ++it1) {
		rv = search_dyngroup(candidates, entry, baseDN); 
		switch (rv) {
		case LDAPU_SUCCESS:
		    // we had a match with a group we're looking for
		    // so looks like we have a winner
		    // match groups again to get at CN and test for uniqueness
		    switch (rv = match_groups(entry, groups, grpcmpfn, baseDN, &cn, 1)) {
		    case LDAPU_FAILED:
			// oops? That really should not happen.
			break;
		    case LDAPU_SUCCESS:
			if (group_out)
			    *group_out = cn;
			/* FALL THROUGH */
		    case LDAPU_ERR_MULTIPLE_MATCHES:
		    default:
			retval = rv;
			goto clean_up_the_mess;
		    }
		    break;
		case LDAPU_FAILED:
		    // no match - next group.
		    break;
		default:
		    // something went wrong
		    retval = rv;
		    goto clean_up_the_mess;
		    break;
		}
	    }

	    // then, search the candidates list
	    // if we find one of the DNs we're looking for, we add it to the new candidates.
	    CListIterator<LdapEntry> it2 (&candidate_hits);
	    while (entry = ++it2) {
		rv = search_dyngroup(candidates, entry, baseDN); 
		switch (rv) {
		case LDAPU_SUCCESS:
		    // we had a match with a group we're NOT looking for
		    // just add it to the candidates list
		    dn = entry->DN();
		    newcandidates.add(dn);
		    ldap_memfree (dn);
		    break;
		case LDAPU_FAILED:
		    // no match - next group.
		    break;
		default:
		    // something went wrong
		    retval = rv;
		    goto clean_up_the_mess;
		    break;
		}
	    }
	    // no dyngroups search after the first round
	    // except if user explicitely wants recursive dyngroups
	    if (dyngroupmode != DYNGROUPS_RECURSIVE)
		num_dyngroups = 0;
	}

#endif /* FEAT_DYNAMIC_GROUPS */

        // replace candidates list with newcandidates
	candidates.clear();
	candidates.append(newcandidates);
    }

    // if we come here (X), it means we have either
    // - done all the allowable recursion and still not found one of our groups
    // or
    // - end up with an empty candidate list
    if (recurse < 0)
	retval = LDAPU_ERR_CIRCULAR_GROUPS;
    else
	retval = LDAPU_FAILED;

clean_up_the_mess:
    candidates.clear();
    newcandidates.clear();

#ifdef FEAT_DYNAMIC_GROUPS

    if (num_dyngroups >= 0) {
	CListIterator<LdapEntry> direct_iterator(&direct_hits);
	CListIterator<LdapEntry> candidate_iterator(&candidate_hits);
	while (entry = ++direct_iterator)
	    delete entry;
	while (entry = ++candidate_iterator)
	    delete entry;
	delete dyngroups;
    }

#endif /* FEAT_DYNAMIC_GROUPS */
	
    return retval;
}

/*
 * Check if the userdn is in any one of the given set of roles.
 *	Arguments:
 *	    userdn		A DN or NULL.
 *	    roles		a comma-separated list of role names.
 *	    baseDN		Search base DN for finding roles. This function can find roles in
 *				the directory subtree rooted at this entry, but
 *				not elsewhere in the directory.
 *	    role_out		if successful, pointer to the CN of the first role that uderDN is in.
 *	Return Values: (same as ldapu_find)
 *	    LDAPU_SUCCESS	userdn or certificate is in at least one of the role
 *	    LDAPU_FAILED	userdn or certificate is not in any role
 *	    LDAPU_ERR_MULTIPLE_MATCHES
 *				userdn or certificate is a member of a group, but the
 *				group's cn is not unique
 *	    LDAPU_ERR_OUT_OF_MEMORY
 *				a memory allocation failed.
 *	    <rv>		Something went wrong. <rv> can be passed to
 *				ldap_err2string to get an error string.
 */
int
LdapSession::user_roleids(const char *userdn, const char *roles, const char* baseDN, char **role_out)
{
    LdapSearchResult *res;
    LdapEntry *entry;
    char *roledn;
    char *rolesbuf, *role;
    char *lasts;
    const char *attrs[] = { "C", 0 };
    char filterbuf[512];
    int rv;

    // no binding - will end up in LdapSession::search

    // for all roles
    //    find DN of role. basedn = basedn, filter = (&(objectclass=nsRole)(cn=rolename))
    //    check if role is unique - if not, continue
    //    do base search of userdn where filter = "nsRoleDN=<roleDN>"
    //    if SUCCESS, done.
    //
    if ((rolesbuf = strdup(roles)) == NULL)
        return LDAPU_ERR_OUT_OF_MEMORY;

    for (role = util_strtok(rolesbuf, ", \t", &lasts); role != NULL; role = util_strtok(NULL, ", \t", &lasts)) {
        // construct the filter
        // (&(objectclass=nsRole)(cn=<role>))
        PL_strcpy(filterbuf, "(&(objectclass=nsroledefinition)(|(objectclass=ldapsubentry))(cn=");
        PL_strcatn(filterbuf, sizeof(filterbuf), role);
        PL_strcatn(filterbuf, sizeof(filterbuf), "))");

	// do the search
	rv = find(baseDN, LDAP_SCOPE_SUBTREE, filterbuf, attrs, 1 /*attrsonly*/, res);
        if (rv == LDAPU_ERR_MULTIPLE_MATCHES) {
            // we had multiple matches - that's a configuration error. fail the whole thing.
            delete res;
            free(rolesbuf);
            return LDAPU_ERR_MULTIPLE_MATCHES;
        }
        if (rv != LDAPU_SUCCESS) {
            // forget about this one
            // either we did not find it, or something went wrong
            delete res;
            continue;
        }
        if ((entry = res->next()) == NULL) {
            // this should never happen - rv is LDAPU_SUCCESS after all.
            delete res;
            continue;
        }

        // get the DN of the role
        roledn = entry->DN();

        delete entry;
        delete res;

        // normalize it
        (void)ldapdn_normalize(roledn);

        // NB: we have to do separate searches because we want to know which one matched
        // so collecting all the role names and then doing a base search with
        // (|(nsRoleDN=role1)(nsRoleDN=role2)...) would not cut it

        // construct the filter
        rv = compare( userdn, "nsRole", roledn, 0 );
        if (rv == LDAP_COMPARE_TRUE) {
            *role_out = strdup(role);
            rv = LDAPU_SUCCESS;
	    ldap_memfree(roledn);
            break;
        } else {
            rv = LDAPU_FAILED;
        }

        ldap_memfree(roledn);
    }
    free(rolesbuf);
    return rv;
}

/*
 *  userdn_digest:
 *      Description:
 *          Checks the user's password against LDAP by binding using the
 *          userdn and the password.
 *      Arguments:
 *          userdn              User's full DN
 *          nonce               nonce
 *          cnonce
 *          realm               usually user@host
 *          passattr            attribute the password is stored in
 *          alg                 MD5 for now
 *          noncecount          count for nonce validity (NOT USED)
 *          method              HTTP Method
 *          qop                 for tighter security (NOT USED)
 *          uri                 the requested URI
 *          cresponse           the client response digest, contains hashed pwd
 *    
 *      Return Values: (same as ldapu_find)
 *          LDAPU_SUCCESS       if user credentials are valid
 *          <rv>                if error, where <rv> can be passed to
 *                              ldap_err2string to get an error string.
 */
int
LdapSession::userdn_digest (const char *userdn, const char *nonce, const char *cnonce, const char *user, const char *realm, const char *passattr, const char *alg, const char *noncecount, const char *method, const char *qop, const char *uri, const char *cresponse, char **response)
{
    /* OID of the extended operation that you are requesting */
    const char *oidrequest = "2.16.840.1.113730.3.5.9";
    char *oidresult;
    struct berval valrequest;
    struct berval *valresult;
    int rc = 0;
    LDAPMessage *res = 0;
    struct timeval zerotime;
    int msgid;
    char tmp[4096];

    valrequest.bv_val = NULL;

    /* Create the request in the form of name/value pairs */
    if (qop != NULL)
        util_snprintf(tmp, 4096, "nonce=%s&cnonce=%s&user=%s&realm=%s&passattr=%s&dn=%s&alg=%s&noncecount=%s&method=%s&qop=%s&uri=%s&cresponse=%s", nonce, cnonce, user, realm, passattr, userdn, alg, noncecount, method, qop, uri, cresponse);
    else
        util_snprintf(tmp, 4096, "nonce=%s&user=%s&realm=%s&passattr=%s&dn=%s&alg=%s&method=%s&uri=%s&cresponse=%s", nonce, user, realm, passattr, userdn, alg,  method, uri, cresponse);

    valrequest.bv_val = tmp;
    valrequest.bv_len = strlen(valrequest.bv_val);

    /* Initiate the extended operation */
    ldap_extended_operation(session, oidrequest, &valrequest, NULL, NULL, &msgid);
    zerotime.tv_sec = zerotime.tv_usec = 1L;

    while ( rc == 0 ) {
        /* Check the status of the LDAP operation */
        rc = ldap_result(session, msgid, 0, &zerotime, &res );

        switch( rc ) {
          case -1: /* If -1 was returned, an error occurred */
             boundto = NONE;
             return( LDAPU_ERR_INTERNAL );
          case 0: /* If 0 was returned, the operation is still in progress */
             continue;
          default:  /* If any other value is returned, assume we are done */
              break;
        }
    }

    /* Check if the extended operation was successful */
    lastbindrv = ldap_result2error(session, res, 0);
    int rv = (lastbindrv == LDAP_SUCCESS) ? LDAPU_SUCCESS : lastbindrv;

    if (lastbindrv == LDAP_INVALID_CREDENTIALS)
        return LDAPU_FAILED;

    /* Get the response value from the LDAP server */
    ldap_parse_extended_result(session, res, &oidresult, &valresult, 1);
    if (valresult!=NULL)
        *response = STRDUP(valresult->bv_val);

    return rv;
}

/*
 *  userdn_password:
 *	Description:
 *	    Checks the user's password against LDAP by binding using the
 *	    userdn and the password.
 *	Arguments:
 *	    userdn		User's full DN
 *	    password		User's password (clear text)
 *	Return Values: (same as ldapu_find)
 *	    LDAPU_SUCCESS	if user credentials are valid
 *	    <rv>		if error, where <rv> can be passed to
 *				ldap_err2string to get an error string.
 */
int
LdapSession::userdn_password (const char *userdn, const char *password)
{
    int retval;

    LDAPMessage *res = 0;
    int rc = 0;
    struct timeval zerotime;

    // don't care about binding - we'll rebind right now.

    int msgid = ldap_simple_bind(session, userdn, password);

    zerotime.tv_sec = zerotime.tv_usec = 1L;

    while ( rc == 0 ) {
	/* Check the status of the LDAP operation */
        rc = ldap_result(session, msgid, 0, &zerotime, &res );

        switch( rc ) {
          case -1: /* If -1 was returned, an error occurred */
	     boundto = NONE;
             return( LDAPU_ERR_INTERNAL );
          case 0: /* If 0 was returned, the operation is still in progress */
             continue;  
          default:  /* If any other value is returned, assume we are done */
              break;
        }
    }
    /* Check if the "bind" operation was successful */
    lastbindrv = ldap_result2error(session, res, 0);
    boundto = (lastbindrv == LDAP_SUCCESS) ? OTHER : NONE;

    int rv = (lastbindrv == LDAP_SUCCESS) ? LDAPU_SUCCESS : LDAPU_FAILED;

#if defined(FEAT_PASSWORD_POLICIES)

    int errcodep;
    char *matcheddnp;
    char *errmsgp;
    char **referralsp;
    LDAPControl **controls;

    // now look if we have additional information on that
    ldap_parse_result(session, res, &errcodep, &matcheddnp, &errmsgp, 
		      &referralsp, &controls, 1);

    // MASTER KLUDGE - take me out for 4.1!
    // as we don't seem to get the correct controls in case the user's
    // password expired, we'll go with this method
    if ((controls == NULL) &&					// no controls
	(lastbindrv == LDAP_INVALID_CREDENTIALS) &&		// but we could not bind
	(errmsgp != NULL) &&					// have an error string
	(strstr(errmsgp, "expire") != NULL))			// which contains "expire"
    {
	rv = LDAPU_ERR_PASSWORD_EXPIRED;
    }

    for (int i=0; controls && controls[i]; i++){
	//
	// OK, we have may password policies and a problem...
	// make sure we return LDAPU_ERR_PASSWORD_EXPIRED only when the bind failed
	// and LDAPU_ERR_PASSWORD_EXPIRING only when the bind succeeded
	if (controls[i]->ldctl_oid && !strcmp(controls[i]->ldctl_oid, LDAP_CONTROL_PWEXPIRED) && rv == LDAPU_FAILED)
	    rv = LDAPU_ERR_PASSWORD_EXPIRED;
	if (controls[i]->ldctl_oid && !strcmp(controls[i]->ldctl_oid, LDAP_CONTROL_PWEXPIRING) && rv == LDAPU_SUCCESS) {
	    rv = LDAPU_ERR_PASSWORD_EXPIRING;
	    // the number of seconds until expiry is in controls[i]->ldctl_value
	}
    }
    // Free up all our garbage.
    ldap_memfree(matcheddnp);
    ldap_memfree(errmsgp);
    ldap_value_free(referralsp);
    if (controls)
	ldap_controls_free(controls);

#endif /* FEAT_PASSWORD_POLICIES */
    
    return rv; 
}

/*
 * find
 *   Description:
 *	Caller should free res if it is not NULL.
 *   Arguments:
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
int LdapSession::find (const char *base, int scope,
		       const char *filter, const char **attrs,
		       int attrsonly, LdapSearchResult *&res)
{
    int numEntries;

    // no binding - will end up in LdapSession::search

    /* If base is NULL set it to null string */
    if (!base) {
	DBG_PRINT1("LdapSession::find: basedn is missing -- assuming null string\n");
	base = "";
    }

    if (!filter || !*filter) {
	DBG_PRINT1("LdapSession::find: filter is missing -- assuming objectclass=*\n");
	filter = "objectclass=*";
    }
    
    DBG_PRINT2("\tbase:\t\"%s\"\n", base);
    DBG_PRINT2("\tfilter:\t\"%s\"\n", filter ? filter : "<NULL>");
    DBG_PRINT2("\tscope:\t\"%s\"\n",
	       (scope == LDAP_SCOPE_SUBTREE ? "LDAP_SCOPE_SUBTREE"
		: (scope == LDAP_SCOPE_ONELEVEL ? "LDAP_SCOPE_ONELEVEL"
		   : "LDAP_SCOPE_BASE")));

    res = search(base, scope, filter, (char **)attrs, attrsonly, 0);

    switch (res->ldapresult()) {
    case LDAP_SUCCESS:
	break;
    case LDAP_INVALID_CREDENTIALS:
	return LDAPU_ERR_BIND_FAILED;
    case LDAP_SERVER_DOWN:
	return LDAPU_ERR_BIND_FAILED;
    default:
	return res->ldapresult();
    }

    numEntries = res->entries();

    if (numEntries == 1) {
	/* success */
	return LDAPU_SUCCESS;
    } else if (numEntries == 0) {
	/* not found -- but not an error */
	return LDAPU_FAILED;
    } else if (numEntries > 0) {
	/* Found more than one entry! */
	return LDAPU_ERR_MULTIPLE_MATCHES;
    } else {
	/* should never get here */
	return LDAPU_ERR_INVALID;
    }
}

/*
 * find_userdn
 *   Description:
 *	Maps the given uid to a user dn.  Caller should free dn if it is not
 *	NULL. 
 *   Arguments:
 *	ld		Pointer to LDAP (assumes connection has been
 *	    		established and the client has called the
 *	    		appropriate bind routine)
 *	uid		User's name
 *	base		basedn (where to start the search)
 *	dn 		user dn
 *   Return Values:
 *	LDAPU_SUCCESS	if entry is found
 *	LDAPU_FAILED	if entry is not found
 *	<rv>		if error, where <rv> can be passed to
 *			ldap_err2string to get an error string.
 */
int 
LdapSession::find_userdn (const char *uid, const char *base, char **dn)
{
    int		retval;
    LdapSearchResult *res = 0;
    static const char *attrs[] = { "c", 0 };
    static const char *smiattrs[] = { "inetUserStatus", 0 };
    char	filter[ MAX_BUFFER ];
    PRBool      smicompliant = (ldapRealm->getDCSuffix() != NULL);

    if (dn == 0)
        return LDAPU_FAILED;

    *dn = 0;

    // If raw_user contains characters which could be interpreted as special
    // in the ldap search string (RFC 2254), just refuse access (4957829).
    if (strpbrk(uid, LDAP_METACHARS)) {
        return LDAPU_FAILED;
    }
    
    // no binding - will end up in LdapSession::search
    //PL_strcpy(filter, "uid=");
    const char* userSearchFilter = ldapRealm->getUserSearchFilter();
    PL_strcpy(filter, userSearchFilter);
    PL_strcatn(filter, sizeof(filter), "=");
    PL_strcatn(filter, sizeof(filter), uid);
    // we ask for attribute values only if we're SMI compliant
    if (smicompliant)
        retval = find(base, LDAP_SCOPE_SUBTREE, filter, smiattrs, 0 /* get attrs */, res);
    else
        retval = find(base, LDAP_SCOPE_SUBTREE, filter, attrs, 1 /* no attrs */, res);
    if (retval != LDAPU_SUCCESS || !res->good()) {
        delete res;
        return retval;
    }

    LdapEntry *entry = res->next();
    if (smicompliant) {
        // see if we're supposed to receive service
        LdapValues *vals = entry->values("inetUserStatus");

        // just carry on if there's no inetUserStatus
        // but if there is, we'll check if it's "active"
        if (vals && vals->length() != 0) {
            // if we are not active, tell the caller, but signal success otherwise.
            if (PL_strcasecmp((*vals)[0]->bv_val, "active") != 0) {
                delete vals;
                delete entry;
                delete res;
                return LDAPU_ERR_USER_NOT_ACTIVE;
            }
        }
        if (vals)
            delete vals;
    }
    *dn = entry->DN();
    delete entry;
    delete res;

    return retval;
}

//
// LdapSession::cert_to_user - map a certificate to a uid and userDN
//
int
LdapSession::cert_to_user (void *cert, const char *baseDN, char **user, char **dn, char *certmap, int retryCount)
{
    int rv;
    LDAPMessage *res;
    LDAPMessage *entry;
    char **attrVals;
    char *dntmp;

    *user = 0;

    // we are rebinding even if we think we're already bound to default because
    // we're just using ldapu_find() operations in here, and those will not (and cannot)
    // reconnect properly on LDAP server failure. See bug # 533400 for details.
    // Alternatively, we could check the propagated LDAP error code, and reconnect/rebind/retry
    // if it indicates a LDAP server down scenario.

    if (retryCount > maxRetries)
        return LDAPU_ERR_BIND_FAILED;

    /* 
    if ((rv = bindAsDefault()) != LDAP_SUCCESS)
        return LDAPU_ERR_BIND_FAILED;
    */

    rv = bindAsDefault();
    while ( (rv != LDAP_SUCCESS) && (retryCount <= maxRetries) )
         rv = reconnect(retryCount++);
    if (rv != LDAP_SUCCESS)
       return LDAPU_ERR_BIND_FAILED;
       
    // first, try to match the certificate to an ldap entry under the base DN
    /*
    if ((rv = ldapu_cert_to_ldap_entry_with_certmap(cert, session, baseDN, &res, certmap)) != LDAPU_SUCCESS)
	return rv;
    */

    rv = ldapu_cert_to_ldap_entry_with_certmap(cert, session, baseDN, &res, certmap);
    if (serverDown(rv))
    {
        while ( (rv != LDAP_SUCCESS) && (retryCount <= maxRetries) )
            rv = reconnect(retryCount++);
    
        if (rv != LDAPU_SUCCESS)
            return rv;
        if ((rv = ldapu_cert_to_ldap_entry_with_certmap(cert,session, baseDN, &res, certmap)) != LDAPU_SUCCESS)
           return rv;
     }
    
    if (!res)
	return LDAPU_ERR_EMPTY_LDAP_RESULT;

    // must have exactly ONE match
    if (ldap_count_entries(session, res) != 1) {
        ldap_msgfree (res);
        return LDAPU_ERR_MULTIPLE_MATCHES;
    }

    if ((entry = ldap_first_entry(session, res)) == 0) {
        ldap_msgfree (res);
        return LDAPU_ERR_MISSING_RES_ENTRY;
    }

    // now extract the userid 
    attrVals = ldap_get_values(session, entry, "uid");

    if (!attrVals || !attrVals[0]) {
        ldap_msgfree (res);
        return LDAPU_ERR_MISSING_UID_ATTR;
    }

    *user = strdup(attrVals[0]);
    ldap_value_free(attrVals);
    if (!*user) {
        ldap_msgfree (res);
        return LDAPU_ERR_OUT_OF_MEMORY;
    }

    // and the DN
    dntmp = get_dn(entry);
    *dn = (dntmp) ? strdup(dntmp) : 0;
    ldap_memfree(dntmp);
    ldap_msgfree (res);

    return LDAPU_SUCCESS;
}

//
// LdapSession::find_vsbasedn - find a virtual server's baseDN
//
// Parameters:
// vsname    - virtual server name
// vsbasedn  - pointer to buffer for result baseDN
// len       - length of vsbasedn buffer
//
// Return values:
// LDAPU_SUCCESS                - OK, baseDN is in "vsbasedn"
// LDAPU_ERR_NO_SERVERNAME      - for SMI LDAP servers, we need to have a servername in the VS
// LDAPU_ERR_INVALID_ARGUMENT   - a sanity check on the arguments failed
// LDAPU_ERR_OUT_OF_MEMORY      - out of memory
// LDAPU_ERR_WRONG_ARGS         - vsname has less than 2 domain components
// LDAPU_ERR_MISSING_RES_ENTRY  - we could not get a result from the LDAP op
// LDAPU_ERR_DOMAIN_NOT_ACTIVE  - domain was found, but is inactive or deleted
// LDAPU_ERR_MISSING_ATTR_VAL   - mandatory attribute "inetDomainBaseDN" was not found
// LDAPU_ERR_INVALID_STRING     - the result buffer is too short
// LDAPU_ERR_INTERNAL           - something's wrong with the internal logic here
//
int
LdapSession::find_vsbasedn(const char *vsname, char *vsbasedn, int len)
{
    int rv;
    LdapSearchResult* nres;
    LdapEntry *entry;
    char *dcsuffix;
    char dctreeDN[1024];
    char ositree_basedn[1024];
    const char *attrs[] = { "inetDomainBaseDN", "inetDomainStatus", 0 };
    char *pvsn, *vsn, *p, *pdctreeDN;
    int domaincomponents = 0;

    // sanity checks...
    if (vsbasedn == NULL || len <= 0)
        return LDAPU_ERR_INVALID_ARGUMENT;

    if (ldapRealm->getDCSuffix() != NULL && ldapRealm->getBaseDN() != NULL) {
        // we are SMI compliant
        // so let's find out where the virtual server's base is

        if (vsname == NULL || vsname[0] == '\0')
            return LDAPU_ERR_NO_SERVERNAME;

        // first, make a copy of the vsname so we can mangle it
        if ((pvsn = vsn = strdup(vsname)) == NULL)
            return LDAPU_ERR_OUT_OF_MEMORY;

        dctreeDN[0] = '\0';
        do {
            if ((p = strchr(pvsn, '.')) != NULL)
                *p = '\0';
            strcat(dctreeDN, "dc=");
            strcat(dctreeDN, pvsn);
            strcat(dctreeDN, ",");
            pvsn = (p != NULL) ? p + 1 : NULL;
            domaincomponents++;
        } while (pvsn != NULL);

        free(vsn);
        if (domaincomponents < 2) {
            // something's fishy
            return LDAPU_ERR_WRONG_ARGS;
        }
        // dctreeDN has a "," at the end...
        strcat(dctreeDN, ldapRealm->getDCSuffix());
        strcat(dctreeDN, ",");
        strcat(dctreeDN, ldapRealm->getBaseDN());

        // ok, at this point, we have something like
        // "dc=www,dc=foo,dc=com,o=Internet,o=ISP,c=US"

        pdctreeDN = dctreeDN;
        do {
            // check if the entry is there.
	    rv = find (pdctreeDN, LDAP_SCOPE_BASE, "(objectclass=inetDomain)", attrs, 0, nres);
	    if (rv == LDAPU_SUCCESS) {
                // found the entry. if something goes wrong now, we bust.
                // no going back into the loop.
                // 
                // inetDomainBaseDN is what we need to store in vsbasedn
                // if inetDomainStatus is present, but the value is not "active",
                // fail with error message and (in the caller) cache this
                // we'll recheck after a configuration reload...

                // look at the results (just the first match)
                if ((entry = nres->next()) == 0) {
                    delete nres;
                    return LDAPU_ERR_MISSING_RES_ENTRY; // no results
                }

                // see if we're supposed to receive service
                LdapValues *vals = entry->values("inetDomainStatus");
		if (vals && vals->length() != 0) {
                    // if we are not active, tell the caller, but signal success otherwise.
                    PRBool active = (PL_strcasecmp((*vals)[0]->bv_val, "active") != 0) ? PR_FALSE : PR_TRUE;
                    delete vals;
                    if (!active) {
                        delete entry;
                        delete nres;
                        return LDAPU_ERR_DOMAIN_NOT_ACTIVE;
                    }
                } else {
		    // no inetDomainStatus attributes. shouldn't happen.
                    // but carry on, in dubio pro reo and stuff.
		    delete vals;
		}

                // now extract the baseDN 
                vals = entry->values("inetDomainBaseDN");
		if (vals == 0 || vals->length() == 0) {
                    delete vals;
                    delete entry;
                    delete nres;
                    return LDAPU_ERR_MISSING_ATTR_VAL;  // oops - found the entry, but the attr isn't here
                }

                if (strlen((*vals)[0]->bv_val) >= len) {
                    delete vals;
                    delete entry;
                    delete nres;
                    return LDAPU_ERR_INVALID_STRING;
                }

                strcpy(vsbasedn, (*vals)[0]->bv_val);
                delete vals;
                delete entry;
                delete nres;

                // we're done
                return LDAPU_SUCCESS;
            }

            delete nres;

            // did not find an entry in the DC tree.
            rv = LDAPU_ERR_MISSING_RES_ENTRY;

            // strip off first domain component for a more general approach
            // we're guaranteed to have a "," in there, but we'll check anyway to avoid crashing
            if ((pdctreeDN = strchr(pdctreeDN, ',')) == NULL) {
                // something is WRONG
                rv = LDAPU_ERR_INTERNAL;
                break;
            }
            pdctreeDN++;        // skip beyond the ","
        } while (--domaincomponents >= 2);

        // that's it. return whatever we got last.
        return rv;

    } else {

        // we're not SMI compliant
        // just use the server baseDN
        if (ldapRealm->getBaseDN() && strlen(ldapRealm->getBaseDN()) + 1 < len) {
            strcpy(vsbasedn, ldapRealm->getBaseDN());
            return LDAPU_SUCCESS;
        } else
            return LDAPU_ERR_INVALID_STRING;
    }
}
