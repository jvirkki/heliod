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

include $(BUILD_ROOT)/make/defines_UNIX.mk
include $(BUILD_ROOT)/make/defines_SOLARIS.mk

LOCAL_COPY=0

SUNWSPRO_DIR = /usr/dist/share/sunstudio_sparc,v11.0/SUNWspro

# Tool locations
C++C		=$(PRE_CC) $(SUNWSPRO_DIR)/bin/CC
CC		=$(PRE_CC) $(SUNWSPRO_DIR)/bin/CC
C               =$(PRE_C)  $(SUNWSPRO_DIR)/bin/cc
AS              =$(SUNWSPRO_DIR)/bin/CC
AR              =/usr/ccs/bin/ar
RANLIB          =/usr/ccs/bin/ranlib
YACC            =/usr/ccs/bin/yacc
LD		=$(PRE_LD) /usr/ccs/bin/ld
PROFILER	=$(SUNWSPRO_DIR)/bin/profile
FTP		=/usr/bin/ftp
PERL		=/usr/bin/perl
LINT		=$(SUNWSPRO_DIR)/bin/lint
STRIP		=/usr/ccs/bin/strip -x
SPEC2MAP	=/usr/lib/abi/spec2map -a sparc

ifdef DEBUG_BUILD
CC_DEBUG	= -g -xs
C_DEBUG         = -g
else
CC_DEBUG        = -dalign -xO4
C_DEBUG         = -dalign -xO4
endif

BASEFLAGS = -xtarget=ultra -xarch=v8plus

JNI_MD_NAME	= solaris
JNI_MD_SYSNAME  = sparc
JNI_MD_SYSNAME64 = sparcv9

LD_DYNAMIC	= -G
LD_SYMBOLIC	= -Bsymbolic
ARFLAGS		= -r
PLATFORM_DEF	= -DSVR4 -DSYSV -DSOLARIS -D_REENTRANT
PLATFORM_LIB	= $(PRE_PLATFORM_LIB) pthread socket nsl dl posix4 kstat Crun Cstd
PLATFORM_CC_OPTS = -mt $(BASEFLAGS)
PLATFORM_C_OPTS  = -mt $(BASEFLAGS)
PLATFORM_AS_OPTS = -mt -D_ASM $(BASEFLAGS)
PLATFORM_LD_OPTS = -mt -norunpath
RPATH_PREFIX = -R 
RPATH_ORIGIN = \$$ORIGIN
ifndef NO_KPIC
PLATFORM_CC_OPTS += -KPIC
PLATFORM_C_OPTS  += -Kpic
PLATFORM_AS_OPTS += -KPIC
endif
SYSTEM_LIBDIRS += $(SUNWSPRO_DIR)/lib

# Mapfile generation.  Set USE_MAPFILE=1 to require a mapfile for each so.
#USE_MAPFILE=1
MAPFILE_SUFFIX=mapfile
SPECFILE_SUFFIX=spec
VERSIONSFILE_SUFFIX=versions

# template database location.
ifndef NO_STD_DATABASE_DEFINE
TEMPLATE_DATABASE_DIR=$(OBJDIR)
endif

ifdef TEMPLATE_DATABASE_DIR
CC_FLAGS += -ptr$(TEMPLATE_DATABASE_DIR)
LD_FLAGS += -ptr$(TEMPLATE_DATABASE_DIR)
endif

ifndef BUILD64
# libCld provides -compat=4 support for plugins
FEAT_CLD=1
PLATFORM_LIB += C
SYSTEM_LIBDIRS += $(SUNWSPRO_DIR)/lib/CC4
endif

SETUPSDK_JNIDIR        = Unix/Solaris/Sparc

# Enables OS specific stats collection
FEAT_PLATFORM_STATS=1

# Enables creation of solaris packages
FEAT_OS_NATIVE_PKG=1
# Enables creation of solaris patch packages
FEAT_OS_PATCH_PKG=1

# Add platform specific NSS modules
ifndef BUILD64
SECURITY_MODULE_LIBS += freebl_32fpu_3 freebl_32int_3 freebl_32int64_3
endif

# Add platform specific JES patch dependencies
REQUIRE_LIST = ""
