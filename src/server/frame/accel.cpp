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
 * accel.cpp: Accelerator cache
 *
 * Chris Elving
 */

#include "xp/xp.h"
#include "support/urimapper.h"
#include "support/SimpleHash.h"
#include "time/nstime.h"
#include "httpdaemon/vsconf.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/connqueue.h"
#include "httpdaemon/daemonsession.h"
#include "httpdaemon/httprequest.h"
#include "httpdaemon/httpheader.h"
#include "httpdaemon/logmanager.h"
#include "safs/nsfcsafs.h"
#include "base/date.h"
#include "base/pool.h"
#include "base/util.h"
#include "base/shexp.h"
#include "base/systhr.h"
#include "base/vs.h"
#include "frame/req.h"
#include "frame/http.h"
#include "frame/http_ext.h"
#include "frame/accel.h"
#include "frame/conf.h"

#define DATE_HEADER_NAME "Date: "
#define DATE_HEADER_NAME_LEN (sizeof(DATE_HEADER_NAME) - 1)
#define DATE_HEADER_VALUE_SIZE 30
#define CONNECTION_KEEP_ALIVE "Connection: keep-alive\r\n"
#define CONNECTION_KEEP_ALIVE_LEN (sizeof(CONNECTION_KEEP_ALIVE) - 1)
#define CONNECTION_CLOSE "Connection: close\r\n"
#define CONNECTION_CLOSE_LEN (sizeof(CONNECTION_CLOSE) - 1)
#define MAX_CONNECTION_HEADER_LEN CONNECTION_KEEP_ALIVE_LEN

class AcceleratorGeneration;
class AcceleratorSet;
class AcceleratorResource;

typedef UriMap<AcceleratorResource *> AcceleratorMap;
typedef UriMapper<AcceleratorResource *> AcceleratorMapper;

/*
 * AcceleratorHandle contains a (possibly NULL) reference to the current
 * AcceleratorGeneration.  Each thread that wishes to use the accelerator cache
 * should have its own AcceleratorHandle.  Because the reference can be removed
 * from the AccleratorHandle at any time, a thread must use XP_AtomicSwapPtr
 * when it attempts to acquire the reference.
 */
struct AcceleratorHandle {
    AcceleratorHandle *next;
    volatile AcceleratorGeneration *gen;
};

/*
 * AcceleratorAsync contains a non-NULL reference to the AcceleratorGeneration
 * that was active at the time of accel_async_begin(), a pointer to the
 * AcceleratorHandle to which that reference should be returned, and linked
 * lists that track pending and completed async operations.
 */
struct AcceleratorAsync {
    pool_handle_t *pool;
    AcceleratorGeneration *gen;
    AcceleratorHandle *handle;
    Connection *again;
    Connection *done;
};

/*
 * AcceleratorGeneration is an instance of the accelerator.  Instances are
 * periodically created and destroyed in response to changing URI access
 * patterns.  Each AcceleratorGeneration is tied to a particular Configuration
 * and contains AcceleratorResources that cache the result of processing
 * requests under that Configuration.
 */
class AcceleratorGeneration {
public:
    static void install(const Configuration *config);
    static void validate();
    inline AcceleratorSet *lookup(const VirtualServer *vs);
    inline void ref();
    inline void unref();

private:
    inline AcceleratorGeneration();
    inline ~AcceleratorGeneration();
    inline void addResource(AcceleratorResource *resource);
    inline void addSet(AcceleratorSet *set);

    PtrVector<AcceleratorResource> _resources;
    PtrVector<AcceleratorSet> _sets;
    PRInt32 _refcount;
};

/*
 * AcceleratorSet maintains the VS-specific mappings from URIs to
 * AcceleratorResources in an AcceleratorGeneration.  Note that it is the
 * AcceleratorGeneration, not the AcceleratorSet, that owns the mapped
 * AcceleratorResources.
 */
class AcceleratorSet {
public:
    inline AcceleratorSet(const VirtualServer *vs, const AcceleratorMap& map);
    inline PRBool isForVirtualServer(const VirtualServer *vs);
    inline AcceleratorResource *lookup(const char *uri);

private:
    const VirtualServer *_vs;
    AcceleratorMapper _mapper;
};

/*
 * AcceleratorData contains information about a cached request.
 * AcceleratorDatas are associated with NSFC entries using the
 * accel_data304_nsfc_key and accel_data200_nsfc_key private data keys.
 * AcceleratorData is inherently tied to the Configuration under which the
 * request was originally processed, but it does not hold a reference.
 * Instead, it knows the ID of that Configuration and a VirtualServer * and
 * FlexLog * from it.  These pointers must not be dereferenced unless the
 * caller holds a reference to a Configuration with the same ID.
 */
struct AcceleratorData {
    PRInt32 id;
    const VirtualServer *vs;
    struct {
        const char *p;
        int len;
    } vsid;
    char *uri;
    time_t lm;
    int status_num;
    char status[4];
    char *etag;
    struct {
        char *p;
        int len;
    } headers;
    struct {
        FlexLog *log;
    } flex;
    PRBool internal;
    char *ssl_unclean_shutdown_browser;
    NSFCEntry entry;
};

/*
 * AcceleratorResource is an entry for a URI in an AcceleratorSet.  Each
 * AcceleratorResource has a reference to an NSFC entry and shortcut pointers
 * to that entry's AcceleratorDatas.
 */
class AcceleratorResource {
public:
    inline AcceleratorResource(NSFCEntry entry, const AcceleratorData *data304, const AcceleratorData *data200);
    inline ~AcceleratorResource();
    inline PRBool isHTTP();
    inline PRStatus checkEntry();
    inline const AcceleratorData *getData(const char *ims, const char *inm);

private:
    struct tm _lms;
    const char *_etag;
    NSFCEntry _entry;
    const AcceleratorData *_data304;
    const AcceleratorData *_data200;
};

/*
 * AcceleratorHHString wraps an HHString, terminating the string with a nul on
 * construction and restorting that character's value on destruction.
 */
class AcceleratorHHString {
public:
    inline AcceleratorHHString(const HHString *hs);
    inline AcceleratorHHString(const HHString& hs);
    inline ~AcceleratorHHString();
    inline operator const char *() const { return p; }

private:
    char *p;
    int len;
    char c;
};

/*
 * accel_initialized is set when the accelerator cache has been initialized.
 */
static PRBool accel_initialized;

/*
 * accel_handle_lock synchronizes the creation of AcceleratorHandles.
 */
static PRLock *accel_handle_lock;

/*
 * accel_handle_list is a linked list of all AcceleratorHandles.  It should
 * not be accessed without holding accel_handle_lock.
 */
static AcceleratorHandle *accel_handle_list;

