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

#include "LdapRealm.h"
#include <plstr.h>
#include <ldaputil/ldaputil.h>
#if (defined(__GNUC__) && (__GNUC__ > 2))
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <string.h>
#include <malloc.h>
#include <base/util.h>
#include <stdlib.h>
#include "ldaputil/LdapSessionPool.h"

#include <libaccess/acl.h>
#include "aclpriv.h"
#include <libaccess/aclproto.h>
#include <libaccess/dbtlibaccess.h>
#include <libaccess/aclglobal.h>
#include <libaccess/aclerror.h>
#include <libaccess/digest.h>
#include "usrcache.h"

#include <libaccess/ldapacl.h>
#include <libaccess/LdapRealm.h>

NSString LdapRealm::NSSDBPwd;
NSString LdapRealm::NSSDBCertdir;

static int getStrIdx(GenericVector& v, char* str)
{
    int length = v.length();
    for (int i = 0; str && i<length; i++) {
        if ( PL_strcmp( (char*)v[i],str) ==0 ) return i;
    }
    return -1;
}


LdapServerEntry::LdapServerEntry(const char* _hostname,int _port, int _ldapOverSSL)
{
  hostname = _hostname? strdup(_hostname) : 0;
  port = _port?_port:_ldapOverSSL?636:389;
 
}


LdapServerEntry::~LdapServerEntry(void) 
{
  if (hostname) free(hostname);
}


const int LdapRealm::maxEntries = 64;



LdapRealm::LdapRealm(const char* _rawUrl,
                     const char* _bindName,
                     const char* _bindPwd,
                     const char* _dcSuffix,
                     dyngroupmode_t  _dynGrpMode,
                     int _nsessions,
                     int _digestAuth,
                     int _timeout,
                     const char* _authMethod,
                     const char* _certName,
                     const char* _userSearchFilter,
                     const char* _groupSearchFilter,
                     const char* _groupTargetAttr)
{
    setRealmType(ACL_DbTypeLdap);

    defport =0;
    rawUrl  =_rawUrl?strdup(_rawUrl):0;
    bindName=_bindName?strdup(_bindName):0;
    bindPwd =_bindPwd?strdup(_bindPwd):0;
    dcSuffix=_dcSuffix?strdup(_dcSuffix):0;
    dynGrpMode=_dynGrpMode;
    nsessions = _nsessions;
    digestAuth = _digestAuth;
    timeout = _timeout;
    ldapOverSSL = 0;
    authMethod=_authMethod?strdup(_authMethod):0;
    clientCertNickName=_certName?strdup(_certName):0;
    baseDN=0;
    userSearchFilter =_userSearchFilter?strdup(_userSearchFilter):strdup("uid");
    groupSearchFilter=_groupSearchFilter?strdup(_groupSearchFilter):strdup("uniquemember");
    groupTargetAttr  =_groupTargetAttr?strdup(_groupTargetAttr):strdup("cn");
    entries = new LdapServerEntry_ptr[maxEntries];
    currentEntries=0;
}


LdapRealm::~LdapRealm(void)
{
    if (rawUrl)     free(rawUrl);
    if (bindName)   free(bindName);
    if (bindPwd)    free(bindPwd);
    if (baseDN)     free(baseDN);
    if (dcSuffix)   free(dcSuffix);
    if (authMethod) free(authMethod);
    if (clientCertNickName)   free(clientCertNickName);
    if (userSearchFilter)  free(userSearchFilter);
    if (groupSearchFilter) free(groupSearchFilter);
    if (groupTargetAttr)   free(groupTargetAttr);

    for (int i=0; i < currentEntries; i++)
        delete entries[i];
    delete [] entries;
}


int LdapRealm::init(NSErr_t *errp)
{
    // parse multiple URLs
    char *urlscopy = strdup(rawUrl);
    char *lasts;
    char *url = util_strtok(urlscopy, " \t", &lasts);
    while (url) {
        if (strncasecmp(rawUrl, LDAP_URL_PREFIX, LDAP_URL_PREFIX_LEN) && strncasecmp(rawUrl, LDAPS_URL_PREFIX, LDAPS_URL_PREFIX_LEN)) {
            free(urlscopy);
            nserrGenerate(errp, ACLERRINVAL, ACLERR5820, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclErrorParsingLdapUrl));
            return -1;
        }
        this->addServer(url);
        url = util_strtok(NULL, " \t", &lasts);
    }
    free(urlscopy);

    if (!this->areServersCompatible()) {
        nserrGenerate(errp, ACLERRINVAL, ACLERR5820, ACL_Program, 1, XP_GetAdminStr(DBT_ldapaclErrorIncompatibleDatabases));
        return -1;
    }

    this->ldapSessionPool = new LdapSessionPool(this,10);
    return 0;
}


const char* LdapRealm::prepLdaphosts()
{
    return ldaphosts.data();
}


