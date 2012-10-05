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


/** ***************************************************************************
 * Implements dynamic CRL loading.
 *
 * NSS 3.10 introduced functions (CERT_CacheCRL, CERT_UncacheCRL) to
 * dynamically insert and remove CRLs during runtime. There is no
 * documentation for these functions beyond the comments in NSS's
 * cert.h include file. Additional details about the behavior of these
 * functions has been obtained from the Sun NSS team. Such details are
 * documented in comments in appropriate places below.
 *
 * For details on the configuration and usage of the dynamic CRL updates,
 * see security specification document in WSARC/2004/076.
 *
 */

#include <assert.h>
#include <prio.h>
#include <cert.h>
#include <base/crit.h>
#include <httpdaemon/dbthttpdaemon.h>
#include <httpdaemon/configuration.h>
#include <httpdaemon/configurationmanager.h>
#include <httpdaemon/updatecrl.h>

using ServerXMLSchema::Pkcs11;

#ifdef XP_WIN32
#define SLASH "\\"
#else
#define SLASH "/"
#endif

// Info kept for each loaded CRL in hashtable
typedef struct crl_info_t {
    const char * file;             // full path from where CRL was loaded
    PRTime mtime;                  // previous mtime from file's stat()
    SECItem * secitem;             // [NSS struct] contains CRL data,len,type
} crl_info;


// Used for temporary list of directory entries
typedef struct crl_file_info_t {
    const char * file;             // full path from where CRL was loaded
    PRTime mtime;                  // mtime from file's stat()
    PROffset32 size;               // file size
    struct crl_file_info_t * next; // next in list
} crl_file_info;

// Used to pass params to crl_remove_enum()
typedef struct crl_remove_enum_param_t {
    CERTCertDBHandle * db;
    crl_file_info * files_head;
} crl_remove_enum_param;

static PLHashTable * hashtable = NULL;  // hashtable of active CRLs
static CRITICAL updatecrl_crit = NULL;  // protects data access



/** ***************************************************************************
 * Free the given SECItem object and the contained data buffer.
 *
 */
static void free_secitem(SECItem * secitem)
{
    free(secitem->data);
    free(secitem);
}


/** ***************************************************************************
 * Frees the given crl_info object and the contained file name buffer. Also
 * calls free_secitem() to free the contained SECItem.
 *
 */
static void free_crlinfo(crl_info * crl)
{
    free((void *)crl->file);
    free_secitem(crl->secitem);
    free(crl);
}


/** ***************************************************************************
 * Walks through the given crl_file_info list, freeing each list entry and
 * contained data buffers.
 *
 */
static void free_file_list(crl_file_info * list)
{
    if (list == NULL) {
        return;
    }

    crl_file_info * next;

    do {
        next = list->next;
        if (list->file) { free((void *)list->file); }
        free(list);
        list = next;
    } while (list != NULL);
}


/** ***************************************************************************
 * Allocates a crl_info struct and populates with given values.
 *
 * Note that the returned struct will contain the pointers to filename and
 * secitem as given so these should not be freed before the crl_info itself 
 * is freed.
 *
 * Returns:
 *   An allocated and initialized crl_info object, or NULL if no memory.
 *
 */
static crl_info * alloc_crl_info(const char * filename, 
                                 PRTime mtime, SECItem * secitem)
{
    crl_info * newcrl = NULL;

    assert(filename != NULL);
    assert(secitem != NULL);

    if ((newcrl = (crl_info *)malloc(sizeof(crl_info))) == NULL) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_OOM_cache), filename);
        return NULL;
    }

    newcrl->file = filename;
    newcrl->mtime = mtime;
    newcrl->secitem = secitem;

    return newcrl;
}


/** ***************************************************************************
 * Creates a crl_file_info list. The list is populated with entries for
 * each file in the directory given by 'path'. See crl_file_info definition
 * above for the data fields saved for each file.  
 *
 * Params:
 *   path: Full path to the directory to read
 *   nodir_warn: If true, log warnings about nonexistent crl dir.
 *
 * Returns:
 *   The list head, or NULL if there is any error. The list head element
 *   is a dummy element, first file entry starts in head->next.
 *   The caller is responsible for freeing the list when no longer needed.
 *   See free_file_list().
 *
 */
