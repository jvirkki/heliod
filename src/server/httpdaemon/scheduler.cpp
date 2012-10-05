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

#include "netsite.h"
#include "support/GenericQueue.h"
#include "base/date.h"
#include "base/util.h"
#include "base/ereport.h"
#include "frame/conf.h"
#include "safs/child.h"
#include "httpdaemon/WebServer.h"
#include "httpdaemon/configuration.h"
#include "httpdaemon/configurationmanager.h"
#include "httpdaemon/logmanager.h"
#include "httpdaemon/scheduler.h"
#include "httpdaemon/dbthttpdaemon.h"
#include "httpdaemon/updatecrl.h"
#include "httpdaemon/vsconf.h"
#include "generated/ServerXMLSchema/SearchEvent.h"
#include "generated/ServerXMLSchema/SearchCollection.h"
#include "generated/ServerXMLSchema/AddDocuments.h"

#ifdef XP_WIN32
#define BAT_SUFFIX ".bat"
#else
#define BAT_SUFFIX
#endif

using ServerXMLSchema::Event;
using ServerXMLSchema::SearchEvent;
using ServerXMLSchema::SearchCollection;
using ServerXMLSchema::AddDocuments;
using ServerXMLSchema::Time;
using ServerXMLSchema::DayOfWeek;
using ServerXMLSchema::Month;
using LIBXSD2CPP_NAMESPACE::String;
using LIBXSD2CPP_NAMESPACE::Integer;
using LIBXSD2CPP_NAMESPACE::IntervalTime;


/*
 * SCHEDULER_PERIOD is the interval in seconds at which events are scheduled.
 * The implementation assumes that scheduler_clock() will always be called at
 * least once every SCHEDULER_PERIOD seconds, even when the system is heavily
 * loaded.
 */
static const int SCHEDULER_PERIOD = 60;

/*
 * scheduler_last_time records the time scheduler_run() was last called.
 */
static time_t scheduler_last_time;

/*
 * scheduler_time_warp is set if the scheduler notices time has gone backwards.
 */
static PRBool scheduler_time_warp;

/*
 * worker_lock synchronizes creation and termination of the worker() thread and
 * protects the worker_rotate_callbacks and worker_commands queues.
 */
static PRLock *worker_lock = PR_NewLock();

/*
 * worker_thread is the PRThread * of the worker() thread or NULL if the
 * worker thread is not running.
 */
static PRThread *worker_thread;

/*
 * worker_rotate_callbacks contains argv[] arrays for post-rotation callback
 * commands.
 */
static GenericQueue<char **> worker_rotate_callbacks(1, PR_TRUE);

/*
 * worker_commands contains command lines that should be executed.
 */
static GenericQueue<char *> worker_commands(1, PR_TRUE);

/*
 * worker_commands_argv contains argv[] arrays for command lines that should be
 * executed.
 */
static GenericQueue<char **> worker_commands_argv(1, PR_TRUE);

/*
 * worker_reopen is set if the server should request that its siblings reopen
 * their log files.
 */
static PRBool worker_reopen;

/*
 * worker_update_crl is set if the CRLs should be updated.
 */
static PRBool worker_update_crl;

/*
 * worker_reconfig is set if the server should be reconfigured.
 */
static PRBool worker_reconfig;

/*
 * worker_restart is set if the server should be restarted.
 */
static PRBool worker_restart;

/*
 * ROTATE_SUFFIX_SIZE is the maximum formatted size of an archive log file
 * suffix.
 */
static const size_t ROTATE_SUFFIX_SIZE = 512;

/*
 * MAX_ADD_DOCS_PARAMS is the maximum number argv[] elements needed to
 * invoke the searchadmin script when adding documents to a collection
 */
static const int MAX_ADD_DOCS_PARAMS = 15;

/*
 * MAX_REINDEX_PARAMS is the maximum number argv[] elements needed to
 * invoke the searchadmin script when reindexing a collection
 */
static const int MAX_REINDEX_PARAMS = 9;


/* ----------------------------- time_hms_cmp ----------------------------- */

static inline int time_hms_cmp(const struct tm *tm1, const struct tm *tm2)
{
    return (tm1->tm_hour - tm2->tm_hour) * 3600 +
           (tm1->tm_min - tm2->tm_min) * 60 +
           (tm1->tm_sec - tm2->tm_sec);
}


/* ---------------------------- time_other_tz ----------------------------- */

