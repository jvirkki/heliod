#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

BUILD_ROOT=../../..
USE_NSPR=1
USE_MAPFILE=1

# disable position independent code to improve execution speed
NO_KPIC=1

include ${BUILD_ROOT}/make/defines.mk

ifeq ($(OS_ARCH),IRIX)
C_DEBUG         = -O2 -OPT:mem_intrinsics=off
endif


MODULE=libdbm

LOCAL_INC+=-I../../include -I../../../lib -I../../../include
LOCAL_DEF+= -DHW_THREADS
LOCAL_DEF+= -DMOZILLA_CLIENT
LOCAL_DEF+= -DLAYERS
LOCAL_DEF+= -DUNIX_LDAP
LOCAL_DEF+= -DNSPR
LOCAL_DEF+= -DNSPR20
LOCAL_DEF+= -DMOCHA
LOCAL_DEF+= -D__DBINTERFACE_PRIVATE 
LOCAL_DEF+= -DBUILDDBM_DLL 


SHIP_PRIVATE_DYNAMIC_LIBRARIES=libdbm

# needed for GAT
EXPORT_TESTS_DIR=httptests
EXPORT_TESTS_DYNAMIC_LIBRARIES=$(SHIP_PRIVATE_DYNAMIC_LIBRARIES)

DLL_TARGET=libdbm
CSRCS=h_bigkey.c 
CSRCS+=h_func.c
CSRCS+=h_log2.c   
CSRCS+=h_page.c   
CSRCS+=hash.c	   
CSRCS+=hash_buf.c 
CSRCS+=h_lock.c 
CSRCS+=hsearch.c  
CSRCS+=mktemp.c   
CSRCS+=ndbm.c	   
CSRCS+=strerror.c 
CSRCS+=nsres.c

#AR_OBJS += 
DLL_OBJS += 	db	   \
        h_bigkey \
        h_func   \
        h_log2   \
        h_page   \
        hash	   \
        hash_buf \
        h_lock \
        hsearch  \
        mktemp   \
        ndbm	   \
        strerror \
        nsres


# this should always be last!
include ${BUILD_ROOT}/make/rules.mk
