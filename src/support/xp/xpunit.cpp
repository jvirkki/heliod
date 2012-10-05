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
 * xpunit.cpp
 *
 * Unit test framework.
 */

#include <stdlib.h>
#include "prprf.h"
#include "xpunit.h"

struct UnitTest {
    struct UnitTest *next;
    const char *name;
    const char *filename;
    int line;
    _XPUnitTestFn *fn;
};

static UnitTest *tests;

XP_EXPORT XPStatus _XP_RegisterUnitTest(const char *name, const char *filename, int line, _XPUnitTestFn *fn)
{
    UnitTest *test = (UnitTest *) malloc(sizeof(UnitTest));
    test->next = tests;
    test->name = name;
    test->filename = filename;
    test->line = line;
    test->fn = fn;
    tests = test;

    return XP_SUCCESS;
}

XP_EXPORT int XP_RunUnitTests(PRFileDesc *fd)
{
    int passed = 0;
    int failed = 0;

    PR_fprintf(fd, "Running unit tests\n");

    UnitTest *test = tests;
    while (test) {
        PR_fprintf(fd, "Running %s from %s:%d\n", test->name, test->filename, test->line);
        XPStatus rv = test->fn(fd);
        if (rv == XP_SUCCESS) {
            PR_fprintf(fd, "PASS: %s passed\n", test->name);
            passed++;
        } else {
            PR_fprintf(fd, "FAILURE: %s failed\n", test->name);
            failed++;
        }
        test = test->next;
    }

    PR_fprintf(fd, "%d test(s) passed\n", passed);
    PR_fprintf(fd, "%d test(s) failed\n", failed);

    int total = passed + failed;
    PR_fprintf(fd, "%.0f%% pass rate\n", total ? passed * 100.0 / total : 100.0);

    if (failed) {
        PR_fprintf(fd, "FAILURE: Some tests failed\n");
    } else {
        PR_fprintf(fd, "SUCCESS: All tests passed\n");
    }

    return failed;
}