/*
 * If the passed UTC time corresponds to a local calendar time that can occur
 * both in the local DST timezone and in the local non-DST timezone,
 * time_other_tz returns the UTC time in the other timezone.  Otherwise,
 * time_other_tz returns the passed UTC time unchanged.
 */

static time_t time_other_tz(time_t t1)
{
    struct tm tm1;
    struct tm tm2;

    util_localtime(&t1, &tm1);

    tm2 = tm1;
    tm2.tm_isdst = !tm2.tm_isdst;

    time_t t2 = mktime(&tm2);

    if (t2 != -1 && !time_hms_cmp(&tm1, &tm2))
        return t2;

    return t1;
}


/* ----------------------------- time_is_now ------------------------------ */

PRBool time_is_now(const Time *time, time_t now, struct tm *tm_now)
{
    // Skip the event if this is the second occurrence of this calendar time
    // due to a DST change
    time_t dst = time_other_tz(now);
    if (dst < now)
        return PR_FALSE;

    // Check hour in hh:mm
    if (atoi(time->timeOfDay) != tm_now->tm_hour)
        return PR_FALSE;

    // Check minute in hh:mm
    if (const char *colon = strchr(time->timeOfDay, ':')) {
        if (atoi(colon + 1) != tm_now->tm_min)
            return PR_FALSE;
    }

    // Check day of week
    if (const DayOfWeek *day = time->getDayOfWeek()) {
        int wday;
        switch (*day) {
        case DayOfWeek::DAYOFWEEK_SUN: wday = 0; break;
        case DayOfWeek::DAYOFWEEK_MON: wday = 1; break;
        case DayOfWeek::DAYOFWEEK_TUE: wday = 2; break;
        case DayOfWeek::DAYOFWEEK_WED: wday = 3; break;
        case DayOfWeek::DAYOFWEEK_THU: wday = 4; break;
        case DayOfWeek::DAYOFWEEK_FRI: wday = 5; break;
        case DayOfWeek::DAYOFWEEK_SAT: wday = 6; break;
        }
        if (wday != tm_now->tm_wday)
            return PR_FALSE;
    }

    // Check day of month
    if (const Integer *mday = time->getDayOfMonth()) {
        if (*mday != tm_now->tm_mday)
            return PR_FALSE;
    }

    // Check month
    if (const Month *month = time->getMonth()) {
        int mon;
        switch (*month) {
        case Month::MONTH_JAN: mon = 0; break;
        case Month::MONTH_FEB: mon = 1; break;
        case Month::MONTH_MAR: mon = 2; break;
        case Month::MONTH_APR: mon = 3; break;
        case Month::MONTH_MAY: mon = 4; break;
        case Month::MONTH_JUN: mon = 5; break;
        case Month::MONTH_JUL: mon = 6; break;
        case Month::MONTH_AUG: mon = 7; break;
        case Month::MONTH_SEP: mon = 8; break;
        case Month::MONTH_OCT: mon = 9; break;
        case Month::MONTH_NOV: mon = 10; break;
        case Month::MONTH_DEC: mon = 11; break;
        }
        if (mon != tm_now->tm_mon)
            return PR_FALSE;
    }

    return PR_TRUE;
}


/* ----------------------------- event_period ----------------------------- */

int event_period(const IntervalTime *interval)
{
    int period = 0;

    if (interval) {
        // Express the event interval as a multiple of SCHEDULER_PERIOD seconds
        period = interval->getSecondsValue() + SCHEDULER_PERIOD - 1;
        period -= (period % SCHEDULER_PERIOD);
    }

    return period;
}


/* ----------------------------- event_is_now ----------------------------- */

template <class T>
PRBool event_is_now(const T *event, time_t now, struct tm *tm_now)
{
    if (event->enabled) {
        int period = event_period(event->getInterval());
        if (period && (now % period) == 0)
            return PR_TRUE;

        for (int i = 0; i < event->getTimeCount(); i++) {
            if (time_is_now(event->getTime(i), now, tm_now))
                return PR_TRUE;
        }
    }

    return PR_FALSE;
}


/* ---------------------------- rotate_suffix ----------------------------- */

static int rotate_suffix(const Configuration *config, struct tm *tm, char *buf, int size)
{
    int rv = util_strlftime(buf, size, config->log.archiveSuffix, tm);
    PR_ASSERT(rv != -1);
    return rv;
}


/* -------------------------- rotate_suffix_cmp --------------------------- */