void LdapRealm::addServer(LdapServerEntry *newEntry)
{
  // this boundary check is not thread safe.
  if (currentEntries >= maxEntries) {
    delete newEntry;
    return;
  }
  if (currentEntries>0)
      ldaphosts.append(" ");
  ldaphosts.append(newEntry->getHost() );
  if (newEntry->getPort()!=0) {
      char buffer[20];
      //itoa(newEntry->getPort(),buffer,10);
      util_itoa(newEntry->getPort(),buffer);
      ldaphosts.append(":");
      ldaphosts.append(buffer);
  }
  // this increment is not thread safe.
  entries[currentEntries++] = newEntry;
  return;
}

void LdapRealm::addServer(const char *url)
{
  LDAPURLDesc *ludp = 0;

  // call ldapsdk's parse function
  if (ldap_url_parse((char *)url, &ludp) != LDAP_SUCCESS) {
    if (ludp) ldap_free_urldesc(ludp);
    return;
  }
  char* _hostname = ludp->lud_host ? strdup(ludp->lud_host) : 0;
  int   _ldapOverSSL = ludp->lud_options & LDAP_URL_OPT_SECURE;
  int   _port     = ludp->lud_port ? ludp->lud_port : _ldapOverSSL ? 636 : 389;
  char* _baseDN   = ludp->lud_dn ? strdup(ludp->lud_dn) : 0;
  ldap_free_urldesc(ludp);

  if (isCompatible(_baseDN,_ldapOverSSL)) {
      this->ldapOverSSL = _ldapOverSSL;
      LdapServerEntry *entry = new LdapServerEntry(_hostname,_port,_ldapOverSSL);
      addServer(entry);
      defport=_port;
  }
  if (count()==1) {
      this->ldapOverSSL = _ldapOverSSL;
      this->baseDN = _baseDN?strdup(_baseDN):0;
  }
  if (_hostname)
    free(_hostname);
  if (_baseDN)
    free(_baseDN);
}


int LdapRealm::count(void) {
  return currentEntries;
}


static int strmatch(const char *s1, const char *s2)
{
  if (s1 == s2)
    return 1;
  if (!s1)
    return 0;
  if (!s2)
    return 0;
  return !strcmp(s1, s2);
}

int LdapRealm::isCompatible(const char *_baseDN,int _ldapOverSSL)
{ 
  if (count()<=0) return 1;

  if (!strmatch(_baseDN,baseDN))
    return 0;
  if (_ldapOverSSL != ldapOverSSL)
    return 0;
  return 1;
}

int LdapRealm::areServersCompatible() {
  return 1;
}


static const char* defaultGroupSearchFilter = 
   "(|(&(objectclass=groupofuniquenames)(|(uniquemember=%s)))"
     "(&(objectclass=groupofnames)(|(uniquemember=%s))) )";



void LdapRealm::addAttrValues(IDType t,GenericVector& vector,LdapValues& attrVals)
{
    for (int i=0; i<attrVals.length(); i++) {
        if (attrVals[i] == NULL)
            continue;
        const size_t len = attrVals[i]->bv_len;
        char *val = attrVals[i]->bv_val;
        char* dn= (char*)MALLOC(attrVals[i]->bv_len + 1 );
        memset(dn,0,attrVals[i]->bv_len + 1);
        PL_strncpy(dn,attrVals[i]->bv_val,attrVals[i]->bv_len);
        const char* userFilter = this->getUserSearchFilter();
        PRBool isUserdn = (PL_strstr(dn,userFilter)==dn);
        if (t==ID_USER) {
            if (isUserdn) {
                if ( getStrIdx(vector,dn) == -1 )
                    vector.append(dn);
            }
            else FREE(dn);
        } else if (t==ID_GROUP) {
            if (!isUserdn) {
                if ( getStrIdx(vector,dn) == -1 )
                    vector.append(dn);
            }
            else FREE(dn);
        } else {
            FREE(dn);
        }
    }
}

void LdapRealm::addEntry(IDType t,GenericVector& vector,LdapEntry& entry)
{
    char* entryDN = entry.DN();
    char* dn=STRDUP(entryDN);
    ldap_memfree(entryDN);

    const char* userFilter = this->getUserSearchFilter();
    PRBool isUserdn = (PL_strstr(dn,userFilter)==dn);
    if (t==ID_USER) {
        if (isUserdn) {
            if ( getStrIdx(vector,dn) == -1)
                vector.append(dn);
        }
        else FREE(dn);
    } else if (t==ID_GROUP) {
        if (!isUserdn) {
            if ( getStrIdx(vector,dn) == -1)
                vector.append(dn);
        }
         else FREE(dn);
    } else {
        FREE(dn);
    }
}



#if 0

Note: See 6210852, 6213541