/*
 * accel_gen_active is the most recent AcceleratorGeneration instance.  It
 * is used when assigning an initial generation to new AcceleratorHandles, so
 * it should not be accessed without holding accel_handle_lock.
 */
static AcceleratorGeneration *accel_gen_active;

/*
 * accel_gen_active_entries is the number of entries in the most recent
 * AcceleratorGeneration instance.
 */
static volatile PRInt32 accel_gen_active_entries;

/*
 * accel_gen_active_http_entries is the number of entries in the most recent
 * AcceleratorGeneration instance with a cached HTTP response.
 */
static volatile PRInt32 accel_gen_active_http_entries;

/*
 * accel_gen_config is the Configuration * that was current when we created
 * the most recent AcceleratorGeneration instance.
 */
static const void *accel_gen_config;

/*
 * accel_gen_nsfc_sig is the NSFCCacheSignature that was current when we
 * created the most recent AcceleratorGeneration instance.
 */
static NSFCCacheSignature accel_gen_nsfc_sig;

/*
 * accel_gen_validate_time is time the we last validated the most recent
 * AcceleratorGeneration's NSFC entries.
 */
static time_t accel_gen_validate_time;

/*
 * accel_nsfc_cache is the NSAPI NSFC file cache instance.
 */
static NSFCCache accel_nsfc_cache;

/*
 * accel_nsfc_max_age is the maximum age in seconds for NSFC entries or -1 if
 * NSFC entries never expire.
 */
static int accel_nsfc_max_age;

/*
 * accel_data304_nsfc_key is the key for the 304 response AcceleratorDatas we
 * store in NSFC.
 */
static NSFCPrivDataKey accel_data304_nsfc_key;

/*
 * accel_data200_nsfc_key is the key for the 200 response AcceleratorDatas we
 * store in NSFC.
 */
static NSFCPrivDataKey accel_data200_nsfc_key;

/*
 * accel_nsfc_dirty is set whenever the accelerator discovers that it's
 * holding a reference to an outdated NSFCEntry.
 */
static PRBool accel_nsfc_dirty;

/*
 * accel_set_vs_slot is the VS slot that holds the index of each VS's
 * AcceleratorSet * in the parent AcceleratorGeneration's _sets vector.
 */
static int accel_set_vs_slot = -1;

/*
 * accel_update_period is the number of milliseconds that accel_update_thread()
 * waits between accel_update() calls.
 */
static int accel_update_period;

#ifdef HAS_ASYNC_ACCELERATOR
/*
 * use_async_accelerator is TRUE if the directive AsyncAccelerator is either
 * set to True or is not set in magnus.conf
 */
static PRBool use_async_accelerator;
#endif

/*
 * Miscellaneous diagnostic counters
 */
volatile PRUint64 accel_eligible;
volatile PRUint64 accel_ineligible;
volatile PRUint64 accel_store_succeeded;
volatile PRUint64 accel_store_failed;
volatile PRUint64 accel_hits;
volatile PRUint64 accel_misses;
volatile PRUint64 accel_data_outdated;
volatile PRUint64 accel_data_inconsistent;
volatile PRUint64 accel_entry_invalid;
volatile PRUint64 accel_data_created;
volatile PRUint64 accel_data_destroyed;
volatile PRUint64 accel_resources_created;
volatile PRUint64 accel_resources_destroyed;
volatile PRUint64 accel_gens_created;
volatile PRUint64 accel_gens_destroyed;
volatile PRUint64 accel_handles_created;
volatile PRUint64 accel_process_http_success;
volatile PRUint64 accel_async_service_success;


/* ------------------------ accel_get_http_entries ------------------------ */

int accel_get_http_entries(void)
{
    return accel_gen_active_http_entries;
}


/* --------------------------- accel_get_stats ---------------------------- */

void accel_get_stats(StatsCacheBucket *bucket)
{
    bucket->countAcceleratorEntries = accel_gen_active_entries;
    bucket->countAcceleratableRequests = accel_eligible;
    bucket->countUnacceleratableRequests = accel_ineligible;
    bucket->countAcceleratableResponses = accel_store_succeeded;
    bucket->countUnacceleratableResponses = accel_store_failed;
    bucket->countAcceleratorHits = accel_hits;
    bucket->countAcceleratorMisses = accel_misses;
}


/* -------------------- AcceleratorGeneration::install -------------------- */