static int rotate_suffix_cmp(const Configuration *config, time_t t1, time_t t2)
{
    struct tm tm1;
    struct tm tm2;
    char suffix1[ROTATE_SUFFIX_SIZE];
    char suffix2[ROTATE_SUFFIX_SIZE];

    util_localtime(&t1, &tm1);
    util_localtime(&t2, &tm2);
    rotate_suffix(config, &tm1, suffix1, sizeof(suffix1));
    rotate_suffix(config, &tm2, suffix2, sizeof(suffix2));

    return strcmp(suffix1, suffix2);
}


/* ------------------------ rotate_suffix_conflict ------------------------ */

static PRBool rotate_suffix_conflict(const Configuration *config, const Event *event, time_t nondst)
{
    // If this log rotation event is scheduled to run at a certain interval...
    int period = event_period(event->getInterval());
    if (period) {
        // If the current time is a non-DST local calendar time that has a DST
        // counterpart...
        int dst = time_other_tz(nondst);
        if (dst < nondst) {
            // If the rotation suffix normally changes for each interval...
            if (rotate_suffix_cmp(config, nondst, nondst + period) &&
                rotate_suffix_cmp(config, dst, dst - period))
            {
                // If the rotation suffix in DST and out of DST are the same...
                if (!rotate_suffix_cmp(config, dst, nondst))
                    return PR_TRUE;
            }
        }
    }

    return PR_FALSE;
}


/* ------------------------- rotate_callback_argv ------------------------- */

static char **rotate_callback_argv(const String *command, const char *log)
{
    char **argv = NULL;

    if (command) {
        // Parse the command line
        argv = util_argv_parse(*command);
        if (argv) {
            // Add the log file filename to the end of the command line
            int i;
            argv = util_env_create(argv, 1, &i);
            if (argv) {
                argv[i++] = STRDUP(log);
                argv[i] = NULL;
            }
        }
    }

    return argv;
}


/* -------------------------- command_shell_exec -------------------------- */

static void command_shell_exec(const char * const *argv, const char *command)
{
    const char *program = argv ? argv[0] : command;

    ereport(LOG_VERBOSE, "Event scheduler executing %s", program);

    PRStatus rv = PR_FAILURE;

    Child *child = child_create(NULL, NULL, program);
    if (child) {
        if (argv) {
            rv = child_shell_argv(child, argv, NULL, NULL, PR_INTERVAL_NO_TIMEOUT);
        } else {
            rv = child_shell(child, NULL, NULL, PR_INTERVAL_NO_TIMEOUT);
        }
        if (rv == PR_SUCCESS) {
            ereport(LOG_VERBOSE, "Waiting for %s to terminate or detach", program);
            child_wait(child);
            ereport(LOG_VERBOSE, "%s terminated or detached", program);
        }
    }

    if (rv == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_Scheduler_CannotExecXReasonY), program, system_errmsg());
}


/* ----------------------------- command_exec ----------------------------- */

static void command_exec(const char * const *argv)
{
    const char *program = argv[0];

    ereport(LOG_VERBOSE, "Event scheduler executing %s", program);

    PRStatus rv = PR_FAILURE;

    Child *child = child_create(NULL, NULL, program);
    if (child) {
        if (argv) {
            rv = child_exec(child, argv, NULL, NULL, PR_INTERVAL_NO_TIMEOUT);
        }
        if (rv == PR_SUCCESS) {
            ereport(LOG_VERBOSE, "Waiting for %s to terminate or detach", program);
            child_wait(child);
            ereport(LOG_VERBOSE, "%s terminated or detached", program);
        }
    }

    if (rv == PR_FAILURE)
        ereport(LOG_FAILURE, XP_GetAdminStr(DBT_Scheduler_CannotExecXReasonY), program, system_errmsg());
}


/* -------------------------------- worker -------------------------------- */

PR_BEGIN_EXTERN_C

