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


BUILD_ROOT=../../..
MODULE=httpdBinary
USE_NSPR=1
USE_LDAPSDK=1
USE_CLIENTLIBS=1
USE_LIBICU=1
USE_XERCESC=1
USE_XALANC=1
USE_SMARTHEAP=1
#USE_MAPFILE=1
USE_ZLIB=1
USE_PCRE=1

# disable position independent code to improve execution speed
NO_KPIC=1

#PURECOV=1

include $(BUILD_ROOT)/make/defines.mk

all::

LOCAL_DEF+= -DENCRYPT_PASSWORDS -DBUILD_DLL 
LOCAL_INC+= -I.. -I../../support
LOCAL_LIBDIRS+= $(OBJDIR) 
LOCAL_LIBDIRS+= ../libsi18n/$(OBJDIR) 
LOCAL_LIBDIRS+= ../libcrypt/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/libdbm/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/NsprWrap/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/filecache/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/threadpool/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/time/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/support/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/libxsd2cpp/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/xp/$(OBJDIR)
LOCAL_LIBDIRS+= ../../support/ares/$(OBJDIR)
LOCAL_LIBDIRS+= ../httpparser/$(OBJDIR)
LOCAL_LIBDIRS+= ../libsed/$(OBJDIR)
LOCAL_LIBDIRS+= ../libserverxml/$(OBJDIR)
ifdef FEAT_SECRULE
LOCAL_LIBDIRS+= ../libap/$(OBJDIR)
LOCAL_LIBDIRS+= ../libsecrule/$(OBJDIR)
endif


ifeq ($(OS_ARCH), WINNT)
LOCAL_LIBDIRS+= ../libnt/$(OBJDIR)
LOCAL_LIB+= libnt
endif

ifeq ($(OS_ARCH), IRIX)
EXE_LIBS+=support 
endif

# extra link flags needed for proper operation of inprocess JVM
ifeq ($(OS_ARCH), AIX)
EXE_EXTRA += -bM:UR -bmaxdata:0xa0000000/dsa
endif

DLLVER=40
DLL_DESC="Http daemon"
DLL_COPYRIGHT="Copyright (c) 1994-1999 Netscape Communications Corporation"
DLL_TARGET=$(DAEMON_DLL)
DLL_OBJS=

# template instantiation
ifeq ($(OS_ARCH),OSF1)
LD_FLAGS += -ptr ../httpdaemon/$(OBJDIR)
endif
ifeq ($(OS_ARCH),SunOS)
#LD_FLAGS += -ptr../ldaputil/$(OBJDIR)
#LD_FLAGS += -ptr../httpdaemon/$(OBJDIR)
ifdef FEAT_SECRULE
#LD_FLAGS += -ptr../libap/$(OBJDIR)
#LD_FLAGS += -ptr../libsecrule/$(OBJDIR)
endif
endif

DLL_LIBS+=$(CLIENTLIBS)
DLL_LIBS+=nsfc nstp nstime support nsprwrap xp
DLL_LIBS+=libcrypt libsi18n httpparser sed
DLL_LIBS+=$(ARES_LIB)
DLL_LIBS+=$(Z_LIB)
DLL_LIBS+=$(PCRE_LIB)
DLL_LIBS+=serverxml
DLL_LIBS+=ServerXMLSchema
DLL_LIBS+=xsd2cpp
ifdef FEAT_SECRULE
DLL_LIBS+=libap
DLL_LIBS+=libsecrule
endif

ifeq ($(OS_ARCH), WINNT)
DLL_OBJS+=ntmagnus
DLL_LIBS+=user32 shell32
endif

# Pick up objects we want to force into the daemon dll.
include ../base/baseobjs.mk
include ../frame/frameobjs.mk
include ../safs/safsobjs.mk
include ../httpdaemon/daemonobjs.mk
include ../libaccess/accessobjs.mk
include ../libadmin/adminobjs.mk
include ../ldaputil/ldaputilobjs.mk
include ../libdrcache/drcacheobjs.mk
include ../libproxy/proxyobjs.mk
include ../shtml/shtmlobjs.mk
ifdef FEAT_SECRULE
include ../libap/libapobjs.mk
include ../libsecrule/libsecruleobjs.mk
endif

