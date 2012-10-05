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

#include <stdio.h>
#include <nspr.h>
#include <unistd.h>
#if !defined(SOLARIS)
#include <getopt.h>
#endif

#include "ldaputil/errors.h"
#include "ldaputil/certmap.h"

#include "LdapServerSet.h"
#include "LdapSessionPool.h"

extern "C" void worker(void *);

PRMonitor *exit_cv;
int alive = 0;

#define MAXTHREADS 32
#define MAXSESSIONS 16
#define MAXOPS 100
#define PROCLOOPS 5

int maxthreads = MAXTHREADS;
int maxsessions = MAXSESSIONS;
int maxops = MAXOPS;
int procloops = PROCLOOPS * 1000;
int verbose = 0;
int testtype = 0;

LdapSessionPool *SessionPool;

const char *filters[] = {
    "(objectclass=*)",
    "(cn=*)",
    "(cn=r*)",
    "(cn=p*)"
};
#define NFILTERS (sizeof(filters) / sizeof(char *))

void
usage(void)
{
    fprintf(stderr, "usage: LdapSessionTest [options] url binddn bindpw\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -v     verbose\n");
    fprintf(stderr, " -t n   Number of threads (default: %d)\n", MAXTHREADS);
    fprintf(stderr, " -s n   Number of sessions in pool (default: %d)\n", MAXSESSIONS);
    fprintf(stderr, " -o n   Number of operations per thread (default: %d)\n", MAXOPS);
    fprintf(stderr, " -l n   Delay between operations (default: %d)\n", PROCLOOPS);
}

main(int argc, char *argv[])
{
    int c;
    char *url;
    char *binddn;
    char *bindpw;
    PRIntervalTime now, then;
    int totalops;
    double elapsed, rate;

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    if ((exit_cv = PR_NewMonitor()) == NULL) {
	fprintf(stderr, "Error creating monitor for exit cv\n");
	exit(1);
    }

    // process args
    while ((c = getopt(argc, argv, "vT:t:s:o:l:")) != EOF) {
	switch (c) {
	case 'T':
	    testtype = atoi(optarg);
	    break;
	case 't':
	    maxthreads = atoi(optarg);
	    break;
	case 's':
	    maxsessions = atoi(optarg);
	    break;
	case 'o':
	    maxops = atoi(optarg);
	    break;
	case 'l':
	    procloops = atoi(optarg) * 1000;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	    usage();
	    exit(1);
	}
    }

    printf("optind = %d, argc = %d\n", optind, argc);
    if (argc - optind < 3) {
	usage();
	exit(2);
    }
    url = argv[optind];
    binddn = argv[optind+1];
    bindpw = argv[optind+2];

    printf("Starting test on \"%s\"\n", url);
    printf(" binddn = \"%s\", bindpw = \"%s\"\n", binddn, bindpw);
    printf("%d threads using %d sessions during %d operations each\n", maxthreads, maxsessions, maxops);
    printf("delay factor = %d\n", procloops / 1000);
    now = PR_IntervalNow();

    // allocate session pool
    LdapServerSet *ServerSet = new LdapServerSet;
    ServerSet->addServer(url, binddn, bindpw, NULL);

    SessionPool = new LdapSessionPool(ServerSet, maxsessions);

    // start threads
    alive = maxthreads;
    for (int i=0; i < maxthreads; i++) {
	int dosearch = 0;

	if (verbose) printf("Creating thread #%d...\n", i);
	if (i % 5 == 0)
	    dosearch = 1;
	if (PR_CreateThread(PR_USER_THREAD,
			worker,
			(void *)(i + (testtype * 1000)),
			PR_PRIORITY_NORMAL,
			PR_LOCAL_THREAD,
			PR_UNJOINABLE_THREAD,
			0) == NULL)
	{
	    fprintf(stderr, "Error creating server thread %d\n", i);
	    exit(1);
	}
    }

    // wait for all the threads to finish
    PR_EnterMonitor(exit_cv);
    PR_Wait(exit_cv, PR_INTERVAL_NO_TIMEOUT);
    PR_ExitMonitor(exit_cv);
    then = PR_IntervalNow();
    totalops = maxops * maxthreads;
    elapsed = PR_IntervalToMilliseconds(then - now) / 1000.0;
    rate = totalops / elapsed;
    fprintf(stderr, "ALL DONE: %d operations in %.2f seconds: %.2f ops/sec\n", totalops,
	elapsed, rate);

    PR_Cleanup();

    exit(0);
}

extern "C" void
worker(void *arg)
{
    LdapSession *ld;
    LdapSearchResult *res;
    int rv;
    int index = (int)arg;
    int type = 0;
    const char *attrs[] = { "c", 0 };

    type = index / 1000;
    index = index % 1000;

    if (verbose)
	fprintf(stderr, "thread #%d starting\n", index);
    // do some random searching in the database
    for (int i=0; i<maxops; i++) {
	ld = SessionPool->get_session();
	if (ld == NULL) {
	    fprintf(stderr, "thread #%d: could not get session.\n", index);
	    break;
	}
	switch (type) {
	case 0:
	    rv = ld->find(ld->getBaseDN(),
			    LDAP_SCOPE_SUBTREE,
			    filters[(i + index) % NFILTERS],
			    attrs,
			    0,
			    res);
	    // forget about the results
	    delete res;
	    if (rv != LDAPU_SUCCESS && rv != LDAPU_ERR_MULTIPLE_MATCHES) {
		fprintf(stderr, "thread #%d: error in find (%d)\n", index, rv);
	    }
	    break;
	case 1:
	    rv = ld->uid_password("chrisk", "wbiedlis", ld->getBaseDN());
	    if (rv != LDAPU_SUCCESS) {
		fprintf(stderr, "thread #%d: error in uid_password (%d)\n", index, rv);
	    }
	    break;
	}
	SessionPool->free_session(ld);

	if (verbose && (i+1) % 25 == 0)
	    fprintf(stderr, "thread #%d @ operation %d\n", index, i+1);
	// fake some heavy processing (nyyyach!!!)
	for (int j=0; j<procloops; j++)
	    ;
	PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
    if (verbose)
	fprintf(stderr, "thread #%d done\n", index);
    PR_EnterMonitor(exit_cv);
    alive--;
    if (alive == 0) // last one
	PR_Notify(exit_cv);
    PR_ExitMonitor(exit_cv);
}