/*
SCOPE: LDAP_SCOPE_BASE LDAP_SCOPE_ONELEVEL LDAP_SCOPE_SUBTREE
Examples:
ldapsearch -h host -b "cn=j2ee,ou=Groups,dc=red,dc=iplanet,dc=com" \
     "(|(objectclass=groupofuniquenames)(objectclass=groupofnames))"  uniquemember member
ldapsearch -h host -b "cn=j2ee,ou=Groups,dc=red,dc=iplanet,dc=com" \
     "(&(objectclass=person)(cn=j2ee))"   dn
ldapsearch -h host -b "uid=j2ee,ou=People, dc=red,dc=iplanet,dc=com" \
     "(&(objectclass=person)(uid=j2ee))" dn
*/
void LdapRealm::getDirectStaticMemberForGroup(IDType t,char* gid,GenericVector& vector)
{
    LdapSessionPool *sp = this->getSessionPool();
    LdapSession *session = sp->get_session();

    LdapEntry *entry=NULL;
    LdapSearchResult *res=NULL;
    LdapValues *vals=NULL;
    char* pval=NULL;
    int rv=0;

    const char* filter1 =
            "(|(objectclass=groupofuniquenames)(objectclass=groupofnames))";
    const char *attrs1[] = { "uniquemember","member", 0 };
    rv = session->find(gid,LDAP_SCOPE_BASE,filter1,attrs1,0,res);
    if (rv == LDAPU_SUCCESS) {
        while ((entry = res->next()) != NULL) {
            vals = entry->values("uniquemember");
            if (vals) {
                this->addAttrValues(t,vector,*vals);
                delete vals;
            }
            vals = entry->values("member");
            if (vals) {
                this->addAttrValues(t,vector,*vals);
                delete vals;
            }
            delete entry;
        }
        delete res;
    }
    sp->free_session(session);
    return;
}  

/*
SCOPE: LDAP_SCOPE_BASE LDAP_SCOPE_ONELEVEL LDAP_SCOPE_SUBTREE
Examples:
ldapsearch -h host -b "cn=j2ee,ou=Groups,dc=red,dc=iplanet,dc=com" \
     "(|(objectclass=groupofuniquenames)(objectclass=groupofnames))"  memberurl
*/
void LdapRealm::getDirectDynamicMemberForGroup(IDType t,char* gid,GenericVector& vector)
{
    LdapSessionPool *sp = this->getSessionPool();
    LdapSession *session = sp->get_session();

    LdapEntry *entry=NULL;
    LdapSearchResult *res=NULL;
    LdapValues *vals=NULL;
    char* pval=NULL;
    int rv=0;

    const char* filter =
            "(|(objectclass=groupofuniquenames)(objectclass=groupofnames))";
    const char *attrs[] = {"memberurl", 0 };
    rv = session->find(gid,LDAP_SCOPE_BASE,filter,attrs,0,res);
    if (rv == LDAPU_SUCCESS) {
        while ((entry = res->next()) != NULL) {
            vals = entry->values("memberurl");
            if (vals) {
                for (int i=0; i<vals->length(); i++) {
                   if ( (*vals)[i] == NULL)
                       continue;
                   const size_t len = (*vals)[i]->bv_len;
                   char *val = (*vals)[i]->bv_val;
                   char* dyn_memberurl= (char*)MALLOC( (*vals)[i]->bv_len + 1 );
                   memset(dyn_memberurl,0, (*vals)[i]->bv_len + 1);
                   PL_strncpy(dyn_memberurl,(*vals)[i]->bv_val,(*vals)[i]->bv_len);
                   char*   dyn_filter=NULL;
                   char*   dyn_basedn=NULL;
                   int     dyn_scope=0;
                   LdapEntry        *dyn_entry=NULL;
                   LdapSearchResult *dyn_res=NULL;
                   if (parse_memberURL(dyn_memberurl,&dyn_filter,&dyn_basedn,&dyn_scope)==LDAPU_SUCCESS) {
                       LdapSearchResult *dyn_res=NULL;
                       const char *dyn_attrs[] = { "dn", 0 };
                       rv = session->find(dyn_basedn,dyn_scope,dyn_filter,dyn_attrs,0,dyn_res);
                       ldapu_free(dyn_filter);
                       ldapu_free(dyn_basedn);
                       if (rv == LDAPU_SUCCESS || rv==LDAPU_ERR_MULTIPLE_MATCHES) {
                          while ((dyn_entry = dyn_res->next()) != NULL) {
                              this->addEntry(t,vector,*dyn_entry);
                              delete dyn_entry;
                          }
                          delete dyn_res;
                       }
                   }
                   FREE(dyn_memberurl);
                }
            }
            delete vals;
        }
        delete entry;
    }
    sp->free_session(session);
    return;
}  


GenericVector* LdapRealm::getDirectMemberUsersForGroup(char* gid)
{
    GenericVector* pVector = new GenericVector();
    this->getDirectStaticMemberForGroup(ID_USER,gid,*pVector);
    this->getDirectDynamicMemberForGroup(ID_USER,gid,*pVector);
    return pVector;
}

GenericVector* LdapRealm::getDirectMemberGroupsForGroup(char* gid)
{
    GenericVector* pVector = new GenericVector();
    this->getDirectStaticMemberForGroup(ID_GROUP,gid,*pVector);
    this->getDirectDynamicMemberForGroup(ID_GROUP,gid,*pVector);
    return pVector;
}
#endif
