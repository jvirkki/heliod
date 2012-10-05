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


#include <assert.h>
#include <plhash.h>
#include <base/crit.h>
#include <base/ereport.h>
#include <base/systems.h>
#include <frame/http.h>
#include <frame/req.h>
#include <frame/log.h>
#include <safs/dbtsafs.h>
#include <safs/reqlimit.h>


// Timeout (sec) after which unaccessed entries can be removed (global)
#define PURGE_TIMEOUT "timeout"
#define DEFAULT_PURGE_TIMEOUT DEFAULT_INTERVAL*10;

// The req/sec limit (on average in interval)
#define MAXRPS_PARAM "max-rps"

// Max concurrent connections. Not limited by default unless value provided.
#define CONC_PARAM "max-connections"

// Interval (in seconds) for recalculating the avg req/sec for the interval
#define INTERVAL_PARAM "interval"
#define DEFAULT_INTERVAL 30

// Continue defines what must happen for requests to be permitted again after
// been limited. 
// - 'threshold' means as soon as request rate drops below max-rps (in an 
//    interval, as usual), they get serviced again.
// - 'silence' means requests in an interval must drop to zero before future
//    requests will be serviced.
#define CONTINUE_PARAM "continue"
#define CONT_THRESHOLD_VAL "threshold"
#define CONT_SILENCE_VAL "silence"
#define CONT_THRESHOLD 1
#define CONT_SILENCE 2
#define DEFAULT_CONTINUE CONT_THRESHOLD

// Response code for rejected requests.
#define ERROR_PARAM "error"
#define DEFAULT_ERROR PROTOCOL_SERVICE_UNAVAILABLE

// Param identifying the attribute being monitored
#define MONITOR_PARAM "monitor"




// Info kept for each bucket:
typedef struct bucket_info_t {
    long count;                 // hits since last recompute
    time_t time;                // next recompute at this time or later
    int state;                  // REQ_NOACTION or REQ_ABORTED
    int conc;                   // how many concurrent reqs for this now
} bucket_info;


static CRITICAL reqlimit_crit = NULL;             // protects data access
static bucket_info anon_bucket;                   // anon bucket, separately
static PLHashTable * hashtable = NULL;            // hashtable of named buckets
static int purge_timeout = DEFAULT_PURGE_TIMEOUT; // purge timeout
static time_t next_timeout;                       // time for next purge
static int ht_entries, ht_expired;                // just for reporting
static int req_cleanup;                           // for reqlimit_conc_done()



/** **************************************************************************
 * Callback used from handle_purge_timeout(). Called for each element in
 * hashtable. Checks whether the element is too old since last use compared
 * to the purge_timeout.   See also
 * http://www.mozilla.org/projects/nspr/reference/html/plhash.html#35195
 *
 * Params:
 *   he: See 
 *       http://www.mozilla.org/projects/nspr/reference/html/plhash.html#35106
 *   index: Index of this element. Not used.
 *   arg: Used by handle_purge_timeout() to pass in a time_t of current time.
 *
 * Returns:
 *   HT_ENUMERATE_REMOVE: If element too old, free the key and value and 
 *        return this value which causes the hashtable to remove the 
 *        entry itself.
 *   HT_ENUMERATE_NEXT: If not too old, do nothing and return this.
 *
 */
static PRIntn cleaner_enum(PLHashEntry *he, PRIntn index, void *arg)
{
    assert(he != NULL);
    assert(arg != NULL);

    ht_entries++;

    int conc = ((bucket_info *)(he->value))->conc;
    if (conc > 0) {
        // Don't purge buckets where conc>0 because that means a request
        // is still ongoing and has a reference to this bucket. (This
        // shouldn't really happen unless a request is very slow or the
        // purge timeout is far too low, but it could.)
        return HT_ENUMERATE_NEXT;
    }

    time_t age = *((time_t *)arg) - ((bucket_info *)(he->value))->time;

    if (age > purge_timeout) {
        ereport(LOG_VERBOSE, 
                "check-request-limits: Removing expired entry for [%s]", 
                (char *)(he->key));
        PERM_FREE(he->value);
        PERM_FREE((void *)(he->key));
        ht_expired++;
        return HT_ENUMERATE_REMOVE;
    }

    return HT_ENUMERATE_NEXT;
}


/** **************************************************************************
 * Walk through hashtable and remove entries which are too old (per timeout).
 * Note: this assumes we own reqlimit_crit.
 *
 */
static void handle_purge_timeout(time_t time_now)
{
    ht_entries = 0;
    ht_expired = 0;
    PL_HashTableEnumerateEntries(hashtable, &cleaner_enum, &time_now);
    next_timeout = time_now + purge_timeout;

    ereport(LOG_VERBOSE, 
            "check-request-limits: Expired %d entries from total of %d", 
            ht_expired, ht_entries);
}


