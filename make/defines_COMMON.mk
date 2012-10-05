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

# Modified 2011 jyri@virkki.com


#! gmake
#
# This file contains the main defines.  They can be overridden by platform 
# defines

RCPSERVER=iws-files
RCPUSER=ftpman

###############
#### TOOLS ####
###############

# Versions
PERL_VER	=v5

# Directories:
WORK_BASE	=$(BUILD_ROOT)/work
INTERNAL_BASE	=$(BUILD_ROOT)/internal
EXTERNAL_BASE	=$(BUILD_ROOT)/external
WORK_ROOT	=$(WORK_BASE)/$(SHIPDIR)
INTERNAL_ROOT	=$(INTERNAL_BASE)/$(SHIPDIR)

# package directories
PACKAGE_ROOT	=$(BUILD_ROOT)/package
ZIPINSTALL_DIR	=$(PACKAGE_ROOT)/zipinstall/$(SHIPDIR)
NATIVEPKG_DIR	=$(PACKAGE_ROOT)/nativepkg/$(SHIPDIR)

# patch package directories
PATCHPKG_DIR	=$(PACKAGE_ROOT)/patches/$(SHIPDIR)

PRSTRMS_LIB= prstrms4
ARES_LIB   = ares3

ifdef DEBUG_BUILD
DEFINES+=-DDEBUG -D_DEBUG
else
ifdef OPTIMIZED_BUILD
DEFINES+=-DNDEBUG
endif
endif

# SmartHeap per-platform disables
USE_SMARTHEAP=

# SmartHeap linkage (run-time or link-time)
ifdef USE_SMARTHEAP
ifeq ($(OS_ARCH),WINNT)
# run-time linkage for NT
LINK_SMARTHEAP=
else
# link-time linkage for Unix
LINK_SMARTHEAP=1
endif
endif

#
#JDK_INFORMATION
#
ifdef JAVA_HOME
JDK_DIR=$(JAVA_HOME)
else
JDK_DIR		= /usr
endif

JAVA		= $(JDK_DIR)/bin/java

#Java defines
ifeq ($(OS_ARCH),WINNT)
CLASSPATH_SEP	= ;
else
CLASSPATH_SEP	= :
endif

JDKCLASSES = $(JDK_DIR)/lib/tools.jar$(CLASSPATH_SEP)$(JDK_DIR)/jre/lib/rt.jar
SYSCLASSPATH = $(JDKCLASSES)

JNI_MD_LIBNAME = jvm
JNI_MD_LIBDIR  = lib
JNI_MD_LIBTYPE = server
#JNI_INCLUDES = -I$(JDK_DIR)/include -I$(JDK_DIR)/include/$(JNI_MD_NAME)
JVM_LIBDIR = $(JDK_DIR)/jre/$(JNI_MD_LIBDIR)/$(JNI_MD_SYSNAME)/$(JNI_MD_LIBTYPE)

# location of compiled java classes and jni headers
JAVA_CLASSES_DIR=$(BUILD_ROOT)/src/java/$(JAVA_OBJDIR)/classes
JAVA_INCLUDE_DIR=$(INTERNAL_ROOT)/include/jni

# JavaScript location
JS_DIR		= $(BUILD_ROOT)/external/js/$(OBJDIR)
JS_LIBNAME	= js

# DEBUG/OPTIMIZE SETTINGS.  Override as needed in the platform definitions
CC_DEBUG=
C_DEBUG=
LD_DEBUG=

# PROFILE SETTINGS. Override as needed in the platform definitions
CC_PROFILE=
LD_PROFILE=

# PURIFY SETTINGS. Override as needed.  Define PURIFY to enable
CC_PURIFY=
PRELINK=

# QUANTIFY SETTINGS. Override as needed in the platform definitions.
CC_QUANTIFY=

ifndef NO_STD_IMPORT
SYSTEM_INC	= -I$(INTERNAL_ROOT)/include -I$(INTERNAL_ROOT)/include/support

SYSTEM_LIBDIRS += $(INTERNAL_ROOT)/lib$(PLATFORM_SUBDIR_SUFFIX)
endif #NO_STD_IMPORT

ifdef USE_ZLIB
SYSTEM_INC+=$(ZLIB_INC)
LIBDIRS+=$(ZLIB_LIBDIR)
SYSTEM_LIB+=$(ZLIB_LIB)
endif

ifdef USE_SMARTHEAP
DEFINES+=-DUSE_SMARTHEAP
endif

# Need to pull in ECC defs from NSS .h files
DEFINES+=-DNSS_ENABLE_ECC

# NSS specific global definitions.
SECURITY_BINARIES = certutil modutil pk12util
SECURITY_COMMON_LIBS = ssl3 smime3 nss3 $(NSPR_LIB)

SECURITY_EXTRA_LIBS = nssckbi
SECURITY_EXTRA_LIBS += nssutil3
SECURITY_EXTRA_LIBS += nssdbm3
SECURITY_EXTRA_LIBS += sqlite3
ifndef BUILD64
SECURITY_EXTRA_LIBS+=jss4
endif
SECURITY_MODULE_LIBS = softokn3

ifdef LINK_SMARTHEAP
LIBDIRS+=$(SMARTHEAP_LIBDIR)
EXE_LIBS+=smartheap_smp smartheapC_smp
endif

ifdef USE_CLIENTLIBS
# unlike other USE_X USE_CLIENTLIBS does not actually link any libraries.
CLIENTLIBS=$(SECURITY_COMMON_LIBS) 
LIBDIRS+=$(NSS_LIBDIR)
INCLUDES+=$(NSS_INC)
endif

# don't change that again
LDAP_LIB_VERSION = 60

