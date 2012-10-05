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

#ifndef _XP_XPUNIT_H
#define _XP_XPUNIT_H

/*
 * xpunit.h
 *
 * Declarations for the unit test framework.
 */

#include "prio.h"
#ifndef _XP_XPPLATFORM_H
#include "xpplatform.h"
#endif
#ifndef _XP_XPTYPES_H
#include "xptypes.h"
#endif

XP_EXTERN_C typedef XPStatus (_XPUnitTestFn)(PRFileDesc *);

XP_IMPORT XPStatus _XP_RegisterUnitTest(const char *, const char *, int, _XPUnitTestFn *);

/*
 * XP_RunUnitTests
 *
 * Run all unit tests.  Unit tests are defined with the XP_UNIT_TEST macro.
 * Diagnostic messages are sent to the passed file descriptor.  Returns the
 * number of unit tests that failed.
 */
XP_IMPORT int XP_RunUnitTests(PRFileDesc *fd);

/*
 * XP_UNIT_TEST
 *
 * Define a unit test.  Unit tests may be defined in any module and invoked
 * with the XP_RUN_UNIT_TESTS macro.  Note that unit tests are only invoked
 * when DEBUG is defined.
 *
 * The unit test body should return XP_SUCCESS if the unit test passes and
 * return XP_FAILURE if the unit test fails.  On failure, the unit test body
 * should use the XP_UNIT_TEST_FD PRFileDesc * to report diagnostic messages.
 *
 * Example:
 *
 * XP_UNIT_TEST(strcmp)
 * {
 *     int result;
 *
 *     result = strcmp("foo", "foo");
 *     if (result) {
 *         PR_fprintf(XP_UNIT_TEST_FD, "strcmp returned %d\n", result);
 *         return XP_FAILURE;
 *     }
 *
 *     return XP_SUCCESS;
 * }
 */
#if defined(DEBUG)
#define XP_UNIT_TEST(name) \
        _XPUnitTestFn _XP_UnitTestFn_##name; \
        static XPStatus _XP_RegisterUnitTest_##name##_rv = _XP_RegisterUnitTest(#name, __FILE__, __LINE__, &_XP_UnitTestFn_##name); \
        XPStatus _XP_UnitTestFn_##name(PRFileDesc *XP_UNIT_TEST_FD)
#else
#define XP_UNIT_TEST(name) \
        static inline XPStatus _XP_UnitTest_##name(PRFileDesc *XP_UNIT_TEST_FD)
#endif

#endif /* _XP_XPUNIT_H */