static crl_file_info * crl_get_files(const char * path, PRBool nodir_warn)
{
    ereport(LOG_VERBOSE, "update-crl: reading CRL files from [%s]", path);

    PRDir * dirs = NULL;
    if ((dirs = PR_OpenDir(path)) == NULL) {
        if (nodir_warn == PR_TRUE) {
            ereport(LOG_WARN, XP_GetAdminStr(DBT_CRL_baddir), path);
        } else {
            ereport(LOG_VERBOSE, XP_GetAdminStr(DBT_CRL_baddir), path);
        }
        return NULL; 
    }

    PRDirEntry * filep;
    PRFileInfo statinfo;
    int rv;
    int need_slash = 0;

    if (strcmp(SLASH, path + strlen(path) - 1)) { need_slash = 1; }

    // initialize list head
    crl_file_info * file_list = NULL;
    if ((file_list = (crl_file_info *)malloc(sizeof(crl_file_info))) == NULL) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_OOM_dir));
        return NULL;
    }
    file_list->file = NULL;
    file_list->next = NULL;

    crl_file_info * file_list_this = NULL;
    crl_file_info * file_list_prev = file_list;

    int pathlen = strlen(path);
    int cnt = 0;

    // loop through each directory entry, creating list entries
    while ((filep = PR_ReadDir(dirs, PR_SKIP_BOTH))) {

        // need to construct full path to file
        char * fpath = NULL;
        if (!(fpath = (char *)malloc(pathlen + strlen(filep->name) + 2))){
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_OOM_dir));
            free_file_list(file_list);
            return NULL;
        }
        strcpy(fpath, path);
        if (need_slash) { strcat(fpath, SLASH); }
        strcat(fpath, filep->name);

        // stat the file
        rv = PR_GetFileInfo(fpath, &statinfo);
        if (rv != PR_SUCCESS) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_read_err), fpath);
            free(fpath);
            free_file_list(file_list);
            return NULL;
        }

        // create list entry for this file
        if ((file_list_this = 
              (crl_file_info *)malloc(sizeof(crl_file_info))) == NULL) {
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_OOM_file), fpath);
            free(fpath);
            free_file_list(file_list);
            return NULL;
        }

        file_list_this->file = fpath;
        file_list_this->mtime = statinfo.modifyTime;
        file_list_this->size = statinfo.size;
        file_list_this->next = NULL;

        file_list_prev->next = file_list_this;
        file_list_prev = file_list_this;

        cnt++;
    }

    PR_CloseDir(dirs);

    if (cnt == 0) {
        ereport(LOG_VERBOSE, "update-crl: No files found");
    }

    return file_list;
}


/** ***************************************************************************
 * Loads one CRL from a given file and allocates a SECItem struct to hold
 * its contents.
 *
 * Params:
 *   fileinfo: A crl_file_info object. Refer to the top of this file
 *             for contents (this function uses the 'file' path and 'size').
 *
 * Returns:
 *    A SECItem struct containing the CRL in the 'data' buffer, with
 *    'size' set accordingly.
 *    The caller is responsible for freeing the SECItem when no longer needed.
 *    See free_secitem(). Returns NULL on any errors.
 *    For documentation on SECItem, see:
 * http://www.mozilla.org/projects/security/pki/nss/ref/ssl/ssltyp.html#1026076
 *
 */
static SECItem * load_crl(crl_file_info * fileinfo)
{
    PRFileDesc * crl_file = NULL;
    if ((crl_file = PR_Open(fileinfo->file, PR_RDONLY, 0)) == NULL) {
        ereport(LOG_FAILURE,
                XP_GetAdminStr(DBT_CRL_read_err), fileinfo->file);
        return NULL;
    }

    unsigned char * crl_buff = NULL;
    if ((crl_buff = (unsigned char *)malloc(fileinfo->size)) == NULL) {
        ereport(LOG_FAILURE, 
                XP_GetAdminStr(DBT_CRL_OOM_file), fileinfo->file);
        PR_Close(crl_file);
        return NULL;
    }

    PRInt32 cnt = PR_Read(crl_file, crl_buff, fileinfo->size);
    if (cnt != fileinfo->size) {
        ereport(LOG_FAILURE, 
                XP_GetAdminStr(DBT_CRL_read_err), fileinfo->file);
        free(crl_buff);
        PR_Close(crl_file);
        return NULL;
    }
    PR_Close(crl_file);

    SECItem * secitem;
    if ((secitem = (SECItem *)malloc(sizeof(SECItem))) == NULL) {
        ereport(LOG_FAILURE, 
                XP_GetAdminStr(DBT_CRL_OOM_file), fileinfo->file);
        free(crl_buff);
        return NULL;
    }

    secitem->data = crl_buff;
    secitem->len = fileinfo->size;
                                // Currently NSS (3.10) ignores the 'type'
                                // value. We set an arbitrary value here.
    SECItemType itype = siBuffer;
    secitem->type = itype;

    return secitem;
}