ifdef USE_LDAPSDK
INCLUDES+=$(LDAPSDK_INC)
LIBDIRS+=$(LDAPSDK_LIBDIR)
SYSTEM_LIB+=$(LDAP_LIBS)
INCLUDES+=-I$(SASL_INC)
LIBDIRS+=$(SASL_LIBDIR)
SYSTEM_LIB+=$(SASL_LIBS)
endif

ifdef USE_XERCESC
INCLUDES+=$(XERCESC_INC)
LIBDIRS+=$(XERCESC_LIBDIR)
SYSTEM_LIB+=$(XERCESC_LIBS)
endif

ifdef USE_XALANC
INCLUDES+=$(XALANC_INC)
LIBDIRS+=$(XALANC_LIBDIR)
SYSTEM_LIB+=$(XALANC_LIBS)
endif

ifdef USE_PCRE
INCLUDES+=$(PCRE_INC)
LIBDIRS+=$(PCRE_LIBDIR)
SYSTEM_LIB+=$(PCRE_LIBS)
endif

LIBICU_SUFFIX=3
ifdef USE_LIBICU
SYSTEM_INC+=$(ICU_INC)
LIBDIRS+=$(ICU_LIBDIR)
SYSTEM_LIB+=$(ICU_LIBS)
endif

ifdef USE_JDK
DEFINES += -DUSE_JDK=$(USE_JDK) 
endif

ifdef USE_JNI
INCLUDES+=$(JNI_INCLUDES) -I$(JAVA_INCLUDE_DIR)
LIBDIRS+=$(JVM_LIBDIR)
SYSTEM_LIB+=$(JNI_MD_LIBNAME)
endif

ifdef USE_NSPR
LIBDIRS+=$(NSPR_LIBDIR)
SYSTEM_INC+=$(NSPR_INC)
SYSTEM_LIB+=$(NSPR_LIB)
endif

CC_INCL		= $(LOCAL_INC) $(PROJECT_INC) $(SUBSYS_INC) \
		  $(SYSTEM_INC) $(PLATFORM_INC) $(INCLUDES) $(LATE_INCLUDES)

CC_DEFS		= $(LOCAL_DEF) $(PROJECT_DEF) $(SUBSYS_DEF) \
		  $(SYSTEM_DEF) $(PLATFORM_DEF) $(DEFINES)

CC_OPTS		= $(LOCAL_CC_OPTS) $(PROJECT_CC_OPTS) $(SUBSYS_CC_OPTS) \
		  $(SYSTEM_CC_OPTS) $(PLATFORM_CC_OPTS)

C_OPTS          = $(LOCAL_C_OPTS) $(PROJECT_C_OPTS) $(SUBSYS_C_OPTS) \
                  $(SYSTEM_C_OPTS) $(PLATFORM_C_OPTS)

AS_OPTS         = $(LOCAL_AS_OPTS) $(PROJECT_AS_OPTS) $(SUBSYS_AS_OPTS) \
                  $(SYSTEM_AS_OPTS) $(PLATFORM_AS_OPTS)

LD_LIBDIRS_RAW  = $(LOCAL_LIBDIRS) $(PROJECT_LIBDIRS) $(SUBSYS_LIBDIRS) \
                  $(SYSTEM_LIBDIRS) $(PLATFORM_LIBDIRS) $(LIBDIRS) \
		  $(LATE_LIBDIRS)

ifeq ($(OS_ARCH),WINNT) 
LD_LIBDIRS      = $(addprefix /LIBPATH:, $(LD_LIBDIRS_RAW))
else
LD_LIBDIRS      = $(addprefix -L, $(LD_LIBDIRS_RAW))
endif

LD_LIBS_RAW	= $(LOCAL_LIB) $(PROJECT_LIB) $(SUBSYS_LIB) \
		  $(LIBS) $(SYSTEM_LIB) $(PLATFORM_LIB)
ifeq ($(OS_ARCH),WINNT)
LD_LIBS      	= $(addsuffix .lib, $(LD_LIBS_RAW))
else
LD_LIBS        	= $(addprefix -l, $(LD_LIBS_RAW))
endif

ifneq ($(OS_ARCH),WINNT)
LD_RPATHS      	= $(addprefix $(RPATH_PREFIX), $(LD_RPATH))
endif

LD_OPTS		= $(LOCAL_LD_OPTS) $(PROJECT_LD_OPTS) $(SUBSYS_LD_OPTS) \
		  $(SYSTEM_LD_OPTS) $(PLATFORM_LD_OPTS)

LD_FLAGS	= $(LD_PREFLAGS) $(LD_OPTS) $(LD_LIBDIRS) \
		  $(LD_DEBUG) $(LD_PROFILE) $(LD_POSTFLAGS)

CC_FLAGS	= $(CC_PREFLAGS) $(CC_OPTS) $(CC_DEFS) $(CC_INCL) \
		  $(CC_DEBUG) $(CC_PROFILE) $(CC_POSTFLAGS)

C++FLAGS	= $(C++PREFLAGS) $(CC_OPTS) $(CC_DEFS) $(CC_INCL) \
		  $(CC_DEBUG) $(CC_PROFILE) $(C++POSTFLAGS)

C_FLAGS         = $(C_PREFLAGS) $(C_OPTS) $(CC_DEFS) $(CC_INCL) \
                  $(C_DEBUG) $(C_PROFILE) $(C_POSTFLAGS)

AS_FLAGS        = $(AS_PREFLAGS) $(AS_OPTS) $(CC_DEFS) $(CC_INCL) \
                  $(AS_POSTFLAGS)

NOSUCHFILE=/thisfilemustnotexist