/** **************************************************************************
 * check-request-limits SAF
 *
 * See top of this file for the pb parameters this SAF can consume and the
 * defaults for each.
 *
 * This SAF counts requests per interval for the given monitor/bucket. If
 * the request/sec in an interval exceeds the max-rps given then it returns
 * REQ_ABORTED for this and all subsequent matching requests in the current
 * interval. After the next interval request rate recomputation the request
 * limiting may be discontinued if the conditions of 'continue' are met.
 *
 * Separately, concurrent requests for the same bucket may be limited to
 * the given number if max-connections is given.
 *
 * On the next request after purge_timeout, a purge sweep of the buckets is
 * done, deleting any entries which have not seen any recomputes in the 
 * purge interval (unless timeout disabled by setting it to zero).
 *
 * For more information, refer to the WS7.0 security functional spec at:
 * http://sac.eng/arc/WSARC/2004/076/
 *
 * For params: http://docs.sun.com/source/817-1835-10/npgmysaf.html#wp14889
 * For returns: http://docs.sun.com/source/817-1835-10/npgmysaf.html#wp14969
 *
 * This returns:
 *      REQ_NOACTION: If the request can go on.
 *      REQ_ABORTED: If the request has hit limits and is not to be processed.
 *          Return code is set to 'error' param (default 503).
 *
 */
int check_request_limits(pblock *pb, Session *sn, Request *rq)
{
    const char * param;
    int response = REQ_NOACTION;

    if (rq->rq_attr.req_restarted) {
        // Do not count restarted requests as new requests for the
        // purpose of reqlimit accounting as it is just one client
        // request.  (This is particularly important for
        // max-connections since processing restarts would cause the
        // conc counter to increase more than once for a given request
        // but only decrease once at the end).
        return response;
    }

    time_t time_now = rq->req_start;
    assert (time_now != NULL);

    // Get max-rps
    
    int max_rps = 0;
    param = pblock_findval(MAXRPS_PARAM, pb);
    if (param) {
        max_rps = atoi(param);
    }

    // Get max-connections

    int conc = 0;
    param = pblock_findval(CONC_PARAM, pb);
    if (param) {
        conc = atoi(param);
    }

    // We must have at least max-rps or max-connections, otherwise can't
    // do anything meaningful here

    if (!max_rps && !conc) {
        log_error(LOG_MISCONFIG, "check-request-limits", sn, rq,
                  XP_GetAdminStr(DBT_reqlimitCantWork));
        return response;
    }

    // Decide bucket name; if none, we use the anonymous bucket anon_bucket

    bucket_info * bucket = NULL;
    const char * bucket_name = pblock_findval(MONITOR_PARAM, pb);
    if (!bucket_name) {
        bucket = &anon_bucket;
    }

    // interval (in seconds), or use default

    int interval = DEFAULT_INTERVAL;
    param = pblock_findval(INTERVAL_PARAM, pb);
    if (param) {
        interval = atoi(param);
    }

    // Check continue, or use default
    int cont = DEFAULT_CONTINUE;
    param = pblock_findval(CONTINUE_PARAM, pb);
    if (param) {
        if (!strcmp(CONT_THRESHOLD_VAL, param)) { cont = CONT_THRESHOLD; }
        else if (!strcmp(CONT_SILENCE_VAL, param)) { cont = CONT_SILENCE; }
        else {
            // Log config error but continue since we have default
            log_error(LOG_MISCONFIG, "check-request-limits", sn, rq,
                      XP_GetAdminStr(DBT_reqlimitBadContinue));
        }
    }

    //----- START_CRIT ------------------------------

    crit_enter(reqlimit_crit);

    if (purge_timeout && (time_now > next_timeout)) { // run purge if needed
        handle_purge_timeout(time_now);
    }

    // If using anon bucket we already have reference to it, otherwise need
    // to go find it in hashtable (and if not found, create one)

    if (!bucket) {

        bucket = (bucket_info *)PL_HashTableLookup(hashtable, bucket_name);

        if (!bucket) {
            // Need to create new entry for this one
            log_error(LOG_VERBOSE, "check-request-limits", sn, rq,
                      "creating new entry for [%s]", bucket_name);
            bucket = (bucket_info *)PERM_MALLOC(sizeof(bucket_info));
            bucket->count = 1;
            bucket->time = time_now + interval;
            bucket->state = REQ_NOACTION;
            bucket->conc = 0;   // handle conc case on initial?
            PL_HashTableAdd(hashtable, 
                            (const void *)PERM_STRDUP(bucket_name), 
                            (void *)bucket);
            // Since it is the first request, no need to check more
            crit_exit(reqlimit_crit);
            return response;
        }
    }

    // If we are doing max-rps limiting then handle it otherwise don't bother

    if (max_rps) {

        bucket->count++;

        if (time_now > bucket->time) { 
            // Interval or more has passed, time to recompute and recheck

            int time_interval = time_now - bucket->time + interval;
            int rps = bucket->count / time_interval;

            log_error(LOG_VERBOSE, "check-request-limits", sn, rq,
                      "bucket [%s] %d req/s (%d req in %d sec)",
                      bucket_name ? bucket_name: "", 
                      rps, bucket->count, time_interval);

            if (rps > max_rps) {
                // Start limiting
                bucket->state = REQ_ABORTED;
                log_error(LOG_WARN,  "check-request-limits", sn, rq,
                          XP_GetAdminStr(DBT_reqlimitAboveMaxRPS),
                          rps, max_rps,
                          bucket_name ? bucket_name: "");

            } else {
                // Reset state if we're under threshhold or if this is first 
                // hit (which means an interval with zero hits has already 
                // passed)
                if ((cont == CONT_THRESHOLD) || (bucket->count == 1)) {
                    bucket->state = REQ_NOACTION;
                }
            }

            // Prepare for next interval by resetting count and recompute time
            bucket->count = 0;
            bucket->time = time_now + interval;
        }

        response = bucket->state;
    }

    // If decision to reject already done, no need to check or increase conc
    // since this is not getting processed anyway. Otherwise, do it if needed.
 
    if (conc && response != REQ_ABORTED) {

        if (bucket->conc >= conc) {
            // Note that this reject is based on conditions at this instant
            // instead of over an interval, so is independent of bucket->state
            response = REQ_ABORTED;

        } else {
            bucket->conc++;
            // This queues up a call to fn associated with req_cleanup
            // (here, reqlimit_conc_done) to be called after request is done
            request_set_data(rq, req_cleanup, bucket);
        }
    }

    crit_exit(reqlimit_crit);

    //----- END_CRIT ------------------------------

    if (response == REQ_NOACTION) {
        return REQ_NOACTION;
    }

    // abort this request

    int err = DEFAULT_ERROR;
    param = pblock_findval(ERROR_PARAM, pb);
    if (param) {
        err = atoi(param);
    }
    protocol_status(sn, rq, err, NULL);

    log_error(LOG_VERBOSE, "check-request-limits", sn, rq,
              "Rejecting request matching bucket [%s] with status %d",
              bucket_name ? bucket_name: "", err);
              
    return response;
}