/** ***************************************************************************
 * Hashtable enumerator callback function. For each hashtable element, 
 * verify if the given file is in the crl_file_info list. If it is not,
 * it means the CRL file was removed from the directory. In that case the
 * corresponding CRL is uncached and removed from the hashtable. See also
 * http://www.mozilla.org/projects/nspr/reference/html/plhash.html#35195
 *
 * Params:
 *   he: See 
 *       http://www.mozilla.org/projects/nspr/reference/html/plhash.html#35106
 *   index: Index of this element. Not used.
 *   arg: Assumed to contain ptr to crl_remove_enum_param, see above.
 *
 * Returns:
 *   HT_ENUMERATE_REMOVE: If CRL removed, we free the key and value and 
 *        return this which causes the hashtable to remove the entry.
 *   HT_ENUMERATE_NEXT: If CRL is to be kept, do nothing and return this.
 *
 */
static PRIntn crl_remove_enum(PLHashEntry *he, PRIntn index, void *arg)
{
    assert(he != NULL);
    assert(arg != NULL);

    CERTCertDBHandle * db = ((crl_remove_enum_param *)arg)->db;
    crl_file_info * files_head = ((crl_remove_enum_param *)arg)->files_head;

    assert(db != NULL);
    assert(files_head != NULL);

    int found = 0;
    const char * file = (const char *)(he->key);

    if (files_head->next != NULL) {
        // try to find file in the given crl_file_info list
        crl_file_info * list_tmp = files_head->next;

        while (!found && (list_tmp != NULL)) {
            if (!strcmp(file, list_tmp->file)) { found = 1; }
            list_tmp = list_tmp->next;
        }
    }

    if (found) {
        // cached CRL is still in dir so no need to do anything
        return HT_ENUMERATE_NEXT;
    }

    // otherwise, get the crl_info, uncache it and then free data
    ereport(LOG_INFORM, XP_GetAdminStr(DBT_CRL_remove), file);

    crl_info * crlinfo = (crl_info *)(he->value);

    assert(crlinfo != NULL);
    assert(crlinfo->secitem != NULL);

    SECStatus st = CERT_UncacheCRL(db, crlinfo->secitem);
    if (st != SECSuccess) {
        // This should never happen but if there is some failure just log
        // it and do not try to free the data since NSS might(?) still end
        // up accessing it. 
        ereport(LOG_CATASTROPHE, XP_GetAdminStr(DBT_CRL_fail_remove), st);
        return HT_ENUMERATE_NEXT;
    }

    // Need to free he->key and he->data. However he->key is same
    // as (he->data)->file which is freed by free_crlinfo() already.
    free_crlinfo(crlinfo);

    // Tell hashtable to remove this entry
    return HT_ENUMERATE_REMOVE;
}


/** ***************************************************************************
 * Loop through crl_file_info list and check if there are any new or 
 * updated CRL files. If so, update NSS CRL cache accordingly.
 *
 * Params:
 *    db: NSS cert db handle (see
 *http://www.mozilla.org/projects/security/pki/nss/ref/ssl/ssltyp.html#1028465)
 *    files_head: head of crl_file_info list (first element is dummy)
 *
 */