DLL_EXTRA+=$(addprefix ../base/$(OBJDIR)/, $(BASEOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../frame/$(OBJDIR)/, $(FRAMEOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../safs/$(OBJDIR)/, $(SAFSOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../httpdaemon/$(OBJDIR)/, $(DAEMONOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../libaccess/$(OBJDIR)/, $(ACCESSOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../libadmin/$(OBJDIR)/, $(ADMINOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../ldaputil/$(OBJDIR)/, $(LDAPUTILOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../libdrcache/$(OBJDIR)/, $(DRCACHEOBJ:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../libproxy/$(OBJDIR)/, $(PROXYOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../shtml/$(OBJDIR)/, $(SHTMLOBJS:=.$(OBJ)))
ifdef FEAT_SECRULE
DLL_EXTRA+=$(addprefix ../libap/$(OBJDIR)/, $(LIBAPOBJS:=.$(OBJ)))
DLL_EXTRA+=$(addprefix ../libsecrule/$(OBJDIR)/, $(LIBSECRULEOBJS:=.$(OBJ)))
endif


ifeq ($(OS_ARCH), WINNT)
DLL2_TARGET=ns-httpd30
DLL2_OBJS=nsapi30_dll
DLL2_LIBS+=$(DAEMON_DLL)
DLL2_COPYRIGHT="Copyright (c) 1994-1999 Netscape Communications Corporation"

DLL3_TARGET=ns-httpd35
DLL3_OBJS=nsapi35_dll
DLL3_LIBS+=$(DAEMON_DLL)
DLL3_COPYRIGHT="Copyright (c) 1994-1999 Netscape Communications Corporation"

DLL4_TARGET=ns-httpd36
DLL4_OBJS=nsapi36_dll
DLL4_LIBS+=$(DAEMON_DLL)
DLL4_COPYRIGHT="Copyright (c) 1994-1999 Netscape Communications Corporation"
endif

ifdef LINK_SMARTHEAP
DLL2_TARGET=smartheap_smp
DLL2_OBJS=smartheapdummy
DLL2_COPYRIGHT="Copyright (c) 2000 Sun Microsystems, Inc."
endif

EXE_TARGET=$(PRODUCT_DAEMON_BIN)
ifneq ($(OS_ARCH), WINNT)
EXE_LIBS+=libdbm
endif

EXE_LIBDIRS=$(OBJDIR)
# link security libs into DAEMON_DLL
DLL_LIBS+=$(SSLDAP_LIB) libdbm

EXE_LIBS+=$(DAEMON_DLL)
EXE_LIBS+=libsi18n

ifeq ($(OS_ARCH), WINNT)
EXE_OBJS=ntmain
EXE_LIBS+=nstime
EXE_LIBS+=user32
EXE_RES=password
else

# unix
EXE_OBJS=main
EXE_LIBS+=nsprwrap

ifeq ($(OS_ARCH), HP-UX)
# Added Jan26 for Java problem.
LOCAL_LIBDIRS += $(JVM_LIBDIR)
EXE_LIBS+=$(JNI_MD_LIBNAME)
# -N option is needed for 32-bit applications to set heap beyond 2GB.
# This is useful for JVM.
# See link http://www.hp.com/products1/unix/java/infolibrary/prog_guide/expanding_memory.html
EXE_EXTRA+=-N
endif
endif

SHIP_PRIVATE_BINARIES=$(EXE_TARGET)
SHIP_PUBLIC_DYNAMIC_LIBRARIES=$(DLL_TARGET)
SHIP_PRIVATE_DYNAMIC_LIBRARIES=$(DLL2_TARGET) $(DLL3_TARGET) $(DLL4_TARGET)

include $(BUILD_ROOT)/make/rules.mk

# Make webservd exempt as documented in
# http://www.ibm.com/developerworks/java/jdk/aix/j532/sdkguide.aix32.html#sed
ifeq ($(OS_ARCH), AIX)
link::
	$(SEDMGR) -c exempt $(OBJDIR)/$(EXE_TARGET)
endif
