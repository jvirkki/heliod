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

/**************************************************************************/
/* CONFIDENTIAL AND PROPRIETARY SOURCE CODE                               */
/* OF NETSCAPE COMMUNICATIONS CORPORATION                                 */
/*                                                                        */
/* Copyright © 1996,1997 Netscape Communications Corporation.  All Rights */
/* Reserved.  Use of this Source Code is subject to the terms of the      */
/* applicable license agreement from Netscape Communications Corporation. */
/*                                                                        */
/* The copyright notice(s) in this Source Code does not indicate actual   */
/* or intended publication of this Source Code.                           */
/**************************************************************************/

#define DATABASE_NAME PRODUCT_I18N_DB ".db"

#ifdef RESOURCE_STR

#undef LIBRARY_NAME
#include "base/dbtbase.h"
#undef LIBRARY_NAME
#include "frame/dbtframe.h"
#undef LIBRARY_NAME
#include "httpdaemon/dbthttpdaemon.h"
#undef LIBRARY_NAME
#include "libaccess/dbtlibaccess.h"
#undef LIBRARY_NAME
#include "safs/dbtsafs.h"
#undef LIBRARY_NAME
#include "shtml/dbtShtml.h"
#ifdef MCC_PROXY
#undef LIBRARY_NAME
#include "../plugins/parray/dbtcarp.h"
#undef LIBRARY_NAME
#include "../plugins/icp/dbticp.h"
#endif
#undef LIBRARY_NAME
#include "../webservd/dbthttpdsrc.h"
#undef LIBRARY_NAME
#include "libsed/dbtlibsed.h"
#undef LIBRARY_NAME
#include "libserverxml/dbtlibserverxml.h"
#undef LIBRARY_NAME
#include "libproxy/dbtlibproxy.h"
#ifdef FEAT_SECRULE
#undef LIBRARY_NAME
#include "libsecrule/dbtlibsecrule.h"
#undef LIBRARY_NAME
#include "libap/dbtlibap.h"
#endif
#undef LIBRARY_NAME
#include "ldaputil/dbtldaputil.h"

static RESOURCE_GLOBAL allxpstr[] = {
  base,
  frame,
  httpdaemon,
  libaccess,
  safs,
  shtml,
#ifdef MCC_PROXY
  carp,
  icp,
#endif
  httpdmain,
  libsed,
  libserverxml,
  libproxy,
#ifdef FEAT_SECRULE
  libsecrule,
  libap,
#endif
  ldaputil,
  0
};

#endif /* ifdef RESOURCE_STR */