static void crl_check_new_updates(CERTCertDBHandle * db,
                                  crl_file_info * files_head)
{
    crl_file_info * list_tmp = files_head;

    // loop thru list of files read from dir

    crl_info * cached;

    while ((list_tmp != NULL) && (list_tmp->next != NULL)) {

        list_tmp = list_tmp->next;

        // check if this file has been cached already
        cached = (crl_info *)PL_HashTableLookup(hashtable, list_tmp->file);

        if (cached == NULL) { // no, haven't seen it, need to get it

            ereport(LOG_INFORM, XP_GetAdminStr(DBT_CRL_adding), 
                    list_tmp->file);

                                // create SECItem entry for it
            SECItem * secitem = NULL;
            if ((secitem = load_crl(list_tmp)) == NULL) {
                continue;
            }
                                // create crl_info entry for it
            crl_info * newcrl = NULL;
            if ((newcrl = alloc_crl_info(strdup(list_tmp->file),
                                         list_tmp->mtime,
                                         secitem)) == NULL) {
                free_secitem(secitem);
                continue;
            }

            // Cache it in NSS now.  The dynamic cache is separate
            // from the static CRLs kept in the db file. If a CRL was
            // in the db and we insert it dynamically here, it will
            // not be an error (we will not get
            // SEC_ERROR_CRL_ALREADY_EXISTS). In fact as of NSS 3.10
            // this error is only returned if the ptr is identical, no
            // other verification is done.

            SECStatus st = CERT_CacheCRL(db, secitem);
            if (st != SECSuccess) {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_cache),
                        list_tmp->file, st);
                free_crlinfo(newcrl);
                continue;
            }

            // Store in hashtable. Note that NSS will now continue to access
            // the CRL data in newcrl->secitem->data so we cannot free this.

            PL_HashTableAdd(hashtable, 
                            (const void *)newcrl->file, (void *)newcrl);


        } else if (list_tmp->mtime > cached->mtime) {

            // This CRL was loaded before but the file mtime is now newer
            // so need to cache new one and remove old one.

            ereport(LOG_INFORM, 
                    XP_GetAdminStr(DBT_CRL_update), list_tmp->file);

                                // create SECItem entry for it
            SECItem * secitem = NULL;
            if ((secitem = load_crl(list_tmp)) == NULL) {
                continue;
            }

            // We cache the new one before uncacheing the old one.
            // This assures that there won't be a time window during
            // which no CRL is available. This is ok even if the CRLs
            // were to be identical (i.e. if someone did 'touch
            // crlfile' without updating the contents).

            SECStatus st = CERT_CacheCRL(db, secitem);
            if (st != SECSuccess) {
                ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_cache),
                        list_tmp->file, st);
                free_secitem(secitem);
                continue;
            }

            // Ok now uncache old one, free old secitem, update entry
            st = CERT_UncacheCRL(db, cached->secitem);
            if (st != SECSuccess) {
                ereport(LOG_CATASTROPHE, 
                        XP_GetAdminStr(DBT_CRL_fail_remove), st);
                continue;
            }

            free_secitem(cached->secitem);
            cached->secitem = secitem;
            cached->mtime = list_tmp->mtime;

        } else {
            ereport(LOG_VERBOSE, "update-crl: No changes to CRL [%s]",
                    list_tmp->file);
        }
    }
}


/** ***************************************************************************
 * Entry point for CRL update check. 
 *
 * Params:
 *    crl_path: The path to the crl directory, must be non-null.
 *    nodir_warn: If true, log warnings about nonexistent crl dir.
 *
 */
void crl_check_updates_p(const char * crl_path, PRBool nodir_warn)
{
    assert(crl_path != NULL);

    // If first time around, do initialization. Note that this occurs during
    // the initial CRL load during server startup.

    if (hashtable == NULL) {
        hashtable = PL_NewHashTable(0, PL_HashString, PL_CompareStrings,
                                    PL_CompareValues, NULL, NULL);
        updatecrl_crit = crit_init();
    }

    assert(hashtable != NULL);
    assert(updatecrl_crit != NULL);

    //----- START_CRIT ------------------------------
    // This is not an issue during event calls since those cannot be
    // concurrent. But lets protect against the unlikely interaction
    // between a scheduled event and reconfig, which also triggers a CRL
    // reload.

    crit_enter(updatecrl_crit);

    // Need NSS db handle; this shouldn't really ever fail
    CERTCertDBHandle * db = NULL;
    if ((db = CERT_GetDefaultCertDB()) == NULL) {
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_CRL_nonssdb));
        crit_exit(updatecrl_crit);
        return;
    }

    // Read the CRL dir, creating crl_file_info list
    crl_file_info * files_head = NULL;
    if ((files_head = crl_get_files(crl_path, nodir_warn)) == NULL) {
        crit_exit(updatecrl_crit);
        return;
    }
    
    // Check if there are any new or updated CRLs
    crl_check_new_updates(db, files_head);

    // Check if any CRLs were removed
    crl_remove_enum_param param;
    param.db = db;
    param.files_head = files_head;
    PL_HashTableEnumerateEntries(hashtable, &crl_remove_enum, &param);

    // Clean up
    free_file_list(files_head);

    crit_exit(updatecrl_crit);
    //----- END_CRIT ------------------------------
}


/** ***************************************************************************
 * Entry point for CRL update check. Called from event trigger during runtime.
 * See scheduler.cpp:worker()
 *
 * Params:
 *    nodir_warn: If true, log warnings about nonexistent crl dir.
 *
 */
void crl_check_updates(PRBool nodir_warn)
{
    // Get CRL dir from config
    const Configuration * config = ConfigurationManager::getConfiguration();
    assert(config != NULL);
    const Pkcs11& pkcs11 = config->pkcs11;

    crl_check_updates_p(pkcs11.crlPath, nodir_warn);

    // Release the config reference
    config->unref();
}