void AcceleratorGeneration::install(const Configuration *config)
{
    int resources = 0;
    int http_resources = 0;
    SimpleIntHash maps(31);
    PtrVector<AcceleratorSet> sets;
    AcceleratorHandle *handle;
    int i;

    ereport(LOG_VERBOSE, "Updating accelerator cache");

    // Create a new generation that will contain new VS-specific resource sets
    AcceleratorGeneration *newgen = new AcceleratorGeneration();

    // Construct VS-specific resource maps from the NSFC entries
    NSFCEntryEnumState state = NSFCENTRYENUMSTATE_INIT;
    while (NSFCEntry entry = NSFC_EnumEntries(accel_nsfc_cache, &state)) {
        // Check if there's AcceleratorData for this NSFC entry
        AcceleratorData *data304;
        AcceleratorData *data200;
        NSFCStatus rfc;

        rfc = NSFC_GetEntryPrivateData(entry,
                                       accel_data304_nsfc_key,
                                       (void **) &data304,
                                       accel_nsfc_cache);
        if (rfc != NSFC_OK)
            data304 = NULL;

        rfc = NSFC_GetEntryPrivateData(entry,
                                       accel_data200_nsfc_key,
                                       (void **) &data200,
                                       accel_nsfc_cache);
        if (rfc != NSFC_OK)
            data200 = NULL;

        if (data304 || data200) {
            // Check whether the AcceleratorData is current and consistent
            PRInt32 id = config->getID();
            PRBool current = PR_TRUE;
            PRBool consistent = PR_TRUE;
            if (data304 && data304->id != id)
                current = PR_FALSE;
            if (data200 && data200->id != id)
                current = PR_FALSE;
            if (data304 && data200 && data304->vs != data200->vs)
                consistent = PR_FALSE;

            // If the AcceleratorData is current and consistent...
            if (current && consistent) {
                // Find the VS and URI from the AcceleratorData
                const VirtualServer *vs;
                const char *uri;
                if (data304) {
                    vs = data304->vs;
                    uri = data304->uri;
                } else {
                    vs = data200->vs;
                    uri = data200->uri;
                }

                ereport(LOG_VERBOSE,
                        "Adding %s for virtual server %s to the accelerator cache",
                        uri, vs_get_id(vs));

                // Create a temporary VS-specific resource map
                AcceleratorMap *map = (AcceleratorMap *) maps.lookup((void *) vs);
                if (!map) {
                    map = new AcceleratorMap();
                    maps.insert((void *) vs, map);
                }

                // Add this entry to the generation and to the temporary
                // VS-specific resource map
                AcceleratorResource *resource;
                resource = new AcceleratorResource(entry, data304, data200);
                map->addUri(uri, resource);
                newgen->addResource(resource);

                // Keep track of how many resources are in the generation
                resources++;
                if (resource->isHTTP())
                    http_resources++;
            } else {
                // Invalidate the entry
                NSFC_SetEntryPrivateData(entry,
                                         accel_data304_nsfc_key,
                                         NULL,
                                         accel_nsfc_cache);
                NSFC_SetEntryPrivateData(entry,
                                         accel_data200_nsfc_key,
                                         NULL,
                                         accel_nsfc_cache);
                NSFC_ReleaseEntry(accel_nsfc_cache, &entry);
                if (!current)
                    accel_data_outdated++;
                if (!consistent)
                    accel_data_inconsistent++;
            }
        } else {
            NSFC_ReleaseEntry(accel_nsfc_cache, &entry);
        }
    }
    NSFC_EndEntryEnum(&state);

    // Construct VS-specific resource sets from the VS-specific resource maps
    for (i = 0; i < config->getVSCount(); i++) {
        const VirtualServer *vs = config->getVS(i);
        AcceleratorSet *set = NULL;
        AcceleratorMap *map = (AcceleratorMap *) maps.lookup((void *) vs);
        if (map) {
            set = new AcceleratorSet(vs, *map);
            newgen->addSet(set);
        }
        sets.append(set);
    }

    PR_Lock(accel_handle_lock);

    // Make sure all CPUs can see the contents of the new generation before
    // we start passing out references
    XP_ProducerMemoryBarrier();

    // Give each handle a reference to the new generation
    for (handle = accel_handle_list; handle; handle = handle->next) {
        newgen->ref();
        void *oldvalue = XP_AtomicSwapPtr(&handle->gen, newgen);
        AcceleratorGeneration *oldgen = (AcceleratorGeneration *) oldvalue;
        if (oldgen)
            oldgen->unref();
    }

    // Setup shortcuts to the VS-specific resource sets in this generation
    for (i = 0; i < config->getVSCount(); i++)
        vs_set_data(config->getVS(i), &accel_set_vs_slot, (void *) i);

    // Give our generation reference to accel_gen_active
    AcceleratorGeneration *oldgen = accel_gen_active;
    accel_gen_active = newgen;
    accel_gen_active_entries = resources;
    accel_gen_active_http_entries = http_resources;
    newgen = NULL;

    PR_Unlock(accel_handle_lock);

    // Discard the old generation, destroying outdated VS-specific sets
    if (oldgen)
        oldgen->unref();

    // Destroy the temporary resource maps
    SimpleHashUnlockedIterator iterator(&maps);
    while (AcceleratorMap *map = (AcceleratorMap *) iterator.next())
        delete map;
}


/* ------------------- AcceleratorGeneration::validate -------------------- */

void AcceleratorGeneration::validate()
{
    // N.B. we assume that accel_gen_active won't change (that is, that
    // AcceleratorGeneration::install won't be called) for the duration of
    // this function

    if (accel_gen_active) {
        ereport(LOG_FINEST, "Validating accelerator cache entries");

        for (int i = 0; i < accel_gen_active->_resources.length(); i++)
            accel_gen_active->_resources[i]->checkEntry();
    }
}


/* ------------- AcceleratorGeneration::AcceleratorGeneration ------------- */

AcceleratorGeneration::AcceleratorGeneration()
: _refcount(1)
{
    accel_gens_created++;
}


/* ------------ AcceleratorGeneration::~AcceleratorGeneration ------------- */

AcceleratorGeneration::~AcceleratorGeneration()
{
    int i;

    PR_ASSERT(_refcount == 0);

    accel_gens_destroyed++;

    for (i = 0; i < _sets.length(); i++)
        delete _sets[i];

    for (i = 0; i < _resources.length(); i++)
        delete _resources[i];
}


/* -------------------- AcceleratorGeneration::lookup --------------------- */

AcceleratorSet *AcceleratorGeneration::lookup(const VirtualServer *vs)
{
    AcceleratorSet *set = NULL;

    int i = (int) (size_t) vs_get_data(vs, accel_set_vs_slot);
    if (i >= 0 && i < _sets.length()) {
        set = _sets[i];
        if (!set->isForVirtualServer(vs))
            set = NULL;
    }

    return set;
}


/* ---------------------- AcceleratorGeneration::ref ---------------------- */

void AcceleratorGeneration::ref()
{
    PR_AtomicIncrement(&_refcount);

    PR_ASSERT(_refcount > 1);
}


/* --------------------- AcceleratorGeneration::unref --------------------- */

void AcceleratorGeneration::unref()
{
    PR_ASSERT(_refcount >= 1);

    if (PR_AtomicDecrement(&_refcount) == 0)
        delete this;
}


/* ------------------ AcceleratorGeneration::addResource ------------------ */

inline void AcceleratorGeneration::addResource(AcceleratorResource *resource)
{
    _resources.append(resource);
}


/* -------------------- AcceleratorGeneration::addSet --------------------- */

inline void AcceleratorGeneration::addSet(AcceleratorSet *set)
{
    _sets.append(set);
}


/* -------------------- AcceleratorSet::AcceleratorSet -------------------- */

AcceleratorSet::AcceleratorSet(const VirtualServer *vs,
                               const AcceleratorMap& map)
: _vs(vs),
  _mapper(map)
{ }


/* ------------------ AcceleratorSet::isForVirtualServer ------------------ */

inline PRBool AcceleratorSet::isForVirtualServer(const VirtualServer *vs)
{
    return (_vs == vs);
}


/* ------------------------ AcceleratorSet::lookup ------------------------ */

AcceleratorResource *AcceleratorSet::lookup(const char *uri)
{
    const char *suffix;
    const char *param;

    AcceleratorResource *resource = _mapper.map(uri, &suffix, &param);
    if (*suffix || param) {
        accel_misses++;
        return NULL;
    }

    accel_hits++;

    return resource;
}


/* --------------- AcceleratorResource::AcceleratorResource --------------- */

AcceleratorResource::AcceleratorResource(NSFCEntry entry,
                                         const AcceleratorData *data304,
                                         const AcceleratorData *data200)
: _entry(entry),
  _data304(data304),
  _data200(data200)
{
    PR_ASSERT(!data304 || !data200 || data304->lm == data200->lm);
    PR_ASSERT(!data304 || data304->status_num == 304);
    PR_ASSERT(!data200 || data200->status_num == 200);
    PR_ASSERT(!data304 || data304->entry == entry);
    PR_ASSERT(!data200 || data200->entry == entry);

    if (data304) {
        system_gmtime(&data304->lm, &_lms);
        _etag = data304->etag;
    } else {
        system_gmtime(&data200->lm, &_lms);
        _etag = data200->etag;
    }

    accel_resources_created++;

    // N.B. we assume ownership of the caller's NSFCEntry reference
}