static void worker(void *arg)
{
    for (;;) {
        char **rotate_callback = NULL;
        char *command = NULL;
        char **command_argv = NULL;
        PRBool reopen = PR_FALSE;
        PRBool update_crl = PR_FALSE;
        PRBool reconfig = PR_FALSE;
        PRBool restart = PR_FALSE;

        // Look for one piece of work
        PR_Lock(worker_lock);

        PR_ASSERT(worker_thread == PR_CurrentThread());

        ;

        if (worker_rotate_callbacks.remove(rotate_callback) != PR_SUCCESS &&
            worker_commands.remove(command) != PR_SUCCESS &&
            worker_commands_argv.remove(command_argv) != PR_SUCCESS &&
            !(reopen = PR_AtomicSet(&worker_reopen, PR_FALSE)) &&
            !(update_crl = PR_AtomicSet(&worker_update_crl, PR_FALSE)) &&
            !(reconfig = PR_AtomicSet(&worker_reconfig, PR_FALSE)) &&
            !(restart = PR_AtomicSet(&worker_restart, PR_FALSE)))
        {
            // No work left for us to do, so we'll exit
            worker_thread = NULL;
        }

        PR_Unlock(worker_lock);

        // Do one piece of work
        if (rotate_callback) {
            command_exec(rotate_callback);
            util_env_free(rotate_callback);
        } else if (command) {
            command_shell_exec(NULL, command);
            PERM_FREE(command);
        } else if (command_argv) {
            command_shell_exec(command_argv, NULL);
            util_env_free(command_argv);
        } else if (reopen) {
            WebServer::RequestReopenLogs();
        } else if (update_crl) {
            crl_check_updates(PR_TRUE);
        } else if (reconfig) {
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_ServerReconfig));
            WebServer::RequestReconfiguration();
        } else if (restart) {
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_ServerRestart));
            WebServer::RequestRestart();
        } else {
            break;
        }
    }
}

PR_END_EXTERN_C


/* ---------------------------- worker_create ----------------------------- */

static void worker_create()
{
    // N.B. worker_lock must be held
    if (!worker_thread) {
        worker_thread = PR_CreateThread(PR_SYSTEM_THREAD, worker, 0,
                                        PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                                        PR_UNJOINABLE_THREAD, 0);
        if (!worker_thread)
            ereport(LOG_FAILURE, XP_GetAdminStr(DBT_Scheduler_CannotCreateThreadReasonX), system_errmsg());
    }
}


/* ---------------------- scheduler_rotate_callback ----------------------- */

static void scheduler_rotate_callback(const char *newname, const char *oldname)
{
    Configuration *config = ConfigurationManager::getConfiguration();
    if (config) {
        char **argv = rotate_callback_argv(config->log.getArchiveCommand(), oldname);
        if (argv) {
            PR_Lock(worker_lock);
            worker_rotate_callbacks.add(argv);
            worker_create();
            PR_Unlock(worker_lock);
        }

        config->unref();
    }
}


/* --------------------------- scheduler_reopen --------------------------- */

static void scheduler_reopen()
{
    PR_Lock(worker_lock);
    worker_reopen = PR_TRUE;
    PR_Unlock(worker_lock);    
}


/* ----------------------- scheduler_rotate_access ------------------------ */

static void scheduler_rotate_access(const char *suffix)
{
    ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_RotatingAccess));

    LogManager::setRotateCallback(scheduler_rotate_callback);
    LogManager::rotate(suffix);

    ereport(LOG_VERBOSE, "Archived access log files with suffix \"%s\"", suffix);
}


/* ------------------------ scheduler_rotate_error ------------------------ */

static void scheduler_rotate_error(const char *suffix)
{
    ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_RotatingError));

    ereport_set_rotate_callback(scheduler_rotate_callback);
    ereport_rotate(suffix);

    ereport(LOG_VERBOSE, "Archived server log files with suffix \"%s\"", suffix);
}


/* --------------------------- scheduler_rotate --------------------------- */

void scheduler_rotate(void *context)
{
    PR_ASSERT(context == NULL);

    const Configuration *config = ConfigurationManager::getConfiguration();
    if (config) {
        time_t t = time(NULL);
        struct tm tm;
        util_localtime(&t, &tm);

        char suffix[ROTATE_SUFFIX_SIZE];
        rotate_suffix(config, &tm, suffix, sizeof(suffix));

        scheduler_rotate_access(suffix);
        scheduler_rotate_error(suffix);
        scheduler_reopen();

        config->unref();
    }
}


/* -------------------------- scheduler_command --------------------------- */

void scheduler_command(const char *cmdline)
{
    PR_Lock(worker_lock);
    worker_commands.add(PERM_STRDUP(cmdline));
    worker_create();
    PR_Unlock(worker_lock);
}


/* ------------------------ scheduler_command_argv ------------------------ */

void scheduler_command_argv(char **argv)
{
    PR_Lock(worker_lock);
    worker_commands_argv.add(argv);
    worker_create();
    PR_Unlock(worker_lock);
}


/* ------------------------- scheduler_update_crl ------------------------- */

static void scheduler_update_crl()
{
    PR_Lock(worker_lock);
    worker_update_crl = PR_TRUE;
    worker_create();
    PR_Unlock(worker_lock);
}


