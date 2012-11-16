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

# defines for Web Server

BRAND_NAME = "heliod"

#build Numbers
# major, minor are numeric
#
# NOTE: Default server header is defined in schema/heliod-web-server_1_0.xsd
# so be sure to update version there as well whenever changing these.

VER_MAJOR=0
VER_MINOR=2

MAJOR_VERSION="$(VER_MAJOR)"
MINOR_VERSION="$(VER_MINOR)"

DAEMON_DLL_VER=40

#######################################
# Product name and feature definition #
#######################################

# HTTP Server: header prefix (must not contain spaces)
PRODUCT_HEADER_ID = "Web-Server"
PRODUCT_NAME = "Web Server"

#
# optional features. do not define to turn off.

# multiprocess mode
FEAT_MULTIPROCESS=1
# internal log rotation
FEAT_INTERNAL_LOG_ROTATION=1
# unlimited operation (if not defined, some restrictions apply)
FEAT_NOLIMITS=1
# can be tuned
FEAT_TUNEABLE=1
# daemonstats subsystem (used for tuning)
FEAT_DAEMONSTATS=1
# cluster administration
FEAT_CLUSTER=1
# dynamic groups
FEAT_DYNAMIC_GROUPS=1
# password policies (expiration)
FEAT_PASSWORD_POLICIES=1
# upgrade/migration capability
FEAT_UPGRADE=1
# PKCS 11 modules
FEAT_PKCS_MODULES=1
# PAM support
ifeq ($(BASE_OS),SunOS)
FEAT_PAM=1
SYSTEM_LIB+=$(PAM_LIBS)
endif
# GSSAPI authentication
ifeq ($(BASE_OS),SunOS)
FEAT_GSS=1
SYSTEM_LIB+=$(GSS_LIBS)
endif
# full text search and indexing
#FEAT_SEARCH=1

# you can define NO_xxxx in defines_$(PLATFORM).mk to turn off some of these
#ifndef FEAT_SEARCH
#NO_HTMLEXPORT=1
#endif
#ifndef NO_SNMP
#FEAT_SNMP=1
#endif
# localized installer and product
ifndef NO_L10N
FEAT_L10N=1
endif

PRODUCT_ID = ""$(BRAND_NAME)" "$(PRODUCT_NAME)""

PRODUCT_VERSION = $(VER_MAJOR).$(VER_MINOR)
PRODUCT_FULL_VERSION := $(PRODUCT_VERSION)

ifneq ($(PLATFORM_PRODUCT_VERSION),)
PRODUCT_FULL_VERSION := "$(PRODUCT_FULL_VERSION) ($(PLATFORM_PRODUCT_VERSION))"
endif

ifndef SECURITY_POLICY
SECURITY_POLICY=DOMESTIC
NS_DOMESTIC=1
DEFINES+= -DNS_DOMESTIC
else
ifeq ($(SECURITY_POLICY),DOMESTIC)
NS_DOMESTIC=1
DEFINES+= -DNS_DOMESTIC
else
NS_EXPORT=1
DEFINES+= -DNS_EXPORT
endif
endif

DEFINES+= -DMCC_HTTPD
DEFINES+= -DNET_SSL
DEFINES+= -DSERVER_BUILD
DEFINES+= -DENCRYPT_PASSWORDS
DEFINES+= -DNSPR20
DEFINES+= -DSPAPI20
DEFINES+= -DPEER_SNMP
#  DEFINES+= -DOSVERSION=$(OSVERSION)

ifdef FEAT_PLATFORM_STATS
DEFINES+= -DPLATFORM_SPECIFIC_STATS_ON
endif

# As of now, we follow the same naming convention as the JDK for the
# 64 bit sub-directory
ifndef PLATFORM_64_SUBDIR
PLATFORM_64_SUBDIR=$(JNI_MD_SYSNAME64)
endif

JES_RPATH=$(PLATFORM_JES_LIBPATH)
ifdef BUILD64
PLATFORM_SUBDIR=$(PLATFORM_64_SUBDIR)
PLATFORM_SUBDIR_SUFFIX=/$(PLATFORM_SUBDIR)
JES_RPATH=$(PLATFORM_JES_LIBPATH_64)
endif

# Platform makefiles set $(RPATH_ORIGIN) on platforms that support $ORIGIN
ifdef RPATH_ORIGIN
ifndef RPATH_ORIGIN_SLASH
RPATH_ORIGIN_SLASH=$(RPATH_ORIGIN)/
endif
else
RPATH_ORIGIN=.
RPATH_ORIGIN_SLASH=
endif

ES_RPATH=$(RPATH_ORIGIN):$(RPATH_ORIGIN_SLASH)../lib$(PLATFORM_SUBDIR_SUFFIX):$(RPATH_ORIGIN_SLASH)../../lib$(PLATFORM_SUBDIR_SUFFIX):$(JES_RPATH)

ifndef LD_RPATH
LD_RPATH=$(ES_RPATH)
endif

# File name and file layout related defines
DAEMON_DLL=ns-httpd$(DAEMON_DLL_VER)
PRODUCT_MAGNUS_CONF=magnus.conf
PRODUCT_WATCHDOG_BIN=webservd-wdog
PRODUCT_DAEMON_BIN=webservd
PRODUCT_I18N_DB=webserv_msg
PRODUCT_ADMSERV_NAME=admin-server

PUBLIC_BIN_SUBDIR=bin$(PLATFORM_SUBDIR_SUFFIX)
PRIVATE_BIN_SUBDIR=lib$(PLATFORM_SUBDIR_SUFFIX)
BIN_SUBDIR=bin$(PLATFORM_SUBDIR_SUFFIX)
LIB_SUBDIR=lib$(PLATFORM_SUBDIR_SUFFIX)

JAR_SUBDIR=lib
RES_SUBDIR=lib/messages
DTD_SUBDIR=lib/dtds
TLD_SUBDIR=lib/tlds
ICONS_SUBDIR=lib/icons
INSTALL_SUBDIR=lib/install/templates
WEBAPP_SUBDIR=lib/webapps

PLUGINS_SUBDIR=plugins
INCLUDE_SUBDIR=include
SAMPLES_SUBDIR=samples

JDK_SUBDIR=jdk
PERL_SUBDIR=lib/perl
SNMP_SUBDIR=lib/snmp