/* -------------- AcceleratorResource::~AcceleratorResource --------------- */

AcceleratorResource::~AcceleratorResource()
{
    accel_resources_destroyed++;

    NSFC_ReleaseEntry(accel_nsfc_cache, &_entry);
}


/* --------------------- AcceleratorResource::isHTTP ---------------------- */

PRBool AcceleratorResource::isHTTP()
{
    return (_data200 && !_data200->internal) || (_data304 && !_data304->internal);
}


/* ------------------- AcceleratorResource::checkEntry -------------------- */

PRStatus AcceleratorResource::checkEntry()
{
    if (NSFC_CheckEntry(_entry, accel_nsfc_cache) != NSFC_OK) {
        // We need to create a new accelerator cache generation
        accel_nsfc_dirty = PR_TRUE;
        accel_entry_invalid++;
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


/* --------------------- AcceleratorResource::getData --------------------- */

const AcceleratorData *AcceleratorResource::getData(const char *ims,
                                                    const char *inm)
{
    // Never return stale data
    if (checkEntry() != PR_SUCCESS)
        return NULL;

    // Handle If-modified-since: and If-none-match: requests
    if (ims || inm) {
        if ((!ims || util_later_than(&_lms, ims)) &&
            (!inm || http_match_etag(inm, _etag, PR_FALSE)))
            return _data304;
    }

    return _data200;
}


/* --------------- AcceleratorHHString::AcceleratorHHString --------------- */

AcceleratorHHString::AcceleratorHHString(const HHString *hs)
{
    if (hs) {
        if (hs->len) {
            p = hs->ptr;
            len = hs->len;
            c = p[len];
        } else {
            p = &c;
            len = 0;
        }
        p[len] = '\0';
    } else {
        p = NULL;
    }
}

AcceleratorHHString::AcceleratorHHString(const HHString& hs)
{
    if (hs.len) {
        p = hs.ptr;
        len = hs.len;
        c = p[len];
    } else {
        p = &c;
        len = 0;
    }
    p[len] = '\0';
}


/* -------------- AcceleratorHHString::~AcceleratorHHString --------------- */

AcceleratorHHString::~AcceleratorHHString()
{
    if (p)
        p[len] = c;
}


/* -------------------------- accel_is_eligible --------------------------- */

PRBool accel_is_eligible(Connection *connection)
{
    const HttpHeader &httpHeader = connection->httpHeader;

    // To keep the compiler from generating a series of (slow) comparisons and
    // branches, we use bitwise | instead of logical ||
    size_t disqualifiers = httpHeader.GetMethodNumber() != METHOD_GET |
                           (size_t) httpHeader.GetQuery() |
                           (size_t) httpHeader.GetPragma() |
                           (size_t) httpHeader.GetCacheControl() |
                           (size_t) httpHeader.GetIfMatch() |
                           (size_t) httpHeader.GetIfRange() |
                           (size_t) httpHeader.GetIfUnmodifiedSince() |
                           (size_t) httpHeader.GetRange() |
                           (size_t) httpHeader.GetContentLength() |
                           (size_t) httpHeader.GetTransferEncoding();
 
    PRBool rv = (disqualifiers == 0);

    if (rv) {
        accel_eligible++;
    } else {
        accel_ineligible++;
    }

    return rv;
}


/* ------------------------- accel_finish_headers ------------------------- */

static inline void accel_finish_headers(pool_handle_t *pool,
                                        const char *prefix, int plen,
                                        int protv_num,
                                        PRBool keep_alive,
                                        char **p, int *sz)
{
    char *buf = NULL;
    int pos = 0;

    // HTTP/1.0 and higher responses have headers
    if (protv_num >= PROTOCOL_VERSION_HTTP10) {
        int maxsz = plen + DATE_HEADER_VALUE_SIZE + 2 + MAX_CONNECTION_HEADER_LEN + 2;
        buf = (char *) pool_malloc(pool, maxsz);

        memcpy(buf, prefix, plen);
        pos += plen;

        pos += date_current_formatted(date_format_http, buf + pos, DATE_HEADER_VALUE_SIZE);
        buf[pos++] = '\r';
        buf[pos++] = '\n';

        if (keep_alive) {
            if (protv_num < PROTOCOL_VERSION_HTTP11) {
                memcpy(buf + pos, CONNECTION_KEEP_ALIVE, CONNECTION_KEEP_ALIVE_LEN);
                pos += CONNECTION_KEEP_ALIVE_LEN;
            }
        } else {
            memcpy(buf + pos, CONNECTION_CLOSE, CONNECTION_CLOSE_LEN);
            pos += CONNECTION_CLOSE_LEN;
        }

        buf[pos++] = '\r';
        buf[pos++] = '\n';

        PR_ASSERT(pos <= maxsz);
    }

    *p = buf;
    *sz = pos;
}


/* ----------------------------- accel_begin ------------------------------ */

static inline AcceleratorGeneration *accel_begin(AcceleratorHandle *handle)
{
    // Get our handle's reference to the current accelerator generation
    return (AcceleratorGeneration *) XP_AtomicSwapPtr(&handle->gen, NULL);
}


/* ------------------------------ accel_end ------------------------------- */

static inline void accel_end(AcceleratorHandle *handle,
                             AcceleratorGeneration *gen)
{
    PR_ASSERT(gen != NULL);

    // Attempt to give the generation reference back to our handle
    if (XP_AtomicCompareAndSwapPtr(&handle->gen, NULL, gen)) {
        // AcceleratorGeneration::install already gave our handle a newer
        // generation reference, so discard this old generation reference
        gen->unref();
    }
}


/* ------------------------- accel_lookup_include ------------------------- */

static inline const AcceleratorData *accel_lookup_include(AcceleratorGeneration *gen, const VirtualServer *vs, const char *uri)
{
    AcceleratorSet *set = gen->lookup(vs);
    if (!set)
        return NULL;

    AcceleratorResource *resource = set->lookup(uri);
    if (!resource)
        return NULL;

    return resource->getData(NULL, NULL);
}


/* ------------------------ accel_process_include ------------------------- */

PRBool accel_process_include(AcceleratorHandle *handle,
                             pool_handle_t *pool,
                             PRFileDesc *csd,
                             const VirtualServer *vs,
                             const char *uri)
{
    PRBool rv = PR_FALSE;

    AcceleratorGeneration *gen = accel_begin(handle);
    if (gen) {
        const AcceleratorData *data = accel_lookup_include(gen, vs, uri);
        if (data) {
            // Send the cached response body
            NSFCStatusInfo si;
            NSFC_TransmitEntryFile(csd,
                                   data->entry,
                                   NULL, 0,
                                   NULL, 0, 
                                   PR_INTERVAL_NO_TIMEOUT,
                                   accel_nsfc_cache,
                                   &si);
            rv = PR_TRUE;
        }
        accel_end(handle, gen);
    }

    return rv;
}


/* -------------------------- accel_lookup_http --------------------------- */

static inline const AcceleratorData *accel_lookup_http(AcceleratorGeneration *gen,
                                                       Connection *connection,
                                                       const VirtualServer *vs)
{
    PR_ASSERT(accel_is_eligible(connection));

    AcceleratorSet *set = gen->lookup(vs);
    if (!set)
        return NULL;

    AcceleratorHHString path(connection->httpHeader.GetRequestAbsPath());
    AcceleratorResource *resource = set->lookup(path);
    if (!resource)
        return NULL;

    AcceleratorHHString ims(connection->httpHeader.GetIfModifiedSince());
    AcceleratorHHString inm(connection->httpHeader.GetIfNoneMatch());
    const AcceleratorData *data = resource->getData(ims, inm);
    if (!data)
        return NULL;

    // We can't use the cached response for an internal request to satisfy a
    // non-internal HTTP request
    if (data->internal)
        return NULL;

    return data;
}


/* ------------------ accel_handle_ssl_unclean_shutdown ------------------- */

static inline void accel_handle_ssl_unclean_shutdown(const AcceleratorData *data, Connection *connection)
{
    if (data->ssl_unclean_shutdown_browser) {
        AcceleratorHHString ua(connection->httpHeader.GetUserAgent());

        const char *browser = ua;
        if (!browser)
            browser = "";

        if (!WILDPAT_CMP(browser, data->ssl_unclean_shutdown_browser))
            connection->fUncleanShutdown = PR_TRUE;
    }
}


/* ------------------------------ accel_log ------------------------------- */

static inline void accel_log(pool_handle_t *pool,
                             Connection *connection,
                             int hlen,
                             const AcceleratorData *data,
                             PRInt64 transmitted)
{
    if (data->flex.log) {
        // Calculate how many bytes of the body were actually transmitted by
        // subtracting the size of the headers
        PRInt64 cl = transmitted - hlen;
        if (cl < 0)
            cl = 0;

        // Log access
        flex_log_accel(data->flex.log,
                       pool,
                       connection,
                       data->status, 3,
                       data->vsid.p, data->vsid.len,
                       cl);
    }
}

static inline void accel_log_async(pool_handle_t *pool,
                                   Connection *connection,
                                   int hlen,
                                   const AcceleratorData *data,
                                   PRInt64 transmitted,
                                   LogBuffer **handle)
{
    if (data->flex.log) {
        // Calculate how many bytes of the body were actually transmitted by
        // subtracting the size of the headers
        PRInt64 cl = transmitted - hlen;
        if (cl < 0)
            cl = 0;

        // Log access
        flex_log_accel_async(data->flex.log,
                             pool,
                             connection,
                             data->status, 3,
                             data->vsid.p, data->vsid.len,
                             cl, handle);
    }
}

/* -------------------------- accel_process_http -------------------------- */

PRBool accel_process_http(AcceleratorHandle *handle,
                          pool_handle_t *pool,
                          Connection *connection,
                          const VirtualServer *vs,
                          int *status_num,
                          PRInt64 *transmitted)
{
    PRBool rv = PR_FALSE;

    AcceleratorGeneration *gen = accel_begin(handle);
    if (gen) {
        const AcceleratorData *data = accel_lookup_http(gen, connection, vs);
        if (data) {
            char *headers;
            int hlen;
            accel_finish_headers(pool,
                                 data->headers.p, data->headers.len,
                                 connection->httpHeader.GetNegotiatedProtocolVersion(),
                                 connection->fKeepAliveReservation,
                                 &headers, &hlen);

            PRInt64 nbytes;
            if (data->status_num == 304) {
                // Send a 304 response
                PRFileDesc *csd = connection->fd;
                nbytes = csd->methods->write(csd, headers, hlen);
            } else {
                // Send a 200 response
                NSFCStatusInfo si;
                nbytes = NSFC_TransmitEntryFile(connection->fd,
                                                data->entry,
                                                headers, hlen,
                                                NULL, 0, 
                                                PR_INTERVAL_NO_TIMEOUT,
                                                accel_nsfc_cache,
                                                &si);
            }
            if (nbytes < 0)
                nbytes = 0;

            accel_handle_ssl_unclean_shutdown(data, connection);

            accel_log(pool, connection, hlen, data, nbytes);

            accel_process_http_success++;

            *status_num = data->status_num;
            *transmitted = nbytes;

            rv = PR_TRUE;
        }
        accel_end(handle, gen);
    }

    return rv;
}


/* -------------------------- accel_async_begin --------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
AcceleratorAsync *accel_async_begin(AcceleratorHandle *handle,
                                    pool_handle_t *pool)
{
    AcceleratorAsync *async = NULL;

    AcceleratorGeneration *gen = accel_begin(handle);
    if (gen) {
        async = (AcceleratorAsync *) pool_malloc(pool, sizeof(AcceleratorAsync));
        if (async) {
            async->pool = pool;
            async->gen = gen;
            async->handle = handle;
            async->again = NULL;
            async->done = NULL;
        } else {
            accel_end(handle, gen);
        }
    }

    return async;
}
#endif


/* --------------------------- accel_async_end ---------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
void accel_async_end(AcceleratorAsync *async)
{
    Connection *connection;

    if (!async)
        return;

    // Handle connections with pending async operations
    for (connection = async->again; connection; connection = connection->next) {
        // If we get here, an async operation should be in progress
        PR_ASSERT(connection->async.accel.data);

        // The headers were allocated from the pool, and the pool can be
        // recycled after accel_async_end() returns.  Don't leave dangling
        // pointers into the pool.
        const char *old_headers_p = connection->async.accel.headers.p;
        connection->async.accel.headers.p = NULL;

        // If this is the first time we've had to resume this response on this
        // connection...
        if (!connection->async.accel.gen) {
            // Give the connection a reference to the generation that contains
            // the in-progress cached response
            async->gen->ref();
            connection->async.accel.gen = async->gen;

            // If the connection hasn't finished sending the headers yet, it
            // will need a copy allocated from the permanent heap.  (This is
            // inefficient, but it should rarely happen in practice; typically,
            // we'll always be able to send the complete headers on the first
            // accel_async_http().)
            if (connection->async.accel.offset < connection->async.accel.headers.len) {
                char *p = (char *) PERM_MALLOC(connection->async.accel.headers.len);
                if (p) {
                    memcpy(p, old_headers_p, connection->async.accel.headers.len);
                    connection->async.accel.headers.p = p;
                } else {
                    // XXX elving how to indicate error?
                }
            }
        }
    }

    // Reuse LogBuffer across access_log() calls
    const VirtualServer *currentVS =  NULL;
    LogBuffer *handle = NULL;
    LogFile *currentLogFile = NULL;
    // Handle connections with completed async operations
    for (connection = async->done; connection; connection = connection->next) {


        // Get the log file for this request
        LogFile* rqLogFile = flex_get_logfile(connection->async.accel.data->flex.log);

        // If there is a handle, then use the same handle if the log file
        // is the same and if the vs is the same
        if (handle &&
            (currentLogFile != rqLogFile) &&
            (currentVS != connection->async.accel.data->vs)) {
            LogManager::unlockBuffer(handle, 0);
            handle = NULL;
        }

        // Write any access log entry
        accel_log_async(async->pool, 
                        connection,
                        connection->async.accel.headers.len,
                        connection->async.accel.data,
                        connection->async.accel.offset,
                        &handle);

        currentVS = connection->async.accel.data->vs;
        currentLogFile = rqLogFile;

        // If we previously gave this connection a generation reference during
        // our handling of the async->again linked list...
        if (connection->async.accel.gen) {
            // Remove the generation reference
            connection->async.accel.gen->unref();
            connection->async.accel.gen = NULL;

            // Free the copy of the headers that we previously placed on the
            // permanent heap
            PERM_FREE(connection->async.accel.headers.p);
        }

        // Indicate that no async operation is in progress
        connection->async.accel.data = NULL;
        connection->async.accel.headers.p = NULL;
        connection->async.accel.headers.len = 0;
        connection->async.accel.offset = 0;
    }

    if (handle)
        LogManager::unlockBuffer(handle, 0);

    accel_end(async->handle, async->gen);

    pool_free(async->pool, async);
}
#endif


/* -------------------------- accel_async_lookup -------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
PRBool accel_async_lookup(AcceleratorAsync *async,
                          Connection *connection,
                          const VirtualServer *vs)
{
    PR_ASSERT(accel_is_eligible(connection));
    PR_ASSERT(connection->async.accel.gen == NULL);
    PR_ASSERT(connection->async.accel.data == NULL);
    PR_ASSERT(connection->async.accel.headers.p == NULL);
    PR_ASSERT(connection->async.accel.offset == 0);

    if (connection->async.fd == -1)
        return PR_FALSE;

    if (!async)
        return PR_FALSE;

    const AcceleratorData *data = accel_lookup_http(async->gen, connection, vs);
    if (!data)
        return PR_FALSE;

    connection->async.accel.data = data;

    return PR_TRUE;    
}
#endif


/* ------------------------- accel_async_transmit ------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
static inline AcceleratorAsyncStatus accel_async_transmit(Connection *connection)
{
    AcceleratorAsyncStatus rv;

    if (connection->async.accel.data->status_num == 304) {
        // Send a 304 response
        ssize_t written = write(connection->async.fd,
                                connection->async.accel.headers.p + connection->async.accel.offset,
                                connection->async.accel.headers.len - connection->async.accel.offset);
        if (written >= 0) {
            if (written < connection->async.accel.headers.len - connection->async.accel.offset) {
                rv = ACCEL_ASYNC_AGAIN;
            } else {
                rv = ACCEL_ASYNC_DONE;
            }
            connection->async.accel.offset += written;
        } else {
            int e = errno;
            if (e == EAGAIN || e == EWOULDBLOCK) {
                rv = ACCEL_ASYNC_AGAIN;
            } else {
                rv = ACCEL_ASYNC_DONE;
            }
        }
    } else {
        // Send a 200 response
        NSFCAsyncStatus nas = NSFC_TransmitAsync(connection->async.fd,
                                                 connection->async.accel.data->entry,
                                                 connection->async.accel.headers.p,
                                                 connection->async.accel.headers.len,
                                                 &connection->async.accel.offset,
                                                 accel_nsfc_cache);
        if (nas == NSFC_ASYNCSTATUS_AGAIN) {
            rv = ACCEL_ASYNC_AGAIN;
            ereport(LOG_VERBOSE, "Accelerator Cache ACCEL_ASYNC_AGAIN, offset = %lld", (long long) connection->async.accel.offset);
        } else if (nas == NSFC_ASYNCSTATUS_WOULDBLOCK) {
            rv = ACCEL_ASYNC_FALSE;
        } else {
            rv = ACCEL_ASYNC_DONE;
        }
    }

    return rv;
}
#endif


/* ------------------------- accel_async_service -------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
AcceleratorAsyncStatus accel_async_service(AcceleratorAsync *async,
                                           Connection *connection,
                                           int *status_num,
                                           PRInt64 *transmitted)
{
    PR_ASSERT(connection->async.fd != -1);
    PR_ASSERT(connection->async.accel.data);

    if (connection->async.accel.gen) {
        // We're continuing an async operation that was initiated earlier
        if (!async)
            return ACCEL_ASYNC_AGAIN;
    } else {
        // We're starting a new async operation
        PR_ASSERT(connection->async.accel.offset == 0);
        if (!async)
            return ACCEL_ASYNC_FALSE;

        // Format headers for the response.  We start off allocating them from
        // the pool in hopes that we can send them in a single try.  If we
        // can't, accel_async_end() will create a copy on the permanent heap.
        accel_finish_headers(async->pool,
                             connection->async.accel.data->headers.p,
                             connection->async.accel.data->headers.len,
                             connection->httpHeader.GetNegotiatedProtocolVersion(),
                             connection->fKeepAliveReservation,
                             &connection->async.accel.headers.p,
                             &connection->async.accel.headers.len);
    }


    AcceleratorAsyncStatus rv = accel_async_transmit(connection);

    switch (rv) {
    case ACCEL_ASYNC_AGAIN:
        // We need to resume transmission later, so let accel_async_end() know
        // that the connection needs a reference to the generation that
        // contains the response data.  Depending on how much was sent, the
        // connection might also need a copy of the headers.
        connection->next = async->again;
        async->again = connection;
        break;

    case ACCEL_ASYNC_DONE:
        accel_async_service_success++;

        // We're done sending the response, so let accel_async_end() know that
        // it needs to finish up by writing any access log entry and releasing
        // any generation reference and freeing any headers
        connection->next = async->done;
        async->done = connection;

        // We can can skip accel_handle_ssl_unclean_shutdown() because we never
        // do async operations on SSL-enabled sockets
        PR_ASSERT(!connection->fSSLEnabled);

        // Report status back to the caller
        *status_num =  connection->async.accel.data->status_num;
        *transmitted = connection->async.accel.offset;
        break;

    default:
        // The request couldn't be serviced by NSFC.  This should only happen
        // on the first attempt to transmit the response, so the connection
        // shouldn't yet have a reference to the generation.
        PR_ASSERT(!connection->async.accel.gen);

        // Indicate that no async operation is in progress
        connection->async.accel.data = NULL;
        connection->async.accel.headers.p = NULL;
        connection->async.accel.headers.len = 0;
        connection->async.accel.offset = 0;
        break;
    }

    return rv;
}
#endif


/* -------------------------- accel_async_abort --------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
void accel_async_abort(AcceleratorAsync *async, Connection *connection)
{
    // Let accel_async_end() know that it needs to finish up by writing any
    // access log entry and releasing any generation reference
    connection->next = async->done;
    async->done = connection;
}
#endif

/* -------------------------- use_accel_async --------------------------- */

#ifdef HAS_ASYNC_ACCELERATOR
PRBool use_accel_async() 
{
    return use_async_accelerator;

}
#endif

/* ---------------------- accel_store_entity_headers ---------------------- */

static inline void accel_store_entity_headers(Session *sn,
                                              Request *rq,
                                              PROffset64 size,
                                              char **prefix, int *plen)
{
    // Remove the Connection: header field as it describes a particular
    // connection, not the entity.  We'll generate our own Connection: header
    // field later.
    pb_param *connection = pblock_removekey(pb_key_connection, rq->srvhdrs);

    // Handle the Content-length: header field.  For historical reasons,
    // "content-length" in rq->srvhdrs may have been set to the number of
    // bytes successfully transmitted instead of the actual size of the
    // entity.
    pb_param *content_length = pblock_removekey(pb_key_content_length, rq->srvhdrs);
    if (rq->status_num == 304) {
        // 304 responses don't have bodies
        PR_ASSERT(!content_length || !atoi(content_length->value));
    } else {
        // On a 200 responses, send-file should have already added a
        // Content-length: header (which may have been subsequently modified
        // by the HTTP filter)
        PR_ASSERT(content_length != NULL);

        // Set Content-length: to the size of the entity
        pblock_kllinsert(pb_key_content_length, size, rq->srvhdrs);
    }

    // Format the headers
    char *buf = (char *) pool_malloc(sn->pool, REQ_MAX_LINE);
    int pos = 0;
    pos += http_format_status(sn, rq, buf + pos, REQ_MAX_LINE - pos);
    pos += http_format_server(sn, rq, buf + pos, REQ_MAX_LINE - pos);
    buf = http_dump822_with_slack(rq->srvhdrs, buf, &pos, REQ_MAX_LINE, DATE_HEADER_NAME_LEN);
    memcpy(buf + pos, DATE_HEADER_NAME, DATE_HEADER_NAME_LEN);
    pos += DATE_HEADER_NAME_LEN;

    // Give caller a permanent copy of the headers
    *prefix = (char *) PERM_MALLOC(pos);
    memcpy(*prefix, buf, pos);
    *plen = pos;

    // Restore the connection's Connection: header field
    if (connection)
        pblock_kpinsert(pb_key_connection, connection, rq->srvhdrs);

    // Restore the connection's Content-length: header field
    pblock_removekey(pb_key_content_length, rq->srvhdrs);
    if (content_length)
        pblock_kpinsert(pb_key_content_length, content_length, rq->srvhdrs);
}


/* -------------------------- accel_data_create --------------------------- */

static AcceleratorData *accel_data_create(Session *sn,
                                          Request *rq,
                                          NSFCEntry entry)
{
    NSAPIRequest *nrq = (NSAPIRequest *) rq;

    const VirtualServer *vs = request_get_vs(rq);
    PR_ASSERT(vs != NULL);

    const char *uri = pblock_findkeyval(pb_key_uri, rq->reqpb);
    PR_ASSERT(uri != NULL);
    if (!uri)
        return NULL;

    NSFCFileInfo *finfo;
    if (request_info_path(NULL, rq, &finfo) != PR_SUCCESS)
        return NULL;

    const char *etag = pblock_findkeyval(pb_key_etag, rq->srvhdrs);

    AcceleratorData *data = (AcceleratorData *) PERM_MALLOC(sizeof(AcceleratorData));
    data->id = vs->getConfiguration()->getID();
    data->vs = vs;
    data->vsid.p = vs->name;
    data->vsid.len = strlen(vs->name);
    data->uri = PERM_STRDUP(uri);
    data->lm = finfo->pr.modifyTime / PR_USEC_PER_SEC;
    data->status_num = rq->status_num;
    if (data->status_num < 100 || data->status_num > 999)
        data->status_num = 500;
    PR_ASSERT(data->status_num == 304 || data->status_num == 200);
    util_itoa(data->status_num, data->status);
    if (INTERNAL_REQUEST(rq)) {
        data->etag = NULL;
        data->headers.p = NULL;
        data->headers.len = 0;
        data->flex.log = NULL;
        data->internal = PR_TRUE;
    } else {
        data->etag = etag ? PERM_STRDUP(etag) : NULL;
        accel_store_entity_headers(sn,
                                   rq,
                                   finfo->pr.size,
                                   &data->headers.p, &data->headers.len);
        data->flex.log = nrq->accel_flex_log;
        data->internal = PR_FALSE;
    }
    if (nrq->accel_ssl_unclean_shutdown_browser) {
        data->ssl_unclean_shutdown_browser = PERM_STRDUP(nrq->accel_ssl_unclean_shutdown_browser);
    } else {
        data->ssl_unclean_shutdown_browser = NULL;
    }
    data->entry = entry;

    accel_data_created++;

    return data;
}


/* --------------------------- accel_data_free ---------------------------- */

static void accel_data_free(AcceleratorData *data)
{
    if (data) {
        accel_data_destroyed++;

        PERM_FREE(data->uri);
        PERM_FREE(data->etag);
        PERM_FREE(data->headers.p);
        PERM_FREE(data->ssl_unclean_shutdown_browser);
        PERM_FREE(data);
    }
}


/* ----------------------------- accel_enable ----------------------------- */

void accel_enable(Session *sn, Request *rq)
{
    PR_ASSERT(accel_initialized);
    PR_ASSERT(((NSAPIRequest *) rq)->accel_nsfc_entry == NSFCENTRY_INIT);
    PR_ASSERT(((NSAPIRequest *) rq)->accel_flex_log == NULL);
    PR_ASSERT(((NSAPIRequest *) rq)->accel_ssl_unclean_shutdown_browser == NULL);
    PR_ASSERT(!rq->request_is_cacheable);

#ifdef DEBUG
    const HttpRequest *hrq = GetHrq(rq);
    if (hrq) {
        const DaemonSession& ds = hrq->GetDaemonSession();
        PR_ASSERT(INTERNAL_REQUEST(rq) || accel_is_eligible(ds.conn));
    } else {
        PR_ASSERT(INTERNAL_REQUEST(rq));
    }
#endif

    rq->request_is_cacheable = 1;
}


/* ----------------------------- accel_store ------------------------------ */

PRBool accel_store(Session *sn, Request *rq)
{
    PRBool response_is_cacheable = PR_FALSE;

    NSAPIRequest *nrq = (NSAPIRequest *) rq;

    if (nrq->rq.request_is_cacheable) {
        PR_ASSERT(!strcmp("GET", pblock_findkeyval(pb_key_method, rq->reqpb)));
        PR_ASSERT(!pblock_findkeyval(pb_key_query, rq->reqpb));
        PR_ASSERT(!pblock_findkeyval(pb_key_path_info, rq->vars));
        PR_ASSERT(!pblock_findkeyval(pb_key_content_encoding, rq->srvhdrs));
        PR_ASSERT(!pblock_findkeyval(pb_key_transfer_encoding, rq->srvhdrs));

        if (NSFCENTRY_ISVALID(&nrq->accel_nsfc_entry)) {
            NSFCPrivDataKey key = NULL;
            if (rq->status_num == 304) {
                key = accel_data304_nsfc_key;
            } else if (rq->status_num == 200) {
                key = accel_data200_nsfc_key;
            }

            if (key) {
                response_is_cacheable = PR_TRUE;

                AcceleratorData *data;
                NSFCStatus rfc = NSFC_GetEntryPrivateData(nrq->accel_nsfc_entry,
                                                          key,
                                                          (void **) &data,
                                                          accel_nsfc_cache);
                if (rfc == NSFC_NOTFOUND) {
                    data = accel_data_create(sn, rq, nrq->accel_nsfc_entry);
                    if (data) {
                        rfc = NSFC_SetEntryPrivateData(nrq->accel_nsfc_entry,
                                                       key,
                                                       data,
                                                       accel_nsfc_cache);
                        if (rfc != NSFC_OK)
                            accel_data_free(data);
                    }
                }
            }
        }
    }

    if (NSFCENTRY_ISVALID(&nrq->accel_nsfc_entry))
        NSFC_ReleaseEntry(accel_nsfc_cache, &nrq->accel_nsfc_entry);

    if (nrq->accel_ssl_unclean_shutdown_browser) {
        pool_free(sn->pool, nrq->accel_ssl_unclean_shutdown_browser);
        nrq->accel_ssl_unclean_shutdown_browser = NULL;
    }

    if (response_is_cacheable) {
        accel_store_succeeded++;
    } else {
        accel_store_failed++;
    }

    return response_is_cacheable;
}


/* ------------------------- accel_handle_create -------------------------- */

AcceleratorHandle *accel_handle_create(void)
{
    PR_ASSERT(accel_initialized);

    PR_Lock(accel_handle_lock);

    AcceleratorHandle *handle = (AcceleratorHandle *) PERM_MALLOC(sizeof(AcceleratorHandle));
    handle->next = accel_handle_list;
    if (accel_gen_active)
        accel_gen_active->ref();
    handle->gen = accel_gen_active;
    accel_handle_list = handle;

    PR_Unlock(accel_handle_lock);

    accel_handles_created++;

    return handle;
}


/* ----------------------------- accel_update ----------------------------- */

static void accel_update(void)
{
    PRBool dirty = PR_AtomicSet(&accel_nsfc_dirty, PR_FALSE);
    Configuration *config = ConfigurationManager::getConfiguration();
    NSFCCacheSignature nsfc_sig = NSFC_GetCacheSignature(accel_nsfc_cache);
    time_t now = ft_time();

    if (dirty || config != accel_gen_config || nsfc_sig != accel_gen_nsfc_sig) {
        // Create and install a new generation
        AcceleratorGeneration::install(config);
        accel_gen_config = config;
        accel_gen_nsfc_sig = nsfc_sig;
        accel_gen_validate_time = now;
    } else if (accel_nsfc_max_age != -1) {
        // Check existing generation for expired entries
        if ((now - accel_gen_validate_time) > accel_nsfc_max_age) {
            AcceleratorGeneration::validate();
            accel_gen_validate_time = now;
        }
    }

    config->unref();
}


/* ------------------------- accel_update_thread -------------------------- */

extern "C" void accel_update_thread(void *)
{
    accel_update_period = 1000;

    for (;;) {
        systhread_sleep(accel_update_period);

        PRIntervalTime start = PR_IntervalNow();

        accel_update();

        PRIntervalTime stop = PR_IntervalNow();

        double elapsed = PR_IntervalToMicroseconds(stop - start) / 1000.0;

        int delay = elapsed * 10000; // target 0.01% == 1/10000 CPU
        if (delay < 100)
            delay = 100; // 10Hz maximum update frequency
        if (delay > 300000)
            delay = 300000; // 5 minute maximum update period

        accel_update_period = (delay + 3 * accel_update_period) / 4;
    }
}


/* ------------------------ accel_data_nsfc_delete ------------------------ */

extern "C" void accel_data_nsfc_delete(NSFCCache cache,
                                       const char *filename,
                                       NSFCPrivDataKey key,
                                       void *p)
{
    AcceleratorData *data = (AcceleratorData *) p;

    accel_data_free(data);
}


/* ------------------------ accel_init_connection ------------------------- */

void accel_init_connection(Connection *connection)
{
    connection->async.accel.gen = NULL;
    connection->async.accel.data = NULL;
    connection->async.accel.headers.p = NULL;
    connection->async.accel.headers.len = 0;
    connection->async.accel.offset = 0;
}


/* --------------------------- accel_init_late ---------------------------- */

void accel_init_late(void)
{
    accel_nsfc_cache = GetServerFileCache();

    accel_nsfc_max_age = GetServerFileCacheMaxAge();

    accel_data304_nsfc_key = NSFC_NewPrivateDataKey(accel_nsfc_cache, accel_data_nsfc_delete);

    accel_data200_nsfc_key = NSFC_NewPrivateDataKey(accel_nsfc_cache, accel_data_nsfc_delete);

    accel_handle_lock = PR_NewLock();

    accel_set_vs_slot = vs_alloc_slot();

#ifdef HAS_ASYNC_ACCELERATOR
    use_async_accelerator = conf_getboolean("AsyncAccelerator", PR_TRUE);
    if (use_async_accelerator)
        ereport(LOG_VERBOSE, "Async accelerator cache is On");
    else
        ereport(LOG_VERBOSE, "Async accelerator cache is Off");
#endif

    PR_CreateThread(PR_SYSTEM_THREAD,
                    accel_update_thread,
                    NULL,
                    PR_PRIORITY_NORMAL,
                    PR_LOCAL_THREAD,
                    PR_UNJOINABLE_THREAD,
                    0);

    PR_ASSERT(!accel_initialized);

    accel_initialized = PR_TRUE;

    ereport(LOG_VERBOSE, "Initialized accelerator cache");
}