/* -------------------------- scheduler_reconfig -------------------------- */

static void scheduler_reconfig()
{
    PR_Lock(worker_lock);
    worker_reconfig = PR_TRUE;
    worker_create();
    PR_Unlock(worker_lock);
}


/* -------------------------- scheduler_restart --------------------------- */

static void scheduler_restart()
{
    PR_Lock(worker_lock);
    worker_restart = PR_TRUE;
    worker_create();
    PR_Unlock(worker_lock);
}


/* ------------------------- get_searchadmin_path ------------------------- */

static NSString get_searchadmin_path()
{
    NSString search_command_path;
    search_command_path.printf("%s/%s/searchadmin"BAT_SUFFIX, conf_getglobals()->Vnetsite_root, PRODUCT_PRIVATE_BIN_SUBDIR);
    return search_command_path;
}

/* ----------------------- scheduler_add_documents ------------------------ */

static void scheduler_add_documents(const SearchEvent *search_event, const char *vs_name, const char *collection_name, struct tm *tm_now)
{
    const AddDocuments *add_docs = search_event->getAddDocuments();

    const char *pattern = add_docs->pattern;
    const char *default_encoding = NULL;
    const char *recursive = add_docs->includeSubdirectories ? "true" : "false";

    if (add_docs->getDefaultEncoding())
        default_encoding = *add_docs->getDefaultEncoding();

    int i = 0;
    char **argv = util_env_create(NULL, MAX_ADD_DOCS_PARAMS, &i);

    argv[i++] = STRDUP(get_searchadmin_path());
    argv[i++] = STRDUP("adddocs");
    argv[i++] = STRDUP("-instance");
    argv[i++] = STRDUP(conf_getglobals()->Vserver_id);
    argv[i++] = STRDUP("-vs");
    argv[i++] = STRDUP(vs_name);
    argv[i++] = STRDUP("-collection");
    argv[i++] = STRDUP(collection_name);
    argv[i++] = STRDUP("-recursive");
    argv[i++] = STRDUP(recursive);
    argv[i++] = STRDUP("-pattern");
    argv[i++] = STRDUP(pattern);

    if (default_encoding) {
        argv[i++] = STRDUP("-enc");
        argv[i++] = STRDUP(default_encoding);
    }

    argv[i] = NULL;
    PR_ASSERT(i < MAX_ADD_DOCS_PARAMS);

    scheduler_command_argv(argv);
}


/* -------------------------- scheduler_reindex --------------------------- */

static void scheduler_reindex(const char *vs_name, const char *collection_name, struct tm *tm_now)
{
    int i = 0;
    char **argv = util_env_create(NULL, MAX_REINDEX_PARAMS, &i);

    argv[i++] = STRDUP(get_searchadmin_path());
    argv[i++] = STRDUP("reindexcoll");
    argv[i++] = STRDUP("-instance");
    argv[i++] = STRDUP(conf_getglobals()->Vserver_id);
    argv[i++] = STRDUP("-vs");
    argv[i++] = STRDUP(vs_name);
    argv[i++] = STRDUP("-collection");
    argv[i++] = STRDUP(collection_name);
    argv[i] = NULL;

    PR_ASSERT(i < MAX_REINDEX_PARAMS);

    scheduler_command_argv(argv);
}


/* ------------------- scheduler_process_search_events -------------------- */

static void scheduler_process_search_events(const VirtualServer *vs, time_t now, struct tm *tm_now)
{
    // Check every search collection for events scheduled to run now
    for (int i = 0; i < vs->getSearchCollectionCount(); i++) {
        const SearchCollection *coll = vs->getSearchCollection(i);

        for (int j = 0; j < coll->getSearchEventCount(); j++) {
            const SearchEvent *event = coll->getSearchEvent(j);

            if (event_is_now(event, now, tm_now)) {
                // Add documents?
                if (event->getAddDocuments())
                    scheduler_add_documents(event, vs->name, coll->name, tm_now);

                // Reindex?
                if (event->reindex)
                    scheduler_reindex(vs->name, coll->name, tm_now);
            }
        }
    }
}


/* ---------------------------- scheduler_run ----------------------------- */