/** ***************************************************************************
 * Reduce the concurrent request count after this request has completed.
 * When max-connections is applied, check_request_limits() inserts this
 * function to be called during request cleanup.
 *
 * Params:
 *   data: ptr to data passed in to request_set_data() when the call was
 *         scheduled for this request. Used here to pass in a ptr to the
 *         bucket tracking this request.
 *
 */
void reqlimit_conc_done(void *data)
{
    bucket_info * bucket = (bucket_info *)data;
    assert(bucket != NULL);

    //----- START_CRIT ------------------------------

    crit_enter(reqlimit_crit);
    bucket->conc--;
    crit_exit(reqlimit_crit);

    //----- END_CRIT ------------------------------
}


/** **************************************************************************
 * Internal initialization. Always called, from lib/httpdaemon/WebServer.cpp
 *
 */
void reqlimit_init_crits()
{
    reqlimit_crit = crit_init();

    anon_bucket.count = 0;
    anon_bucket.time = time(NULL);
    anon_bucket.state = REQ_NOACTION;
    anon_bucket.conc = 0;
    
    hashtable = PL_NewHashTable(0, PL_HashString, PL_CompareStrings,
                                PL_CompareValues, NULL, NULL);

    next_timeout = time(NULL) + purge_timeout;

    // Initializes a "slot" for our reqlimit_conc_done() function. See 
    // call to request_set_data() in check_request_limits() for more.
    req_cleanup = request_alloc_slot(&reqlimit_conc_done);
    assert(req_cleanup != -1);
}


/** **************************************************************************
 * Optional init. Can be used to set non-default value to purge_timeout.
 * Registered in netsite/lib/frame/httpd-fn.cpp
 * See: http://docs.sun.com/source/817-1835-10/npgmysaf.html#wp15421
 *
 * magnus.conf:
 *    Init fn="init-request-limits" timeout="300"
 *
 */
int init_request_limits(pblock *pb, Session *sn, Request *rq)
{
    char * param = pblock_findval(PURGE_TIMEOUT, pb);

    if (reqlimit_crit == NULL || hashtable == NULL) {
                                // "should never happen"
        pblock_nvinsert("error", "internal error", pb);
        return REQ_ABORTED;
    }

    if (!param) {
        pblock_nvinsert("error", XP_GetAdminStr(DBT_reqlimitNoTimeout), pb);
        return REQ_ABORTED;
    }

    purge_timeout = atoi(param);

    if (purge_timeout) {
        next_timeout = time(NULL) + purge_timeout;
        log_error(LOG_VERBOSE, "init-request-limits", sn, rq,
                  "purge timeout set to %ds", purge_timeout);

    } else {
         log_error(LOG_VERBOSE, "init-request-limits", sn, rq,
                   "purge timeout disabled.");
    }

    return REQ_PROCEED;
}

