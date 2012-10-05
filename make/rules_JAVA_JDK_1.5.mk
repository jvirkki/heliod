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

# Java rules

ifdef EXPORT_JNI_HEADERS
_EXPORT_JNI_HEADERS+=$(addprefix $(OBJDIR)/, $(EXPORT_JNI_HEADERS))
endif

ifdef PUBLIC_JAR
SOMETHING_JAVA_PUBLIC=1
endif

ifdef PUBLIC_WAR
SOMETHING_JAVA_PUBLIC=1
endif

ifdef PUBLIC_JAVA_SAMPLES
SOMETHING_JAVA_PUBLIC=1
endif

# Fill in dependencies and build rules for java here

ifdef BUILD_JAVA
$(OBJDIR)/%.class : %.java
	@$(MAKE_OBJDIR)
	@$(ECHO) Doing a JAVA build
	$(JAVAC) -d $(OBJDIR) $(JAVAC_FLAGS) $(addsuffix .java, $(JAVA_SRCS))
endif # BUILD_JAVA

ifdef GROUP_JAR_TARGET

ifdef GROUP_JAVA_SRC
_JAVA_GROUP_SRCS_TEMP+=$(addsuffix .java,$(GROUP_JAVA_SRC)) 
endif
ifdef GROUP_SRC_DIRS
_JAVA_GROUP_SRCS_TEMP+=$(addsuffix /*.java, $(GROUP_SRC_DIRS))
endif
ifeq ($(OS_ARCH),WINNT)
_JAVA_GROUP_SRCS=$(subst /,\\,$(_JAVA_GROUP_SRCS_TEMP))
else
_JAVA_GROUP_SRCS=$(_JAVA_GROUP_SRCS_TEMP)
endif

ifdef GROUP_JAR_MISC
_GROUP_JAR_MISC=$(GROUP_JAR_MISC)
endif

ifdef GROUP_PACKAGE
_JAVA_PKG_DIR=$(subst .,/,$(GROUP_PACKAGE))
endif
ifdef GROUP_JAR_PKG_MISC
_GROUP_JAR_MISC+=$(addprefix $(_JAVA_PKG_DIR)/, $(GROUP_JAR_PKG_MISC))
endif

_REAL_GROUP_JAR:=$(OBJDIR)/$(GROUP_JAR_TARGET).jar

$(_REAL_GROUP_JAR) :
	@echo Creating $(OBJDIR)
	@$(MAKE_OBJDIR)
ifdef GROUP_JAR_PKG_MISC
	@echo Copying JAR misc
	@for src_file in $(GROUP_JAR_PKG_MISC) ; do \
		$(MKDIR_DASH_P) `$(DIRNAME) $(OBJDIR)/$(_JAVA_PKG_DIR)/$${src_file}` ; \
		$(CP) ./$${src_file} `$(DIRNAME) $(OBJDIR)/$(_JAVA_PKG_DIR)/$${src_file}` ; \
	done
endif        
ifdef GROUP_JAR_MISC 
	@echo Copying JAR misc files
	@for src_file in $(GROUP_JAR_MISC) ; do \
		$(MKDIR_DASH_P) `$(DIRNAME) $(OBJDIR)/$${src_file}` ; \
		$(CP) ./$${src_file} `$(DIRNAME) $(OBJDIR)/$${src_file}` ; \
	done
endif        
	@echo Compiling JAVA sources
	$(JAVAC) -d $(OBJDIR) $(JAVAC_FLAGS) $(_JAVA_GROUP_SRCS)
ifdef JNI_SRC
	@echo Building JNI headers
	$(JAVAH) $(JAVAH_FLAGS) -d $(JNI_HEADERS_DIR) $(JNI_SRC)
endif
	@echo Building $(GROUP_JAR_TARGET)
	cd $(OBJDIR) && \
	../$(JAR) cf $(GROUP_JAR_TARGET).jar `$(FIND) . -name "*.class" -print` $(_GROUP_JAR_MISC)

endif #GROUP_JAR_TARGET

ifdef WAR_TARGET
_REAL_WAR:=$(OBJDIR)/$(WAR_TARGET).war

ifdef WAR_JAVA_SRC
_WAR_JAVA_SRCS+=$(addsuffix .java,$(WAR_JAVA_SRC)) 
endif
ifdef WAR_SRC_DIRS
_WAR_JAVA_SRCS+=$(addsuffix /*.java, $(WAR_SRC_DIRS))
endif

ifeq ($(OS_ARCH),WINNT)
_REAL_WAR_JAVA_SRCS:=$(subst /,\\,$(_WAR_JAVA_SRCS))
else
_REAL_WAR_JAVA_SRCS:=$(_WAR_JAVA_SRCS)
endif

_WAR_WEBINF=$(OBJDIR)/WEB-INF
_WAR_CLASSDIR=$(_WAR_WEBINF)/classes
$(_REAL_WAR) :
	@$(MAKE_OBJDIR)
	@echo Compiling JAVA sources
	$(MKDIR_DASH_P) $(_WAR_CLASSDIR)
	$(JAVAC) -d $(_WAR_CLASSDIR) $(JAVAC_FLAGS) $(_REAL_WAR_JAVA_SRCS)

	@$(MKDIR_DASH_P) $(_WAR_WEBINF)
ifdef WAR_WEBXML
	@echo Copying WAR web.xml
	@$(CP) $(WAR_WEBXML) $(_WAR_WEBINF)
endif
ifdef WAR_JARS
	@echo Copying WAR jar files
	@$(MKDIR_DASH_P) $(_WAR_WEBINF)/lib
	@$(CP) $(WAR_JARS) $(_WAR_WEBINF)/lib
endif
ifdef WAR_TLDS
	@echo Copying WAR tld files
	@$(MKDIR_DASH_P) $(_WAR_WEBINF)/tlds
	@$(CP) $(WAR_TLDS) $(_WAR_WEBINF)/tlds
endif
ifdef WAR_PROPERTIES
	@echo Copying WAR properties
	@for prop_file in $(WAR_PROPERTIES) ; do \
		$(MKDIR_DASH_P) `$(DIRNAME) $(_WAR_CLASSDIR)/$${prop_file}` ; \
		$(CP) ./$${prop_file} `$(DIRNAME) $(_WAR_CLASSDIR)/$${prop_file}` ; \
	done
endif
ifdef ADD_SRC_TO_WAR
	@echo Copying WAR java sources
	@for src_file in $(_WAR_JAVA_SRCS) ; do \
		$(MKDIR_DASH_P) `$(DIRNAME) $(_WAR_CLASSDIR)/$${src_file}` ; \
		$(CP) ./$${src_file} `$(DIRNAME) $(_WAR_CLASSDIR)/$${src_file}` ; \
	done
endif
ifdef WAR_MISC 
	@echo Copying WAR misc files
	@for src_file in $(WAR_MISC) ; do \
		$(MKDIR_DASH_P) `$(DIRNAME) $(OBJDIR)/$${src_file}` ; \
		$(CP) ./$${src_file} `$(DIRNAME) $(OBJDIR)/$${src_file}` ; \
	done
endif        
	@echo Building $(WAR_TARGET)
	cd $(OBJDIR) && \
	../$(JAR) cf $(WAR_TARGET).war WEB-INF $(WAR_MISC)
endif #WAR_TARGET

ifndef NO_STD_ALL_TARGET
all:: compile libraries
endif #NO_STD_ALL_TARGET

ifndef NO_STD_COMPILE_TARGET
compile:: $(OBJS)
	+$(LOOP_OVER_DIRS)
endif #NO_STD_COMPILE_TARGET

ifdef JAR_TARGET
REAL_JAR_TARGET:=$(OBJDIR)/$(JAR_TARGET).jar
REAL_JAR_CLASSES:=$(addprefix $(OBJDIR)/, $(JAR_CLASSES:=.class))
ifdef JAR_CLASSES
	$(JAVAC) -d $(OBJDIR) $(JAVAC_FLAGS) $(JAR_CLASSES:=.java)
endif
	@$(ECHO) "Building JAR"
	cd $(OBJDIR) && \
	../$(JAR) cf $(addsuffix .jar, $(JAR_TARGET)) `$(FIND) . -name "*.class" -print`
endif #JAR_TARGET

ifndef NO_STD_LIBRARIES_TARGET
libraries:: $(REAL_JAR_TARGET) $(_REAL_GROUP_JAR) $(_REAL_WAR)
	+$(LOOP_OVER_DIRS)
endif #NO_STD_LIBRARIES_TARGET

ifdef EXPORT_JAR
_EXPORT_JARS =$(addprefix $(OBJDIR)/, $(addsuffix .jar, $(EXPORT_JAR)))
endif
ifdef EXPORT_WAR
_EXPORT_JARS+=$(addprefix $(OBJDIR)/, $(addsuffix .war, $(EXPORT_WAR)))
endif

ifndef EXPORT_JAR_DEST
EXPORT_JAR_DEST=$(INTERNAL_ROOT)/lib
endif

ifdef _EXPORT_JARS
print_exports::
	@$(ECHO) "JARS: $(_EXPORT_JARS)"
publish_copy::
	$(MKDIR_DASH_P) $(EXPORT_JAR_DEST)
	$(MKDIR_DASH_P) $(INTERNAL_ROOT)/lib
	$(CP) -f $(_EXPORT_JARS) $(EXPORT_JAR_DEST)
	$(CP) -f $(_EXPORT_JARS) $(INTERNAL_ROOT)/lib
endif #_EXPORT_JARS

ifdef SOMETHING_JAVA_PUBLIC
ifdef PUBLIC_JAR
_PUBLIC_JARS+=$(addprefix $(OBJDIR)/, $(addsuffix .jar, $(PUBLIC_JAR)))
endif
ifdef PUBLIC_WAR
_PUBLIC_JARS+=$(addprefix $(OBJDIR)/, $(addsuffix .war, $(PUBLIC_WAR)))
endif

ifndef PUBLIC_JAR_DEST
PUBLIC_JAR_DEST=$(WORK_ROOT)/lib
endif
print_exports::
	@$(ECHO) "JARS: $(_PUBLIC_JARS)"
	@$(ECHO) "Samples: $(PUBLIC_JAVA_SAMPLES)"
publish_copy::
ifdef _PUBLIC_JARS
	$(MKDIR_DASH_P) $(PUBLIC_JAR_DEST)
	$(CP) -f $(_PUBLIC_JARS) $(PUBLIC_JAR_DEST)
endif # PUBLIC_JAR
ifdef PUBLIC_JAVA_SAMPLES
	$(MKDIR_DASH_P) $(WORK_ROOT)/samples/servlets/$(MODULE)
	$(CP) -r $(PUBLIC_JAVA_SAMPLES) $(WORK_ROOT)/samples/servlets/$(MODULE)
endif #PUBLIC_JAVA_SAMPLES
endif #SOMETHING_JAVA_PUBLIC

ifdef EXPORT_JNI_HEADERS
print_exports::
	@$(ECHO) "HEADERS: $(_EXPORT_JNI_HEADERS)"
publish_copy::
	$(MKDIR_DASH_P) $(INTERNAL_ROOT)/include/$(MODULE)
	$(CP) -f $(_EXPORT_JNI_HEADERS) $(INTERNAL_ROOT)/include/$(MODULE)
endif #EXPORT_JNI_HEADERS