static void scheduler_run(time_t now)
{
    /*
     * N.B. We're running on the clock thread.  If you need to do any heavy
     * lifting, do it on the worker() thread.
     */

    // Acquire access to the current configuration
    const Configuration *config = ConfigurationManager::getConfiguration();
    if (!config)
        return;

    /*
     * Figure out what work needs to be done
     */

    PRBool skip_rotation = PR_FALSE;
    PRBool rotate_access = PR_FALSE;
    PRBool rotate_error = PR_FALSE;
    GenericQueue<const char *> commands(1, PR_TRUE);
    PRBool update_crl = PR_FALSE;
    PRBool reconfig = PR_FALSE;
    PRBool restart = PR_FALSE;

    struct tm tm_now;
    util_localtime(&now, &tm_now);

    // Check every event that's scheduled to run now
    for (int i = 0; i < config->getEventCount(); i++) {
        const Event *event = config->getEvent(i);

        if (event_is_now(event, now, &tm_now)) {
            // Rotate logs?
            if (event->rotateAccessLog || event->rotateLog) {
                if (rotate_suffix_conflict(config, event, now)) {
                    skip_rotation = PR_TRUE;
                } else {
                    if (event->rotateAccessLog)
                        rotate_access = PR_TRUE;
                    if (event->rotateLog)
                        rotate_error = PR_TRUE;
                }
            }

            // Run event commands?
            for (int i = 0; i < event->getCommandCount(); i++)
                commands.add(*event->getCommand(i));

            // Update CRLs?
            if (event->updateCrl)
                update_crl = PR_TRUE;

            // Reconfigure server?
            if (event->reconfig)
                reconfig = PR_TRUE;

            // Restart server?
            if (event->restart)
                restart = PR_TRUE;
        }
    }

    /*
     * Process events
     */

    // Rotate logs?
    char suffix[ROTATE_SUFFIX_SIZE];
    rotate_suffix(config, &tm_now, suffix, sizeof(suffix));
    if (rotate_access || rotate_error) {
        // Rotate access logs?
        if (rotate_access)
            scheduler_rotate_access(suffix);

        // Rotate error logs?
        if (rotate_error)
            scheduler_rotate_error(suffix);

        // Instruct our siblings to reopen their log files if we rotated them
        if (rotate_access || rotate_error)
            scheduler_reopen();
    } else if (skip_rotation) {
        // We're skipping log rotation due to a DST change
        ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_LogSuffixXConflict), suffix);
    }

    // Run event commands?
    const char *command;
    while (commands.remove(command) == PR_SUCCESS)
        scheduler_command(command);

    // Update CRLs?
    if (update_crl)
        scheduler_update_crl();

    // Reconfigure server?
    if (reconfig)
        scheduler_reconfig();

    // Restart server?
    if (restart)
        scheduler_restart();

    // Check every virtual server for such search collections that have events
    // scheduled to run now
    for (int si = 0; si < config->getVSCount(); si++) {
        const VirtualServer *vs = config->getVS(si);
        scheduler_process_search_events(vs, now, &tm_now);
    }

    // Release access to the current configuration
    config->unref();
}


/* ---------------------------- log_time_jump ----------------------------- */

static void log_time_jump(int dbt, time_t t1, time_t t2)
{
    char f1[64];
    char f2[64];
    date_format_time(t1, date_format_locale, f1, sizeof(f1));
    date_format_time(t2, date_format_locale, f2, sizeof(f2));
    ereport(LOG_WARN, XP_GetAdminStr(dbt), f1, f2);
}


/* --------------------------- scheduler_clock ---------------------------- */

void scheduler_clock(void *context)
{
    PR_ASSERT(context == NULL);

    // Get the current time as a multiple of SCHEDULER_PERIOD seconds
    time_t now = time(NULL);
    now -= (now % SCHEDULER_PERIOD);

    if (scheduler_last_time == 0)
        scheduler_last_time = now;

    // It's astounding, time is fleeting
    if (now < scheduler_last_time - SCHEDULER_PERIOD) {
        if (!scheduler_time_warp) {
            scheduler_time_warp = PR_TRUE;
            log_time_jump(DBT_Scheduler_TimeJumpBackwardXToY, scheduler_last_time, now);
        }
    } else if (now > scheduler_last_time + SCHEDULER_PERIOD) {
        log_time_jump(DBT_Scheduler_TimeJumpForwardXToY, scheduler_last_time, now);
    }

    // If it's time to schedule the next set of events...
    if (now > scheduler_last_time) {
        if (scheduler_time_warp) {
            scheduler_time_warp = PR_FALSE;
            ereport(LOG_INFORM, XP_GetAdminStr(DBT_Scheduler_TimeCaughtUp));
        }
        scheduler_run(now);
        scheduler_last_time = now;
    }
}
